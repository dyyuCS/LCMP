import argparse
import csv
import os
import subprocess


def get_pctl(a, p):
	idx = int(len(a) * p)
	return a[idx]


def parse_range(r):
	"""
	Parse a range string like "0-15" into (start, end) inclusive.
	"""
	if isinstance(r, (list, tuple)) and len(r) == 2:
		return int(r[0]), int(r[1])
	if '-' in r:
		l, r2 = r.split('-', 1)
		return int(l), int(r2)
	raise ValueError("range should be in 'start-end' format")


def build_awk_cmd(file_path, flow_type, time_limit, a_range, b_range):
	A0, A1 = a_range
	B0, B1 = b_range
	# Compute cross-DC filter in awk: restore node ids from hex IPs
	# Use sprintf to convert hex to decimal, then extract node id
	base_prog = (
		"sip=sprintf(\"%d\", \"0x\"$1); dip=sprintf(\"%d\", \"0x\"$2); "
		"sid=int(sip/256)%65536; did=int(dip/256)%65536; "
		"inA=(sid>=A0 && sid<=A1); inB=(sid>=B0 && sid<=B1); "
		"inA2=(did>=A0 && did<=A1); inB2=(did>=B0 && did<=B1); "
		"cross=(inA && inB2) || (inB && inA2); "
	)
	if flow_type == 0:
		cond = f"($4==100 && $6+$7<{time_limit} && cross)"
	elif flow_type == 1:
		cond = f"($4==200 && $6+$7<{time_limit} && cross)"
	else:
		cond = f"($6+$7<{time_limit} && cross)"
	prog = f"{{{base_prog} if {cond} {{slow=$7/$8; print slow<1?1:slow, $5}}}}"
	cmd = (
		"cat %s | awk -v A0=%d -v A1=%d -v B0=%d -v B1=%d '" % (file_path, A0, A1, B0, B1)
		+ prog + "' | sort -n -k 2"
	)
	return cmd


if __name__ == "__main__":
	parser = argparse.ArgumentParser(description='Analyze slowdown for flows crossing between two DC node ranges')
	parser.add_argument('-p', dest='prefix', action='store', default='fct', help="Prefix of the fct file (kept for compatibility)")
	parser.add_argument('-s', dest='step', action='store', default='5')
	parser.add_argument('-t', dest='type', action='store', type=int, default=0, help="0: normal(dport=100), 1: incast(dport=200), 2: all")
	parser.add_argument('-T', dest='time_limit', action='store', type=int, default=300000000000000000, help="only consider flows that finish before T")
	parser.add_argument('-b', dest='bw', action='store', type=int, default=25, help="bandwidth of edge link (Gbps)")
	parser.add_argument('-o', dest='outputPath', action='store', default='result', help="output path root")
	parser.add_argument('-i', dest='inputPath', action='store', default='../simulation', help="input file path root")
	parser.add_argument('--dcA', dest='dcA', required=True, help="DC-A id (e.g., 1) or host range (e.g., 0-15)")
	parser.add_argument('--dcB', dest='dcB', required=True, help="DC-B id (e.g., 8) or host range (e.g., 112-127)")
	args = parser.parse_args()

	flow_type = args.type
	time_limit = args.time_limit
	step = int(args.step)

	def parse_dc_or_range(s):
		# if given like "0-15", parse as range; else treat as DC index (1-based), map to 16 hosts
		if '-' in s:
			return parse_range(s), None
		dc_id = int(s)
		start = (dc_id - 1) * 16
		end = start + 15
		return (start, end), dc_id

	a_range, dcA_id = parse_dc_or_range(args.dcA)
	b_range, dcB_id = parse_dc_or_range(args.dcB)
	if dcA_id is not None:
		dcA_label = f"DC{dcA_id}"
	else:
		dcA_label = f"{a_range[0]}-{a_range[1]}"
	if dcB_id is not None:
		dcB_label = f"DC{dcB_id}"
	else:
		dcB_label = f"{b_range[0]}-{b_range[1]}"

	# CCs / UTILs / ROUTINGs to iterate (keep consistent with existing script)
	CCs = [
		'dcqcn',
		'hpcc',
		'timely',
		'dctcp',
	]
	UTILs = [
		'0.3util',
		# '0.5util',
		# '0.8util'
	]
	ROUTINGs = [
		"Ours",
		"ECMP",
		"UCMP",
	]

	DATASETs =[
		'WebSearch',
		# 'AliStorage',
		# 'GoogleRPC',
	]
	
	# absolute input root
	current_file_path = os.path.abspath(__file__)
	input_root = os.path.join(os.path.dirname(current_file_path), args.inputPath)

	for dataset in DATASETs:
		for cc in CCs:
			for util in UTILs:
				# init result grid (each row: [percentile, FlowSize, Ours_p50, Ours_p99, UCMP_p50, UCMP_p99, ...])
				res = [[i / 100.0] for i in range(0, 100, step)]
				for routing in ROUTINGs:
					file_rel = f"{dataset}/{util}/{routing}/fct_{cc}.txt"
					file_path = f"{input_root}/{file_rel}"
					if not os.path.exists(file_path):
						print(f"File not found: {file_rel}")
						exit()
					cmd = build_awk_cmd(file_path, flow_type, time_limit, a_range, b_range)
					output = subprocess.check_output(cmd, shell=True)
					if isinstance(output, bytes):
						output = output.decode('utf-8')
					a = output.split('\n')[:-2]
					n = len(a)
					if n == 0:
						print(f"No matching 2DC records after filtering: {file_rel}")
						exit(1)
					for i in range(0, 100, step):
						l = i * n // 100
						r = (i + step) * n // 100
						idx = i // step
						if l >= n:
							# empty bucket: ensure FlowSize present once, then default stats
							if len(res[idx]) == 1:
								res[idx].append(0)
							res[idx].append(1.0)
							res[idx].append(1.0)
							continue
						d = [[float(x.split()[0]), int(x.split()[1])] for x in a[l:r]]
						if len(d) == 0:
							if len(res[idx]) == 1:
								res[idx].append(0)
							res[idx].append(1.0)
							res[idx].append(1.0)
							continue
						fct = sorted([x[0] for x in d])
						if len(res[idx]) == 1:
							res[idx].append(d[-1][1])
						res[idx].append(get_pctl(fct, 0.5))
						res[idx].append(get_pctl(fct, 0.99))

				# write csv per cc+util, with columns for each routing on same row
				out_dir = os.path.dirname(f"{args.outputPath}/{util}/")
				if out_dir and not os.path.exists(out_dir):
					os.makedirs(out_dir)
				out_name = f"{out_dir}/{dataset}_{cc}_{util}-FCTslowdown-{dcA_label}-{dcB_label}.csv"
				with open(out_name, 'w', newline='') as csvfile:
					writer = csv.writer(csvfile)
					# header
					header = ['Percentile', 'FlowSize']
					for routing in ROUTINGs:
						header += [f'{routing}-fct_p50', f'{routing}-fct_p99']
					writer.writerow(header)
					# rows & terminal output
					for item in res:
						line = "%.3f %d" % (item[0], item[1])
						row = [item[0], item[1]]
						for j in range(2, len(item), 2):
							line += "\t%.3f %.3f" % (item[j], item[j+1])
							row += [item[j], item[j+1]]
						print(line)
						writer.writerow(row)



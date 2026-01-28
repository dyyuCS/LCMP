import subprocess
import argparse
import csv
import os

def get_pctl(a, p):
	i = int(len(a) * p)
	return a[i]

if __name__=="__main__":
	parser = argparse.ArgumentParser(description='')
	parser.add_argument('-p', dest='prefix', action='store', default='fct', help="Specify the prefix of the fct file. Usually like fct_<topology>_<trace>")
	parser.add_argument('-s', dest='step', action='store', default='5')
	parser.add_argument('-t', dest='type', action='store', type=int, default=0, help="0: normal, 1: incast, 2: all")
	parser.add_argument('-T', dest='time_limit', action='store', type=int, default=300000000000000000, help="only consider flows that finish before T")
	parser.add_argument('-b', dest='bw', action='store', type=int, default=25, help="bandwidth of edge link (Gbps)")
	parser.add_argument('-o', dest='outputPath', action='store', default='result', help="output path")
	parser.add_argument('-i', dest='inputPath', action='store', default='../simulation', help="input file path")
	args = parser.parse_args()

	type = args.type
	time_limit = args.time_limit

	# Please list all the cc (together with parameters) that you want to compare.
	# For example, here we list two CC: 1. HPCC-PINT with utgt=95,AI=50Mbps,pint_log_base=1.05,pint_prob=1; 2. HPCC with utgt=95,ai=50Mbps.
	# For the exact naming, please check ../simulation/mix/fct_*.txt output by the simulation.
	CCs = [
		'dcqcn',
		# 'hpcc',
		# 'timely',
		# 'dctcp',
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
		# 'link-util',
	]

	cc = CCs[0]
	step = int(args.step)
	
	
	for routing in ROUTINGs:
		for util in UTILs:
			res = [[i/100.] for i in range(0, 100, step)]
			for cc in CCs:
				#file = "%s_%s.txt"%(args.prefix, cc)
				# 获取prefix的绝对路径
				current_file_path = os.path.abspath(__file__)
				inputPath_abs = os.path.join(os.path.dirname(current_file_path), args.inputPath)
				fileName = f'{util}/{routing}/fct_{cc}.txt'
				file = f'{inputPath_abs}/{fileName}'
				# print("===: ", file)
				if not os.path.exists(file):
					print(f"File not found: {fileName}")
					exit()

				if type == 0:
					cmd = "cat %s"%(file)+" | awk '{if ($4==100 && $6+$7<"+"%d"%time_limit+") {slow=$7/$8;print slow<1?1:slow, $5}}' | sort -n -k 2"
					# print cmd
					output = subprocess.check_output(cmd, shell=True)
				elif type == 1:
					cmd = "cat %s"%(file)+" | awk '{if ($4==200 && $6+$7<"+"%d"%time_limit+") {slow=$7/$8;print slow<1?1:slow, $5}}' | sort -n -k 2"
					#print cmd
					output = subprocess.check_output(cmd, shell=True)
				else:
					cmd = "cat %s"%(file)+" | awk '{$6+$7<"+"%d"%time_limit+") {slow=$7/$8;print slow<1?1:slow, $5}}' | sort -n -k 2"
					#print cmd
					output = subprocess.check_output(cmd, shell=True)

				# up to here, `output` should be a string of multiple lines, each line is: fct, size
				# 如果 output 是 bytes，先解码
				if isinstance(output, bytes):
					output = output.decode('utf-8')
				a = output.split('\n')[:-2]
				n = len(a)
				for i in range(0, 100, step):
					l = i * n // 100
					r = (i + step) * n // 100
					d = [ [float(x.split()[0]), int(x.split()[1])] for x in a[l:r] ]
					fct = sorted([x[0] for x in d])
					idx = i // step
					res[idx].append(d[-1][1])  # flow size
					# res[idx].append(sum(fct) / len(fct)) # avg fct
					res[idx].append(get_pctl(fct, 0.5))   # mid fct
					# res[idx].append(get_pctl(fct, 0.95))  # 95-pct fct
					res[idx].append(get_pctl(fct, 0.99))  # 99-pct fct
			
			# 新增: 检查并创建文件夹、输出并写入csv --------
			output_dir = args.outputPath
			output_dir = os.path.dirname(f'{output_dir}/{util}/')
			if output_dir and not os.path.exists(output_dir):
				os.makedirs(output_dir)
			
			output_name = f"{output_dir}/{routing}_{util}-FCTslowdown.csv"
			with open(f'{output_name}', 'w') as csvfile:
				writer = csv.writer(csvfile)
				# 写入表头
				header = ['Percentile', 'FlowSize']
				for cc in CCs:
					header += [f'{routing}-{cc}-fct_p50', f'{routing}-{cc}-fct_p99']
					# header += [f'{routing}-{cc}-fct_median', f'{routing}-{cc}-fct_p95', f'{routing}-{cc}-fct_p99']
				writer.writerow(header)

			# 新增: 检查并创建文件夹、输出并写入csv --------
				# 写入数据
				for item in res:
					line = "%.3f %d"%(item[0], item[1])
					row = [item[0], item[1]]
					i = 1
					for cc in CCs:
						# line += "\t%.3f %.3f %.3f"%(item[i+1], item[i+2], item[i+3])
						# row += [item[i+1], item[i+2], item[i+3]]
						# i += 4
						line += "\t%.3f %.3f"%(item[i+1], item[i+2])
						row += [item[i+1], item[i+2]]
						i += 3
					print(line)
					writer.writerow(row)



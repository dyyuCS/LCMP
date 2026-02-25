#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import random
import math
import heapq
from optparse import OptionParser
from custom_rand import CustomRand
import os
class Flow:
	def __init__(self, src, dst, size, t):
		self.src, self.dst, self.size, self.t = src, dst, size, t # 包含源地址、目的地址、流量大小和时间戳
	def __str__(self):
		return "%d %d 3 100 %d %.9f"%(self.src, self.dst, self.size, self.t)

def translate_bandwidth(b):
	'''
	将带宽字符串转换为数值 (以bit为单位)
	'''
	if b == None:
		return None
	if type(b)!=str:
		return None
	if b[-1] == 'G':
		return float(b[:-1])*1e9
	if b[-1] == 'M':
		return float(b[:-1])*1e6
	if b[-1] == 'K':
		return float(b[:-1])*1e3
	return float(b)

def poisson(lam):
	'''
	生成 泊松分布的随机数
	'''
	return -math.log(1-random.random())*lam

if __name__ == "__main__":
	# 用户可指定src_dc和dst_dc，示例：src_dc=1，dst_dc=5
	# 可通过变量或参数指定，以下为硬编码示例
	user_src_dc = 0  # 指定src所在DC编号（从0开始）
	user_dst_dc = 7  # 指定dst所在DC编号（从0开始）
	
	port = 80
	# 使用 OptionParser 解析命令行参数，定义了多个选项：cdf_file、nhost、load、bandwidth、time 和 output。
	parser = OptionParser()
	parser.add_option("-c", "--cdf", dest = "cdf_file", help = "the file of the traffic size cdf", default = "uniform_distribution.txt")
	parser.add_option("-n", "--nhost", dest = "nhost", help = "number of hosts")
	parser.add_option("-l", "--load", dest = "load", help = "the percentage of the traffic load to the network capacity, by default 0.3", default = "0.3") #通常是一个介于0和1之间的值，表示网络的利用率
	parser.add_option("-b", "--bandwidth", dest = "bandwidth", help = "the bandwidth of host link (G/M/K), by default 10G", default = "10G")
	parser.add_option("-t", "--time", dest = "time", help = "the total run time (s), by default 10", default = "10")
	parser.add_option("-o", "--output", dest = "output", help = "the output file", default = "tmp_traffic.txt")
	# [NEW]添加新的选项以支持数据中心数量和每个数据中心的主机数量
	parser.add_option("-d", "--dc_count", dest="dc_count",
										help="number of data centers", default="1")
	parser.add_option("-m", "--dc_nnodes", dest="dc_nnodes",
										help="number of hosts per data center (nodes in each DC)")
	options,args = parser.parse_args()

	base_t = 2000000000 # 设置基准时间 base_t。这里的时间单位是纳秒。

	# 读取并校验 DC 参数
	dc_count = int(options.dc_count)
	if not options.dc_nnodes:
			print("please use -m to specify number of hosts per DC")
			sys.exit(1)
	dc_nnodes = int(options.dc_nnodes)

	# 节点总数
	nhost = dc_count * dc_nnodes

	load = float(options.load)
	bandwidth = translate_bandwidth(options.bandwidth)
	time = float(options.time)*1e9 # translates to ns
	if bandwidth == None:
		print("bandwidth format incorrect")
		sys.exit(0)

	# mkdir of output traffic load
	dir_path = 'output_traffic_load'
	os.makedirs(dir_path, exist_ok=True)
	output = f"{dir_path}/{options.output}"


  # 打开并读取 CDF 文件，将其内容存储在 cdf 列表中。
	fileName = options.cdf_file
	file = open(fileName,"r", encoding='utf-8')
	lines = file.readlines()
	# read the cdf, save in cdf as [[x_i, cdf_i] ...]
	cdf = []
	for line in lines:
		x,y = map(float, line.strip().split(' '))
		cdf.append([x,y])

	# 创建 CustomRand 对象，并使用 CDF 初始化它。如果 CDF 无效，则退出程序。
	# create a custom random generator, which takes a cdf, and generate number according to the cdf
	customRand = CustomRand()
	if not customRand.setCdf(cdf):
		print("Error: Not valid cdf")
		sys.exit(0)

	# 生成流量数据
		# 计算平均流量大小和平均到达间隔时间。
		# 估算流量数量并初始化流量计数器。
		# 初始化主机列表并使用 heapq 进行堆排序。
		# 生成流量并写入输出文件。
		# 更新流量计数器并在文件开头写入实际流量数量。
	ofile = open(output, "w", encoding='utf-8')

	# generate flows
	avg = customRand.getAvg() # 根据CDF（累积分布函数）计算出的数据包的平均流量大小
	avg_inter_arrival = 1/(bandwidth*load/8./avg)*1000000000 # 计算流量的平均到达间隔时间（纳秒）
	print("avg_inter_arrival: ", avg_inter_arrival, "bandwidth: ", bandwidth, "load: ", load, "avg: ", avg)
	# 公式：流量的平均到达间隔时间 = 1 / (带宽 * 负载 / 8 / 平均数据包大小) * 1000000000
	# 其中：
	# bandwidth: 带宽，通常以比特每秒（bps）为单位。
	# load: 负载，通常是一个介于0和1之间的值，表示网络的利用率。
	# avg: 平均数据包大小，通常以字节（bytes）为单位。
	# 1000000000: 1秒 = 10^9纳秒
	# bandwidth * load / 8.: 将bit转换为byte（1字节 = 8比特）。这里的单位是字节每秒（Bps）。
	# bandwidth * load / 8. / avg: 计算每秒钟传输的数据包数量（即每秒钟传输的字节数除以平均数据包大小）。
	# 1 / (bandwidth * load / 8. / avg): 计算平均到达间隔时间（秒）。
	# 这段代码的目的是计算在给定带宽、负载和平均数据包大小的情况下，计算每个数据包的平均到达间隔时间（以纳秒为单位）。

	n_flow_estimate = int(time / avg_inter_arrival * nhost) # 估算流量数量
	print("n_flow_estimate: ", n_flow_estimate, "time: ", time, "avg_inter_arrival: ", avg_inter_arrival, "nhost: ", nhost)
	n_flow = 0

	flows = []
	flows.append("# 流源IP 目的IP 优先级pg 目的端口 数据量(Bytes) 流开始时间(s)\n")
	flows.append("# src 		dst 	pg    		dport  	 size				start_time\n")
	flows.append("0\n")  # 占位，稍后替换为实际流量数量

	# DC间流量生成
	if dc_count is not None and dc_nnodes is not None:
		if user_src_dc >= dc_count or user_dst_dc >= dc_count or user_src_dc == user_dst_dc:
			print("Error: src_dc和dst_dc必须不同且在有效范围内")
			sys.exit(1)

		# 仅生成src在user_src_dc，dst在user_dst_dc的流
		src_hosts = list(range(user_src_dc * dc_nnodes, (user_src_dc + 1) * dc_nnodes))
		dst_hosts = list(range(user_dst_dc * dc_nnodes, (user_dst_dc + 1) * dc_nnodes))
		host_heap = [(base_t + int(poisson(avg_inter_arrival)), src) for src in src_hosts]
		heapq.heapify(host_heap)
		while host_heap:
			t, src = heapq.heappop(host_heap)
			inter_t = int(poisson(avg_inter_arrival))
			next_t = t + inter_t
			if next_t > time + base_t:
				continue

			# dst在指定DC中随机选择主机
			dst = random.choice(dst_hosts)

			# 生成流大小
			size = int(customRand.rand())
			if size <= 0:
				size = 1

			flows.append(f"{src} {dst} 3 100 {size} {t * 1e-9:.9f}\n")
			n_flow += 1
			heapq.heappush(host_heap, (next_t, src))
	else:
	# 保持原有逻辑
		host_list = [(base_t + int(poisson(avg_inter_arrival)), i) for i in range(nhost)] # 生成初始的主机列表，每个主机有一个到达时间
		heapq.heapify(host_list) # 将主机列表转换为堆
		while len(host_list) > 0: # 当主机列表不为空时
			t, src = host_list[0] # 获取堆顶元素（最早到达的主机）
			inter_t = int(poisson(avg_inter_arrival)) # 生成下一个到达时间间隔
			new_tuple = (src, t + inter_t) # 创建新的元组，包含主机和新的到达时间
			dst = random.randint(0, nhost-1) # 随机选择一个目的主机
			while (dst == src): # 确保目的主机与源主机不同
				dst = random.randint(0, nhost-1)
			if (t + inter_t > time + base_t): # 如果新的到达时间超过了总时间
				heapq.heappop(host_list) # 弹出堆顶元素
			else:
				size = int(customRand.rand())  # 生成随机大小
				if size <= 0:
					size = 1 # 确保大小至少为1
				n_flow += 1 # 更新流量计数器
				flows.append("%d %d 3 100 %d %.9f\n"%(src, dst, size, t * 1e-9)) # 将流量信息写入文件
				heapq.heapreplace(host_list, (t + inter_t, src)) # 替换堆顶元素
	
	# 写入实际流量数量
	flows[2] = "%d\n" % n_flow

	with open(output, "w", encoding='utf-8') as ofile:
			ofile.writelines(flows)
	ofile.close() # 关闭文件



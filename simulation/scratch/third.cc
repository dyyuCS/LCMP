/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation;
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

// 条件编译指令，根据编译时是否定义了PGO_TRAINING宏，选择不同的方式打开配置文件
#undef PGO_TRAINING
#define PATH_TO_PGO_CONFIG "path_to_pgo_config"

#include <iostream>
#include <fstream>
#include <unordered_map>
#include <time.h> 
#include "ns3/core-module.h"
#include "ns3/qbb-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/global-route-manager.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/packet.h"
#include "ns3/error-model.h"
#include <ns3/rdma.h>
#include <ns3/rdma-client.h>
#include <ns3/rdma-client-helper.h>
#include <ns3/rdma-driver.h>
#include <ns3/switch-node.h>
#include <ns3/dci-switch-node.h>
#include <ns3/sim-setting.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <iomanip>  // 调用setw缩进函数


#include <queue>      // 用于优先队列 std::priority_queue
#include <vector>     // 作为优先队列的底层容器
#include <functional> // 用于 std::greater，实现最小优先队列
#include <limits>     // 用于获取数值类型的最大值

// 函数声明：
std::istream& SkipComments(std::istream& is); // 跳过输入流中的注释行和空行
std::string GetCurrentTime(); // 获取当前时间并格式化为hh:mm:ss
void ConfigureFlowTracking(const std::string& trace_flows_file, const std::string& output_dir); // 配置流追踪
bool DirectoryExists(const std::string& path);
std::string replace_config_variables(const std::string& input);
void periodic_monitoring(FILE *fout_uplink);
// void periodic_monitoring(FILE *fout_uplink, FILE *fout_conn);


using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE("GENERIC_SIMULATION");

uint32_t cc_mode = -1;
bool enable_qcn = true, use_dynamic_pfc_threshold = true;
int routing_mode = 0; // 0: ECMP, 1: UCMP, 2: Ours

uint32_t packet_payload_size = 1000, l2_chunk_size = 0, l2_ack_interval = 0;
double pause_time = 5, simulator_stop_time = 2.5;
std::string data_rate, link_delay, topology_file, flow_file, trace_file, trace_output_file;
std::string working_dir = ""; // 工作目录
std::string output_dir = ""; // 输出目录

std::string fct_output_file = "fct.txt";
std::string pfc_output_file = "pfc.txt";
std::string qlen_mon_file;
bool enable_link_util_record = false;
std::string link_util_output_file = "link_util.txt";

double alpha_resume_interval = 55, rp_timer, ewma_gain = 1 / 16;
double rate_decrease_interval = 4;
uint32_t fast_recovery_times = 5;
std::string rate_ai, rate_hai, min_rate = "100Mb/s";
std::string dctcp_rate_ai = "1000Mb/s";

bool clamp_target_rate = false, l2_back_to_zero = false;
double error_rate_per_link = 0.0;
uint32_t has_win = 1;
uint32_t global_t = 1;
uint32_t mi_thresh = 5;
bool var_win = false, fast_react = true;
bool multi_rate = true;
bool sample_feedback = false;
double pint_log_base = 1.05;
double pint_prob = 1.0;
double u_target = 0.95;
uint32_t int_multi = 1;
bool rate_bound = true;

uint32_t ack_high_prio = 0;
uint64_t link_down_time = 0;
uint32_t link_down_A = 0, link_down_B = 0;

uint32_t enable_trace = 1;

uint32_t buffer_size = 16;
uint32_t dci_buffer_size = 128; 

uint32_t qlen_dump_interval = 100000000, qlen_mon_interval = 100;
uint64_t qlen_mon_start = 2000000000, qlen_mon_end = 2100000000;

// 成本权重参数（来自配置 67-78 行）
uint32_t w_dl = 3;      // W_DL
uint32_t w_bw = 1;      // W_BW
uint32_t s_static = 2;  // S_STATIC
uint32_t w_ql = 2;      // W_QL
uint32_t w_tl = 1;      // W_TL
uint32_t w_dp = 1;      // W_DP
uint32_t s_cong = 2;    // S_CONG
uint32_t alpha_cost = 3; // ALPHA
uint32_t beta_cost = 1;  // BETA
uint32_t s_total = 2;    // S_TOTAL

unordered_map<uint64_t, uint32_t> rate2kmax, rate2kmin;
unordered_map<uint64_t, double> rate2pmax;

std::string dc_topo = "";
std::string file_size = "";

/************************************************
 * Runtime varibles
 ***********************************************/
std::ifstream topof, flowf, tracef;

NodeContainer n; // keep track of a set of node pointers

uint64_t nic_rate;

uint64_t maxRtt, maxBdp;


// 存储网络中所有节点之间的连接关系
// 记录每个连接的接口信息
struct Interface{
	uint32_t idx; // 接口编号
	bool up; // 接口状态（是否启用）
	uint64_t delay; 
	uint64_t bw;

	Interface() : idx(0), up(false){}
};
map<Ptr<Node>, map<Ptr<Node>, Interface> > nbr2if; // neighbor to interface 的缩写，表示从邻居节点到接口的映射关系。
// 外层 map<Ptr<Node>, ...>：key 是节点指针（源节点）。
// 内层 map<Ptr<Node>, Interface>：key 是邻居节点指针（目标节点），value 是 Interface 结构体，记录连接这两个节点的端口编号、状态、时延、带宽等信息。
// [存储网络拓扑信息结构，它维护了网络中所有节点之间的连接关系及其属性，为路由计算和包转发提供了必要的信息]
// Mapping destination to next hop for each node: <node, <dest, <nexthop0, ...> > >
map<Ptr<Node>, map<Ptr<Node>, vector<Ptr<Node> > > > nextHop; // 存储每个节点到达其他节点的下一跳路由信息
// 第一层 map<Ptr<Node>, ...>: 表示当前节点（源节点）
// 第二层 map<Ptr<Node>, ...>: 表示目标节点（目的地）
// 第三层 vector<Ptr<Node>>: 表示从当前节点到目标节点的所有可能的下一跳节点列表

map<Ptr<Node>, map<Ptr<Node>, uint64_t> > pairDelay; // 链路时延
map<Ptr<Node>, map<Ptr<Node>, uint64_t> > pairTxDelay; // 发送端到链路的传输时延
map<uint32_t, map<uint32_t, uint64_t> > pairBw; // 链路带宽
map<Ptr<Node>, map<Ptr<Node>, uint64_t> > pairBdp; // 链路带宽时延积
map<uint32_t, map<uint32_t, uint64_t> > pairRtt; // 链路往返时延

std::vector<Ipv4Address> serverAddress;

// maintain port number for each host pair
std::unordered_map<uint32_t, unordered_map<uint32_t, uint16_t> > portNumder;
// 维护接收端 RxQp 的引用计数，键 = (dip<<32)|(pg<<16)|dport
std::unordered_map<uint64_t, uint32_t> rxqp_refcount;

struct FlowInput{
	uint32_t src, dst, pg, port, dport;
	uint64_t flowSize; // 改为64bits, 否则原先的32bits无法存储超过4294967295（2^32-1）大小的流
	double start_time;
	uint32_t idx;
};
FlowInput flow_input = {0}; // 初始化FlowInput结构体，所有成员均为0
uint32_t flow_num;

// 调度/停止相关的全局计数与标志，需在使用前定义
static uint32_t scheduled_flows = 0;
static bool all_flows_scheduled = false;
// 移除看门狗相关

// [NEW] 记录DCI switch的uplink和downlink端口 
std::map<uint32_t, std::vector<uint32_t>> dciId2UplinkIf;
// std::map<uint32_t, std::vector<uint32_t>> dciId2DownlinkIf; // 没必要记录

// 读取flow文件（健壮版：跳过注释/空行并检测读取失败）
bool ReadFlowInput(){
	if (flow_input.idx < flow_num){
		if (!(SkipComments(flowf) >> flow_input.src >> flow_input.dst >> flow_input.pg >> flow_input.dport >> flow_input.flowSize >> flow_input.start_time)){
			std::cerr << "[ERROR] Failed to read flow line #" << flow_input.idx << ", shrink flow_num to " << flow_input.idx << std::endl;
			// 将声明的流数量收缩到已成功读取的数量，避免后续死循环
			flow_num = flow_input.idx;
			return false;
		}
		std::cout << "[INFO] Flow Idx: " << flow_input.idx << ". Read flow: " << flow_input.src << " -> " << flow_input.dst << ", size: " << flow_input.flowSize << ", start_time: " << flow_input.start_time << "s" << std::endl;
		NS_ASSERT(n.Get(flow_input.src)->GetNodeType() == 0 && n.Get(flow_input.dst)->GetNodeType() == 0); // 断言确保流量的源节点和目标节点都是主机类型（NodeType为0），而不是交换机或其他类型的节点。这是因为只有主机才能作为流量的源和目的地。
		// 当断言失败时，NS-3会调用内部的NS_FATAL_ERROR函数，该函数最终会调用std::terminate()或类似函数强制终止程序。
		return true;
	}
	return false;
}

// 重点：网络流调度
void ScheduleFlowInputs(){
	// 允许轻微时间误差，避免因浮点/取整导致的调度遗漏
	Time now = Simulator::Now();
	Time eps = NanoSeconds(1);
	Time flowStart = Seconds(flow_input.start_time);
	while (flow_input.idx < flow_num && flowStart <= now + eps){
		scheduled_flows++;
		uint32_t port = portNumder[flow_input.src][flow_input.dst]++; // get a new port number 
		// 增加接收端 RxQp 引用计数（dip, pg, dport）
		uint32_t dip_u32 = serverAddress[flow_input.dst].Get();
		uint64_t rxkey = ((uint64_t)dip_u32 << 32) | ((uint64_t)flow_input.pg << 16) | (uint64_t)flow_input.dport;
		rxqp_refcount[rxkey]++;
		// 创建RdmaClientHelper对象，用于创建应用程序
		RdmaClientHelper clientHelper(
			flow_input.pg, serverAddress[flow_input.src], serverAddress[flow_input.dst], port, flow_input.dport, flow_input.flowSize, has_win?(global_t==1?maxBdp:pairBdp[n.Get(flow_input.src)][n.Get(flow_input.dst)]):0, global_t==1?maxRtt:pairRtt[flow_input.src][flow_input.dst]
		);

		// 创建应用容器，给流量的[发送端]安装应用
		ApplicationContainer appCon = clientHelper.Install(n.Get(flow_input.src));
		appCon.Start(Time(0)); // 设置应用的开始时间

		// 读取下一条流
		flow_input.idx++;
		if (!ReadFlowInput()){
			// 读取失败已将 flow_num 收缩至当前 idx，跳出循环
			break;
		}
		flowStart = Seconds(flow_input.start_time);
		now = Simulator::Now();
	}

	// 安排下一次调度；若已错过启动时间，则立即重试
	if (flow_input.idx < flow_num){
		Time delta = Seconds(flow_input.start_time) - Simulator::Now();
		if (delta <= NanoSeconds(0)) delta = NanoSeconds(1);
		Simulator::Schedule(delta, ScheduleFlowInputs);
	}else { // no more flows, close the file
		all_flows_scheduled = true;
		std::cout << GetCurrentTime() << "All flows scheduled: " << scheduled_flows << "/" << flow_num << std::endl;
		flowf.close();
	}
}

Ipv4Address node_id_to_ip(uint32_t id){
	return Ipv4Address(0x0b000001 + ((id / 256) * 0x00010000) + ((id % 256) * 0x00000100));
}

uint32_t ip_to_node_id(Ipv4Address ip){
	return (ip.Get() >> 8) & 0xffff;
}

// 在 qp_finish 函数中增加计数
static uint32_t completed_flows = 0;

void qp_finish(FILE* fout, Ptr<RdmaQueuePair> q){
	uint32_t sid = ip_to_node_id(q->sip), did = ip_to_node_id(q->dip);
	uint64_t base_rtt = pairRtt[sid][did], b = pairBw[sid][did]; // 获取路径RTT和带宽
	uint64_t total_bytes = q->m_size + ((q->m_size-1) / packet_payload_size + 1) * (CustomHeader::GetStaticWholeHeaderSize() - IntHeader::GetStaticSize()); // translate to the minimum bytes required (with header but no INT)

	// printf("[TEST] total_bytes: %lu\n", total_bytes); 
	// std::cout << "[TEST] base rtt: " << base_rtt << std::endl;

	uint64_t standalone_fct = base_rtt + total_bytes * 8000000000lu / b; // 计算理论FCT
	// sip, dip, sport, dport, size (B), start_time, fct (ns), standalone_fct (ns)
	fprintf(fout, "%08x %08x %u %u %lu %lu %lu %lu\n", q->sip.Get(), q->dip.Get(), q->sport, q->dport, q->m_size, q->startTime.GetTimeStep(), (Simulator::Now() - q->startTime).GetTimeStep(), standalone_fct);
	// printf("[TEST] fout: %08x %08x %u %u %lu %lu %lu %lu\n", q->sip.Get(), q->dip.Get(), q->sport, q->dport, q->m_size, q->startTime.GetTimeStep(), (Simulator::Now() - q->startTime).GetTimeStep(), standalone_fct); 
	fflush(fout);

	// remove rxQp from the receiver
	Ptr<Node> dstNode = n.Get(did);
	Ptr<RdmaDriver> rdma = dstNode->GetObject<RdmaDriver> ();
	rdma->m_rdma->DeleteRxQp(q->sip.Get(), q->m_pg, q->sport);

	completed_flows++;
	std::cout << "[TEST] completed_flows: " << completed_flows << std::endl;
    if (completed_flows == flow_num) {
        // 所有流都完成了,停止仿真
        std::cout << GetCurrentTime() << "All flows completed. Stopping simulation.\n";
        Simulator::Stop();
    }
}

void get_pfc(FILE* fout, Ptr<QbbNetDevice> dev, uint32_t type){
	fprintf(fout, "%lu %u %u %u %u\n", Simulator::Now().GetTimeStep(), dev->GetNode()->GetId(), dev->GetNode()->GetNodeType(), dev->GetIfIndex(), type);
}

struct QlenDistribution{
	vector<uint32_t> cnt; // cnt[i] is the number of times that the queue len is i KB

	void add(uint32_t qlen){
		uint32_t kb = qlen / 1000;
		if (cnt.size() < kb+1)
			cnt.resize(kb+1);
		cnt[kb]++;
	}
};

// 用于收集队列长度分布
map<uint32_t, map<uint32_t, QlenDistribution> > queue_result;

void monitor_buffer(FILE* qlen_output, NodeContainer *n){
	for (uint32_t i = 0; i < n->GetN(); i++){
		if (n->Get(i)->GetNodeType() == 1){ // is switch
			Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(n->Get(i));
			if (queue_result.find(i) == queue_result.end())
				queue_result[i];
			for (uint32_t j = 1; j < sw->GetNDevices(); j++){
				uint32_t size = 0;
				for (uint32_t k = 0; k < SwitchMmu::qCnt; k++)
					size += sw->m_mmu->egress_bytes[j][k];
				queue_result[i][j].add(size);
			}
		} else if (n->Get(i)->GetNodeType() == 2){ // is DCISwitch
			Ptr<DCISwitchNode> sw = DynamicCast<DCISwitchNode>(n->Get(i));
			if (queue_result.find(i) == queue_result.end())
				queue_result[i];
			for (uint32_t j = 1; j < sw->GetNDevices(); j++){
				uint32_t size = 0;
				for (uint32_t k = 0; k < SwitchMmu::qCnt; k++)
					size += sw->m_mmu->egress_bytes[j][k];
				queue_result[i][j].add(size);
			}
		}
	}
	if (Simulator::Now().GetTimeStep() % qlen_dump_interval == 0){
		fprintf(qlen_output, "time: %lu\n", Simulator::Now().GetTimeStep());
		for (auto &it0 : queue_result)
			for (auto &it1 : it0.second){
				fprintf(qlen_output, "%u %u", it0.first, it1.first);
				auto &dist = it1.second.cnt;
				for (uint32_t i = 0; i < dist.size(); i++)
					fprintf(qlen_output, " %u", dist[i]);
				fprintf(qlen_output, "\n");
			}
		fflush(qlen_output);
	}
	if (Simulator::Now().GetTimeStep() < qlen_mon_end)
		Simulator::Schedule(NanoSeconds(qlen_mon_interval), &monitor_buffer, qlen_output, n);
}

// 路由相关 ----------------------------------------------------------
// 单个host的路由计算函数，基于广度优先搜索算法计算从host到其他节点的最短路径
	// 计算从源主机到所有其他节点的最短路径
	// 计算路径上的延迟、传输延迟和带宽
	// 建立每个节点到目标主机的下一跳映射关系
	// 只允许通过交换机转发，不允许通过主机中转
/*
void CalculateRoute(Ptr<Node> host){
	vector<Ptr<Node> > q; // queue for the BFS.
	map<Ptr<Node>, int> dis; // Distance from the host to each node.
	map<Ptr<Node>, uint64_t> delay;
	map<Ptr<Node>, uint64_t> txDelay;
	map<Ptr<Node>, uint64_t> bw;
	// init BFS.
	q.push_back(host);
	dis[host] = 0;
	delay[host] = 0;
	txDelay[host] = 0;
	bw[host] = 0xfffffffffffffffflu;
	// BFS.
	for (int i = 0; i < (int)q.size(); i++){
		Ptr<Node> now = q[i];
		int d = dis[now];
		for (auto it = nbr2if[now].begin(); it != nbr2if[now].end(); it++){
			// skip down link
			if (!it->second.up)
				continue;
			Ptr<Node> next = it->first;
			
			// If 'next' have not been visited.
			if (dis.find(next) == dis.end()){
				dis[next] = d + 1;
				delay[next] = delay[now] + it->second.delay;
				txDelay[next] = txDelay[now] + packet_payload_size * 1000000000lu * 8 / it->second.bw;
				bw[next] = std::min(bw[now], it->second.bw);
				// we only enqueue switch, because we do not want packets to go through host as middle point
				if (next->GetNodeType() == 1)
					q.push_back(next);
				else if (next->GetNodeType() == 2) // is DCISwitch
					q.push_back(next);
			}
			// if 'now' is on the shortest path from 'next' to 'host'.
			if (d + 1 == dis[next]){
				nextHop[next][host].push_back(now);
			}
		}
	}
	for (auto it : delay)
		pairDelay[it.first][host] = it.second;
	for (auto it : txDelay)
		pairTxDelay[it.first][host] = it.second;
	for (auto it : bw)
		pairBw[it.first->GetId()][host->GetId()] = it.second;
}
*/

// NEW 更新后的BFS,记录所有候选路径中时延最小的，带宽最大的
void CalculateRoute(Ptr<Node> host) {
    // 定义路径信息结构
    struct PathInfo {
        uint64_t delay;      // 路径总延迟
        uint64_t txDelay;    // 路径传输延迟
        uint64_t bw;         // 路径最小带宽
        vector<Ptr<Node>> path;  // 路径上的节点
    };
    
    // BFS所需的数据结构
    vector<Ptr<Node>> q;  // BFS队列
    map<Ptr<Node>, int> dis;  // 到每个节点的最小跳数
    map<Ptr<Node>, vector<PathInfo>> nodePaths;  // 每个节点的所有候选路径
    
    // 初始化
    q.push_back(host);
    dis[host] = 0;
    nodePaths[host].push_back({0, 0, 0xfffffffffffffffflu, {host}});
    
    // BFS遍历
    for (int i = 0; i < (int)q.size(); i++) {
        Ptr<Node> now = q[i];
        int d = dis[now];
        
        // 遍历当前节点的所有邻居
        for (auto it = nbr2if[now].begin(); it != nbr2if[now].end(); it++) {
            if (!it->second.up)
                continue;
            Ptr<Node> next = it->first;
            
            // 计算到达next的新路径信息
            for (const auto& curPath : nodePaths[now]) {
                PathInfo newPath = curPath;
                newPath.path.push_back(next);
                newPath.delay = curPath.delay + it->second.delay;
                newPath.txDelay = curPath.txDelay + packet_payload_size * 1000000000lu * 8 / it->second.bw;
                newPath.bw = std::min(curPath.bw, it->second.bw);
                
                // 如果是第一次访问next或者是相同跳数的其他路径
                if (dis.find(next) == dis.end()) {
                    dis[next] = d + 1;
                    nodePaths[next].push_back(newPath);
                    // 只将交换机加入队列继续扩展
                    if (next->GetNodeType() == 1 || next->GetNodeType() == 2)
                        q.push_back(next);

					// 添加nextHop (仅在第一次访问时添加)
                    nextHop[next][host].push_back(now);
                } else if (d + 1 == dis[next]) {
                    nodePaths[next].push_back(newPath);

					// 检查是否已经添加过这个nextHop
                    bool already_added = false;
                    for (const auto& nh : nextHop[next][host]) {
                        if (nh == now) {
                            already_added = true;
                            break;
                        }
                    }
                    if (!already_added) {
                        nextHop[next][host].push_back(now);
                    }
                }
                
            }
        }
    }
    
    // 对每个可达节点，选择最优路径
    for (const auto& it : nodePaths) {
        Ptr<Node> node = it.first;
        const vector<PathInfo>& paths = it.second;
        
        if (paths.empty())
            continue;
            
        // 1. 找出最小delay
        uint64_t minDelay = UINT64_MAX;
        for (const auto& p : paths) {
            minDelay = std::min(minDelay, p.delay);
        }
        
        // 2. 在最小delay的路径中找出最大带宽
        uint64_t maxBw = 0;
        uint64_t bestTxDelay = 0;
        for (const auto& p : paths) {
            if (p.delay == minDelay && p.bw > maxBw) {
                maxBw = p.bw;
                bestTxDelay = p.txDelay;
            }
        }
        
        // 3. 记录最优路径的信息
        pairDelay[node][host] = minDelay;
        pairTxDelay[node][host] = bestTxDelay;
        pairBw[node->GetId()][host->GetId()] = maxBw;
    }
}

/*
void CalculateRoute(Ptr<Node> host) {
    struct PathInfo {
        uint64_t delay;      // 总延迟
        uint64_t txDelay;    // 传输延迟
        uint64_t bw;         // 最小带宽
        Ptr<Node> next_hop;  // 下一跳节点
    };

    // BFS所需的数据结构
    vector<Ptr<Node>> q;
    map<Ptr<Node>, int> dis;                    // 到每个节点的最小跳数
    map<Ptr<Node>, vector<PathInfo>> best_paths; // 每个节点的最优路径信息

    // 初始化
    q.push_back(host);
    dis[host] = 0;
    best_paths[host].push_back({0, 0, 0xfffffffffffffffflu, nullptr});

    // BFS遍历
    for (int i = 0; i < (int)q.size(); i++) {
        Ptr<Node> now = q[i];
        int d = dis[now];

        // 遍历当前节点的所有邻居
        for (auto it = nbr2if[now].begin(); it != nbr2if[now].end(); it++) {
            if (!it->second.up)
                continue;
            Ptr<Node> next = it->first;

            // 计算经过当前链路的delay和带宽
            uint64_t link_delay = it->second.delay;
            uint64_t link_bw = it->second.bw;
            uint64_t link_tx_delay = packet_payload_size * 1000000000lu * 8 / link_bw;

            // 处理第一次访问的节点
            if (dis.find(next) == dis.end()) {
                dis[next] = d + 1;
                // 只将交换机加入队列继续扩展
                if (next->GetNodeType() == 1 || next->GetNodeType() == 2)
                    q.push_back(next);

                // 继承当前节点的最优路径
                for (const auto& p : best_paths[now]) {
                    uint64_t new_delay = p.delay + link_delay;
                    uint64_t new_tx_delay = p.txDelay + link_tx_delay;
                    uint64_t new_bw = std::min(p.bw, link_bw);
                    best_paths[next].push_back({new_delay, new_tx_delay, new_bw, now});
                }

                // 添加nextHop
                nextHop[next][host].push_back(now);
            }
            // 处理等距节点
            else if (d + 1 == dis[next]) {
                // 检查新路径是否更优
                bool need_update = false;
                uint64_t min_delay = UINT64_MAX;
                uint64_t max_bw = 0;

                // 计算当前节点所有路径到next的延迟和带宽
                for (const auto& p : best_paths[now]) {
                    uint64_t new_delay = p.delay + link_delay;
                    uint64_t new_tx_delay = p.txDelay + link_tx_delay;
                    uint64_t new_bw = std::min(p.bw, link_bw);

                    // 更新最优值
                    if (new_delay < min_delay) {
                        min_delay = new_delay;
                        max_bw = new_bw;
                        need_update = true;
                    } else if (new_delay == min_delay && new_bw > max_bw) {
                        max_bw = new_bw;
                        need_update = true;
                    }
                }

                // 如果找到更优路径，更新best_paths
                if (need_update) {
                    best_paths[next].clear();
                    for (const auto& p : best_paths[now]) {
                        uint64_t new_delay = p.delay + link_delay;
                        uint64_t new_tx_delay = p.txDelay + link_tx_delay;
                        uint64_t new_bw = std::min(p.bw, link_bw);
                        if (new_delay == min_delay && new_bw == max_bw) {
                            best_paths[next].push_back({new_delay, new_tx_delay, new_bw, now});
                        }
                    }
                }

                // 检查并添加nextHop
                bool already_added = false;
                for (const auto& nh : nextHop[next][host]) {
                    if (nh == now) {
                        already_added = true;
                        break;
                    }
                }
                if (!already_added) {
                    nextHop[next][host].push_back(now);
                }
            }
        }
    }

    // 记录最终结果
    for (const auto& it : best_paths) {
        if (it.second.empty())
            continue;
            
        Ptr<Node> node = it.first;
        const PathInfo& best = it.second[0]; // 已经保证了best_paths中只包含最优路径

        pairDelay[node][host] = best.delay;
        pairTxDelay[node][host] = best.txDelay;
        pairBw[node->GetId()][host->GetId()] = best.bw;
    }
}
*/


// 计算所有host的路由
void CalculateRoutes(NodeContainer &n){
	for (int i = 0; i < (int)n.GetN(); i++){
		Ptr<Node> node = n.Get(i);
		if (node->GetNodeType() == 0)
			CalculateRoute(node);
	}
}

// [重要]路由表设置函数，负责为网络中的所有节点（主机、交换机、DCI交换机）配置路由表项。
// 它使用预先计算好的下一跳信息（存储在 nextHop 数据结构中）来设置每个节点的路由表
// 为每个主机的RdmaHw对象(调用AddTableEntry)设置路由表
void SetRoutingEntries(){
	// 将已经构建好的路由表信息写入本地CSV文件
		std::string route_file = output_dir + "routing_table.csv";

	// For each node.
	for (auto i = nextHop.begin(); i != nextHop.end(); i++){
		Ptr<Node> node = i->first;
		auto &table = i->second;
		for (auto j = table.begin(); j != table.end(); j++){
			// The destination node.
			Ptr<Node> dst = j->first;
			// The IP address of the dst.
			Ipv4Address dstAddr = dst->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
			// The next hops towards the dst.
			vector<Ptr<Node> > nexts = j->second;
			for (int k = 0; k < (int)nexts.size(); k++){
				Ptr<Node> next = nexts[k];
				uint32_t interface = nbr2if[node][next].idx;
				// 根据节点类型，添加路由表项
				if (node->GetNodeType() == 1) // is switch
					DynamicCast<SwitchNode>(node)->AddTableEntry(dstAddr, interface);
				else if (node->GetNodeType() == 2) // is DCISwitch
					DynamicCast<DCISwitchNode>(node)->AddTableEntry(dstAddr, interface);
				else{ // is host
					node->GetObject<RdmaDriver>()->m_rdma->AddTableEntry(dstAddr, interface);
				}
			}
		}

		// 将路由表信息写入CSV文件
		static std::ofstream route_csv(route_file.c_str(), std::ios::out);
		// 写入标题行（仅在文件为空时写入）
		static bool header_written = false;
		if (!header_written) {
			route_csv << "src_id,dst_id,next_hop_id\n";
			header_written = true;
		}
		for (auto j2 = table.begin(); j2 != table.end(); j2++) {
			Ptr<Node> dst2 = j2->first;
			route_csv << node->GetId() << "," << dst2->GetId() << ",";
			bool first = true;
			for (auto nh : j2->second) {
				if (!first) route_csv << ";";
				route_csv << nh->GetId();
				first = false;
			}
			route_csv << "\n";
		}
	}
	std::cout << GetCurrentTime() << "Routing table set and saved to " << route_file << std::endl;

}

// 链路故障处理函数
// take down the link between a and b, and redo the routing
void TakeDownLink(NodeContainer n, Ptr<Node> a, Ptr<Node> b){
	if (!nbr2if[a][b].up)
		return;
	// take down link between a and b
	nbr2if[a][b].up = nbr2if[b][a].up = false;
	nextHop.clear();
	CalculateRoutes(n);
	// clear routing tables
	for (uint32_t i = 0; i < n.GetN(); i++){
		if (n.Get(i)->GetNodeType() == 1)
			DynamicCast<SwitchNode>(n.Get(i))->ClearTable();
		else if (n.Get(i)->GetNodeType() == 2) // is DCISwitch
			DynamicCast<DCISwitchNode>(n.Get(i))->ClearTable();
		else
			n.Get(i)->GetObject<RdmaDriver>()->m_rdma->ClearTable();
	}
	DynamicCast<QbbNetDevice>(a->GetDevice(nbr2if[a][b].idx))->TakeDown();
	DynamicCast<QbbNetDevice>(b->GetDevice(nbr2if[b][a].idx))->TakeDown();
	// reset routing table
	SetRoutingEntries();

	// redistribute qp on each host
	for (uint32_t i = 0; i < n.GetN(); i++){
		if (n.Get(i)->GetNodeType() == 0)
			n.Get(i)->GetObject<RdmaDriver>()->m_rdma->RedistributeQp();
	}
}

// 路由相关 ----------------------------------------------------------
uint64_t get_nic_rate(NodeContainer &n){
	for (uint32_t i = 0; i < n.GetN(); i++)
		if (n.Get(i)->GetNodeType() == 0)
			return DynamicCast<QbbNetDevice>(n.Get(i)->GetDevice(1))->GetDataRate().GetBitRate();
}


// =================================== 主函数 ===================================
int main(int argc, char *argv[])
{
	// LogComponentEnable ("RdmaClient", LOG_LEVEL_INFO); // 启用UDP回显客户端应用程序的日志记录
	// LogComponentEnable ("GlobalRouteManager", LOG_LEVEL_INFO); // 启用UDP回显客户端应用程序的日志记录

	clock_t begint, endt;
	begint = clock();
#ifndef PGO_TRAINING
	if (argc > 1)
#else
	if (true)
#endif
	{
		// Step 1: Read the configuration file 读取配置文件
		std::ifstream conf;
#ifndef PGO_TRAINING
		conf.open(argv[1]);
		if (!conf.is_open())
		{
			std::cout << "Failed to open configuration file: " << argv[1] << std::endl;
			return 1;
		}
#else
		conf.open(PATH_TO_PGO_CONFIG);
#endif
		while (!conf.eof())
		{
			std::string key;
			conf >> key;

			//std::cout << conf.cur << "\n";

			if (key.compare("ENABLE_QCN") == 0)
			{
				uint32_t v;
				conf >> v;
				enable_qcn = v;
				if (enable_qcn)
					std::cout << std::left << setw(27) << "ENABLE_QCN" << "Yes" << "\n";
				else
					std::cout << std::left << setw(27) << "ENABLE_QCN" << "No" << "\n";
			}
			else if (key.compare("USE_DYNAMIC_PFC_THRESHOLD") == 0)
			{
				uint32_t v;
				conf >> v;
				use_dynamic_pfc_threshold = v;
				if (use_dynamic_pfc_threshold)
					std::cout << std::left << setw(27) << "USE_DYNAMIC_PFC_THRESHOLD" << "Yes" << "\n";
				else
					std::cout << std::left << setw(27) << "USE_DYNAMIC_PFC_THRESHOLD" << "No" << "\n";
			}
			else if (key.compare("CLAMP_TARGET_RATE") == 0)
			{
				uint32_t v;
				conf >> v;
				clamp_target_rate = v;
				if (clamp_target_rate)
					std::cout << std::left << setw(27) << "CLAMP_TARGET_RATE" << "Yes" << "\n";
				else
					std::cout << std::left << setw(27) << "CLAMP_TARGET_RATE" << "No" << "\n";
			}
			else if (key.compare("PAUSE_TIME") == 0)
			{
				double v;
				conf >> v;
				pause_time = v;
				std::cout << std::left << setw(27) << "PAUSE_TIME" << pause_time << "\n";
			}
			else if (key.compare("DATA_RATE") == 0)
			{
				std::string v;
				conf >> v;
				data_rate = v;
				std::cout << std::left << setw(27) << "DATA_RATE" << data_rate << "\n";
			}
			else if (key.compare("LINK_DELAY") == 0)
			{
				std::string v;
				conf >> v;
				link_delay = v;
				std::cout << std::left << setw(27) << "LINK_DELAY" << link_delay << "\n";
			}
			else if (key.compare("PACKET_PAYLOAD_SIZE") == 0)
			{
				uint32_t v;
				conf >> v;
				packet_payload_size = v;
				std::cout << std::left << setw(27) << "PACKET_PAYLOAD_SIZE" << packet_payload_size << "\n";
			}
			else if (key.compare("L2_CHUNK_SIZE") == 0)
			{
				uint32_t v;
				conf >> v;
				l2_chunk_size = v;
				std::cout << std::left << setw(27) << "L2_CHUNK_SIZE" << l2_chunk_size << "\n";
			}
			else if (key.compare("L2_ACK_INTERVAL") == 0)
			{
				uint32_t v;
				conf >> v;
				l2_ack_interval = v;
				std::cout << std::left << setw(27) << "L2_ACK_INTERVAL" << l2_ack_interval << "\n";
			}
			else if (key.compare("L2_BACK_TO_ZERO") == 0)
			{
				uint32_t v;
				conf >> v;
				l2_back_to_zero = v;
				if (l2_back_to_zero)
					std::cout << std::left << setw(27) << "L2_BACK_TO_ZERO" << "Yes" << "\n";
				else
					std::cout << std::left << setw(27) << "L2_BACK_TO_ZERO" << "No" << "\n";
			}
			else if (key.compare("WORKING_DIR") == 0)
			{
				conf >> working_dir;
				std::cout << std::left << setw(27) << "WORKING_DIR" << working_dir << "\n";
			}
			else if (key.compare("OUTPUT_DIR") == 0)
			{
				conf >> output_dir;
				std::cout << std::left << setw(27) << "OUTPUT_DIR" << output_dir << "\n";
			}
			else if (key.compare("TOPOLOGY_FILE") == 0)
			{
				std::string v;
				conf >> v;
				topology_file = replace_config_variables(v);
				std::cout << std::left << setw(27) << "TOPOLOGY_FILE" << topology_file << "\n";
			}
			else if (key.compare("FLOW_FILE") == 0)
			{
				std::string v;
				conf >> v;
				flow_file = replace_config_variables(v);
				std::cout << std::left << setw(27) << "FLOW_FILE" << flow_file << "\n";
			}
			else if (key.compare("TRACE_FILE") == 0)
			{
				std::string v;
				conf >> v;
				trace_file = replace_config_variables(v);
				std::cout << std::left << setw(27) << "TRACE_FILE" << trace_file << "\n";
			}
			else if (key.compare("TRACE_OUTPUT_FILE") == 0)
			{
				std::string temp;
				conf >> temp;
				trace_output_file = replace_config_variables(temp);
				if (argc > 2)
				{
					trace_output_file = trace_output_file + std::string(argv[2]);
				}
				std::cout << std::left << setw(27) << "TRACE_OUTPUT_FILE" << trace_output_file << "\n";
			}
			else if (key.compare("SIMULATOR_STOP_TIME") == 0)
			{
				double v;
				conf >> v;
				simulator_stop_time = v;
				std::cout << std::left << setw(27) << "SIMULATOR_STOP_TIME (s)" << simulator_stop_time << "\n";
			}
			else if (key.compare("ALPHA_RESUME_INTERVAL") == 0)
			{
				double v;
				conf >> v;
				alpha_resume_interval = v;
				std::cout << std::left << setw(27) << "ALPHA_RESUME_INTERVAL" << alpha_resume_interval << "\n";
			}
			else if (key.compare("RP_TIMER") == 0)
			{
				double v;
				conf >> v;
				rp_timer = v;
				std::cout << std::left << setw(27) << "RP_TIMER" << rp_timer << "\n";
			}
			else if (key.compare("EWMA_GAIN") == 0)
			{
				double v;
				conf >> v;
				ewma_gain = v;
				std::cout << std::left << setw(27) << "EWMA_GAIN" << ewma_gain << "\n";
			}
			else if (key.compare("FAST_RECOVERY_TIMES") == 0)
			{
				uint32_t v;
				conf >> v;
				fast_recovery_times = v;
				std::cout << std::left << setw(27) << "FAST_RECOVERY_TIMES" << fast_recovery_times << "\n";
			}
			else if (key.compare("RATE_AI") == 0)
			{
				std::string v;
				conf >> v;
				rate_ai = v;
				std::cout << std::left << setw(27) << "RATE_AI" << rate_ai << "\n";
			}
			else if (key.compare("RATE_HAI") == 0)
			{
				std::string v;
				conf >> v;
				rate_hai = v;
				std::cout << std::left << setw(27) << "RATE_HAI" << rate_hai << "\n";
			}
			else if (key.compare("ERROR_RATE_PER_LINK") == 0)
			{
				double v;
				conf >> v;
				error_rate_per_link = v;
				std::cout << std::left << setw(27) << "ERROR_RATE_PER_LINK" << error_rate_per_link << "\n";
			}
			else if (key.compare("CC_MODE") == 0){
				conf >> cc_mode;
				if (cc_mode == 1)
					std::cout << std::left << setw(27) << "CC_MODE" << "DCQCN" << '\n';
				else if (cc_mode == 3)
					std::cout << std::left << setw(27) << "CC_MODE" << "HPCC" << '\n';
				else if (cc_mode == 7)
					std::cout << std::left << setw(27) << "CC_MODE" << "TIMELY" << '\n';
				else if (cc_mode == 8)
					std::cout << std::left << setw(27) << "CC_MODE" << "DCTCP" << '\n';
				else if (cc_mode == 10)
					std::cout << std::left << setw(27) << "CC_MODE" << "HPCC-PINT" << '\n';
			}else if (key.compare("RATE_DECREASE_INTERVAL") == 0){
				double v;
				conf >> v;
				rate_decrease_interval = v;
				std::cout << std::left << setw(27) << "RATE_DECREASE_INTERVAL" << rate_decrease_interval << "\n";
			}else if (key.compare("MIN_RATE") == 0){
				conf >> min_rate;
				std::cout << std::left << setw(27) << "MIN_RATE" << min_rate << "\n";
			}else if (key.compare("FCT_OUTPUT_FILE") == 0){
				std::string temp;
				conf >> temp;
				fct_output_file = replace_config_variables(temp);
				std::cout << std::left << setw(27) << "FCT_OUTPUT_FILE" << fct_output_file << '\n';
			}else if (key.compare("HAS_WIN") == 0){
				conf >> has_win;
				std::cout << std::left << setw(27) << "HAS_WIN" << has_win << "\n";
			}else if (key.compare("GLOBAL_T") == 0){
				conf >> global_t;
				std::cout << std::left << setw(27) << "GLOBAL_T" << global_t << '\n';
			}else if (key.compare("MI_THRESH") == 0){
				conf >> mi_thresh;
				std::cout << std::left << setw(27) << "MI_THRESH" << mi_thresh << '\n';
			}else if (key.compare("VAR_WIN") == 0){
				uint32_t v;
				conf >> v;
				var_win = v;
				std::cout << std::left << setw(27) << "VAR_WIN" << v << '\n';
			}else if (key.compare("FAST_REACT") == 0){
				uint32_t v;
				conf >> v;
				fast_react = v;
				std::cout << std::left << setw(27) << "FAST_REACT" << v << '\n';
			}else if (key.compare("U_TARGET") == 0){
				conf >> u_target;
				std::cout << std::left << setw(27) << "U_TARGET" << u_target << '\n';
			}else if (key.compare("INT_MULTI") == 0){
				conf >> int_multi;
				std::cout << std::left << setw(27) << "INT_MULTI" << int_multi << '\n';
			}else if (key.compare("RATE_BOUND") == 0){
				uint32_t v;
				conf >> v;
				rate_bound = v;
				std::cout << std::left << setw(27) << "RATE_BOUND" << rate_bound << '\n';
			}else if (key.compare("ACK_HIGH_PRIO") == 0){
				conf >> ack_high_prio;
				std::cout << std::left << setw(27) << "ACK_HIGH_PRIO" << ack_high_prio << '\n';
			}else if (key.compare("DCTCP_RATE_AI") == 0){
				conf >> dctcp_rate_ai;
				std::cout << std::left << setw(27) << "DCTCP_RATE_AI" << dctcp_rate_ai << "\n";
			}else if (key.compare("PFC_OUTPUT_FILE") == 0){
				std::string temp;
				conf >> temp;
				pfc_output_file = replace_config_variables(temp);
				std::cout << std::left << setw(27) << "PFC_OUTPUT_FILE" << pfc_output_file << '\n';
			}else if (key.compare("ENABLE_LINK_UTIL_RECORD") == 0){
				uint32_t v;
				conf >> v;
				enable_link_util_record = v;
				std::cout << std::left << setw(27) << "ENABLE_LINK_UTIL_RECORD" << (enable_link_util_record ? "Yes" : "No") << "\n";
			}else if (key.compare("LINK_UTIL_OUTPUT_FILE") == 0){
				std::string temp;
				conf >> temp;
				link_util_output_file = replace_config_variables(temp);
				std::cout << std::left << setw(27) << "LINK_UTIL_OUTPUT_FILE" << link_util_output_file << '\n';
			}else if (key.compare("LINK_DOWN") == 0){
				conf >> link_down_time >> link_down_A >> link_down_B;
				std::cout << std::left << setw(27) << "LINK_DOWN" << link_down_time << ' '<< link_down_A << ' ' << link_down_B << '\n';
			}else if (key.compare("ENABLE_TRACE") == 0){
				conf >> enable_trace;
				if (enable_trace)
					std::cout << std::left << setw(27) << "ENABLE_TRACE" << "YES" << '\n';
				else
					std::cout << std::left << setw(27) << "ENABLE_TRACE" << "NO" << '\n';
			}else if (key.compare("KMAX_MAP") == 0){
				int n_k ;
				conf >> n_k;
				std::cout << std::left << setw(26) << "KMAX_MAP";
				for (int i = 0; i < n_k; i++){
					uint64_t rate;
					uint32_t k;
					conf >> rate >> k;
					rate2kmax[rate] = k;
					std::cout << ' ' << rate << ' ' << k;
				}
				std::cout<<'\n';
			}else if (key.compare("KMIN_MAP") == 0){
				int n_k ;
				conf >> n_k;
				std::cout << std::left << setw(26) << "KMIN_MAP";
				for (int i = 0; i < n_k; i++){
					uint64_t rate;
					uint32_t k;
					conf >> rate >> k;
					rate2kmin[rate] = k;
					std::cout << ' ' << rate << ' ' << k;
				}
				std::cout<<'\n';
			}else if (key.compare("PMAX_MAP") == 0){
				int n_k ;
				conf >> n_k;
				std::cout << std::left << setw(26) << "PMAX_MAP";
				for (int i = 0; i < n_k; i++){
					uint64_t rate;
					double p;
					conf >> rate >> p;
					rate2pmax[rate] = p;
					std::cout << ' ' << rate << ' ' << p;
				}
				std::cout<<'\n';
			}else if (key.compare("BUFFER_SIZE") == 0){
				conf >> buffer_size;
				std::cout << std::left << setw(27) << "BUFFER_SIZE" << buffer_size << '\n';
			} else if (key.compare("DCI_BUFFER_SIZE") == 0){
				conf >> dci_buffer_size;
				std::cout << std::left << setw(27) << "DCI_BUFFER_SIZE" << dci_buffer_size << '\n';
			} else if (key.compare("QLEN_MON_FILE") == 0){
				std::string temp;
				conf >> temp;
				qlen_mon_file = replace_config_variables(temp);
				std::cout << std::left << setw(27) << "QLEN_MON_FILE" << qlen_mon_file << '\n';
			}else if (key.compare("QLEN_MON_START") == 0){
				conf >> qlen_mon_start;
				std::cout << std::left << setw(27) << "QLEN_MON_START" << qlen_mon_start << '\n';
			}else if (key.compare("QLEN_MON_END") == 0){
				conf >> qlen_mon_end;
				std::cout << std::left << setw(27) << "QLEN_MON_END" << qlen_mon_end << '\n';
			}else if (key.compare("MULTI_RATE") == 0){
				int v;
				conf >> v;
				multi_rate = v;
				std::cout << std::left << setw(27) << "MULTI_RATE" << multi_rate << '\n';
			}else if (key.compare("SAMPLE_FEEDBACK") == 0){
				int v;
				conf >> v;
				sample_feedback = v;
				std::cout << std::left << setw(27) << "SAMPLE_FEEDBACK" << sample_feedback << '\n';
			}else if(key.compare("PINT_LOG_BASE") == 0){
				conf >> pint_log_base;
				std::cout << std::left << setw(27) << "PINT_LOG_BASE" << pint_log_base << '\n';
			}else if (key.compare("PINT_PROB") == 0){
				conf >> pint_prob;
				std::cout << std::left << setw(27) << "PINT_PROB" << pint_prob << '\n';
			// NEW
			} else if (key.compare("ROUTING_MODE") == 0){
				conf >> routing_mode;
				if (routing_mode == 0)
					std::cout << std::left << setw(27) << "ROUTING_MODE" << "ECMP" << '\n';
				else if (routing_mode == 1)
					std::cout << std::left << setw(27) << "ROUTING_MODE" << "UCMP" << '\n';
				else if (routing_mode == 2)
					std::cout << std::left << setw(27) << "ROUTING_MODE" << "Ours" << '\n';
				// NEW
			}else if (key.compare("W_DL") == 0) {
				conf >> w_dl;
				std::cout << std::left << setw(27) << "W_DL" << w_dl << '\n';
			}else if (key.compare("W_BW") == 0) {
				conf >> w_bw;
				std::cout << std::left << setw(27) << "W_BW" << w_bw << '\n';
			}else if (key.compare("S_STATIC") == 0) {
				conf >> s_static;
				std::cout << std::left << setw(27) << "S_STATIC" << s_static << '\n';
			}else if (key.compare("W_QL") == 0) {
				conf >> w_ql;
				std::cout << std::left << setw(27) << "W_QL" << w_ql << '\n';
			}else if (key.compare("W_TL") == 0) {
				conf >> w_tl;
				std::cout << std::left << setw(27) << "W_TL" << w_tl << '\n';
			}else if (key.compare("W_DP") == 0) {
				conf >> w_dp;
				std::cout << std::left << setw(27) << "W_DP" << w_dp << '\n';
			}else if (key.compare("S_CONG") == 0) {
				conf >> s_cong;
				std::cout << std::left << setw(27) << "S_CONG" << s_cong << '\n';
			}else if (key.compare("ALPHA") == 0) {
				conf >> alpha_cost;
				std::cout << std::left << setw(27) << "ALPHA" << alpha_cost << '\n';
			}else if (key.compare("BETA") == 0) {
				conf >> beta_cost;
				std::cout << std::left << setw(27) << "BETA" << beta_cost << '\n';
			}else if (key.compare("S_TOTAL") == 0) {
				conf >> s_total;
				std::cout << std::left << setw(27) << "S_TOTAL" << s_total << '\n';
			}

			fflush(stdout);
		}
		conf.close();
	}
	else
	{
		std::cout << "Error: require a config file\n";
		fflush(stdout);
		return 1;
	}


	bool dynamicth = use_dynamic_pfc_threshold;
	Config::SetDefault("ns3::QbbNetDevice::PauseTime", UintegerValue(pause_time));
	Config::SetDefault("ns3::QbbNetDevice::QcnEnabled", BooleanValue(enable_qcn));
	Config::SetDefault("ns3::QbbNetDevice::DynamicThreshold", BooleanValue(dynamicth));

	// set int_multi
	IntHop::multi = int_multi;
	// IntHeader::mode
	if (cc_mode == 7) // timely, use ts
		IntHeader::mode = IntHeader::TS;
	else if (cc_mode == 3) // hpcc, use int
		IntHeader::mode = IntHeader::NORMAL;
	else if (cc_mode == 10) // hpcc-pint
		IntHeader::mode = IntHeader::PINT;
	else // others, no extra header
		IntHeader::mode = IntHeader::NONE;

	// Set Pint
	if (cc_mode == 10){
		Pint::set_log_base(pint_log_base);
		IntHeader::pint_bytes = Pint::get_n_bytes();
		printf("PINT bits: %d bytes: %d\n", Pint::get_n_bits(), Pint::get_n_bytes());
	}

	//SeedManager::SetSeed(time(NULL));

	// Step 2: 构建网络拓扑
	// 2.1 读取拓扑、预设网络流、所检测节点信息
	// 检查拓扑文件是否存在
	topof.open(topology_file.c_str()); // 读取TOPOLOGY_FILE文件，指定网络拓扑
	if (!topof.is_open()) {
		std::cerr << "Error: Unable to open topology file " << topology_file << std::endl;
		return 1;
	}
	
	flowf.open(flow_file.c_str());  // 读取FLOW_FILE文件，指定网络流量
	if (!flowf.is_open()) {
		std::cerr << "Error: Unable to open flow file " << flow_file << std::endl;
		return 1;
	}

	tracef.open(trace_file.c_str()); // 读取TRACE_FILE文件，指定监测节点
	if (!tracef.is_open()) {
		std::cerr << "Error: Unable to open trace file " << trace_file << std::endl;
		return 1;
	}
	uint32_t node_num, switch_num, link_num, trace_num;
	// C++中的>>操作符在读取时会自动忽略所有"空白字符", 包括：空格、制表符、换行符、回车符等
	SkipComments(flowf) >> flow_num;
	SkipComments(tracef) >> trace_num;

	// 读取拓扑基本信息，支持两种格式：
	// switch_num表示Intra DC的交换机, dci_switch_num表示连接广域网的Datacenter Interconnection交换机
	// 1. 单DC格式：node_num switch_num link_num
	// 2. 多DC格式：node_num switch_num dci_switch_num link_num
	std::string line;
	std::getline(SkipComments(topof), line);
	std::istringstream iss(line);
	// std::cout << GetCurrentTime() <<"[TEST]line: " << line << std::endl;
	uint32_t dci_switch_num = 0;
	
	iss >> node_num >> switch_num;
	// 检查是否有第四个参数，判断是否为多DC拓扑
	uint32_t temp;
	if (iss >> temp >> link_num) {
		// 多DC拓扑格式：有4个参数
		dci_switch_num = temp;
		std::cout << GetCurrentTime() << "[test]Got 4 parameters, node_num: " << node_num << ", switch_num: " << switch_num << ", dci_switch_num: " << dci_switch_num << ", link_num: " << link_num << std::endl;
	} else {
		// 单DC拓扑格式：只有3个参数
		link_num = temp;
		std::cout << GetCurrentTime() << "[test]Got 3 parameters, node_num: " << node_num << ", switch_num: " << switch_num << ", link_num: " << link_num << std::endl;
	}

	// 2.2 根据节点类型，创建服务器节点或交换机节点
	// 2.2.1 初始化节点类型数组
	std::vector<uint32_t> node_type(node_num, 0); // 初始全部为主机节点(0)
	// 对交换机(Intra-DC Switch(Tor, Leaf or Spine) or DCI Switch)节点进行标记
	for (uint32_t i = 0; i < switch_num; i++)
	{
		uint32_t sid;
		SkipComments(topof) >> sid;
		node_type[sid] = 1; // DC内交换机节点(1)
		// std::cout << "[TEST]Intra-DC Switch sid: " << sid << " has been created." << std::endl;	
	}
	// NEW 创建所有DCI交换机节点
	for (uint32_t i = 0; i < dci_switch_num; i++)
	{
		uint32_t dci_sid;
		SkipComments(topof) >> dci_sid;
		node_type[dci_sid] = 2; // DCI交换机节点(2)
		// std::cout << "[test]DCISwitch dci_sid: " << dci_sid << " has been created." << std::endl;
	}

	// 2.2.2 根据节点类型，创建服务器节点或交换机节点
	for (uint32_t i = 0; i < node_num; i++){
		if (node_type[i] == 0) // 创建主机节点
			n.Add(CreateObject<Node>()); 
		else if (node_type[i] == 1){ // 创建DC内交换机节点
			Ptr<SwitchNode> sw = CreateObject<SwitchNode>(); 
			n.Add(sw); // 创建交换机节点
			sw->SetAttribute("EcnEnabled", BooleanValue(enable_qcn));
		}
		else if (node_type[i] == 2){ // 创建DCI交换机节点
			Ptr<DCISwitchNode> sw = CreateObject<DCISwitchNode>(); 
			n.Add(sw); // 创建交换机节点
			sw->SetAttribute("EcnEnabled", BooleanValue(enable_qcn));
			sw->SetAttribute("Mtu", UintegerValue(packet_payload_size));
		}
	}

	NS_LOG_INFO("Create nodes.");
	std::cout << GetCurrentTime() << "[test]Create nodes." << std::endl;

	InternetStackHelper internet;
	internet.Install(n); // 安装Internet Stack

	
	for (uint32_t i = 0; i < node_num; i++){
		if (n.Get(i)->GetNodeType() == 0){ // is server
			serverAddress.resize(i + 1);
			serverAddress[i] = node_id_to_ip(i); // Assign IP to each server
		}
	}

	// Step 3: 使用Qbb助手，辅助创建点到点通信网络
	NS_LOG_INFO("Create channels.");
	std::cout << GetCurrentTime() << "[test]Create channels and links." << std::endl;

	// Explicitly create the channels required by the topology.
	Ptr<RateErrorModel> rem = CreateObject<RateErrorModel>();
	Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
	rem->SetRandomVariable(uv);
	uv->SetStream(50);
	rem->SetAttribute("ErrorRate", DoubleValue(error_rate_per_link));
	rem->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));

	FILE *pfc_file = fopen(pfc_output_file.c_str(), "w");

	QbbHelper qbb;
	Ipv4AddressHelper ipv4;
	// 3.1 读取链路信息
	for (uint32_t i = 0; i < link_num; i++)
	{
		uint32_t src, dst;
		std::string data_rate, link_delay;
		double error_rate;
		SkipComments(topof) >> src >> dst >> data_rate >> link_delay >> error_rate;
		// std::cout << "[TEST]src: " << src << " dst: " << dst << " data_rate: " << data_rate << " link_delay: " << link_delay << " error_rate: " << error_rate << std::endl;

		Ptr<Node> snode = n.Get(src), dnode = n.Get(dst);

		// 设置链路属性
		qbb.SetDeviceAttribute("DataRate", StringValue(data_rate));
		qbb.SetChannelAttribute("Delay", StringValue(link_delay));

		if (error_rate > 0) {
			Ptr<RateErrorModel> rem = CreateObject<RateErrorModel>();
			Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
			rem->SetRandomVariable(uv);
			uv->SetStream(50);
			rem->SetAttribute("ErrorRate", DoubleValue(error_rate));
			rem->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));
			qbb.SetDeviceAttribute("ReceiveErrorModel", PointerValue(rem));
		}
		else {
			qbb.SetDeviceAttribute("ReceiveErrorModel", PointerValue(rem));
		}

		fflush(stdout);

		// 3.2: 给两个节点安装网卡设备/QbbNetDevice+出队列/BEgressQueue+通道管理/QbbChannel
		// Assigne server IP
		// Note: this should be before the automatic assignment below (ipv4.Assign(d)),
		// because we want our IP to be the primary IP (first in the IP address list),
		// so that the global routing is based on our IP
		NetDeviceContainer d = qbb.Install(snode, dnode);
		if (snode->GetNodeType() == 0){
			Ptr<Ipv4> ipv4 = snode->GetObject<Ipv4>(); // source node
			ipv4->AddInterface(d.Get(0));
			ipv4->AddAddress(1, Ipv4InterfaceAddress(serverAddress[src], Ipv4Mask(0xff000000)));
		}
		if (dnode->GetNodeType() == 0){
			Ptr<Ipv4> ipv4 = dnode->GetObject<Ipv4>(); // destination node
			ipv4->AddInterface(d.Get(1));
			ipv4->AddAddress(1, Ipv4InterfaceAddress(serverAddress[dst], Ipv4Mask(0xff000000)));
		}

		// used to create a graph of the topology
		nbr2if[snode][dnode].idx = DynamicCast<QbbNetDevice>(d.Get(0))->GetIfIndex();
		nbr2if[snode][dnode].up = true;
		nbr2if[snode][dnode].delay = DynamicCast<QbbChannel>(DynamicCast<QbbNetDevice>(d.Get(0))->GetChannel())->GetDelay().GetTimeStep();
		nbr2if[snode][dnode].bw = DynamicCast<QbbNetDevice>(d.Get(0))->GetDataRate().GetBitRate();
		nbr2if[dnode][snode].idx = DynamicCast<QbbNetDevice>(d.Get(1))->GetIfIndex();
		nbr2if[dnode][snode].up = true;
		nbr2if[dnode][snode].delay = DynamicCast<QbbChannel>(DynamicCast<QbbNetDevice>(d.Get(1))->GetChannel())->GetDelay().GetTimeStep();
		nbr2if[dnode][snode].bw = DynamicCast<QbbNetDevice>(d.Get(1))->GetDataRate().GetBitRate();

		// This is just to set up the connectivity between nodes. The IP addresses are useless
		char ipstring[16];
		sprintf(ipstring, "10.%d.%d.0", i / 254 + 1, i % 254 + 1);
		ipv4.SetBase(ipstring, "255.255.255.0");
		ipv4.Assign(d);

		// setup PFC trace
		DynamicCast<QbbNetDevice>(d.Get(0))->TraceConnectWithoutContext("QbbPfc", MakeBoundCallback (&get_pfc, pfc_file, DynamicCast<QbbNetDevice>(d.Get(0))));
		DynamicCast<QbbNetDevice>(d.Get(1))->TraceConnectWithoutContext("QbbPfc", MakeBoundCallback (&get_pfc, pfc_file, DynamicCast<QbbNetDevice>(d.Get(1))));
		
		// ======= 此处为同步到DCISwitch的代码 =======
		// 获取链路参数
		uint64_t delay_src = DynamicCast<QbbChannel>(DynamicCast<QbbNetDevice>(d.Get(0))->GetChannel())->GetDelay().GetTimeStep();
		uint64_t bw_src = DynamicCast<QbbNetDevice>(d.Get(0))->GetDataRate().GetBitRate();
		uint32_t port_src = DynamicCast<QbbNetDevice>(d.Get(0))->GetIfIndex(); // port_src 是DCI switch: snod的端口编号

		uint64_t delay_dst = DynamicCast<QbbChannel>(DynamicCast<QbbNetDevice>(d.Get(1))->GetChannel())->GetDelay().GetTimeStep();
		uint64_t bw_dst = DynamicCast<QbbNetDevice>(d.Get(1))->GetDataRate().GetBitRate();
		uint32_t port_dst = DynamicCast<QbbNetDevice>(d.Get(1))->GetIfIndex(); // port_dst 是DCI switch: dnode的端口编号

		// 如果是DCI交换机，存储下一跳链路信息
		if (snode->GetNodeType() == 2) {
			// std::cout << GetCurrentTime() << "[test]DCI id: " << snode->GetId() << std::endl;
			Ptr<DCISwitchNode> dci_sw = DynamicCast<DCISwitchNode>(snode);
			dci_sw->m_linkDelay[port_src] = delay_src;
			dci_sw->m_linkBw[port_src] = bw_src;

			// 获取端口所对应的节点编号
			for (auto& kv : nbr2if[snode]) {
				Ptr<Node> neighbor = kv.first;
				Interface& iface = kv.second;
				// std::cout << kv.first->GetId() << ', ' << kv.second.idx << std::endl;
				if (iface.idx == port_src) {
					// std::cout << GetCurrentTime() 
					// << "[TEST]DCI id-" << snode->GetId() 
					// <<  "-->" << neighbor->GetId() << " (port-"<< port_src << ")" 
					// << ", linkDelay: " << dci_sw->m_linkDelay[port_src] << " ns"
					// << ", linkBw: " << dci_sw->m_linkBw[port_src] << " bps"
					// << std::endl;
				}
			}
		}

		if (dnode->GetNodeType() == 2) {
        Ptr<DCISwitchNode> dci_sw = DynamicCast<DCISwitchNode>(dnode);
        dci_sw->m_linkDelay[port_dst] = delay_dst;
        dci_sw->m_linkBw[port_dst] = bw_dst;

		// 获取端口所对应的节点编号
		for (auto& kv : nbr2if[dnode]) {
			Ptr<Node> neighbor = kv.first;
			Interface& iface = kv.second;
			// std::cout << kv.first->GetId() << ', ' << kv.second.idx << std::endl;
			// if (iface.idx == port_dst) {
			// 	std::cout << GetCurrentTime() 
			// 	<< "[TEST] 获取端口对应的节点DCI id: " << dnode->GetId() 
			// 	<<  "-->" << neighbor->GetId() << " (port-"<< port_dst << ")" 
			// 	<< ", linkDelay: " << dci_sw->m_linkDelay[port_dst]  << " ns"
			// 	<< ", linkBw: " << dci_sw->m_linkBw[port_dst] << " bps"
			// 	<< std::endl;
			// }
		}
    }
	}


	nic_rate = get_nic_rate(n);

	std::cout << GetCurrentTime() << "[test]Config switch." << std::endl;
	
	// Step 4: config switch 配置交换机
	for (uint32_t i = 0; i < node_num; i++){
		// std::cout << "[TEST]n.Get(i)->GetNodeType(): " << n.Get(i)->GetNodeType() << std::endl;
		if (n.Get(i)->GetNodeType() == 1){ // is switch
			Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(n.Get(i));
			uint32_t shift = 3; // by default 1/8
			for (uint32_t j = 1; j < sw->GetNDevices(); j++){
				Ptr<QbbNetDevice> dev = DynamicCast<QbbNetDevice>(sw->GetDevice(j));
				// set ecn
				uint64_t rate = dev->GetDataRate().GetBitRate();
				// std::cout << "[TEST]rate: " << rate << std::endl;
				// std::cout << "[TEST]rate2kmin: " << rate2kmin[rate] << std::endl;
				// std::cout << "[TEST]rate2kmax: " << rate2kmax[rate] << std::endl;
				// std::cout << "[TEST]rate2pmax: " << rate2pmax[rate] << std::endl;
				NS_ASSERT_MSG(rate2kmin.find(rate) != rate2kmin.end(), "must set kmin for each link speed");
				NS_ASSERT_MSG(rate2kmax.find(rate) != rate2kmax.end(), "must set kmax for each link speed");
				NS_ASSERT_MSG(rate2pmax.find(rate) != rate2pmax.end(), "must set pmax for each link speed");
				sw->m_mmu->ConfigEcn(j, rate2kmin[rate], rate2kmax[rate], rate2pmax[rate]); //在SwitchMmu中为交换机的每个端口所对应的带宽设置内存大小，基于config文件的KMAX_MAP
				// set pfc
				uint64_t delay = DynamicCast<QbbChannel>(dev->GetChannel())->GetDelay().GetTimeStep();
				uint32_t headroom = rate * delay / 8 / 1000000000 * 3;
				sw->m_mmu->ConfigHdrm(j, headroom);

				// set pfc alpha, proportional to link bw
				sw->m_mmu->pfc_a_shift[j] = shift;
				while (rate > nic_rate && sw->m_mmu->pfc_a_shift[j] > 0){
					sw->m_mmu->pfc_a_shift[j]--;
					rate /= 2;
				}
			}
			sw->m_mmu->ConfigNPort(sw->GetNDevices()-1);
			sw->m_mmu->ConfigBufferSize(buffer_size* 1024 * 1024);
			// std::cout << "[TEST]Intra-DC Switch: " << sw->GetId() << " buffer_size: " << buffer_size << "MB" << std::endl;
			sw->m_mmu->node_id = sw->GetId();
		}
		else if (n.Get(i)->GetNodeType() == 2){ // is DCISwitch
			Ptr<DCISwitchNode> sw = DynamicCast<DCISwitchNode>(n.Get(i));
			uint32_t shift = 3; // by default 1/8
			for (uint32_t j = 1; j < sw->GetNDevices(); j++){
				Ptr<QbbNetDevice> dev = DynamicCast<QbbNetDevice>(sw->GetDevice(j));
				// set ecn
				uint64_t rate = dev->GetDataRate().GetBitRate();
				NS_ASSERT_MSG(rate2kmin.find(rate) != rate2kmin.end(), "must set kmin for each link speed");
				NS_ASSERT_MSG(rate2kmax.find(rate) != rate2kmax.end(), "must set kmax for each link speed");
				NS_ASSERT_MSG(rate2pmax.find(rate) != rate2pmax.end(), "must set pmax for each link speed");
				sw->m_mmu->ConfigEcn(j, rate2kmin[rate], rate2kmax[rate], rate2pmax[rate]);
				// set pfc
				uint64_t delay = DynamicCast<QbbChannel>(dev->GetChannel())->GetDelay().GetTimeStep();
				uint32_t headroom = rate * delay / 8 / 1000000000 * 3;
				sw->m_mmu->ConfigHdrm(j, headroom);

				// set pfc alpha, proportional to link bw
				sw->m_mmu->pfc_a_shift[j] = shift;
				while (rate > nic_rate && sw->m_mmu->pfc_a_shift[j] > 0){
					sw->m_mmu->pfc_a_shift[j]--;
					rate /= 2;
				}
			}
			sw->m_mmu->ConfigNPort(sw->GetNDevices()-1);
			sw->m_mmu->ConfigBufferSize(dci_buffer_size* 1024 * 1024); // 设置DCI交换机缓冲区大小 原单位MB
			// std::cout << "[TEST]DCISwitch: " << sw->GetId() << "buffer_size: " << dci_buffer_size << "MB" << std::endl;
			sw->m_mmu->node_id = sw->GetId();

			// [NEW] 启动对端口队列的周期性监控
			// Ptr<DCISwitchNode> dciSw = DynamicCast<DCISwitchNode>(n.Get(i));
        	// Simulator::Schedule(NanoSeconds(0), &DCISwitchNode::PeriodicMonitorCongestionState, dciSw);
		}
	}

	// #if ENABLE_QP
	FILE *fct_output = fopen(fct_output_file.c_str(), "w");

	// Step 5: install RDMA driver for server host 安装RDMA驱动 [服务器主机端]
	for (uint32_t i = 0; i < node_num; i++){
		if (n.Get(i)->GetNodeType() == 0){ // for server
			// create RdmaHw (hardware)
			Ptr<RdmaHw> rdmaHw = CreateObject<RdmaHw>();
			rdmaHw->SetAttribute("ClampTargetRate", BooleanValue(clamp_target_rate));
			rdmaHw->SetAttribute("AlphaResumInterval", DoubleValue(alpha_resume_interval));
			rdmaHw->SetAttribute("RPTimer", DoubleValue(rp_timer));
			rdmaHw->SetAttribute("FastRecoveryTimes", UintegerValue(fast_recovery_times));
			rdmaHw->SetAttribute("EwmaGain", DoubleValue(ewma_gain));
			rdmaHw->SetAttribute("RateAI", DataRateValue(DataRate(rate_ai)));
			rdmaHw->SetAttribute("RateHAI", DataRateValue(DataRate(rate_hai)));
			rdmaHw->SetAttribute("L2BackToZero", BooleanValue(l2_back_to_zero));
			rdmaHw->SetAttribute("L2ChunkSize", UintegerValue(l2_chunk_size));
			rdmaHw->SetAttribute("L2AckInterval", UintegerValue(l2_ack_interval));
			rdmaHw->SetAttribute("CcMode", UintegerValue(cc_mode));
			rdmaHw->SetAttribute("RateDecreaseInterval", DoubleValue(rate_decrease_interval));
			rdmaHw->SetAttribute("MinRate", DataRateValue(DataRate(min_rate)));
			rdmaHw->SetAttribute("Mtu", UintegerValue(packet_payload_size));
			rdmaHw->SetAttribute("MiThresh", UintegerValue(mi_thresh));
			rdmaHw->SetAttribute("VarWin", BooleanValue(var_win));
			rdmaHw->SetAttribute("FastReact", BooleanValue(fast_react));
			rdmaHw->SetAttribute("MultiRate", BooleanValue(multi_rate));
			rdmaHw->SetAttribute("SampleFeedback", BooleanValue(sample_feedback));
			rdmaHw->SetAttribute("TargetUtil", DoubleValue(u_target));
			rdmaHw->SetAttribute("RateBound", BooleanValue(rate_bound));
			rdmaHw->SetAttribute("DctcpRateAI", DataRateValue(DataRate(dctcp_rate_ai)));
			rdmaHw->SetPintSmplThresh(pint_prob);
			// create and install RdmaDriver
			Ptr<RdmaDriver> rdma = CreateObject<RdmaDriver>();
			Ptr<Node> node = n.Get(i);
			rdma->SetNode(node);
			rdma->SetRdmaHw(rdmaHw);

			node->AggregateObject (rdma);
			rdma->Init(); // check rdma-driver.cc, 根据网卡的数量，为每一个网卡创建一个Qbb网卡设备
			rdma->TraceConnectWithoutContext("QpComplete", MakeBoundCallback (qp_finish, fct_output));
		}
	}
	// #endif

	// set ACK priority on hosts
	if (ack_high_prio)
		RdmaEgressQueue::ack_q_idx = 0;
	else
		RdmaEgressQueue::ack_q_idx = 3;

	// Step 6: setup routing 计算每个节点的路由，以及建立路由表
	std::cout << GetCurrentTime() << "[test]Setup routing." << std::endl;
	CalculateRoutes(n);
	SetRoutingEntries();

	// get BDP and delay
	maxRtt = maxBdp = 0;
	// TEST
	uint64_t crtRtt = 0; 
	uint64_t crtBw = 0;
	uint64_t crtDelay = 0;
	uint64_t crtTxDelay = 0;

	for (uint32_t i = 0; i < node_num; i++){
		if (n.Get(i)->GetNodeType() != 0)
			continue;
		for (uint32_t j = 0; j < node_num; j++){
			if (n.Get(j)->GetNodeType() != 0)
				continue;
			uint64_t delay = pairDelay[n.Get(i)][n.Get(j)];
			uint64_t txDelay = pairTxDelay[n.Get(i)][n.Get(j)];
			uint64_t rtt = delay * 2 + txDelay;
			uint64_t bw = pairBw[i][j];
			uint64_t bdp = rtt * bw / 1000000000/8; 
			pairBdp[n.Get(i)][n.Get(j)] = bdp;
			pairRtt[i][j] = rtt;

			
			if (bdp > maxBdp) {
				maxBdp = bdp;
				crtRtt = rtt;
				crtBw = bw;
				crtDelay = delay;
				crtTxDelay = txDelay;
			}
			if (rtt > maxRtt)
				maxRtt = rtt;

			// std::cout<< "[TEST] [from node: " << n.Get(i)->GetId() << " to node: " << n.Get(j)->GetId() << "] rtt: " << rtt << " (ns)" << ", bw: " << bw << " (bps)" << std::endl;
		}
	}
	// printf("%smaxRtt=%lu (ns), maxBdp=%lu (bits)\n", GetCurrentTime().c_str(), maxRtt, maxBdp);
	// printf("%scrtBw=%lu (bps)\n", GetCurrentTime().c_str(), crtBw);
	// printf("%scrtRtt=crtDelay*2+crtTxDelay = %lu * 2 + %lu  =%lu(ns)\n", GetCurrentTime().c_str(), crtDelay, crtTxDelay, crtRtt);


	// Step 7: setup switch CC 配置交换机拥塞控制参数
	//
	for (uint32_t i = 0; i < node_num; i++){
		if (n.Get(i)->GetNodeType() == 1){ // switch
			Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(n.Get(i));
			sw->SetAttribute("CcMode", UintegerValue(cc_mode));
			sw->SetAttribute("MaxRtt", UintegerValue(maxRtt));
		} else if (n.Get(i)->GetNodeType() == 2){ // is DCISwitch
			Ptr<DCISwitchNode> dciSw = DynamicCast<DCISwitchNode>(n.Get(i));
			dciSw->SetAttribute("CcMode", UintegerValue(cc_mode));
			dciSw->SetAttribute("MaxRtt", UintegerValue(maxRtt));
			dciSw->SetAttribute("RoutingMode", UintegerValue(routing_mode));
			// 应用成本权重参数到 DCI 交换机
			dciSw->SetAttribute("W_dl", UintegerValue(w_dl));
			dciSw->SetAttribute("W_bw", UintegerValue(w_bw));
			dciSw->SetAttribute("S_static", UintegerValue(s_static));
			dciSw->SetAttribute("W_ql", UintegerValue(w_ql));
			dciSw->SetAttribute("W_tl", UintegerValue(w_tl));
			dciSw->SetAttribute("W_dp", UintegerValue(w_dp));
			dciSw->SetAttribute("S_cong", UintegerValue(s_cong));
			dciSw->SetAttribute("Alpha", UintegerValue(alpha_cost));
			dciSw->SetAttribute("Beta", UintegerValue(beta_cost));
			dciSw->SetAttribute("S_total", UintegerValue(s_total));
		}
	}

	
	std::cout << GetCurrentTime() << "[test]Setup switch CC." << std::endl;

	// 用于指定监测节点
	// add trace
	//
	FILE *trace_output = NULL;
	if (enable_trace)
	{
		NodeContainer trace_nodes;
		for (uint32_t i = 0; i < trace_num; i++)
		{
			uint32_t nid;
			tracef >> nid;
			if (nid >= n.GetN()){
				continue;
			}
			trace_nodes = NodeContainer(trace_nodes, n.Get(nid));
		}

		trace_output = fopen(trace_output_file.c_str(), "w");
		
		qbb.EnableTracing(trace_output, trace_nodes);
		// dump link speed to trace file
		
			SimSetting sim_setting;
			for (auto i: nbr2if){
				for (auto j : i.second){
					uint16_t node = i.first->GetId();
					uint8_t intf = j.second.idx;
					uint64_t bps = DynamicCast<QbbNetDevice>(i.first->GetDevice(j.second.idx))->GetDataRate().GetBitRate();
					sim_setting.port_speed[node][intf] = bps;
				}
			}
			sim_setting.win = maxBdp;
			sim_setting.Serialize(trace_output);
	}


	Ipv4GlobalRoutingHelper::PopulateRoutingTables();


	Time interPacketInterval = Seconds(0.0000005 / 2);

	// maintain port number for each host
	for (uint32_t i = 0; i < node_num; i++){
		if (n.Get(i)->GetNodeType() == 0)
			for (uint32_t j = 0; j < node_num; j++){
				if (n.Get(j)->GetNodeType() == 0)
					portNumder[i][j] = 10000; // each host pair use port number from 10000
			}
	}

	// 	[NEW]设置要追踪的流ID=================
	// 添加要追踪的流
	// 新的调用方式:
	std::string trace_flows_file = working_dir + "flowToShowRouting.txt";
	ConfigureFlowTracking(trace_flows_file, output_dir);

	NS_LOG_INFO("Create Applications.");
	std::cout << GetCurrentTime() << "[test]Create Applications." << std::endl;

	// Step 8: 创建应用 (读取流量信息、并调度)
	flow_input.idx = 0;
	if (flow_num > 0){
		if (!ReadFlowInput()){
			std::cerr << "[ERROR] First flow line cannot be read. flow_num set to 0." << std::endl;
			flow_num = 0;
		}else{
			// ScheduleFlowInputs函数负责调度流
			Simulator::Schedule(Seconds(flow_input.start_time)-Simulator::Now(), ScheduleFlowInputs);
		}
	}

	topof.close();
	tracef.close();

	// schedule link down
	if (link_down_time > 0){
		Simulator::Schedule(Seconds(2) + MicroSeconds(link_down_time), &TakeDownLink, n, n.Get(link_down_A), n.Get(link_down_B));
	}

	// schedule buffer monitor
	FILE* qlen_output = fopen(qlen_mon_file.c_str(), "w");
	Simulator::Schedule(NanoSeconds(qlen_mon_start), &monitor_buffer, qlen_output, &n);

	// [NEW] 新增链路利用率的追踪代码
	FILE *fout_uplink = nullptr;
	if (enable_link_util_record) {
	// 创建 uplink 和 conn 输出文件
	fout_uplink = fopen(link_util_output_file.c_str(), "w");
	// FILE *fout_conn = fopen((output_dir + "/conn.txt").c_str(), "w");

	
	for (int ToRId = 0; ToRId < node_num; ToRId++) {
		Ptr<Node> node = n.Get(ToRId);
		if (node->GetNodeType() == 2) {  // DCI switches
			auto swNode = DynamicCast<DCISwitchNode>(node);
			for (auto &nextNodeIf : nbr2if[node]) {
				if (nextNodeIf.first->GetNodeType() == 2) {  // nextNode is DCI switch (i.e., uplink)
					dciId2UplinkIf[ToRId].push_back(nextNodeIf.second.idx);
				}
				// else {  // downlink
				// 	dciId2DownlinkIf[ToRId].push_back(nextNodeIf.second.idx);
				// }
			}
		}
	}
	
	// 监控链路利用率
	Simulator::Schedule(Seconds(2.0), &periodic_monitoring, fout_uplink);
	// Simulator::Schedule(Seconds(2.0), &periodic_monitoring, fout_uplink, fout_conn);

}

	// Step 9: 运行仿真
	// Now, do the actual simulation.
	//
	std::cout << "------------------------------------------" << std::endl;
	std::cout << GetCurrentTime() << "Running Simulation.\n";
	fflush(stdout);
	NS_LOG_INFO("Run Simulation.");
	// Simulator::Stop(Seconds(simulator_stop_time)); // 设置仿真停止时间

	Simulator::Run();

	Simulator::Destroy();
	NS_LOG_INFO("Done.");

	// [new]清理资源
	QbbChannel::CleanupTraceFiles();

	if (enable_trace && trace_output) {
		fclose(trace_output);
	}

	endt = clock();
	std::cout << GetCurrentTime() << "Simulation time: " << (double)(endt - begint) / CLOCKS_PER_SEC << "s\n";
	// 仿真结束后关闭文件
if (enable_link_util_record) {
	fclose(fout_uplink);
	// fclose(fout_conn);
}
}

/**
 * 定期监控网络流量
 */
void periodic_monitoring(FILE *fout_uplink) {
// void periodic_monitoring(FILE *fout_uplink, FILE *fout_conn) {
    uint64_t now = Simulator::Now().GetNanoSeconds();

    // 监控TOR的uplink负载
    for (const auto &tor2If : dciId2UplinkIf) {
        Ptr<Node> node = n.Get(tor2If.first);    // tor id
        auto DciSwNode = DynamicCast<DCISwitchNode>(node);
        for (const auto &iface : tor2If.second) {
            uint64_t uplink_txbyte = DciSwNode->GetTxBytesOutDev(iface);
            uint32_t dst_id = -1; // Default to an invalid ID
			for (auto const& pair : nbr2if.at(node))
			{
				if (pair.second.idx == iface)
				{
					dst_id = pair.first->GetId();
					break;
				}
			}
            fprintf(fout_uplink, "%lu,%u,%u,%lu\n", now, tor2If.first, dst_id, uplink_txbyte);
        }
    }

    // 监控每个服务器的连接数
    // for (uint32_t i = 0; i < node_num; i++) {
    //     if (n.Get(i)->GetNodeType() == 0) {  // is server
    //         Ptr<Node> server = n.Get(i);
    //         Ptr<RdmaDriver> rdmaDriver = server->GetObject<RdmaDriver>();
    //         Ptr<RdmaHw> rdmaHw = rdmaDriver->m_rdma;
    //         uint64_t nQP = rdmaHw->m_qpMap.size();
    //         uint64_t nActiveQP = 0;
    //         for (auto qp : rdmaHw->m_qpMap) {
    //             if (qp.second->GetBytesLeft() > 0) {
    //                 nActiveQP++;
    //             }
    //         }
    //         fprintf(fout_conn, "%lu,%u,%lu,%lu\n", now, i, nQP, nActiveQP);
    //     }
    // }

	uint32_t switch_mon_interval = 1000000;  // ns = 1ms, 10000ns=10us
    // 递归调度
    if (!Simulator::IsFinished()) {
        Simulator::Schedule(NanoSeconds(switch_mon_interval), &periodic_monitoring, fout_uplink);
        // Simulator::Schedule(NanoSeconds(switch_mon_interval), &periodic_monitoring, fout_uplink, fout_conn);
    }
    return;
}

/**
 * 跳过输入流中所有的注释行和空行
 * 注释行以#开头，空行只包含空白字符
 * 
 * @param is 输入流
 * @return 输入流的引用（允许链式调用）
 */
std::istream& SkipComments(std::istream& is) {
    std::string line;
    bool isShow = false;
    std::streampos current_pos;

    while (true) {
        current_pos = is.tellg(); // 记录当前位置
        if (!std::getline(is, line)) {
            // 读取失败（包括EOF），直接退出
            break;
        }
				
        size_t start = line.find_first_not_of(" \t\r\n"); // 去除前后空白
        if (start == std::string::npos) {
            // 空行，继续
            continue;
        }
        if (line[start] == '#') {
            // 注释行，继续
            if (isShow) {
                std::cout << "[test]Skipped comment line: " << line << std::endl;
            }
            continue;
        }
        // 非注释行，回退到行首
        is.clear(); // 清除eof标志
        is.seekg(current_pos);
        break;
    }
    return is;
}


/**
 * 获取当前时间并格式化为hh:mm:ss
 * 
 * @return 格式化后的时间字符串
 */
std::string GetCurrentTime() {
    time_t now = time(0);
    tm *ltm = localtime(&now);
    char time_str[9];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", ltm);
    return std::string(time_str) + ' ';
}


void ConfigureFlowTracking(const std::string& trace_flows_file, const std::string& output_dir) {
    // 打开文件
    std::ifstream tracef(trace_flows_file.c_str());
    if (!tracef.is_open()) {
        std::cerr << GetCurrentTime() << "Error: Unable to open trace flows file " << trace_flows_file << std::endl;
        return;
    }

    // 读取要追踪的流数量
    uint32_t flow_num;
    SkipComments(tracef) >> flow_num;

    // 读取每条流的信息并设置追踪
    for (uint32_t i = 0; i < flow_num; i++) {
        uint32_t src, dst, pg, dport;
        uint64_t size;
        double start_time;
        
        // 读取流信息
        SkipComments(tracef) >> src >> dst >> pg >> dport >> size >> start_time;
        
				std::cout << GetCurrentTime() 
									<< "[Debug] Added flow " << i << " tracking: " << ": src=" << src 
									<< ", dst=" << dst 
									<< ", pg=" << pg 
									<< ", dport=" << dport 
									<< ", size=" << size 
									<< ", start_time=" << start_time 
									<< std::endl;
        // 使用src、dst、size生成流ID
        uint32_t flowIDToTrace = RdmaQueuePair::GenerateTraceFlowId(
            node_id_to_ip(src).Get(),
            node_id_to_ip(dst).Get(),
            size
        );
        
        // 添加到追踪列表
        QbbChannel::AddTraceFlow(flowIDToTrace, output_dir);
        
    }

    tracef.close();
}

// 辅助函数来检查目录是否存在
bool DirectoryExists(const std::string& path) {
    struct stat info;
#ifdef _WIN32
    return (stat(path.c_str(), &info) == 0) && (info.st_mode & _S_IFDIR);
#else
    return (stat(path.c_str(), &info) == 0) && (info.st_mode & S_IFDIR);
#endif
}

// 对输出文件进行统一命名（包括对应文件夹，和后缀cc）
std::string replace_config_variables(const std::string& input) {
	std::string result = input;
	// 替换${WORKING_DIR} 或 ${OUTPUT_DIR}
	size_t pos = result.find("${WORKING_DIR}");
	size_t output_pos = result.find("${OUTPUT_DIR}");
	if (pos != std::string::npos || output_pos != std::string::npos) {
		std::string dir_value;
		size_t replace_pos;
		size_t replace_len;
		if (pos != std::string::npos) {
			dir_value = working_dir;
			replace_pos = pos;
			replace_len = 14;
		} else {
			dir_value = output_dir;
			replace_pos = output_pos;
			replace_len = 13;
		}
		result.replace(replace_pos, replace_len, dir_value);

		// 获取文件所在目录路径
		std::string dir = result.substr(0, result.find_last_of("/\\"));

		// 检查并创建目录
		if (!dir.empty()) {
			std::string current_path;
			std::istringstream path_stream(dir);
			std::string path_component;

			while (std::getline(path_stream, path_component, '/')) {
				if (!path_component.empty()) {
					if (current_path.empty()) {
						current_path = path_component;
					} else {
						current_path += "/" + path_component;
					}
					if (!DirectoryExists(current_path)) {
						std::string command = "mkdir -p " + current_path;
						int ret = system(command.c_str());
						if (ret != 0) {
							std::cerr << GetCurrentTime() << "警告：无法创建目录 " << current_path << std::endl;
						} else {
							std::cout << GetCurrentTime() << "[Debug] Created directory: " << current_path << std::endl;
						}
					}
				}
			}
		}
	} else {
		std::cerr << GetCurrentTime() << "错误：配置文件路径变量未设置（必须包含${WORKING_DIR}或${OUTPUT_DIR}），当前配置为：" << input << std::endl;
	}

	// 替换${CC_NAME}
	pos = result.find("${CC_NAME}");
	while (pos != std::string::npos) {
		std::string cc_name;
		if (cc_mode == 1)
			cc_name = "dcqcn";
		else if (cc_mode == 3)
			cc_name = "hpcc";
		else if (cc_mode == 7)
			cc_name = "timely";
		else if (cc_mode == 8)
			cc_name = "dctcp";
		else if (cc_mode == 10)
			cc_name = "hpcc-pint";
		else
			cc_name = std::to_string(cc_mode);

		result.replace(pos, 10, cc_name);
		pos = result.find("${CC_NAME}", pos + cc_name.length());
	}

	return result;
}

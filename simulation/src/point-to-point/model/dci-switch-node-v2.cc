#include "ns3/ipv4.h"
#include "ns3/packet.h"
#include "ns3/ipv4-header.h"
#include "ns3/pause-header.h"
#include "ns3/flow-id-tag.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "dci-switch-node.h"
#include "qbb-net-device.h"
#include "ppp-header.h"
#include "ns3/int-header.h"
#include <cmath>

#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
namespace ns3 {

TypeId DCISwitchNode::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::DCISwitchNode")
    .SetParent<Node> ()
    .AddConstructor<DCISwitchNode> ()
	.AddAttribute("EcnEnabled",
			"Enable ECN marking.",
			BooleanValue(false),
			MakeBooleanAccessor(&DCISwitchNode::m_ecnEnabled),
			MakeBooleanChecker())
	.AddAttribute("CcMode",
			"CC mode.",
			UintegerValue(0),
			MakeUintegerAccessor(&DCISwitchNode::m_ccMode),
			MakeUintegerChecker<uint32_t>())
	.AddAttribute("AckHighPrio",
			"Set high priority for ACK/NACK or not",
			UintegerValue(0),
			MakeUintegerAccessor(&DCISwitchNode::m_ackHighPrio),
			MakeUintegerChecker<uint32_t>())
	.AddAttribute("MaxRtt",
			"Max Rtt of the network",
			UintegerValue(9000),
			MakeUintegerAccessor(&DCISwitchNode::m_maxRtt),
			MakeUintegerChecker<uint32_t>())
	.AddAttribute("RoutingMode",
			"Mode of routing method",
			UintegerValue(0),
			MakeUintegerAccessor(&DCISwitchNode::m_routingMode),
			MakeUintegerChecker<uint32_t>())
	.AddAttribute("Mtu",
		"Mtu.",
		UintegerValue(1000),
		MakeUintegerAccessor(&DCISwitchNode::m_mtu),
		MakeUintegerChecker<uint32_t>())
  ;
  return tid;
}

DCISwitchNode::DCISwitchNode(){
	m_ecmpSeed = m_id;
	m_node_type = 2; // 2 for DCI Switch
	m_mmu = CreateObject<SwitchMmu>(); // 创建交换机MMU
	for (uint32_t i = 0; i < pCnt; i++)
		for (uint32_t j = 0; j < pCnt; j++)
			for (uint32_t k = 0; k < qCnt; k++)
				m_bytes[i][j][k] = 0;
	for (uint32_t i = 0; i < pCnt; i++)
		m_txBytes[i] = 0;
	for (uint32_t i = 0; i < pCnt; i++)
		m_lastPktSize[i] = m_lastPktTs[i] = 0;
	for (uint32_t i = 0; i < pCnt; i++)
		m_u[i] = 0;

	// [NEW] 带宽分段阈值与分数初始化（示例 N=10, MAX_BW=800Gbps）
    for (int i = 0; i < kClassNum; ++i) {
        m_bw_thresh[i] = (MAX_BW * (i + 1)) / kClassNum; // ceil可用整数运算近似
    }

	// [NEW] 初始化缓冲区容量和队列阈值
	qThresh.resize(kClassNum);
	for (int i = 0; i < kClassNum; i++) {
		qThresh[i] = m_bufferCapacity * i / kClassNum;
		// std::cout << "[DCI " << this->GetId() << "] QLevel " << i
		// << " threshold set to " << qThresh[i] << " bytes" << std::endl;
	}

	// [NEW] 初始化Level分数表（线性映射0~255，10级）
    levelScore.resize(kClassNum);
    for (int i = 0; i < kClassNum; i++)
        levelScore[i] = static_cast<uint8_t>(255 * (i + 1) / kClassNum);

    // [NEW] 初始化TrendLevel标准化阈值表（单位bytes/ms）
    for (auto rate_gbps : bwSharedList) {
        std::vector<uint32_t> thresh(kClassNum);
		uint64_t rate_bps = rate_gbps * 1000000000ULL;
        // 这里每一级阈值为该速率下的理论最大bytes/ms的百分比
        uint64_t bytes_per_ms = rate_bps / 8 / 1000;
        for (int i = 0; i < 10; i++)
            thresh[i] = bytes_per_ms * (i + 1) / 10;
        trendThreshNorm[rate_gbps] = thresh;
    }
}

int DCISwitchNode::GetOutDev(Ptr<const Packet> p, CustomHeader &ch){
	// look up entries
	auto entry = m_rtTable.find(ch.dip);
	// no matching entry
	if (entry == m_rtTable.end())
		return -1;
	
	// entry found
	auto &nexthops = entry->second;
	
	// 构造流ID（可用五元组哈希或自定义方式）
	// uint64_t flowId = RdmaHw::GetQpKey(ch.dip, ch.tcp.sport, ch.udp.pg);
	uint32_t sport = 0, dport = 0;
	if (ch.l3Prot == 0x6) { // TCP
		sport = ch.tcp.sport;
		dport = ch.tcp.dport;
	} else if (ch.l3Prot == 0x11) { // UDP
		sport = ch.udp.sport;
		dport = ch.udp.dport;
	} else if (ch.l3Prot == 0xFC || ch.l3Prot == 0xFD) { // ACK/NACK
		sport = ch.ack.sport;
		dport = ch.ack.dport;
	}
	uint64_t flowId = RdmaQueuePair::GenerateFlowId(ch.sip, ch.dip, sport, dport);

	// === 启动开销函数的构建 ===
	if (m_routingMode == 2) { // 0: ECMP, 1: UCMP, 2: Ours
		CleanIdleFlows(); // 清理超时流

		auto it = flow2outdev.find(flowId);
		// 仅在流的第一个包时调用，进行成本计算
		if (it == flow2outdev.end()) {
			uint32_t best_intf = nexthops[0];
			uint32_t min_cost = UINT32_MAX;

			// 新流：基于负载的智能ECMP
            std::vector<int> available_paths;
			// 存储路径成本和端口索引的容器，每个元素是一个二元组 (total_cost, intf_idx)。
			std::vector<std::pair<uint32_t, int>> cost_path_pairs;
			// --------------------------------

			// === 新增调试部分 ===
			const std::string dirPath = "/home/dyyu/Desktop/workspace/High-Precision-Congestion-Control/simulation/mix/config/routingChoice";
if (saveRoutingChoice) {
			struct stat info;
			if (stat(dirPath.c_str(), &info) != 0) {
				// 文件夹不存在，创建
				mkdir(dirPath.c_str(), 0755);
			} else if (!(info.st_mode & S_IFDIR)) {
				// 路径存在但不是文件夹
				std::cerr << "[ERROR] " << dirPath << " exists but is not a directory!" << std::endl;
			}
}
		std::ostringstream buffer; // 调试使用
		std::ostringstream oss;
		oss << dirPath << "/DCI-" << this->GetId() << ".txt";
		std::string filePath = oss.str();
		// === 新增调试部分 ===

		// 第一步：计算所有路径的拥塞成本
        // static权重(w_dl=3, w_bw=1, S_static=2)，
        // IC权重(w_ql=3, w_dp=1, S_IC=2)，
        // 最终权重(alpha=2, beta=4, gamma=2, S_final=3)
        const int w_dl = 3, w_bw = 1, S_static = 2;         // static部分权重与归一化 v2


        const int w_ql = 3, w_dp = 1, S_IC = 2;             // IC部分权重与归一化

		
        const int alpha = 4, beta = 2, gamma = 2, S_final = 3; // 总权重与归一化
        // const int alpha = 0, beta = 2, gamma = 2, S_final = 3; // 消融实验: 移除alpha (rm-alpha)
        // const int alpha = 4, beta = 0, gamma = 2, S_final = 3; // 消融实验: 移除beta (rm-beta)
        // const int alpha = 4, beta = 2, gamma = 0, S_final = 3; // 消融实验: 移除gamma (rm-gamma)

		for (auto intf_idx : nexthops) {
			// 1.1 计算静态成本
			uint16_t delay_ms = static_cast<uint16_t>(m_linkDelay[intf_idx] / 1e6);
            uint8_t delay_score = CalcDelayCost(delay_ms); // 计算时延成本
            uint8_t bw_score = CalcBwCost(m_linkBw[intf_idx]); // 计算链路容量成本
            int staticScore = w_dl * delay_score + w_bw * bw_score;
            uint8_t C_static = clamp_uint8(staticScore >> S_static, 0, 255);

			// 1.2 计算即时队列成本
    		MonitorCongestionState(); // [重要]先更新当前队列拥塞状态
            uint8_t Qlevel = CalcQLevel(intf_idx);

			UpdateDurationPenalty(intf_idx, Qlevel); // 更新拥塞持续性计数器
            uint8_t DurationPenalty = CalcDurationPenalty(intf_idx);
            uint32_t instScore = w_ql * Qlevel + w_dp * DurationPenalty;
            uint8_t C_IC = clamp_uint8(instScore >> S_IC, 0, 255);

			// 1.3 计算队列趋势成本
            uint8_t C_TC = CalcTrendLevel(intf_idx, m_linkBw[intf_idx]);

			// 1.4 三个元素加权求和
            int total_cost_raw = alpha * C_static + beta * C_IC + gamma * C_TC;
            uint32_t total_cost = clamp_uint8(total_cost_raw >> S_final, 0, 255);

			cost_path_pairs.emplace_back(total_cost, intf_idx);

			// === 新增调试部分 ===
if (saveRoutingChoice) {
			buffer << "[flow-" << flowId << "][DCI " << this->GetId()
				<< "] port " << intf_idx
				<< ": staticCost " << (int)C_static
				<< ", InstCongCost " << (int)C_IC
				<< ", TrendCongCost " << (int)C_TC
				<< ", TotalCost " << total_cost
				<< std::endl;
}
			// === 新增调试部分 ===
		}
		// 排序并选取前一半
		std::sort(cost_path_pairs.begin(), cost_path_pairs.end()); // 所有路径会按照“总成本”从小到大排序，
		// size_t half = (cost_path_pairs.size() * 2 ) / 3;
		size_t half = cost_path_pairs.size() / 2;
		if (half < 1) half = 1; // 至少保留1个端口
		else if( half < 2) half = 2;
		for (size_t i = 0; i < half; i++) {
			available_paths.push_back(cost_path_pairs[i].second);
		}

		// 第二步：路径选择策略
		uint32_t selected_intf;
		if (!available_paths.empty()) {
			// 在低拥塞路径中ECMP
			union {
				uint8_t u8[4+4+2+2];
				uint32_t u32[3];
			} buf;
			buf.u32[0] = ch.sip;
			buf.u32[1] = ch.dip;
			if (ch.l3Prot == 0x6)
				buf.u32[2] = ch.tcp.sport | ((uint32_t)ch.tcp.dport << 16);
			else if (ch.l3Prot == 0x11)
				buf.u32[2] = ch.udp.sport | ((uint32_t)ch.udp.dport << 16);
			else if (ch.l3Prot == 0xFC || ch.l3Prot == 0xFD)
				buf.u32[2] = ch.ack.sport | ((uint32_t)ch.ack.dport << 16);
			
			uint32_t idx = EcmpHash(buf.u8, 12, m_ecmpSeed) % available_paths.size();
			selected_intf = available_paths[idx];

if (saveRoutingChoice) {
			buffer << "[SMART-ECMP] [DCI " << GetId() << "] Flow " << flowId 
						<< " selected from " << available_paths.size() 
						<< " low-congestion paths, chose port " << selected_intf << std::endl;
}

		}
		else {
			// 所有路径都高拥塞，选择拥塞最轻的
			auto min_it = std::min_element(
				cost_path_pairs.begin(), cost_path_pairs.end(),
				[](const std::pair<uint32_t, int>& a, const std::pair<uint32_t, int>& b) {
					return a.first < b.first;
				}
			);
			selected_intf = min_it->second;

if (saveRoutingChoice) {
			buffer << "[SMART-ECMP] [DCI " << GetId() << "] Flow " << flowId 
						<< " all paths congested, chose least congested port " << selected_intf 
						<< " with cost " << (int) min_it->first << std::endl;
}

            }

			// 记录流与输出端口的映射关系
			flow2outdev[flowId].outDevIdx = selected_intf; // 或你的端口选择逻辑
			flow2outdev[flowId].lastSeen = Simulator::Now();

			// === 新增调试部分 ===
			// 批量写入
if (saveRoutingChoice) {
			std::ofstream fout(filePath, std::ios::app);
			if (!fout.is_open()) {
				std::cerr << "[ERROR] Cannot open file: " << filePath << std::endl;
			} else {
				fout << buffer.str();
				fout.close();
			}
}
			// === 新增调试部分 ===

			return selected_intf;
		} else { // 后续包, 更新lastSeen
			// 刷新 last_seen 时间戳，并返回之前选择的端口
			it->second.lastSeen =  Simulator::Now();

            return it->second.outDevIdx;
		}
	}

	else if ( m_routingMode == 1) { // 0: ECMP, 1: UCMP, 2: Ours
		auto it = flow2outdev.find(flowId);
		// 如果是流的第一个包，则进行选路计算
		if (it == flow2outdev.end()) {
			std::vector<std::pair<uint64_t, int>> bw_path_pairs;
			uint64_t max_bw = 0;

			// 1. 计算所有路径的带宽，并找到最大带宽
			for (auto intf_idx : nexthops) {
				uint64_t bw = m_linkBw[intf_idx];
				bw_path_pairs.emplace_back(bw, intf_idx);
				if (bw > max_bw) {
					max_bw = bw;
				}
			}

			// 2. 收集所有带宽最大的路径
			std::vector<int> best_paths;
			for (const auto& pair : bw_path_pairs) {
				if (pair.first == max_bw) {
					best_paths.push_back(pair.second);
				}
			}

			// 3. 从最优路径（best_paths）中通过ECMP哈希选择一条
			uint32_t selected_intf;
			if (!best_paths.empty()) {
				// 构造五元组用于哈希
				union {
					uint8_t u8[4+4+2+2];
					uint32_t u32[3];
				} buf;
				buf.u32[0] = ch.sip;
				buf.u32[1] = ch.dip;
				if (ch.l3Prot == 0x6)
					buf.u32[2] = ch.tcp.sport | ((uint32_t)ch.tcp.dport << 16);
				else if (ch.l3Prot == 0x11)
					buf.u32[2] = ch.udp.sport | ((uint32_t)ch.udp.dport << 16);
				else if (ch.l3Prot == 0xFC || ch.l3Prot == 0xFD)
					buf.u32[2] = ch.ack.sport | ((uint32_t)ch.ack.dport << 16);
				
				// 哈希计算并选择路径
				uint32_t idx = EcmpHash(buf.u8, 12, m_ecmpSeed) % best_paths.size();
				selected_intf = best_paths[idx];
			} else {
				// 理论上不应发生，但作为安全保障，选择第一个下一跳
				selected_intf = nexthops[0];
			}

			// 为此流记录路由决策
			flow2outdev[flowId] = {selected_intf, Simulator::Now()};

			return selected_intf;
		} else { // 对于流的后续包
			// 返回之前选择的端口
			return it->second.outDevIdx;
		}

	}

	// 非DCI路由，默认ECMP
	// pick one next hop based on hash
	union {
		uint8_t u8[4+4+2+2];
		uint32_t u32[3];
	} buf;
	buf.u32[0] = ch.sip;
	buf.u32[1] = ch.dip;
	if (ch.l3Prot == 0x6)
		buf.u32[2] = ch.tcp.sport | ((uint32_t)ch.tcp.dport << 16);
	else if (ch.l3Prot == 0x11)
		buf.u32[2] = ch.udp.sport | ((uint32_t)ch.udp.dport << 16);
	else if (ch.l3Prot == 0xFC || ch.l3Prot == 0xFD)
		buf.u32[2] = ch.ack.sport | ((uint32_t)ch.ack.dport << 16);

	uint32_t idx = EcmpHash(buf.u8, 12, m_ecmpSeed) % nexthops.size();

	return nexthops[idx];
}

void DCISwitchNode::CheckAndSendPfc(uint32_t inDev, uint32_t qIndex){
	Ptr<QbbNetDevice> device = DynamicCast<QbbNetDevice>(m_devices[inDev]);
	if (m_mmu->CheckShouldPause(inDev, qIndex)){
		device->SendPfc(qIndex, 0);
		m_mmu->SetPause(inDev, qIndex);
	}
}
void DCISwitchNode::CheckAndSendResume(uint32_t inDev, uint32_t qIndex){
	Ptr<QbbNetDevice> device = DynamicCast<QbbNetDevice>(m_devices[inDev]);
	if (m_mmu->CheckShouldResume(inDev, qIndex)){
		device->SendPfc(qIndex, 1);
		m_mmu->SetResume(inDev, qIndex);
	}
}

void DCISwitchNode::SendToDev(Ptr<Packet>p, CustomHeader &ch){
	int idx = GetOutDev(p, ch);
	if (idx >= 0){
		NS_ASSERT_MSG(m_devices[idx]->IsLinkUp(), "The routing table look up should return link that is up");

		// determine the qIndex
		uint32_t qIndex;
		if (ch.l3Prot == 0xFF || ch.l3Prot == 0xFE || (m_ackHighPrio && (ch.l3Prot == 0xFD || ch.l3Prot == 0xFC))){  //QCN or PFC or NACK, go highest priority
			qIndex = 0;
		}else{
			qIndex = (ch.l3Prot == 0x06 ? 1 : ch.udp.pg); // if TCP, put to queue 1
		}

		// admission control
		FlowIdTag t;
		p->PeekPacketTag(t);
		uint32_t inDev = t.GetFlowId();
		if (qIndex != 0){ //not highest priority
			if (m_mmu->CheckIngressAdmission(inDev, qIndex, p->GetSize()) && m_mmu->CheckEgressAdmission(idx, qIndex, p->GetSize())){			// Admission control
				m_mmu->UpdateIngressAdmission(inDev, qIndex, p->GetSize());
				m_mmu->UpdateEgressAdmission(idx, qIndex, p->GetSize());
			}else{
				return; // Drop
			}
			CheckAndSendPfc(inDev, qIndex);
		}
		m_bytes[inDev][idx][qIndex] += p->GetSize();
		m_devices[idx]->SwitchSend(qIndex, p, ch);

		if(m_routingMode == 2) { // 0: ECMP, 1: UCMP, 2: Ours
			// --- 预期队列动态增量分配 ---
			uint32_t sport = 0, dport = 0;
			if (ch.l3Prot == 0x6) { // TCP
				sport = ch.tcp.sport;
				dport = ch.tcp.dport;
			} else if (ch.l3Prot == 0x11) { // UDP
				sport = ch.udp.sport;
				dport = ch.udp.dport;
			} else if (ch.l3Prot == 0xFC || ch.l3Prot == 0xFD) { // ACK/NACK
				sport = ch.ack.sport;
				dport = ch.ack.dport;
			}
			// 获取流ID
			// uint64_t flowId = RdmaHw::GetQpKey(ch.dip, ch.tcp.sport, ch.udp.pg);
			uint64_t flowId = RdmaQueuePair::GenerateFlowId(ch.sip, ch.dip, sport, dport);

		}

	}else
		return; // Drop
}

// 基于流大小调整增量
uint64_t DCISwitchNode::calcIncrementBytes(uint64_t actualBytes) {
    // 分段函数：流越大，增量越大
    if (actualBytes < 10 * m_mtu)
        return 5 * m_mtu;  // 小流：小增量
    else if (actualBytes < 100 * m_mtu)
        return 10 * m_mtu; // 中等流：中等增量
    else if (actualBytes < 1000 * m_mtu)
        return 20 * m_mtu; // 大流：大增量
    else
        return 30 * m_mtu; // 超大流：最大增量
}

uint32_t DCISwitchNode::EcmpHash(const uint8_t* key, size_t len, uint32_t seed) {
  uint32_t h = seed;
  if (len > 3) {
    const uint32_t* key_x4 = (const uint32_t*) key;
    size_t i = len >> 2;
    do {
      uint32_t k = *key_x4++;
      k *= 0xcc9e2d51;
      k = (k << 15) | (k >> 17);
      k *= 0x1b873593;
      h ^= k;
      h = (h << 13) | (h >> 19);
      h += (h << 2) + 0xe6546b64;
    } while (--i);
    key = (const uint8_t*) key_x4;
  }
  if (len & 3) {
    size_t i = len & 3;
    uint32_t k = 0;
    key = &key[i - 1];
    do {
      k <<= 8;
      k |= *key--;
    } while (--i);
    k *= 0xcc9e2d51;
    k = (k << 15) | (k >> 17);
    k *= 0x1b873593;
    h ^= k;
  }
  h ^= len;
  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  h ^= h >> 16;
  return h;
}

void DCISwitchNode::SetEcmpSeed(uint32_t seed){
	m_ecmpSeed = seed;
}

void DCISwitchNode::AddTableEntry(Ipv4Address &dstAddr, uint32_t intf_idx){
	uint32_t dip = dstAddr.Get();
	m_rtTable[dip].push_back(intf_idx);
}

void DCISwitchNode::ClearTable(){
	m_rtTable.clear();
}

// This function can only be called in switch mode
bool DCISwitchNode::SwitchReceiveFromDevice(Ptr<NetDevice> device, Ptr<Packet> packet, CustomHeader &ch){
	SendToDev(packet, ch);
	return true;
}

void DCISwitchNode::SwitchNotifyDequeue(uint32_t ifIndex, uint32_t qIndex, Ptr<Packet> p){
	FlowIdTag t;
	p->PeekPacketTag(t);
	if (qIndex != 0){
		uint32_t inDev = t.GetFlowId();
		m_mmu->RemoveFromIngressAdmission(inDev, qIndex, p->GetSize());
		m_mmu->RemoveFromEgressAdmission(ifIndex, qIndex, p->GetSize());
		m_bytes[inDev][ifIndex][qIndex] -= p->GetSize();
		if (m_ecnEnabled){
			bool egressCongested = m_mmu->ShouldSendCN(ifIndex, qIndex);
			if (egressCongested){
				PppHeader ppp;
				Ipv4Header h;
				p->RemoveHeader(ppp);
				p->RemoveHeader(h);
				h.SetEcn((Ipv4Header::EcnType)0x03);
				p->AddHeader(h);
				p->AddHeader(ppp);
			}
		}
		//CheckAndSendPfc(inDev, qIndex);
		CheckAndSendResume(inDev, qIndex);
	}
	if (1){
		uint8_t* buf = p->GetBuffer();
		if (buf[PppHeader::GetStaticSize() + 9] == 0x11){ // udp packet
			IntHeader *ih = (IntHeader*)&buf[PppHeader::GetStaticSize() + 20 + 8 + 6]; // ppp, ip, udp, SeqTs, INT
			Ptr<QbbNetDevice> dev = DynamicCast<QbbNetDevice>(m_devices[ifIndex]);
			if (m_ccMode == 3){ // HPCC
				ih->PushHop(Simulator::Now().GetTimeStep(), m_txBytes[ifIndex], dev->GetQueue()->GetNBytesTotal(), dev->GetDataRate().GetBitRate());
				// printf("[TEST]dci-switch-node.cc: current queueBytes: %u\n", dev->GetQueue()->GetNBytesTotal());

			}else if (m_ccMode == 10){ // HPCC-PINT
				uint64_t t = Simulator::Now().GetTimeStep();
				uint64_t dt = t - m_lastPktTs[ifIndex];
				if (dt > m_maxRtt)
					dt = m_maxRtt;
				uint64_t B = dev->GetDataRate().GetBitRate() / 8; //Bps
				uint64_t qlen = dev->GetQueue()->GetNBytesTotal();
				double newU;

				/**************************
				 * approximate calc
				 *************************/
				int b = 20, m = 16, l = 20; // see log2apprx's paremeters
				int sft = logres_shift(b,l);
				double fct = 1<<sft; // (multiplication factor corresponding to sft)
				double log_T = log2(m_maxRtt)*fct; // log2(T)*fct
				double log_B = log2(B)*fct; // log2(B)*fct
				double log_1e9 = log2(1e9)*fct; // log2(1e9)*fct
				double qterm = 0;
				double byteTerm = 0;
				double uTerm = 0;
				if ((qlen >> 8) > 0){
					int log_dt = log2apprx(dt, b, m, l); // ~log2(dt)*fct
					int log_qlen = log2apprx(qlen >> 8, b, m, l); // ~log2(qlen / 256)*fct
					qterm = pow(2, (
								log_dt + log_qlen + log_1e9 - log_B - 2*log_T
								)/fct
							) * 256;
					// 2^((log2(dt)*fct+log2(qlen/256)*fct+log2(1e9)*fct-log2(B)*fct-2*log2(T)*fct)/fct)*256 ~= dt*qlen*1e9/(B*T^2)
				}
				if (m_lastPktSize[ifIndex] > 0){
					int byte = m_lastPktSize[ifIndex];
					int log_byte = log2apprx(byte, b, m, l);
					byteTerm = pow(2, (
								log_byte + log_1e9 - log_B - log_T
								)/fct
							);
					// 2^((log2(byte)*fct+log2(1e9)*fct-log2(B)*fct-log2(T)*fct)/fct) ~= byte*1e9 / (B*T)
				}
				if (m_maxRtt > dt && m_u[ifIndex] > 0){
					int log_T_dt = log2apprx(m_maxRtt - dt, b, m, l); // ~log2(T-dt)*fct
					int log_u = log2apprx(int(round(m_u[ifIndex] * 8192)), b, m, l); // ~log2(u*512)*fct
					uTerm = pow(2, (
								log_T_dt + log_u - log_T
								)/fct
							) / 8192;
					// 2^((log2(T-dt)*fct+log2(u*512)*fct-log2(T)*fct)/fct)/512 = (T-dt)*u/T
				}
				newU = qterm+byteTerm+uTerm;

				#if 0
				/**************************
				 * accurate calc
				 *************************/
				double weight_ewma = double(dt) / m_maxRtt;
				double u;
				if (m_lastPktSize[ifIndex] == 0)
					u = 0;
				else{
					double txRate = m_lastPktSize[ifIndex] / double(dt); // B/ns
					u = (qlen / m_maxRtt + txRate) * 1e9 / B;
				}
				newU = m_u[ifIndex] * (1 - weight_ewma) + u * weight_ewma;
				printf(" %lf\n", newU);
				#endif

				/************************
				 * update PINT header
				 ***********************/
				uint16_t power = Pint::encode_u(newU);
				if (power > ih->GetPower())
					ih->SetPower(power);

				m_u[ifIndex] = newU;
			}
		}
	}
	m_txBytes[ifIndex] += p->GetSize();
	m_lastPktSize[ifIndex] = p->GetSize();
	m_lastPktTs[ifIndex] = Simulator::Now().GetTimeStep();
}

int DCISwitchNode::logres_shift(int b, int l){
	static int data[] = {0,0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5};
	return l - data[b];
}

int DCISwitchNode::log2apprx(int x, int b, int m, int l){
	int x0 = x;
	int msb = int(log2(x)) + 1;
	if (msb > m){
		x = (x >> (msb - m) << (msb - m));
		#if 0
		x += + (1 << (msb - m - 1));
		#else
		int mask = (1 << (msb-m)) - 1;
		if ((x0 & mask) > (rand() & mask))
			x += 1<<(msb-m);
		#endif
	}
	return int(log2(x) * (1<<logres_shift(b, l)));
}

// Calculate delay cost based on one-way delay
uint8_t DCISwitchNode::CalcDelayCost(uint16_t one_way_delay_ms)
{
    // 最大参考时延=512ms，权重系数=255，右移9位（÷512）
    if (one_way_delay_ms > 512)
        return 255;
    else
        return (one_way_delay_ms * 255) >> 9;
}

// 计算带宽成本，输入为带宽（单位：Gbps）
uint8_t DCISwitchNode::CalcBwCost(uint64_t bw_bps)
{
    // 输入带宽为bps，转换为Gbps
    uint64_t bw_gbps = bw_bps / 1000000000ULL;
    for (int i = kClassNum - 1; i >= 0; i--) {
        if (bw_gbps >= m_bw_thresh[i])
            return 255 - levelScore[i]; // 带宽越高，成本越低
    }
    return 255; // 低于最低阈值
}

// 计算队列级别，输入为端口号
uint8_t DCISwitchNode::CalcQLevel(uint32_t port) {
    uint32_t bytes = m_congState[port].queueBytes_cur;
	// std::cout << "[TEST] CalcQLevel: [DCI " << this->GetId() << "] port " << port << " queueBytes_cur=" << bytes << std::endl;
	
    const std::vector<uint32_t>& thresh = qThresh;
	if (bytes <= 0)
        return 0;
    for (int i = kClassNum - 1; i >= 0; i--) {
		if (bytes >= thresh[i])
			return levelScore[i];
    }
	return 0;
}

// 更新队列增长趋势（每次采样时调用）
void DCISwitchNode::UpdateTrend(uint32_t port)
{
    // 默认K=3，平滑系数α=1/8
    int32_t delta = m_congState[port].queueBytes_cur - m_congState[port].queueBytes_prev;
    m_congState[port].trend = m_congState[port].trend - (m_congState[port].trend >> m_K) + (delta >> m_K);
    m_congState[port].queueBytes_prev = m_congState[port].queueBytes_cur; // 将 queueBytes_prev 更新为当前采样值 

	
}

// 计算TrendLevel分数（队列增长趋势）
uint8_t DCISwitchNode::CalcTrendLevel(uint32_t port, uint64_t link_rate_bps)
{
	// 获取上次采样时间
    Time lastSample = m_congState[port].lastTrendSampleTime;
	double interval_ms = (Simulator::Now() - lastSample).GetMilliSeconds();
	if (interval_ms < 1.0) interval_ms = 1.0; // 防止除零

    // 查找速率组对应的标准化阈值表（单位bytes/ms）
	uint32_t rate_gbps = static_cast<uint32_t>(link_rate_bps / 1000000000ULL);
    auto normThreshIter = trendThreshNorm.find(rate_gbps);
    if (normThreshIter == trendThreshNorm.end()) {
        // 未找到则动态创建
        std::vector<uint32_t> thresh(kClassNum);
        uint64_t rate_bps = rate_gbps * 1000000000ULL;
        uint64_t bytes_per_ms = rate_bps / 8 / 1000;
        for (int i = 0; i < kClassNum; i++)
            thresh[i] = bytes_per_ms * (i + 1) / kClassNum;
        trendThreshNorm[rate_gbps] = thresh;
        normThreshIter = trendThreshNorm.find(rate_gbps);
		// std::cout << "[DCI " << this->GetId() << "] Dynamically created trendThreshNorm for " << rate_gbps << " Gbps" << std::endl;
    }
    const std::vector<uint32_t>& normThresh = normThreshIter->second;
    int32_t trend_val = m_congState[port].trend;
    
    if (trend_val <= 0) // 只关心增长趋势（正值），消退趋势可直接返回0
        return 0;
    for (int i = kClassNum - 1; i >= 0; i--) {
        if (trend_val >= static_cast<int32_t>(normThresh[i] * interval_ms))
            return levelScore[i];
    }
    return 0;
}


// 单次采样端口队列长度的调度函数
void DCISwitchNode::MonitorCongestionState()
{
    for (int port = 1; port < GetNDevices(); port++) {
        Ptr<QbbNetDevice> dev = DynamicCast<QbbNetDevice>(GetDevice(port));
        if (!dev) continue;
        // 获取当前端口所有队列的总字节数
        uint32_t qlen = dev->GetQueue()->GetNBytesTotal();
        m_congState[port].queueBytes_cur = qlen;
		// 更新采样时间
		m_congState[port].lastTrendSampleTime = Simulator::Now();
		// 更新队列增长趋势
        UpdateTrend(port);
        // 可选：输出调试信息
		// if (qlen > 0)
	        // std::cout << "[TEST] MonitorCongestionState: [DCI " << GetId() << "] port " << port << " queueBytes_cur=" << qlen << std::endl;
    }
}

// 计算拥塞成本（总合成）
uint8_t DCISwitchNode::CalcCongCost(uint32_t port, uint64_t link_rate_bps)
{
	// 1. 先更新当前队列拥塞状态
    MonitorCongestionState();
	
	// 权重求和
    const int w_q = 4, w_t = 2, w_d = 2, S = 3;
    uint8_t QLevel = CalcQLevel(port);
	// 更新拥塞持续性计数器
	UpdateDurationPenalty(port, QLevel);
    uint8_t DurationPenalty = CalcDurationPenalty(port);


    uint8_t TrendLevel = CalcTrendLevel(port, link_rate_bps);

    uint32_t cong_score = w_q * QLevel + w_t * TrendLevel + w_d * DurationPenalty;
    uint8_t cong_cost = std::min(cong_score >> S, 255u);
	
	// std::cout << "[TEST] CalcCongCost: [DCI " << GetId() << "] port " << port 
	// 		  << " QLevel=" << (int)QLevel 
	// 		  << " TrendLevel=" << (int)TrendLevel 
	// 		  << " DurationPenalty=" << (int)DurationPenalty 
	// 		  << " cong_cost=" << (int)cong_cost << std::endl;

    return cong_cost;
}

uint8_t DCISwitchNode::CalcDurationPenalty(uint32_t port)
{
    // 右移2位缩放，最大255
    return std::min((m_congState[port].durCounter >> 2), 255u);
}

// 更新计数器
void DCISwitchNode::UpdateDurationPenalty(uint32_t port, uint8_t QLevel)
{
    // 高水位阈值（如224，对应80%）
    if (QLevel >= 255 * 0.8)
        m_congState[port].durCounter += 1;
    else if (QLevel <= 255 * 0.4) // 低水位阈值（如40%）
        m_congState[port].durCounter = m_congState[port].durCounter > 0 ? m_congState[port].durCounter - 1 : 0;
}

// 定期清理超时流项
void DCISwitchNode::CleanIdleFlows()
{
	Time shortTimeout = MilliSeconds(5 * m_maxRtt / 1000000); // 5×RTT

    for (auto it = flow2outdev.begin(); it != flow2outdev.end(); ) {
        if (Simulator::Now() - it->second.lastSeen > IDLE_TIMEOUT) {
        // if (Simulator::Now() - it->second.lastSeen > shortTimeout) {
			uint64_t flowId = it->first;
            uint32_t outDevIdx = it->second.outDevIdx;

            std::cout << "[GC] [DCI " << GetId() << "] Remove idle flow " << it->first << std::endl;
            it = flow2outdev.erase(it);
        } else {
            it++;
        }
    }
}
uint64_t DCISwitchNode::GetTxBytesOutDev(uint32_t outdev) {
    NS_ASSERT_MSG(outdev < pCnt, "Invalid output device index");
    return m_txBytes[outdev];
}

uint8_t DCISwitchNode::clamp_uint8(int value, int low, int high) {
    return static_cast<uint8_t>(value < low ? low : (value > high ? high : value));
}

} /* namespace ns3 */

#ifndef DCI_SWITCH_NODE_H
#define DCI_SWITCH_NODE_H

#include <unordered_map>
#include <ns3/node.h>
#include "qbb-net-device.h"
#include "switch-mmu.h"
#include "rdma-queue-pair.h"
#include "rdma-hw.h"
#include "pint.h"

namespace ns3 {

class Packet;

class DCISwitchNode : public Node{
protected:
	bool saveRoutingChoice = false; // 是否将选择结果输出到本地

	static const uint32_t pCnt = 257;	// Number of ports used
	static const uint32_t qCnt = 8;	// Number of queues/priorities used
	uint32_t m_ecmpSeed;
	std::unordered_map<uint32_t, std::vector<int> > m_rtTable; // map from ip address (u32) to possible ECMP port (index of dev)

	// monitor of PFC
	uint32_t m_bytes[pCnt][pCnt][qCnt]; // m_bytes[inDev][outDev][qidx] is the bytes from inDev enqueued for outDev at qidx
	
	uint64_t m_txBytes[pCnt]; // counter of tx bytes

	uint32_t m_lastPktSize[pCnt];
	uint64_t m_lastPktTs[pCnt]; // ns
	double m_u[pCnt];

	uint32_t m_mtu; // Maximum Transmission Unit

	// [NEW] 拥塞成本相关
	const uint64_t m_bufferCapacity = 5ULL * 1000ULL * 1000ULL * 1000ULL; // 例如 5GB (单位统一为Byte); // 所有端口共享
	const uint64_t MAX_BW = 400; // 单位Gbps
	//[new] 延迟成本相关
	const int MAX_DELAY_SHIFT = 5; // 表示2**5 = 32ms

	// [NEW] 关于带宽
	static const int kClassNum = 10;
	uint64_t m_bw_thresh[kClassNum];   // 带宽阈值（单位Gbps）
	std::vector<uint32_t> qThresh; // 所有端口共享阈值表

	// 端口速率共享阈值表的带宽列表（以常见速率组为例：25G, 100G, 400G）
	std::vector<uint32_t> bwSharedList = {25, 100, 200, 400, 800}; // 单位Gbps
	// TrendLevel阈值表：每个速率组对应一个分段阈值表（单位：bytes/interval）
	std::map<uint32_t, std::vector<uint32_t>> trendThreshNorm; // key: link_rate_bps, value: 10级分段阈值

	// Level分数表（所有速率组共享，线性映射）for trend 
	std::vector<uint8_t> levelScore; // 10级分数

	int m_K = 3; // TrendLevel_p（队列增长速率) 所使用的平滑因子
	Time IDLE_TIMEOUT = Seconds(2); // 流闲置超时时间

	// 流信息的结构
	struct FlowEntry {
		uint32_t outDevIdx; // 输出端口索引
		Time lastSeen;      // 最近一次包到达时间
	};
	std::map<uint64_t, FlowEntry> flow2outdev; // flowId -> FlowEntry

// protected:

	bool m_ecnEnabled;
	uint32_t m_ccMode;
	uint64_t m_maxRtt;
	int m_routingMode; // 是否启用DCI路由
	uint32_t m_ackHighPrio; // set high priority for ACK/NACK

private:
	int GetOutDev(Ptr<const Packet>, CustomHeader &ch);
	void SendToDev(Ptr<Packet>p, CustomHeader &ch);
	static uint32_t EcmpHash(const uint8_t* key, size_t len, uint32_t seed);
	void CheckAndSendPfc(uint32_t inDev, uint32_t qIndex);
	void CheckAndSendResume(uint32_t inDev, uint32_t qIndex);

	// Calculate delay cost based on one-way delay
	uint8_t CalcDelayCost(uint16_t one_way_delay_ms);
	// Calculate bandwidth cost based on bandwidth (bps)
	uint8_t CalcBwCost(uint64_t bw_bps);

	// Calculate congestion cost
	uint8_t CalcCongCost(uint32_t port, uint64_t link_rate_bps);

	// Calculate queue level based on port
	uint8_t CalcQLevel(uint32_t port);
	// Calculate trend level based on port, link rate, and interval
	uint8_t CalcTrendLevel(uint32_t port, uint64_t link_rate_bps);
	// Update trend information
	void UpdateTrend(uint32_t port);

	// 计算持续时间惩罚
	uint8_t CalcDurationPenalty(uint32_t port);
	// 更新持续时间惩罚
	void UpdateDurationPenalty(uint32_t port, uint8_t QLevel);

public:
	// Cost calculation parameters
	uint32_t m_w_dl;
	uint32_t m_w_bw;
	uint32_t m_S_static;
	uint32_t m_w_ql;
	uint32_t m_w_tl;
	uint32_t m_w_dp;
	uint32_t m_S_cong;
	uint32_t m_alpha;
	uint32_t m_beta;
	uint32_t m_S_total;

	Ptr<SwitchMmu> m_mmu;

	static TypeId GetTypeId (void);
	DCISwitchNode();
	void SetEcmpSeed(uint32_t seed);
	void AddTableEntry(Ipv4Address &dstAddr, uint32_t intf_idx);
	void ClearTable();
	bool SwitchReceiveFromDevice(Ptr<NetDevice> device, Ptr<Packet> packet, CustomHeader &ch);
	void SwitchNotifyDequeue(uint32_t ifIndex, uint32_t qIndex, Ptr<Packet> p);

	// for approximate calc in PINT
	int logres_shift(int b, int l);
	int log2apprx(int x, int b, int m, int l); // given x of at most b bits, use most significant m bits of x, calc the result in l bits

	

	// [NEW] Link characteristics
	std::map<uint32_t, uint64_t> m_linkDelay;   // key: interface idx, value: delay (ns)
	std::map<uint32_t, uint64_t> m_linkBw;      // key: interface idx, value: bandwidth (bps)

	


	struct CongestionState {
		uint32_t queueBytes_cur = 0;      // 当前队列字节数
		uint32_t queueBytes_prev = 0;     // 上一次采样队列字节数
		int32_t  trend = 0;               // 队列增长趋势（平滑后的字节/周期）
		uint32_t durCounter = 0;          // 持续高占用计数器
		Time lastTrendSampleTime = Seconds(0); // 最近一次趋势采样时间
	};

	std::map<uint32_t, CongestionState> m_congState; // key: 端口号


	// 单次采样调度函数
	void MonitorCongestionState();

	// 定期清理超时流项
	void CleanIdleFlows();

	// 获取指定输出端口的发送字节数
	uint64_t GetTxBytesOutDev(uint32_t outdev);

	// 计算增量字节数
	uint64_t calcIncrementBytes(uint64_t actualBytes);

	// clamp function
	uint8_t clamp_uint8(int value, int low, int high);

};

} /* namespace ns3 */

#endif /* DCI_SWITCH_NODE_H */

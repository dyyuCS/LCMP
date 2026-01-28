/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007, 2008 University of Washington
 *
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

#include "qbb-channel.h"
#include "qbb-net-device.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include <iostream>
#include <fstream>
#include <iomanip> // 调用setw缩进函数

NS_LOG_COMPONENT_DEFINE ("QbbChannel");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (QbbChannel);

// 定义静态成员
std::set<uint32_t> QbbChannel::m_traceFlowIds; // 需要追踪的流ID set
std::map<uint32_t, std::ofstream*> QbbChannel::m_flowTraceFiles; // 添加文件流映射

std::string OUTPUT_DIR = "output"; // 默认输出目录

// 静态方法定义
// 添加清理函数
void QbbChannel::CleanupTraceFiles() {
    for (auto& file : m_flowTraceFiles) {
        if (file.second) {
            file.second->close();
            delete file.second;
        }
    }
    m_flowTraceFiles.clear();
}

void QbbChannel::AddTraceFlow(uint32_t flowId, std::string output_dir) {
    m_traceFlowIds.insert(flowId);
    // std::cout << "[QbbChannel] Adding flow " << flowId << " to trace list." << std::endl;
    OUTPUT_DIR = output_dir; // 更新输出目录
}

TypeId 
QbbChannel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::QbbChannel")
    .SetParent<Channel> ()
    .AddConstructor<QbbChannel> ()
    .AddAttribute ("Delay", "Transmission delay through the channel",
                   TimeValue (Seconds (0)),
                   MakeTimeAccessor (&QbbChannel::m_delay),
                   MakeTimeChecker ())
    .AddTraceSource ("TxRxQbb",
                     "Trace source indicating transmission of packet from the QbbChannel, used by the Animation interface.",
                     MakeTraceSourceAccessor (&QbbChannel::m_txrxQbb))
  ;
  return tid;
}

//
// By default, you get a channel that 
// has an "infitely" fast transmission speed and zero delay.
QbbChannel::QbbChannel()
  :
    PointToPointChannel ()
{
  NS_LOG_FUNCTION_NOARGS ();
  m_nDevices = 0;
  m_traceFlowIds = std::set<uint32_t>(); // 初始化追踪流ID集合
}

void
QbbChannel::Attach (Ptr<QbbNetDevice> device)
{
  
  //std::cout << m_nDevices << " " << N_DEVICES << "\n";
  //fflush(stdout);
  NS_LOG_FUNCTION (this << device);
  NS_ASSERT_MSG (m_nDevices < N_DEVICES, "Only two devices permitted");
  NS_ASSERT (device != 0);

  m_link[m_nDevices++].m_src = device;
//
// If we have both devices connected to the channel, then finish introducing
// the two halves and set the links to IDLE.
//
  if (m_nDevices == N_DEVICES)
    {
      m_link[0].m_dst = m_link[1].m_src;
      m_link[1].m_dst = m_link[0].m_src;
      m_link[0].m_state = IDLE;
      m_link[1].m_state = IDLE;
    }

}

// 辅助函数：将IP转换为节点ID
std::string str_ip_to_node_id(uint32_t ip){
  return std::to_string((ip >> 8) & 0xffff);
}

bool
QbbChannel::TransmitStart (
  Ptr<Packet> p,
  Ptr<QbbNetDevice> src,
  Time txTime)
{
  NS_LOG_FUNCTION (this << p << src);
  NS_LOG_LOGIC ("UID is " << p->GetUid () << ")");

  NS_ASSERT (m_link[0].m_state != INITIALIZING);
  NS_ASSERT (m_link[1].m_state != INITIALIZING);

  uint32_t wire = src == m_link[0].m_src ? 0 : 1; // 确定数据包在点对点信道中的传输方向
  // 假设有两个设备A和B通过这个信道连接：
  // 如果A发送数据到B, A是m_link[0].m_src，则wire=0, 数据将发送到m_link[0].m_dst（即B）
  // 如果B发送数据到A, B是m_link[1].m_src，则wire=1,数据将发送到m_link[1].m_dst（即A）

  // 添加详细的路由追踪信息
  CustomHeader ch(CustomHeader::L2_Header | CustomHeader::L3_Header | CustomHeader::L4_Header);
  p->PeekHeader(ch);

  uint32_t crt_flow_id = p->GetTraceFlowId(); // [new] 获取当前数据包的流ID;

  std::string srcNodeType;
  std::string dstNodeType;

  if (m_traceFlowIds.find(crt_flow_id) != m_traceFlowIds.end()) { // 检查当前流是否在追踪集合中
    // 获取或创建对应流的文件
    if (m_flowTraceFiles.find(crt_flow_id) == m_flowTraceFiles.end()) {
        std::string dir = OUTPUT_DIR + "flowTrace";
        NS_LOG_INFO("[QbbChannel] Creating directory: " << dir);
        system(("mkdir -p " + dir).c_str()); // Create directory if it doesn't exist
        std::string filename = dir + "/flow_" + std::to_string(crt_flow_id) + "_node_" + str_ip_to_node_id(ch.sip) + "_to_" + str_ip_to_node_id(ch.dip) + ".txt";
        m_flowTraceFiles[crt_flow_id] = new std::ofstream(filename.c_str());
    }

    // 获取节点类型
    switch(src->GetNode()->GetNodeType()) {
        case 0: srcNodeType = "Host  "; break;
        case 1: srcNodeType = "Switch"; break;
        case 2: srcNodeType = "DCI   "; break;
        default: srcNodeType = "Unknown"; break;
    }
    switch(m_link[wire].m_dst->GetNode()->GetNodeType()) {
        case 0: dstNodeType = "Host  "; break;
        case 1: dstNodeType = "Switch"; break;
        case 2: dstNodeType = "DCI   "; break;
        default: dstNodeType = "Unknown"; break;
    }

    // 写入到对应流的文件
    *m_flowTraceFiles[crt_flow_id] 
        << "[Flow-" << crt_flow_id 
        << " Seq-" << ch.udp.seq  // 包序列号(由于在RDMA中使用的是UDP协议,因此使用UDP头中的序列号获取包序列号,且包序号不是0,1,2,...的顺序,而是累加m_mtu,即根据单个payload大小计算的包序列号)
        << "] From " << srcNodeType << "-" << std::left << std::setw(4) << src->GetNode()->GetId()
        << " -> " 
        << "To " << dstNodeType << "-" << std::left << std::setw(4) << m_link[wire].m_dst->GetNode()->GetId()
        << " (Src IP:" << ch.sip << ":" << ch.udp.sport 
        << " -> Dst IP:" << ch.dip << ":" << ch.udp.dport << ")"
        << std::endl;
        
    // 确保立即写入文件
    m_flowTraceFiles[crt_flow_id]->flush();

    std::cout << "[Flow-" << crt_flow_id 
            << " Seq-" << ch.udp.seq
            << "] From " << srcNodeType << "-" << std::left << std::setw(4) << src->GetNode()->GetId()
            << " -> " 
            << "To " << dstNodeType << "-" << std::left << std::setw(4) << m_link[wire].m_dst->GetNode()->GetId()
            << " (Src IP:" << ch.sip << ":" << ch.udp.sport 
            << " -> Dst IP:" << ch.dip << ":" << ch.udp.dport << ")"
            << std::endl;
    
    std::string filename = "mix/output/WorkLoad/debug/routing/mixed_trace.txt"; // 统一的追踪文件名
    std::ofstream *traceFile = new std::ofstream(filename);
    p->Print(*traceFile);
  }

            
  Simulator::ScheduleWithContext (m_link[wire].m_dst->GetNode ()->GetId (), //与当前QbbNetDevice直连的对端设备，即“下一跳”的节点
                                  txTime + m_delay, &QbbNetDevice::Receive, // 调用对应网卡，完成收到包的操作
                                  m_link[wire].m_dst, p);

  // Call the tx anim callback on the net device
  m_txrxQbb (p, src, m_link[wire].m_dst, txTime, txTime + m_delay);
  return true;
}

uint32_t 
QbbChannel::GetNDevices (void) const
{
  NS_LOG_FUNCTION_NOARGS ();

  //std::cout<<m_nDevices<<"\n";
  //std::cout.flush();


  return m_nDevices;
}

Ptr<QbbNetDevice>
QbbChannel::GetQbbDevice (uint32_t i) const
{
  NS_LOG_FUNCTION_NOARGS ();
  NS_ASSERT (i < 2);
  return m_link[i].m_src;
}

Ptr<NetDevice>
QbbChannel::GetDevice (uint32_t i) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return GetQbbDevice (i);
}

Time
QbbChannel::GetDelay (void) const
{
  return m_delay;
}

Ptr<QbbNetDevice>
QbbChannel::GetSource (uint32_t i) const
{
  return m_link[i].m_src;
}

Ptr<QbbNetDevice>
QbbChannel::GetDestination (uint32_t i) const
{
  return m_link[i].m_dst;
}

bool
QbbChannel::IsInitialized (void) const
{
  NS_ASSERT (m_link[0].m_state != INITIALIZING);
  NS_ASSERT (m_link[1].m_state != INITIALIZING);
  return true;
}

} // namespace ns3

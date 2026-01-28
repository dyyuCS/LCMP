#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
多数据中心拓扑结构生成器
支持两种拓扑类型：
1. 简单拓扑：2个主机 → 1个ToR交换机 → 1个DCI交换机
2. 叶脊拓扑：主机 → 叶层交换机 → 脊层交换机 → DCI交换机
"""

import os

# ================== 拓扑类型选择 ==================
# 1: 简单拓扑, 2: 叶脊拓扑
TOPOLOGY_TYPE = 2

# ================== 通用参数设置 ==================
dc_num = 8                    # 数据中心数量
dci_per_dc = 1               # 每个数据中心的DCI交换机数量
dci_link_rate = '100Gbps'    # DCI链路速率
dci_link_latency = '10ms'   # DCI链路延迟

# ================== 1. 简单拓扑参数 ==================
simple_hosts_per_dc = 2       # 简单拓扑：每个DC的主机数量
simple_tors_per_dc = 1        # 简单拓扑：每个DC的ToR交换机数量
simple_link_rate = '100Gbps'  # 简单拓扑：内部链路速率
simple_link_latency = '1us' # 简单拓扑：内部链路延迟

# ================== 2. 叶脊拓扑参数 ==================
leafspine_hosts_per_dc = 32     # 叶脊拓扑：每个DC的主机数量 如128
leafspine_leaves_per_dc = 8      # 叶脊拓扑：每个DC的叶层交换机数量 如8
leafspine_spines_per_dc = 4      # 叶脊拓扑：每个DC的脊层交换机数量 如8
leafspine_hosts_per_leaf = 4    # 叶脊拓扑：每个叶层交换机连接的主机数量
leafspine_oversubscript = 2      # 叶脊拓扑：过载比
leafspine_link_rate = '100Gbps'    # 叶脊拓扑：内部链路速率
leafspine_link_latency = '1us'  # 叶脊拓扑：内部链路延迟

# ================== 拓扑生成基类 ==================
class TopologyGenerator:
    def __init__(self):
        pass
        
    def calculate_node_counts(self):
        """计算节点数量，由子类实现"""
        raise NotImplementedError
        
    def calculate_link_counts(self):
        """计算链路数量，由子类实现"""
        raise NotImplementedError
        
    def generate_topology_content(self):
        """生成拓扑内容，由子类实现"""
        raise NotImplementedError
        
    def write_to_file(self, filename):
        """将拓扑写入文件"""
        if os.path.exists(filename):
            print(f"警告: 文件 {filename} 已存在，将被覆盖。")
        
        with open(filename, 'w', encoding='utf-8') as f:
            f.write("\n".join(self.generate_topology_content()))
        
        print(f"成功生成拓扑文件: {filename}")
        print("注意: 文件中以#开头的行是注释，NS3读取拓扑时应跳过这些行。")

# ================== 简单拓扑生成器 ==================
class SimpleTopologyGenerator(TopologyGenerator):
    def __init__(self):
        super().__init__()
        self.topology_name = "简单拓扑"

    def calculate_node_counts(self):
        """计算简单拓扑的节点数量"""
        self.total_hosts = dc_num * simple_hosts_per_dc
        self.total_tors = dc_num * simple_tors_per_dc  
        self.total_dcis = dc_num * dci_per_dc
        self.total_nodes = self.total_hosts + self.total_tors + self.total_dcis
        
        # ID范围分配
        self.tor_id_start = self.total_hosts
        self.dci_id_start = self.total_hosts + self.total_tors
        
        return {
            'total_nodes': self.total_nodes,
            'total_intra_dc_switches': self.total_tors,
            'total_dcis': self.total_dcis
        }
        
    def calculate_link_counts(self):
        """计算简单拓扑的链路数量"""
        # 每个DC内部链路：主机到ToR + ToR到DCI
        links_per_dc = simple_hosts_per_dc + simple_tors_per_dc
        total_intra_dc_links = links_per_dc * dc_num
        
        # DCI间互联链路（完全图）
        dci_links = int(self.total_dcis * (self.total_dcis - 1) / 2)
        
        total_links = total_intra_dc_links + dci_links
        return total_links
        
    def generate_topology_content(self):
        """生成简单拓扑内容"""
        node_counts = self.calculate_node_counts()
        total_links = self.calculate_link_counts()
        
        output = []
        
        # 添加文件头注释
        output.extend(self._generate_header_comments())
        
        # 添加节点分配信息
        output.extend(self._generate_node_allocation_comments())
        
        # 添加总节点数、总交换机数、总链路数
        output.append(f"{node_counts['total_nodes']} {node_counts['total_intra_dc_switches']} {node_counts['total_dcis']} {total_links}")
        
        # 添加所有交换机ID
        output.append(self._generate_switch_ids())
        output.append("")
        
        # 生成每个数据中心的连接
        output.extend(self._generate_intra_dc_connections())
        
        # 生成DCI间互联
        output.extend(self._generate_dci_interconnections())
        
        # 添加结尾注释
        output.append("")
        output.append("# 如果删减DCI互连链路，别忘记修改文件开头的<总链路数>")
        
        return output
        
    def _generate_header_comments(self):
        """生成文件头注释"""
        return [
            f"# 多数据中心{self.topology_name}结构",
            "# 拓扑类型: 简单拓扑（2个主机 → 1个ToR交换机 → 1个DCI交换机）",
            "# 参数设置:",
            f"# 数据中心数量: {dc_num}",
            f"# 每个数据中心的主机数量: {simple_hosts_per_dc}",
            f"# 每个数据中心的ToR交换机数量: {simple_tors_per_dc}",
            f"# 每个数据中心的DCI交换机数量: {dci_per_dc}",
            f"# DC内链路速率: {simple_link_rate}",
            f"# DC内链路延迟: {simple_link_latency}",
            f"# DCI链路速率: {dci_link_rate}",
            f"# DCI链路延迟: {dci_link_latency}",
            ""
        ]
        
    def _generate_node_allocation_comments(self):
        """生成节点分配注释"""
        output = ["# 数据中心节点分配:"]
        
        for i in range(dc_num):
            host_start = i * simple_hosts_per_dc
            host_end = host_start + simple_hosts_per_dc - 1
            tor = self.tor_id_start + i * simple_tors_per_dc
            if dci_per_dc > 0:
                dci = self.dci_id_start + i
                output.append(f"# DC{i+1}: 主机 {host_start}-{host_end}, ToR交换机 {tor}, DCI交换机 {dci}")
            else:
                output.append(f"# DC{i+1}: 主机 {host_start}-{host_end}, ToR交换机 {tor}, 无DCI交换机")
        output.extend([
            "",
            "# 文件格式:",
            "# 第一行: <总节点数> <总DC内交换机数> <总DCI交换机数> <总链路数>",
            "# 第二行: 所有DC内 交换机ID列表",
            "# 第三行: 所有DCI 交换机ID列表",
            "# 后续行: <源节点> <目标节点> <带宽> <延迟> <丢包率>",
            ""
        ])
        
        return output
        
    def _generate_switch_ids(self):
      """生成所有交换机ID列表"""
      # ToR交换机ID
      tor_ids = [str(self.tor_id_start + i * simple_tors_per_dc) for i in range(dc_num)]
      # DCI交换机ID
      dci_ids = [str(self.dci_id_start + i) for i in range(dc_num)]
      # 用换行分隔两组ID，避免多余空格
      return " ".join(tor_ids) + "\n" + " ".join(dci_ids)
        
    def _generate_intra_dc_connections(self):
        """生成DC内部连接"""
        output = []
        
        for dc_idx in range(dc_num):
            host_start = dc_idx * simple_hosts_per_dc
            tor = self.tor_id_start + dc_idx * simple_tors_per_dc
            dci = self.dci_id_start + dc_idx
            
            output.append(f"# DC{dc_idx+1} 内部连接")
            
            # 主机到ToR连接
            output.append("# 主机到ToR交换机连接")
            for i in range(simple_hosts_per_dc):
                host = host_start + i
                output.append(f"{host} {tor} {simple_link_rate} {simple_link_latency} 0")
                
            # ToR到DCI连接
            output.append("# ToR交换机到DCI交换机连接")
            output.append(f"{tor} {dci} {simple_link_rate} {simple_link_latency} 0")
            output.append("")
            
        return output
        
    def _generate_dci_interconnections(self):
        """生成DCI间互联"""
        if dci_per_dc == 0:
            output = ["# 无DCI交换机，跳过DCI间互联"]
        else:
            output = ["# DCI交换机间互联（完全图）"]
        
            for i in range(dc_num):
                dci1 = self.dci_id_start + i
                for j in range(i+1, dc_num):
                    dci2 = self.dci_id_start + j
                    output.append(f"{dci1} {dci2} {dci_link_rate} {dci_link_latency} 0")
                
        return output

# ================== 叶脊拓扑生成器 ==================
class LeafSpineTopologyGenerator(TopologyGenerator):
    def __init__(self):
        super().__init__()
        self.topology_name = "叶脊拓扑"
        # print(f"dci_per_dc: {dci_per_dc}")

        
    def calculate_node_counts(self):
        """计算叶脊拓扑的节点数量"""
        self.total_hosts = dc_num * leafspine_hosts_per_dc
        self.total_leaves = dc_num * leafspine_leaves_per_dc
        self.total_spines = dc_num * leafspine_spines_per_dc
        self.total_dcis = dc_num * dci_per_dc
        self.total_nodes = self.total_hosts + self.total_leaves + self.total_spines + self.total_dcis
        
        # ID范围分配
        self.leaf_id_start = self.total_hosts
        self.spine_id_start = self.total_hosts + self.total_leaves
        self.dci_id_start = self.total_hosts + self.total_leaves + self.total_spines
        
        return {
            'total_nodes': self.total_nodes,
            'total_intra_dc_switches': self.total_leaves + self.total_spines,
            'total_dcis': self.total_dcis
        }
        
    def calculate_link_counts(self):
        """计算叶脊拓扑的链路数量"""
        # 每个DC内部链路
        if dci_per_dc > 0:
            links_per_dc = (leafspine_hosts_per_dc + 
                        (leafspine_leaves_per_dc * leafspine_spines_per_dc) + 
                        leafspine_spines_per_dc)
        # 如果没有DCI交换机，则脊层交换机不连接到DCI
        else:
            links_per_dc = (leafspine_hosts_per_dc + 
                        (leafspine_leaves_per_dc * leafspine_spines_per_dc))

        total_intra_dc_links = links_per_dc * dc_num
        
        # DCI间互联链路
        dci_links = int(self.total_dcis * (self.total_dcis - 1) / 2)
        
        total_links = total_intra_dc_links + dci_links
        return total_links
        
    def generate_topology_content(self):
        """生成叶脊拓扑内容"""
        node_counts = self.calculate_node_counts()
        total_links = self.calculate_link_counts()
        
        output = []
        
        # 添加文件头注释
        output.extend(self._generate_header_comments())
        
        # 添加节点分配信息
        output.extend(self._generate_node_allocation_comments())
        
        # 添加总节点数、总交换机数、总链路数
        if dci_per_dc > 0:
            output.append(f"{node_counts['total_nodes']} {node_counts['total_intra_dc_switches']} {node_counts['total_dcis']} {total_links}")
        else:
            output.append(f"{node_counts['total_nodes']} {node_counts['total_intra_dc_switches']} {total_links}")

        # 添加所有交换机ID
        output.append(self._generate_switch_ids())
        output.append("")
        
        # 生成每个数据中心的连接
        output.extend(self._generate_intra_dc_connections())
        
        # 生成DCI间互联
        output.extend(self._generate_dci_interconnections())
        
        # 添加结尾注释
        output.append("")
        output.append("# 如果删减DCI互连链路，别忘记修改文件开头的<总链路数>")
        
        return output
        
    def _generate_header_comments(self):
        """生成文件头注释"""
        return [
            f"# 多数据中心{self.topology_name}结构",
            "# 拓扑类型: 叶脊拓扑（主机 → 叶层交换机 → 脊层交换机 → DCI交换机）",
            "# 参数设置:",
            f"# 数据中心数量: {dc_num}",
            f"# 每个数据中心的主机数量: {leafspine_hosts_per_dc}",
            f"# 每个数据中心的叶层交换机数量: {leafspine_leaves_per_dc}",
            f"# 每个数据中心的脊层交换机数量: {leafspine_spines_per_dc}",
            f"# 每个叶层交换机连接的主机数量: {leafspine_hosts_per_leaf}",
            f"# 过载比: {leafspine_oversubscript}:1",
            f"# DC内链路速率: {leafspine_link_rate}",
            f"# DC内链路延迟: {leafspine_link_latency}",
            f"# DCI链路速率: {dci_link_rate}",
            f"# DCI链路延迟: {dci_link_latency}",
            ""
        ]
        
    def _generate_node_allocation_comments(self):
        """生成节点分配注释"""
        output = ["# 数据中心节点分配:"]
        
        for i in range(dc_num):
            host_start = i * leafspine_hosts_per_dc
            host_end = host_start + leafspine_hosts_per_dc - 1
            leaf_start = self.leaf_id_start + i * leafspine_leaves_per_dc
            leaf_end = leaf_start + leafspine_leaves_per_dc - 1
            spine_start = self.spine_id_start + i * leafspine_spines_per_dc
            spine_end = spine_start + leafspine_spines_per_dc - 1
            dci = self.dci_id_start + i
            
            if dci_per_dc > 0:
                output.append(f"# DC{i+1}: 主机 {host_start}-{host_end}, 叶层交换机 {leaf_start}-{leaf_end}, 脊层交换机 {spine_start}-{spine_end}, DCI交换机 {dci}")
            else:
                output.append(f"# DC{i+1}: 主机 {host_start}-{host_end}, 叶层交换机 {leaf_start}-{leaf_end}, 脊层交换机 {spine_start}-{spine_end}, 无DCI交换机")

        output.extend([
            "",
            "# 文件格式:",
            "# 第一行: <总节点数> <总DC内交换机数> <总DCI交换机数> <总链路数>",
            "# 第二行: 所有交换机ID列表",
            "# 后续行: <源节点> <目标节点> <带宽> <延迟> <丢包率>",
            ""
        ])
        
        return output
        
    def _generate_switch_ids(self):
        """生成所有交换机ID列表"""
        switch_ids = []
        dci_ids = []
        # 添加叶层交换机ID
        for i in range(dc_num):
            leaf_start = self.leaf_id_start + i * leafspine_leaves_per_dc
            for j in range(leafspine_leaves_per_dc):
                switch_ids.append(str(leaf_start + j))
                
        # 添加脊层交换机ID
        for i in range(dc_num):
            spine_start = self.spine_id_start + i * leafspine_spines_per_dc
            for j in range(leafspine_spines_per_dc):
                switch_ids.append(str(spine_start + j))
                
        # 添加DCI交换机ID
        if dci_per_dc > 0:
            for i in range(dc_num):
                dci_ids.append(str(self.dci_id_start + i))
            
        return " ".join(switch_ids) + "\n" + " ".join(dci_ids)

    def _generate_intra_dc_connections(self):
        """生成DC内部连接"""
        output = []
        
        for dc_idx in range(dc_num):
            host_start = dc_idx * leafspine_hosts_per_dc
            leaf_start = self.leaf_id_start + dc_idx * leafspine_leaves_per_dc
            spine_start = self.spine_id_start + dc_idx * leafspine_spines_per_dc
            if dci_per_dc > 0:
                dci = self.dci_id_start + dc_idx
            else:
                dci = 0
            
            output.append(f"# DC{dc_idx+1} 内部连接")
            
            # 主机到叶层交换机连接
            output.append("# 主机到叶层交换机连接")
            for i in range(leafspine_leaves_per_dc):
                leaf = leaf_start + i
                for j in range(leafspine_hosts_per_leaf):
                    host = host_start + i * leafspine_hosts_per_leaf + j
                    output.append(f"{host} {leaf} {leafspine_link_rate} {leafspine_link_latency} 0")
                    
            # 叶层到脊层交换机连接
            output.append("# 叶层交换机到脊层交换机连接")
            for i in range(leafspine_leaves_per_dc):
                leaf = leaf_start + i
                for j in range(leafspine_spines_per_dc):
                    spine = spine_start + j
                    output.append(f"{leaf} {spine} {leafspine_link_rate} {leafspine_link_latency} 0")
                    
            # 脊层到DCI连接
            output.append("# 脊层交换机到DCI交换机连接")
            if dci_per_dc > 0:
                for i in range(leafspine_spines_per_dc):
                    spine = spine_start + i
                    output.append(f"{spine} {dci} {leafspine_link_rate} {leafspine_link_latency} 0")
            
            output.append("")
            
        return output
        
    def _generate_dci_interconnections(self):
        """生成DCI间互联"""
        if dci_per_dc == 0:
            output = ["# 无DCI交换机，跳过DCI间互联"]
        else:
            output = ["# DCI交换机间互联（完全图）"]
            
            for i in range(dc_num):
                dci1 = self.dci_id_start + i
                for j in range(i+1, dc_num):
                    dci2 = self.dci_id_start + j
                    output.append(f"{dci1} {dci2} {dci_link_rate} {dci_link_latency} 0")
                
        return output

# ================== 主函数 ==================
def main():
    """主函数"""
    # 获取当前脚本所在目录
    script_dir = os.path.dirname(os.path.abspath(__file__))

    # 根据TOPOLOGY_TYPE选择生成器
    if TOPOLOGY_TYPE == 1:
        generator = SimpleTopologyGenerator()
        output_filename = f"topology_Simple_MultiDC{dc_num}.txt"
        print(f"正在生成简单拓扑...")
    elif TOPOLOGY_TYPE == 2:
        generator = LeafSpineTopologyGenerator()
        output_filename = f"topology_LeafSpine_MultiDC{dc_num}.txt"
        print(f"正在生成叶脊拓扑...")
    else:
        print(f"错误: 不支持的拓扑类型 {TOPOLOGY_TYPE}")
        return
    
    # 拼接为绝对路径，确保文件生成在脚本同目录下
    output_path = os.path.join(script_dir, output_filename)

    # 生成拓扑文件
    try:
        generator.write_to_file(output_path)

        # 显示生成的拓扑信息
        node_counts = generator.calculate_node_counts()
        total_links = generator.calculate_link_counts()
        
        print(f"\n生成的拓扑统计信息:")
        print(f"总节点数: {node_counts['total_nodes']}")
        print(f"总主机数: {generator.total_hosts}")
        if TOPOLOGY_TYPE == 1:
            print(f"总ToR交换机数: {generator.total_tors}")
        else:
            print(f"总叶层交换机数: {generator.total_leaves}")
            print(f"总脊层交换机数: {generator.total_spines}")
        print(f"总DCI交换机数: {generator.total_dcis}")
        print(f"总链路数: {total_links}")
        
        print(f"\n拓扑文件生成完成: {output_filename}")
        
    except Exception as e:
        print(f"生成拓扑文件时出错: {e}")

if __name__ == "__main__":
    main()
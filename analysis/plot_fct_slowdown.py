import pandas as pd
import matplotlib.pyplot as plt
import argparse
import os
import numpy as np

def plot_fct_slowdown(csv_file, output_dir=None):
    """
    绘制FCT slowdown对比图
    横轴: FlowSize
    纵轴: FCT slowdown
    对比不同算法的p50和p99（p50实线，p99虚线，在同一张图）
    """
    # 读取CSV文件
    df = pd.read_csv(csv_file)
    
    # 如果未指定输出目录，则使用CSV文件所在目录
    if output_dir is None:
        output_dir = os.path.dirname(csv_file)
    
    # 创建输出目录
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    
    # 获取文件名（不含扩展名）作为图片标题的一部分
    base_name = os.path.splitext(os.path.basename(csv_file))[0]
    
    # 提取FlowSize
    flow_sizes = df['FlowSize'].values
    
    # 获取所有算法列（排除Percentile和FlowSize）
    columns = [col for col in df.columns if col not in ['Percentile', 'FlowSize']]
    
    # 分离p50和p99的列
    p50_cols = [col for col in columns if 'p50' in col]
    p99_cols = [col for col in columns if 'p99' in col]
    
    # 定义颜色和标记样式
    colors = ['#1f77b4', '#ff7f0e', '#2ca02c', '#d62728', '#9467bd', 
              '#8c564b', '#e377c2', '#7f7f7f', '#bcbd22', '#17becf']
    markers = ['o', 's', '^', 'D', 'v', '<', '>', 'p', '*', 'h']
    
    # 创建单个图
    fig, ax = plt.subplots(figsize=(12, 7))
    
    # 绘制p50（实线）
    for idx, col in enumerate(p50_cols):
        # 提取算法名称
        algo_name = col.replace('-fct_p50', '')
        ax.plot(flow_sizes, df[col].values, 
                marker=markers[idx % len(markers)], 
                color=colors[idx % len(colors)],
                label=f'{algo_name} (p50)', 
                linewidth=2, 
                linestyle='-',  # 实线
                markersize=6,
                markevery=max(1, len(flow_sizes)//10))
    
    # 绘制p99（虚线）
    for idx, col in enumerate(p99_cols):
        # 提取算法名称
        algo_name = col.replace('-fct_p99', '')
        ax.plot(flow_sizes, df[col].values, 
                marker=markers[idx % len(markers)], 
                color=colors[idx % len(colors)],
                label=f'{algo_name} (p99)', 
                linewidth=2, 
                linestyle='--',  # 虚线
                markersize=6,
                markevery=max(1, len(flow_sizes)//10))
    
    ax.set_xlabel('Flow Size (Bytes)', fontsize=12, fontweight='bold')
    ax.set_ylabel('FCT Slowdown', fontsize=12, fontweight='bold')
    ax.set_title(f'FCT Slowdown Comparison (p50 & p99)\n{base_name}', fontsize=14, fontweight='bold')
    ax.set_xscale('log')
    ax.set_yscale('log')
    ax.grid(True, alpha=0.3, linestyle='--')
    ax.legend(loc='best', fontsize=9, ncol=2)
    ax.tick_params(labelsize=10)
    
    plt.tight_layout()
    
    # 保存图片
    output_file = os.path.join(output_dir, f'{base_name}_combined.png')
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"图片已保存到: {output_file}")
    
    # 显示图片
    # plt.show()
    plt.close()


def plot_fct_slowdown_separate(csv_file, output_dir=None):
    """
    绘制FCT slowdown对比图（分开绘制p50和p99）
    """
    # 读取CSV文件
    df = pd.read_csv(csv_file)
    
    # 如果未指定输出目录，则使用CSV文件所在目录
    if output_dir is None:
        output_dir = os.path.dirname(csv_file)
    
    # 创建输出目录
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    
    # 获取文件名（不含扩展名）
    base_name = os.path.splitext(os.path.basename(csv_file))[0]
    
    # 提取FlowSize
    flow_sizes = df['FlowSize'].values
    
    # 获取所有算法列
    columns = [col for col in df.columns if col not in ['Percentile', 'FlowSize']]
    
    # 分离p50和p99的列
    p50_cols = [col for col in columns if 'p50' in col]
    p99_cols = [col for col in columns if 'p99' in col]
    
    # 定义颜色和标记样式
    colors = ['#1f77b4', '#ff7f0e', '#2ca02c', '#d62728', '#9467bd', 
              '#8c564b', '#e377c2', '#7f7f7f', '#bcbd22', '#17becf']
    markers = ['o', 's', '^', 'D', 'v', '<', '>', 'p', '*', 'h']
    
    # 绘制p50图
    fig, ax = plt.subplots(figsize=(10, 6))
    for idx, col in enumerate(p50_cols):
        algo_name = col.replace('-fct_p50', '')
        ax.plot(flow_sizes, df[col].values, 
                marker=markers[idx % len(markers)], 
                color=colors[idx % len(colors)],
                label=algo_name, 
                linewidth=2, 
                markersize=6,
                markevery=max(1, len(flow_sizes)//10))
    
    ax.set_xlabel('Flow Size (Bytes)', fontsize=12, fontweight='bold')
    ax.set_ylabel('FCT Slowdown', fontsize=12, fontweight='bold')
    ax.set_title(f'FCT Slowdown Comparison (p50)\n{base_name}', fontsize=14, fontweight='bold')
    ax.set_xscale('log')
    ax.set_yscale('log')
    ax.grid(True, alpha=0.3, linestyle='--')
    ax.legend(loc='best', fontsize=10)
    ax.tick_params(labelsize=10)
    plt.tight_layout()
    
    output_file = os.path.join(output_dir, f'{base_name}_p50.png')
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"p50图片已保存到: {output_file}")
    plt.close()
    
    # 绘制p99图
    fig, ax = plt.subplots(figsize=(10, 6))
    for idx, col in enumerate(p99_cols):
        algo_name = col.replace('-fct_p99', '')
        ax.plot(flow_sizes, df[col].values, 
                marker=markers[idx % len(markers)], 
                color=colors[idx % len(colors)],
                label=algo_name, 
                linewidth=2, 
                markersize=6,
                markevery=max(1, len(flow_sizes)//10))
    
    ax.set_xlabel('Flow Size (Bytes)', fontsize=12, fontweight='bold')
    ax.set_ylabel('FCT Slowdown', fontsize=12, fontweight='bold')
    ax.set_title(f'FCT Slowdown Comparison (p99)\n{base_name}', fontsize=14, fontweight='bold')
    ax.set_xscale('log')
    ax.set_yscale('log')
    ax.grid(True, alpha=0.3, linestyle='--')
    ax.legend(loc='best', fontsize=10)
    ax.tick_params(labelsize=10)
    plt.tight_layout()
    
    output_file = os.path.join(output_dir, f'{base_name}_p99.png')
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"p99图片已保存到: {output_file}")
    plt.close()


def batch_plot(input_dir='result', output_dir=None):
    """
    批量处理指定目录下的所有CSV文件
    """
    # 遍历input_dir下的所有CSV文件
    for root, dirs, files in os.walk(input_dir):
        for file in files:
            if file.endswith('.csv') and 'FCTslowdown' in file:
                csv_path = os.path.join(root, file)
                print(f"\n处理文件: {csv_path}")
                
                # 如果指定了输出目录，则使用相对路径结构；否则输出到CSV同级目录
                if output_dir:
                    rel_path = os.path.relpath(root, input_dir)
                    current_output_dir = os.path.join(output_dir, rel_path)
                else:
                    current_output_dir = root  # CSV文件所在目录
                
                try:
                    # 绘制合并图（p50实线 + p99虚线）
                    plot_fct_slowdown(csv_path, current_output_dir)
                except Exception as e:
                    print(f"处理文件 {csv_path} 时出错: {e}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='绘制FCT slowdown对比图')
    parser.add_argument('-i', '--input', dest='input', action='store', 
                        help='输入CSV文件路径（单个文件）')
    parser.add_argument('-d', '--input_dir', dest='input_dir', action='store', 
                        default='result', help='输入CSV文件目录（批量处理）')
    parser.add_argument('-o', '--output', dest='output_dir', action='store', 
                        default=None, help='输出图片目录（默认为CSV文件所在目录）')
    parser.add_argument('-m', '--mode', dest='mode', action='store', 
                        default='batch', choices=['single', 'batch'],
                        help='处理模式: single(单个文件) 或 batch(批量处理)')
    
    args = parser.parse_args()
    
    if args.mode == 'single':
        if not args.input:
            print("错误: 单个文件模式需要指定 -i 参数")
            exit(1)
        if not os.path.exists(args.input):
            print(f"错误: 文件不存在: {args.input}")
            exit(1)
        
        print(f"处理单个文件: {args.input}")
        plot_fct_slowdown(args.input, args.output_dir)
    else:
        # 批量处理模式
        if not os.path.exists(args.input_dir):
            print(f"错误: 目录不存在: {args.input_dir}")
            exit(1)
        
        print(f"批量处理目录: {args.input_dir}")
        batch_plot(args.input_dir, args.output_dir)
    
    print("\n所有图片生成完成！")


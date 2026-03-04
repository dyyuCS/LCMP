import pandas as pd
import matplotlib.pyplot as plt
import argparse
import os
import numpy as np

def plot_fct_slowdown(csv_file, output_dir=None):
    """
    Plot FCT slowdown comparison.
    X-axis: Flow Size
    Y-axis: FCT Slowdown
    Compares p50 and p99 of different algorithms (p50 as solid line, p99 as dashed line, on the same plot).
    """
    # Read CSV file
    df = pd.read_csv(csv_file)
    
    # If output directory is not specified, use the CSV file's directory
    if output_dir is None:
        output_dir = os.path.dirname(csv_file)
    
    # Create output directory
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    
    # Get filename (without extension) to use as part of the plot title
    base_name = os.path.splitext(os.path.basename(csv_file))[0]
    
    # Extract FlowSize
    flow_sizes = df['FlowSize'].values
    
    # Get all algorithm columns (exclude Percentile and FlowSize)
    columns = [col for col in df.columns if col not in ['Percentile', 'FlowSize']]
    
    # Separate p50 and p99 columns
    p50_cols = [col for col in columns if 'p50' in col]
    p99_cols = [col for col in columns if 'p99' in col]
    
    # Define colors and marker styles
    colors = ['#1f77b4', '#ff7f0e', '#2ca02c', '#d62728', '#9467bd', 
              '#8c564b', '#e377c2', '#7f7f7f', '#bcbd22', '#17becf']
    markers = ['o', 's', '^', 'D', 'v', '<', '>', 'p', '*', 'h']
    
    # Create a single figure
    fig, ax = plt.subplots(figsize=(12, 7))
    
    # Plot p50 (solid line)
    for idx, col in enumerate(p50_cols):
        # Extract algorithm name
        algo_name = col.replace('-fct_p50', '')
        ax.plot(flow_sizes, df[col].values, 
                marker=markers[idx % len(markers)], 
                color=colors[idx % len(colors)],
                label=f'{algo_name} (p50)', 
                linewidth=2, 
                linestyle='-',  # Solid line
                markersize=6,
                markevery=max(1, len(flow_sizes)//10))
    
    # Plot p99 (dashed line)
    for idx, col in enumerate(p99_cols):
        # Extract algorithm name
        algo_name = col.replace('-fct_p99', '')
        ax.plot(flow_sizes, df[col].values, 
                marker=markers[idx % len(markers)], 
                color=colors[idx % len(colors)],
                label=f'{algo_name} (p99)', 
                linewidth=2, 
                linestyle='--',  # Dashed line
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
    
    # Save the plot
    output_file = os.path.join(output_dir, f'{base_name}_combined.png')
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Plot saved to: {output_file}")
    
    # Show the plot
    # plt.show()
    plt.close()


def plot_fct_slowdown_separate(csv_file, output_dir=None):
    """
    Plot FCT slowdown comparison (separate plots for p50 and p99).
    """
    # Read CSV file
    df = pd.read_csv(csv_file)
    
    # If output directory is not specified, use the CSV file's directory
    if output_dir is None:
        output_dir = os.path.dirname(csv_file)
    
    # Create output directory
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    
    # Get filename (without extension)
    base_name = os.path.splitext(os.path.basename(csv_file))[0]
    
    # Extract FlowSize
    flow_sizes = df['FlowSize'].values
    
    # Get all algorithm columns
    columns = [col for col in df.columns if col not in ['Percentile', 'FlowSize']]
    
    # Separate p50 and p99 columns
    p50_cols = [col for col in columns if 'p50' in col]
    p99_cols = [col for col in columns if 'p99' in col]
    
    # Define colors and marker styles
    colors = ['#1f77b4', '#ff7f0e', '#2ca02c', '#d62728', '#9467bd', 
              '#8c564b', '#e377c2', '#7f7f7f', '#bcbd22', '#17becf']
    markers = ['o', 's', '^', 'D', 'v', '<', '>', 'p', '*', 'h']
    
    # Plot p50 figure
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
    print(f"p50 plot saved to: {output_file}")
    plt.close()
    
    # Plot p99 figure
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
    print(f"p99 plot saved to: {output_file}")
    plt.close()


def batch_plot(input_dir='result', output_dir=None):
    """
    Batch process all CSV files in the specified directory.
    """
    # Traverse all CSV files in input_dir
    for root, dirs, files in os.walk(input_dir):
        for file in files:
            if file.endswith('.csv') and 'FCTslowdown' in file:
                csv_path = os.path.join(root, file)
                print(f"\nProcessing file: {csv_path}")
                
                # If output directory is specified, use relative path structure; otherwise, output to the CSV file's directory
                if output_dir:
                    rel_path = os.path.relpath(root, input_dir)
                    current_output_dir = os.path.join(output_dir, rel_path)
                else:
                    current_output_dir = root  # Directory where the CSV file is located
                
                try:
                    # Plot combined figure (p50 solid line + p99 dashed line)
                    plot_fct_slowdown(csv_path, current_output_dir)
                except Exception as e:
                    print(f"Error processing file {csv_path}: {e}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Plot FCT slowdown comparison')
    parser.add_argument('-i', '--input', dest='input', action='store', 
                        help='Input CSV file path (single file)')
    parser.add_argument('-d', '--input_dir', dest='input_dir', action='store', 
                        default='result', help='Input CSV file directory (batch processing)')
    parser.add_argument('-o', '--output', dest='output_dir', action='store', 
                        default=None, help='Output plot directory (defaults to the CSV file\'s directory)')
    parser.add_argument('-m', '--mode', dest='mode', action='store', 
                        default='batch', choices=['single', 'batch'],
                        help='Processing mode: single (single file) or batch (batch processing)')
    
    args = parser.parse_args()
    
    if args.mode == 'single':
        if not args.input:
            print("Error: Single file mode requires the -i parameter.")
            exit(1)
        if not os.path.exists(args.input):
            print(f"Error: File does not exist: {args.input}")
            exit(1)
        
        print(f"Processing single file: {args.input}")
        plot_fct_slowdown(args.input, args.output_dir)
    else:
        # Batch processing mode
        if not os.path.exists(args.input_dir):
            print(f"Error: Directory does not exist: {args.input_dir}")
            exit(1)
        
        print(f"Batch processing directory: {args.input_dir}")
        batch_plot(args.input_dir, args.output_dir)
    
    print("\nAll plots generated successfully!")
#!/usr/bin/env python3
import os
import argparse
import pandas as pd
import matplotlib.pyplot as plt
import re
import numpy as np

def parse_bandwidth(bw_str):
    """Converts bandwidth string (e.g., '200Gbps', '100Mbps') to bps."""
    if isinstance(bw_str, (int, float)):
        return bw_str
    
    bw_str = bw_str.lower()
    val = float(re.findall(r'[0-9\.]+', bw_str)[0])
    
    if 'gbps' in bw_str:
        return val * 1024 * 1024 * 1024
    elif 'mbps' in bw_str:
        return val * 1024 * 1024
    elif 'kbps' in bw_str:
        return val * 1024
    else:
        return val # Assume bps if no unit

def load_bandwidths(topology_file):
    """Loads link bandwidths from the topology file."""
    bandwidths = {}
    with open(topology_file, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            
            parts = line.split()
            if len(parts) >= 3:
                try:
                    src, dst, bw_str = int(parts[0]), int(parts[1]), parts[2]
                    bw = parse_bandwidth(bw_str)
                    # Use a canonical representation for the link pair (sorted tuple)
                    link_pair = tuple(sorted((src, dst)))
                    bandwidths[link_pair] = bw
                except (ValueError, IndexError):
                    continue
    
    print("\n--- Parsed Link Bandwidths (Gbps) ---")
    for link, bw in bandwidths.items():
        print("Link {}-{}: {:.2f} Gbps".format(link[0], link[1], bw / 1e9))
    print("-------------------------------------\n")
    return bandwidths

def load_cutoff_from_fct(fct_file_path):
    """
    Loads FCT records and returns the last flow completion time in ns.
    Expected columns per line (space-separated):
      sip_hex dip_hex sport dport size start_time fct standalone_fct
    where times are in ns. Completion time = start_time + fct.
    """
    if not os.path.exists(fct_file_path):
        print("FCT file not found: {}. Will not apply cutoff.".format(fct_file_path))
        return None

    try:
        cols = ['sip_hex', 'dip_hex', 'sport', 'dport', 'size', 'start_time', 'fct', 'standalone_fct']
        fdf = pd.read_csv(
            fct_file_path,
            sep=r"\s+",
            header=None,
            names=cols,
            engine='python'
        )
    except Exception as e:
        print("Failed to read FCT file {}: {}. Will not apply cutoff.".format(fct_file_path, e))
        return None

    if fdf is None or fdf.empty:
        print("FCT file is empty: {}. Will not apply cutoff.".format(fct_file_path))
        return None

    # Ensure numeric types for time columns
    for col in ['start_time', 'fct']:
        fdf[col] = pd.to_numeric(fdf[col], errors='coerce')
    fdf.dropna(subset=['start_time', 'fct'], inplace=True)
    if fdf.empty:
        print("FCT file contains no valid timing records: {}. Will not apply cutoff.".format(fct_file_path))
        return None

    fdf['completion_time'] = fdf['start_time'] + fdf['fct']
    cutoff_ns = int(fdf['completion_time'].max())
    print("Detected last flow completion at {:.6f} s ({} ns) from {}".format(cutoff_ns / 1e9, cutoff_ns, fct_file_path))
    return cutoff_ns


def get_last_timestamp_ns(link_util_file_path):
    """Reads link_util file and returns its maximum timestamp in ns, or None on failure."""
    try:
        df = pd.read_csv(link_util_file_path, header=None, names=['timestamp', 'src', 'dst', 'cum_bytes'])
    except Exception as e:
        print("Failed to read link util file {}: {}".format(link_util_file_path, e))
        return None
    if df is None or df.empty or 'timestamp' not in df:
        return None
    ts = pd.to_numeric(df['timestamp'], errors='coerce').dropna()
    if ts.empty:
        return None
    return int(ts.max())


def analyze_and_plot(data_file, bandwidths, per_mode_cutoff_ns=None, global_max_cutoff_ns=None):
    """
    Analyzes a link utilization file, generates a time-series plot,
    and returns the average utilization for links connected to switch 176.
    """
    print("Processing {}...".format(data_file))
    
    try:
        df = pd.read_csv(data_file, header=None, names=['timestamp', 'src', 'dst', 'cum_bytes'])
    except FileNotFoundError:
        print("File not found: {}. Skipping.".format(data_file))
        return None
    except pd.errors.EmptyDataError:
        print("File is empty: {}. Skipping.".format(data_file))
        return None

    # Optionally cap the analysis window to the last flow completion time (per mode)
    if per_mode_cutoff_ns is not None:
        original_rows = len(df)
        df = df[df['timestamp'] <= per_mode_cutoff_ns]
        print("Applied per-mode cutoff at {:.6f} s: kept {}/{} rows.".format(per_mode_cutoff_ns / 1e9, len(df), original_rows))
        if df.empty:
            print("All link-utilization samples are after the cutoff. Skipping {}.".format(data_file))
            return None

    # --- Resampling data from 1ms to 10ms ---
    df['datetime'] = pd.to_datetime(df['timestamp'], unit='ns')
    resampled_list = []
    for link_pair_key, group in df.groupby(['src', 'dst']):
        group = group.set_index('datetime')
        resampled_group = group.resample('10ms').last()
        # 前向填充，避免未来 pandas 的 chained assignment 警告
        if 'cum_bytes' in resampled_group:
            resampled_group['cum_bytes'] = resampled_group['cum_bytes'].ffill()
        # Extend each link's time series to the global max cutoff with zero throughput (by ffill cum_bytes)
        if global_max_cutoff_ns is not None:
            global_cutoff_dt = pd.to_datetime(global_max_cutoff_ns, unit='ns')
            start_dt = resampled_group.index.min()
            if pd.isna(start_dt):
                continue
            full_index = pd.date_range(start=start_dt.floor('10ms'), end=global_cutoff_dt.floor('10ms'), freq='10ms')
            resampled_group = resampled_group.reindex(full_index)
            # 确保重建索引后的索引名为 'datetime'，便于 reset_index 后得到同名列
            resampled_group.index.name = 'datetime'
            # 对补齐区间做前向填充，起始 NaN 置为 0，表示没有新增字节
            if 'cum_bytes' in resampled_group:
                resampled_group['cum_bytes'] = resampled_group['cum_bytes'].ffill().fillna(0)
        resampled_group['src'] = link_pair_key[0]
        resampled_group['dst'] = link_pair_key[1]
        resampled_list.append(resampled_group)

    if not resampled_list:
        print("No data to plot after resampling. Skipping.")
        return None
        
    df = pd.concat(resampled_list)
    # 如果索引名不是 'datetime'（例如未走全局补齐分支），这里补一下
    if df.index.name != 'datetime':
        df.index.name = 'datetime'
    df.reset_index(inplace=True)
    df['timestamp'] = df['datetime'].view('int64')
    # --- End of Resampling ---

    df['link_pair'] = df.apply(lambda row: tuple(sorted((row['src'], row['dst']))), axis=1)
    agg_df = df.groupby(['link_pair', 'timestamp'])['cum_bytes'].sum().reset_index()
    agg_df.sort_values(by=['link_pair', 'timestamp'], inplace=True)
    
    agg_df['time_diff_ns'] = agg_df.groupby('link_pair')['timestamp'].diff()
    agg_df['bytes_diff'] = agg_df.groupby('link_pair')['cum_bytes'].diff()

    agg_df.dropna(inplace=True)
    if agg_df.empty:
        print("Not enough data to calculate utilization for {}. Skipping.".format(data_file))
        return None

    agg_df['bandwidth_bps'] = agg_df.apply(lambda row: bandwidths.get(row['link_pair'], 0), axis=1)

    # Calculate bidirectional utilization
    # Utilization = (Total bits in interval) / (Interval duration * Total bidirectional bandwidth)
    # Total bidirectional bandwidth = unidirectional_bandwidth * 2
    agg_df['utilization'] = 0.0
    valid_bw = agg_df['bandwidth_bps'] > 0
    agg_df.loc[valid_bw, 'utilization'] = (agg_df.loc[valid_bw, 'bytes_diff'] * 8) / \
                                          (agg_df.loc[valid_bw, 'time_diff_ns'] * 1e-9 * (agg_df.loc[valid_bw, 'bandwidth_bps']))

    # --- Plotting Time Series ---
    routing_mode_name = os.path.basename(os.path.dirname(data_file))
    output_file = os.path.join(args.output_dir, 'link_util_timeseries_{}.png'.format(routing_mode_name))
    title = 'DCI Link Utilization for Switch 176 ({})'.format(routing_mode_name)
    
    plt.style.use('seaborn-v0_8-whitegrid')
    fig, ax = plt.subplots(1, 1, figsize=(15, 8))

    links_to_plot = agg_df[agg_df['link_pair'].apply(lambda lp: 176 in lp)]
    
    if links_to_plot.empty:
        print(f"No links connected to switch 176 found in {data_file}. Skipping plot and average calculation.")
        plt.close()
        return None

    for link_pair, group in links_to_plot.groupby('link_pair'):
        src, dst = link_pair
        label = 'Link {}-{}'.format(src, dst)
        ax.plot(group['timestamp'] / 1e9, group['utilization'] * 100, label=label, lw=1.5)

    ax.set_title(title, fontsize=18)
    ax.set_xlabel('Time (seconds)', fontsize=14)
    ax.set_ylabel('Link Utilization (%)', fontsize=14)
    ax.legend(loc='upper right', fontsize='small')
    ax.grid(True)
    ax.set_ylim(0)

    plt.tight_layout()
    print("Saving time-series plot to {}...".format(output_file))
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()

    # --- Calculate and return average utilization ---
    avg_utilization = links_to_plot.groupby('link_pair')['utilization'].mean() * 100
    return avg_utilization


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Analyze and plot link utilization from simulation outputs.')
    parser.add_argument('--base_dir', type=str, 
                        default='../simulation/mix/config/8DC-hetero-onlyDC1-8/server-output/posCor_8DC_linkUtil/link-util/0.3util', 
                        help='Base directory where the ECMP, UCMP, Ours folders are located.')
    parser.add_argument('--topology', type=str, 
                        default='../simulation/mix/config/8DC-hetero-onlyDC1-8/topology_LeafSpine_MultiDC8-posCor.txt', 
                        help='Path to the topology file.')
    parser.add_argument('--output_dir', type=str, default='server-output/posCor_8DC_linkUtil', help='Directory to save the plot images and CSV.')

    args = parser.parse_args()

    if not os.path.exists(args.output_dir):
        os.makedirs(args.output_dir)
        
    bandwidths = load_bandwidths(args.topology)
    if not bandwidths:
        print("Error: Could not load bandwidth information from topology file. Exiting.")
        exit(1)

    routing_modes = ["ECMP", "UCMP", "Ours"]
    all_avg_utilizations = {}

    # First pass: gather per-mode FCT cutoffs and link-util end times
    mode_to_data_file = {}
    mode_to_fct_cutoff = {}
    mode_to_lu_end = {}

    for mode in routing_modes:
        data_file = os.path.join(args.base_dir, mode, 'link_util_dcqcn.txt')
        fct_file = os.path.join(args.base_dir, mode, 'fct_dcqcn.txt')
        mode_to_data_file[mode] = data_file
        mode_to_fct_cutoff[mode] = load_cutoff_from_fct(fct_file)
        mode_to_lu_end[mode] = get_last_timestamp_ns(data_file)

    # Determine global comparison window end
    available_fct_cutoffs = [v for v in mode_to_fct_cutoff.values() if v is not None]
    if available_fct_cutoffs:
        global_max_cutoff_ns = max(available_fct_cutoffs)
        print("Global comparison end (from FCT max completion): {:.6f} s ({} ns)".format(global_max_cutoff_ns / 1e9, global_max_cutoff_ns))
    else:
        available_lu_ends = [v for v in mode_to_lu_end.values() if v is not None]
        global_max_cutoff_ns = max(available_lu_ends) if available_lu_ends else None
        if global_max_cutoff_ns is not None:
            print("Global comparison end (from link-util max timestamp): {:.6f} s ({} ns)".format(global_max_cutoff_ns / 1e9, global_max_cutoff_ns))

    # Second pass: analyze per mode using its own cutoff and extend to global window
    for mode in routing_modes:
        data_file = mode_to_data_file[mode]
        per_mode_cutoff_ns = mode_to_fct_cutoff.get(mode) or mode_to_lu_end.get(mode) or global_max_cutoff_ns
        avg_util = analyze_and_plot(
            data_file,
            bandwidths,
            per_mode_cutoff_ns=per_mode_cutoff_ns,
            global_max_cutoff_ns=global_max_cutoff_ns
        )
        if avg_util is not None:
            all_avg_utilizations[mode] = avg_util

    if not all_avg_utilizations:
        print("\nNo average utilization data was collected. Cannot generate summary CSV or bar chart.")
        exit()

    # --- Create a summary DataFrame and save to CSV ---
    summary_df = pd.DataFrame(all_avg_utilizations)
    summary_df.index.name = 'Link'
    summary_df.index = summary_df.index.map(lambda x: f'{x[0]}-{x[1]}')
    
    csv_output_path = os.path.join(args.output_dir, 'average_link_utilization_summary.csv')
    print(f"\nSaving average utilization summary to {csv_output_path}...")
    summary_df.to_csv(csv_output_path)
    print("--- Average Utilization Summary ---")
    print(summary_df)
    print("---------------------------------")

    # --- Create and save the comparison bar chart ---
    bar_chart_output_path = os.path.join(args.output_dir, 'average_link_utilization_comparison.png')
    print(f"\nSaving comparison bar chart to {bar_chart_output_path}...")
    
    plt.style.use('seaborn-v0_8-whitegrid')
    ax = summary_df.plot(kind='bar', figsize=(16, 9), width=0.8, edgecolor='black')
    
    ax.set_title('Average DCI Link Utilization Comparison (Switch 176)', fontsize=18, pad=20)
    ax.set_xlabel('DCI Link', fontsize=14)
    ax.set_ylabel('Average Utilization (%)', fontsize=14)
    ax.tick_params(axis='x', rotation=45, labelsize=12)
    ax.tick_params(axis='y', labelsize=12)
    ax.legend(title='Routing Mode', fontsize=12)
    ax.grid(axis='y', linestyle='--', alpha=0.7)
    
    # Add value labels on top of each bar
    for container in ax.containers:
        ax.bar_label(container, fmt='%.2f', fontsize=10, padding=3)
    
    ax.set_ylim(0, max(ax.get_ylim()) * 1.1) # Add some padding to the top
    
    plt.tight_layout()
    plt.savefig(bar_chart_output_path, dpi=300)
    plt.close()

    print("\nAnalysis complete.")

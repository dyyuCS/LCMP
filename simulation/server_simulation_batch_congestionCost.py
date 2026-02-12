# -*- coding: utf-8 -*-
import os
import re
import argparse


def modify_config(config_path, output_dir_base, topology, w_ql, w_tl, w_dp):
    with open(config_path, 'r') as f:
        lines = f.readlines()

    # config_path的父目录
    base_dir = os.path.dirname(config_path)
    
    # 从topology文件名中提取posCor或negCor
    topology_variant = "posCor" if "posCor" in topology else "negCor"

    params = {
        # Experiment parameters
        'W_QL': w_ql,
        'W_TL': w_tl,
        'W_DP': w_dp,
        # Default parameters
        'W_DL': 3, 'W_BW': 1, 'S_STATIC': 2,
        'S_CONG': 2,
        'ALPHA': 3, 'BETA': 1, 'S_TOTAL': 2,
        # General config
        'ROUTING_MODE': 2,
        'CC_MODE': 1,
        'OUTPUT_DIR': '{}/Figure11/{}/output-8DC-hetero-onlyDC1-8-{}-DCQCN/w_ql={}-w_tl={}-w_dp={}/'.format(base_dir, output_dir_base, topology_variant, w_ql, w_tl, w_dp),
        'FLOW_FILE': '${WORKING_DIR}traffic_WebSearch_8DC_forDC1And8-0.3util.txt',
        'TOPOLOGY_FILE': '${{WORKING_DIR}}{}.txt'.format(topology),
        'WORKING_DIR': '{}/'.format(base_dir),
    }

    new_lines = []
    updated_keys = set()

    for line in lines:
        parts = line.strip().split()
        key = parts[0] if parts else ''
        
        if key in params:
            new_lines.append('{} {}\n'.format(key, params[key]))
            updated_keys.add(key)
        else:
            new_lines.append(line)
    
    # Add any parameters that were not in the original file at all
    for key in params:
        if key not in updated_keys:
            new_lines.append('{} {}\n'.format(key, params[key]))

    with open(config_path, 'w') as f:
        f.writelines(new_lines)

def run_simulation(config_path):
    cmd = "./waf --run 'scratch/third {}'".format(config_path)
    print("Running: {}".format(cmd))
    os.system(cmd)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='')
    parser.add_argument('-o', dest='output', action='store', default='server-output/congestion-cost', help="output file base directory")
    args = parser.parse_args()

    output_dir_base = args.output
    CONFIG_PATH = 'mix/config/8DC-hetero-onlyDC1-8/config_batch.txt'
    
    TOPOLOGIES = ['topology_LeafSpine_MultiDC8']
    PARAM_CONFIGS = [{'w_ql': 2, 'w_tl': 1, 'w_dp': 1}, 
                     {'w_ql': 1, 'w_tl': 2, 'w_dp': 1}, 
                     {'w_ql': 1, 'w_tl': 1, 'w_dp': 2}]

    for topology in TOPOLOGIES:
        for params in PARAM_CONFIGS:
            w_ql = params['w_ql']
            w_tl = params['w_tl']
            w_dp = params['w_dp']
            print("--- Starting simulation for {}, w_ql={}, w_tl={}, w_dp={} ---".format(topology, w_ql, w_tl, w_dp))
            modify_config(CONFIG_PATH, output_dir_base, topology, w_ql, w_tl, w_dp)
            run_simulation(CONFIG_PATH)
            print("--- Finished simulation for {}, w_ql={}, w_tl={}, w_dp={} ---\n".format(topology, w_ql, w_tl, w_dp))

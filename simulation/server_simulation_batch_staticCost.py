# -*- coding: utf-8 -*-
import os
import re
import argparse


def modify_config(config_path, output_dir_base, topology, w_dl, w_bw):
    with open(config_path, 'r') as f:
        lines = f.readlines()

    # config_path的父目录
    base_dir = os.path.dirname(config_path)
    
    # 从topology文件名中提取posCor或negCor
    topology_variant = "posCor" if "posCor" in topology else "negCor"

    params = {
        # Experiment parameters
        'W_DL': w_dl,
        'W_BW': w_bw,
        # Default parameters
        'S_STATIC': 2,
        'W_QL': 2, 'W_TL': 1, 'W_DP': 1, 'S_CONG': 2,
        'ALPHA': 3, 'BETA': 1, 'S_TOTAL': 2,
        # General config
        'ROUTING_MODE': 2,
        'CC_MODE': 1,
        'OUTPUT_DIR': '{}/{}/output-8DC-hetero-onlyDC1-8-{}-DCQCN/w_dl={}-w_bw={}/'.format(base_dir, output_dir_base, topology_variant, w_dl, w_bw),
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
    parser.add_argument('-o', dest='output', action='store', default='server-output/static-cost', help="output file base directory")
    args = parser.parse_args()

    output_dir_base = args.output
    CONFIG_PATH = 'mix/config/8DC-hetero-onlyDC1-8/config_batch.txt'
    
    # TOPOLOGIES = ['topology_LeafSpine_MultiDC8-posCor']
    TOPOLOGIES = ['topology_LeafSpine_MultiDC8-posCor', 'topology_LeafSpine_MultiDC8-negCor']
    PARAM_CONFIGS = [{'w_dl': 3, 'w_bw': 1}, {'w_dl': 2, 'w_bw': 2}, {'w_dl': 1, 'w_bw': 3}]
    # PARAM_CONFIGS = [{'w_dl': 1, 'w_bw': 3}]

    for topology in TOPOLOGIES:
        for params in PARAM_CONFIGS:
            w_dl = params['w_dl']
            w_bw = params['w_bw']
            print("--- Starting simulation for {}, w_dl={}, w_bw={} ---".format(topology, w_dl, w_bw))
            modify_config(CONFIG_PATH, output_dir_base, topology, w_dl, w_bw)
            run_simulation(CONFIG_PATH)
            print("--- Finished simulation for {}, w_dl={}, w_bw={} ---\n".format(topology, w_dl, w_bw))

# -*- coding: utf-8 -*-
import os
import re
import argparse


def modify_config(config_path, output_dir_base, topology, alpha, beta):
    with open(config_path, 'r') as f:
        lines = f.readlines()

    # config_path的父目录
    base_dir = os.path.dirname(CONFIG_PATH)
    
    # 从topology文件名中提取posCor或negCor
    topology_variant = "posCor" if "posCor" in topology else "negCor"

    params = {
        # Experiment parameters
        'ALPHA': alpha,
        'BETA': beta,
        # Default parameters
        'W_DL': 3, 'W_BW': 1, 'S_STATIC': 2,
        'W_QL': 2, 'W_TL': 1, 'W_DP': 1, 'S_CONG': 2,
        'S_TOTAL': 2,
        # General config
        'ROUTING_MODE': 2,
        'CC_MODE': 1,
        'OUTPUT_DIR': '{}/{}/output-8DC-hetero-onlyDC1-8-{}-DCQCN/alpha={}-beta={}/'.format(base_dir, output_dir_base, topology_variant, alpha, beta),
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
    parser.add_argument('-o', dest='output', action='store', default='server-output/ablation-study', help="output file base directory")
    args = parser.parse_args()

    output_dir_base = args.output
    CONFIG_PATH = 'mix/config/8DC-hetero-onlyDC1-8/config_batch.txt'
    
    TOPOLOGIES = ['topology_LeafSpine_MultiDC8-negCor' ]
                #    , 'topology_LeafSpine_MultiDC8-posCor']
    # PARAM_CONFIGS = [{'alpha': 0, 'beta': 4} , {'alpha': 4, 'beta': 0}]
    PARAM_CONFIGS = [{'alpha': 4, 'beta': 0}]      

    for topology in TOPOLOGIES:
        for params in PARAM_CONFIGS:
            alpha = params['alpha']
            beta = params['beta']
            print("--- Starting simulation for {}, alpha={}, beta={} ---".format(topology, alpha, beta))
            modify_config(CONFIG_PATH, output_dir_base, topology, alpha, beta)
            run_simulation(CONFIG_PATH)
            print("--- Finished simulation for {}, alpha={}, beta={} ---\n".format(topology, alpha, beta))

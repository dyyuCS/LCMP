# -*- coding: utf-8 -*-
import os
import re
import argparse


def modify_config(config_path, output_dir, routing_mode, x_util, cc_mode, dataset):
    with open(config_path, 'r') as f:
        lines = f.readlines()

    # 获取CONFIG_PATH的前半部分（去掉最后一个斜杠后的内容）
    base_dir = os.path.dirname(CONFIG_PATH)
    new_lines = []
    for line in lines:
        if line.strip().startswith('ROUTING_MODE'):
            new_lines.append('ROUTING_MODE {}\n'.format(routing_mode))
        elif line.strip().startswith('CC_MODE'):
            new_lines.append('CC_MODE {}\n'.format(cc_mode))
        elif line.strip().startswith('OUTPUT_DIR'):
            if routing_mode == '0':
                new_dir = '{}/{}/{}/{}/ECMP/'.format(base_dir, output_dir, dataset, x_util)
            elif routing_mode == '1':
                new_dir = '{}/{}/{}/{}/UCMP/'.format(base_dir, output_dir, dataset, x_util)
            elif routing_mode == '2':
                new_dir = '{}/{}/{}/{}/Ours/'.format(base_dir, output_dir, dataset, x_util)
            new_lines.append('OUTPUT_DIR {}\n'.format(new_dir))
        elif line.strip().startswith('FLOW_FILE'):
            new_lines.append('FLOW_FILE ${WORKING_DIR}'+f'traffic_{dataset}_8DC_forDC1And8-{x_util}.txt\n')
        elif line.strip().startswith('TOPOLOGY_FILE'):
            new_lines.append('TOPOLOGY_FILE ${WORKING_DIR}'+f'topology_LeafSpine_MultiDC8-posCor.txt\n')
        elif line.strip().startswith('WORKING_DIR'):
            new_lines.append('WORKING_DIR {}/\n'.format(base_dir))
        else:
            new_lines.append(line)

    with open(config_path, 'w') as f:
        f.writelines(new_lines)

def run_simulation(config_path):
    cmd = "./waf --run 'scratch/third {}'".format(config_path)
    print("Running: {}".format(cmd))
    os.system(cmd)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='')
    parser.add_argument('-o', dest='output', action='store', default='server-output', help="output file")
    args = parser.parse_args()

    output_dir = args.output
    CONFIG_PATH = 'mix/config/8DC-hetero-onlyDC1-8/config_batch.txt'
    UTIL_LIST = ['0.3util']
    # UTIL_LIST = ['0.3util', '0.5util', '0.8util']

    # DATASET = ['AliStorage', 'GoogleRPC']
    DATASET = ['WebSearch']

    # for routing_mode in ['2']:  # 0: ECMP, 1: UCMP, 2: Ours
    # for routing_mode in ['0', '2']:  # 0: ECMP, 1: UCMP, 2: Ours
    # for routing_mode in ['0', '1', '2']:  # 0: ECMP, 1: UCMP, 2: Ours
        for x_util in UTIL_LIST:
            for cc_mode in ['3', '7', '8']: # CC_MODE 1: DCQCN, 3: HPCC, 7: TIMELY, 8: DCTCP, 10: HPCC-PINT}
            # for cc_mode in [ '1']: # CC_MODE 1: DCQCN, 3: HPCC, 7: TIMELY, 8: DCTCP, 10: HPCC-PINT}
                for dataset in DATASET:
                    modify_config(CONFIG_PATH, output_dir, routing_mode, x_util, cc_mode, dataset)
                    run_simulation(CONFIG_PATH)
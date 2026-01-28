# -*- coding: utf-8 -*-
import os
import re

CONFIG_PATH = 'mix/config/8DC-hetero-onlyDC1-8/config_debug.txt'
UTIL_LIST = ['0.3util']
# UTIL_LIST = ['0.3util', '0.5util', '0.8util']

def modify_config(config_path, routing_mode, x_util):
    with open(config_path, 'r') as f:
        lines = f.readlines()

    # 获取CONFIG_PATH的前半部分（去掉最后一个斜杠后的内容）
    base_dir = os.path.dirname(CONFIG_PATH)

    new_lines = []
    for line in lines:
        if line.strip().startswith('ROUTING_MODE'):
            new_lines.append('ROUTING_MODE {}\n'.format(routing_mode))
        elif line.strip().startswith('OUTPUT_DIR'):
            if routing_mode == '0':
                # new_dir = '{}/debugOutput/{}/ECMP/'.format(base_dir, x_util)
                new_dir = '{}/debugOutput-sameOrderBwDelay/{}/ECMP/'.format(base_dir, x_util)
            elif routing_mode == '1':
                # new_dir = '{}/debugOutput/{}/UCMP/'.format(base_dir, x_util)
                new_dir = '{}/debugOutput-sameOrderBwDelay/{}/UCMP/'.format(base_dir, x_util)
            elif routing_mode == '2':
                # new_dir = '{}/debugOutput/{}/Ours/'.format(base_dir, x_util)
                new_dir = '{}/debugOutput-sameOrderBwDelay/{}/Ours/'.format(base_dir, x_util)
            new_lines.append('OUTPUT_DIR {}\n'.format(new_dir))
        elif line.strip().startswith('FLOW_FILE'):
            new_lines.append('FLOW_FILE ${WORKING_DIR}traffic_MultiDC_forDC1And8-0.3util_for_test.txt\n')
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
    for routing_mode in ['2']: # 0: ECMP, 1: UCMP, 2: Ours
    # for routing_mode in ['0', '2']: # 0: ECMP, 1: UCMP, 2: Ours
    # for routing_mode in ['0', '1', '2']: # 0: ECMP, 1: UCMP, 2: Ours
        for x_util in UTIL_LIST:
            modify_config(CONFIG_PATH, routing_mode, x_util)
            run_simulation(CONFIG_PATH)
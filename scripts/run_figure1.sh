#!/bin/bash
# Figure 1: Motivation Experiment (8DC Link Utilization)

set -e  # Exit on error

echo "=========================================="
echo "Running Figure 1: Motivation Experiment"
echo "=========================================="

# Step 1: Run simulation
echo "[1/3] Running simulation..."
cd ../simulation
python3 server_simulation_batch_8DC_linkUtil.py \
    -o "mix/config/8DC-hetero/server-output/Figure1-posCor_8DC_original_flow_linkUtil"

# Step 2: Analyze FCT
echo "[2/3] Analyzing FCT..."
cd ../analysis
python3 fct_analysis_py3_batch.py \
    -i "../simulation/mix/config/8DC-hetero/server-output/Figure1-posCor_8DC_original_flow_linkUtil" \
    -o "server-output/Figure1-8DC_linkUtil"

# Step 3: Plot link utilization
echo "[3/3] Plotting link utilization..."
python3 plot_link_utilization.py \
    --base_dir '../simulation/mix/config/8DC-hetero/server-output/Figure1-posCor_8DC_original_flow_linkUtil/link-util/0.3util' \
    --topology '../simulation/mix/config/8DC-hetero/topology_LeafSpine_MultiDC8.txt' \
    --output_dir 'server-output/Figure1-8DC_linkUtil'

echo "=========================================="
echo "Figure 1 completed successfully!"
echo "Results saved in: analysis/server-output/Figure1-8DC_linkUtil"
echo "=========================================="


#!/bin/bash
# Figure 1: Motivation Experiment (8DC Link Utilization)

set -e  # Exit on error

echo "=========================================="
echo "Running Figure 1: Motivation Experiment"
echo "=========================================="

# Step 1: Run simulation
echo "[1/5] Running simulation..."
cd ../simulation
python3 server_simulation_batch_8DC_linkUtil.py \
    -o "server-output/Figure1-8DC_linkUtil/link-util"

# Step 2: Analyze FCT
echo "[2/5] Analyzing FCT..."
cd ../analysis
python3 fct_analysis_py3_batch_linkutil.py \
    -i "../simulation/mix/config/8DC-hetero/server-output/Figure1-8DC_linkUtil/link-util" \
    -o "server-output/Figure1-8DC_linkUtil"

# Step 3: Merge results
echo "[3/5] Merging results..."
python3 merge_fct_results.py \
    -i "server-output/Figure1-8DC_linkUtil" \
    -o "server-output/Figure1-8DC_linkUtil"

# Step 4: Plot FCT slowdown
echo "[4/5] Plotting FCT slowdown..."
python3 plot_fct_slowdown.py -m single -i server-output/Figure1-8DC_linkUtil/merged_0.3util-FCTslowdown.csv

# Step 5: Plot link utilization
echo "[5/5] Plotting link utilization..."
python3 plot_link_utilization.py \
    --base_dir '../simulation/mix/config/8DC-hetero/server-output/Figure1-8DC_linkUtil/link-util/0.3util' \
    --topology '../simulation/mix/config/8DC-hetero/topology_LeafSpine_MultiDC8.txt' \
    --output_dir 'server-output/Figure1-8DC_linkUtil'

echo "=========================================="
echo "Figure 1 completed successfully!"
echo "Results saved in: analysis/server-output/Figure1-8DC_linkUtil"
echo "=========================================="

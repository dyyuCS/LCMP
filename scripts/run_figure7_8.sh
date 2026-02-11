#!/bin/bash
# Figure 7 & 8: Routing and Traffic Load Comparison (13DC)

set -e  # Exit on error

echo "=========================================="
echo "Running Figure 7 & 8: Routing and Traffic Load Comparison (13DC)"
echo "=========================================="

# Step 1: Run simulation
echo "[1/4] Running simulation..."
cd ../simulation
python3 server_simulation_batch_for13DC.py \
    -o "mix/config/13DC-hetero/server-output/Figure-7and8-13DC_3routing_3traffic"

# Step 2: Analyze FCT
echo "[2/4] Analyzing FCT..."
cd ../analysis
python3 fct_analysis_py3_batch.py \
    -i "../simulation/mix/config/13DC-hetero/server-output/Figure-7and8-13DC_3routing_3traffic" \
    -o "server-output/Figure-7and8-13DC_3routing_3traffic"

# Step 3: Merge results
echo "[3/4] Merging results..."
python3 merge_fct_results.py \
    -i "server-output/Figure-7and8-13DC_3routing_3traffic" \
    -o "server-output/Figure-7and8-13DC_3routing_3traffic"

# Step 4: Plot FCT slowdown
echo "[4/4] Plotting FCT slowdown..."
python3 plot_fct_slowdown.py -m single -i server-output/Figure7and8-13DC_3routing_3traffic/0.3util/WebSearch_dcqcn_0.3util-FCTslowdown.csv
python3 plot_fct_slowdown.py -m single -i server-output/Figure7and8-13DC_3routing_3traffic/0.5util/WebSearch_dcqcn_0.5util-FCTslowdown.csv
python3 plot_fct_slowdown.py -m single -i server-output/Figure7and8-13DC_3routing_3traffic/0.8util/WebSearch_dcqcn_0.8util-FCTslowdown.csv
python3 plot_fct_slowdown.py -m single -i server-output/Figure7and8-13DC_3routing_3traffic/0.3util/WebSearch_dcqcn_0.3util-FCTslowdown-DC1-DC13.csv
python3 plot_fct_slowdown.py -m single -i server-output/Figure7and8-13DC_3routing_3traffic/0.5util/WebSearch_dcqcn_0.5util-FCTslowdown-DC1-DC13.csv
python3 plot_fct_slowdown.py -m single -i server-output/Figure7and8-13DC_3routing_3traffic/0.8util/WebSearch_dcqcn_0.8util-FCTslowdown-DC1-DC13.csv

echo "=========================================="
echo "Figure 7 & 8 completed successfully!"
echo "Results saved in: analysis/server-output/Figure-7and8-13DC_3routing_3traffic"
echo "=========================================="


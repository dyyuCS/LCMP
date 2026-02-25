#!/bin/bash
# Figure 10: Different Congestion Control Comparison (Robustness)

set -e  # Exit on error

echo "=========================================="
echo "Running Figure 10: Different Congestion Control Comparison"
echo "=========================================="

# Step 1: Run simulation
echo "[1/3] Running simulation..."
cd ../simulation
python3 server_simulation_batch_8DC_differCc.py \
    -o "server-output/Figure10-8DC_differCC"

# Step 2: Analyze FCT
echo "[2/3] Analyzing FCT and Merging results..."
cd ../analysis
python3 fct_analysis_py3_batch_differCC.py \
    -i "../simulation/mix/config/8DC-hetero/server-output/Figure10-8DC_differCC/WebSearch" \
    -o "server-output/Figure10-8DC_differCC"


# Step 3: Plot FCT slowdown
echo "[3/3] Plotting FCT slowdown..."
python3 plot_fct_slowdown.py -d server-output/Figure10-8DC_differCC/0.3util

echo "=========================================="
echo "Figure 10 completed successfully!"
echo "Results saved in: analysis/server-output/Figure10-8DC_differCC"
echo "=========================================="

#!/bin/bash
# Figure 10: Different Congestion Control Comparison (Robustness)

set -e  # Exit on error

echo "=========================================="
echo "Running Figure 10: Different Congestion Control Comparison"
echo "=========================================="

# Step 1: Run simulation
echo "[1/4] Running simulation..."
cd ../simulation
python3 server_simulation_batch_8DC_differCc.py \
    -o "mix/config/8DC-hetero/server-output/Figure10-8DC-differCC"

# Step 2: Analyze FCT
echo "[2/4] Analyzing FCT..."
cd ../analysis
python3 fct_analysis_py3_batch.py \
    -i "../simulation/mix/config/8DC-hetero/server-output/Figure10-8DC-differCC" \
    -o "server-output/Figure10-8DC-differCC"

# Step 3: Merge results
echo "[3/4] Merging results..."
python3 merge_fct_results.py \
    -i "server-output/Figure10-8DC-differCC" \
    -o "server-output/Figure10-8DC-differCC"

# Step 4: Plot FCT slowdown
echo "[4/4] Plotting FCT slowdown..."
python3 plot_fct_slowdown.py -d server-output/Figure10-8DC-differCC/0.3util

echo "=========================================="
echo "Figure 10 completed successfully!"
echo "Results saved in: analysis/server-output/Figure10-8DC-differCC"
echo "=========================================="


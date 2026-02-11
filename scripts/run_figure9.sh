#!/bin/bash
# Figure 9: Different Traffic Datasets Comparison (Robustness)

set -e  # Exit on error

echo "=========================================="
echo "Running Figure 9: Different Traffic Datasets Comparison"
echo "=========================================="

# Step 1: Run simulation
echo "[1/4] Running simulation..."
cd ../simulation
python3 server_simulation_batch_8DC_differDataset.py \
    -o "mix/config/8DC-hetero/server-output/Figure9-8DC_differDataset"

# Step 2: Analyze FCT
echo "[2/4] Analyzing FCT..."
cd ../analysis
python3 fct_analysis_py3_batch.py \
    -i "../simulation/mix/config/8DC-hetero/server-output/Figure9-8DC_differDataset" \
    -o "server-output/Figure9-8DC_differDataset"

# Step 3: Merge results
echo "[3/4] Merging results..."
python3 merge_fct_results.py \
    -i "server-output/Figure9-8DC_differDataset" \
    -o "server-output/Figure9-8DC_differDataset"

# Step 4: Plot FCT slowdown
echo "[4/4] Plotting FCT slowdown..."
python3 plot_fct_slowdown.py -d server-output/Figure9-8DC_differDataset/0.3util

echo "=========================================="
echo "Figure 9 completed successfully!"
echo "Results saved in: analysis/server-output/Figure9-8DC_differDataset"
echo "=========================================="


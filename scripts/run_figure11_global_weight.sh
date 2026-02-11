#!/bin/bash
# Figure 11: Global Weight Component Tests

set -e  # Exit on error

echo "=========================================="
echo "Running Figure 11: Global Weight Component Tests"
echo "=========================================="

# Step 1: Run simulation
echo "[1/4] Running simulation..."
cd ../simulation
python3 server_simulation_batch_globalWeight.py \
    -o "mix/config/8DC-hetero/server-output/Figure11/global-weight"

# Step 2: Analyze FCT
echo "[2/4] Analyzing FCT..."
cd ../analysis
python3 fct_analysis_py3_batch.py \
    -i "../simulation/mix/config/8DC-hetero/server-output/Figure11/global-weight" \
    -o "server-output/Figure11/global-weight"

# Step 3: Merge results
echo "[3/4] Merging results..."
python3 merge_fct_results.py \
    -i "server-output/Figure11/global-weight" \
    -o "server-output/Figure11/global-weight"

# Step 4: Plot FCT slowdown
echo "[4/4] Plotting FCT slowdown..."
python3 plot_fct_slowdown.py -d server-output/Figure11/global-weight

echo "=========================================="
echo "Figure 11 (Global Weight) completed successfully!"
echo "Results saved in: analysis/server-output/Figure11/global-weight"
echo "=========================================="


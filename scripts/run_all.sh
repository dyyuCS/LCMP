#!/bin/bash
# Run all experiments

set -e  # Exit on error

echo "=========================================="
echo "Running ALL Experiments"
echo "=========================================="

# Small-Scale (8 datacenters) Tests
echo ""
echo "Starting Small-Scale (8DC) Experiments..."
echo ""

bash run_figure1.sh
bash run_figure5.sh
bash run_figure9.sh
bash run_figure10.sh
bash run_figure11_ablation.sh
bash run_figure11_path_cost.sh
bash run_figure11_congestion_cost.sh
bash run_figure11_global_weight.sh

# Large-Scale (13 datacenters) Experiments
echo ""
echo "Starting Large-Scale (13DC) Experiments..."
echo ""

bash run_figure7_8.sh

echo ""
echo "=========================================="
echo "ALL Experiments completed successfully!"
echo "=========================================="
echo ""
echo "Results are saved in: analysis/server-output/"
echo ""
echo "Summary of completed experiments:"
echo "  - Figure 1: Motivation Experiment"
echo "  - Figure 5: Routing and Traffic Load Comparison (8DC)"
echo "  - Figure 7 & 8: Routing and Traffic Load Comparison (13DC)"
echo "  - Figure 9: Different Traffic Datasets Comparison"
echo "  - Figure 10: Different Congestion Control Comparison"
echo "  - Figure 11: Ablation Study & Component Tests"
echo "=========================================="


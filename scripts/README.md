# Scripts Folder

This folder contains automated shell scripts for running complete experiment workflows, including simulation, analysis, and visualization for all figures in the paper.

## Overview

Each script automates the entire pipeline for a specific experiment:
1. **Simulation**: Run NS-3 network simulations
2. **FCT Analysis**: Process flow completion time logs
3. **Result Aggregation**: Merge and organize results
4. **Visualization**: Generate comparison plots

## Available Scripts

### Small-Scale (8 Datacenters) Experiments

- **`run_figure1.sh`** - Motivation Experiment
  - Demonstrates link utilization patterns
  - Includes link utilization visualization
  - Output: `analysis/server-output/Figure1-8DC_linkUtil`

- **`run_figure5.sh`** - Routing and Traffic Load Comparison
  - Compares ECMP, UCMP, and LCMP under different traffic loads (0.3, 0.5, 0.8 util)
  - Output: `analysis/server-output/Figure5-8DC_3routing_3traffic`

- **`run_figure9.sh`** - Different Traffic Datasets Comparison (Robustness)
  - Tests robustness across WebSearch, AliStorage, and GoogleRPC datasets
  - Output: `analysis/server-output/Figure9-8DC_differDataset`

- **`run_figure10.sh`** - Different Congestion Control Comparison (Robustness)
  - Tests with DCQCN, DCTCP, HPCC, and Timely
  - Output: `analysis/server-output/Figure10-8DC-differCC`

- **`run_figure11_ablation.sh`** - Ablation Study
  - Evaluates impact of different LCMP components
  - Output: `analysis/server-output/Figure11/ablation-study`

- **`run_figure11_path_cost.sh`** - Path Cost Component Tests
  - Analyzes static path cost function
  - Output: `analysis/server-output/Figure11/path-cost`

- **`run_figure11_congestion_cost.sh`** - Congestion Cost Component Tests
  - Analyzes dynamic congestion cost function
  - Output: `analysis/server-output/Figure11/congestion-cost`

- **`run_figure11_global_weight.sh`** - Global Weight Component Tests
  - Analyzes global weight parameter effects
  - Output: `analysis/server-output/Figure11/global-weight`

### Large-Scale (13 Datacenters) Experiments

- **`run_figure7_8.sh`** - Routing and Traffic Load Comparison (13DC)
  - Large-scale evaluation of routing algorithms
  - Tests both all-DC and DC1-DC13 specific flows
  - Output: `analysis/server-output/Figure-7and8-13DC_3routing_3traffic`

### Master Script

- **`run_all.sh`** - Run All Experiments
  - Executes all experiments sequentially
  - Provides progress summary
  - Recommended for complete reproduction of paper results




# Simulation Folder

This folder contains an NS-3-based network simulator for the study and evaluation of datacenter network load balancing schemes, especially focusing on the implementation and comparison of ECMP, UCMP, and LCMP (our proposed method) in `dci-switch-node.cc`.

## Main Focus

- **Purpose**: To simulate and analyze different load balancing algorithms (Equal-Cost Multi-Path - ECMP, Unequal-Cost Multi-Path - UCMP, and LCMP - LCMP/Ours) for datacenter switching fabrics.
- **Key Switch Logic**: All core routing logic is implemented in `src/point-to-point/model/dci-switch-node.cc`, where you can find clear implementation and configurability of the three major schemes.
- **Supported Devices**: The simulator also supports Broadcom shared buffer switch architectures to better replicate real-world datacenter hardware.

## Load Balancing Schemes

- **ECMP (Equal-Cost Multi-Path)**: Default multi-path routing; hashes flows across all available paths with equal cost. Simple and widely used, but may cause imbalanced throughput with uneven path utilization.
- **UCMP (Unequal-Cost Multi-Path)**: Considers path link capacities to distribute flows non-uniformly proportionate to available bandwidth.
- **LCMP/Ours (Long-Haul Cost-Aware Multi-Path)**: A novel scheme proposed in this branch, which dynamically considers path delay, bandwidth, queue/congestion states, and other real-time metrics to intelligently route flows for maximized utilization and minimized congestion.
- **Switching the Algorithm**: Schemes can typically be controlled via the `RoutingMode` attribute in the DCI switch configuration:
  - 0: ECMP
  - 1: UCMP
  - 2: LCMP (Ours)
  Modifying this attribute in scripts or configuration files allows you to easily compare the schemes.

## Quick Start

### 1. Build
```bash
./waf configure
```

### 2. Configuring Experiments
- See `mix/config_doc.txt` for example experimental configurations describing topologies and simulation parameters.
- Topology examples: `mix/8DC-hetero/topology_LeafSpine_MultiDC8-posCor.txt` and others.
- Use the Python automation scripts (such as `server_simulation_batch_8DC.py`) for batch generation and execution of test cases.

### 3. Running Simulations

Due to the large number of experimental cases and the need for reproducibility, we recommend using the provided automation scripts for batch simulation runs. These scripts will automatically loop over configuration sets, routing schemes, or experimental parameters as needed.

- **Automated (Recommended):**
  ```bash
  # For example:
  python3 server_simulation_batch_8DC.py
  ```
- **Manual Run (advanced or for debugging only):**
  You may run a specific experiment directly using NS-3:
  ```bash
  # for example:
  ./waf --run 'scratch/third mix/8DC-hetero/config_batch.txt'
  ```

#### Experiment Types in the Paper

Experiments in the paper are organized into two categories:

**Small-Scale (8 datacenters) Tests**
- Typical mini testbeds (e.g., 8 datacenters) for benchmarking routing and cost-function mechanisms. Use these scripts for rapid iteration and core algorithm validation:
  - Motivation Experiment: 
    ```bash
    python3 server_simulation_batch_8DC_linkUtil.py 
    -o "server-output/Figure1-posCor_8DC_original_flow_linkUtil"
    ```
  - Routing and Traffic Load Comparison:
    ```bash
    python3 server_simulation_batch_8DC.py 
    -o "server-output/Figure5-posCor_8DC_3routing_3traffic"
    ```
  - Different Traffic Datasets Comparison(Robustness Analysis):
    ```bash
    python3 server_simulation_batch_8DC_differDataset.py 
    -o "server-output/Figure9-posCor_8DC_differDataset"
    ```
  - Different Congestion Control Comparison(Robustness Analysis):
    ```bash
    python3 server_simulation_batch_8DC_differCc.py
    -o "server-output/Figure10-output-8DC-hetero-posCor-differCC"
    ```
  - Ablation Study:
    ```bash
    python3 server_simulation_batch_ablationStudy.py
    -o "server-output/Figure11/ablation-study"
    ```
  - Path Cost Tests (Cost Function Component Analysis):
    ```bash
    python3 server_simulation_batch_staticCost.py
    -o "server-output/Figure11/static-cost"
    ```
  - Congestion Cost Tests (Cost Function Component Analysis):
    ```bash
    python3 server_simulation_batch_congestionCost.py
    -o "server-output/Figure11/congestion-cost"
    ```
  - Global Weight Tests (Cost Function Component Analysis):
    ```bash
    python3 server_simulation_batch_globalWeight.py
    -o "server-output/Figure11/global-weight"
    ```    
  - All scripts support `-h` to display configurable options and help.

**Large-Scale (13 datacenters) Experiments**
- For scaling up to real, more complex topologies and mega-data center scenarios, and for stress-testing LCMP in even broader environments. 
  - Routing and Traffic Load Comparison:
    ```bash
    python3 server_simulation_batch_for13DC.py
    -o "server-output/Figure-7&8-13DC_3routing_3traffic"
    ```

## Notes
- See code comments in `dci-switch-node.cc` for in-depth logic and customizability.
- Further attributes for tuning cost computation (weights, thresholds, etc.) are exposed as NS-3 attributes in the DCI switch implementation.
- The simulator may evolve as the research advances.

For more information, examine the annotated code in the core model and the experiment results generated by the analysis scripts.

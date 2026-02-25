# LCMP: Distributed Long-Haul Cost-Aware Multi-Path Routing for Inter-Datacenter RDMA Networks

This repository contains the code, scripts, and data for the paper:

> **"LCMP: Distributed Long-Haul Cost-Aware Multi-Path Routing for Inter-Datacenter RDMA Networks"**  
> _EuroSys 2026 (accepted)_


## Overview

Modern datacenter fabrics rely on multipath routing to maximize resource utilization, yet classic ECMP and even bandwidth-aware UCMP fundamentally ignore real-time congestion, leading to persistent imbalance and degraded tail latency. **LCMP** (Long-Haul Cost-Aware Multi-Path) is a novel, practical datacenter multipathing design that dynamically selects paths based on both path (delay, bandwidth) and congestion costs (queue, persistence, trend), optimizing for both throughput and flow completion. Our framework is fully implemented atop NS-3 and evaluated using large-scale, realistic topologies and traffic.

Key contributions:
- A cost function that adaptively balances path and congestion signals, enabling granular differentiation even among seemingly "equal" paths
- A DCI-switch-based simulation platform supporting rapid prototyping and fair comparison among ECMP, UCMP, and LCMP
- Exhaustive analysis and visualization scripts to reproduce major results in the paper

We describe how to run this repository using your local machine with `ubuntu:22.04`. 

## Environment Setup

This project requires a legacy C++ toolchain (gcc/g++ 5.x), older system libraries, and Python 2.x to ensure compatibility with NS-3.17 and its modules. We provide two methods to set up your environment: using Docker (highly recommended) or a bare-metal installation on Ubuntu 22.04.

### Method 1: Run via Docker (Recommended)

To avoid modifying your host operating system and resolve legacy dependency conflicts, we strongly recommend using Docker to run the NS-3 simulation. We have provided a `Dockerfile` that encapsulates the exact environment needed.

**1. Build the Docker image:** Build the image using the provided `Dockerfile`. The `--platform linux/amd64` flag ensures architecture compatibility, which is especially important if you are building on an ARM-based machine (e.g., Apple Silicon Mac).

```bash
docker build --platform linux/amd64 -f Dockerfile -t lcmp-img:amd64 .
```

**2. Run the container in the background:** Start the container in detached mode (`-d`). This command mounts your current repository directory into `/root` inside the container and automatically configures the `waf` build system for the NS-3 simulation.

```bash
docker run -itd --name lcmp-sim -v $(pwd):/root lcmp-img:amd64 bash -c "cd simulation/; chmod +x waf; ./waf configure; cd ~; exec bash"
```

**3. Enter the running container:** Use `docker exec` to access the interactive bash shell of the running container. Once inside, you can seamlessly run the simulation and analysis scripts.

```bash
docker exec -it lcmp-sim bash
```

**4. Exit the container:** When you are finished with your experiments, you can safely exit the container shell. (Note: The container will remain running in the background until you explicitly stop it using `docker stop lcmp-sim`).

```bash
exit
```

### Method 2: Bare-metal Installation on Ubuntu 22.04

If you prefer to run the simulation directly on your host machine, follow the steps below to set up your environment on Ubuntu 22.04:

**1. Install Required Packages**

```bash
sudo apt update

sudo apt install -y build-essential gcc g++ mercurial libsqlite3-dev libxml2-dev libgtk2.0-0 libgtk2.0-dev uncrustify python2-dev python2 gnupg python3-pip cmake libboost-all-dev git

pip3 install pandas matplotlib
```

**2. Link the python binary** This project depends on Python 2.x. Some scripts or build steps require Python to point to python2:

```bash
sudo ln -sf /usr/bin/python2 /usr/bin/python
```

**3. Install Legacy GCC/G++ 5.x** Ubuntu 22.04 does not include GCC 5 in the official repos. Add the Ubuntu Xenial repository temporarily to obtain these older compilers:

- Open apt sources list:
```bash
sudo vim /etc/apt/sources.list
```
- Add these lines at the end, then save and exit:
```
deb http://us.archive.ubuntu.com/ubuntu/ xenial main
deb http://us.archive.ubuntu.com/ubuntu/ xenial universe
```
- Update apt and add missing keys if needed:
```bash
sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 40976EAF437D05B5 3B4FE6ACC0B21F32
sudo apt update
```
- Install GCC/G++ 5:
```bash
sudo apt install -y gcc-5 g++-5
```
- Verify the version:
```bash
gcc-5 --version  # Should print 5.x.x
```
- (Strongly recommended): Remove the xenial lines from `/etc/apt/sources.list` and update again:
```bash
sudo nano /etc/apt/sources.list
# remove the xenial lines
deb ... xenial main
sudo apt update
```

#### 4. Manage and Set Compiler Alternatives
Set gcc and g++ to use the 5.x versions via update-alternatives:
```bash
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-5 50
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-5 50
```
- Verify the change:
```bash
gcc --version
g++ --version
# Both should report 5.x.x as the active version
```

---



## NS-3 simulation

The ns-3 simulation is under `simulation/`. Refer to the README.md under it for more details.



## Traffic generator

The traffic generator is under `traffic_gen/`. Refer to the README.md under it for more details.



## Analysis
We provide analysis scripts under `analysis/` to analyze the fct-slowdown and link-utilization of different algorithms.
Refer to the README.md under it for more details.




## Reproducing Experiments

For convenience, we provide automated shell scripts in the `scripts/` folder that execute the complete workflow (simulation, analysis, and visualization) for each experiment presented in the paper. Each script handles the entire pipeline automatically, from running simulations to generating plots.

To reproduce all experiments:
```bash
cd scripts
bash run_all.sh
```

To reproduce a specific experiment (e.g., Figure 5):
```bash
cd scripts
bash run_figure5.sh
```

See `scripts/README.md` for detailed information about available scripts and their usage.



## License

This project is licensed under the Apache-2.0 License.



## Contact

Dong-Yang Yu: A @ bupt.edu.cn (A<=>dyyu)
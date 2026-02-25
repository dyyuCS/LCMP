# Traffic Generator
This folder includes the scripts for generating traffic.

## Usage

`python3 traffic_gen_only_for_interDC.py -h` or 
`python3 traffic_gen_only_for_interDC_specify2dc.py -h` for help.

Example:
```bash
python3 traffic_gen_only_for_interDC.py -c flowCDF/WebSearch_distribution.txt --dc_count 8 --dc_nnodes 16 -l 0.3 -b 100G -t 0.01 -o "traffic_WebSearch_8DC-0.3util.txt"
```
generates inter data center traffic according to the web search flow size distribution, for 8 data center network with 16 hosts in each data center, at 30% network load with 100Gbps host bandwidth for 0.01 seconds.

```bash
python3 traffic_gen_only_for_interDC_specify2dc.py -c flowCDF/WebSearch_distribution.txt --dc_count 8 --dc_nnodes 16 -l 0.3 -b 100G -t 0.01 -o "traffic_WebSearch_8DC-onlyDC1-8-0.3util.txt"
```
generates the same inter data center traffic between only DC1 and DC13.

The generate traffic can be directly used by the simulation.

## Traffic format
The first line is the number of flows.

Each line after that is a flow: `<source host> <dest host> 3 <dest port number> <flow size (bytes)> <start time (seconds)>`

## Flow size distributions
We provide 4 distributions. `WebSearch_distribution.txt` and `FbHdp_distribution.txt` are the ones used in the HPCC paper. `AliStorage2019.txt` are collected from Alibaba's production distributed storage system in 2019. 

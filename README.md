# V2X

# V2X Simulation with ns-3.38

This repository contains an ns-3 simulation program **`v2x-sim-realtime-fixed.cc`** for testing real-time-like V2X (Vehicle-to-Everything) communication.

The simulation models:
- **RSU (Road Side Unit) broadcasting beacons** over UDP
- **Vehicles listening for RSU beacons**
- Vehicles sending **ARP-probe** and then **DATA packets** to RSU
- Integration with **FlowMonitor**, **PCAP**, **ASCII traces**, and **QueueDisc traces**
- Optional **NetAnim visualization**

---

## Features
- **Beaconing**: RSU broadcasts UDP beacon messages periodically.
- **Vehicle reaction**: Each vehicle responds to the first beacon with:
  - An **ARP-probe** (small packet).
  - A **DATA packet** after ARP resolution.
  - Optional redundant DATA packet later.
- **Tracing**:
  - PCAP dumps of packets at PHY.
  - ASCII traces for debugging.
  - FlowMonitor XML for flow-level statistics.
  - NetAnim XML (optional) for animation.
  - Queue traces (enqueue/dequeue/drop events).

---

## Requirements
- [ns-3.38](https://www.nsnam.org/releases/ns-3-38/) installed.
- C++17 compatible compiler (e.g., `g++` ≥ 9.0).
- Standard ns-3 modules: `core`, `network`, `mobility`, `wifi`, `internet`, `applications`, `flow-monitor`, `traffic-control`, `animation`.

---

## Build

From your **ns-3.38 root directory**:

```bash
# build the program
./ns3 build scratch/V2X-Main.cc
./ns3 run scratch/V2X-Main.cc

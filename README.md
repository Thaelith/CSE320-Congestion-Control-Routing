# CSE 320 - TCP Congestion Control and Link-State Routing Simulation

**Student ID:** 20220808061

**Algorithm Selection Calculation:**  
`20220808061 % 3 = 1` -> **TCP Reno**

## Project Overview

This project simulates TCP congestion control behavior and demonstrates link-state routing using Dijkstra's shortest path algorithm. It is written in C and evaluates network state handling for different variations of TCP (Tahoe, Reno, NewReno) based on simulated incoming events (ACKs, Duplicate ACKs, and Timeouts).

## Features

- **TCP Algorithms**: TCP Tahoe, TCP Reno, TCP NewReno
- **Congestion Control Mechanisms**: Slow Start, Congestion Avoidance, Fast Retransmit, Fast Recovery
- **Loss Scenarios**: Timeout-based loss and Triple Duplicate ACK processing
- **Metrics Evaluation**: Tracking and printing of Congestion Window (`cwnd`) and Slow Start Threshold (`ssthresh`) evolution
- **Link-State Routing**: Dijkstra's algorithm for route generation
- **Topology Configuration**: Routing table parsing and generation from `A.conf` to `F.conf`
- **Simulation**: Shortest-path message forwarding simulation

## File Structure

| File | Description |
|---|---|
| `node.c` | Main source code implementing TCP transitions and Dijkstra's algorithm. |
| `node.h` | Header file containing structures, constants, and function declarations. |
| `A.conf` - `F.conf` | Configuration files for the six-node network topology. |
| `example_events.txt` | General ACK, duplicate ACK, and timeout test scenario. |
| `duplicate_ack_test.txt` | Test scenarios for analyzing the triple duplicate ACK threshold. |
| `timeout_test.txt` | Test scenarios for analyzing timeout-based loss. |

The folder includes `cse320_node.exe` for direct Windows execution. Rebuild from source with the commands below if needed.

## Compilation

### Windows (with MinGW)
```bash
gcc -Wall -Wextra node.c -o cse320_node.exe
```

### Linux / macOS
```bash
gcc -Wall -Wextra node.c -o cse320_node
```

## Running the Simulation

*(Note: The commands below assume compilation on Windows `.\cse320_node.exe`. On Linux/macOS, use `./cse320_node` instead.)*

### How to Run the Selected TCP Reno Tests
```bash
.\cse320_node.exe A.conf reno example_events.txt
```

### How to Compare Reno and Tahoe
```bash
.\cse320_node.exe A.conf tahoe duplicate_ack_test.txt
.\cse320_node.exe A.conf reno duplicate_ack_test.txt
```

### How to Run Timeout Scenario
```bash
.\cse320_node.exe A.conf reno timeout_test.txt
```

### How to Run Triple Duplicate ACK Scenario
```bash
.\cse320_node.exe A.conf reno duplicate_ack_test.txt
```

### How to Run Routing Table Generation
```bash
.\cse320_node.exe --route A
.\cse320_node.exe --route B
.\cse320_node.exe --route F
```

### How to Run Message Forwarding Examples
```bash
.\cse320_node.exe --send A D hello_from_A_to_D
.\cse320_node.exe --send F E hello_from_F_to_E
```

## Expected Key Outputs

### Routing Expectations
According to Dijkstra's algorithm implemented in this project:
- **A to D:** The shortest path must route via `A -> B -> D` with a total cost of **12**, bypassing the direct link of cost 13.
- **F to E:** The shortest path must route via `F -> A -> B -> E` with a total cost of **12**.

### Theoretical Background

#### TCP Reno vs TCP Tahoe
- **TCP Tahoe:** Upon detecting packet loss (via timeout OR three duplicate ACKs), Tahoe reacts by aggressively dropping the congestion window (`cwnd`) to 1 MSS, resetting the workflow entirely back to Slow Start.
- **TCP Reno:** When encountering three duplicate ACKs, Reno avoids a full reset. It drops `cwnd` by half and initiates **Fast Recovery** and **Fast Retransmit**, performing significantly better on moderately congested lines than Tahoe. On a timeout, Reno behaves identical to Tahoe, dropping `cwnd` to 1. 

#### Timeout vs Triple Duplicate ACK
- **Timeout:** Indicates a severe congestion problem or an actual disconnected link, meaning ACKs failed to arrive within the RTO timeframe. The response is an immediate `cwnd` reset.
- **Triple Duplicate ACK:** Indicates moderate congestion. The receiver successfully got subsequent packets, but is missing one. Network traffic is still actively flowing, so a softer back-off algorithm (Fast Recovery) is employed.

## Demo Video
**Demo Video:** [https://drive.google.com/file/d/1AQbJoSWlubVlU3jlPJutbbMH98w6Dunw/view?usp=sharing](https://drive.google.com/file/d/1AQbJoSWlubVlU3jlPJutbbMH98w6Dunw/view?usp=sharing)

# CSE 320 - TCP Congestion Control and Link-State Routing Simulation

**Student ID:** 20220808061

**Selected Algorithm:** TCP Reno

The assignment selects the main TCP congestion control algorithm with:

```text
20220808061 % 3 = 1
```

Result `1` means the selected algorithm is **TCP Reno**.

## Project Overview

This project is a C simulation for TCP congestion control and link-state routing. It prints congestion window (`cwnd`) evolution step-by-step for packet events, and it uses Dijkstra's algorithm on the provided six-node topology.

The program supports:

- TCP Tahoe event simulation
- TCP Reno event simulation, which is the selected main algorithm
- Basic NewReno option in the parser, without full partial ACK handling
- Slow Start
- Congestion Avoidance
- Fast Retransmit
- Fast Recovery for Reno
- Timeout-based loss
- Triple duplicate ACK loss
- Link-state routing for nodes A, B, C, D, E, F
- Interactive node mode with `send`, `route`, `help`, and `quit`

## Files

| File | Description |
|---|---|
| `node.c` | Main C source code for TCP event handling, Dijkstra routing, and interactive commands. |
| `node.h` | Header file with constants, structures, enums, and function declarations. |
| `A.conf` - `F.conf` | Instructor-provided topology configuration files. |
| `example_events.txt` | Instructor-provided TCP event scenario. |
| `duplicate_ack_test.txt` | Instructor-provided triple duplicate ACK scenario. |
| `timeout_test.txt` | Instructor-provided timeout scenario. |

## Compilation

Linux / macOS:

```bash
gcc -Wall -Wextra node.c -o node
```

Windows with MinGW:

```bash
gcc -Wall -Wextra node.c -o node.exe
```


## Interactive Node Mode

The assignment expects nodes to be started with a configuration file:

```bash
./node A.conf
./node B.conf
./node C.conf
./node D.conf
./node E.conf
./node F.conf
```

On Windows:

```bash
.\node.exe A.conf
```

After starting a node, the program prints node information, loads the full topology, prints the routing table for the current node, and shows a prompt:

```text
node A>
```

Available commands:

```text
send <destination> <message>
route
help
quit
```

Example from node A:

```text
node A> send D hello_from_A_to_D
```

Expected forwarding path:

```text
[A] Destination D, next hop B
[B] Forwarding message from A to D, next hop D
[D] Received message from A: hello_from_A_to_D
```

Example from node F:

```text
node F> send E hello_from_F_to_E
```

Expected forwarding path:

```text
[F] Destination E, next hop A
[A] Forwarding message from F to E, next hop B
[B] Forwarding message from F to E, next hop E
[E] Received message from F: hello_from_F_to_E
```

## Routing Requirements

The provided topology has nodes A, B, C, D, E, and F.

Important shortest paths:

- A to D must use `A -> B -> D` with cost `12`.
- A to D must not use direct `A -> D`, because direct cost is `13`.
- F to E must use `F -> A -> B -> E` with cost `12`.

You can also print a routing table directly:

```bash
./node --route A
./node --route F
```

## TCP Congestion Control Event Mode

The selected algorithm for this student ID is TCP Reno:

```bash
./node A.conf reno example_events.txt
```

Compare Tahoe and Reno under the same triple duplicate ACK input:

```bash
./node A.conf tahoe duplicate_ack_test.txt
./node A.conf reno duplicate_ack_test.txt
```

Run timeout-based loss:

```bash
./node A.conf reno timeout_test.txt
```

Run triple duplicate ACK loss:

```bash
./node A.conf reno duplicate_ack_test.txt
```

The output includes:

- Round number
- Event type
- ACK number
- `cwnd`
- `ssthresh`
- Current congestion control state
- Explanation of the transition

## Experimental Analysis

### Timeout Loss

For both Tahoe and Reno, timeout is treated as severe congestion. The program sets `ssthresh` to half of the current `cwnd`, resets `cwnd` to `1`, clears duplicate ACK count, and returns to Slow Start.

This shows that Reno does not avoid a full restart when timeout happens.

### Triple Duplicate ACK Loss

Tahoe treats three duplicate ACKs as packet loss and resets `cwnd` to `1`, returning to Slow Start.

Reno handles three duplicate ACKs with Fast Retransmit and Fast Recovery. It sets `ssthresh` to half of `cwnd`, sets `cwnd` to `ssthresh + 3`, and enters Fast Recovery instead of restarting from `cwnd = 1`.

This demonstrates Reno recovering faster than Tahoe for triple duplicate ACK loss.

## Demo Video Checklist

The demonstration video should show:

1. Running at least two TCP algorithms, such as Tahoe and Reno.
2. Step-by-step `cwnd` evolution.
3. Timeout loss scenario.
4. Triple duplicate ACK scenario.
5. Brief explanation of Tahoe vs Reno behavior.
6. Interactive node mode using `./node A.conf`.
7. Message forwarding from A to D through `A -> B -> D`.
8. Message forwarding from F to E through `F -> A -> B -> E`.

## Demo Video

**Demo Video:** [https://drive.google.com/file/d/1AQbJoSWlubVlU3jlPJutbbMH98w6Dunw/view?usp=sharing](https://drive.google.com/file/d/1AQbJoSWlubVlU3jlPJutbbMH98w6Dunw/view?usp=sharing)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "node.h"

Algorithm parse_algorithm(const char *name) {
    if (strcmp(name, "tahoe") == 0) return ALG_TAHOE;
    if (strcmp(name, "reno") == 0) return ALG_RENO;
    if (strcmp(name, "newreno") == 0) return ALG_NEWRENO;
    return ALG_UNKNOWN;
}

const char *algorithm_name(Algorithm algorithm) {
    switch (algorithm) {
        case ALG_TAHOE: return "TCP Tahoe";
        case ALG_RENO: return "TCP Reno";
        case ALG_NEWRENO: return "TCP NewReno";
        default: return "Unknown";
    }
}

const char *state_name(CCState state) {
    switch (state) {
        case SLOW_START: return "Slow Start";
        case CONGESTION_AVOIDANCE: return "Congestion Avoidance";
        case FAST_RECOVERY: return "Fast Recovery";
        default: return "Unknown";
    }
}

int load_config(const char *filename, NodeConfig *config) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        return 0;
    }

    char line[MAX_LINE];

    config->neighbor_count = 0;

    /*
       First non-comment line:
       NodeID Port

       Example:
       A 5001
    */
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }

        if (sscanf(line, " %c %d", &config->node_id, &config->port) == 2) {
            break;
        }
    }

    /*
       Remaining non-comment lines:
       NeighborID IP Port Cost

       Example:
       B 127.0.0.1 5002 4
    */
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }

        if (config->neighbor_count >= MAX_NEIGHBORS) {
            printf("Too many neighbors. Increase MAX_NEIGHBORS.\n");
            fclose(fp);
            return 0;
        }

        Neighbor *n = &config->neighbors[config->neighbor_count];

        if (sscanf(line, " %c %63s %d %d", &n->id, n->ip, &n->port, &n->cost) == 4) {
            config->neighbor_count++;
        }
    }

    fclose(fp);
    return 1;
}

void init_tcp(TCPState *tcp, Algorithm algorithm) {
    tcp->algorithm = algorithm;
    tcp->state = SLOW_START;
    tcp->cwnd = 1.0;
    tcp->ssthresh = 16.0;
    tcp->dup_ack_count = 0;
    tcp->round = 0;
}

static void print_tcp_status(TCPState *tcp, const char *event, int ack_no, const char *note) {
    printf("%5d | %-10s | ACK=%-3d | cwnd=%5.2f | ssthresh=%5.2f | %-22s | %s\n",
           tcp->round,
           event,
           ack_no,
           tcp->cwnd,
           tcp->ssthresh,
           state_name(tcp->state),
           note);
}

void on_ack(TCPState *tcp, int ack_no) {
    tcp->round++;
    tcp->dup_ack_count = 0;

    if (tcp->state == SLOW_START) {
        tcp->cwnd += 1.0;

        if (tcp->cwnd >= tcp->ssthresh) {
            tcp->state = CONGESTION_AVOIDANCE;
        }

        print_tcp_status(tcp, "ACK", ack_no, "new ACK received; cwnd increased");
    }
    else if (tcp->state == CONGESTION_AVOIDANCE) {
        tcp->cwnd += 1.0 / tcp->cwnd;
        print_tcp_status(tcp, "ACK", ack_no, "new ACK received; additive increase");
    }
    else if (tcp->state == FAST_RECOVERY) {
        /*
           Reno:
           A full ACK exits Fast Recovery.

           NewReno:
           Students may extend this part:
           - partial ACK: retransmit next missing segment and stay in Fast Recovery
           - full ACK: exit Fast Recovery
        */
        tcp->cwnd = tcp->ssthresh;
        tcp->state = CONGESTION_AVOIDANCE;
        print_tcp_status(tcp, "ACK", ack_no, "full ACK; exit Fast Recovery");
    }
}

void on_duplicate_ack(TCPState *tcp, int ack_no) {
    tcp->round++;
    tcp->dup_ack_count++;

    if (tcp->dup_ack_count < 3) {
        print_tcp_status(tcp, "DUPACK", ack_no, "duplicate ACK received; waiting");
        return;
    }

    /*
       Three duplicate ACKs indicate likely packet loss.
    */
    tcp->ssthresh = tcp->cwnd / 2.0;
    if (tcp->ssthresh < 2.0) {
        tcp->ssthresh = 2.0;
    }

    if (tcp->algorithm == ALG_TAHOE) {
        tcp->cwnd = 1.0;
        tcp->state = SLOW_START;
        print_tcp_status(tcp, "DUPACK", ack_no, "3 duplicate ACKs; Tahoe resets cwnd");
    }
    else if (tcp->algorithm == ALG_RENO) {
        tcp->cwnd = tcp->ssthresh + 3.0;
        tcp->state = FAST_RECOVERY;
        print_tcp_status(tcp, "DUPACK", ack_no, "3 duplicate ACKs; Reno Fast Recovery");
    }
    else if (tcp->algorithm == ALG_NEWRENO) {
        tcp->cwnd = tcp->ssthresh + 3.0;
        tcp->state = FAST_RECOVERY;
        print_tcp_status(tcp, "DUPACK", ack_no, "3 duplicate ACKs; NewReno Fast Recovery");
    }
}

void on_timeout(TCPState *tcp) {
    tcp->round++;

    tcp->ssthresh = tcp->cwnd / 2.0;
    if (tcp->ssthresh < 2.0) {
        tcp->ssthresh = 2.0;
    }

    tcp->cwnd = 1.0;
    tcp->dup_ack_count = 0;
    tcp->state = SLOW_START;

    print_tcp_status(tcp, "TIMEOUT", 0, "timeout; cwnd reset to 1");
}

static void print_config(NodeConfig *config) {
    int i;

    printf("Node %c listening on port %d\n", config->node_id, config->port);
    printf("Neighbors:\n");

    for (i = 0; i < config->neighbor_count; i++) {
        printf("  %c %s %d cost=%d\n",
               config->neighbors[i].id,
               config->neighbors[i].ip,
               config->neighbors[i].port,
               config->neighbors[i].cost);
    }
}

static void print_interactive_help(void) {
    printf("Commands:\n");
    printf("  send <destination> <message>\n");
    printf("  route\n");
    printf("  help\n");
    printf("  quit\n");
}

static int run_event_file(const char *event_file, TCPState *tcp) {
    FILE *fp = fopen(event_file, "r");
    if (!fp) {
        printf("Could not open event file: %s\n", event_file);
        return 0;
    }

    char event[32];
    int ack_no;

    printf("\nCongestion-control trace\n");
    printf("Round | Event      | ACK    | cwnd       | ssthresh   | State                  | Explanation\n");
    printf("------------------------------------------------------------------------------------------------\n");

    while (fscanf(fp, "%31s", event) == 1) {
        if (strcmp(event, "ACK") == 0) {
            if (fscanf(fp, "%d", &ack_no) != 1) {
                printf("Malformed ACK event.\n");
                fclose(fp);
                return 0;
            }
            on_ack(tcp, ack_no);
        }
        else if (strcmp(event, "DUPACK") == 0) {
            if (fscanf(fp, "%d", &ack_no) != 1) {
                printf("Malformed DUPACK event.\n");
                fclose(fp);
                return 0;
            }
            on_duplicate_ack(tcp, ack_no);
        }
        else if (strcmp(event, "TIMEOUT") == 0) {
            on_timeout(tcp);
        }
        else {
            printf("Unknown event: %s\n", event);
            fclose(fp);
            return 0;
        }
    }

    fclose(fp);
    return 1;
}

static int graph_index(NetworkGraph *graph, char node_id) {
    int i;

    for (i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i] == node_id) {
            return i;
        }
    }

    return -1;
}

static int graph_add_node(NetworkGraph *graph, char node_id) {
    int index;

    index = graph_index(graph, node_id);
    if (index != -1) {
        return index;
    }

    if (graph->node_count >= MAX_NODES) {
        printf("Too many nodes. Increase MAX_NODES.\n");
        return -1;
    }

    graph->nodes[graph->node_count] = node_id;
    graph->node_count++;

    return graph->node_count - 1;
}

void init_graph(NetworkGraph *graph) {
    int i, j;

    graph->node_count = 0;

    for (i = 0; i < MAX_NODES; i++) {
        graph->nodes[i] = '\0';

        for (j = 0; j < MAX_NODES; j++) {
            if (i == j) {
                graph->cost[i][j] = 0;
            } else {
                graph->cost[i][j] = INF;
            }
        }
    }
}

static int load_config_into_graph(NetworkGraph *graph, const char *filename) {
    NodeConfig config;
    int source_index;
    int neighbor_index;
    int i;

    if (!load_config(filename, &config)) {
        printf("Could not load config file for graph: %s\n", filename);
        return 0;
    }

    source_index = graph_add_node(graph, config.node_id);
    if (source_index == -1) {
        return 0;
    }

    for (i = 0; i < config.neighbor_count; i++) {
        neighbor_index = graph_add_node(graph, config.neighbors[i].id);
        if (neighbor_index == -1) {
            return 0;
        }

        graph->cost[source_index][neighbor_index] = config.neighbors[i].cost;
        graph->cost[neighbor_index][source_index] = config.neighbors[i].cost;
    }

    return 1;
}

int load_all_configs(NetworkGraph *graph) {
    const char *files[] = {
        "A.conf",
        "B.conf",
        "C.conf",
        "D.conf",
        "E.conf",
        "F.conf"
    };

    int file_count = 6;
    int i;

    init_graph(graph);

    for (i = 0; i < file_count; i++) {
        if (!load_config_into_graph(graph, files[i])) {
            return 0;
        }
    }

    return 1;
}

static void dijkstra(NetworkGraph *graph, int source_index, int dist[], int previous[]) {
    int visited[MAX_NODES];
    int i, step;

    for (i = 0; i < graph->node_count; i++) {
        dist[i] = INF;
        previous[i] = -1;
        visited[i] = 0;
    }

    dist[source_index] = 0;

    for (step = 0; step < graph->node_count; step++) {
        int u = -1;
        int best = INF;

        for (i = 0; i < graph->node_count; i++) {
            if (!visited[i] && dist[i] < best) {
                best = dist[i];
                u = i;
            }
        }

        if (u == -1) {
            break;
        }

        visited[u] = 1;

        for (i = 0; i < graph->node_count; i++) {
            if (!visited[i] && graph->cost[u][i] != INF) {
                int alternative = dist[u] + graph->cost[u][i];

                if (alternative < dist[i]) {
                    dist[i] = alternative;
                    previous[i] = u;
                }
            }
        }
    }
}

static int build_path(NetworkGraph *graph, int source_index, int destination_index, int previous[], char path[]) {
    int stack[MAX_NODES];
    int count = 0;
    int current = destination_index;
    int i;

    while (current != -1) {
        stack[count++] = current;

        if (current == source_index) {
            break;
        }

        current = previous[current];
    }

    if (count == 0 || stack[count - 1] != source_index) {
        return 0;
    }

    for (i = 0; i < count; i++) {
        path[i] = graph->nodes[stack[count - 1 - i]];
    }

    return count;
}

void print_routing_table(NetworkGraph *graph, char source) {
    int source_index;
    int dist[MAX_NODES];
    int previous[MAX_NODES];
    int order[MAX_NODES];
    int i, j;

    source_index = graph_index(graph, source);

    if (source_index == -1) {
        printf("Source node %c not found in graph.\n", source);
        return;
    }

    dijkstra(graph, source_index, dist, previous);

    for (i = 0; i < graph->node_count; i++) {
        order[i] = i;
    }

    for (i = 0; i < graph->node_count - 1; i++) {
        for (j = i + 1; j < graph->node_count; j++) {
            if (graph->nodes[order[j]] < graph->nodes[order[i]]) {
                int temp = order[i];
                order[i] = order[j];
                order[j] = temp;
            }
        }
    }

    printf("\nRouting table for node %c\n", source);
    printf("Destination | Next Hop | Cost | Path\n");
    printf("---------------------------------------------\n");

    for (i = 0; i < graph->node_count; i++) {
        char path[MAX_NODES];
        int path_length;
        char next_hop = '-';
        int destination_index = order[i];

        path_length = build_path(graph, source_index, destination_index, previous, path);

        if (path_length > 1) {
            next_hop = path[1];
        }

        printf("%11c | %8c | ", graph->nodes[destination_index], next_hop);

        if (dist[destination_index] == INF) {
            printf("%4s | ", "INF");
        } else {
            printf("%4d | ", dist[destination_index]);
        }

        if (path_length == 0) {
            printf("unreachable");
        } else {
            for (j = 0; j < path_length; j++) {
                printf("%c", path[j]);

                if (j < path_length - 1) {
                    printf(" -> ");
                }
            }
        }

        printf("\n");
    }
}

int simulate_send(NetworkGraph *graph, char source, char destination, const char *message) {
    int source_index;
    int destination_index;
    int dist[MAX_NODES];
    int previous[MAX_NODES];
    char path[MAX_NODES];
    int path_length;
    int i;

    source_index = graph_index(graph, source);
    destination_index = graph_index(graph, destination);

    if (source_index == -1) {
        printf("Source node %c not found in graph.\n", source);
        return 0;
    }

    if (destination_index == -1) {
        printf("Destination node %c not found in graph.\n", destination);
        return 0;
    }

    dijkstra(graph, source_index, dist, previous);

    if (dist[destination_index] == INF) {
        printf("No route from %c to %c.\n", source, destination);
        return 0;
    }

    path_length = build_path(graph, source_index, destination_index, previous, path);

    if (path_length == 0) {
        printf("Could not build path from %c to %c.\n", source, destination);
        return 0;
    }

    printf("\nSending message from %c to %c\n", source, destination);
    printf("Shortest path cost: %d\n", dist[destination_index]);
    printf("Path: ");

    for (i = 0; i < path_length; i++) {
        printf("%c", path[i]);

        if (i < path_length - 1) {
            printf(" -> ");
        }
    }

    printf("\n\n");

    for (i = 0; i < path_length; i++) {
        if (i == 0 && path_length > 1) {
            printf("[%c] Destination %c, next hop %c\n", path[i], destination, path[i + 1]);
        }
        else if (i < path_length - 1) {
            printf("[%c] Forwarding message from %c to %c, next hop %c\n",
                   path[i],
                   source,
                   destination,
                   path[i + 1]);
        }
        else {
            printf("[%c] Received message from %c: %s\n", path[i], source, message);
        }
    }

    return 1;
}

static int run_interactive_node(const char *config_file) {
    NodeConfig config;
    NetworkGraph graph;
    char line[MAX_COMMAND];

    if (!load_config(config_file, &config)) {
        printf("Could not load config file: %s\n", config_file);
        return 0;
    }

    print_config(&config);

    if (!load_all_configs(&graph)) {
        return 0;
    }

    print_routing_table(&graph, config.node_id);
    printf("\nType 'help' for available commands.\n");

    while (1) {
        char *command;

        printf("\nnode %c> ", config.node_id);

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        line[strcspn(line, "\r\n")] = '\0';
        command = line;

        while (*command == ' ' || *command == '\t') {
            command++;
        }

        if (*command == '\0') {
            continue;
        }

        if (strcmp(command, "quit") == 0 || strcmp(command, "exit") == 0) {
            printf("Exiting node %c.\n", config.node_id);
            break;
        }
        else if (strcmp(command, "help") == 0) {
            print_interactive_help();
        }
        else if (strcmp(command, "route") == 0) {
            print_routing_table(&graph, config.node_id);
        }
        else if (strncmp(command, "send", 4) == 0 &&
                 (command[4] == '\0' || command[4] == ' ' || command[4] == '\t')) {
            char *p = command + 4;
            char destination;
            char *message;

            while (*p == ' ' || *p == '\t') {
                p++;
            }

            if (*p == '\0') {
                printf("Usage: send <destination> <message>\n");
                continue;
            }

            destination = *p;

            while (*p != '\0' && *p != ' ' && *p != '\t') {
                p++;
            }

            while (*p == ' ' || *p == '\t') {
                p++;
            }

            message = p;

            if (*message == '\0') {
                printf("Usage: send <destination> <message>\n");
                continue;
            }

            simulate_send(&graph, config.node_id, destination, message);
        }
        else {
            printf("Unknown command. Type 'help' for available commands.\n");
        }
    }

    return 1;
}

int main(int argc, char *argv[]) {
    NodeConfig config;
    TCPState tcp;

    if (argc == 3 && strcmp(argv[1], "--route") == 0) {
        NetworkGraph graph;

        if (!load_all_configs(&graph)) {
            return 1;
        }

        print_routing_table(&graph, argv[2][0]);
        return 0;
    }

    if (argc == 5 && strcmp(argv[1], "--send") == 0) {
        NetworkGraph graph;

        if (!load_all_configs(&graph)) {
            return 1;
        }

        simulate_send(&graph, argv[2][0], argv[3][0], argv[4]);
        return 0;
    }

    if (argc == 2) {
        if (!run_interactive_node(argv[1])) {
            return 1;
        }

        return 0;
    }

    /*
       For simplicity:
       ./node A.conf reno events/example_events.txt

       Students may also hard-code their selected algorithm if the instructor prefers:
       Algorithm selected = ALG_RENO;
    */
    if (argc != 4) {
        printf("Usage: ./node <config_file>\n");
        printf("Usage: ./node <config_file> <algorithm> <event_file>\n");
        printf("Usage: ./node --route <source>\n");
        printf("Usage: ./node --send <source> <destination> <message>\n");
        printf("Interactive example: ./node A.conf\n");
        printf("Example: ./node A.conf reno events/example_events.txt\n");
        printf("Algorithms: tahoe, reno, newreno\n");
        return 1;
    }

    Algorithm selected = parse_algorithm(argv[2]);
    if (selected == ALG_UNKNOWN) {
        printf("Invalid algorithm. Use: tahoe, reno, or newreno\n");
        return 1;
    }

    if (!load_config(argv[1], &config)) {
        printf("Could not load config file: %s\n", argv[1]);
        return 1;
    }

    print_config(&config);

    init_tcp(&tcp, selected);

    printf("\nSelected congestion control algorithm: %s\n", algorithm_name(selected));
    printf("Initial cwnd=%.2f, ssthresh=%.2f\n", tcp.cwnd, tcp.ssthresh);

    if (!run_event_file(argv[3], &tcp)) {
        return 1;
    }

    return 0;
}

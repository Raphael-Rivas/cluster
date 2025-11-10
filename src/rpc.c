#include "rpc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>

static uint32_t g_node_id = 0;

int rpc_server_start(int port) {
    int server_socket;
    struct sockaddr_in server_addr;
    int opt = 1;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        return -1;
    }

    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Setsockopt failed");
        close(server_socket);
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        return -1;
    }

    if (listen(server_socket, 10) < 0) {
        perror("Listen failed");
        close(server_socket);
        return -1;
    }

    printf("RPC Server started on port %d\n", port);
    return server_socket;
}

void rpc_handle_client(int client_socket) {
    rpc_message_t msg;
    ssize_t bytes_received;

    bytes_received = recv(client_socket, &msg, sizeof(rpc_message_t), 0);

    if (bytes_received < 0) {
        perror("Receive failed");
        close(client_socket);
        return;
    }

    if (bytes_received == 0) {
        printf("Client disconnected\n");
        close(client_socket);
        return;
    }

    switch (msg.type) {
        case MSG_PING:
            printf("Received PING from node %u (timestamp: %u)\n",
                   msg.node_id, msg.timestamp);
            if (rpc_send_pong(client_socket, g_node_id) < 0) {
                fprintf(stderr, "Failed to send PONG\n");
            }
            break;

        case MSG_PONG:
            printf("Received PONG from node %u (timestamp: %u)\n",
                   msg.node_id, msg.timestamp);
            break;

        default:
            fprintf(stderr, "Unknown message type: %u\n", msg.type);
            break;
    }

    close(client_socket);
}

int rpc_send_ping(const char *host, int port, uint32_t node_id) {
    int client_socket;
    struct sockaddr_in server_addr;
    rpc_message_t msg;

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Socket creation failed");
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(client_socket);
        return -1;
    }

    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(client_socket);
        return -1;
    }

    msg.type = MSG_PING;
    msg.node_id = node_id;
    msg.timestamp = (uint32_t)time(NULL);

    if (send(client_socket, &msg, sizeof(rpc_message_t), 0) < 0) {
        perror("Send failed");
        close(client_socket);
        return -1;
    }

    printf("Sent PING to %s:%d (node_id: %u)\n", host, port, node_id);

    // Attente de la réponse PONG
    rpc_message_t response;
    ssize_t bytes_received = recv(client_socket, &response, sizeof(rpc_message_t), 0);

    if (bytes_received > 0 && response.type == MSG_PONG) {
        printf("Received PONG from %s:%d (node_id: %u)\n",
               host, port, response.node_id);
    }

    close(client_socket);
    return 0;
}

int rpc_send_pong(int client_socket, uint32_t node_id) {
    rpc_message_t msg;

    msg.type = MSG_PONG;
    msg.node_id = node_id;
    msg.timestamp = (uint32_t)time(NULL);

    if (send(client_socket, &msg, sizeof(rpc_message_t), 0) < 0) {
        perror("Send failed");
        return -1;
    }

    printf("Sent PONG to client (node_id: %u)\n", node_id);
    return 0;
}

void* rpc_client_thread(void* arg) {
    int client_socket = *((int*)arg);
    free(arg);

    rpc_handle_client(client_socket);

    return NULL;
}

void rpc_server_run(int server_socket, uint32_t node_id) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    pthread_t thread_id;

    g_node_id = node_id;

    printf("RPC Server running (node_id: %u)...\n", node_id);

    while (1) {
        int* client_socket = malloc(sizeof(int));
        if (client_socket == NULL) {
            perror("Memory allocation failed");
            continue;
        }

        *client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);

        if (*client_socket < 0) {
            perror("Accept failed");
            free(client_socket);
            continue;
        }

        printf("New connection from %s:%d\n",
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));

        if (pthread_create(&thread_id, NULL, rpc_client_thread, client_socket) != 0) {
            perror("Thread creation failed");
            close(*client_socket);
            free(client_socket);
            continue;
        }

        pthread_detach(thread_id);
    }
}


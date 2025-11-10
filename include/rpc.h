#ifndef RPC_H
#define RPC_H

#include <stdint.h>

#define MSG_PING 1
#define MSG_PONG 2

typedef struct {
    uint8_t type;
    uint32_t node_id;
    uint32_t timestamp;
} rpc_message_t;

int rpc_server_start(int port);
void rpc_handle_client(int client_socket);
void* rpc_client_thread(void* arg);
void rpc_server_run(int server_socket, uint32_t node_id);
int rpc_send_ping(const char *host, int port, uint32_t node_id);
int rpc_send_pong(int client_socket, uint32_t node_id);

#endif

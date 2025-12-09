#ifndef RPC_H
#define RPC_H

#include <stdint.h>
#include "node.h"

// Protocole simplifié : 1 octet
#define MSG_PING 1
#define MSG_PONG 1
#define REQUEST_VOTE 2

// Démarre un serveur RPC sur le port spécifié
int rpc_server_start(int port);

// Gère une connexion client (appelé dans un thread)
void rpc_handle_client(int client_socket, node_t *node);

// Thread wrapper pour gérer un client
void* rpc_client_thread(void* arg);

// Boucle principale du serveur RPC
void rpc_server_run(int server_socket, node_t *node);

// Envoie un ping à un pair et attend le pong
// Retourne 0 en cas de succès, -1 en cas d'erreur
int rpc_send_ping(const char *host, int port);

// Envoie un pong au client
int rpc_send_pong(int client_socket);

#endif

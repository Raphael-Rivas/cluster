#ifndef RPC_H
#define RPC_H

#include "node.h"
#include <stdint.h>

// Protocole simplifié : 1 octet
#define MSG_PING 1
#define MSG_PONG 1
#define REQUEST_VOTE 2
#define VOTE_GRANTED 1
#define VOTE_DENIED 0
#define MSG_HEARTBEAT 3
#define MSG_HEARTBEAT_ACK 4
#define MSG_APPEND_ENTRIES 5
#define MSG_APPEND_ENTRIES_RESPONSE 6
#define MSG_CLIENT_PUT 10
#define MSG_CLIENT_GET 11
#define MSG_REDIRECT 12
#define MSG_OK 13
#define MSG_FAIL 14

#define CMD_NOOP 0
#define CMD_PUT 1

// Démarre un serveur RPC sur le port spécifié
int rpc_server_start(int port);

// Gère une connexion client (appelé dans un thread)
void rpc_handle_client(int client_socket, node_t *node);

// Thread wrapper pour gérer un client
void *rpc_client_thread(void *arg);

// Boucle principale du serveur RPC
void rpc_server_run(int server_socket, node_t *node);

// Envoie un ping à un pair et attend le pong
// Retourne 0 en cas de succès, -1 en cas d'erreur
int rpc_send_ping(const char *host, int port);

// Envoie un pong au client
int rpc_send_pong(int client_socket);

// Envoie une requête de vote à un pair et attend la réponse
// Retourne 1 si vote accordé, 0 si refusé, -1 en cas d'erreur de communication
int rpc_send_vote_request(const char *host, int port, int candidate_id,
                          int candidate_term, int last_log_index,
                          int last_log_term);

// Envoie un heartbeat à un pair (leader vers follower)
// Retourne 0 en cas de succès, -1 en cas d'erreur
int rpc_send_heartbeat(const char *host, int port, int leader_id,
                       int leader_term);

// Envoie un AppendEntries à un pair (pour heartbeat ou réplication)
// prev_log_index: index du log précédant les nouvelles entrées
// prev_log_term: terme du log précédent
// entries: tableau d'entrées à envoyer (peut être NULL pour heartbeat)
// entries_count: nombre d'entrées
// leader_commit: commitIndex du leader
int rpc_send_append_entries(const char *host, int port, int leader_id,
                            int leader_term, int prev_log_index,
                            int prev_log_term, log_entry_t *entries,
                            int entries_count, int leader_commit);

#endif

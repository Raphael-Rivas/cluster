#include "rpc.h"
#include "node.h"
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// Démarre un serveur TCP sur le port spécifié
int rpc_server_start(int port) {
  int server_socket;
  struct sockaddr_in server_addr;
  int opt = 1;

  // Création du socket
  server_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (server_socket < 0) {
    perror("Socket creation failed");
    return -1;
  }

  // Option SO_REUSEADDR pour éviter "Address already in use"
  if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) <
      0) {
    perror("Setsockopt failed");
    close(server_socket);
    return -1;
  }

  // Configuration de l'adresse
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);

  // Bind
  if (bind(server_socket, (struct sockaddr *)&server_addr,
           sizeof(server_addr)) < 0) {
    perror("Bind failed");
    close(server_socket);
    return -1;
  }

  // Listen
  if (listen(server_socket, 10) < 0) {
    perror("Listen failed");
    close(server_socket);
    return -1;
  }

  printf("[RPC] Serveur démarré sur le port %d\n", port);
  return server_socket;
}

// Gère une connexion client : lit un octet (ping) et répond par un octet (pong)
void rpc_handle_client(int client_socket, node_t *node) {
  uint8_t msg;
  ssize_t bytes_received;

  // Lecture d'un octet
  bytes_received = recv(client_socket, &msg, 1, 0);

  if (bytes_received < 0) {
    perror("[RPC] Receive failed");
    close(client_socket);
    return;
  }

  if (bytes_received == 0) {
    printf("[RPC] Client déconnecté\n");
    close(client_socket);
    return;
  }

  // Vérification du message
  if (msg == MSG_PING) {
    printf("[RPC] PING reçu\n");

    // Envoi du PONG
    if (rpc_send_pong(client_socket) < 0) {
      fprintf(stderr, "[RPC] Échec de l'envoi du PONG\n");
    }
  } else if (msg == REQUEST_VOTE) {
    // Lecture des paramètres : candidate_id (4), candidate_term (4),
    // last_log_index (4), last_log_term (4)
    int32_t candidate_id, candidate_term, last_log_index, last_log_term;

    if (recv(client_socket, &candidate_id, sizeof(int32_t), 0) !=
            sizeof(int32_t) ||
        recv(client_socket, &candidate_term, sizeof(int32_t), 0) !=
            sizeof(int32_t) ||
        recv(client_socket, &last_log_index, sizeof(int32_t), 0) !=
            sizeof(int32_t) ||
        recv(client_socket, &last_log_term, sizeof(int32_t), 0) !=
            sizeof(int32_t)) {
      close(client_socket);
      return;
    }

    // Conversion network byte order -> host byte order
    candidate_id = ntohl(candidate_id);
    candidate_term = ntohl(candidate_term);
    last_log_index = ntohl(last_log_index);
    last_log_term = ntohl(last_log_term);

    // Traiter la requête de vote et obtenir le résultat
    int vote_result = node_handle_request_vote(
        node, candidate_id, candidate_term, last_log_index, last_log_term);

    // Envoyer la réponse au candidat (1 octet : 1 = accordé, 0 = refusé)
    uint8_t response = vote_result ? VOTE_GRANTED : VOTE_DENIED;
    if (send(client_socket, &response, 1, 0) < 0) {
      fprintf(stderr, "[RPC] Échec de l'envoi de la réponse du vote\n");
    }
  } else if (msg == MSG_APPEND_ENTRIES) {
    int32_t leader_id, leader_term, prev_log_index, prev_log_term,
        entries_count, leader_commit;

    // Lecture des paramètres de base
    if (recv(client_socket, &leader_id, sizeof(int32_t), 0) !=
            sizeof(int32_t) ||
        recv(client_socket, &leader_term, sizeof(int32_t), 0) !=
            sizeof(int32_t) ||
        recv(client_socket, &prev_log_index, sizeof(int32_t), 0) !=
            sizeof(int32_t) ||
        recv(client_socket, &prev_log_term, sizeof(int32_t), 0) !=
            sizeof(int32_t) ||
        recv(client_socket, &entries_count, sizeof(int32_t), 0) !=
            sizeof(int32_t) ||
        recv(client_socket, &leader_commit, sizeof(int32_t), 0) !=
            sizeof(int32_t)) {
      close(client_socket);
      return;
    }

    leader_id = ntohl(leader_id);
    leader_term = ntohl(leader_term);
    prev_log_index = ntohl(prev_log_index);
    prev_log_term = ntohl(prev_log_term);
    entries_count = ntohl(entries_count);
    leader_commit = ntohl(leader_commit);

    log_entry_t *entries = NULL;
    if (entries_count > 0) {
      entries = malloc(sizeof(log_entry_t) * entries_count);
      if (!entries) {
        fprintf(stderr, "[RPC] Échec d'allocation mémoire pour entries\n");
        close(client_socket);
        return;
      }
      for (int i = 0; i < entries_count; i++) {
        // Lecture de chaque entrée (simplifié: read tout le struct ou champ par
        // champ pour portabilité) Pour éviter problèmes de padding/endianness
        // struct, on lit champ par champ
        int32_t idx, term, type, val;

        if (recv(client_socket, &idx, sizeof(int32_t), 0) != sizeof(int32_t) ||
            recv(client_socket, &term, sizeof(int32_t), 0) != sizeof(int32_t) ||
            recv(client_socket, &type, sizeof(int32_t), 0) != sizeof(int32_t) ||
            recv(client_socket, entries[i].key, 32, 0) != 32 ||
            recv(client_socket, &val, sizeof(int32_t), 0) != sizeof(int32_t)) {
          free(entries);
          close(client_socket);
          return;
        }

        entries[i].index = ntohl(idx);
        entries[i].term = ntohl(term);
        entries[i].type = ntohl(type);
        entries[i].value = ntohl(val);
        // Key déjà lue
      }
    }

    int success = node_handle_append_entries(
        node, leader_id, leader_term, prev_log_index, prev_log_term, entries,
        entries_count, leader_commit);

    if (entries)
      free(entries);

    // Envoyer la réponse (1 octet success)
    uint8_t response = success ? 1 : 0;
    send(client_socket, &response, 1, 0);
  } else if (msg == MSG_CLIENT_PUT) {
    char key[32];
    int32_t value;

    // Lire Key
    if (recv(client_socket, key, 32, 0) != 32) {
      close(client_socket);
      return;
    }
    // Lire Value
    if (recv(client_socket, &value, sizeof(int32_t), 0) != sizeof(int32_t)) {
      close(client_socket);
      return;
    }
    value = ntohl(value);

    int leader_id = -1;
    int result = node_handle_client_put(node, key, value, &leader_id);

    uint8_t resp = (uint8_t)result;
    send(client_socket, &resp, 1, 0);

    if (result == MSG_REDIRECT) {
      int32_t l_id = htonl(leader_id);
      send(client_socket, &l_id, sizeof(int32_t), 0);
    }
  } else if (msg == MSG_CLIENT_GET) {
    char key[32];
    if (recv(client_socket, key, 32, 0) != 32) {
      close(client_socket);
      return;
    }

    int found = 0;
    int leader_id = -1;
    int value = node_handle_client_get(node, key, &found, &leader_id);

    if (found) {
      uint8_t resp = MSG_OK;
      send(client_socket, &resp, 1, 0);
      int32_t val_net = htonl(value);
      send(client_socket, &val_net, sizeof(int32_t), 0);
    } else {
      uint8_t resp = MSG_FAIL;
      send(client_socket, &resp, 1, 0);
    }
  } else {
    fprintf(stderr, "[RPC] Message inconnu: %u\n", msg);
  }

  close(client_socket);
}

// Envoie un PONG (1 octet) au client
int rpc_send_pong(int client_socket) {
  uint8_t msg = MSG_PONG;

  if (send(client_socket, &msg, 1, 0) < 0) {
    perror("[RPC] Send failed");
    return -1;
  }

  printf("[RPC] PONG envoyé\n");
  return 0;
}

// Structure pour passer les arguments au thread client
typedef struct {
  int client_socket;
  node_t *node;
} client_thread_args_t;

// Thread wrapper pour gérer un client
void *rpc_client_thread(void *arg) {
  client_thread_args_t *args = (client_thread_args_t *)arg;
  int client_socket = args->client_socket;
  node_t *node = args->node;
  free(args);

  rpc_handle_client(client_socket, node);

  return NULL;
}

// Boucle principale du serveur : accepte les connexions et crée un thread par
// client
void rpc_server_run(int server_socket, node_t *node) {
  struct sockaddr_in client_addr;
  socklen_t client_len = sizeof(client_addr);
  pthread_t thread_id;

  printf("[RPC] Serveur en écoute...\n");

  while (1) {
    // Allocation pour les arguments du thread client
    client_thread_args_t *args = malloc(sizeof(client_thread_args_t));
    if (args == NULL) {
      perror("[RPC] Memory allocation failed");
      continue;
    }

    // Acceptation de la connexion
    args->client_socket =
        accept(server_socket, (struct sockaddr *)&client_addr, &client_len);

    if (args->client_socket < 0) {
      perror("[RPC] Accept failed");
      free(args);
      continue;
    }

    args->node = node;

    // Création du thread pour gérer le client
    if (pthread_create(&thread_id, NULL, rpc_client_thread, args) != 0) {
      perror("[RPC] Thread creation failed");
      close(args->client_socket);
      free(args);
      continue;
    }

    // Détachement du thread (nettoyage automatique)
    pthread_detach(thread_id);
  }
}

// Envoie un PING à un pair et attend le PONG
int rpc_send_ping(const char *host, int port) {
  int client_socket;
  struct sockaddr_in server_addr;
  uint8_t msg;
  ssize_t bytes_received;

  // Création du socket
  client_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (client_socket < 0) {
    perror("[RPC] Socket creation failed");
    return -1;
  }

  // Configuration de l'adresse du serveur
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);

  if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
    perror("[RPC] Invalid address");
    close(client_socket);
    return -1;
  }

  // Connexion au serveur
  if (connect(client_socket, (struct sockaddr *)&server_addr,
              sizeof(server_addr)) < 0) {
    // Ne pas afficher d'erreur ici (normal si le pair n'est pas encore démarré)
    close(client_socket);
    return -1;
  }

  // Envoi du PING
  msg = MSG_PING;
  if (send(client_socket, &msg, 1, 0) < 0) {
    perror("[RPC] Send failed");
    close(client_socket);
    return -1;
  }

  // Attente du PONG
  bytes_received = recv(client_socket, &msg, 1, 0);

  if (bytes_received > 0 && msg == MSG_PONG) {
    printf("[RPC] PING -> %s:%d : OK (PONG reçu)\n", host, port);
    close(client_socket);
    return 0;
  } else {
    fprintf(stderr, "[RPC] PING -> %s:%d : Pas de PONG reçu\n", host, port);
    close(client_socket);
    return -1;
  }
}

// Envoie un REQUEST_VOTE à un pair
int rpc_send_vote_request(const char *host, int port, int candidate_id,
                          int candidate_term, int last_log_index,
                          int last_log_term) {
  int client_socket;
  struct sockaddr_in server_addr;
  uint8_t msg;
  int32_t id_net, term_net, lli_net, llt_net;

  // Création du socket
  client_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (client_socket < 0) {
    perror("[RPC] Socket creation failed");
    return -1;
  }

  // Configuration de l'adresse du serveur
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);

  if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
    perror("[RPC] Invalid address");
    close(client_socket);
    return -1;
  }

  // Connexion au serveur
  if (connect(client_socket, (struct sockaddr *)&server_addr,
              sizeof(server_addr)) < 0) {
    fprintf(stderr, "[RPC] Connexion à %s:%d impossible\n", host, port);
    close(client_socket);
    return -1;
  }

  // Envoi du type de message REQUEST_VOTE
  msg = REQUEST_VOTE;
  if (send(client_socket, &msg, 1, 0) < 0) {
    perror("[RPC] Send failed");
    close(client_socket);
    return -1;
  }

  // Conversion host byte order -> network byte order
  id_net = htonl(candidate_id);
  term_net = htonl(candidate_term);
  lli_net = htonl(last_log_index);
  llt_net = htonl(last_log_term);

  // Envoi du candidate_id
  if (send(client_socket, &id_net, sizeof(int32_t), 0) < 0) {
    perror("[RPC] Send candidate_id failed");
    close(client_socket);
    return -1;
  }

  // Envoi du candidate_term
  if (send(client_socket, &term_net, sizeof(int32_t), 0) < 0) {
    perror("[RPC] Send candidate_term failed");
    close(client_socket);
    return -1;
  }

  // Envoi du last_log_index
  if (send(client_socket, &lli_net, sizeof(int32_t), 0) < 0) {
    perror("[RPC] Send last_log_index failed");
    close(client_socket);
    return -1;
  }

  // Envoi du last_log_term
  if (send(client_socket, &llt_net, sizeof(int32_t), 0) < 0) {
    perror("[RPC] Send last_log_term failed");
    close(client_socket);
    return -1;
  }

  // Attente de la réponse (1 octet : 1 = accordé, 0 = refusé)
  uint8_t response;
  ssize_t bytes_received = recv(client_socket, &response, 1, 0);

  close(client_socket);

  if (bytes_received <= 0) {
    return -1;
  }

  if (response == VOTE_GRANTED) {
    return 1; // Vote accordé
  } else {
    return 0; // Vote refusé
  }
}

// Envoie un HEARTBEAT à un pair
int rpc_send_heartbeat(const char *host, int port, int leader_id,
                       int leader_term) {
  int client_socket;
  struct sockaddr_in server_addr;
  uint8_t msg;
  int32_t id_net, term_net;

  // Création du socket
  client_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (client_socket < 0) {
    return -1; // Erreur silencieuse pour ne pas polluer les logs
  }

  // Configuration de l'adresse du serveur
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);

  if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
    close(client_socket);
    return -1;
  }

  // Connexion au serveur
  if (connect(client_socket, (struct sockaddr *)&server_addr,
              sizeof(server_addr)) < 0) {
    close(client_socket);
    return -1;
  }

  // Envoi du type de message HEARTBEAT
  msg = MSG_HEARTBEAT;
  if (send(client_socket, &msg, 1, 0) < 0) {
    close(client_socket);
    return -1;
  }

  // Conversion host byte order -> network byte order
  id_net = htonl(leader_id);
  term_net = htonl(leader_term);

  // Envoi du leader_id
  if (send(client_socket, &id_net, sizeof(int32_t), 0) < 0) {
    close(client_socket);
    return -1;
  }

  // Envoi du leader_term
  if (send(client_socket, &term_net, sizeof(int32_t), 0) < 0) {
    close(client_socket);
    return -1;
  }

  // Attente de l'ACK (optionnel, on peut ignorer)
  uint8_t ack;
  recv(client_socket, &ack, 1, 0);

  close(client_socket);
  return 0;
}

// Envoie un AppendEntries à un pair
// Retourne 1 si succès (log accepté), 0 si échec (log rejeté/incohérent), -1 si
// erreur réseau
int rpc_send_append_entries(const char *host, int port, int leader_id,
                            int leader_term, int prev_log_index,
                            int prev_log_term, log_entry_t *entries,
                            int entries_count, int leader_commit) {
  int client_socket;
  struct sockaddr_in server_addr;
  uint8_t msg;
  int32_t tmp_net;

  // Création du socket
  client_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (client_socket < 0)
    return -1;

  // Configuration de l'adresse
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
    close(client_socket);
    return -1;
  }

  if (connect(client_socket, (struct sockaddr *)&server_addr,
              sizeof(server_addr)) < 0) {
    close(client_socket);
    return -1;
  }

  // Envoi header
  msg = MSG_APPEND_ENTRIES;
  if (send(client_socket, &msg, 1, 0) < 0) {
    close(client_socket);
    return -1;
  }

  // Envoi params
  tmp_net = htonl(leader_id);
  send(client_socket, &tmp_net, 4, 0);
  tmp_net = htonl(leader_term);
  send(client_socket, &tmp_net, 4, 0);
  tmp_net = htonl(prev_log_index);
  send(client_socket, &tmp_net, 4, 0);
  tmp_net = htonl(prev_log_term);
  send(client_socket, &tmp_net, 4, 0);
  tmp_net = htonl(entries_count);
  send(client_socket, &tmp_net, 4, 0);
  tmp_net = htonl(leader_commit);
  send(client_socket, &tmp_net, 4, 0);

  // Envoi entrées
  for (int i = 0; i < entries_count; i++) {
    tmp_net = htonl(entries[i].index);
    send(client_socket, &tmp_net, 4, 0);
    tmp_net = htonl(entries[i].term);
    send(client_socket, &tmp_net, 4, 0);
    tmp_net = htonl(entries[i].type);
    send(client_socket, &tmp_net, 4, 0);
    send(client_socket, entries[i].key, 32, 0);
    tmp_net = htonl(entries[i].value);
    send(client_socket, &tmp_net, 4, 0);
  }

  // Réception réponse (1 octet : 1=success, 0=fail)
  uint8_t response;
  if (recv(client_socket, &response, 1, 0) <= 0) {
    close(client_socket);
    return -1;
  }

  close(client_socket);
  return response; // 1 ou 0
}

#include "node.h"
#include "rpc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

// Structure pour passer des arguments au thread du serveur
typedef struct {
    int server_socket;
} server_thread_args_t;

// Thread pour exécuter le serveur RPC
static void* server_thread_func(void* arg) {
    server_thread_args_t* args = (server_thread_args_t*)arg;
    rpc_server_run(args->server_socket);
    free(args);
    return NULL;
}

// Initialise un nSud
int node_init(node_t *node, int node_id, const char *config_file) {
    peer_t all_peers[MAX_PEERS];
    int total_peers;

    // Chargement de la configuration
    total_peers = load_config(config_file, all_peers, MAX_PEERS);
    if (total_peers < 0) {
        fprintf(stderr, "[NODE] Échec du chargement de la configuration\n");
        return -1;
    }

    // Recherche de ce nSud dans la configuration
    int found = 0;
    for (int i = 0; i < total_peers; i++) {
        if (all_peers[i].id == node_id) {
            node->node_id = node_id;
            node->port = all_peers[i].port;
            found = 1;
            break;
        }
    }

    if (!found) {
        fprintf(stderr, "[NODE] NSud ID %d introuvable dans la configuration\n", node_id);
        return -1;
    }

    // Allocation des pairs (tous sauf soi-même)
    node->peers = malloc(sizeof(peer_t) * (total_peers - 1));
    node->peer_status = calloc(total_peers - 1, sizeof(int));

    if (!node->peers || !node->peer_status) {
        fprintf(stderr, "[NODE] Échec d'allocation mémoire\n");
        free(node->peers);
        free(node->peer_status);
        return -1;
    }

    // Copie des pairs (en excluant soi-même)
    int peer_idx = 0;
    for (int i = 0; i < total_peers; i++) {
        if (all_peers[i].id != node_id) {
            node->peers[peer_idx] = all_peers[i];
            node->peer_status[peer_idx] = 0; // Initialement indisponible
            peer_idx++;
        }
    }

    node->peer_count = peer_idx;

    printf("[NODE] NSud %d initialisé (port %d, %d pair(s))\n",
           node->node_id, node->port, node->peer_count);

    return 0;
}

// Libère les ressources d'un nSud
void node_cleanup(node_t *node) {
    if (node->peers) {
        free(node->peers);
        node->peers = NULL;
    }
    if (node->peer_status) {
        free(node->peer_status);
        node->peer_status = NULL;
    }
}

// Démarre le serveur RPC du nSud dans un thread séparé
int node_start_server(node_t *node) {
    int server_socket;
    pthread_t server_thread;

    // Démarrage du serveur RPC
    server_socket = rpc_server_start(node->port);
    if (server_socket < 0) {
        fprintf(stderr, "[NODE] Échec du démarrage du serveur RPC\n");
        return -1;
    }

    // Allocation des arguments pour le thread
    server_thread_args_t *args = malloc(sizeof(server_thread_args_t));
    if (!args) {
        fprintf(stderr, "[NODE] Échec d'allocation mémoire pour le thread serveur\n");
        close(server_socket);
        return -1;
    }
    args->server_socket = server_socket;

    // Création du thread serveur
    if (pthread_create(&server_thread, NULL, server_thread_func, args) != 0) {
        fprintf(stderr, "[NODE] Échec de création du thread serveur\n");
        free(args);
        close(server_socket);
        return -1;
    }

    // Détachement du thread (nettoyage automatique)
    pthread_detach(server_thread);

    printf("[NODE] Serveur RPC démarré dans un thread séparé\n");

    return 0;
}

// Envoie un ping à tous les pairs et met à jour leur statut
// Retourne 1 si tous les pairs sont disponibles, 0 sinon
int node_ping_all_peers(node_t *node) {
    int all_available = 1;

    for (int i = 0; i < node->peer_count; i++) {
        // Tentative de ping
        int result = rpc_send_ping(node->peers[i].host, node->peers[i].port);

        if (result == 0) {
            // PONG reçu
            if (node->peer_status[i] == 0) {
                printf("[NODE] Pair %d (%s:%d) est maintenant DISPONIBLE\n",
                       node->peers[i].id, node->peers[i].host, node->peers[i].port);
            }
            node->peer_status[i] = 1;
        } else {
            // Pas de réponse
            node->peer_status[i] = 0;
            all_available = 0;
        }
    }

    return all_available;
}

// Boucle de synchronisation principale
void node_sync_loop(node_t *node) {
    int synchronized = 0;

    printf("[NODE] Démarrage de la boucle de synchronisation...\n");
    printf("[NODE] En attente de tous les pairs...\n");

    while (!synchronized) {
        // Tentative de ping de tous les pairs
        synchronized = node_ping_all_peers(node);

        if (synchronized) {
            printf("\n========================================\n");
            printf("  TOUS LES PAIRS SONT DISPONIBLES\n");
            printf("========================================\n\n");
            break;
        }

        // Affichage du statut
        printf("[NODE] En attente... (");
        int available_count = 0;
        for (int i = 0; i < node->peer_count; i++) {
            if (node->peer_status[i]) {
                available_count++;
            }
        }
        printf("%d/%d pairs disponibles)\n", available_count, node->peer_count);

        // Attente avant la prochaine tentative
        sleep(1);
    }
}

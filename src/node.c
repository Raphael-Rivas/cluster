#include "node.h"
#include "rpc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <time.h>

// Structure pour passer des arguments au thread du serveur
typedef struct {
    int server_socket;
    node_t *node;
} server_thread_args_t;

// Thread pour exécuter le serveur RPC
static void* server_thread_func(void* arg) {
    server_thread_args_t* args = (server_thread_args_t*)arg;
    rpc_server_run(args->server_socket, args->node);
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
        fprintf(stderr, "[NODE] �chec du chargement de la configuration\n");
        return -1;
    }

    // Recherche de ce nSud dans la configuration
    int found = 0;
    for (int i = 0; i < total_peers; i++) {
        if (all_peers[i].id == node_id) {
            node->node_id = node_id;
            node->port = all_peers[i].port;
            node->role = FOLLOWER;
            node->term = 0;
            node->voted_for = -1;
            node->votes_received = 0;

            // Initialisation du timeout d'élection (150-300ms, randomisé)
            srand(time(NULL) + node_id);  // Seed différent pour chaque nœud
            node->election_timeout = 150 + (rand() % 150);  // 150-300ms
            node->last_heartbeat_time = node_get_current_time_ms();
            node->last_heartbeat_sent = 0;  // Pas encore envoyé de heartbeat

            found = 1;
            break;
        }
    }

    if (!found) {
        fprintf(stderr, "[NODE] NSud ID %d introuvable dans la configuration\n", node_id);
        return -1;
    }

    // Allocation des pairs (tous sauf soi-m�me)
    node->peers = malloc(sizeof(peer_t) * (total_peers - 1));
    node->peer_status = calloc(total_peers - 1, sizeof(int));

    if (!node->peers || !node->peer_status) {
        fprintf(stderr, "[NODE] �chec d'allocation m�moire\n");
        free(node->peers);
        free(node->peer_status);
        return -1;
    }

    // Copie des pairs (en excluant soi-m�me)
    int peer_idx = 0;
    for (int i = 0; i < total_peers; i++) {
        if (all_peers[i].id != node_id) {
            node->peers[peer_idx] = all_peers[i];
            node->peer_status[peer_idx] = 0; // Initialement indisponible
            peer_idx++;
        }
    }

    node->peer_count = peer_idx;

    printf("[NODE] NSud %d initialis� (port %d, %d pair(s))\n",
           node->node_id, node->port, node->peer_count);

    return 0;
}

// Lib�re les ressources d'un nSud
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

// D�marre le serveur RPC du nSud dans un thread s�par�
int node_start_server(node_t *node) {
    int server_socket;
    pthread_t server_thread;

    // D�marrage du serveur RPC
    server_socket = rpc_server_start(node->port);
    if (server_socket < 0) {
        fprintf(stderr, "[NODE] �chec du d�marrage du serveur RPC\n");
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
    args->node = node;  // Liaison du node au serveur RPC

    // Création du thread serveur
    if (pthread_create(&server_thread, NULL, server_thread_func, args) != 0) {
        fprintf(stderr, "[NODE] Échec de création du thread serveur\n");
        free(args);
        close(server_socket);
        return -1;
    }

    // D�tachement du thread (nettoyage automatique)
    pthread_detach(server_thread);

    printf("[NODE] Serveur RPC d�marr� dans un thread s�par�\n");

    return 0;
}

// Envoie un ping � tous les pairs et met � jour leur statut
// Retourne 1 si tous les pairs sont disponibles, 0 sinon
int node_ping_all_peers(node_t *node) {
    int all_available = 1;

    for (int i = 0; i < node->peer_count; i++) {
        // Tentative de ping
        int result = rpc_send_ping(node->peers[i].host, node->peers[i].port);

        if (result == 0) {
            // PONG re�u
            if (node->peer_status[i] == 0) {
                printf("[NODE] Pair %d (%s:%d) est maintenant DISPONIBLE\n",
                       node->peers[i].id, node->peers[i].host, node->peers[i].port);
            }
            node->peer_status[i] = 1;
        } else {
            // Pas de r�ponse
            node->peer_status[i] = 0;
            all_available = 0;
        }
    }

    return all_available;
}

// Boucle de synchronisation principale
void node_sync_loop(node_t *node) {
    int synchronized = 0;

    printf("[NODE] D�marrage de la boucle de synchronisation...\n");
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

int node_handle_request_vote(node_t *node, int candidate_id, int candidate_term) {
    // Règle 1 : Si le terme du candidat est inférieur au terme actuel, rejeter
    if (candidate_term < node->term) {
        return 0;  // Vote refusé
    }

    // Règle 2 : Si le terme du candidat est supérieur, mettre à jour le terme local
    if (candidate_term > node->term) {
        node->term = candidate_term;
        node->voted_for = -1;  // Réinitialiser le vote pour le nouveau terme
        node->role = FOLLOWER;  // Redevenir follower
        node_reset_election_timeout(node);  // Réinitialiser le timeout
    }

    // Règle 3 : Un nœud vote pour un seul candidat par terme
    if (node->voted_for == -1 || node->voted_for == candidate_id) {
        // Accorder le vote
        node->voted_for = candidate_id;
        node_reset_election_timeout(node);  // Réinitialiser le timeout car on a vu un candidat
        return 1;  // Vote accordé
    } else {
        // Refuser le vote (déjà voté pour un autre candidat)
        return 0;  // Vote refusé
    }
}

void node_request_votes(node_t *node) {
    // Voter pour soi-même
    node->voted_for = node->node_id;
    node->votes_received = 1;  // Initialiser avec 1 vote (soi-même)

    int nodes_available = 1;  // Compteur de nœuds disponibles (on commence à 1 pour soi-même)

    // Envoyer REQUEST_VOTE à tous les pairs et compter les votes
    for (int i = 0; i < node->peer_count; i++) {
        int result = rpc_send_vote_request(node->peers[i].host, node->peers[i].port,
                                           node->node_id, node->term);

        if (result == 1) {
            // Vote accordé
            node->votes_received++;
            nodes_available++;
            printf("Node %d: reçu vote de %d\n", node->node_id, node->peers[i].id);
        } else if (result == 0) {
            // Vote refusé mais le nœud est disponible
            nodes_available++;
        }
        // Si result == -1, le nœud n'est pas joignable (on ne compte pas)
    }

    // Vérifier si majorité atteinte sur les nœuds DISPONIBLES
    int majority = nodes_available / 2 + 1;  // Majorité simple des nœuds disponibles

    if (node->votes_received >= majority) {
        printf("Node %d: élu leader du terme %d\n", node->node_id, node->term);
        node->role = LEADER;
        node->last_heartbeat_sent = node_get_current_time_ms();  // Initialiser le timestamp
        node_reset_election_timeout(node);  // Réinitialiser pour envoyer des heartbeats
    }
    // Si pas de majorité, on reste CANDIDATE et on retentera au prochain timeout
}

// Gère la réception d'un heartbeat du leader
void node_handle_heartbeat(node_t *node, int leader_id, int leader_term) {
    // Si le terme du leader est inférieur au nôtre, ignorer
    if (leader_term < node->term) {
        return;
    }

    // Si le terme du leader est supérieur, mettre à jour notre terme
    if (leader_term > node->term) {
        node->term = leader_term;
        node->voted_for = -1;
        node->role = FOLLOWER;
    }

    // Si on est CANDIDATE ou FOLLOWER, redevenir/rester FOLLOWER
    if (node->role != LEADER) {
        node->role = FOLLOWER;
    }

    // Réinitialiser le timeout d'élection (on a reçu un message du leader)
    node_reset_election_timeout(node);
}

// Envoie des heartbeats à tous les pairs (appelé par le leader)
void node_send_heartbeats(node_t *node) {
    for (int i = 0; i < node->peer_count; i++) {
        rpc_send_heartbeat(node->peers[i].host, node->peers[i].port,
                          node->node_id, node->term);
    }
}

// Retourne le temps actuel en millisecondes
long node_get_current_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

// Réinitialise le timeout d'élection
void node_reset_election_timeout(node_t *node) {
    node->last_heartbeat_time = node_get_current_time_ms();
    // Randomiser à nouveau le timeout pour éviter les élections simultanées
    node->election_timeout = 150 + (rand() % 150);  // 150-300ms
}

// Fonction tick appelée périodiquement pour gérer les timeouts et élections
void node_tick(node_t *node) {
    long current_time = node_get_current_time_ms();

    // Si on est LEADER, envoyer des heartbeats périodiquement
    if (node->role == LEADER) {
        long elapsed_since_last_heartbeat = current_time - node->last_heartbeat_sent;

        // Envoyer un heartbeat toutes les 100ms (bien avant le timeout d'élection)
        if (elapsed_since_last_heartbeat >= 100) {
            node_send_heartbeats(node);
            node->last_heartbeat_sent = current_time;
        }
    }
    // Si on est FOLLOWER ou CANDIDATE, vérifier le timeout d'élection
    else {
        long elapsed = current_time - node->last_heartbeat_time;

        // Si le timeout est expiré, lancer une élection
        if (elapsed >= node->election_timeout) {
            // Passer en mode CANDIDATE
            node->role = CANDIDATE;
            node->term++;  // Incrémenter le terme
            node->voted_for = -1;  // Réinitialiser le vote
            node->votes_received = 0;  // Réinitialiser les votes reçus

            printf("Node %d: deviens candidat du terme %d\n", node->node_id, node->term);

            // Réinitialiser le timeout pour le prochain round
            node_reset_election_timeout(node);

            // Lancer l'élection
            node_request_votes(node);
        }
    }
}

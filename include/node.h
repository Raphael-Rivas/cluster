#ifndef NODE_H
#define NODE_H

#include "config.h"

#define FOLLOWER  0
#define CANDIDATE 1
#define LEADER    2

// Structure représentant l'état d'un nœud
typedef struct {
    int node_id;              // ID de ce nœud
    int port;                 // Port d'écoute RPC
    peer_t *peers;            // Liste des pairs
    int peer_count;           // Nombre de pairs
    int *peer_status;         // Statut de chaque pair (1=disponible, 0=indisponible)

    int term;                 // terme (cycle de vote) actuel
    int voted_for;            // personne pour qui le node a voté lors de ce terme
    int role;                 // valeur numérique représentant follower, canditate ou leader
    int votes_received;       // nombre de votes reçus lors d'une élection

    // Gestion des timeouts pour les élections
    long last_heartbeat_time; // Timestamp du dernier heartbeat/message du leader (en ms)
    int election_timeout;     // Durée du timeout d'élection (en ms)
    long last_heartbeat_sent; // Timestamp du dernier heartbeat envoyé (pour le leader, en ms)

} node_t;

// Initialise un nœud
int node_init(node_t *node, int node_id, const char *config_file);

// Libère les ressources d'un nœud
void node_cleanup(node_t *node);

// Démarre le serveur RPC du nœud (dans un thread séparé)
int node_start_server(node_t *node);

// Envoie un ping à tous les pairs et attend les réponses
// Retourne 1 si tous les pairs sont disponibles, 0 sinon
int node_ping_all_peers(node_t *node);

// Boucle de synchronisation principale
void node_sync_loop(node_t *node);

// Gestion des requêtes de votes (réception)
// Retourne 1 si le vote est accordé, 0 sinon
int node_handle_request_vote(node_t *node, int candidate_id, int candidate_term);

// Envoi de requêtes de vote à tous les pairs (émission)
void node_request_votes(node_t *node);

// Gestion de la réception d'un heartbeat (follower/candidate)
void node_handle_heartbeat(node_t *node, int leader_id, int leader_term);

// Envoi de heartbeats à tous les pairs (émission par le leader)
void node_send_heartbeats(node_t *node);

// Fonction tick appelée périodiquement pour gérer les timeouts et élections
void node_tick(node_t *node);

// Réinitialise le timeout d'élection (appelé lors de la réception d'un heartbeat)
void node_reset_election_timeout(node_t *node);

// Retourne le temps actuel en millisecondes
long node_get_current_time_ms(void);

#endif

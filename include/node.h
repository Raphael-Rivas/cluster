#ifndef NODE_H
#define NODE_H

#include "config.h"

// Structure représentant l'état d'un nœud
typedef struct {
    int node_id;              // ID de ce nœud
    int port;                 // Port d'écoute RPC
    peer_t *peers;            // Liste des pairs
    int peer_count;           // Nombre de pairs
    int *peer_status;         // Statut de chaque pair (1=disponible, 0=indisponible)
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

#endif

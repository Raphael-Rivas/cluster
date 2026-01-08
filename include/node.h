#ifndef NODE_H
#define NODE_H

#include "config.h"

#define FOLLOWER 0
#define CANDIDATE 1
#define LEADER 2

// Structure d'une entrée de log
typedef struct {
  int index;    // Index de log (1-based)
  int term;     // Terme de l'entrée
  int type;     // 0=NOOP, 1=PUT
  char key[32]; // Clé
  int value;    // Valeur
} log_entry_t;

// Structure pour le Key-Value Store
typedef struct {
  char key[32];
  int value;
} kv_entry_t;

#define MAX_KV_SIZE 100
#define MAX_LOG_SIZE 1000

// Structure représentant l'état d'un nœud
typedef struct {
  int node_id;      // ID de ce nœud
  int port;         // Port d'écoute RPC
  peer_t *peers;    // Liste des pairs
  int peer_count;   // Nombre de pairs
  int *peer_status; // Statut de chaque pair (1=disponible, 0=indisponible)

  int term;      // terme (cycle de vote) actuel
  int voted_for; // personne pour qui le node a voté lors de ce terme
  int role;      // valeur numérique représentant follower, canditate ou leader
  int votes_received; // nombre de votes reçus lors d'une élection

  // Gestion des logs (State Machine Replication)
  log_entry_t
      log[MAX_LOG_SIZE]; // Tableau d'entrées de log (simplifié: taille fixe)
  int log_size;          // Taille actuelle du log (nombre d'entrées)
  int commitIndex;       // Index du plus haut log commité
  int lastApplied;       // Index du plus haut log appliqué

  // Machine à états (Key-Value Store)
  kv_entry_t kv_store[MAX_KV_SIZE];
  int kv_count;

  // État persistant du Leader (réinitialisé à chaque élection)
  int *nextIndex;  // Index du prochain log à envoyer à chaque follower
  int *matchIndex; // Index du plus haut log répliqué sur chaque follower

  // Gestion des timeouts pour les élections
  long last_heartbeat_time; // Timestamp du dernier heartbeat/message du leader
                            // (en ms)
  int election_timeout;     // Durée du timeout d'élection (en ms)
  long last_heartbeat_sent; // Timestamp du dernier heartbeat envoyé (pour le
                            // leader, en ms)

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
int node_handle_request_vote(node_t *node, int candidate_id,
                             int candidate_term);

// Envoi de requêtes de vote à tous les pairs (émission)
void node_request_votes(node_t *node);

// (Heartbeat handled by AppendEntries)

// Fonction tick appelée périodiquement pour gérer les timeouts et élections
void node_tick(node_t *node);

// Réinitialise le timeout d'élection (appelé lors de la réception d'un
// heartbeat)
void node_reset_election_timeout(node_t *node);

// Retourne le temps actuel en millisecondes
long node_get_current_time_ms(void);

// Gère une requête PUT d'un client
// Retourne MSG_OK si succès (Leader), MSG_REDIRECT si follower (avec
// leader_id), MSG_FAIL si erreur
int node_handle_client_put(node_t *node, const char *key, int value,
                           int *leader_id);

// Gère une requête GET d'un client
// Retourne la valeur ou -1 si introuvable. Remplit found=1 si trouvé.
int node_handle_client_get(node_t *node, const char *key, int *found,
                           int *leader_id);

// Gère la réception d'un AppendEntries (Heartbeat ou Réplication)
// Retourne 1 si succès, 0 si échec (term obsolète ou incohérence de log)
int node_handle_append_entries(node_t *node, int leader_id, int leader_term,
                               int prev_log_index, int prev_log_term,
                               log_entry_t *entries, int entries_count,
                               int leader_commit);

#endif

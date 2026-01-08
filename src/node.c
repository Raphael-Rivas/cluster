#include "node.h"
#include "rpc.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

// Structure pour passer des arguments au thread du serveur
typedef struct {
  int server_socket;
  node_t *node;
} server_thread_args_t;

// Thread pour exécuter le serveur RPC
static void *server_thread_func(void *arg) {
  server_thread_args_t *args = (server_thread_args_t *)arg;
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
    fprintf(stderr, "[NODE] Échec du chargement de la configuration\n");
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
      node->log_size = 0;
      node->commitIndex = 0;
      node->lastApplied = 0;
      node->kv_count = 0;

      // Initialisation du timeout d'élection (150-300ms, randomisé)
      srand(time(NULL) + node_id); // Seed différent pour chaque nœud
      node->election_timeout = 150 + (rand() % 150); // 150-300ms
      // Délai de grâce au démarrage : +500ms pour laisser le temps de recevoir
      // un heartbeat du leader existant avant de lancer une élection
      node->last_heartbeat_time = node_get_current_time_ms() + 500;
      node->last_heartbeat_sent = 0; // Pas encore envoyé de heartbeat

      found = 1;
      break;
    }
  }

  if (!found) {
    fprintf(stderr, "[NODE] Nœud ID %d introuvable dans la configuration\n",
            node_id);
    return -1;
  }

  // Allocation des pairs (tous sauf soi-même)
  node->peers = malloc(sizeof(peer_t) * (total_peers - 1));
  node->peer_status = calloc(total_peers - 1, sizeof(int));
  node->nextIndex = calloc(total_peers, sizeof(int));
  node->matchIndex = calloc(total_peers, sizeof(int));

  if (!node->peers || !node->peer_status || !node->nextIndex ||
      !node->matchIndex) {
    fprintf(stderr, "[NODE] Échec d'allocation mémoire\n");
    free(node->peers);
    free(node->peer_status);
    free(node->nextIndex);
    free(node->matchIndex);
    return -1;
  }

  // Copie des pairs (en excluant soi-mme)
  int peer_idx = 0;
  for (int i = 0; i < total_peers; i++) {
    if (all_peers[i].id != node_id) {
      node->peers[peer_idx] = all_peers[i];
      node->peer_status[peer_idx] = 0; // Initialement indisponible
      peer_idx++;
    }
  }

  node->peer_count = peer_idx;

  printf("[NODE] Nœud %d initialisé (port %d, %d pair(s))\n", node->node_id,
         node->port, node->peer_count);

  return 0;
}

// Libre les ressources d'un nSud
void node_cleanup(node_t *node) {
  if (node->peers) {
    free(node->peers);
    node->peers = NULL;
  }
  if (node->peer_status) {
    free(node->peer_status);
    free(node->peer_status);
    node->peer_status = NULL;
  }
  if (node->nextIndex) {
    free(node->nextIndex);
    node->nextIndex = NULL;
  }
  if (node->matchIndex) {
    free(node->matchIndex);
    node->matchIndex = NULL;
  }
}

// Dmarre le serveur RPC du nSud dans un thread spar
int node_start_server(node_t *node) {
  int server_socket;
  pthread_t server_thread;

  // Dmarrage du serveur RPC
  server_socket = rpc_server_start(node->port);
  if (server_socket < 0) {
    fprintf(stderr, "[NODE] Échec du démarrage du serveur RPC\n");
    return -1;
  }

  // Allocation des arguments pour le thread
  server_thread_args_t *args = malloc(sizeof(server_thread_args_t));
  if (!args) {
    fprintf(stderr,
            "[NODE] Échec d'allocation mémoire pour le thread serveur\n");
    close(server_socket);
    return -1;
  }
  args->server_socket = server_socket;
  args->node = node; // Liaison du node au serveur RPC

  // Création du thread serveur
  if (pthread_create(&server_thread, NULL, server_thread_func, args) != 0) {
    fprintf(stderr, "[NODE] Échec de création du thread serveur\n");
    free(args);
    close(server_socket);
    return -1;
  }

  // Dtachement du thread (nettoyage automatique)
  pthread_detach(server_thread);

  printf("[NODE] Serveur RPC démarré dans un thread séparé\n");

  return 0;
}

// --- Gestion KV Store ---

int node_kv_get(node_t *node, const char *key) {
  for (int i = 0; i < node->kv_count; i++) {
    if (strcmp(node->kv_store[i].key, key) == 0) {
      return node->kv_store[i].value;
    }
  }
  return -1; // Introuvable (ou valeur par défaut)
}

void node_kv_put(node_t *node, const char *key, int value) {
  // Mise à jour si existe
  for (int i = 0; i < node->kv_count; i++) {
    if (strcmp(node->kv_store[i].key, key) == 0) {
      node->kv_store[i].value = value;
      return;
    }
  }
  // Ajout si n'existe pas
  if (node->kv_count < MAX_KV_SIZE) {
    strncpy(node->kv_store[node->kv_count].key, key, 31);
    node->kv_store[node->kv_count].key[31] = '\0';
    node->kv_store[node->kv_count].value = value;
    node->kv_count++;
  }
}

// Applique les logs committés à la machine à états (KV Store)
void node_apply_committed_entries(node_t *node) {
  while (node->lastApplied < node->commitIndex) {
    node->lastApplied++;
    log_entry_t *entry =
        &node->log[node->lastApplied -
                   1]; // -1 car log_size vs index 1-based (à vérifier)
    // Note: Si on utilise index 1-based pour Raft, tab[0] est log index 1.
    // On va supposer ici que log[] est 0-indexed mais correspond aux
    // index 1..N. Donc log[i] a pour index i+1.

    if (entry->type == CMD_PUT) {
      printf("[NODE %d] APPLIQUER Log Index %d: PUT %s %d\n", node->node_id,
             node->lastApplied, entry->key, entry->value);
      node_kv_put(node, entry->key, entry->value);
    }
  }
}

// Envoie un ping à tous les pairs et met à jour leur statut
// Retourne 1 si tous les pairs sont disponibles, 0 sinon
int node_ping_all_peers(node_t *node) {
  int all_available = 1;

  for (int i = 0; i < node->peer_count; i++) {
    // Tentative de ping
    int result = rpc_send_ping(node->peers[i].host, node->peers[i].port);

    if (result == 0) {
      // PONG reu
      if (node->peer_status[i] == 0) {
        printf("[NODE] Pair %d (%s:%d) est maintenant DISPONIBLE\n",
               node->peers[i].id, node->peers[i].host, node->peers[i].port);
      }
      node->peer_status[i] = 1;
    } else {
      // Pas de rponse
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

int node_handle_request_vote(node_t *node, int candidate_id, int candidate_term,
                             int last_log_index, int last_log_term) {
  // Règle 1 : Si le terme du candidat est inférieur au terme actuel, rejeter
  if (candidate_term < node->term) {
    return 0; // Vote refusé
  }

  // LEADER STICKINESS (Prévention de coup d'état)
  // Si on a reçu un message du leader actuel récement (moins que le timeout
  // minimum), on ignore les demandes de vote provocatrices (même avec un terme
  // supérieur). Cela empêche un nœud qui vient de rejoindre (et qui n'a pas
  // reçu de logs) de perturber le cluster.
  long elapsed_since_leader =
      node_get_current_time_ms() - node->last_heartbeat_time;
  if (node->role == FOLLOWER && elapsed_since_leader < 150) {
    // 150ms est notre timeout électoral minimum
    // On rejette poliment le vote pour ne pas perturber la stabilité
    return 0;
  }

  // Règle de sécurité Raft (Log Matching Property)
  // Vérifier si le log du candidat est au moins aussi à jour que le nôtre
  int my_last_log_term = node_get_last_log_term(node);
  int my_last_log_index = node_get_last_log_index(node);

  if (last_log_term < my_last_log_term) {
    return 0; // Notre log est plus récent (terme plus élevé)
  }
  if (last_log_term == my_last_log_term && last_log_index < my_last_log_index) {
    return 0; // Mêmes termes, mais notre log est plus long
  }

  // Règle 2 : Si le terme du candidat est supérieur, mettre à jour le terme
  // local
  if (candidate_term > node->term) {
    node->term = candidate_term;
    node->voted_for = -1;  // Réinitialiser le vote pour le nouveau terme
    node->role = FOLLOWER; // Redevenir follower
    node_reset_election_timeout(node); // Réinitialiser le timeout
  }

  // Règle 3 : Un nœud vote pour un seul candidat par terme
  if (node->voted_for == -1 || node->voted_for == candidate_id) {
    // Accorder le vote
    node->voted_for = candidate_id;
    node_reset_election_timeout(
        node); // Réinitialiser le timeout car on a vu un candidat
    return 1;  // Vote accordé
  } else {
    // Refuser le vote (déjà voté pour un autre candidat)
    return 0; // Vote refusé
  }
}

void node_request_votes(node_t *node) {
  // Voter pour soi-même
  node->voted_for = node->node_id;
  node->votes_received = 1; // Initialiser avec 1 vote (soi-même)

  int nodes_available =
      1; // Compteur de nœuds disponibles (on commence à 1 pour soi-même)

  // Envoyer REQUEST_VOTE à tous les pairs et compter les votes
  for (int i = 0; i < node->peer_count; i++) {
    int last_log_index = node_get_last_log_index(node);
    int last_log_term = node_get_last_log_term(node);
    int result = rpc_send_vote_request(node->peers[i].host, node->peers[i].port,
                                       node->node_id, node->term,
                                       last_log_index, last_log_term);

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
  int majority =
      nodes_available / 2 + 1; // Majorité simple des nœuds disponibles

  if (node->votes_received >= majority) {
    printf("Node %d: élu leader du terme %d\n", node->node_id, node->term);
    node->role = LEADER;
    node->last_heartbeat_sent =
        node_get_current_time_ms(); // Initialiser le timestamp
    node_reset_election_timeout(
        node); // Réinitialiser pour envoyer des heartbeats

    // Initialiser nextIndex et matchIndex
    for (int i = 0; i < node->peer_count; i++) {
      node->nextIndex[i] = node->log_size + 1;
      node->matchIndex[i] = 0;
    }
  }
  // Si pas de majorité, on reste CANDIDATE et on retentera au prochain timeout
}

// Retourne l'index du dernier log
int node_get_last_log_index(node_t *node) {
  return node->log_size; // log_size = nombre d'entrées. Index 1-based =
                         // log_size. 0 si vide.
}

// Retourne le terme du dernier log
int node_get_last_log_term(node_t *node) {
  if (node->log_size == 0)
    return 0;
  return node->log[node->log_size - 1].term;
}

// Retourne le terme du log à un index donné
int node_get_log_term(node_t *node, int log_index) {
  if (log_index == 0)
    return 0;
  if (log_index > node->log_size)
    return -1;
  return node->log[log_index - 1].term;
}

// Gère la réception d'un AppendEntries (Heartbeat ou Réplication)
// Retourne 1 si succès, 0 si échec (term obsolète ou incohérence de log)
int node_handle_append_entries(node_t *node, int leader_id, int leader_term,
                               int prev_log_index, int prev_log_term,
                               log_entry_t *entries, int entries_count,
                               int leader_commit) {
  // 1. Si le terme du leader est inférieur au nôtre, refuser
  if (leader_term < node->term) {
    return 0; // Terme obsolète
  }

  // Mise à jour du terme / rôle comme pour un heartbeat
  if (leader_term > node->term) {
    node->term = leader_term;
    node->voted_for = -1;
    node->role = FOLLOWER;
  } else if (node->role != FOLLOWER) {
    node->role = FOLLOWER;
  }

  node_reset_election_timeout(node);

  // 2. Vérifier la consistance du log (PrevLogIndex / PrevLogTerm)
  // Si prev_log_index > 0, on doit avoir un log à cet index avec le même terme
  if (prev_log_index > 0) {
    if (prev_log_index > node->log_size) {
      return 0; // Log manquant
    }
    if (node_get_log_term(node, prev_log_index) != prev_log_term) {
      // Conflit de terme : supprimer ce log et les suivants (simplification
      // brute) Dans une implémentation optimisée, on tronque. Ici, si le terme
      // ne matche pas, on retourne faux, le leader décrémentera nextIndex.
      return 0;
    }
  }

  // 3. Ajouter les nouvelles entrées (si pas déjà présentes)
  // entries_count peut être 0 (heartbeat simple)
  for (int i = 0; i < entries_count; i++) {
    int index = prev_log_index + 1 + i;
    if (index > node->log_size) {
      // Nouveau log
      if (node->log_size < MAX_LOG_SIZE) {
        node->log[node->log_size] = entries[i];
        node->log[node->log_size].index =
            index; // S'assurer que l'index est bon
        node->log_size++;
      }
    } else {
      // Log existant : vérifier conflit
      if (node->log[index - 1].term != entries[i].term) {
        // Conflit : supprimer la fin du log à partir d'ici
        node->log_size = index - 1;
        // Ajouter le nouveau
        if (node->log_size < MAX_LOG_SIZE) {
          node->log[node->log_size] = entries[i];
          node->log[node->log_size].index = index;
          node->log_size++;
        }
      }
    }
  }

  // 4. Mettre à jour commitIndex
  if (leader_commit > node->commitIndex) {
    // commitIndex = min(leaderCommit, index du dernier log (nouvellement
    // ajouté))
    int last_new_entry_index = prev_log_index + entries_count;
    if (leader_commit < last_new_entry_index) {
      node->commitIndex = leader_commit;
    } else {
      node->commitIndex = last_new_entry_index;
    }

    // Appliquer les logs committés à la machine à états
    node_apply_committed_entries(node);
  }

  return 1; // Succès
}

// Envoie des AppendEntries à tous les pairs (appelé par le leader)
void node_send_append_entries(node_t *node) {
  for (int i = 0; i < node->peer_count; i++) {
    int next_idx = node->nextIndex[i];

    // Si nextIndex n'a pas été initialisé correctement (devrait être log_size +
    // 1 au début leader) On va assumer init à 0 ou 1. Si 0, on met à 1.
    if (next_idx < 1)
      next_idx = 1;

    int prev_log_index = next_idx - 1;
    int prev_log_term = node_get_log_term(node, prev_log_index);

    log_entry_t *entries = NULL;
    int entries_count = 0;

    if (node->log_size >= next_idx) {
      entries = &node->log[next_idx - 1];
      entries_count = node->log_size - next_idx + 1;
    }

    // Envoyer RPC
    // Note: rpc_send_append_entries doit être implémenté/modifié pour retourner
    // le succès/matchIndex Ici on envoie juste, la réponse sera traitée
    // asynchronement ou ignorée dans cette version simple Pour un TP simple, on
    // peut faire un send "fire and forget" ou bloquant. Vu rpc.c existant,
    // c'est bloquant (recv wait).

    // TODO: Gérer la réponse de AppendEntries pour update nextIndex/matchIndex
    // Pour l'instant, on envoie, et on assume que rpc.c ne change pas nextIndex
    // automatiquement. On doit modifier rpc_send_append_entries pour retourner
    // le statut.

    // Appel modifié pour passer les arrays
    int success = rpc_send_append_entries(
        node->peers[i].host, node->peers[i].port, node->node_id, node->term,
        prev_log_index, prev_log_term, entries, entries_count,
        node->commitIndex);

    if (success == 1) { // Success
      if (entries_count > 0) {
        node->nextIndex[i] = next_idx + entries_count;
        node->matchIndex[i] = node->nextIndex[i] - 1;
      }
    } else if (success == 0) { // Failed (inconsistency)
      // Decrement nextIndex (optimisation possible, mais -1 est sûr)
      if (node->nextIndex[i] > 1) {
        node->nextIndex[i]--;
      }
    }
    // else (-1) Network error, ignore
  }

  // Mise à jour de commitIndex du leader
  // Si il existe un N tel que N > commitIndex, et une majorité de matchIndex[i]
  // >= N, et log[N].term == currentTerm: set commitIndex = N
  for (int n = node->log_size; n > node->commitIndex; n--) {
    if (node->log[n - 1].term == node->term) {
      int count = 1; // Soi-même
      for (int i = 0; i < node->peer_count; i++) {
        if (node->matchIndex[i] >= n) {
          count++;
        }
      }
      if (count > (node->peer_count + 1) / 2) {
        node->commitIndex = n;
        node_apply_committed_entries(node);
        break;
      }
    }
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
  node->election_timeout = 150 + (rand() % 150); // 150-300ms
}

// Gère une requête PUT d'un client
int node_handle_client_put(node_t *node, const char *key, int value,
                           int *leader_id) {
  if (node->role != LEADER) {
    *leader_id = node->voted_for; // Meilleur effort pour redirection (souvent
                                  // voted_for est le leader actuel ou null)
    // Si on ne sait pas qui est leader, on peut retourner -1 ou une hint.
    // Ici on assume voted_for garde le dernier leader connu ou candidat.
    return MSG_REDIRECT;
  }

  // Leader : Ajouter au log
  log_entry_t entry;
  entry.term = node->term;
  entry.type = CMD_PUT;
  strncpy(entry.key, key, 31);
  entry.key[31] = '\0';
  entry.value = value;

  // Append simple
  if (node->log_size < MAX_LOG_SIZE) {
    // Index sera log_size + 1
    entry.index = node->log_size + 1;
    node->log[node->log_size] = entry;
    node->log_size++;

    printf("[NODE %d] CLIENT PUT %s=%d -> Log Index %d\n", node->node_id, key,
           value, entry.index);

    // Optimisation : Tenter de répliquer immédiatement (optionnel mais bon pour
    // latence) node_send_append_entries(node);

    return MSG_OK;
  } else {
    return MSG_FAIL; // Log plein
  }
}

// Gère une requête GET d'un client
int node_handle_client_get(node_t *node, const char *key, int *found,
                           int *leader_id) {
  // Lecture locale autorisée pour tous (Leader ou Follower) => Cohérence
  // éventuelle Spec : "Si non-Leader -> Renvoie leaderId au client pour
  // redirection." (pour PUT) Spec : "Lecture (GET k) : Client demande valeur de
  // clé k à n'importe quel nœud -> lit dans cache local"

  int val = node_kv_get(node, key);
  if (val != -1) {
    *found = 1;
    return val;
  } else {
    *found = 0;
    return 0;
  }
}

// Fonction tick appelée périodiquement pour gérer les timeouts et élections
void node_tick(node_t *node) {
  long current_time = node_get_current_time_ms();

  // Si on est LEADER, envoyer des heartbeats périodiquement
  if (node->role == LEADER) {
    long elapsed_since_last_heartbeat =
        current_time - node->last_heartbeat_sent;

    // Envoyer des AppendEntries périodiquement (sert de heartbeat si
    // entries_count=0)
    if (elapsed_since_last_heartbeat >= 100) {
      node_send_append_entries(node);
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
      node->term++;             // Incrémenter le terme
      node->voted_for = -1;     // Réinitialiser le vote
      node->votes_received = 0; // Réinitialiser les votes reçus

      printf("Node %d: deviens candidat du terme %d\n", node->node_id,
             node->term);

      // Réinitialiser le timeout pour le prochain round
      node_reset_election_timeout(node);

      // Lancer l'élection
      node_request_votes(node);
    }
  }
}

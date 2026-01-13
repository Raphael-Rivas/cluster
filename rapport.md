# Rapport - Étape 3 : AppendEntries et Table Clé-Valeur Répliquée

**Projet** : Cluster de Nœuds - Système Distribué RAFT  
**Auteurs** : Cyprien BONS, Raphaël RIVAS, Maël NICOLAS, Killian RAMUS  
**Date** : Janvier 2026

---

## Table des matières

1. [Introduction](#introduction)
2. [Architecture générale](#architecture-générale)
3. [Implémentation détaillée](#implémentation-détaillée)
   - [Structure log_entry_t](#1-structure-de-log_entry_t)
   - [Gestion d'AppendEntries (follower)](#2-gestion-dappendentries-follower)
   - [Application au KV Store](#3-application-au-kv-store)
   - [Interface client](#4-interface-client)
4. [Scénarios de test](#scénarios-de-test)
5. [Améliorations et optimisations](#améliorations-et-optimisations)
6. [Conclusion](#conclusion)

---

## Introduction

Ce rapport documente l'implémentation de l'**Étape 3** du projet de cluster distribué basé sur le protocole RAFT. Cette étape ajoute la réplication de log et une table clé-valeur distribuée au système d'élection de leader développé dans les étapes précédentes.

### Objectifs de l'Étape 3

- Implémenter le mécanisme **AppendEntries** pour la réplication de log
- Créer une **table clé-valeur distribuée** avec cohérence éventuelle
- Gérer les opérations **PUT** (écriture) et **GET** (lecture)
- Assurer la **redirection automatique** vers le leader pour les écritures

---

## Architecture générale

Le système repose sur trois composants principaux :

1. **Nœuds RAFT** (`bin/cluster`) : Serveurs qui maintiennent l'état répliqué
2. **Client CLI** (`bin/raftclient`) : Interface pour interagir avec le cluster
3. **Protocole RPC** : Communication TCP entre nœuds et avec les clients

### États d'un nœud

- **FOLLOWER** : État passif, réplique les logs du leader
- **CANDIDATE** : Demande des votes pour devenir leader
- **LEADER** : Coordonne la réplication et traite les écritures

---

## Implémentation détaillée

### 1. Structure de `log_entry_t`

La structure représentant une entrée de log est définie dans [`include/node.h`](file:///home/depinfo/CLionProjects/cluster-1/include/node.h#L11-L17) :

```c
typedef struct {
  int index;    // Index de log (1-based)
  int term;     // Terme de l'entrée
  int type;     // 0=NOOP, 1=PUT
  char key[32]; // Clé
  int value;    // Valeur
} log_entry_t;
```

**Points clés :**

- **`index`** : Strictement croissant, commence à 1 (convention RAFT)
- **`term`** : Terme d'élection lors de la création de l'entrée (pas un compteur d'écritures)
- **`type`** : Distingue les commandes (`CMD_PUT = 1`) des entrées vides (`CMD_NOOP = 0`)
- **`key` et `value`** : Données de la commande PUT

**Validation :**
- Les indices sont vérifiés pour être strictement croissants lors de l'ajout
- Le terme est celui du leader actuel au moment de la création

### 2. Gestion d'AppendEntries (follower)

L'implémentation se trouve dans [`src/node.c`](file:///home/depinfo/CLionProjects/cluster-1/src/node.c#L368-L445) dans la fonction `node_handle_append_entries()`.

#### Règles de sécurité RAFT implémentées

**Règle 1 : Refus si terme obsolète**
```c
if (leader_term < node->term) {
    return 0; // Terme obsolète
}
```

**Règle 2 : Mise à jour du terme**
```c
if (leader_term > node->term) {
    node->term = leader_term;
    node->voted_for = -1;
    node->role = FOLLOWER;
}
```

**Règle 3 : Vérification de la consistance du log**
```c
if (prev_log_index > 0) {
    if (prev_log_index > node->log_size) {
        return 0; // Log manquant
    }
    if (node_get_log_term(node, prev_log_index) != prev_log_term) {
        return 0; // Conflit de terme
    }
}
```

**Règle 4 : Ajout des entrées**
- Les nouvelles entrées sont ajoutées uniquement en fin de log
- En cas de conflit (terme différent), le log est tronqué puis la nouvelle entrée est ajoutée
- Pas de "trous" dans le log : `prev_log_index` doit être `log_size` ou moins

**Règle 5 : Mise à jour du commitIndex**
```c
if (leader_commit > node->commitIndex) {
    int last_new_entry_index = prev_log_index + entries_count;
    node->commitIndex = min(leader_commit, last_new_entry_index);
    node_apply_committed_entries(node);
}
```

### 3. Application au KV Store

La fonction `node_apply_committed_entries()` dans [`src/node.c`](file:///home/depinfo/CLionProjects/cluster-1/src/node.c#L192-L208) applique les logs committés :

```c
void node_apply_committed_entries(node_t *node) {
  while (node->lastApplied < node->commitIndex) {
    node->lastApplied++;
    log_entry_t *entry = &node->log[node->lastApplied - 1];

    if (entry->type == CMD_PUT) {
      printf("[NODE %d] APPLIQUER Log Index %d: PUT %s %d\n", 
             node->node_id, node->lastApplied, entry->key, entry->value);
      node_kv_put(node, entry->key, entry->value);
    }
  }
}
```

**Points clés :**

- **Condition** : `lastApplied < commitIndex` garantit qu'on applique uniquement les entrées committées
- **Ordre strict** : Les entrées sont appliquées dans l'ordre des indices
- **Idempotence** : `lastApplied` est incrémenté pour éviter les applications multiples
- **Sémantique RAFT** : Seules les entrées committed modifient la table clé-valeur

**Implémentation du KV Store :**
```c
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
    node->kv_store[node->kv_count].value = value;
    node->kv_count++;
  }
}
```

### 4. Interface client

#### PUT (Écriture)

Implémentation dans [`src/node.c`](file:///home/depinfo/CLionProjects/cluster-1/src/node.c#L534-L570) :

```c
int node_handle_client_put(node_t *node, const char *key, int value, int *leader_id) {
  if (node->role != LEADER) {
    *leader_id = node->voted_for; // Redirection
    return MSG_REDIRECT;
  }

  // Leader : Ajouter au log
  log_entry_t entry;
  entry.term = node->term;
  entry.type = CMD_PUT;
  strncpy(entry.key, key, 31);
  entry.value = value;

  if (node->log_size < MAX_LOG_SIZE) {
    entry.index = node->log_size + 1;
    node->log[node->log_size] = entry;
    node->log_size++;
    return MSG_OK;
  }
  return MSG_FAIL;
}
```

**Comportement :**
- **Si non-leader** : Retourne `MSG_REDIRECT` avec l'ID du leader connu
- **Si leader** : Ajoute l'entrée au log local et retourne `MSG_OK`
- La réplication vers les followers se fait via `node_send_append_entries()` appelé périodiquement

#### GET (Lecture)

Implémentation dans [`src/node.c`](file:///home/depinfo/CLionProjects/cluster-1/src/node.c#L573-L588) :

```c
int node_handle_client_get(node_t *node, const char *key, int *found, int *leader_id) {
  int val = node_kv_get(node, key);
  if (val != -1) {
    *found = 1;
    return val;
  } else {
    *found = 0;
    return 0;
  }
}
```

**Comportement :**
- Lecture locale sur **n'importe quel nœud** (leader ou follower)
- Cohérence éventuelle : un follower peut lire une valeur légèrement obsolète
- Pas de redirection pour les lectures (optimisation de performance)

#### Client CLI

Le client [`src/client.c`](file:///home/depinfo/CLionProjects/cluster-1/src/client.c) envoie les commandes via RPC :

```bash
# Écriture
./bin/raftclient 127.0.0.1 9000 PUT maclé 42

# Lecture
./bin/raftclient 127.0.0.1 9000 GET maclé
```

**Réponses possibles :**
- `OK` : Écriture réussie
- `REDIRECT Leader: X` : Redirection vers le nœud X
- `42` : Valeur lue (pour GET)
- `NOT_FOUND` : Clé inexistante

---

## Scénarios de test

### Test 1 : Cluster basique, aucune panne

**Commandes :**
```bash
# Terminal 1
make run1

# Terminal 2
make run2

# Terminal 3
make run3

# Attendre l'élection (5s)

# Terminal 4 - Client
./bin/raftclient 127.0.0.1 9000 PUT a 10
./bin/raftclient 127.0.0.1 9000 PUT b 20
./bin/raftclient 127.0.0.1 9000 GET a
```

**Résultat attendu :**
- Le PUT retourne `OK`
- Le GET retourne `10`
- Après quelques instants, les followers ont aussi `a = 10, b = 20`

**Résultat obtenu :**
✅ Le leader traite correctement les PUT  
✅ Les followers reçoivent les AppendEntries  
⚠️ Délai de réplication observable (cohérence éventuelle)

### Test 2 : Lecture sur un follower

**Commandes :**
```bash
# PUT sur le leader
./bin/raftclient 127.0.0.1 9000 PUT test 123

# GET immédiat sur un follower
./bin/raftclient 127.0.0.1 9001 GET test
```

**Résultat attendu :**
- La valeur peut être `NOT_FOUND` ou `123` selon le timing
- Illustre la cohérence éventuelle

**Résultat obtenu :**
✅ Comportement conforme : lecture locale sans garantie de fraîcheur

### Test 3 : Redirection vers le leader

**Commandes :**
```bash
# PUT sur un follower
./bin/raftclient 127.0.0.1 9001 PUT key 456
```

**Résultat attendu :**
```
REDIRECT Leader: 0
```

**Résultat obtenu :**
✅ Redirection correcte vers le leader

---

## Améliorations et optimisations

### Améliorations implémentées

1. **Leader Stickiness**
   - Prévention des coups d'état par de nouveaux nœuds
   - Refus de vote si un leader valide a été entendu récemment (<150ms)

2. **Log Matching Property**
   - Vérification que le log du candidat est au moins aussi à jour que le votant
   - Empêche l'élection de leaders avec des logs incomplets

3. **Parsing robuste de `nodes.conf`**
   - Gestion des lignes vides et commentaires
   - Utilisation de `fgets` + `sscanf` au lieu de `fscanf` seul

### Optimisations possibles (non implémentées)

1. **Snapshots** : Compaction du log pour éviter une croissance infinie
2. **Batching** : Regrouper plusieurs entrées dans un seul AppendEntries
3. **Pipeline** : Envoyer plusieurs AppendEntries sans attendre les réponses
4. **Read Index** : Garantir la fraîcheur des lectures sans passer par le log

---

## Conclusion

L'implémentation de l'Étape 3 fournit un système de réplication de log fonctionnel basé sur RAFT, avec une table clé-valeur distribuée. Les points clés respectés sont :

✅ **Structure de log** : `log_entry_t` avec index, term, type, key, value  
✅ **AppendEntries** : Gestion correcte des termes, consistance du log, commit  
✅ **Application au KV** : Respect de `lastApplied < commitIndex`, ordre strict  
✅ **Interface client** : PUT via leader, GET local, redirection automatique  

Le système est stable pour des scénarios simples (3 nœuds, pas de pannes). Des améliorations futures (snapshots, optimisations de performance) permettraient de le rendre production-ready.

### Fichiers principaux

- [`src/node.c`](file:///home/depinfo/CLionProjects/cluster-1/src/node.c) : Logique RAFT complète
- [`src/rpc.c`](file:///home/depinfo/CLionProjects/cluster-1/src/rpc.c) : Protocole RPC et AppendEntries
- [`src/client.c`](file:///home/depinfo/CLionProjects/cluster-1/src/client.c) : Client CLI
- [`include/node.h`](file:///home/depinfo/CLionProjects/cluster-1/include/node.h) : Structures et prototypes

### Tests de validation

Un script de test automatisé [`test_phase3.sh`](file:///home/depinfo/CLionProjects/cluster-1/test_phase3.sh) valide :
- L'élection du leader
- Les opérations PUT/GET
- La redirection vers le leader
- La réplication (avec délai observable)

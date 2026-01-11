# Cluster - Système Distribué RAFT

Implémentation d'un système distribué avec protocole de consensus et d'élection de leader basé sur RAFT.

## Description

Ce projet implémente un cluster de nœuds capables de communiquer entre eux via RPC (Remote Procedure Call) sur TCP, d'élire un leader de manière distribuée, et de répliquer une table clé-valeur sur tous les nœuds.

### Étapes implémentées

- ✅ **Étape 1** : Infrastructure RPC, ping et synchronisation
- ✅ **Étape 2** : Protocole d'élection de leader (RAFT simplifié)
- ✅ **Étape 3** : AppendEntries et table clé-valeur répliquée

### Fonctionnalités

- **Infrastructure RPC** : Serveur TCP multi-thread avec gestion des connexions
- **Élection de leader** : Mécanisme d'élection basé sur le protocole RAFT
  - Timeouts randomisés (150-300ms)
  - Gestion des états : FOLLOWER, CANDIDATE, LEADER
  - Système de votes avec majorité simple
  - Gestion des termes (rounds d'élection)
  - Leader Stickiness (protection contre les coups d'état)
- **Réplication de log** : AppendEntries avec gestion de la consistance
  - Log Matching Property vérifié
  - Gestion des `nextIndex` et `matchIndex`
  - Commit sur majorité
- **Table clé-valeur distribuée** : KV Store répliqué
  - PUT : Écriture via le leader avec réplication
  - GET : Lecture locale sur n'importe quel nœud
  - Redirection automatique vers le leader
- **Client CLI** : Outil `raftclient` pour interagir avec le cluster

## Compilation et Utilisation

### Prérequis
- Compilateur GCC (MinGW sur Windows)
- Bibliothèque pthread
- Make (ou mingw32-make sur Windows)

### Configuration du cluster

Le fichier `src/nodes.conf` contient la configuration de tous les nœuds du cluster :

```
0 127.0.0.1 9000
1 127.0.0.1 9001
2 127.0.0.1 9002
...
9 127.0.0.1 9009
```

Format : `<node_id> <host> <port>`

### Utilisation

#### Compilation

```bash
make build
```

#### Lancer le cluster

Lancez autant de terminaux que de nœuds définis dans `nodes.conf` :

**Terminal 1 :**
```bash
make run1  # Lance le noeud 0
```

**Terminal 2 :**
```bash
make run2  # Lance le noeud 1
```

**Terminal 3 :**
```bash
make run3  # Lance le noeud 2
```
 
Vous pouvez continuer ainsi : `make run4`, `make run5`, etc.

#### Commandes disponibles

```bash
make help      # Affiche l'aide
make build     # Compile le projet (cluster + client)
make re        # Recompile tout
make clean     # Nettoie les fichiers objets
make fclean    # Nettoie tout (objets + exécutables)
make deploy    # Push vers etape3 et development
```

#### Utiliser le client CLI

Une fois le cluster lancé et un leader élu, vous pouvez utiliser le client :

```bash
# Écrire une valeur (PUT)
./bin/raftclient 127.0.0.1 9000 PUT maclé 42

# Lire une valeur (GET)
./bin/raftclient 127.0.0.1 9000 GET maclé
```

**Réponses possibles :**
- `OK` : Écriture réussie
- `REDIRECT Leader: X` : Le nœud contacté n'est pas le leader, contacter le nœud X
- `42` : Valeur lue (pour GET)
- `NOT_FOUND` : Clé inexistante (pour GET)

## Tests et Scénarios

### Test 1 : Élection simple

Lancez les 3 nœuds dans l'ordre. Le premier à expirer son timeout devient candidat et devrait être élu.

**Résultat attendu :**
```
Node 1: deviens candidat du terme 1
Node 1: reçu vote de 2
Node 1: élu leader du terme 1
```

### Test 2 : Conflit d'élection

Lancez les nœuds presque simultanément. Plusieurs peuvent devenir candidats mais un seul sera élu.

**Résultat attendu :**
```
Node 0: deviens candidat du terme 1
Node 1: deviens candidat du terme 1
Node 1: reçu vote de 2
Node 1: élu leader du terme 1
```

### Test 3 : Mort du leader

1. Lancez les 3 nœuds
2. Identifiez le leader
3. Tuez le processus (Ctrl+C)
4. Observez une nouvelle élection avec un terme incrémenté

**Résultat attendu :**
```
Node 2: deviens candidat du terme 2
Node 2: reçu vote de 0
Node 2: élu leader du terme 2
```

### Test 4 : Réplication de données (Étape 3)

1. Lancez les 3 nœuds
2. Attendez l'élection du leader
3. Envoyez une commande PUT au leader :
   ```bash
   ./bin/raftclient 127.0.0.1 9000 PUT test 123
   ```
4. Vérifiez la valeur sur un follower :
   ```bash
   ./bin/raftclient 127.0.0.1 9001 GET test
   ```

**Résultat attendu :**
- Le PUT retourne `OK`
- Le GET retourne `123` (après réplication)

### Test 5 : Redirection vers le leader

1. Envoyez un PUT à un follower :
   ```bash
   ./bin/raftclient 127.0.0.1 9001 PUT key 456
   ```

**Résultat attendu :**
```
REDIRECT Leader: 0
```

## Architecture

### Structure du projet

```
cluster/
├── include/
│   ├── config.h      # Gestion de la configuration
│   ├── node.h        # Structure et fonctions du nœud
│   └── rpc.h         # Protocole RPC
├── src/
│   ├── config.c      # Chargement de nodes.conf
│   ├── node.c        # Logique RAFT (états, élections, votes, réplication)
│   ├── rpc.c         # Communications TCP/RPC
│   ├── client.c      # Client CLI (raftclient)
│   ├── main.c        # Boucle principale avec node_tick()
│   └── nodes.conf    # Configuration du cluster
├── bin/
│   ├── cluster       # Exécutable du nœud
│   └── raftclient    # Client CLI
├── Makefile
└── README.md
```

### États d'un nœud (RAFT)

- **FOLLOWER** (0) : État initial, passif, accepte les requêtes
- **CANDIDATE** (1) : Demande des votes pour devenir leader
- **LEADER** (2) : Coordonne le cluster

### Protocole RPC

Les messages échangés :

| Code | Message | Description |
|------|---------|-------------|
| 1 | `MSG_PING` / `MSG_PONG` | Test de connectivité |
| 2 | `REQUEST_VOTE` | Demande de vote (+ candidate_id, term, last_log_index, last_log_term) |
| 5 | `MSG_APPEND_ENTRIES` | Heartbeat ou réplication de log |
| 10 | `MSG_CLIENT_PUT` | Écriture clé-valeur |
| 11 | `MSG_CLIENT_GET` | Lecture clé-valeur |
| 12 | `MSG_REDIRECT` | Redirection vers le leader |
| 13 | `MSG_OK` | Succès |
| 14 | `MSG_FAIL` | Échec |

## Implémentation technique

### Fonctions principales

- `node_tick()` : Appelée toutes les 50ms, gère les timeouts et lance les élections
- `node_request_votes()` : Envoie des REQUEST_VOTE à tous les pairs
- `node_handle_request_vote()` : Traite une demande de vote selon les règles RAFT
- `node_handle_append_entries()` : Traite un AppendEntries (heartbeat ou réplication)
- `node_send_append_entries()` : Envoie les logs aux followers (leader uniquement)
- `node_apply_committed_entries()` : Applique les logs committés au KV Store
- `node_handle_client_put()` : Traite une requête PUT client
- `node_handle_client_get()` : Traite une requête GET client

### Règles d'élection (RAFT)

1. **Timeout expiré** → Passage en CANDIDATE, incrémentation du terme
2. **Vote pour soi-même** → `votes_received = 1`
3. **Envoi de REQUEST_VOTE** → À tous les pairs (avec last_log_index et last_log_term)
4. **Majorité atteinte** → Devenir LEADER
5. **Terme supérieur détecté** → Redevenir FOLLOWER
6. **Log Matching Property** → Vote refusé si le log du candidat n'est pas à jour

### Réplication de log (RAFT)

1. **Client envoie PUT** → Leader ajoute au log local
2. **Leader envoie AppendEntries** → À tous les followers
3. **Followers répondent** → Succès ou échec (incohérence de log)
4. **Majorité confirme** → Entry devient committed
5. **Application au KV Store** → `lastApplied` incrémenté

## État du développement

### ✅ Fonctionnalités implémentées

#### Étape 1 - Infrastructure
- [x] Infrastructure RPC avec serveur TCP multi-thread
- [x] Système de ping/pong pour synchronisation
- [x] Chargement de la configuration depuis `nodes.conf`

#### Étape 2 - Élection
- [x] Gestion des états FOLLOWER/CANDIDATE/LEADER
- [x] Fonction `node_tick()` avec timeouts randomisés (150-300ms)
- [x] Envoi et réception de REQUEST_VOTE
- [x] Comptage des votes dans `votes_received`
- [x] Détection de la majorité et élection du leader
- [x] Gestion des termes et prévention des conflits
- [x] Leader Stickiness (protection contre les coups d'état)

#### Étape 3 - Réplication
- [x] Heartbeats du leader vers les followers (100ms)
- [x] Réplication de log (AppendEntries avec payload)
- [x] Gestion de `nextIndex[]` et `matchIndex[]`
- [x] Commit sur majorité avec mise à jour de `commitIndex`
- [x] Application des logs committés au KV Store
- [x] Table clé-valeur distribuée (PUT/GET)
- [x] Client CLI (`raftclient`) avec redirection leader

## Auteurs

- Cyprien BONS
- Raphaël RIVAS
- Maël NICOLAS
- Killian RAMUS
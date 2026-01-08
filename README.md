# Cluster - Système Distribué

Implémentation d'un système distribué avec protocole d'élection de leader inspiré de RAFT.

## Description

Ce projet implémente un cluster de nœuds capables de communiquer entre eux via RPC (Remote Procedure Call) sur TCP et d'élire un leader de manière distribuée.

### Étapes implémentées

- ✅ **Étape 1** : Infrastructure RPC, ping et synchronisation
- ✅ **Étape 2** : Protocole d'élection de leader (RAFT simplifié)

### Fonctionnalités

- **Infrastructure RPC** : Serveur TCP multi-thread avec gestion des connexions
- **Élection de leader** : Mécanisme d'élection basé sur le protocole RAFT
  - Timeouts randomisés (150-300ms)
  - Gestion des états : FOLLOWER, CANDIDATE, LEADER
  - Système de votes avec majorité simple
  - Gestion des termes (rounds d'élection)
- **Système de votes** : Comptage des votes reçus dans `votes_received`
- **Détection automatique** : Fonction `node_tick()` pour surveiller les timeouts

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
```

Format : `<node_id> <host> <port>`

### Utilisation

#### Compilation

```bash
make build
```

#### Lancer le cluster

Ouvrez **3 terminaux** et lancez un nœud dans chacun :

**Terminal 1 :**
```bash
make run1
# ou directement: ./bin/cluster 0
```

**Terminal 2 :**
```bash
make run2
# ou directement: ./bin/cluster 1
```

**Terminal 3 :**
```bash
make run3
# ou directement: ./bin/cluster 2
```

#### Commandes disponibles

```bash
make help      # Affiche l'aide
make build     # Compile le projet
make re        # Recompile tout
make clean     # Nettoie les fichiers objets
make fclean    # Nettoie tout (objets + exécutable)
```

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
│   ├── node.c        # Logique RAFT (états, élections, votes)
│   ├── rpc.c         # Communications TCP/RPC
│   ├── main.c        # Boucle principale avec node_tick()
│   └── nodes.conf    # Configuration du cluster
├── Makefile
└── README.md
```

### États d'un nœud (RAFT)

- **FOLLOWER** (0) : État initial, passif, accepte les requêtes
- **CANDIDATE** (1) : Demande des votes pour devenir leader
- **LEADER** (2) : Coordonne le cluster

### Protocole RPC

Les messages échangés :
- `MSG_PING` (1) : Test de connectivité
- `MSG_PONG` (1) : Réponse au ping
- `REQUEST_VOTE` (2) : Demande de vote (+ candidate_id + term)
- `VOTE_GRANTED` (1) : Vote accordé
- `VOTE_DENIED` (0) : Vote refusé

## Implémentation technique

### Fonctions principales

- `node_tick()` : Appelée toutes les 50ms, gère les timeouts et lance les élections
- `node_request_votes()` : Envoie des REQUEST_VOTE à tous les pairs
- `node_handle_request_vote()` : Traite une demande de vote selon les règles RAFT
- `node_reset_election_timeout()` : Réinitialise le timeout avec randomisation

### Règles d'élection (RAFT)

1. **Timeout expiré** → Passage en CANDIDATE, incrémentation du terme
2. **Vote pour soi-même** → `votes_received = 1`
3. **Envoi de REQUEST_VOTE** → À tous les pairs
4. **Majorité atteinte** → Devenir LEADER
5. **Terme supérieur détecté** → Redevenir FOLLOWER

## État du développement

### ✅ Fonctionnalités implémentées

- [x] Infrastructure RPC avec serveur TCP multi-thread
- [x] Système de ping/pong pour synchronisation
- [x] Gestion des états FOLLOWER/CANDIDATE/LEADER
- [x] Fonction `node_tick()` avec timeouts randomisés
- [x] Envoi et réception de REQUEST_VOTE
- [x] Comptage des votes dans `votes_received`
- [x] Détection de la majorité et élection du leader
- [x] Gestion des termes et prévention des conflits
- [x] Réinitialisation des timeouts lors de réception de votes

### 🔄 À venir

- [ ] Heartbeats du leader vers les followers
- [ ] Réplication de log
- [ ] Gestion de la persistance
- [ ] Tests unitaires complets 

## Auteurs

- Cyprien BONS
- Raphaël RIVAS
- Maël NICOLAS
- Killian RAMUS
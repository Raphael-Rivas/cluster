# Cluster - Systeme Distribue

## 1. Explication du Projet et Objectifs

### Vue d'ensemble
Ce projet implemente un **systeme de cluster distribue** permettant a plusieurs noeuds de communiquer entre eux via un protocole RPC (Remote Procedure Call) simplifie. Il s'agit d'un systeme de synchronisation et de detection de disponibilite entre pairs.

### Objectifs principaux
- **Communication inter-noeuds** : Permettre a plusieurs processus (noeuds) de communiquer entre eux sur un reseau
- **Synchronisation** : Assurer que tous les noeuds du cluster sont operationnels avant de commencer les operations
- **Detection de disponibilite** : Utiliser un mecanisme PING/PONG pour verifier qu'un noeud est accessible
- **Architecture client-serveur** : Chaque noeud est a la fois serveur (ecoute les requetes) et client (envoie des requetes)

### Fonctionnalites
- Demarrage d'un serveur RPC multi-threade sur chaque noeud
- Communication TCP/IP entre noeuds
- Protocole PING/PONG pour verifier la disponibilite
- Boucle de synchronisation attendant que tous les pairs soient disponibles
- Configuration centralisee via fichier `nodes.conf`

---

## 2. Compilation et Utilisation

### Prerequis
- Compilateur GCC
- Bibliotheque pthread (incluse avec GCC sur Linux)
- Make

### Compilation

```bash
# Compilation du projet
make

# Recompiler completement
make re

# Nettoyer les fichiers objets
make clean

# Nettoyer tout (objets + executable)
make fclean
```

### Configuration du cluster

Le fichier `src/nodes.conf` contient la configuration de tous les noeuds du cluster :

```
0 127.0.0.1 9000
1 127.0.0.1 9001
2 127.0.0.1 9002
```

Format : `<node_id> <host> <port>`

### Utilisation

#### Lancement manuel de chaque noeud

Ouvrez **3 terminaux differents** et lancez :

```bash
  a remplir
```

## 3. Explication Detaillee du Projet

### Architecture globale

```
+-----------------------------------------------------+
|                   CLUSTER SYSTEM                     |
|                                                      |
|  +----------+       +----------+       +----------+ |
|  |  Node 0  |<----->|  Node 1  |<----->|  Node 2  | |
|  | Port:9000|  RPC  | Port:9001|  RPC  | Port:9002| |
|  +----------+       +----------+       +----------+ |
|       |                  |                  |       |
|       +------------------+------------------+       |
|              Synchronisation PING/PONG              |
+-----------------------------------------------------+
```

### Structure du code

```
cluster/
├── src/
│   ├── main.c       # Point d'entree du programme
│   ├── node.c       # Gestion des noeuds et synchronisation
│   ├── rpc.c        # Serveur/Client RPC et communication
│   ├── config.c     # Chargement de la configuration
│   └── nodes.conf   # Fichier de configuration du cluster
├── include/
│   ├── node.h       # Definitions pour les noeuds
│   ├── rpc.h        # Definitions pour le RPC
│   └── config.h     # Definitions pour la configuration
├── obj/             # Fichiers objets (.o)
├── bin/             # Executable final
└── Makefile         # Automatisation de la compilation
```

### Description des modules

#### 1. **config.c / config.h** - Chargement de la configuration

**Role** : Lit le fichier `nodes.conf` et charge la liste de tous les noeuds du cluster.

**Structures principales** :
```c
typedef struct {
    int id;                    // ID unique du noeud
    char host[MAX_HOST_LEN];   // Adresse IP
    int port;                  // Port d'ecoute
} peer_t;
```

**Fonction cle** :
- `load_config()` : Parse le fichier de configuration et retourne un tableau de peers

#### 2. **rpc.c / rpc.h** - Communication RPC

**Role** : Implemente le protocole de communication entre noeuds.

**Protocole** : Simple echange de 1 octet
- `MSG_PING (1)` : Message envoye pour tester la disponibilite
- `MSG_PONG (1)` : Reponse positive a un PING

**Fonctions principales** :

- **Serveur** :
  - `rpc_server_start(port)` : Cree et bind un socket TCP sur le port specifie
  - `rpc_server_run(socket)` : Boucle infinie acceptant les connexions entrantes
  - `rpc_handle_client(socket)` : Traite une connexion : lit PING, envoie PONG
  - `rpc_client_thread(arg)` : Wrapper pour gerer chaque client dans un thread separe

- **Client** :
  - `rpc_send_ping(host, port)` : Se connecte a un pair, envoie PING, attend PONG

**Architecture multi-thread** :
- Un thread principal pour le serveur (accept loop)
- Un thread par connexion client (traitement PING/PONG)

#### 3. **node.c / node.h** - Gestion des noeuds

**Role** : Orchestration de la logique metier d'un noeud.

**Structure principale** :
```c
typedef struct {
    int node_id;           // ID de ce noeud
    int port;              // Port d'ecoute RPC
    peer_t *peers;         // Liste des autres noeuds (sans soi-meme)
    int peer_count;        // Nombre de pairs
    int *peer_status;      // Statut de chaque pair (1=disponible, 0=indisponible)
} node_t;
```

**Fonctions principales** :

- `node_init()` :
  1. Charge la configuration complete
  2. Trouve les informations de ce noeud (port, etc.)
  3. Alloue la liste des pairs (tous sauf soi-meme)
  4. Initialise le statut de chaque pair a 0 (indisponible)

- `node_start_server()` :
  1. Demarre le serveur RPC sur le port du noeud
  2. Lance le serveur dans un thread detache

- `node_ping_all_peers()` :
  1. Envoie un PING a chaque pair
  2. Met a jour le statut (1 si PONG recu, 0 sinon)
  3. Retourne 1 si tous disponibles, 0 sinon

- `node_sync_loop()` : **Boucle de synchronisation**
  1. Appelle `node_ping_all_peers()` en boucle
  2. Attend 1 seconde entre chaque tentative
  3. Affiche le nombre de pairs disponibles
  4. Sort quand tous les pairs sont disponibles

- `node_cleanup()` : Libere la memoire allouee

#### 4. **main.c** - Point d'entree

**Role** : Initialise et demarre un noeud.

**Sequence d'execution** :
1. Affiche le banner
2. Parse l'argument `node_id`
3. Initialise le noeud (`node_init`)
4. Demarre le serveur RPC (`node_start_server`)
5. Lance la synchronisation (`node_sync_loop`)
6. Une fois synchronise, entre dans une boucle infinie

### Flux de communication

#### Scenario : Demarrage de 3 noeuds

```
T=0s : Node 0 demarre
       |-- Serveur RPC ecoute sur port 9000
       +-- Envoie PING vers Node 1 et 2 -> ECHEC (pas encore demarres)

T=1s : Node 0 retry
       +-- Envoie PING vers Node 1 et 2 -> ECHEC

T=2s : Node 1 demarre
       |-- Serveur RPC ecoute sur port 9001
       +-- Envoie PING vers Node 0 -> SUCCES (PONG recu)
       +-- Envoie PING vers Node 2 -> ECHEC

T=3s : Node 0 retry
       +-- Envoie PING vers Node 1 -> SUCCES (PONG recu)
       +-- Envoie PING vers Node 2 -> ECHEC

T=4s : Node 2 demarre
       |-- Serveur RPC ecoute sur port 9002
       +-- Envoie PING vers Node 0 -> SUCCES
       +-- Envoie PING vers Node 1 -> SUCCES
       +-- TOUS DISPONIBLES -> Synchronisation terminee

T=5s : Node 0 retry
       +-- Envoie PING vers Node 1 -> SUCCES
       +-- Envoie PING vers Node 2 -> SUCCES
       +-- TOUS DISPONIBLES -> Synchronisation terminee

T=6s : Node 1 retry
       +-- Envoie PING vers Node 0 -> SUCCES
       +-- Envoie PING vers Node 2 -> SUCCES
       +-- TOUS DISPONIBLES -> Synchronisation terminee
```

### Gestion des threads

Le programme utilise **pthread** pour le parallelisme :

1. **Thread principal** (main) :
   - Initialisation
   - Synchronisation (boucle PING)
   - Boucle infinie finale

2. **Thread serveur RPC** (detache) :
   - Ecoute et accepte les connexions
   - Cree un nouveau thread par client

3. **Threads clients RPC** (detaches) :
   - Un par connexion entrante
   - Lit PING, envoie PONG
   - Se termine apres reponse

### Mecanisme de synchronisation

**Objectif** : S'assurer que tous les noeuds sont demarres avant de continuer.

**Algorithme** :
```
tant que (tous les pairs ne sont pas disponibles) :
    pour chaque pair :
        envoyer PING
        si PONG recu :
            marquer pair comme disponible
        sinon :
            marquer pair comme indisponible

    si tous disponibles :
        sortir de la boucle

    attendre 1 seconde
```

### Protocole de communication

#### Format des messages

| Byte | Description |
|------|-------------|
| 0x01 | PING ou PONG |

**Sequence PING/PONG** :
```
Client                          Serveur
  |                                |
  |--- connect() ----------------->|
  |                                |
  |--- send(0x01) [PING] --------->|
  |                                |
  |                         recv() |-- Recoit PING
  |                                |
  |                         send() |-- Envoie PONG
  |                                |
  |<--- recv(0x01) [PONG] ---------|
  |                                |
  +--- close() --------------------+
```

### Gestion d'erreurs

- **Pair non disponible** : Le timeout/echec de connexion est normal en phase de demarrage
- **Port deja utilise** : Option `SO_REUSEADDR` pour reutiliser rapidement le port
- **Allocation memoire** : Verifications et nettoyage systematique
- **Threads** : Detachement pour eviter les fuites de ressources

---

## Auteurs

- Cyprien BONS
- Raphaël RIVAS
- Maël NICOLAS
- Killian RAMUS
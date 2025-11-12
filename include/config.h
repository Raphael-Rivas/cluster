#ifndef CONFIG_H
#define CONFIG_H

#define MAX_PEERS 10
#define MAX_HOST_LEN 64

// Structure représentant un pair dans le cluster
typedef struct {
    int id;
    char host[MAX_HOST_LEN];
    int port;
} peer_t;

// Charge la configuration depuis un fichier
// Retourne le nombre de pairs chargés, ou -1 en cas d'erreur
int load_config(const char *filename, peer_t *peers, int max_peers);

#endif

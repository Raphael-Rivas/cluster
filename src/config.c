#include "config.h"
#include <stdio.h>

int load_config(const char *filename, peer_t *peers, int max_peers) {
    FILE *file;
    int count = 0;

    // Ouverture du fichier
    file = fopen(filename, "r");
    if (!file) {
        perror("[CONFIG] Erreur d'ouverture du fichier");
        return -1;
    }

    // Lecture ligne par ligne
    while (count < max_peers &&
           fscanf(file, "%d %63s %d",
                  &peers[count].id,
                  peers[count].host,
                  &peers[count].port) == 3) {
        count++;
    }

    fclose(file);

    printf("[CONFIG] %d pair(s) chargé(s) depuis %s\n", count, filename);

    // Affichage des pairs chargés
    for (int i = 0; i < count; i++) {
        printf("[CONFIG] Pair %d: ID=%d, Host=%s, Port=%d\n",
               i, peers[i].id, peers[i].host, peers[i].port);
    }

    return count;
}

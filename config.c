#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int id;
    char host[64];
    int port;
} peer_t;

int main(void) {
    peer_t peers[10];
    FILE *file = fopen("nodes.conf", "r");
    if (!file) {
        perror("Erreur d'ouverture du fichier");
        return -1;
    }
    int count = 0;
	int max_peers = 3;
    while (count < max_peers && fscanf(file, "%d %63s %d",
            &peers[count].id, peers[count].host, &peers[count].port) == 3) {
        count++;
    }
    fclose(file);

    printf("Pairs chargés depuis le fichier :\n");
    for (int i = 0; i < count; i++) {
        printf("ID: %d | Host: %s | Port: %d\n",
               peers[i].id, peers[i].host, peers[i].port);
    }

    return 0;
}

#include "config.h"
#include <stdio.h>

int load_config(const char *filename, peer_t *peers, int max_peers) {
  FILE *file;
  int count = 0;
  char line[256];

  // Ouverture du fichier
  file = fopen(filename, "r");
  if (!file) {
    perror("[CONFIG] Erreur d'ouverture du fichier");
    return -1;
  }

  // Lecture ligne par ligne
  while (count < max_peers && fgets(line, sizeof(line), file)) {
    // Ignorer les lignes vides ou commentaires
    char *ptr = line;
    while (*ptr == ' ' || *ptr == '\t')
      ptr++; // Skip whitespace
    if (*ptr == '\0' || *ptr == '\n' || *ptr == '#')
      continue;

    // Parse logic
    if (sscanf(line, "%d %63s %d", &peers[count].id, peers[count].host,
               &peers[count].port) == 3) {
      count++;
    } else {
      fprintf(stderr, "[CONFIG] Ligne ignorée (format invalide): %s", line);
    }
  }

  fclose(file);

  printf("[CONFIG] %d pair(s) chargé(s) depuis %s\n", count, filename);

  // Affichage des pairs chargés
  for (int i = 0; i < count; i++) {
    printf("[CONFIG] Pair %d: ID=%d, Host=%s, Port=%d\n", i, peers[i].id,
           peers[i].host, peers[i].port);
  }

  return count;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "node.h"

int main(int argc, char *argv[]) {
    node_t node;
    int node_id;
    const char *config_file = "src/nodes.conf";

    // Affichage du banner
    printf("========================================\n");
    printf("  CLUSTER - Système distribué\n");
    printf("  Étape 1: Ping et Synchronisation\n");
    printf("========================================\n\n");

    // Vérification des arguments
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <node_id>\n", argv[0]);
        fprintf(stderr, "Exemple: %s 0\n", argv[0]);
        return 1;
    }

    // Récupération de l'ID du nœud
    node_id = atoi(argv[1]);

    printf("[MAIN] Démarrage du nœud %d...\n\n", node_id);

    // Initialisation du nœud
    if (node_init(&node, node_id, config_file) < 0) {
        fprintf(stderr, "[MAIN] Échec de l'initialisation du nœud\n");
        return 1;
    }

    // Démarrage du serveur RPC
    if (node_start_server(&node) < 0) {
        fprintf(stderr, "[MAIN] Échec du démarrage du serveur RPC\n");
        node_cleanup(&node);
        return 1;
    }

    printf("\n[MAIN] Nœud %d opérationnel\n", node_id);
    printf("[MAIN] Serveur RPC en écoute sur le port %d\n\n", node.port);

    // Boucle de synchronisation
    node_sync_loop(&node);

    // Après synchronisation
    printf("[MAIN] Synchronisation terminée. Le cluster est opérationnel.\n");
    printf("[MAIN] Appuyez sur Ctrl+C pour arrêter...\n\n");

    // Boucle infinie (le programme reste actif)
    while (1) {
        sleep(10);
    }

    // Nettoyage (jamais atteint dans ce cas)
    node_cleanup(&node);

    return 0;
}

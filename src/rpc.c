#include "rpc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <pthread.h>

// Démarre un serveur TCP sur le port spécifié
int rpc_server_start(int port) {
    int server_socket;
    struct sockaddr_in server_addr;
    int opt = 1;

    // Création du socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        return -1;
    }

    // Option SO_REUSEADDR pour éviter "Address already in use"
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Setsockopt failed");
        close(server_socket);
        return -1;
    }

    // Configuration de l'adresse
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Bind
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        return -1;
    }

    // Listen
    if (listen(server_socket, 10) < 0) {
        perror("Listen failed");
        close(server_socket);
        return -1;
    }

    printf("[RPC] Serveur démarré sur le port %d\n", port);
    return server_socket;
}

// Gère une connexion client : lit un octet (ping) et répond par un octet (pong)
void rpc_handle_client(int client_socket) {
    uint8_t msg;
    ssize_t bytes_received;

    // Lecture d'un octet
    bytes_received = recv(client_socket, &msg, 1, 0);

    if (bytes_received < 0) {
        perror("[RPC] Receive failed");
        close(client_socket);
        return;
    }

    if (bytes_received == 0) {
        printf("[RPC] Client déconnecté\n");
        close(client_socket);
        return;
    }

    // Vérification du message
    if (msg == MSG_PING) {
        printf("[RPC] PING reçu\n");

        // Envoi du PONG
        if (rpc_send_pong(client_socket) < 0) {
            fprintf(stderr, "[RPC] Échec de l'envoi du PONG\n");
        }
    } else {
        fprintf(stderr, "[RPC] Message inconnu: %u\n", msg);
    }

    close(client_socket);
}

// Envoie un PONG (1 octet) au client
int rpc_send_pong(int client_socket) {
    uint8_t msg = MSG_PONG;

    if (send(client_socket, &msg, 1, 0) < 0) {
        perror("[RPC] Send failed");
        return -1;
    }

    printf("[RPC] PONG envoyé\n");
    return 0;
}

// Thread wrapper pour gérer un client
void* rpc_client_thread(void* arg) {
    int client_socket = *((int*)arg);
    free(arg);

    rpc_handle_client(client_socket);

    return NULL;
}

// Boucle principale du serveur : accepte les connexions et crée un thread par client
void rpc_server_run(int server_socket) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    pthread_t thread_id;

    printf("[RPC] Serveur en écoute...\n");

    while (1) {
        // Allocation pour le socket client (sera libéré dans le thread)
        int* client_socket = malloc(sizeof(int));
        if (client_socket == NULL) {
            perror("[RPC] Memory allocation failed");
            continue;
        }

        // Acceptation de la connexion
        *client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);

        if (*client_socket < 0) {
            perror("[RPC] Accept failed");
            free(client_socket);
            continue;
        }

        printf("[RPC] Nouvelle connexion de %s:%d\n",
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));

        // Création du thread pour gérer le client
        if (pthread_create(&thread_id, NULL, rpc_client_thread, client_socket) != 0) {
            perror("[RPC] Thread creation failed");
            close(*client_socket);
            free(client_socket);
            continue;
        }

        // Détachement du thread (nettoyage automatique)
        pthread_detach(thread_id);
    }
}

// Envoie un PING à un pair et attend le PONG
int rpc_send_ping(const char *host, int port) {
    int client_socket;
    struct sockaddr_in server_addr;
    uint8_t msg;
    ssize_t bytes_received;

    // Création du socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("[RPC] Socket creation failed");
        return -1;
    }

    // Configuration de l'adresse du serveur
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
        perror("[RPC] Invalid address");
        close(client_socket);
        return -1;
    }

    // Connexion au serveur
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        // Ne pas afficher d'erreur ici (normal si le pair n'est pas encore démarré)
        close(client_socket);
        return -1;
    }

    // Envoi du PING
    msg = MSG_PING;
    if (send(client_socket, &msg, 1, 0) < 0) {
        perror("[RPC] Send failed");
        close(client_socket);
        return -1;
    }

    // Attente du PONG
    bytes_received = recv(client_socket, &msg, 1, 0);

    if (bytes_received > 0 && msg == MSG_PONG) {
        printf("[RPC] PING -> %s:%d : OK (PONG reçu)\n", host, port);
        close(client_socket);
        return 0;
    } else {
        fprintf(stderr, "[RPC] PING -> %s:%d : Pas de PONG reçu\n", host, port);
        close(client_socket);
        return -1;
    }
}

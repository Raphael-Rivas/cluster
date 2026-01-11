#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MSG_CLIENT_PUT 10
#define MSG_CLIENT_GET 11
#define MSG_REDIRECT 12
#define MSG_OK 13
#define MSG_FAIL 14

void usage(char *prog) {
  fprintf(stderr, "Usage: %s <host> <port> <PUT|GET> <key> [value]\n", prog);
  exit(1);
}

int main(int argc, char *argv[]) {
  if (argc < 5)
    usage(argv[0]);

  const char *host = argv[1];
  int port = atoi(argv[2]);
  char *cmd = argv[3];
  char *key = argv[4];
  char key_buf[32] = {0}; // Buffer paddé à zéro
  int value = 0;

  // Copier la clé dans le buffer (max 31 chars + null terminator)
  strncpy(key_buf, key, 31);

  if (strcmp(cmd, "PUT") == 0) {
    if (argc != 6)
      usage(argv[0]);
    value = atoi(argv[5]);
  } else if (strcmp(cmd, "GET") == 0) {
    if (argc != 5)
      usage(argv[0]);
  } else {
    usage(argv[0]);
  }

  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("Socket");
    return 1;
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
    perror("Invalid address");
    return 1;
  }

  if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("Connect");
    return 1;
  }

  if (strcmp(cmd, "PUT") == 0) {
    uint8_t type = MSG_CLIENT_PUT;
    send(sock, &type, 1, 0);
    send(sock, key_buf, 32, 0); // Buffer paddé à 32 octets
    int32_t val_net = htonl(value);
    send(sock, &val_net, 4, 0);

    uint8_t resp;
    recv(sock, &resp, 1, 0);

    if (resp == MSG_OK) {
      printf("OK\n");
    } else if (resp == MSG_REDIRECT) {
      int32_t leader_id;
      recv(sock, &leader_id, 4, 0);
      printf("REDIRECT Leader: %d\n", ntohl(leader_id));
    } else {
      printf("FAIL\n");
    }
  } else { // GET
    uint8_t type = MSG_CLIENT_GET;
    send(sock, &type, 1, 0);
    send(sock, key_buf, 32, 0); // Buffer paddé à 32 octets

    uint8_t resp;
    recv(sock, &resp, 1, 0);

    if (resp == MSG_OK) {
      int32_t val_net;
      recv(sock, &val_net, 4, 0);
      printf("%d\n", ntohl(val_net));
    } else {
      printf("NOT_FOUND\n");
    }
  }

  close(sock);
  return 0;
}

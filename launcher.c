#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define UDP_PORT 11111
#define MAX_BUFFER_SIZE 256

int generate_random_port() {
    return rand() % (65535 - 1024 + 1) + 1024;
}

int main() {
    srand(time(NULL));
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in server_addr, client_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(UDP_PORT);

    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Listening on UDP port %d...\n", UDP_PORT);

    char buffer[MAX_BUFFER_SIZE];
    socklen_t len = sizeof(client_addr);

    while (1) {
        int n = recvfrom(sockfd, (char *)buffer, MAX_BUFFER_SIZE, MSG_WAITALL,
                          (struct sockaddr *)&client_addr, &len);
        buffer[n] = '\0';

        printf("Received: %s\n", buffer);

        int tcp_port;
        if (sscanf(buffer, "%d", &tcp_port) != 1) {
            printf("Wrong format\n");
            continue;
        }

        int udp_port = generate_random_port();
        printf("UDP port: %d\n", udp_port);

        char command[MAX_BUFFER_SIZE];
        snprintf(command, sizeof(command), "./game_server.out %d %d", tcp_port, udp_port);
        printf("Launching game server %s\n", command);
        system(command);
    }

    close(sockfd);
    return 0;
}
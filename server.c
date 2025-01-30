#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <ifaddrs.h>
#include <stdbool.h>
#include <sys/time.h>


#define UDP_PORT 12345
#define PI 3.14159265358979323846
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define GRID_SIZE 20
#define SNAKE_LENGTH 100
#define MAX_PLAYERS 3

typedef struct {
    int x, y;
} Point;

Point snake[MAX_PLAYERS][SNAKE_LENGTH];
int snake_length = 0;
int score[MAX_PLAYERS];
int counter = 0;
int alive = 4;
int alPl[4] = {1, 1, 1, 1};
int direction[4] = {0,0,0,0};

struct GraczTCP {
    int socket;
    char name[256];
};
struct GraczTCP client_names[MAX_PLAYERS];
int client_count = 0;
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

bool is_not_colliding(int numb);
void *game_handler(void *ptr);
void move_snake(int i, int j);
void* handle_disconnects(void*ptr);

int main(){
    pthread_t threadTCP;
    struct sockaddr_in sa;
    int SocketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (SocketFD == -1) {
        perror("cannot create socket");
        exit(EXIT_FAILURE);
    }
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = htons(1102);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(SocketFD, (struct sockaddr *)&sa, sizeof sa) == -1) {
        perror("bind failed");
        close(SocketFD);
        exit(EXIT_FAILURE);
    }

    if (listen(SocketFD, 10) == -1) {
        perror("listen failed");
        close(SocketFD);
        exit(EXIT_FAILURE);
    }

    while(client_count < MAX_PLAYERS){
        int ConnectFD = accept(SocketFD, NULL, NULL);
        if (ConnectFD == -1) {
            perror("accept failed");
            continue;
        }

        pthread_mutex_lock(&client_mutex);
        char buff[32]; //max size is 32
        memset(buff, 0, sizeof buff);
        if (read(ConnectFD, buff, sizeof(buff)) <= 0) {
            close(ConnectFD);
            pthread_mutex_unlock(&client_mutex);
            continue;
        }
        int *new_sock = malloc(sizeof(int));
        if (!new_sock) {
            close(ConnectFD);
            pthread_mutex_unlock(&client_mutex);
            continue;
        }
        *new_sock = ConnectFD;
        // Send all other users the new user name
        for(int i = 0; i < client_count; i++){
            write(client_names[i].socket, buff, sizeof(buff));
            printf("Sent %s to %d\n", buff, client_names[i].socket);
        }

        // Add the new user to the current users
        client_names[client_count].socket = ConnectFD;
        strncpy(client_names[client_count].name, buff, sizeof(client_names[client_count].name) - 1);
        client_names[client_count].name[sizeof(client_names[client_count].name) - 1] = '\0';
        client_count++;

        // Send the new user all current users
        for(int i = 0; i < client_count; i++){
            write(ConnectFD, client_names[i].name, sizeof(client_names[i].name));
            sleep(0.2);
            printf("Sent %s to %d\n", client_names[i].name, ConnectFD);
        }
        if (pthread_create(&threadTCP, NULL, handle_disconnects, (void *)new_sock) != 0) {
            close(ConnectFD);
            free(new_sock);
            pthread_mutex_unlock(&client_mutex);
            continue;
        } else {
            pthread_detach(threadTCP);
        }
        pthread_mutex_unlock(&client_mutex);
    }

    sleep(3); //make sure all is read on client side
    struct sockaddr_in localAddress, clientAddress;
    socklen_t clientAddrLen = sizeof(clientAddress);
    localAddress.sin_family = AF_INET;
    localAddress.sin_port = htons(UDP_PORT);
    localAddress.sin_addr.s_addr = htonl(INADDR_ANY);

    int server_socket = socket(PF_INET, SOCK_DGRAM, 0);
    if (server_socket == -1) {
        printf("cannot create socket\n");
        exit(EXIT_FAILURE);
    }

    if (bind(server_socket, (struct sockaddr*) &localAddress, sizeof(localAddress)) == -1) {
        printf("Could not bind UDP\n");
        close(server_socket);
        close(SocketFD);
        exit(EXIT_FAILURE);
    }

    char str[8];
    char str2[8];
    memset(str, 0, sizeof str);
    sprintf(str, "%d\n", UDP_PORT);
    char buff[32];
    memset(buff, 0, sizeof buff);
    strncpy(buff, "@Start", sizeof(buff) - 1);
    buff[sizeof(buff) - 1] = '\n';
    for(int i = 0; i < client_count; i++){
        memset(str2, 0, sizeof str2);
        write(client_names[i].socket, buff, sizeof(buff));
        sleep(0.2);
        write(client_names[i].socket, str, sizeof(str));
        sleep(0.2);
        sprintf(str2, "%d\n", i);
        write(client_names[i].socket, str2, sizeof(str2));
        printf("Sent %s, %s, %s to %d\n", buff, str, str2, i);
    }

    //set starting positions
    int alive = client_count;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        int gray = i ^ (i >> 1);
        snake[i][0].x = ((gray & 2) >> 1) * 3;
        snake[i][0].y = (gray & 1) * 3;
        printf("Player %d: Start Position -> x: %d, y: %d\n", i, snake[i][0].x, snake[i][0].y);
    }

    while (alive > 1) {
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 300000; // 300 ms
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_socket, &read_fds);
        int select_result = select(server_socket + 1, &read_fds, NULL, NULL, &timeout);
        while (select_result > 0) {
            char buffer[256];
            memset(buffer, 0, sizeof buffer);
            int bytes_received = recvfrom(server_socket, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&clientAddress, &clientAddrLen);
            if (bytes_received > 0) {
                int number = -1;
                int direction2 = 0;
                sscanf(buffer, "%d.%d\n", &number, &direction2);
                if(number > -1 && number < client_count && direction2 > -1 && direction2 < 4){
                    direction[number] = direction2;
                }
            }
            FD_ZERO(&read_fds);
            FD_SET(server_socket, &read_fds);
            timeout.tv_sec = 0;
            timeout.tv_usec = 0; // No wait, just poll
            select_result = select(server_socket + 1, &read_fds, NULL, NULL, &timeout);
        }

        // Process game state and send updates to clients
        for(int i = 0; i < client_count; i++){
            if(alPl[i] == 1){
                move_snake(i, direction[i]);
                if(!(is_not_colliding(i))){
                    alPl[i] = 0;
                    printf("Killed %d\n", i);
                    alive--;
                }
            }
        }
        char buff[64];
        int curmax = 0;
        memset(buff, 0, sizeof buff);
        for(int i = 0; i < client_count; i++){
            if(alPl[i] == 1){
                curmax += snprintf(buff+ curmax, sizeof buff, "%d.%d.", i, direction[i]);
            }
            else{
                curmax += snprintf(buff+ curmax, sizeof buff, "%d.%d.", i, 5);
            }
        }
        for(int i = 0; i < client_count; i++){
            write(client_names[i].socket, buff, sizeof buff);
            printf("Sent %s to %d\n", buff, i);
        }
    }

    for(int i = 0; i < client_count; i++){
        close(client_names[i].socket);
    }
    close(SocketFD);
    close(server_socket);
    return 0;
}

bool is_not_colliding(int numb){
    //wall
    if (snake[numb][0].x < 0 || snake[numb][0].x >= SCREEN_WIDTH / GRID_SIZE ||
        snake[numb][0].y < 0 || snake[numb][0].y >= SCREEN_HEIGHT / GRID_SIZE) {
            printf("Screen problems %d: X: %d Y: %d\n", snake[numb][0].x, snake[numb][0].x, snake[numb][0].y );
        return false;
    }

    //self && other
    for(int j = 0; j < MAX_PLAYERS; j++){
        if(alPl[j] != 5){
            for (int i = 1; i < snake_length; i++) {
                if (snake[numb][0].x == snake[j][i].x && snake[numb][0].y == snake[j][i].y) {
                    printf("Collision\n");
                    return false;
                }
            }
        }
    }
    if(--counter < 0){
        counter = 10;
        snake_length++;
    }
    printf("Snakel %d\n", snake_length);
    return true;
}

void move_snake(int numb, int direction) {
    for (int i = snake_length - 1; i > 0; i--) {
        snake[numb][i] = snake[0][i - 1];
    }

    //head
    switch (direction) {
        case 0: snake[numb][0].x++; break;
        case 1: snake[numb][0].y++; break;
        case 2: snake[numb][0].x--; break;
        case 3: snake[numb][0].y--; break;
    }
}

void* handle_disconnects(void * ptr){
    int socket = *(int *)ptr;
    free(ptr);
    char buff[32];
    if(read(socket, buff, sizeof(buff)) > 0){
        if(strcmp("@Disconnect", buff) == 0){
            pthread_mutex_lock(&client_mutex);
            for(int i = 0; i < client_count; i++){
                if(client_names[i].socket == socket){
                    printf("%s\n", client_names[i].name);
                    for(int j = i; j < client_count - 1; j++){
                        client_names[j] = client_names[j + 1];
                    }
                    client_count--;
                    break;
                }
            }
            pthread_mutex_unlock(&client_mutex);
        }
    }
    close(socket);
    return NULL;
}
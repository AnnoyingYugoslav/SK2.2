#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <math.h>
#include <pthread.h>
#include <sys/select.h>

//gcc -o snake_game client.c -lSDL2 -lm -lSDL2_ttf -lpthread


#define LAUNCHER 11111
#define PI 3.14159265358979323846
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define GRID_SIZE 20
#define SNAKE_LENGTH 100
#define MAX_PLAYERS 2

typedef struct {
    int x, y;
} Point;

Point snake[MAX_PLAYERS][SNAKE_LENGTH];
int snake_length[MAX_PLAYERS]; // Array to store the length for each player
Point food;
int my_direction = 0; //gets my direction
int myNumber = -1; //my player number on server
bool running = true; //game running
bool try_connect = false; //try to connect to server
bool on_start_screen = true; //start screen
bool on_connect_screen = false;
bool on_wait_screen = false;
bool on_game_screen = false;
bool on_end_screen = false;
bool connect_TCP = false;
bool connect_UDP = false;
bool change = false;
int last_accepted_move = 0;
char myName[64] = ""; //my name
char serverAdress[64] = "";
int directions[4] = {0,0,0,0};
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

int counter[MAX_PLAYERS]; // Array to store the counter for each player
int currentplayers = 0;

void render_start_screen(SDL_Renderer *renderer); //start screen
void render_game_screen(SDL_Renderer *renderer); //actual game
void render_end_screen(SDL_Renderer *renderer); //end screen
int render_connect_screen(SDL_Renderer *renderer); //waiting for connection to server, returns my number
int render_players_screen(SDL_Renderer *renderer, int socket); //waiting for players
int* getUDPData(int socket, struct sockaddr_in *serverAdress); //get direction of players according to server
char* getNames(int socket);
void set_nonblocking(int socket); //set TCP to non-blocking - needed for reading player connecting
void sendDisconnect(int socket);
void set_blocking(int socket); //set TCP to blocking - needed for writing to server
void move_snake(int numb, int dir); //move the snake
void send_UDP(struct sockaddr_in socket, int sock); //send my direction to server
void read_TCP(int socket); //read from server
void* handle_cancel(void* arg);
void call_launcher(struct sockaddr_in launcher, char * port);

int main(int argc, char *argv[]) {
    last_accepted_move = 0;
    pthread_t threadUDP;
    struct sockaddr_in* UDPsock;
    int portTCP = -1;
    int portUDP = -1;
    int UDPSocket = socket(PF_INET, SOCK_DGRAM, 0);
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *window = SDL_CreateWindow("Snake", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    srand(time(NULL));
    for (int i = 0; i < MAX_PLAYERS; i++) {
        int gray = i ^ (i >> 1);
        snake[i][0].x = ((gray & 2) >> 1) * 3;
        snake[i][0].y = (gray & 1) * 3;
        snake_length[i] = 1; // Initialize snake length for each player
        counter[i] = 10; // Initialize counter for each player
        printf("Player %d: Start Position -> x: %d, y: %d\n", i, snake[i][0].x, snake[i][0].y);
    }

    SDL_Event e;
    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
            }
            else if (on_game_screen && e.type == SDL_KEYDOWN) {
                pthread_mutex_lock(&client_mutex);
                printf("Key pressed\n");
                switch (e.key.keysym.sym) {
                    case SDLK_UP:
                        if (last_accepted_move != 1) { // Prevent moving down
                            my_direction = 3;
                            printf("my_dire %d \n", my_direction);
                            change = true;
                        }
                        break;
                    case SDLK_DOWN:
                        if (last_accepted_move != 3) { // Prevent moving up
                            my_direction = 1;
                            printf("my_dire %d \n", my_direction);
                            change = true;
                        }
                        break;
                    case SDLK_LEFT:
                        if (last_accepted_move != 0) { // Prevent moving right
                            my_direction = 2;
                            printf("my_dire %d \n", my_direction);
                            change = true;
                        }
                        break;
                    case SDLK_RIGHT:
                        if (last_accepted_move != 2) { // Prevent moving left
                            my_direction = 0;
                            printf("my_dire %d \n", my_direction);
                            change = true;
                        }
                        break;
                }
                pthread_mutex_unlock(&client_mutex);
            }
        }
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        if (on_start_screen) {
            render_start_screen(renderer);
        }
        else if(on_connect_screen){
            portTCP = render_connect_screen(renderer);
            if(portTCP != -1){
                on_connect_screen = false;
                on_wait_screen = true;
                set_blocking(portTCP);
            }
        }
        else if(on_wait_screen){
            portUDP = render_players_screen(renderer, portTCP);
            if(portUDP != -1){
                set_nonblocking(portTCP);
            }
        }
        else if(on_game_screen){
            static int udp_initialized = 0;
            static struct sockaddr_in server_addr;
            if (!udp_initialized) {
                memset(&server_addr, 0, sizeof(server_addr));
                server_addr.sin_family = AF_INET;
                server_addr.sin_addr.s_addr = inet_addr(serverAdress);
                server_addr.sin_port = htons(portUDP);
                printf("Port UDP: %d\n", portUDP);
                if (inet_pton(AF_INET, serverAdress, &server_addr.sin_addr) <= 0) {
                    exit(EXIT_FAILURE);
                }

                udp_initialized = 1;
            }
            if(last_accepted_move != my_direction){
                send_UDP(server_addr,UDPSocket);
            }
            read_TCP(portTCP);
            render_game_screen(renderer);
        }
        else if(on_end_screen){
            render_end_screen(renderer);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16); // Delay to limit the frame rate
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

void render_start_screen(SDL_Renderer *renderer) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_Rect button = {350, 275, 100, 50};
    SDL_RenderFillRect(renderer, &button);
    if (TTF_Init() < 0) {
        printf("Lib\n");
        return;
    }
    TTF_Font *font = TTF_OpenFont("font.ttf", 24);
    if (!font) {
        printf("Font\n");
        TTF_Quit();
        return;
    }
    SDL_Color textColor = {0, 0, 0, 255};
    SDL_Surface *textSurface = TTF_RenderText_Blended(font, "Start", textColor);
    if (!textSurface) {
        printf("Surfacet\n");
        TTF_CloseFont(font);
        TTF_Quit();
        return;
    }
    SDL_Texture *textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    if (!textTexture) {
        SDL_FreeSurface(textSurface);
        TTF_CloseFont(font);
        TTF_Quit();
        return;
    }
    SDL_Rect textRect = {button.x + 10, button.y + 10, textSurface->w, textSurface->h};
    SDL_FreeSurface(textSurface);
    SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
    SDL_DestroyTexture(textTexture);
    TTF_CloseFont(font);
    TTF_Quit();

    SDL_Event e;
    SDL_PollEvent(&e);
    if (e.type == SDL_MOUSEBUTTONDOWN) {
        int x = e.button.x;
        int y = e.button.y;
        if (x >= 350 && x <= 450 && y >= 275 && y <= 325) {
            on_start_screen = false;
            on_connect_screen = true;
        }
    }
}


void* handle_cancel(void* arg) {
    int socket = *(int*)arg;
    free(arg);

    SDL_Event e;
    while (true) {
        if(on_game_screen){
            return NULL;
        }
        SDL_PollEvent(&e);
        if (e.type == SDL_MOUSEBUTTONDOWN) {
            int x = e.button.x;
            int y = e.button.y;
            if (x >= 100 && x <= 300 && y >= 360 && y <= 400) { // Coordinates of the "Cancel" button
                printf("Cancel, go to start\n");
                on_start_screen = true;
                on_connect_screen = false;
                on_end_screen = false;
                on_wait_screen = false;
                sendDisconnect(socket);
                break;
            }
        }
    }
    return NULL;
}

int render_connect_screen(SDL_Renderer *renderer) {
    if (TTF_Init() < 0) {
        printf("Lib - connect\n");
        return -1;
    }
    TTF_Font* font = TTF_OpenFont("font.ttf", 24);
    if (!font) {
        printf("Font - connect\n");
        return -1;
    }
    SDL_Color textColor = {255, 255, 255, 255};
    SDL_Rect input1Rect = {100, 100, 200, 40};
    SDL_Rect input2Rect = {100, 160, 200, 40};
    SDL_Rect input3Rect = {100, 220, 200, 40};
    SDL_Rect buttonRect = {100, 280, 200, 40};
    bool input1Focused = false, input2Focused = false, input3Focused = false;
    char input1Text[256] = "";
    char input2Text[256] = "";
    char input3Text[256] = "";
    SDL_Event e;
    bool quit = false;
    while (!quit) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            }
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                int x, y;
                SDL_GetMouseState(&x, &y);
                if (x >= buttonRect.x && x <= buttonRect.x + buttonRect.w &&
                    y >= buttonRect.y && y <= buttonRect.y + buttonRect.h) {
                    try_connect = true;
                }
                input1Focused = (x >= input1Rect.x && x <= input1Rect.x + input1Rect.w &&
                                 y >= input1Rect.y && y <= input1Rect.y + input1Rect.h);
                input2Focused = (x >= input2Rect.x && x <= input2Rect.x + input2Rect.w &&
                                 y >= input2Rect.y && y <= input2Rect.y + input2Rect.h);
                input3Focused = (x >= input3Rect.x && x <= input3Rect.x + input3Rect.w &&
                                 y >= input3Rect.y && y <= input3Rect.y + input3Rect.h);
            }
            if (e.type == SDL_KEYDOWN) {
                if (input1Focused) {
                    if (e.key.keysym.sym == SDLK_BACKSPACE && strlen(input1Text) > 0) {
                        input1Text[strlen(input1Text) - 1] = '\0';
                    } else if (e.key.keysym.sym >= SDLK_SPACE && e.key.keysym.sym <= SDLK_z) {
                        int len = strlen(input1Text);
                        if (len < sizeof(input1Text) - 1) {
                            input1Text[len] = e.key.keysym.sym;
                            input1Text[len + 1] = '\0';
                        }
                    }
                }
                if (input2Focused) {
                    if (e.key.keysym.sym == SDLK_BACKSPACE && strlen(input2Text) > 0) {
                        input2Text[strlen(input2Text) - 1] = '\0';
                    } else if (e.key.keysym.sym >= SDLK_SPACE && e.key.keysym.sym <= SDLK_z) {
                        int len = strlen(input2Text);
                        if (len < sizeof(input2Text) - 1) {
                            input2Text[len] = e.key.keysym.sym;
                            input2Text[len + 1] = '\0';
                        }
                    }
                }
                if (input3Focused) {
                    if (e.key.keysym.sym == SDLK_BACKSPACE && strlen(input3Text) > 0) {
                        input3Text[strlen(input3Text) - 1] = '\0';
                    } else if (e.key.keysym.sym >= SDLK_SPACE && e.key.keysym.sym <= SDLK_z) {
                        int len = strlen(input3Text);
                        if (len < sizeof(input3Text) - 1) {
                            input3Text[len] = e.key.keysym.sym;
                            input3Text[len + 1] = '\0';
                        }
                    }
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Black background
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // White for input field borders
        SDL_RenderDrawRect(renderer, &input1Rect);
        SDL_RenderDrawRect(renderer, &input2Rect);
        SDL_RenderDrawRect(renderer, &input3Rect);
        SDL_RenderDrawRect(renderer, &buttonRect);

        SDL_Surface* buttonTextSurface = TTF_RenderText_Solid(font, "Connect", textColor);
        SDL_Texture* buttonTextTexture = SDL_CreateTextureFromSurface(renderer, buttonTextSurface);
        SDL_FreeSurface(buttonTextSurface);
        SDL_Rect textRect = {buttonRect.x + 60, buttonRect.y + 10, 80, 20};
        SDL_RenderCopy(renderer, buttonTextTexture, NULL, &textRect);
        SDL_DestroyTexture(buttonTextTexture);

        SDL_Surface* input1TextSurface = TTF_RenderText_Solid(font, input1Text, textColor);
        SDL_Texture* input1TextTexture = SDL_CreateTextureFromSurface(renderer, input1TextSurface);
        SDL_FreeSurface(input1TextSurface);
        SDL_Rect input1TextRect = {input1Rect.x + 5, input1Rect.y + 5, input1Rect.w - 10, input1Rect.h - 10};
        SDL_RenderCopy(renderer, input1TextTexture, NULL, &input1TextRect);
        SDL_DestroyTexture(input1TextTexture);

        SDL_Surface* input2TextSurface = TTF_RenderText_Solid(font, input2Text, textColor);
        SDL_Texture* input2TextTexture = SDL_CreateTextureFromSurface(renderer, input2TextSurface);
        SDL_FreeSurface(input2TextSurface);
        SDL_Rect input2TextRect = {input2Rect.x + 5, input2Rect.y + 5, input2Rect.w - 10, input2Rect.h - 10};
        SDL_RenderCopy(renderer, input2TextTexture, NULL, &input2TextRect);
        SDL_DestroyTexture(input2TextTexture);

        SDL_Surface* input3TextSurface = TTF_RenderText_Solid(font, input3Text, textColor);
        SDL_Texture* input3TextTexture = SDL_CreateTextureFromSurface(renderer, input3TextSurface);
        SDL_FreeSurface(input3TextSurface);
        SDL_Rect input3TextRect = {input3Rect.x + 5, input3Rect.y + 5, input3Rect.w - 10, input3Rect.h - 10};
        SDL_RenderCopy(renderer, input3TextTexture, NULL, &input3TextRect);
        SDL_DestroyTexture(input3TextTexture);

        // Present renderer to window
        SDL_RenderPresent(renderer);

        if (try_connect) {
            try_connect = false;
            printf("TRYING\n");
            struct sockaddr_in sa;
            int SocketFD;
            printf("%s\n", input1Text);
            printf("%s\n", input2Text);
            int port = atoi(input2Text); //get it in the windows
            struct sockaddr_in launcher;
            memset(&launcher, 0, sizeof(launcher));
            launcher.sin_family = AF_INET;
            launcher.sin_addr.s_addr = inet_addr(input1Text); // Assuming the launcher is at the same address
            launcher.sin_port = htons(LAUNCHER);
            call_launcher(launcher, input2Text);
            printf("Waiting for server to open: \n");
            sleep(2);
            SocketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (SocketFD == -1) {
                printf("cannot create socket\n");
                return -1;
            }
            memset(&sa, 0, sizeof sa);
            sa.sin_addr.s_addr = inet_addr(input1Text); //get it from window
            sa.sin_family = AF_INET;
            sa.sin_port = htons(port);

            set_nonblocking(SocketFD);

            int result = connect(SocketFD, (struct sockaddr *)&sa, sizeof sa);
            if (result == -1 && errno != EINPROGRESS) {
                printf("Connect failed\n");
                close(SocketFD);
                return -1;
            }

            if (result == 0) {
                strcpy(serverAdress, input1Text);
                set_blocking(SocketFD);
                write(SocketFD, input3Text, strlen(input3Text));
                return SocketFD;
            }

            fd_set wfds;
            struct timeval tv;
            FD_ZERO(&wfds);
            FD_SET(SocketFD, &wfds);

            tv.tv_sec = 5;
            tv.tv_usec = 0;

            result = select(SocketFD + 1, NULL, &wfds, NULL, &tv);
            if (result == 0) {
                printf("Connect failed2\n");
                close(SocketFD);
                return -1;
            } else if (result == -1) {
                printf("Select failed\n");
                close(SocketFD);
                return -1;
            }

            int optval;
            socklen_t optlen = sizeof(optval);
            if (getsockopt(SocketFD, SOL_SOCKET, SO_ERROR, &optval, &optlen) == -1) {
                close(SocketFD);
                return -1;
            }

            if (optval != 0) {
                printf("Connect timeout\n");
                close(SocketFD);
                return -1;
            }

            set_blocking(SocketFD);
            write(SocketFD, input3Text, strlen(input3Text));
            strcpy(serverAdress, input1Text);
            return SocketFD;
        }
    }
    TTF_CloseFont(font);
    return -1;
}

int render_players_screen(SDL_Renderer *renderer, int socket) {
    if (TTF_Init() < 0) {
        printf("Lib - plt\n");
        return -1;
    }
    TTF_Font* font = TTF_OpenFont("font.ttf", 24);
    if (!font) {
        printf("Font - plt\n");
        return -1;
    }
    SDL_Color textColor = {255, 255, 255, 255};
    SDL_Rect input1Rect = {100, 100, 200, 40};
    SDL_Rect input2Rect = {100, 160, 200, 40};
    SDL_Rect input3Rect = {100, 240, 200, 40};
    SDL_Rect input4Rect = {100, 300, 200, 40};
    SDL_Rect buttonRect = {100, 360, 200, 40};
    char input1Text[256] = "";
    char input2Text[256] = "";
    char input3Text[256] = "";
    char input4Text[256] = "";
    SDL_Event e;
    bool quit = false;
    bool getPort = false;
    bool getMyNumber = false;
    int port = -1;
    int* socket_ptr = malloc(sizeof(int));
    *socket_ptr = socket;
    pthread_t cancel_thread;
    pthread_create(&cancel_thread, NULL, handle_cancel, socket_ptr);
    pthread_detach(cancel_thread);
    while (!quit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quit = true;
            }
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                int x, y;
                SDL_GetMouseState(&x, &y);
                printf("X: %d, Y: %d\n", x, y);
                if (x >= buttonRect.x && x <= buttonRect.x + buttonRect.w &&
                    y >= buttonRect.y && y <= buttonRect.y + buttonRect.h) {
                    printf("Cancel, go to start\n");
                    on_start_screen = true;
                    on_connect_screen = false;
                    on_end_screen = false;
                    on_wait_screen = false;
                    sendDisconnect(socket);
                    return -1;
                }
            }
        }
        char* name;
        name = getNames(socket);

        if (strcmp("", name) != 0) { //it read smth
            if (strcmp("@Error", name) == 0) {
                continue;
            }
            else if(getMyNumber){
                int myumber;
                sscanf(name, "%d.%d\n", &myumber, &port);
                myNumber = myumber;
                printf("I am player %d\n", myNumber);
                getMyNumber = false;
                sleep(0.1);
                printf("Port: %d\n", port);
                on_wait_screen = false;
                on_game_screen = true;
                return port;
            }
            else if (strcmp("@Start", name) == 0) { //game started
                printf("Game start detected\n");
                getMyNumber = true;
            }
            else if (strncmp("@Disconnected", name, strlen("@Disconnected")) == 0) { //handle Disconnect
                char *client_name = name + strlen("@Disconnected ");
                if (strcmp(input1Text, client_name) == 0) {
                    strcpy(input1Text, input2Text);
                    strcpy(input2Text, input3Text);
                    strcpy(input3Text, input4Text);
                    input4Text[0] = '\0';
                } else if (strcmp(input2Text, client_name) == 0) {
                    strcpy(input2Text, input3Text);
                    strcpy(input3Text, input4Text);
                    input4Text[0] = '\0';
                } else if (strcmp(input3Text, client_name) == 0) {
                    strcpy(input3Text, input4Text);
                    input4Text[0] = '\0';
                } else if (strcmp(input4Text, client_name) == 0) {
                    input4Text[0] = '\0';
                }
                currentplayers--;
            }
            else{
                switch(currentplayers){
                    case 0:
                        strcpy(input1Text, name);
                        currentplayers++;
                        break;
                    case 1:
                        strcpy(input2Text, name);
                        currentplayers++;
                        break;
                    case 2:
                        strcpy(input3Text, name);
                        currentplayers++;
                        break;
                    case 3:
                        strcpy(input4Text, name);
                        currentplayers++;
                        break;
                }
            }
        }



        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Black background
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // White for input field borders
        SDL_RenderDrawRect(renderer, &input1Rect);
        SDL_RenderDrawRect(renderer, &input2Rect);
        SDL_RenderDrawRect(renderer, &input3Rect);
        SDL_RenderDrawRect(renderer, &input4Rect);
        SDL_RenderDrawRect(renderer, &buttonRect);


        SDL_Surface* buttonTextSurface = TTF_RenderText_Solid(font, "Cancel", textColor);
        SDL_Texture* buttonTextTexture = SDL_CreateTextureFromSurface(renderer, buttonTextSurface);
        SDL_FreeSurface(buttonTextSurface);
        SDL_Rect textRect = {buttonRect.x + 60, buttonRect.y + 10, 80, 20};
        SDL_RenderCopy(renderer, buttonTextTexture, NULL, &textRect);
        SDL_DestroyTexture(buttonTextTexture);

        SDL_Surface* input1TextSurface = TTF_RenderText_Solid(font, input1Text, textColor);
        SDL_Texture* input1TextTexture = SDL_CreateTextureFromSurface(renderer, input1TextSurface);
        SDL_FreeSurface(input1TextSurface);
        SDL_Rect input1TextRect = {input1Rect.x + 5, input1Rect.y + 5, input1Rect.w - 10, input1Rect.h - 10};
        SDL_RenderCopy(renderer, input1TextTexture, NULL, &input1TextRect);
        SDL_DestroyTexture(input1TextTexture);

        SDL_Surface* input2TextSurface = TTF_RenderText_Solid(font, input2Text, textColor);
        SDL_Texture* input2TextTexture = SDL_CreateTextureFromSurface(renderer, input2TextSurface);
        SDL_FreeSurface(input2TextSurface);
        SDL_Rect input2TextRect = {input2Rect.x + 5, input2Rect.y + 5, input2Rect.w - 10, input2Rect.h - 10};
        SDL_RenderCopy(renderer, input2TextTexture, NULL, &input2TextRect);
        SDL_DestroyTexture(input2TextTexture);

        SDL_Surface* input3TextSurface = TTF_RenderText_Solid(font, input3Text, textColor);
        SDL_Texture* input3TextTexture = SDL_CreateTextureFromSurface(renderer, input3TextSurface);
        SDL_FreeSurface(input3TextSurface);
        SDL_Rect input3TextRect = {input3Rect.x + 5, input3Rect.y + 5, input3Rect.w - 10, input3Rect.h - 10};
        SDL_RenderCopy(renderer, input3TextTexture, NULL, &input3TextRect);
        SDL_DestroyTexture(input3TextTexture);

        SDL_Surface* input4TextSurface = TTF_RenderText_Solid(font, input4Text, textColor);
        SDL_Texture* input4TextTexture = SDL_CreateTextureFromSurface(renderer, input4TextSurface);
        SDL_FreeSurface(input4TextSurface);
        SDL_Rect input4TextRect = {input4Rect.x + 5, input4Rect.y + 5, input4Rect.w - 10, input4Rect.h - 10};
        SDL_RenderCopy(renderer, input4TextTexture, NULL, &input4TextRect);
        SDL_DestroyTexture(input4TextTexture);

        // Present renderer to window
        SDL_RenderPresent(renderer);
    }
    return -1;
}

void render_game_screen(SDL_Renderer *renderer) {
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255); // Green color for the snake
    for (int j = 0; j < MAX_PLAYERS; j++) {
        if (directions[j] != 5) {
            for (int i = 0; i < snake_length[j]; i++) {
                // Ensure the snake segment is within the window boundaries
                if (snake[j][i].x >= 0 && snake[j][i].x < SCREEN_WIDTH / GRID_SIZE &&
                    snake[j][i].y >= 0 && snake[j][i].y < SCREEN_HEIGHT / GRID_SIZE) {
                    SDL_Rect segment = {snake[j][i].x * GRID_SIZE, snake[j][i].y * GRID_SIZE, GRID_SIZE, GRID_SIZE};
                    SDL_RenderFillRect(renderer, &segment);
                }
            }
        }
    }
}

void render_end_screen(SDL_Renderer *renderer){
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_Rect button = {350, 275, 100, 50};
    SDL_RenderFillRect(renderer, &button);
    if (TTF_Init() < 0) {
        printf("Lib\n");
        return;
    }
    TTF_Font *font = TTF_OpenFont("font.ttf", 24);
    if (!font) {
        printf("Font\n");
        TTF_Quit();
        return;
    }
    SDL_Color textColor = {0, 0, 0, 255};
    char buff[16];
    memest(buff,0, sizeof buff);
    if(directions[myNumber] < 4){
        strcpy(buff, "You Win!")
    }
    else{
        strcpy(buff, "You Lose!")
    }
    SDL_Surface *textSurface = TTF_RenderText_Blended(font, buff, textColor);
    if (!textSurface) {
        printf("Surfacet\n");
        TTF_CloseFont(font);
        TTF_Quit();
        return;
    }
    SDL_Texture *textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    if (!textTexture) {
        SDL_FreeSurface(textSurface);
        TTF_CloseFont(font);
        TTF_Quit();
        return;
    }
    SDL_Rect textRect = {button.x + 10, button.y + 10, textSurface->w, textSurface->h};
    SDL_FreeSurface(textSurface);
    SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
    SDL_DestroyTexture(textTexture);
    TTF_CloseFont(font);
    TTF_Quit();

    SDL_Event e;
    SDL_PollEvent(&e);
    if (e.type == SDL_MOUSEBUTTONDOWN) {
        int x = e.button.x;
        int y = e.button.y;
        if (x >= 350 && x <= 450 && y >= 275 && y <= 325) {
            on_start_screen = true;
            on_end_screen = false;
        }
    }
}

void move_snake(int numb, int dir) {
    for (int i = snake_length[numb] - 1; i > 0; i--) {
        snake[numb][i] = snake[numb][i - 1];
    }
    switch (dir) {
        case 0: snake[numb][0].x++; break;
        case 1: snake[numb][0].y++; break;
        case 2: snake[numb][0].x--; break;
        case 3: snake[numb][0].y--; break;
    }
}

void sendDisconnect(int socket){
    write(socket, "@Disconnect", sizeof("@Disconnect"));
}

void set_nonblocking(int socket) {
    int flags = fcntl(socket, F_GETFL, 0);
    fcntl(socket, F_SETFL, flags | O_NONBLOCK);
}

void set_blocking(int socket) {
    int flags = fcntl(socket, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return;
    }
    flags &= ~O_NONBLOCK;
    if (fcntl(socket, F_SETFL, flags) == -1) {
        perror("fcntl F_SETFL");
    }
}

char* getNames(int socket){
    static char buffer[64];
    memset(buffer, 0, sizeof(buffer));

    int bytes_read = read(socket, buffer, sizeof(buffer));
    if (bytes_read > 0) {
        printf("Bytes read: %s\n", buffer);
        return buffer;
    }else if (bytes_read == -1) {
        return "@Error";
    }
    return "";
}

void send_UDP(struct sockaddr_in socket, int UDPSocket){
    if(change){
        char buffer[64];
        memset(buffer, 0, sizeof(buffer));
        sprintf(buffer, "%d.%d\n",myNumber, my_direction);
        sendto(UDPSocket, buffer, strlen(buffer), 0, (struct sockaddr *)&socket, sizeof socket);
        printf("Sent UDP: %s\n", buffer);
        change = false;
    }
}

void read_TCP(int socket){
    char buffer[64];
    memset(buffer, 0, sizeof(buffer));
    int total_bytes_read = 0;
    int bytes_read;
    
    while (total_bytes_read < sizeof(buffer) - 1) {
        bytes_read = read(socket, buffer + total_bytes_read, sizeof(buffer) - 1 - total_bytes_read);
        if (bytes_read > 0) {
            total_bytes_read += bytes_read;
        } else if (bytes_read == 0) {
            break;
        } else if (bytes_read == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else {
                break;
            }
        }
    }

    buffer[total_bytes_read] = '\0';
    if (total_bytes_read > 0) {
        if(strcmp(buffer, "@End") == 0){
            on_game_screen = false;
            on_end_screen = true;
        }
        int player, direction;
        char *ptr = buffer;
        while (sscanf(ptr, "%d.%d.", &player, &direction) == 2) {
            if (player >= 0 && player < 4) {
                directions[player] = direction;
            }
            ptr = strchr(ptr, '.') + 1;
            ptr = strchr(ptr, '.') + 1;
        }
        for(int i = 0; i < MAX_PLAYERS; i++){
            if (--counter[i] < 0) {
                counter[i] = 10;
                snake_length[i]++;
            }
            if(directions[i] != 5){
                move_snake(i, directions[i]);
            }
        }
        last_accepted_move = directions[myNumber];
    }
}

void call_launcher(struct sockaddr_in launcher, char* port){
    printf("Calling launcher, %s\n", port);
    int SocketFD = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sendto(SocketFD, port, strlen(port), 0, (struct sockaddr *)&launcher, sizeof launcher);
    close(SocketFD);
}
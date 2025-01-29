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


//gcc -o snake_game client.c -lSDL2 -lm -lSDL2_ttf

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
int snake_length;
Point food;
int my_direction = 0; //gets my direction
bool running = true; //game running
bool try_connect = false; //try to connect to server
bool on_start_screen = true; //start screen
bool on_connect_screen = false;
bool on_wait_screen = false;
bool on_game_screen = false;
bool on_end_screen = false;
bool connect_TCP = false;
bool connect_UDP = false;
char myName[64] = ""; //my name
char serverAdress[64] = "";
int directions[4] = {0,0,0,0};
int mynumber = -1;
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

int counter = 10;
int currentplayers = 0;

void render_start_screen(SDL_Renderer *renderer); //start screen
void render_game_screen(SDL_Renderer *renderer); //actual game
void render_end_screen(SDL_Renderer *renderer); //end screen
int render_connect_screen(SDL_Renderer *renderer); //waiting for connection to server, returns my number
int render_players_screen(SDL_Renderer *renderer, int socket); //wawiting for players 
int* getUDPData(int socket, struct sockaddr_in *serverAdress); //get direction of playes according to server
char* getNames( int socket );
void set_nonblocking(int socket); //set TCP to non blocking - needed for reading player connecting
void sendDisconnect(int socket);

int main(int argc, char *argv[]) {
    pthread_t threadUDP;
    struct sockaddr_in* UDPsock;
    int portTCP = -1;
    int portUDP = -1;
    int UDPSocket = socket(PF_INET, SOCK_DGRAM, 0);
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *window = SDL_CreateWindow("Snake", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    srand(time(NULL));

    SDL_Event e;
    bool running = true;
    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
            }
            else if (!on_game_screen && e.type == SDL_KEYDOWN) {
                pthread_mutex_lock(&client_mutex);
                switch (e.key.keysym.sym) {
                    case SDLK_UP:    if (my_direction != 1) my_direction = 3; break;
                    case SDLK_DOWN:  if (my_direction != 3) my_direction = 1; break;
                    case SDLK_LEFT:  if (my_direction != 0) my_direction = 2; break;
                    case SDLK_RIGHT: if (my_direction != 2) my_direction = 0; break;
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
            }
        }
        else if(on_wait_screen){
            portUDP = render_players_screen(renderer, portTCP);
        }
        else if(on_game_screen){
            
        }
        else if(on_end_screen){

        }
        else if(connect_UDP){

        }
        SDL_RenderPresent(renderer);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
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
    SDL_Event e;
    SDL_PollEvent(&e);
    SDL_Rect textRect = {button.x + 10, button.y + 10, textSurface->w, textSurface->h};
    if (e.type == SDL_MOUSEBUTTONDOWN) {
        int x = e.button.x;
        int y = e.button.y;
        if (x >= 350 && x <= 450 && y >= 275 && y <= 325) {
            on_start_screen = false;
            on_connect_screen = true;
        }
    }
    SDL_FreeSurface(textSurface);
    SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
    SDL_DestroyTexture(textTexture);
    TTF_CloseFont(font);
    TTF_Quit();
}

void render_game_screen(SDL_Renderer *renderer){
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    for(int j = 0; j < currentplayers; j++){
        if(directions[j] != 5){
            for (int i = 0; i < snake_length; i++) {
                SDL_Rect segment = {snake[j][i].x * GRID_SIZE, snake[j][i].y * GRID_SIZE, GRID_SIZE, GRID_SIZE};
                SDL_RenderFillRect(renderer, &segment);
            }
        }
    }
}

int render_connect_screen(SDL_Renderer *renderer){
    if (TTF_Init() < 0) {
        printf("Lib - connect\n");
        return - 1;
    }
    TTF_Font* font = TTF_OpenFont("font.ttf", 24);
    if (!font) {
        printf("Font - connect\n");
        return -1;
    }
    SDL_Color textColor = {255, 255, 255, 255};
    SDL_Rect input1Rect = {100, 100, 200, 40};
    SDL_Rect input2Rect = {100, 160, 200, 40};
    SDL_Rect buttonRect = {100, 220, 200, 40};
    SDL_Rect input3Rect = {100, 280, 200, 40};
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
            if (e.type == SDL_MOUSEBUTTONDOWN) { //if button
                int x, y;
                SDL_GetMouseState(&x, &y);
                if (x >= buttonRect.x && x <= buttonRect.x + buttonRect.w &&
                    y >= buttonRect.y && y <= buttonRect.y + buttonRect.h) {
                    try_connect = !try_connect;
                    printf("tryConnect = %s\n", try_connect ? "true" : "false");
                }
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
            if (e.type == SDL_MOUSEBUTTONDOWN) { //if write in box
                int x, y;
                SDL_GetMouseState(&x, &y);
                input1Focused = (x >= input1Rect.x && x <= input1Rect.x + input1Rect.w &&
                                 y >= input1Rect.y && y <= input1Rect.y + input1Rect.h);
                input2Focused = (x >= input2Rect.x && x <= input2Rect.x + input2Rect.w &&
                                 y >= input2Rect.y && y <= input2Rect.y + input2Rect.h);
                input3Focused = (x >= input3Rect.x && x <= input3Rect.x + input3Rect.w &&
                                 y >= input3Rect.y && y <= input3Rect.y + input3Rect.h);
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

        if(try_connect){
            try_connect = false;
            printf("TRYING\n");
            struct sockaddr_in sa;
            int SocketFD;
            printf("%s\n", input1Text);
            printf("%s\n", input2Text);
            int port=atoi(input2Text); //get it in the windows
            SocketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (SocketFD == -1) {
                printf("cannot create socket\n");
                try_connect = false;
                return -1;
            }
            memset(&sa, 0, sizeof sa);
            
            sa.sin_addr.s_addr=inet_addr(input1Text); //get it from window
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
                write(SocketFD, input3Text, sizeof input3Text);
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
            write(SocketFD, input3Text, sizeof input3Text);
            strcpy(serverAdress, input1Text);
            return SocketFD;
        }
    }
    TTF_CloseFont(font);
}

int render_players_screen(SDL_Renderer *renderer, int socket){
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
    bool getAdress = false;
    while(!quit){
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
        
        if(strcmp("", name) != 0){ //it read smth
            if(strcmp("@Error", name) == 0){ //-1 got returned
            printf("E1\n");
                continue;
            }
            else if(getPort){
                int port = atoi(name);
                printf("Port: %d\n", port);
                return port;
            }
            else if(strcmp("@Start\n", name) == 0){ //game started
                on_wait_screen = false;
                getPort = true;
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

void move_snake(int numb, int dir) {
    for (int i = snake_length - 1; i > 0; i--) {
        snake[numb][i] = snake[0][i - 1];
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

char* getNames(int socket){
    static char buffer[1024];
    memset(buffer, 0, sizeof(buffer));

    int bytes_read = read(socket, buffer, sizeof(buffer));
    if (bytes_read > 0) {
        return buffer;
    }else if (bytes_read == -1) {
        return "@Error";
    }
    return "";
}
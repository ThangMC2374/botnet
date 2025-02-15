#include "checks.h"
#include <arpa/inet.h>

int validate_ip(const char *ip) {
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ip, &(sa.sin_addr));
}

int validate_port(int port) {
    return port > 0 && port <= 65535;
}

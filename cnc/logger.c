#include "logger.h"
#include <stdio.h>
#include <string.h>

void log_command(const char* user, const char* ip, const char* command) {
    int filelogs = 1;
    int logips = 1;

    FILE* sf = fopen("settings.txt", "r");
    if (sf) {
        char line[128];
        while (fgets(line, sizeof(line), sf)) {
            if (strncmp(line, "filelogs:", 9) == 0) {
                filelogs = strstr(line, "yes") ? 1 : 0;
            } else if (strncmp(line, "logips:", 7) == 0) {
                logips = strstr(line, "yes") ? 1 : 0;
            }
        }
        fclose(sf);
    }

    char logline[1024];
    if (logips)
        snprintf(logline, sizeof(logline), "[%s] %s ran command: %s\n", ip, user, command);
    else
        snprintf(logline, sizeof(logline), "%s ran command: %s\n", user, command);

    printf("%s", logline);

    if (filelogs) {
        FILE* f = fopen("logs.txt", "a");
        if (f) {
            fputs(logline, f);
            fclose(f);
        }
    }

}
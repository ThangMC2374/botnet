#include "logger.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

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

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestr[20];       // YYYY-MM-DD HH:MM:SS
    char datedir[11];       // YYYY-MM-DD
    strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", tm_info);
    strftime(datedir, sizeof(datedir), "%Y-%m-%d", tm_info);

    char logline[2048];
    if (logips) {
        snprintf(logline, sizeof(logline), "[%s] [%s] %s ran command: %s\n",
                 timestr, ip, user, command);
    } else {
        snprintf(logline, sizeof(logline), "[%s] %s ran command: %s\n",
                 timestr, user, command);
    }

    printf("%s", logline);

    if (filelogs) {
        mkdir("logs", 0755);
        char filepath[256];
        snprintf(filepath, sizeof(filepath), "logs/%s.log", datedir);
        FILE* f = fopen(filepath, "a");
        if (f) {
            fputs(logline, f);
            fclose(f);
        }
    }
}

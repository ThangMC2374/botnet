#include "logger.h"
#include <stdio.h>

void log_command(const char* user, const char* command) {
    printf("%s ran command: %s\n", user, command);
}

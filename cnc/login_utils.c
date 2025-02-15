#include "login_utils.h"
#include <stdio.h>
#include <string.h>

User users[MAX_USERS];
int user_count = 0;

void load_users() {
    FILE *file = fopen("logins.txt", "r");
    while (fscanf(file, "%s %s %d %d", users[user_count].user, users[user_count].pass, &users[user_count].maxtime, &users[user_count].maxbots) != EOF) {
        user_count++;
    }
    fclose(file);
}

int check_login(const char *user, const char *pass) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(user, users[i].user) == 0 && strcmp(pass, users[i].pass) == 0) {
            if (users[i].is_logged_in) {
                return -2;
            }
            return i;
        }
    }
    return -1;
}

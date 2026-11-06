#include "compiler.h"
#include "../../kernel/kernel/kernel.h"

#define MAX_TOKENS 16

int term_strcmp(const char* s1, const char* s2) {
    int i = 0;
    while (s1[i] != '\0' && s2[i] != '\0') {
        if (s1[i] != s2[i]) return s1[i] - s2[i];
        i++;
    }
    return s1[i] - s2[i];
}

int tokenize_command(char* cmd, char* argv[], int max_args) {
    int argc = 0;
    int i = 0;

    if (!cmd) return 0;

    while (cmd[i] != '\0' && argc < max_args) {
        while (cmd[i] == ' ' || cmd[i] == '\t' || cmd[i] == '\r' || cmd[i] == '\n') {
            cmd[i] = '\0';
            i++;
        }

        if (cmd[i] == '\0') break;

        if (cmd[i] == '"') {
            i++;
            argv[argc++] = &cmd[i];

            while (cmd[i] != '\0' && cmd[i] != '"') {
                i++;
            }
            if (cmd[i] == '"') {
                cmd[i] = '\0';
                i++;
            }
        } else {
            argv[argc++] = &cmd[i];
            while (cmd[i] != '\0' && cmd[i] != ' ' && cmd[i] != '\t' && cmd[i] != '\r' && cmd[i] != '\n') {
                i++;
            }
        }
    }
    argv[argc] = 0;
    return argc;
}

int terminal_execute_command(char* cmd, int x, int y, int o) {
    int is_quoted = (cmd && cmd[0] == '"');

    char* argv[MAX_TOKENS];
    int argc = tokenize_command(cmd, argv, MAX_TOKENS);

    if (argc == 0) {
        return 0;
    }

    int out_y = (o + 1) * 15;
    int out_x = 0;

    if (is_quoted) {
        printf(argv[0], out_x, out_y, 255, 255, 255, 255);
        return 1;
    }

    if (term_strcmp(argv[0], "help") == 0) {
        printf("Available commands:", out_x, out_y, 200, 200, 200, 255);
        printf("  ver - Display OS version", out_x, out_y + 15, 150, 150, 150, 255);
        return 2;
    }

    else if (term_strcmp(argv[0], "ver") == 0) {
        printf("Pillarix OS v4-alpha", out_x, out_y, 0, 255, 100, 255);
        return 1;
    }

    else {
        char err_msg[128];
        const char* prefix = "Unknown command: ";
        int idx = 0;
        for (int k = 0; prefix[k] != '\0' && idx < 100; k++) {
            err_msg[idx++] = prefix[k];
        }
        for (int k = 0; argv[0][k] != '\0' && idx < 120; k++) {
            err_msg[idx++] = argv[0][k];
        }
        err_msg[idx] = '\0';

        printf(err_msg, out_x, out_y, 255, 80, 80, 255);
        return 1;
    }
}

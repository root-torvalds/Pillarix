#include "compiler.h"
#include "../../kernel/kernel/kernel.h"

#define MAX_TOKENS 16

extern void sys_reset(void);

char active_edit_filename[256] = {0};

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
    int line_height = 20;
    int out_y = (o + 1) * line_height;
    int out_x = 15;

    int is_quoted = (cmd && cmd[0] == '"');

    // Проверяем, есть ли закрывающая кавычка, если строка началась с кавычки
    if (is_quoted) {
        int has_closing = 0;
        for (int k = 1; cmd[k] != '\0'; k++) {
            if (cmd[k] == '"') {
                has_closing = 1;
                break;
            }
        }
        if (!has_closing) {
            printf("Error: Missing closing quote", out_x, out_y, 255, 80, 80, 255);
            return 1;
        }
    }

    char* argv[MAX_TOKENS];
    int argc = tokenize_command(cmd, argv, MAX_TOKENS);

    if (argc == 0) {
        return 0;
    }

    if (is_quoted) {
        printf(argv[0], out_x, out_y, 255, 255, 255, 255);
        return 1;
    }

    if (term_strcmp(argv[0], "help") == 0) {
        printf("Available commands:", out_x, out_y, 200, 200, 200, 255);
        printf("  ver    - Display OS version", out_x, out_y + line_height, 150, 150, 150, 255);
        printf("  clear  - Clear terminal screen", out_x, out_y + line_height * 2, 150, 150, 150, 255);
        printf("  reboot - Restart the system", out_x, out_y + line_height * 3, 150, 150, 150, 255);
        printf("  ls     - List directory contents", out_x, out_y + line_height * 4, 150, 150, 150, 255);
        printf("  cat    - Display file contents", out_x, out_y + line_height * 5, 150, 150, 150, 255);
        printf("  touch  - Create an empty file", out_x, out_y + line_height * 6, 150, 150, 150, 255);
        printf("  edit   - Edit file (RShift+Enter to save)", out_x, out_y + line_height * 7, 150, 150, 150, 255);
        return 8;
    } else if (term_strcmp(argv[0], "ver") == 0) {
        printf("Pillarix OS v4-alpha", out_x, out_y, 0, 255, 100, 255);
        return 1;
    } else if (term_strcmp(argv[0], "reboot") == 0) {
        sys_reset();
        return 1;
    } else if (term_strcmp(argv[0], "ls") == 0) {
        const char* path = (argc > 1) ? argv[1] : "/";
        return ext4_ls(path, out_x, out_y);
    } else if (term_strcmp(argv[0], "cat") == 0) {
        if (argc > 1) {
            char full_path[300];
            if (argv[1][0] != '/') {
                full_path[0] = '/';
                int idx = 0;
                while (argv[1][idx] != '\0' && idx < 290) {
                    full_path[idx + 1] = argv[1][idx];
                    idx++;
                }
                full_path[idx + 1] = '\0';
            } else {
                int idx = 0;
                while (argv[1][idx] != '\0' && idx < 290) {
                    full_path[idx] = argv[1][idx];
                    idx++;
                }
                full_path[idx] = '\0';
            }

            return ext4_cat(full_path, out_x, out_y);
        } else {
            printf("Usage: cat <filename>", out_x, out_y, 255, 80, 80, 255);
            return 1;
        }
    } else if (term_strcmp(argv[0], "touch") == 0) {
        if (argc > 1) {
            ext4_create_file("/", argv[1], 0x8180, "", 0);
            printf("File created successfully", out_x, out_y, 0, 255, 0, 255);
            return 1;
        } else {
            printf("Usage: touch <filename>", out_x, out_y, 255, 80, 80, 255);
            return 1;
        }
    } else if (term_strcmp(argv[0], "edit") == 0) {
        if (argc > 1) {
            for (int k = 0; argv[1][k] != '\0' && k < 255; k++) {
                active_edit_filename[k] = argv[1][k];
                active_edit_filename[k+1] = '\0';
            }
            return -2;
        } else {
            printf("Usage: edit <filename>", out_x, out_y, 255, 80, 80, 255);
            return 1;
        }
    } else if (term_strcmp(argv[0], "clear") == 0) {
        return -1;
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

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


// ---------------- math: калькулятор ----------------
typedef struct {
    const char* p;
    int err;      // 0 = ок, 1 = синтаксис, 2 = деление на ноль
} MathParser;

static void math_skip(MathParser* m) {
    while (*m->p == ' ' || *m->p == '\t') m->p++;
}

static double math_expr(MathParser* m);

static double math_primary(MathParser* m) {
    math_skip(m);
    if (*m->p == '(') {
        m->p++;
        double v = math_expr(m);
        math_skip(m);
        if (*m->p != ')') { if (!m->err) m->err = 1; return 0; }
        m->p++;
        return v;
    }
    if ((*m->p >= '0' && *m->p <= '9') || *m->p == '.') {
        double v = 0;
        int digits = 0;
        while (*m->p >= '0' && *m->p <= '9') {
            v = v * 10.0 + (*m->p - '0');
            m->p++; digits++;
        }
        if (*m->p == '.') {
            m->p++;
            double scale = 0.1;
            while (*m->p >= '0' && *m->p <= '9') {
                v += (*m->p - '0') * scale;
                scale /= 10.0;
                m->p++; digits++;
            }
        }
        if (digits == 0) { if (!m->err) m->err = 1; return 0; }
        return v;
    }
    if (!m->err) m->err = 1;
    return 0;
}

static double math_unary(MathParser* m) {
    math_skip(m);
    if (*m->p == '-') { m->p++; return -math_unary(m); }
    if (*m->p == '+') { m->p++; return math_unary(m); }
    return math_primary(m);
}

static double math_term(MathParser* m) {
    double v = math_unary(m);
    for (;;) {
        math_skip(m);
        if (*m->p == '*') {
            m->p++;
            v *= math_unary(m);
        } else if (*m->p == '/') {
            m->p++;
            double d = math_unary(m);
            if (d == 0.0) { if (!m->err) m->err = 2; return 0; }
            v /= d;
        } else break;
    }
    return v;
}

static double math_expr(MathParser* m) {
    double v = math_term(m);
    for (;;) {
        math_skip(m);
        if (*m->p == '+') { m->p++; v += math_term(m); }
        else if (*m->p == '-') { m->p++; v -= math_term(m); }
        else break;
    }
    return v;
}

// double -> строка (до 8 знаков после точки, хвостовые нули убираются). Без libc.
static int math_format(double v, char* out) {
    int n = 0;
    if (v != v) { const char* s = "nan"; while (*s) out[n++] = *s++; out[n] = 0; return n; }
    if (v < 0) { out[n++] = '-'; v = -v; }
    if (v >= 1e18) { const char* s = "overflow"; while (*s) out[n++] = *s++; out[n] = 0; return n; }

    uint64_t ip = (uint64_t)v;
    double frac = v - (double)ip;
    uint64_t f = (uint64_t)(frac * 100000000.0 + 0.5);
    if (f >= 100000000ULL) { ip++; f -= 100000000ULL; }

    char tmp[24];
    int t = 0;
    if (ip == 0) tmp[t++] = '0';
    while (ip > 0) { tmp[t++] = '0' + (int)(ip % 10); ip /= 10; }
    while (t > 0) out[n++] = tmp[--t];

    if (f > 0) {
        char fd[8];
        for (int i = 7; i >= 0; i--) { fd[i] = '0' + (int)(f % 10); f /= 10; }
        int last = 7;
        while (last >= 0 && fd[last] == '0') last--;
        out[n++] = '.';
        for (int i = 0; i <= last; i++) out[n++] = fd[i];
    }
    out[n] = 0;
    return n;
}

int terminal_execute_command(char* cmd, int x, int y, int o) {
    int line_height = 20;
    int out_y = (o + 1) * line_height;
    int out_x = 15;

    int is_quoted = (cmd && cmd[0] == '"');

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

    extern int ext4_install(int x, int y);

    if (term_strcmp(argv[0], "help") == 0) {
        printf("Available commands:", out_x, out_y, 200, 200, 200, 255);
        printf("  ver     - Display OS version", out_x, out_y + line_height, 150, 150, 150, 255);
        printf("  clear   - Clear terminal screen", out_x, out_y + line_height * 2, 150, 150, 150, 255);
        printf("  reboot  - Restart the system", out_x, out_y + line_height * 3, 150, 150, 150, 255);
        printf("  ls      - List directory contents", out_x, out_y + line_height * 4, 150, 150, 150, 255);
        printf("  cat     - Display file contents", out_x, out_y + line_height * 5, 150, 150, 150, 255);
        printf("  touch   - Create an empty file", out_x, out_y + line_height * 6, 150, 150, 150, 255);
        printf("  edit    - Edit file (RShift+Enter to save)", out_x, out_y + line_height * 7, 150, 150, 150, 255);
        printf("  install - Format disk and install OS", out_x, out_y + line_height * 8, 150, 150, 150, 255);
        printf("  math    - Calculator: math 2*(3+4)", out_x, out_y + line_height * 9, 150, 150, 150, 255);
        return 10;
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
            char dir_path[300];
            char file_name[256];
            ext4_split_path(argv[1], dir_path, file_name);

            int res = ext4_create_file(dir_path, file_name, 0x8180, "", 0);
            if (res == 0) {
                printf("File created successfully", out_x, out_y, 0, 255, 0, 255);
            } else {
                // Выводим точный код ошибки для диагностики
                char err_buf[64];
                // Простая конвертация кода ошибки в строку или вывод текста
                if (res == -1) printf("touch: err -1 (Parent dir not found)", out_x, out_y, 255, 80, 80, 255);
                else if (res == -2) printf("touch: err -2 (Parent inode read fail)", out_x, out_y, 255, 80, 80, 255);
                else if (res == -3) printf("touch: err -3 (Parent block is 0)", out_x, out_y, 255, 80, 80, 255);
                else if (res == -4) printf("touch: err -4 (Dir block read fail)", out_x, out_y, 255, 80, 80, 255);
                else if (res == -5) printf("touch: err -5 (File inode read fail)", out_x, out_y, 255, 80, 80, 255);
                else if (res == -6) printf("touch: err -6 (AHCI disk write error)", out_x, out_y, 255, 80, 80, 255);
                else if (res == -7) printf("touch: err -7 (Inode table write error)", out_x, out_y, 255, 80, 80, 255);
                else printf("touch: failed with unknown code", out_x, out_y, 255, 80, 80, 255);
            }
            return 1;
        } else {
            printf("Usage: touch <filename>", out_x, out_y, 255, 80, 80, 255);
            return 1;
        }
    } else if (term_strcmp(argv[0], "edit") == 0) {
        if (argc > 1 && argv[1][0] != '\0') {
            int dest_idx = 0;
            int src_idx = 0;
            while (argv[1][src_idx] != '\0' && dest_idx < 255) {
                active_edit_filename[dest_idx++] = argv[1][src_idx++];
            }
            active_edit_filename[dest_idx] = '\0';
            return -2; // Запуск графического редактора
        } else {
            printf("Usage: edit <filename>", out_x, out_y, 255, 80, 80, 255);
            return 1;
        }
    } else if (term_strcmp(argv[0], "math") == 0) {
        if (argc < 2) {
            printf("Usage: math <expression>  e.g. math 2*(3+4)", out_x, out_y, 255, 80, 80, 255);
            return 1;
        }
        // склеиваем все аргументы, чтобы работало и "math 2 * 3"
        char expr[256];
        int ei = 0;
        for (int a = 1; a < argc; a++) {
            for (int k = 0; argv[a][k] != '\0' && ei < 250; k++) expr[ei++] = argv[a][k];
            if (ei < 250) expr[ei++] = ' ';
        }
        expr[ei] = '\0';

        MathParser mp;
        mp.p = expr;
        mp.err = 0;
        double result = math_expr(&mp);
        math_skip(&mp);
        if (!mp.err && *mp.p != '\0') mp.err = 1; // остался мусор в конце

        if (mp.err == 2) {
            printf("math: division by zero", out_x, out_y, 255, 80, 80, 255);
        } else if (mp.err) {
            printf("math: syntax error", out_x, out_y, 255, 80, 80, 255);
        } else {
            char res_buf[48];
            math_format(result, res_buf);
            printf(res_buf, out_x, out_y, 0, 255, 100, 255);
        }
        return 1;
    } else if (term_strcmp(argv[0], "install") == 0) {
        mkfs_ext4();
        printf("OS installed successfully! Filesystem formatted.", out_x, out_y, 0, 255, 0, 255);
        return 1;
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

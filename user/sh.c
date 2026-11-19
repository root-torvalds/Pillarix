#include "px.h"

#define LINE_HEIGHT   20
#define CHAR_W        10
#define MAX_LINES     50
#define HISTORY_MAX   16
#define CMD_MAX_LEN   512
#define EDIT_MAX      4096
#define MAX_TOKENS    16

static char current_dir[256] = "/";
static char cmd_history[HISTORY_MAX][CMD_MAX_LEN];
static int history_count = 0;
static int history_index = 0;
static char current_draft[CMD_MAX_LEN] = {0};

static const char kbd_make_normal[256] = {
    [0x02] = '1',  [0x03] = '2',  [0x04] = '3',  [0x05] = '4',  [0x06] = '5',
    [0x07] = '6',  [0x08] = '7',  [0x09] = '8',  [0x0A] = '9',  [0x0B] = '0',
    [0x0C] = '-',  [0x0D] = '=',  [0x29] = '`',  [0x1A] = '[',  [0x1B] = ']',
    [0x27] = ';',  [0x28] = '\'', [0x33] = ',',  [0x34] = '.',  [0x35] = '/',
    [0x2B] = '\\', [0x1E] = 'a',  [0x30] = 'b',  [0x2E] = 'c',  [0x20] = 'd',
    [0x12] = 'e',  [0x21] = 'f',  [0x22] = 'g',  [0x23] = 'h',  [0x17] = 'i',
    [0x24] = 'j',  [0x25] = 'k',  [0x26] = 'l',  [0x32] = 'm',  [0x31] = 'n',
    [0x18] = 'o',  [0x19] = 'p',  [0x10] = 'q',  [0x13] = 'r',  [0x1F] = 's',
    [0x14] = 't',  [0x16] = 'u',  [0x2F] = 'v',  [0x11] = 'w',  [0x2D] = 'x',
    [0x15] = 'y',  [0x2C] = 'z'
};

static const char kbd_make_shift[256] = {
    [0x02] = '!',  [0x03] = '@',  [0x04] = '#',  [0x05] = '$',  [0x06] = '%',
    [0x07] = '^',  [0x08] = '&',  [0x09] = '*',  [0x0A] = '(',  [0x0B] = ')',
    [0x0C] = '_',  [0x0D] = '+',  [0x29] = '~',  [0x1A] = '{',  [0x1B] = '}',
    [0x27] = ':',  [0x28] = '\"', [0x33] = '<',  [0x34] = '>',  [0x35] = '?',
    [0x2B] = '|',  [0x1E] = 'A',  [0x30] = 'B',  [0x2E] = 'C',  [0x20] = 'D',
    [0x12] = 'E',  [0x21] = 'F',  [0x22] = 'G',  [0x23] = 'H',  [0x17] = 'I',
    [0x24] = 'J',  [0x25] = 'K',  [0x26] = 'L',  [0x32] = 'M',  [0x31] = 'N',
    [0x18] = 'O',  [0x19] = 'P',  [0x10] = 'Q',  [0x13] = 'R',  [0x1F] = 'S',
    [0x14] = 'T',  [0x16] = 'U',  [0x2F] = 'V',  [0x11] = 'W',  [0x2D] = 'X',
    [0x15] = 'Y',  [0x2C] = 'Z'
};

static int str_len(const char* s) {
    int n = 0;
    while (s[n] != '\0') n++;
    return n;
}

static int str_cmp(const char* a, const char* b) {
    int i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
        i++;
    }
    return (unsigned char)a[i] - (unsigned char)b[i];
}

static void str_copy(char* dst, const char* src, int max) {
    int i = 0;
    while (src[i] != '\0' && i < max - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void int_to_str(int64_t v, char* out) {
    char tmp[24];
    int t = 0, n = 0;
    if (v == 0) { out[0] = '0'; out[1] = '\0'; return; }
    if (v < 0) { out[n++] = '-'; v = -v; }
    while (v > 0) { tmp[t++] = '0' + (int)(v % 10); v /= 10; }
    while (t > 0) out[n++] = tmp[--t];
    out[n] = '\0';
}

static void u64_to_hex(uint64_t v, char* out) {
    const char* hex = "0123456789ABCDEF";
    out[0] = '0'; out[1] = 'x';
    int started = 0, pos = 2;
    for (int shift = 60; shift >= 0; shift -= 4) {
        uint8_t d = (uint8_t)((v >> shift) & 0xF);
        if (d != 0 || started || shift == 0) {
            started = 1;
            out[pos++] = hex[d];
        }
    }
    out[pos] = '\0';
}

static void history_add(const char* cmd) {
    if (cmd[0] == '\0') return;
    if (history_count > 0 && str_cmp(cmd_history[history_count - 1], cmd) == 0) {
        history_index = history_count;
        return;
    }
    if (history_count < HISTORY_MAX) {
        str_copy(cmd_history[history_count++], cmd, CMD_MAX_LEN);
    } else {
        for (int k = 0; k < HISTORY_MAX - 1; k++) {
            str_copy(cmd_history[k], cmd_history[k + 1], CMD_MAX_LEN);
        }
        str_copy(cmd_history[HISTORY_MAX - 1], cmd, CMD_MAX_LEN);
    }
    history_index = history_count;
}

static int tokenize(char* cmd, char* argv[], int max_args) {
    int argc = 0, i = 0;
    while (cmd[i] != '\0' && argc < max_args) {
        while (cmd[i] == ' ' || cmd[i] == '\t' || cmd[i] == '\r' || cmd[i] == '\n') {
            cmd[i++] = '\0';
        }
        if (cmd[i] == '\0') break;
        if (cmd[i] == '"') {
            i++;
            argv[argc++] = &cmd[i];
            while (cmd[i] != '\0' && cmd[i] != '"') i++;
            if (cmd[i] == '"') cmd[i++] = '\0';
        } else {
            argv[argc++] = &cmd[i];
            while (cmd[i] != '\0' && cmd[i] != ' ' && cmd[i] != '\t' && cmd[i] != '\r' && cmd[i] != '\n') i++;
        }
    }
    argv[argc] = 0;
    return argc;
}

static void resolve_path(const char* input, char* out, int max_len) {
    char raw[512];
    int r = 0;
    if (!input || input[0] == '\0') {
        str_copy(out, current_dir, max_len);
        return;
    }
    if (input[0] == '/') {
        for (int i = 0; input[i] != '\0' && r < 510; i++) raw[r++] = input[i];
    } else {
        for (int i = 0; current_dir[i] != '\0' && r < 510; i++) raw[r++] = current_dir[i];
        if (r > 0 && raw[r - 1] != '/' && r < 510) raw[r++] = '/';
        for (int i = 0; input[i] != '\0' && r < 510; i++) raw[r++] = input[i];
    }
    raw[r] = '\0';

    const char* parts[32];
    int depth = 0, idx = 0;
    while (raw[idx] != '\0') {
        while (raw[idx] == '/') raw[idx++] = '\0';
        if (raw[idx] == '\0') break;
        const char* seg = &raw[idx];
        while (raw[idx] != '\0' && raw[idx] != '/') idx++;
        if (str_cmp(seg, ".") == 0) continue;
        if (str_cmp(seg, "..") == 0) { if (depth > 0) depth--; }
        else if (depth < 32) parts[depth++] = seg;
    }
    if (depth == 0) { out[0] = '/'; out[1] = '\0'; return; }
    int o = 0;
    for (int d = 0; d < depth; d++) {
        if (o < max_len - 1) out[o++] = '/';
        for (int k = 0; parts[d][k] != '\0' && o < max_len - 1; k++) out[o++] = parts[d][k];
    }
    out[o] = '\0';
}

/* ---------------- math parser ---------------- */
typedef struct { const char* p; int err; } MathParser;
static void math_skip(MathParser* m) { while (*m->p == ' ' || *m->p == '\t') m->p++; }
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
        double v = 0; int digits = 0;
        while (*m->p >= '0' && *m->p <= '9') { v = v * 10.0 + (*m->p - '0'); m->p++; digits++; }
        if (*m->p == '.') {
            m->p++;
            double scale = 0.1;
            while (*m->p >= '0' && *m->p <= '9') { v += (*m->p - '0') * scale; scale /= 10.0; m->p++; digits++; }
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
        if (*m->p == '*') { m->p++; v *= math_unary(m); }
        else if (*m->p == '/') {
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

static void math_format(double v, char* out) {
    int n = 0;
    if (v != v) { str_copy(out, "nan", 16); return; }
    if (v < 0) { out[n++] = '-'; v = -v; }
    if (v >= 1e18) { str_copy(out, "overflow", 16); return; }
    uint64_t ip = (uint64_t)v;
    uint64_t f = (uint64_t)((v - (double)ip) * 100000000.0 + 0.5);
    if (f >= 100000000ULL) { ip++; f -= 100000000ULL; }
    char tmp[24]; int t = 0;
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
    out[n] = '\0';
}

/* ---------------- встроенный текстовый редактор ---------------- */
static void run_editor(const char* path, int* line_out) {
    static char edit_buf[EDIT_MAX + 1];
    int len = 0;
    int r = fs_read(path, edit_buf, EDIT_MAX);
    if (r > 0) len = r;
    edit_buf[len] = '\0';

    int lshift = 0, rshift = 0;
    for (;;) {
        fillScreen(0, 0, 0, 255);
        drawText("--- EDIT (RShift+Enter = save & exit, ESC = cancel) ---", 0, 20, 200, 200, 0, 255);
        drawText(path, 600, 20, 100, 200, 255, 255);

        int cx = 0, cy = 50;
        for (int i = 0; i < len; i++) {
            if (edit_buf[i] == '\n') { cx = 0; cy += LINE_HEIGHT; }
            else {
                char s[2] = { edit_buf[i], '\0' };
                drawText(s, cx, cy, 255, 255, 255, 255);
                cx += CHAR_W;
                if (cx > 1880) { cx = 0; cy += LINE_HEIGHT; }
            }
        }
        drawText("_", cx, cy, 0, 255, 255, 255);
        swapBuffers();

        int sc = get_scancode(1);
        if (sc == 0x2A) { lshift = 1; continue; }
        if (sc == 0xAA) { lshift = 0; continue; }
        if (sc == 0x36) { rshift = 1; continue; }
        if (sc == 0xB6) { rshift = 0; continue; }
        if (sc == 0x01) { /* ESC */
            fillScreen(0, 0, 0, 255);
            *line_out = 1;
            drawText("Edit cancelled.", 15, LINE_HEIGHT, 255, 255, 0, 255);
            (*line_out) += 2;
            return;
        }
        if (sc == 0x1C) {
            if (rshift) {
                int wres = fs_write(path, edit_buf, (uint64_t)len);
                fillScreen(0, 0, 0, 255);
                *line_out = 1;
                if (wres == 0) drawText("File saved successfully.", 15, LINE_HEIGHT, 0, 255, 0, 255);
                else           drawText("Error saving file.", 15, LINE_HEIGHT, 255, 80, 80, 255);
                (*line_out) += 2;
                return;
            } else if (len < EDIT_MAX) {
                edit_buf[len++] = '\n';
                edit_buf[len] = '\0';
            }
        } else if (sc == 0x0E) {
            if (len > 0) edit_buf[--len] = '\0';
        } else if (sc < 128) {
            char c = (sc == 0x39) ? ' ' : ((lshift || rshift) ? kbd_make_shift[sc] : kbd_make_normal[sc]);
            if (c != 0 && len < EDIT_MAX) {
                edit_buf[len++] = c;
                edit_buf[len] = '\0';
            }
        }
    }
}

static void redraw_prompt(int line, const char* cmd, int len, int cur_pos, int show_cursor) {
    int y = line * LINE_HEIGHT;
    drawRect(0, y - 15, 1920, 24, 0, 0, 0, 255);

    int dir_len = str_len(current_dir);
    drawText(current_dir, 0, y, 100, 200, 255, 255);
    drawText(">", dir_len * CHAR_W, y, 255, 255, 255, 255);

    int offset = (dir_len * CHAR_W) + 12;
    for (int idx = 0; idx < len; idx++) {
        char s[2] = { cmd[idx], '\0' };
        drawText(s, offset + idx * CHAR_W, y, 255, 255, 255, 255);
    }
    if (show_cursor) {
        drawText("_", offset + cur_pos * CHAR_W, y, 0, 255, 255, 255);
    }
    swapBuffers();
}

static int exec_command(char* cmd, int* line_ptr) {
    int o = *line_ptr;
    int out_x = 15;
    int out_y = (o + 1) * LINE_HEIGHT;

    char* argv[MAX_TOKENS];
    int argc = tokenize(cmd, argv, MAX_TOKENS);
    if (argc == 0) return 0;

    if (str_cmp(argv[0], "help") == 0) {
        drawText("Available commands:", out_x, out_y, 200, 200, 200, 255);
        const char* help_lines[] = {
            "  ver      - Display OS version",
            "  clear    - Clear terminal screen",
            "  reboot   - Restart the system",
            "  shutdown - Power off the system",
            "  ls       - List directory contents",
            "  cd       - Change current directory",
            "  pwd      - Print working directory",
            "  mkdir    - Create a new directory",
            "  cat      - Display file contents",
            "  touch    - Create an empty file",
            "  edit     - Edit file (RShift+Enter to save)",
            "  rm       - Remove file or empty directory",
            "  install  - Format disk and install OS",
            "  math     - Calculator: math 2*(3+4)",
            "  ./name   - Run program (add & for background)",
            "  cores    - Show SMP cores and load",
            "  ps       - Show running tasks"
        };
        for (int i = 0; i < 17; i++) {
            drawText(help_lines[i], out_x, out_y + (i + 1) * LINE_HEIGHT, 150, 150, 150, 255);
        }
        return 18;
    } else if (str_cmp(argv[0], "ver") == 0) {
        drawText("Pillarix OS v4C-RC (User Space Shell)", out_x, out_y, 0, 255, 100, 255);
        return 1;
    } else if (str_cmp(argv[0], "clear") == 0) {
        fillScreen(0, 0, 0, 255);
        *line_ptr = 1;
        return -1;
    } else if (str_cmp(argv[0], "reboot") == 0) {
        sys_reboot();
        return 1;
    } else if (str_cmp(argv[0], "shutdown") == 0) {
        sys_shutdown();
        return 1;
    } else if (str_cmp(argv[0], "pwd") == 0) {
        drawText(current_dir, out_x, out_y, 255, 255, 255, 255);
        return 1;
    } else if (str_cmp(argv[0], "cd") == 0) {
        char resolved[300];
        resolve_path((argc > 1) ? argv[1] : "/", resolved, sizeof(resolved));
        if (!fs_is_dir(resolved)) {
            drawText("cd: No such directory", out_x, out_y, 255, 80, 80, 255);
            return 1;
        }
        str_copy(current_dir, resolved, sizeof(current_dir));
        return 0;
    } else if (str_cmp(argv[0], "ls") == 0) {
        static PxDirEntry entries[64];
        char resolved[300];
        resolve_path((argc > 1) ? argv[1] : "", resolved, sizeof(resolved));
        int cnt = fs_list(resolved, entries, 64);
        if (cnt < 0) {
            drawText("ls: failed or not a directory", out_x, out_y, 255, 80, 80, 255);
            return 1;
        }
        for (int i = 0; i < cnt; i++) {
            char line[260];
            str_copy(line, entries[i].name, 256);
            if (entries[i].file_type == 2) {
                int l = str_len(line);
                line[l] = '/'; line[l + 1] = '\0';
            }
            drawText(line, out_x, out_y + i * LINE_HEIGHT,
                     entries[i].file_type == 2 ? 100 : 255,
                     entries[i].file_type == 2 ? 200 : 255, 255, 255);
        }
        return cnt > 0 ? cnt : 1;
    } else if (str_cmp(argv[0], "cat") == 0) {
        if (argc < 2) { drawText("Usage: cat <file>", out_x, out_y, 255, 80, 80, 255); return 1; }
        static char fbuf[4097];
        char resolved[300];
        resolve_path(argv[1], resolved, sizeof(resolved));
        int n = fs_read(resolved, fbuf, 4096);
        if (n < 0) { drawText("cat: failed to read file", out_x, out_y, 255, 80, 80, 255); return 1; }
        if (n == 0) { drawText("cat: empty file", out_x, out_y, 255, 255, 0, 255); return 1; }
        int lines = 0, lpos = 0;
        char lbuf[128];
        for (int i = 0; i < n; i++) {
            if (fbuf[i] == '\n' || lpos >= 120) {
                lbuf[lpos] = '\0';
                drawText(lbuf, out_x, out_y + lines * LINE_HEIGHT, 255, 255, 255, 255);
                lines++; lpos = 0;
            } else if ((unsigned char)fbuf[i] >= 32) {
                lbuf[lpos++] = fbuf[i];
            }
        }
        if (lpos > 0) {
            lbuf[lpos] = '\0';
            drawText(lbuf, out_x, out_y + lines * LINE_HEIGHT, 255, 255, 255, 255);
            lines++;
        }
        return lines > 0 ? lines : 1;
    } else if (str_cmp(argv[0], "touch") == 0) {
        if (argc < 2) { drawText("Usage: touch <file>", out_x, out_y, 255, 80, 80, 255); return 1; }
        char resolved[300];
        resolve_path(argv[1], resolved, sizeof(resolved));
        if (fs_write(resolved, "", 0) == 0) drawText("File created", out_x, out_y, 0, 255, 0, 255);
        else                                 drawText("touch: error", out_x, out_y, 255, 80, 80, 255);
        return 1;
    } else if (str_cmp(argv[0], "mkdir") == 0) {
        if (argc < 2) { drawText("Usage: mkdir <dir>", out_x, out_y, 255, 80, 80, 255); return 1; }
        char resolved[300];
        resolve_path(argv[1], resolved, sizeof(resolved));
        if (fs_mkdir(resolved) == 0) drawText("Directory created", out_x, out_y, 0, 255, 0, 255);
        else                          drawText("mkdir: error", out_x, out_y, 255, 80, 80, 255);
        return 1;
    } else if (str_cmp(argv[0], "edit") == 0) {
        if (argc < 2) { drawText("Usage: edit <file>", out_x, out_y, 255, 80, 80, 255); return 1; }
        char resolved[300];
        resolve_path(argv[1], resolved, sizeof(resolved));
        run_editor(resolved, line_ptr);
        return -1;
    } else if (str_cmp(argv[0], "rm") == 0) {
        if (argc < 2) { drawText("Usage: rm <path>", out_x, out_y, 255, 80, 80, 255); return 1; }
        char resolved[300];
        resolve_path(argv[1], resolved, sizeof(resolved));
        int rres = fs_remove(resolved);
        if (rres == 0) drawText("Removed successfully", out_x, out_y, 0, 255, 0, 255);
        else if (rres == -21) drawText("rm: directory not empty", out_x, out_y, 255, 80, 80, 255);
        else drawText("rm: failed to remove", out_x, out_y, 255, 80, 80, 255);
        return 1;
    } else if (str_cmp(argv[0], "install") == 0) {
        fs_mkfs();
        str_copy(current_dir, "/", sizeof(current_dir));
        drawText("Filesystem formatted.", out_x, out_y, 0, 255, 0, 255);
        return 1;
    } else if (str_cmp(argv[0], "math") == 0) {
        if (argc < 2) { drawText("Usage: math <expr>", out_x, out_y, 255, 80, 80, 255); return 1; }
        char expr[256]; int ei = 0;
        for (int a = 1; a < argc; a++) {
            for (int k = 0; argv[a][k] != '\0' && ei < 250; k++) expr[ei++] = argv[a][k];
            if (ei < 250) expr[ei++] = ' ';
        }
        expr[ei] = '\0';
        MathParser mp = { expr, 0 };
        double res = math_expr(&mp);
        math_skip(&mp);
        if (!mp.err && *mp.p != '\0') mp.err = 1;
        if (mp.err == 2) drawText("math: division by zero", out_x, out_y, 255, 80, 80, 255);
        else if (mp.err) drawText("math: syntax error", out_x, out_y, 255, 80, 80, 255);
        else {
            char rb[48]; math_format(res, rb);
            drawText(rb, out_x, out_y, 0, 255, 100, 255);
        }
        return 1;
    } else if (str_cmp(argv[0], "cores") == 0) {
        PxCpuInfo cpus[16];
        int n = sys_cores(cpus, 16);
        char buf[32];
        for (int c = 0; c < n; c++) {
            int ry = out_y + c * LINE_HEIGHT;
            drawText("CPU #", out_x, ry, 100, 200, 255, 255);
            int_to_str(cpus[c].cpu_id, buf);
            drawText(buf, out_x + 55, ry, 255, 255, 255, 255);
            drawText("tasks:", out_x + 100, ry, 180, 180, 180, 255);
            int_to_str(cpus[c].task_count, buf);
            drawText(buf, out_x + 170, ry, 0, 255, 100, 255);
            drawText("ticks:", out_x + 230, ry, 180, 180, 180, 255);
            u64_to_hex(cpus[c].ticks, buf);
            drawText(buf, out_x + 300, ry, 255, 255, 255, 255);
        }
        return n > 0 ? n : 1;
    } else if (str_cmp(argv[0], "ps") == 0) {
        PxTaskInfo list[32];
        int cnt = sys_ps(list, 32);
        if (cnt <= 0) { drawText("No active tasks", out_x, out_y, 180, 180, 180, 255); return 1; }
        char buf[16];
        for (int t = 0; t < cnt; t++) {
            int ry = out_y + t * LINE_HEIGHT;
            drawText("PID", out_x, ry, 150, 150, 150, 255);
            int_to_str(list[t].id, buf);
            drawText(buf, out_x + 40, ry, 255, 255, 255, 255);
            drawText("CPU", out_x + 90, ry, 100, 200, 255, 255);
            int_to_str(list[t].cpu_id, buf);
            drawText(buf, out_x + 130, ry, 255, 255, 255, 255);
            drawText(list[t].name, out_x + 180, ry, 0, 255, 100, 255);
        }
        return cnt;
    } else if (argv[0][0] == '.' && argv[0][1] == '/') {
        int bg = (argc > 1 && str_cmp(argv[1], "&") == 0) ? 1 : 0;
        char resolved[300];
        resolve_path(argv[0] + 2, resolved, sizeof(resolved));
        int tid = sys_spawn(resolved);
        if (tid < 0) {
            drawText("./: failed to spawn program", out_x, out_y, 255, 80, 80, 255);
            return 1;
        }
        if (bg) {
            drawText("Started in background", out_x, out_y, 0, 255, 100, 255);
            return 1;
        }
        uint64_t code = sys_wait(tid);
        if (code != 0) {
            char hb[24]; u64_to_hex(code, hb);
            drawText("exit:", out_x, out_y, 255, 255, 0, 255);
            drawText(hb, out_x + 60, out_y, 255, 255, 0, 255);
            return 1;
        }
        return 2;
    }

    drawText("Unknown command (type 'help')", out_x, out_y, 255, 80, 80, 255);
    return 1;
}

int main(void) {
    fillScreen(0, 0, 0, 255);
    drawText("Pillarix User-Space Shell ready.", 0, 20, 0, 255, 100, 255);

    char cmd[CMD_MAX_LEN] = {0};
    int len = 0, cur = 0, line = 2, sh = 0;
    int cursor_visible = 1;
    uint64_t last_blink = sys_get_ticks();

    redraw_prompt(line, cmd, len, cur, cursor_visible);

    for (;;) {
        int sc = get_scancode(0);
        if (sc < 0) {
            uint64_t now = sys_get_ticks();
            if (now - last_blink >= 20) {
                last_blink = now;
                cursor_visible = !cursor_visible;
                redraw_prompt(line, cmd, len, cur, cursor_visible);
            }
            sys_yield();
            continue;
        }
        cursor_visible = 1;
        last_blink = sys_get_ticks();
        if (sc == 0x2A || sc == 0x36) { sh = 1; continue; }
        if (sc == 0xAA || sc == 0xB6) { sh = 0; continue; }

        if (sc == 0x4B) { if (cur > 0) cur--; }
        else if (sc == 0x4D) { if (cur < len) cur++; }
        else if (sc == 0x48) {
            if (history_count > 0 && history_index > 0) {
                if (history_index == history_count) { cmd[len] = '\0'; str_copy(current_draft, cmd, CMD_MAX_LEN); }
                history_index--;
                str_copy(cmd, cmd_history[history_index], CMD_MAX_LEN);
                len = str_len(cmd);
                cur = len;
            }
        } else if (sc == 0x50) {
            if (history_index < history_count) {
                history_index++;
                if (history_index == history_count) str_copy(cmd, current_draft, CMD_MAX_LEN);
                else str_copy(cmd, cmd_history[history_index], CMD_MAX_LEN);
                len = str_len(cmd);
                cur = len;
            }
        } else if (sc == 0x53) {
            if (cur < len) {
                for (int k = cur; k < len - 1; k++) cmd[k] = cmd[k + 1];
                cmd[--len] = '\0';
            }
        } else if (sc == 0x0E) {
            if (cur > 0) {
                for (int k = cur - 1; k < len - 1; k++) cmd[k] = cmd[k + 1];
                len--; cur--; cmd[len] = '\0';
            }
        } else if (sc == 0x1C) {
            redraw_prompt(line, cmd, len, cur, 0);
            if (len > 0) {
                cmd[len] = '\0';
                history_add(cmd);
                current_draft[0] = '\0';
                int res = exec_command(cmd, &line);
                if (res >= 0) line += res + 1;
            } else {
                line++;
            }
            if (line >= MAX_LINES) {
                fillScreen(0, 0, 0, 255);
                line = 1;
            }
            len = 0; cur = 0; cmd[0] = '\0';
        } else if (sc < 128) {
            char c = (sc == 0x39) ? ' ' : (sh ? kbd_make_shift[sc] : kbd_make_normal[sc]);
            if (c != 0 && len < CMD_MAX_LEN - 2) {
                for (int k = len; k > cur; k--) cmd[k] = cmd[k - 1];
                cmd[cur++] = c;
                cmd[++len] = '\0';
            }
        }
        redraw_prompt(line, cmd, len, cur, 1);
    }
    return 0;
}
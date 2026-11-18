#include "px.h"

int main(void) {
    printf("Press any key (q to quit):\n");
    for (;;) {
        char c = sys_getkey();
        if (c == 'q') break;
        char buf[2] = { c, '\n' };
        sys_write(buf, 2);
    }
    printf("Bye!\n");
    return 0;
}

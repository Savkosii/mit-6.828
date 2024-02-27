#include <inc/lib.h>

void
umain(int argc, char **argv)
{
    cprintf("hello e1000\n");
    char *s = "e1000 hello";
    sys_transmit_packet(s, strlen(s) + 1);
    sys_transmit_packet(s, strlen(s) - 5);
}

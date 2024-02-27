#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	int r;
    const char *arg[] = {"hello", NULL};
	cprintf("i am environment %08x\n", thisenv->env_id);
	if ((r = exec("hello", arg)) < 0)
		panic("exec(hello) failed: %e", r);
}

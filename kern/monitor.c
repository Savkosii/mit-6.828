// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/pmap.h>
#include <kern/kdebug.h>
#include <kern/trap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
    { "backtrace", "Dump the debug info of the current instruction", mon_backtrace },
    { "vmmaps", "Display all of the physical page mappings that apply to a particular range of virtual/linear addresses", mon_vmmaps},
    { "setperm", "Set the permission of a page entry specified by virtual/linear address va", mon_setperm},
    { "dump", "Dump the n bytes at either a virtual or physical address.", mon_dump},
};

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.
    cprintf("Stack backtrace:\n");
    uint32_t ebp = read_ebp();
    while (ebp != 0) {
        uint32_t eip = *(uint32_t *)((void *)ebp + 4);
        uint32_t arg1 = *(uint32_t *)((void *)ebp + 8);
        uint32_t arg2 = *(uint32_t *)((void *)ebp + 12);
        uint32_t arg3 = *(uint32_t *)((void *)ebp + 16);
        uint32_t arg4 = *(uint32_t *)((void *)ebp + 20);
        uint32_t arg5 = *(uint32_t *)((void *)ebp + 24);
        cprintf("  ");
        cprintf("ebp %08x  eip %08x  args %08x %08x %08x %08x %08x\n", ebp, eip, arg1, arg2, arg3, arg4, arg5);
        struct Eipdebuginfo info;
        debuginfo_eip(eip, &info);
        cprintf("         ");
        cprintf("%s:%u: %.*s+%u\n", info.eip_file, info.eip_line, 
               info.eip_fn_namelen, info.eip_fn_name, eip - info.eip_fn_addr);
        ebp = *(uint32_t *)ebp;
    }

	return 0;
}


int
mon_vmmaps(int argc, char **argv, struct Trapframe *tf)
{
    if (argc != 2 && argc != 3) {
        cprintf("vmmaps: invalid numbers of arguments\n");
        return -1;
    }
    pte_t *pte;
    uintptr_t start;
    uintptr_t end;
    end = start = strtol(argv[1], NULL, 16);
    if (argc == 3) {
        end = strtol(argv[2], NULL, 16);
    } 
    if (end < start) {
        cprintf("vmmaps: invalid range\n");
    }
    int off_start = PGOFF(start);
    int off_end = PGOFF(end);
    cprintf("%5s%9s%9s%9s%9s%9s\n", "va", "ptx", "pdx", "pgoff", "pa", "perm");
    for (uintptr_t va = start; va <= end; va += PGSIZE) {
        if ((pte = pgdir_walk(kern_pgdir, (void *)va, 0)) == NULL || *pte == 0) {
            cprintf("%08x%5s%9s%9s%9s%9s\n", va, "-", "-", "-", "-", "-");
        } else {
            size_t pdx = PDX(va);
            size_t ptx = PTX(va);
            size_t pgoff = PGOFF(va);
            physaddr_t pa = pgoff + PTE_ADDR(*pte);
            char perm[] = {0};
            if (*pte & PTE_P) {
                strcat(perm, "P");
            }
            if (*pte & PTE_W) {
                strcat(perm, "W");
            }
            if (*pte & PTE_U) {
                strcat(perm, "U");
            }
            if (*pte & PTE_A) {
                strcat(perm, "A");
            }
            if (*pte & PTE_D) {
                strcat(perm, "D");
            }
            if (*pte & PTE_G) {
                strcat(perm, "G");
            }
            if (*perm == 0) {
                strcat(perm, "-");
            }
            cprintf("%08x %08x %08x %08x %08x%5s\n", va, pdx, ptx, pgoff, pa, perm);
        }
        if (va <= end && off_start) {
            off_start = 0;
            va = ROUNDDOWN(va, PGSIZE);
        }
        if (va + PGSIZE > end && off_end && start != end) {
            off_end = 0;
            va = end - PGSIZE;
        }
        // in case of int overflow
        if (va + PGSIZE < va) {
            break;
        }
    }
	return 0;
}

int
mon_setperm(int argc, char **argv, struct Trapframe *tf)
{
    if (argc != 3) {
        cprintf("setperm: invalid numbers of arguments\n");
        return -1;
    }
    pte_t *pte;
    int perm = 0;
    uintptr_t va = strtol(argv[1], NULL, 16);
    if ((pte = pgdir_walk(kern_pgdir, (void *)va, 0)) == NULL || *pte == 0) {
        cprintf("setperm: no such a page refered by %08x", va);
        return -1;
    }
    for (char *p = argv[2]; *p != 0; p++) {
        if (*p == 'P' || *p == 'p') {
            perm |= PTE_P;
        } else if (*p == 'U' || *p == 'u') {
            perm |= PTE_U;
        } else if (*p == 'W' || *p == 'w') {
            perm |= PTE_W;
        } else if (*p == 'A' || *p == 'a') {
            perm |= PTE_A;
        } else if (*p == 'G' || *p == 'g') {
            perm |= PTE_G;
        } else if (*p == 'D' || *p == 'd') {
            perm |= PTE_D;
        } else if (*p == '-' || *p == '0') {
            continue;
        } else {
            cprintf("setperm: invalid permission indicator %c\n", *p);
            return -1;
        }
    }
    *pte = perm;
    return 0;
}

// If va maps pa, then dumping k bytes at va and pa might have different result.
// 
// We should expect the former result as the "correct" one, as virtual memory
// are what users believe they "have" instead of physical memory.
int
mon_dump(int argc, char **argv, struct Trapframe *tf)
{
    if (argc != 2 && argc != 3) {
        cprintf("dump: invalid numbers of arguments\n");
        return -1;
    }
    pte_t *pte;
    uint32_t p = strtol(argv[1], NULL, 16);
    int32_t count = 4;
    if (argc == 3 && (count = strtol(argv[2], NULL, 10)) <= 0) {
        cprintf("dump: invalid count\n");
        return -1;
    }
    uint32_t end = p + count;
    if (end < p) {
        cprintf("dump: address overflow\n");
        return -1;
    }
    if (end >= npages * PGSIZE && p < npages * PGSIZE) {
        cprintf("dump: invalid physical memory range\n");
        return -1;
    }
    int printed = 0;
    // handle virtual address case
    if (p >= npages * PGSIZE) {
        while ((pte = pgdir_walk(kern_pgdir, (void *)p, 0)) != NULL && *pte != 0) {
            uint32_t pgoff = PGOFF((void *)p);
            physaddr_t pa = PTE_ADDR(*pte) + pgoff;
            char *va = KADDR(pa);
            for (; pgoff < PGSIZE && count != 0; p++, pgoff++, count--) {
                cprintf("%d ", *va++);
                printed = 1;
            }
            if (count == 0) {
                cprintf("\n");
                return 0;
            }
        }
        if (printed) {
            cprintf("\n");
        }
        // if there is no physical memory mapped by virtural address p + k, 
        // then dumping k + 1 bytes at p should trigger error
        cprintf("dump: invalid address %08x\n", p);
        return -1;
    }

    // handle physical address case
    char *va = KADDR(p);
    for (size_t i = 0; i < count; i++) {
        cprintf("%d ", va[i]);
    }
    cprintf("\n");
    return 0;
}

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}

#include "inc/assert.h"
#include "inc/error.h"
#include "inc/memlayout.h"
#include "inc/mmu.h"
#include "inc/stdio.h"
#include "inc/syscall.h"
#include "inc/types.h"
#include <inc/lib.h>
#include <inc/elf.h>

#define UTEMP2USTACK(addr)	((void*) (addr) + (USTACKTOP - PGSIZE) - UTEMP)
  
static int
map_segment(uintptr_t va, size_t memsz,
    int fd, size_t filesz, off_t fileoffset, int perm) 
{
    int err;
    size_t va_off;
    if (memsz < filesz) {
        return -E_INVAL;
    }
    if (va + memsz < va) {
        return -E_INVAL;
    }
    if (va + memsz > UTOP) {
        return -E_INVAL;
    }
    if ((va_off = PGOFF(va))) {
        va -= va_off;
        memsz += va_off;
        filesz += va_off;
        fileoffset -= va_off;
    }

	for (size_t off = 0; off < memsz; off += PGSIZE) {
        if (off >= filesz) {
            if ((err = sys_exec_config_page_alloc(0, (void *)va + off, perm))) {
                return err;
            }
            continue;
        }
        if ((err = sys_page_alloc(0, (void *)UTEMP, PTE_P | PTE_U | PTE_W))) {
            return err;
        }
        if ((err = seek(fd, fileoffset + off))) {
            return err;
        }
        ssize_t count;
        if (((count = readn(fd, (void *)UTEMP, MIN(PGSIZE, filesz - off)) < 0))) {
            return count;
        }
        if ((err = sys_exec_config_page_map(0, (void *)UTEMP, 0, (void *)va + off, perm))) {
            return err;
        }
        if ((err = sys_page_unmap(0, (void *)UTEMP))) {
            return err;
        }
	}
	return 0;
}

// Set up the initial stack for the new program image 
// in the page mapped at UTEMP, and then map this page 
// from UTEMP in curenv->env_pgdir to USTACKTOP - PGSIZE in curenv->exec_pgdir
//
// Use UTEMP2USTACK to map the address from UTEMP to USTACKTOP - PGSIZE
// 
// On success, returns 0 and sets *init_esp
// to the initial stack pointer with which the child should start.
// Returns < 0 on failure.
static int
init_stack(const char **argv, uintptr_t *init_esp)
{
    int err;
    void *sp = (void *)UTEMP + PGSIZE;
    if ((err = sys_page_alloc(0, (void *)UTEMP, PTE_P | PTE_W | PTE_U))) {
        return err;
    }
    if (argv != NULL) {
        // Count the number of arguments (argc)
        // and the total amount of space needed for strings (string_size).
        int argc = 0;
        size_t string_size = 0;
        while (argv[argc] != NULL) {
            string_size += strlen(argv[argc++]) + 1;
        }

        char *string_store = sp - string_size;
        uintptr_t *argv_store = (uintptr_t *)(ROUNDDOWN(string_store, 4) - 4 * (argc + 1));
        sp = (void *)(argv_store - 2);
        // Make sure that argv, strings, and the 2 words that hold 'argc'
        // and 'argv' themselves will all fit in a single stack page.
        if (sp < (void *)UTEMP) {
            return -E_NO_MEM;
        }

        //	* Initialize 'argv_store[i]' to point to argument string i,
        //	  for all 0 <= i < argc.
        //	  Also, copy the argument strings from 'argv' into the
        //	  newly-allocated stack page.
        //
        //	* Set 'argv_store[argc]' to 0 to null-terminate the args array.
        //
        //	* Push two more words onto the child's stack below 'args',
        //	  containing the argc and argv parameters to be passed
        //	  to the child's umain() function.
        //	  argv should be below argc on the stack.
        //	  (Again, argv should use an address valid in the child's
        //	  environment.)
        //
        //	* Set *init_esp to the initial stack pointer for the child,
        //	  (Again, use an address valid in the child's environment.)
        for (int i = 0; i < argc; i++) {
            argv_store[i] = UTEMP2USTACK(string_store);
            strcpy(string_store, argv[i]);
            string_store += strlen(argv[i]) + 1;
        }
        argv_store[argc] = 0;
        assert(string_store == (char*)UTEMP + PGSIZE);

        argv_store[-1] = UTEMP2USTACK(argv_store);
        argv_store[-2] = argc;
    }

    if ((err = sys_exec_config_page_map(0, (void *)UTEMP, 
        0, (void *)USTACKTOP - PGSIZE, PTE_W | PTE_U | PTE_P))) {
        return err;
    }

    if ((err = sys_page_unmap(0, (void *)UTEMP))) {
        return err;
    }

	*init_esp = UTEMP2USTACK(sp);
	return 0;
}

int
exec(const char *path, const char *argv[]) 
{
    int fd;
    int err;
	unsigned char elf_buf[512];
    struct Elf *elf = (struct Elf *)elf_buf;

    if ((fd = open(path, O_RDONLY)) < 0) {
        return fd; 
    }
	if (readn(fd, elf_buf, sizeof(elf_buf)) != sizeof(elf_buf)
	    || elf->e_magic != ELF_MAGIC) {
		close(fd);
		return -E_NOT_EXEC;
	}

    if ((err = sys_exec_config_pgdir_alloc(0))) {
        close(fd);
        return err;
    }

    for (size_t pdx = 0; pdx < PDX(UTOP); pdx++) {
        volatile pde_t *pde = &uvpd[pdx];
        if (!(*pde & PTE_P)) {
            continue;
        }
        for (size_t ptx = 0; ptx < NPTENTRIES; ptx++) {
            size_t pn = pdx * NPTENTRIES + ptx;
            void *va = (void *)(pn * PGSIZE);
            volatile pte_t *pte = &uvpt[pn];
            if (*pte & PTE_SHARE) {
                if ((err = sys_exec_config_page_map(0, va, 0, va, *pte & PTE_SYSCALL))) {
                    close(fd);
                    return err;
                }
            }
        }
    }

	struct Proghdr *ph = (struct Proghdr *)(elf_buf + elf->e_phoff);
	struct Proghdr *eph = ph + elf->e_phnum;
	for (; ph < eph; ph++) {
        if (ph->p_type == ELF_PROG_LOAD) {
            int perm = PTE_P | PTE_U;
            if (ph->p_flags & ELF_PROG_FLAG_WRITE) {
                perm |= PTE_W;
            }
            if ((err = map_segment(ph->p_va, ph->p_memsz,
                 fd, ph->p_filesz, ph->p_offset, perm))) {
		        close(fd);
                return err;
            }
        } 
    }

    close(fd);

    uintptr_t sp;
    if ((err = init_stack(argv, &sp))) {
        return err;
    }
    struct Trapframe tf = thisenv->env_tf;
    tf.tf_esp = sp;
    tf.tf_eip = elf->e_entry;
    memset(&tf.tf_regs, 0, sizeof(struct PushRegs));
    if ((err = sys_env_set_pgfault_upcall(0, NULL))) {
        return err;
    }

    if ((err = sys_exec(0, &tf))) {
        return err;
    }

    return -1;
}

// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
    if (!(err & FEC_WR)) {
        panic("pgfault: not a write acess");
    }
    // uvpd[] points to the page directory entries (page tables) of curenv->env_pgdir
    // uvpt[] points to all the page table entries (pages) in curenv->env_pgdir.
    // see https://pdos.csail.mit.edu/6.828/2018/labs/lab4/uvpt.html
    unsigned pn = PGNUM(addr);
    volatile pte_t *pte = &uvpt[pn];
    if (*pte == 0) {
        panic("pgfault: no such a page");
    }
    if (!(*pte & PTE_COW)) {
        panic("pgfault: invalid permission");
    }

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
      
	// LAB 4: Your code here.
    if ((r = sys_page_alloc(0, (void *)PFTEMP, PTE_U | PTE_P | PTE_W))) {
        panic("pgfault: %e", r);
    }
    void *fp_va = (void *)((uintptr_t)addr & ~0xfff);
    memcpy((void *)PFTEMP, fp_va, PGSIZE);
    // the permission to be restored
    int perm = ((*pte & 0xf0f) & ~PTE_COW) | PTE_W;
    if ((r = sys_page_map(0, (void *)PFTEMP, 0, fp_va, perm))) {
        panic("pgfault: %e", r);
    }
    if ((r = sys_page_unmap(0, (void *)PFTEMP))) {
        panic("pgfault: %e", r);
    }
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
    void *va = (void *)(pn * PGSIZE);
    volatile pte_t *pte = &uvpt[pn];
    int perm = *pte & 0xf0f;
    int remap = 0;
    if ((perm & PTE_W) || (perm & PTE_COW)) {
        // mark it as not writable.
        perm &= ~PTE_W;
        perm |= PTE_COW;
        remap = 1;
    }
    if ((r = sys_page_map(0, va, envid, va, perm))) {
        return r;
    }
    if (remap && (r = sys_page_map(0, va, 0, va, perm))) {
        return r;
    }
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
    int err;
    envid_t envid;
    set_pgfault_handler(pgfault);
    if ((envid = sys_exofork()) < 0) {
        return envid;
    }
      
    // Child will start from here if spawned successfully 
      
    // Child
    if (envid == 0) {
        // we know that environment for child has already been set up by the parent.
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
    }
              
    // Parent
    // map from .text to the end of .bss in user address space.
	extern unsigned char end[];
	for (uintptr_t va = UTEXT; va < (uintptr_t)end; va += PGSIZE) {
		if ((err = duppage(envid, PGNUM(va)))) {
            return err;
        }
    }
    if ((err = duppage(envid, PGNUM(USTACKTOP - PGSIZE)))) {
        return err;
    }
    if ((err = sys_page_alloc(envid, (void *)UXSTACKTOP - PGSIZE, 
        PTE_U | PTE_P | PTE_W))) {
        return err;
    }
    extern void _pgfault_upcall(void);
    if ((err = sys_env_set_pgfault_upcall(envid, _pgfault_upcall))) {
        return err;
    }
    // mark the child as runnable
    // if the environment is not set up appropriately, 
    // then child might be spawned as well, but it can fault easily.
    if ((err = sys_env_set_status(envid, ENV_RUNNABLE))) {
        return err;
    }
    return envid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}

// implement fork from user space

#include "inc/memlayout.h"
#include "inc/mmu.h"
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
    unsigned pdx = PDX(addr);
    volatile pde_t *pde = &uvpd[pdx];
    if (!(*pde & PTE_P)) {
        panic("pgfault: no such a page");
    }

    unsigned pn = PGNUM(addr);
    volatile pte_t *pte = &uvpt[pn];
    if (!(*pte & PTE_P)) {
        panic("pgfault: no such a page");
    }

    if (!(*pte & PTE_COW)) {
        panic("pgfault: not copy-on-write page");
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

    void *fp_va = (void *)ROUNDDOWN(addr, PGSIZE);
    memmove((void *)PFTEMP, fp_va, PGSIZE);
    // the permission to be restored
    int perm = ((*pte & PTE_SYSCALL) & ~PTE_COW) | PTE_W;
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
    //
    // The caller should make sure that the correpsonding page table of page pn
    // has been allocated (i.e., (uvpd[pn / NPTENTRIES] & PTE_P) != 0),
    // or dereferencing &uvpt[pn] will trigger a page fault.
    //
    volatile pte_t *pte = &uvpt[pn];
    int perm = *pte & PTE_SYSCALL;
    bool remap = 0;
    // return silently if page pn is not mapped
    if (!(*pte & PTE_P)) {
        return 0;
    }
    // If page pn is a share page, do not mark it as Copy-On-Write -
    // map it directly instead.
    if (!(perm & PTE_SHARE) && ((perm & PTE_W) || (perm & PTE_COW))) {
        // mark pn as not writable.
        perm &= ~PTE_W;
        perm |= PTE_COW;
        remap = 1;
    }
    void *va = (void *)(pn * PGSIZE);
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
      
    //
    // Child will start from here if spawned successfully 
    //
    if (envid == 0) {
        // We know that environment for child has already been set up by the parent.
        // Reset thisenv. Note: curenv is kernel-private.
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
    }

    //
    // Parent only
    // 
    for (size_t pdx = 0; pdx < PDX(UTOP); pdx++) {
        volatile pde_t *pde = &uvpd[pdx];
        if (!(*pde & PTE_P)) {
            continue;
        }
        for (size_t ptx = 0; ptx < NPTENTRIES; ptx++) {
            size_t pn = pdx * NPTENTRIES + ptx;
            // Do not map child's user exception stack.
            // We will allocate a new page for it instead.
            if (pn == PGNUM(UXSTACKTOP - PGSIZE)) {
                continue;
            }
            if ((err = duppage(envid, pn))) {
                return err;
            }
        }
    }

    if ((err = sys_page_alloc(envid, (void *)UXSTACKTOP - PGSIZE, 
       PTE_U | PTE_P | PTE_W))) {
       return err;
    }

    // Set the parent's pgfault upcall to the child 
    // (modify child's env_pgfault_upcall field in kernel).
    // It is ok to do so since they share one program data space.
    extern void _pgfault_upcall(void);
    if ((err = sys_env_set_pgfault_upcall(envid, _pgfault_upcall))) {
        return err;
    }

    // Mark the child as runnable
    // if the environment is not set up appropriately, 
    // then child might be spawned as well, but it can fault easily.
    if ((err = sys_env_set_status(envid, ENV_RUNNABLE))) {
        return err;
    }
    return envid;
}

static int
sduppage(envid_t envid, unsigned pn)
{
	int r;

    volatile pte_t *pte = &uvpt[pn];
    int perm = *pte & PTE_SYSCALL;
    bool remap = 0;
    // return silently if page pn is not mapped
    if (!(*pte & PTE_P)) {
        return 0;
    }
    // mark stack page as Copy-On-Write
    if (pn == PGNUM(USTACKTOP - PGSIZE)) {
        perm &= ~PTE_W;
        perm |= PTE_COW;
        remap = 1;
    }
    void *va = (void *)(pn * PGSIZE);
    if ((r = sys_page_map(0, va, envid, va, perm))) {
        return r;
    }
    if (remap && (r = sys_page_map(0, va, 0, va, perm))) {
        return r;
    }
	return 0;
}

// Challenge!
int
sfork(void)
{
    int err;
    envid_t envid;
    set_pgfault_handler(pgfault);
    if ((envid = sys_exofork()) < 0) {
        return envid;
    }
      
    if (envid == 0) {
        // 
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
    }

    for (size_t pdx = 0; pdx < PDX(UTOP); pdx++) {
        volatile pde_t *pde = &uvpd[pdx];
        if (!(*pde & PTE_P)) {
            continue;
        }
        for (size_t ptx = 0; ptx < NPTENTRIES; ptx++) {
            size_t pn = pdx * NPTENTRIES + ptx;
            if ((err = duppage(envid, pn))) {
                return err;
            }
        }
    }

    // Set the parent's pgfault upcall to the child 
    // (modify child's env_pgfault_upcall field in kernel).
    // It is ok to do so since they share one program data space.
    extern void _pgfault_upcall(void);
    if ((err = sys_env_set_pgfault_upcall(envid, _pgfault_upcall))) {
        return err;
    }

    // Mark the child as runnable
    // if the environment is not set up appropriately, 
    // then child might be spawned as well, but it can fault easily.
    if ((err = sys_env_set_status(envid, ENV_RUNNABLE))) {
        return err;
    }
    return envid;
}

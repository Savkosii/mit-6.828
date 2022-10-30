// User-level page fault handler support.
// Rather than register the C page fault handler directly with the
// kernel as the page fault handler, we register the assembly language
// wrapper in pfentry.S, which in turns calls the registered C
// function.

#include <inc/lib.h>


// Assembly language pgfault entrypoint defined in lib/pfentry.S.
extern void _pgfault_upcall(void);

// Pointer to currently installed C-language pgfault handler.
void (*_pgfault_handler)(struct UTrapframe *utf);

//
// Set the page fault handler function.
// If there isn't one yet, _pgfault_handler will be 0.
// The first time we register a handler, we need to
// allocate an exception stack (one page of memory with its top
// at UXSTACKTOP), and tell the kernel to call the assembly-language
// _pgfault_upcall routine when a page fault occurs.
//
void
set_pgfault_handler(void (*handler)(struct UTrapframe *utf))
{
	int err;

	if (_pgfault_handler == 0) {
		// First time through!
		// LAB 4: Your code here.
        // Allocate the exception stack and register _pgfault_upcall 
        // to curenv->env_pgfault_upcall the first time the user set a handler.
        // The upcall is called by page_fault_handler() at trap_dispatch() in kern/trap.c, 
        // responsible for calling user's _pgfault_handler and returning to 
        // the trap-time frame after it finishes, see lib/pfentry.S.
        if ((err = sys_page_alloc(0, (void *)UXSTACKTOP - PGSIZE, PTE_W | PTE_P | PTE_U))) {
            cprintf("set_pgfault_handler: sys_page_alloc: %e\n", err);
            return;
        }
        if ((err = sys_env_set_pgfault_upcall(0, _pgfault_upcall))) {
            cprintf("set_pgfault_handler: sys_env_set_pgfault_upcall: %e\n", err);
            return;
        }
	}

	// Save handler pointer for assembly to call.
	_pgfault_handler = handler;
}

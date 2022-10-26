/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>



###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);  /* trapno */				        \
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.text


/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */
    // We add the declaration of these handler functions in kern/trap.h.
    // IDT stores the address of these functions.
    TRAPHANDLER_NOEC(HANDLER_DIVIDE, 0)
    TRAPHANDLER(HANDLER_DEBUG, 1)
    TRAPHANDLER(HANDLER_NMI, 2)
    TRAPHANDLER(HANDLER_BRKPT, 3)
    TRAPHANDLER(HANDLER_OFLOW, 4)
    TRAPHANDLER(HANDLER_BOUND, 5)
    TRAPHANDLER(HANDLER_ILLOP, 6)
    TRAPHANDLER(HANDLER_DEVICE, 7)
    TRAPHANDLER(HANDLER_DBLFLT, 8)
    TRAPHANDLER(HANDLER_COPROC, 9)
    TRAPHANDLER(HANDLER_TSS, 10)
    TRAPHANDLER(HANDLER_SEGNP, 11)
    TRAPHANDLER(HANDLER_STACK, 12)
    TRAPHANDLER(HANDLER_GPFLT, 13)
    TRAPHANDLER(HANDLER_PGFLT, 14)
    TRAPHANDLER(HANDLER_RES, 15)
    TRAPHANDLER(HANDLER_FPERR, 16)
    TRAPHANDLER(HANDLER_ALIGN, 17)
    TRAPHANDLER(HANDLER_MCHK, 18)
    TRAPHANDLER(HANDLER_SIMDERR, 19)

/*
 * Lab 3: Your code here for _alltraps
 */
_alltraps:
    // set up struct Trapframe tf
    pushl %ds;
    pushl %es;
    pushal;  // eax, ecx, edx, ebx, esp, ebp, esi, and edi
    
    movw $GD_KD, %ax; // Kernel Data Global Descriptor Number
    movw %ax, %ds;
    movw %ax, %es;

    pushl %esp; // esp points to tf
    call trap
    

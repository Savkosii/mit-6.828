/* See COPYRIGHT for copyright information. */

#ifndef JOS_KERN_TRAP_H
#define JOS_KERN_TRAP_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

#include <inc/trap.h>
#include <inc/mmu.h>

/* The kernel's interrupt descriptor table */
extern struct Gatedesc idt[];
extern struct Pseudodesc idt_pd;

void trap_init(void);
void trap_init_percpu(void);
void print_regs(struct PushRegs *regs);
void print_trapframe(struct Trapframe *tf);
void page_fault_handler(struct Trapframe *);
void backtrace(struct Trapframe *);

void HANDLER_DIVIDE(void);
void HANDLER_DEBUG(void);
void HANDLER_NMI(void);
void HANDLER_BRKPT(void);
void HANDLER_OFLOW(void);
void HANDLER_BOUND(void);
void HANDLER_ILLOP(void);
void HANDLER_DEVICE(void);
void HANDLER_DBLFLT(void);
void HANDLER_COPROC(void);
void HANDLER_TSS(void);
void HANDLER_SEGNP(void);
void HANDLER_STACK(void);
void HANDLER_GPFLT(void);
void HANDLER_PGFLT(void);
void HANDLER_RES(void);
void HANDLER_FPERR(void);
void HANDLER_ALIGN(void);
void HANDLER_MCHK(void);
void HANDLER_SIMDERR(void);

#endif /* JOS_KERN_TRAP_H */

/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


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
#define TRAPHANDLER(name, num)												\
	.text;																	\
	.globl name;					/* define global symbol for 'name' */	\
	.type name, @function;			/* symbol type is function */			\
	.align 2;						/* align function definition */			\
	name:							/* function starts here */				\
	pushl $(num);															\
	jmp _alltraps;															\
	.data;																	\
	.long name


/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.text;											\
	.globl name;									\
	.type name, @function;							\
	.align 2;										\
	name:											\
	pushl $0;										\
	pushl $(num);									\
	jmp _alltraps;									\
	.data;											\
	.long name


.data
.global trap_handlers
trap_handlers:
	TRAPHANDLER_NOEC(trap_vector0, T_DIVIDE)
	TRAPHANDLER_NOEC(trap_vector1, T_DEBUG)
	TRAPHANDLER_NOEC(trap_vector2, T_NMI)
	TRAPHANDLER_NOEC(trap_vector3, T_BRKPT)
	TRAPHANDLER_NOEC(trap_vector4, T_OFLOW)
	TRAPHANDLER_NOEC(trap_vector5, T_BOUND)
	TRAPHANDLER_NOEC(trap_vector6, T_ILLOP)
	TRAPHANDLER_NOEC(trap_vector7, T_DEVICE)
	TRAPHANDLER(trap_vector8, T_DBLFLT)
	TRAPHANDLER_NOEC(trap_vector9, 9)
	TRAPHANDLER(trap_vector10, T_TSS)
	TRAPHANDLER(trap_vector11, T_SEGNP)
	TRAPHANDLER(trap_vector12, T_STACK)
	TRAPHANDLER(trap_vector13, T_GPFLT)	
	TRAPHANDLER(trap_vector14, T_PGFLT)
	TRAPHANDLER_NOEC(trap_vector15, 15)
	TRAPHANDLER_NOEC(trap_vector16, T_FPERR)
	TRAPHANDLER_NOEC(trap_vector17, T_ALIGN)
	TRAPHANDLER_NOEC(trap_vector18, T_MCHK)
	TRAPHANDLER_NOEC(trap_vector19, T_SIMDERR)
	TRAPHANDLER_NOEC(trap_vector20, 20)
	TRAPHANDLER_NOEC(trap_vector21, 21)
	TRAPHANDLER_NOEC(trap_vector22, 22)
	TRAPHANDLER_NOEC(trap_vector23, 23)
	TRAPHANDLER_NOEC(trap_vector24, 24)
	TRAPHANDLER_NOEC(trap_vector25, 25)
	TRAPHANDLER_NOEC(trap_vector26, 26)
	TRAPHANDLER_NOEC(trap_vector27, 27)
	TRAPHANDLER_NOEC(trap_vector28, 28)
	TRAPHANDLER_NOEC(trap_vector29, 29)
	TRAPHANDLER_NOEC(trap_vector30, 30)
	TRAPHANDLER_NOEC(trap_vector31, 31)
	TRAPHANDLER_NOEC(trap_vector32, 32)
	TRAPHANDLER_NOEC(trap_vector33, 33)
	TRAPHANDLER_NOEC(trap_vector34, 34)
	TRAPHANDLER_NOEC(trap_vector35, 35)
	TRAPHANDLER_NOEC(trap_vector36, 36)
	TRAPHANDLER_NOEC(trap_vector37, 37)
	TRAPHANDLER_NOEC(trap_vector38, 38)
	TRAPHANDLER_NOEC(trap_vector39, 39)
	TRAPHANDLER_NOEC(trap_vector40, 40)
	TRAPHANDLER_NOEC(trap_vector41, 41)
	TRAPHANDLER_NOEC(trap_vector42, 42)
	TRAPHANDLER_NOEC(trap_vector43, 43)
	TRAPHANDLER_NOEC(trap_vector44, 44)
	TRAPHANDLER_NOEC(trap_vector45, 45)
	TRAPHANDLER_NOEC(trap_vector46, 46)
	TRAPHANDLER_NOEC(trap_vector47, 47)
	TRAPHANDLER_NOEC(trap_vector48, T_SYSCALL)



_alltraps:
	pushl %ds
	pushl %es
	pushal
	movw $GD_KD, %ax
	movw %ax, %ds
	movw %ax, %es
	pushl %esp
	call trap


// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>
#include <kern/pmap.h>


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
    { "showmappings", "Display the information about memory mappings",mon_showmappings},
    { "dumpcontent", "Display the content of given range",mon_dumpcontent},
    { "changeperms", "change permissions ",mon_changeperms},
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
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

    //lab1 - start
int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{

    uint32_t* ebp = (uint32_t*)read_ebp();
    cprintf("Stack backtrace:\n");
    while(ebp != 0){
        cprintf("  ebp %08x  eip %08x  args ", ebp,*(ebp+1) /*eip*/);
        cprintf("%08x %08x %08x %08x %08x\n",*(ebp+2),*(ebp+3),*(ebp+4),*(ebp+5),*(ebp+6));
        
        struct Eipdebuginfo info; //declared in kdebug.c
        int res = debuginfo_eip(*(ebp+1),&info);
        if (res == 0){
        cprintf("\t   %s:%d: %.*s+%d\n",info.eip_file,info.eip_line,info.eip_fn_namelen,info.eip_fn_name,*(ebp+1)- info.eip_fn_addr);
        }
        ebp = (uint32_t*) (*ebp);     
    }

	return 0;
}
    //lab1 - finish

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
	for (i = 0; i < NCOMMANDS; i++) {
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


int mon_showmappings(int argc, char **argv, struct Trapframe *tf){
    uint32_t i = 0;
    pte_t* ppage;
    
    if(argc != 3 && argc != 2){
        cprintf("invalid num of params\n");
        return -1;
    }
    
    uint32_t start = ROUNDDOWN(strtol(argv[1],NULL,16), PGSIZE);
    uint32_t end = (argc == 3) ? ROUNDDOWN(strtol(argv[2],NULL,16),PGSIZE) : start;
    
    if(end < start){
        cprintf("invalid range of params\n");
        return -1;
    }
    
    for(i = start; i < end + PGSIZE; i += PGSIZE){
       ppage = pgdir_walk(kern_pgdir,(const void*)i,0);
       
       if((!ppage) || ((*ppage & PTE_P) == 0)){
            cprintf("Virtual address %08x is not mapped\n",i);
            continue;
       }
       uint32_t present_bit = (*ppage & PTE_P) ? 1 : 0;
       uint32_t read_write_bit = (*ppage & PTE_W) ? 1 : 0;
       uint32_t user_bit = (*ppage & PTE_U) ? 1 : 0;
       
       cprintf("Virtual address %08x is mapped to phyical address %08x\n",i,PTE_ADDR(*ppage));
       cprintf("\tpresent bit is: %d\n",present_bit);
       cprintf("\tread write bit is: %d\n",read_write_bit);
       cprintf("\tuser  bit is: %d\n\n",user_bit);
            
    }
    
    return 0;
}

int mon_dumpcontent(int argc, char **argv, struct Trapframe *tf){
    uintptr_t  i = 0;

    if(argc != 3 && argc != 4){
        cprintf("invalid num of params\n");
        return -1;
    }
    
    if(strcmp(argv[1],"-p") && strcmp(argv[1],"-v")){
        cprintf("unknown address type\n");
        return -1;  
    }
    
    
    uintptr_t start = strtol(argv[2],NULL,16);
    uintptr_t  end = (argc == 4) ? strtol(argv[3],NULL,16) : start;
    
    
    if(start > end){
        cprintf("invalid range of params\n");
        return -1;
    }
    
        if(!strcmp(argv[1],"-v") && start < KERNBASE){
            cprintf("lab 2 - virtual addresses bellow KERNBASE hasnt been mapped yet\n");
            return -1;
        }
    
    if(!strcmp(argv[1],"-p")){
        start = (uintptr_t)KADDR(start);
        end = (uintptr_t)KADDR(end);
    }

    for(i = start; i <= end; i++){ 
    
        cprintf("%08x : %x\n",i,(*(uintptr_t*)(i)));
        }
   return 0;
}

//first arg is the action(set/clear/change)
//second arg is the bit to action on
//addresses is from arg 3 until the last
int mon_changeperms(int argc, char **argv, struct Trapframe *tf){
    
    if( argc < 4){
        cprintf("invalid num of params\n");
        return -1;
    }
    
    if((strcmp(argv[1],"-set")) && (strcmp(argv[1],"-clear")) && (strcmp(argv[1],"-change"))){
        
        cprintf("invalid command\n");
        return -1;
    }
    
    if((strcmp(argv[2],"p")) && (strcmp(argv[2],"w")) && (strcmp(argv[2],"u"))){
         
        cprintf("invalid command\n");
        return -1;
    }
    
    uint32_t perm = 0;
    if (!strcmp(argv[2],"p")) perm = PTE_P;
    if (!strcmp(argv[2],"w")) perm = PTE_W;
    if (!strcmp(argv[2],"u")) perm = PTE_U;
    
    uint32_t i = 0;
    pte_t* ppage;
    
    for(i = 3; i < argc; i++){
        
        ppage = pgdir_walk(kern_pgdir,(const void*)(argv[i]),0);
      
        if(!ppage){
            cprintf("address &08x is illegal!\n",strtol(argv[i],NULL,16)); 
            return -1;
      }
      
        uint32_t present_bit = (*ppage & PTE_P) ? 1 : 0;
        uint32_t read_write_bit = (*ppage & PTE_W) ? 1 : 0;
        uint32_t user_bit = (*ppage & PTE_U) ? 1 : 0;
      
        cprintf("before\n");
        cprintf("address is %08x  p_bit is: %d  w_bit is: %d  u_bit is: %d\n", strtol(argv[i],NULL,16),present_bit,read_write_bit,user_bit);
      
      
        if(!strcmp(argv[1],"-set")){
          *ppage |= perm;
      }
      
        else if(!strcmp(argv[1],"-clear")){
        *ppage &= ~(perm);
      }
      
        else{
            *ppage ^= perm;
        }
        
            present_bit = (*ppage & PTE_P) ? 1 : 0;
         read_write_bit = (*ppage & PTE_W) ? 1 : 0;
         user_bit = (*ppage & PTE_U) ? 1 : 0;
        cprintf("after\n");
        cprintf("address is %08x  p_bit is: %d  w_bit is: %d  u_bit is: %d\n", strtol(argv[i],NULL,16),present_bit,read_write_bit,user_bit);

    }
    return 0;   
}

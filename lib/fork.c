// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

extern void _pgfault_upcall();
//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	if(((err & FEC_WR) == 0) || ((uvpt[PGNUM(addr)] & PTE_COW) == 0))
        panic("pgfault - not cow page\n");

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

    addr = ROUNDDOWN(addr, PGSIZE);
	if(sys_page_alloc(0, PFTEMP, PTE_W|PTE_P|PTE_U) < 0) panic("page allocation problem\n");
	memcpy(PFTEMP, addr, PGSIZE);
	if(sys_page_map(0, PFTEMP, 0, addr, PTE_W|PTE_P|PTE_U) < 0) panic("page mapping problem\n");
	if(sys_page_unmap(0, PFTEMP) < 0) panic("should never happen\n");


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
    void* addr = (void*)(pn * PGSIZE);
    
    //if the page is write or cow
    if((uvpt[pn] & PTE_W) || (uvpt[pn] * PTE_COW)) {
    if(sys_page_map(0, addr, envid, addr, PTE_P | PTE_U | PTE_COW) <  0) panic("page mapping problem_1\n");
    if(sys_page_map(0, addr, 0, addr, PTE_P | PTE_U | PTE_COW) < 0) panic("page mapping problem_2\n");
    return 0;
    }
    
    if(sys_page_map(0, addr, envid, addr, PTE_P | PTE_U) < 0) panic("page mapping problem_3\n"); 
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
envid_t fork(void)
{
    set_pgfault_handler(pgfault);
    envid_t new_id = sys_exofork();
    uintptr_t addr;

    if(new_id < 0) panic("fork -> creating a child failed\n");
    
    if(new_id == 0) {
		set_pgfault_handler(pgfault);
        thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}
    
    for(addr = 0; addr < USTACKTOP; addr += PGSIZE){
        if((uvpd[PDX(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_U))
            duppage(new_id, PGNUM(addr));
    }

        if(sys_page_alloc(new_id, (void *) (UXSTACKTOP - PGSIZE), PTE_U|PTE_P|PTE_W) < 0)
            panic("fork - could not allocate exception stack");

        if(sys_env_set_pgfault_upcall(new_id, _pgfault_upcall) < 0)
            panic("fork - could not set pgfault entry point");

        if(sys_env_set_status(new_id, ENV_RUNNABLE) < 0)
            panic("fork - could not change env status");

	return new_id;
    
}

// Challenge!

static int sduppage(envid_t envid, unsigned pn)
{
	int r;
	void * addr = (void *) (pn * PGSIZE);
	if(sys_page_map(0, addr, envid, addr, uvpt[pn] & PTE_SYSCALL) < 0)
		panic("could not share page");
	return 0;
}

int sfork(void)
{
    uint32_t addr;
	set_pgfault_handler(pgfault);

	envid_t envid = sys_exofork();
	if(envid < 0) 
		panic("exofork failed");

	if(envid == 0) {
		set_pgfault_handler(pgfault);
		return 0;
	}

	for(addr = USTACKTOP - PGSIZE; addr >= UTEXT; addr -= PGSIZE)
			if((uvpd[PDX(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_U))
				duppage(envid, PGNUM(addr));
			else break;
	for(addr; addr > 0; addr -= PGSIZE)
		if((uvpd[PDX(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_U))
				sduppage(envid, PGNUM(addr));

	if(sys_page_alloc(envid, (void *) (UXSTACKTOP - PGSIZE), PTE_U|PTE_P|PTE_W) < 0)
		panic("could not allocate exception stack");

	extern void _pgfault_upcall();
	if(sys_env_set_pgfault_upcall(envid, _pgfault_upcall) < 0)
		panic("could not set pgfault entry point");

	if(sys_env_set_status(envid, ENV_RUNNABLE) < 0)
		panic("could not change env status");

	return envid;
}

#include "ns.h"
#include "inc/lib.h"

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver
        int val;
        envid_t from_env = 1;
        struct jif_pkt* packet = (struct jif_pkt *)REQVA;

        if(sys_page_alloc(0, packet, PTE_U|PTE_W|PTE_P) < 0) 
            panic("output.c: could not allocate a page to the packet");

        while (1)
        {
            val = ipc_recv(&from_env, packet, NULL);
            if (from_env != ns_envid){
                cprintf("Bad recv envid in output\n");
                continue;
            }
            if (val != NSREQ_OUTPUT){
                cprintf("Non-NSREQ_OUTPUT request sent to output\n");
                continue;
            }
            sys_send_packet((void *) packet->jp_data, packet->jp_len);
            sys_page_unmap(0, packet);
        }
}
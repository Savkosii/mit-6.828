#include <net/ns.h>
#include <inc/memlayout.h>

// see lib/nsipc.c
extern union Nsipc nsipcbuf;

// Note: ns_envid is an argument! The actual network server env is not run yet
// See net/testinput.c and net/testoutput.c
void
input(envid_t ns_envid)
{
	binaryname = "ns_input";
    sys_page_alloc(0, (void *)UTEMP, PTE_P | PTE_W | PTE_U);

	// LAB 6: Your code here:
	// 	- read a packet from the device driver
	//	- send it to the network server (identified by arg ns_envid)
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.
    while (1) {
        // it is weird that !(nsipcbuf & PTE_W)
        struct jif_pkt *packet_buf = &nsipcbuf.pkt;
        ssize_t n;
        while ((n = sys_recv_packet((void *)UTEMP, PGSIZE)) < 0)  {
            sys_yield();
        }
        memcpy(packet_buf->jp_data, (void *)UTEMP, n);
        packet_buf->jp_len = n;
        ipc_send(ns_envid, NSREQ_INPUT, &nsipcbuf, PTE_P|PTE_W|PTE_U);
        for (int i = 0; i < 5; i++) {
            sys_yield();
        }
    }
}

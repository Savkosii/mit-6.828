#include <net/ns.h>

extern union Nsipc nsipcbuf;

// see inc/ns.h and net/testoutput.c
void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet from the network server (identified by arg ns_envid)
	//	- send the packet to the device driver
    while (1) {
        ipc_recv(NULL, &nsipcbuf, NULL);
        struct jif_pkt *packet = &nsipcbuf.pkt;
        sys_transmit_packet(packet->jp_data, packet->jp_len);
    }
}

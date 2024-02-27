#include <inc/string.h>
#include <inc/error.h>
#include <kern/e1000.h>
#include <kern/pmap.h>
#include <kern/env.h>

volatile uint32_t *e1000_bar0;

/* DMA descriptor for the transmit buffer */
struct tx_desc
{
	uint64_t addr;   /* Address of the descriptor's data buffer */
	uint16_t length; /* Data buffer length */
	uint8_t cso;    /* Checksum offset */
	uint8_t cmd;    /* Descriptor control */
	uint8_t status;
	uint8_t css;    /* Checksum start */
	uint16_t special;
};

/* Receive Descriptor */
struct rx_desc {
    uint64_t addr; /* Address of the descriptor's data buffer */
    uint16_t length;     /* Length of data DMAed into data buffer */
    uint16_t csum;       /* Packet checksum */
    uint8_t status;      /* Descriptor status */
    uint8_t errors;      /* Descriptor Errors */
    uint16_t special;
};

#define MAX_TX_DESC_NUM 64
#define MAX_RX_DESC_NUM 128
#define MAX_PACKET_LEN 1518

struct tx_desc *tx_desc_array;
void *tx_buffers[MAX_TX_DESC_NUM];

struct rx_desc *rx_desc_array;
void *rx_buffers[MAX_RX_DESC_NUM];

// LAB 6: Your driver code here
int e1000_attach(struct pci_func *pcif) {
    // alloc physical memory for the device
    pci_func_enable(pcif);
    cprintf("%p %d\n", pcif->reg_base[0], pcif->reg_size[0]);
    // make the physical memory of bar 0 (where registers are mapped) accessible by the kernel
    e1000_bar0 = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);
    uint32_t status_reg = e1000_bar0[E1000_STATUS];
    assert(status_reg == 0x80080783);

    /* setup transmit queue ring (section 3.4 && 14.5) */

    // alloc memory tx desc array
    struct PageInfo *pp = page_alloc(ALLOC_ZERO);
    if (pp == NULL) {
        return -E_NO_MEM;
    }
    tx_desc_array = page2kva(pp);

    // alloc memory pointed to by each tx desc
    for (size_t tx = 0; tx < MAX_TX_DESC_NUM; tx++) {
        if (tx % 2 == 0) {
            struct PageInfo *pp = page_alloc(ALLOC_ZERO);
            if (pp == NULL) {
                return -E_NO_MEM;
            }
            tx_buffers[tx] = page2kva(pp);
        } else {
            tx_buffers[tx] = tx_buffers[tx - 1] + MAX_PACKET_LEN;
        }
        tx_desc_array[tx].addr = PADDR(tx_buffers[tx]); // the driver access physical address!
        // Report Status (RS): set Descriptor done bit (DD) of status field 
        // if the descriptor is ready to be recyle
        tx_desc_array[tx].cmd |= E1000_TXD_CMD_RS;
        tx_desc_array[tx].status |= E1000_TXD_STAT_DD;
    }

    // configure the register of device
    e1000_bar0[E1000_TDBAL] = PADDR(tx_desc_array);
    e1000_bar0[E1000_TDLEN] = MAX_TX_DESC_NUM * sizeof(struct tx_desc);
    assert(e1000_bar0[E1000_TDLEN] % 128 == 0);

    e1000_bar0[E1000_TDH] = e1000_bar0[E1000_TDT] = 0;

    e1000_bar0[E1000_TCTL] |= E1000_TCTL_EN;
    e1000_bar0[E1000_TCTL] |= E1000_TCTL_PSP;
    unsigned char ct = 0x10;
    unsigned char cold = 0x40;
    e1000_bar0[E1000_TCTL] = (e1000_bar0[E1000_TCTL] & ~E1000_TCTL_CT) | (ct << 4);
    e1000_bar0[E1000_TCTL] = (e1000_bar0[E1000_TCTL] & ~E1000_TCTL_COLD) | (cold << 12);

    e1000_bar0[E1000_TIPG] = (10) | (8 << 10) | (6 << 20);


    /* setup receive queue ring (section 3.2.6 && 14.4) */

    // alloc memory rx desc array
    pp = page_alloc(ALLOC_ZERO);
    if (pp == NULL) {
        return -E_NO_MEM;
    }
    rx_desc_array = page2kva(pp);


    // alloc memory (2048 in bytes) pointed to by each rx desc
    for (size_t rx = 0; rx < MAX_RX_DESC_NUM; rx++) {
        if (rx % 2 == 0) {
            struct PageInfo *pp = page_alloc(ALLOC_ZERO);
            if (pp == NULL) {
                return -E_NO_MEM;
            }
            rx_buffers[rx] = page2kva(pp);
        } else {
            rx_buffers[rx] = rx_buffers[rx - 1] + 2048;
        }
        rx_desc_array[rx].addr = PADDR(rx_buffers[rx]); // the driver access physical address!
    }

    // configure the register of device
    e1000_bar0[E1000_RDBAL] = PADDR(rx_desc_array);
    e1000_bar0[E1000_RDLEN] = MAX_RX_DESC_NUM * sizeof(struct rx_desc);
    assert(e1000_bar0[E1000_RDLEN] % 128 == 0);

    e1000_bar0[E1000_RDH] = 0;
    /* tail points to the location where software writes the first new descriptor */
    e1000_bar0[E1000_RDT] = MAX_RX_DESC_NUM - 1;  // TODO:

    e1000_bar0[E1000_RCTL] |= E1000_RCTL_EN;
    e1000_bar0[E1000_RCTL] |= E1000_RCTL_SECRC;
    e1000_bar0[E1000_RCTL] &= ~E1000_RCTL_BSEX;
    e1000_bar0[E1000_RCTL] |= E1000_RCTL_SZ_2048;

    // MAC address of 52:54:00:12:34:56 (from the lowest to the highest)
    e1000_bar0[E1000_RAL] = 0x12005452;
    e1000_bar0[E1000_RAH] = 0x5634 | E1000_RAH_AV;

    return 0;
}


// Transmit packet to the buffer of driver 
// TODO: split packets
// TODO: block when queue is full
//
// called by the Output Helper Env at net/input.c
int transmit_packet(void *va, size_t n) {
    if (n > MAX_PACKET_LEN) {
        return -E_INVAL;
    }
    uint32_t tx = e1000_bar0[E1000_TDT];
    // ok to recyle the descriptor ?
    if (!(tx_desc_array[tx].status & E1000_TXD_STAT_DD)) {
        cprintf("Error: Transmit Buffer Full !\n");
        return -1;
    }
    tx_desc_array[tx].status &= ~E1000_TXD_STAT_DD;
    memcpy(tx_buffers[tx], va, n);
    tx_desc_array[tx].length = n;
    tx_desc_array[tx].cmd |= E1000_TXD_CMD_EOP; // end of packet
    e1000_bar0[E1000_TDT] = (tx + 1) % MAX_TX_DESC_NUM;

    return 0;
}


// Fetch packets from the buffer of the driver
//
// Initially, RDH points to 0, and RDT points to N-1
//
// Whenever the driver receives and processes a new packet (which can be transmitted 
// by transmit_packet()), it move the packet data to the buffer pointed to by descriptor
// at [RDH] and increment RDH by 1 via hardware
//
// To fetch a packet from the buffer, fetch [RDT + 1] and increment RDT by 1 via software
//
// If RDH == RDT, the buffer is full
// 
// called by the Input Helper Env at net/input.c
ssize_t recv_packet(void *va, size_t max_n) {
    uint32_t rx = (e1000_bar0[E1000_RDT] + 1) % MAX_RX_DESC_NUM; 
    if (!(rx_desc_array[rx].status & E1000_TXD_STAT_DD)) {
        // cprintf("Error: Receive Buffer Empty !\n");
        return -1;
    }
    size_t n = MIN(max_n, rx_desc_array[rx].length);
    memcpy(va, rx_buffers[rx], n);
    rx_desc_array[rx].status = 0;
    e1000_bar0[E1000_RDT] = rx;
    return n;
}

#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

// https://pdos.csail.mit.edu/6.828/2018/labs/lab6/e1000_hw.h and section 13.4
 
#define E1000_CTRL     (0x00000)  /* Device Control - RW */
#define E1000_CTRL_DUP (0x00004 / 4) /* Device Control Duplicate (Shadow) - RW */
#define E1000_STATUS   (0x00008 / 4) /* Device Status - RO */

#define E1000_TCTL     (0x00400 / 4) /* TX Control - RW */
#define E1000_TDBAL    (0x03800 / 4)  /* TX Descriptor Base Address Low - RW */
#define E1000_TDBAH    (0x03804 / 4)  /* TX Descriptor Base Address High - RW */
#define E1000_TDLEN    (0x03808 / 4)  /* TX Descriptor Length - RW */
#define E1000_TDH      (0x03810 / 4)  /* TX Descriptor Head - RW */
#define E1000_TDT      (0x03818 / 4)  /* TX Descripotr Tail - RW */
#define E1000_TIPG     (0x00410 / 4)  /* TX Inter-packet gap -RW */


/* Transmit Control */
#define E1000_TCTL_RST    0x00000001    /* software reset */
#define E1000_TCTL_EN     0x00000002    /* enable tx */
#define E1000_TCTL_BCE    0x00000004    /* busy check enable */
#define E1000_TCTL_PSP    0x00000008    /* pad short packets */
#define E1000_TCTL_CT     0x00000ff0    /* collision threshold (mask) */
#define E1000_TCTL_COLD   0x003ff000    /* collision distance (mask) */
#define E1000_TCTL_SWXOFF 0x00400000    /* SW Xoff transmission */
#define E1000_TCTL_PBE    0x00800000    /* Packet Burst Enable */
#define E1000_TCTL_RTLC   0x01000000    /* Re-transmit on late collision */
#define E1000_TCTL_NRTU   0x02000000    /* No Re-transmit on underrun */
#define E1000_TCTL_MULR   0x10000000    /* Multiple request support */

/* status field of transmit descriptor */
#define E1000_TXD_STAT_DD    0x01 /* Descriptor Done */
#define E1000_TXD_STAT_EC    0x02 /* Excess Collisions */
#define E1000_TXD_STAT_LC    0x04 /* Late Collisions */
#define E1000_TXD_STAT_TU    0x08 /* Transmit underrun */

/* cmd field of transmit descriptor */
#define E1000_TXD_CMD_EOP    0x01 /* End of Packet */
#define E1000_TXD_CMD_IFCS   0x02 /* Insert FCS (Ethernet CRC) */
#define E1000_TXD_CMD_IC     0x04 /* Insert Checksum */
#define E1000_TXD_CMD_RS     0x08 /* Report Status */
#define E1000_TXD_CMD_RPS    0x10 /* Report Packet Sent */
#define E1000_TXD_CMD_DEXT   0x20 /* Descriptor extension (0 = legacy) */
#define E1000_TXD_CMD_VLE    0x40 /* Add VLAN tag */
#define E1000_TXD_CMD_IDE    0x80 /* Enable Tidv register */


#define E1000_RCTL     (0x00100 / 4)   /* RX Control - RW */
#define E1000_RDBAL    (0x02800 / 4) /* RX Descriptor Base Address Low - RW */
#define E1000_RDBAH    (0x02804 / 4) /* RX Descriptor Base Address High - RW */
#define E1000_RDLEN    (0x02808 / 4) /* RX Descriptor Length - RW */
#define E1000_RDH      (0x02810 / 4) /* RX Descriptor Head - RW */
#define E1000_RDT      (0x02818 / 4) /* RX Descriptor Tail - RW */
#define E1000_RDTR     (0x02820 / 4) /* RX Delay Timer - RW */

#define E1000_RAL      (0x05400 / 4) /* The lower bits of the 48-bit Ethernet address. 
                                        All 32 bits are valid. */
#define E1000_RAH      (0x05404 / 4) /* The upper bits of the 48-bit Ethernet address */

/* Receive Control */
#define E1000_RCTL_RST            0x00000001    /* Software reset */
#define E1000_RCTL_EN             0x00000002    /* enable */
#define E1000_RCTL_SECRC          0x04000000    /* Strip Ethernet CRC */
#define E1000_RCTL_BSEX           0x02000000    /* Buffer size extension */
/* these buffer sizes are valid if E1000_RCTL_BSEX is 0 */
#define E1000_RCTL_SZ_2048        0x00000000    /* rx buffer size 2048 */
#define E1000_RCTL_SZ_1024        0x00010000    /* rx buffer size 1024 */
#define E1000_RCTL_SZ_512         0x00020000    /* rx buffer size 512 */
#define E1000_RCTL_SZ_256         0x00030000    /* rx buffer size 256 */
/* these buffer sizes are valid if E1000_RCTL_BSEX is 1 */
#define E1000_RCTL_SZ_16384       0x00010000    /* rx buffer size 16384 */
#define E1000_RCTL_SZ_8192        0x00020000    /* rx buffer size 8192 */
#define E1000_RCTL_SZ_4096        0x00030000    /* rx buffer size 4096 */

#define E1000_RAH_AV              0x80000000    /* Receive descriptor valid */

/* Receive Descriptor bit definitions */
#define E1000_RXD_STAT_DD       0x01    /* Descriptor Done */
#define E1000_RXD_STAT_EOP      0x02    /* End of Packet */


#include <kern/pci.h>
int e1000_attach(struct pci_func *pcif);
int transmit_packet(void *va, size_t n);
ssize_t recv_packet(void *va, size_t max_n);

#endif  // SOL >= 6

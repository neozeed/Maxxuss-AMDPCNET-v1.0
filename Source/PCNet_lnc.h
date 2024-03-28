/**
 * Copyright and Terms of Use/Distribution
 * 
 * The source code for "AMD-PCNET-II Network Driver for Darwin/x86 and Mac 
 * OS X/x86" and notes are written and released by Maxxuss.
 * 
 * http://maxxuss.hotbox.ru/pcnet.html
 * 
 * You can use this source code for any purpose as long as you provide this 
 * copyright note file with your source or binary distribution. Please also 
 * keep a reference to it in the header section of each provided source 
 * code file.
 * 
 * Please read the attached Copyright.txt for further information.
 * 
 */

#ifndef _PCNET_FREEBSD_H
#define _PCNET_FREEBSD_H

#include <sys/kernel_types.h>

struct host_ring_entry {
	struct pcnet32_mds *md;
    char *data;
};

typedef struct lnc_softc {
    // ------ IOKit specific
    IOBufferMemoryDescriptor *memDescr; // alloc'd in lnc_pci_attach
    // ------
    int nrdre;
	struct host_ring_entry *recv_ring;  /* start of alloc'd mem */
	int recv_next;
	int ntdre;
	struct host_ring_entry *trans_ring;
	int trans_next;
	struct pcnet32_init_block *init_block;      /* Initialisation block */
	int pending_transmits;        /* No. of transmit descriptors in use */
	int next_to_send;

    u_char ladrb[MULTICAST_FILTER_LEN];  // Preset for Logical Adr. Filter (Multicast)
    bool dwio;
    int flags;
    // ----------- Taken from nic_info structure (referenced as: nic.*)
    int nic_mode;          /* Mode setting at initialization */
    // ----------- Taken from ifp
    unsigned int if_drv_flags;
    unsigned int if_flags;
} lnc_softc_t;

#define LNC_NDESC(len2) (1 << len2)

#define LNC_INC_MD_PTR(ptr, no_entries) \
	if (++ptr >= LNC_NDESC(no_entries)) \
		ptr = 0;

#define LNC_DEC_MD_PTR(ptr, no_entries) \
	if (--ptr < 0) \
		ptr = LNC_NDESC(no_entries) - 1;

#define LNC_RECV_NEXT (sc.recv_ring->base + sc.recv_next)
#define LNC_TRANS_NEXT (sc.trans_ring->base + sc.trans_next)

#define lnc_inb(port) csrRead8( (port) )
#define lnc_inw(port) csrRead16( (port) )
#define lnc_outw(port, val) csrWrite16( (port), (val) )
#define lnc_indw(port) csrRead32( (port) )
#define lnc_outdw(port, val) csrWrite32( (port), (val) )

// From FreeBSD
// for sc.if_drv_flags
// for sc.if_flags
#define LNC_IFF_BROADCAST   0x1           /* (i) broadcast address valid */
#define LNC_IFF_DRV_RUNNING 0x2           /* (d) resources allocated */
#define LNC_IFF_PROMISC     0x4           /* (n) receive all packets */
#define LNC_IFF_ALLMULTI    0x8           /* (n) receive all multicast packets */
#define LNC_IFF_DRV_OACTIVE 0x10          /* (d) tx hardware queue is full */
#define LNC_IFF_SIMPLEX     0x20          /* (i) can't hear own transmissions */
#define LNC_IFF_MULTICAST   0x40          /* (i) supports multicast */

#endif


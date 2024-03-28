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

#include "PCNet.h"

// - Must be called after initPCIConfigSpace, because this is commented out here
bool PCNET::lnc_pci_attach( void )
{
    vm_size_t lnc_mem_size;

    sc.nrdre  = LNC_NRDRE;
    sc.ntdre  = LNC_NTDRE;

    /* Create a DMA tag describing the ring memory we need */

    lnc_mem_size = ((LNC_NDESC(sc.nrdre) + LNC_NDESC(sc.ntdre))
                           * sizeof(struct host_ring_entry));

    lnc_mem_size += sizeof(struct pcnet32_init_block)
                    + (sizeof(struct pcnet32_mds) * (LNC_NDESC(sc.nrdre) + LNC_NDESC(sc.ntdre)))
                    + LNC_MEM_SLEW + 0x20 /* to be sure */;

    lnc_mem_size += (LNC_NDESC(sc.nrdre) * LNC_RECVBUFSIZE) +
                    (LNC_NDESC(sc.ntdre) * LNC_TRANSBUFSIZE);

    sc.memDescr = IOBufferMemoryDescriptor::withOptions(
                  /* options   */ kIOMemoryPhysicallyContiguous,
                  /* capacity  */ lnc_mem_size,
                  /* alignment */ PAGE_SIZE );

    if ( 0 == sc.memDescr || sc.memDescr->prepare() != kIOReturnSuccess ) {
        IOLog("%s: Can't allocate %d contiguous bytes\n",
                getName(), lnc_mem_size);
        return false;
    }
    sc.recv_ring = (struct host_ring_entry *) sc.memDescr->getBytesNoCopy();

    DEBUG_LOG("Rx Buffer len = %d virt = %p phys = 0x%lx\n",
            sc.memDescr->getCapacity(), sc.recv_ring,
            sc.memDescr->getPhysicalSegment(0, 0L));

    /* Call generic attach code */
    if (! lnc_attach_common()) {
        IOLog("Generic attach code failed\n");
        //- lnc_release_resources(dev);
        return false;
    }
    return true;
}


bool PCNET::lnc_attach_common( void )
{
    sc.nic_mode = MODE_NORMAL;   

    /* Fill in arpcom structure entries */
    sc.if_flags = LNC_IFF_BROADCAST | LNC_IFF_SIMPLEX | LNC_IFF_MULTICAST;

    // Set LAF/Multicast to default = no multicast packet will be received:
    bzero(sc.ladrb, MULTICAST_FILTER_LEN);

    return true;
}

// must be called after lnc_pci_attach (because sc.recv_ring must be set)
void PCNET::lnc_init( void )
{
    int i;
    char *lnc_mem;

    pcnet32_stop();
    if (read_csr_wio(0) == 4 && pcnet32_check_wio()) {
        DEBUG_LOG("%s: In WIO mode\n", getName());
        sc.dwio = false;
    }
    else if (read_csr_dwio(0) == 4 && pcnet32_check_wio()) {
        DEBUG_LOG("%s: In DWIO mode\n", getName());
        sc.dwio = true;
    }
    else {
        IOLog("%s: Cannot determine DWIO or WIO mode - Defaulting to DWIO\n", getName());
        sc.dwio = true;
    }

    sc.if_flags |= LNC_IFF_BROADCAST | LNC_IFF_SIMPLEX | LNC_IFF_MULTICAST; /* XXX??? */

    sc.recv_next = 0;
    sc.trans_ring = sc.recv_ring + LNC_NDESC(sc.nrdre);
    sc.trans_next = 0;

    lnc_mem = (char *) (sc.trans_ring + LNC_NDESC(sc.ntdre));

    lnc_mem = (char *)(((UInt32)lnc_mem + 1) & ~1);
    sc.init_block = (struct pcnet32_init_block *) ((UInt32) lnc_mem & ~1);
    lnc_mem = (char *) (sc.init_block + 1);
    lnc_mem = (char *)(((UInt32)lnc_mem + 0xF) & ~0xF);

    /* Initialise pointers to descriptor entries */
    for (i = 0; i < LNC_NDESC(sc.nrdre); i++) {
        (sc.recv_ring + i)->md = (struct pcnet32_mds *) lnc_mem;
        lnc_mem += sizeof(struct pcnet32_mds);
    }
    for (i = 0; i < LNC_NDESC(sc.ntdre); i++) {
        (sc.trans_ring + i)->md = (struct pcnet32_mds *) lnc_mem;
        lnc_mem += sizeof(struct pcnet32_mds);
    }

    /* Initialise the remaining ring entries */

    for (i = 0; i < LNC_NDESC(sc.nrdre); i++) {
        (sc.recv_ring + i)->md->buffer =
                iokit_kvtop(sc.memDescr, (char*)sc.recv_ring, lnc_mem);
        (sc.recv_ring + i)->md->length = -LNC_RECVBUFSIZE;
        (sc.recv_ring + i)->md->status = RMD_OWN;
        (sc.recv_ring + i)->md->misc.rx_msg_length = 0;
        (sc.recv_ring + i)->data = lnc_mem;
        lnc_mem += LNC_RECVBUFSIZE;
    }
    for (i = 0; i < LNC_NDESC(sc.ntdre); i++) {
        (sc.trans_ring + i)->md->buffer =
                iokit_kvtop(sc.memDescr, (char*)sc.recv_ring, lnc_mem);
        (sc.trans_ring + i)->md->length = 0;
        (sc.trans_ring + i)->md->status = 0;
        (sc.trans_ring + i)->md->misc.tx_misc.ext_status = 0;
        (sc.trans_ring + i)->data = lnc_mem;
        lnc_mem += LNC_TRANSBUFSIZE;
    }

    sc.next_to_send = 0;

    /* Set up initialisation block */

    // From the manual:
    // "The mode register field of the initialization block is copied
    // into CSR15 and interpreted according to the description
    // of CSR15."
    sc.init_block->mode = sc.nic_mode;

    for (i = 0; i < ETHER_ADDR_LEN; i++)
        sc.init_block->phys_addr[i] = lnc_inb(i);

    for (i = 0; i < MULTICAST_FILTER_LEN; i++)
        sc.init_block->ladr.ladrb[i] = sc.ladrb[i];

    sc.init_block->rx_ring = iokit_kvtop(sc.memDescr, (char*)sc.recv_ring, (char*)sc.recv_ring->md);
    sc.init_block->tx_ring = iokit_kvtop(sc.memDescr, (char*)sc.recv_ring, (char*)sc.trans_ring->md);
    sc.init_block->tlen_rlen = (sc.ntdre << 12) | (sc.nrdre << 4);

    /* Set flags to show that the memory area is valid */
    sc.flags |= LNC_INITIALISED;

    sc.pending_transmits = 0;

    // Reset pcnet32
    pcnet32_reset();

    // Switch pcnet32 to 32bit mode
    write_bcr(20, 2);   // Style "PCnet-PCI", SSIZE32=1

    // Set full duplex
    lnc_set_medium(MEDIUM_INDEX_10_FD);

    // Activate Link Status LEDs (used for getting update on link status in watchdog)
    u_short val = read_bcr(LNKST);
    write_bcr(LNKST, val | LNKST_FDLSE | LNKST_LNKSTE);

    /* Give the LANCE the physical address of the initialisation block */
    u_long init_block = iokit_kvtop(sc.memDescr, (char*)sc.recv_ring, (char*)sc.init_block);
    write_csr(INIT_BLOCK_ADDRESS_LOW, (u_short) (init_block & 0xffff));
    write_csr(INIT_BLOCK_ADDRESS_HIGH, (u_short) (init_block >> 16));

    /* Set Error Masks for: JABM, RCVCCOM, MFCOM, TXSTRTM
       Also: APAD_XMT (Padding)
     */
    write_csr(FEATURE_CONTROL, CSR4_APAD_XMT | CSR4_MFCOM | CSR4_RCVCCOM
              | CSR4_TXSTRTM | CSR4_JABM);

    /*
     * Depending on which controller this is, CSR3 has different meanings.
     * For the Am7990 it controls DMA operations, for the Am79C960 it
     * controls interrupt masks and transmitter algorithms. In either
     * case, none of the flags are set.
     *
     */
    //write_csr(INTERRUPT_MASK, 0);

    write_csr(CSR, CSR_INIT);
    for (i = 0; i < 1000; i++)
        if (read_csr(CSR) & CSR_IDON)
            break;

    if (read_csr(CSR) & CSR_IDON) {
        /*
         * Enable interrupts, start the LANCE, mark the interface as
         * running and transmit any pending packets.
         */
        pcnet32_start();
        sc.if_drv_flags |= LNC_IFF_DRV_RUNNING;
        sc.if_drv_flags &= ~LNC_IFF_DRV_OACTIVE;

        if ( transmitQueue )
            transmitQueue->service( IOBasicOutputQueue::kServiceAsync );
    }
    else
        IOLog("%s: Initialisation failed\n", getName());
}

// - called from outPacket, which is itself a callback
UInt32 PCNET::lnc_output_packet(mbuf_t pkt, void * param)
{
    struct host_ring_entry *desc;
    long len;

    if (sc.pending_transmits >= LNC_NDESC(sc.ntdre)) {

        // Stall the output queue until the ack by the interrupt handler.
        return kIOReturnOutputStall;
    }

    sc.pending_transmits++;

    desc = sc.trans_ring + sc.next_to_send;
    len = mbuf_to_buffer(pkt, desc->data);
    desc->md->misc.tx_misc.ext_status = 0;
    //desc->md->length = -max(len, ETHER_MIN_LEN - ETHER_CRC_LEN);
    desc->md->length = -len;
    desc->md->status = TMD_OWN | TMD_STP | TMD_ENP;
    LNC_INC_MD_PTR(sc.next_to_send, sc.ntdre)

            /* Force an immediate poll of the transmit ring */
            //lnc_outw(sc.rdp, LNC_TDMD | LNC_INEA);
    write_csr(CSR,  CSR_TDMD | CSR_IENA);
    //lnc_dump_state();

    if (sc.pending_transmits >= LNC_NDESC(sc.ntdre)) {
        /*
        * Transmit ring is full so set IFF_DRV_OACTIVE
        * since we can't buffer any more packets.
        */

        sc.if_drv_flags |= LNC_IFF_DRV_OACTIVE;
        // - LNCSTATS(trans_ring_full)
    }
    return kIOReturnOutputSuccess;
}

long PCNET::mbuf_to_buffer(mbuf_t pkt, char *buffer)
{
    long pkt_len;
    mbuf_t mn;

    pkt_len = mbuf_pkthdr_len(pkt);

    for ( mn = pkt; mn; mn = mbuf_next(mn) ) {
        bcopy( mbuf_data(mn), buffer, mbuf_len(mn) );
        buffer += mbuf_len(mn);
    }

    return pkt_len;
}

/*
 * The interrupt flag (INTR) will be set and provided that the interrupt enable
 * flag (INEA) is also set, the interrupt pin will be driven low when any of
 * the following occur:
 *
 * 1) Completion of the initialisation routine (IDON). 2) The reception of a
 * packet (RINT). 3) The transmission of a packet (TINT). 4) A transmitter
 * timeout error (BABL). 5) A missed packet (MISS). 6) A memory error (MERR).
 *
 * The interrupt flag is cleared when all of the above conditions are cleared.
 *
 * If the driver is reset from this routine then it first checks to see if any
 * interrupts have ocurred since the reset and handles them before returning.
 * This is because the NIC may signify a pending interrupt in CSR0 using the
 * INTR flag even if a hardware interrupt is currently inhibited (at least I
 * think it does from reading the data sheets). We may as well deal with
 * these pending interrupts now rather than get the overhead of another
 * hardware interrupt immediately upon returning from the interrupt handler.
 *
 */
void PCNET::lnc_intr( bool* flushInputQ, bool* serviceOutputQ)
{
    u_short csr0;
    u_short rap;

    rap = sc.dwio ? lnc_indw(PCNET32_DWIO_RAP) : lnc_inw(PCNET32_WIO_RAP);

    //lnc_dump_state();

    /*
     * INEA is the only bit that can be cleared by writing a 0 to it so
     * we have to include it in any writes that clear other flags.
     */

    while ((csr0 = read_csr(CSR)) & (CSR_INTR)) {

        write_csr(CSR, csr0);

        if (csr0 & CSR_ERR) {
            if (csr0 & CSR_CERR) {
                BUMP_ETHER_COUNTER( sqeTestErrors );
                IOLog("%s: Heartbeat error -- SQE test failed\n", getName());
                //- LNCSTATS(cerr)
            }
            if (csr0 & CSR_BABL) {
                BUMP_ETHER_TX_COUNTER( phyErrors );
                IOLog("%s: Babble error - more than 1519 bytes transmitted\n", getName());
                //- LNCSTATS(babl)
                //- sc.ifp->if_oerrors++;
            }
            if (csr0 & CSR_MISS) {
                BUMP_ETHER_COUNTER( missedFrames );
                IOLog("%s: Missed packet -- no receive buffer\n", getName());
                //- LNCSTATS(miss)
                //- sc.ifp->if_ierrors++;
            }
            if (csr0 & CSR_MERR) {
                IOLog("%s: Memory error  -- Resetting\n", getName());
                //- LNCSTATS(merr)
                lnc_reset();
                continue;
            }
        }
        if (csr0 & CSR_RINT) {
            //- LNCSTATS(rint)
            lnc_rint(flushInputQ);
        }
        if (csr0 & CSR_TINT) {
            //- LNCSTATS(tint)
            //- sc.ifp->if_timer = 0;
            lnc_tint(serviceOutputQ);
        }
    }

    if (sc.dwio)
        lnc_outdw(PCNET32_DWIO_RAP, rap);
    else
        lnc_outw(PCNET32_WIO_RAP, rap);
}

void PCNET::lnc_rint( bool * queued )
{
    struct host_ring_entry *next, *start;
    int start_of_packet;
    mbuf_t pkt;
    struct ether_header *eh;
    int lookahead;
    int flags;
    UInt16 pkt_len;

    /*
     * The LANCE will issue a RINT interrupt when the ownership of the
     * last buffer of a receive packet has been relinquished by the LANCE.
     * Therefore, it can be assumed that a complete packet can be found
     * before hitting buffers that are still owned by the LANCE, if not
     * then there is a bug in the driver that is causing the descriptors
     * to get out of sync.
     */

#ifdef DEBUG
    if ((sc.recv_ring + sc.recv_next)->md->status & RMD_OWN) {
        DEBUG_LOG("%s: Receive interrupt with buffer still owned by controller -- Resetting\n", getName());
        BUMP_ETHER_RX_COUNTER( resets );
        lnc_reset();
        return;
    }
    if (!((sc.recv_ring + sc.recv_next)->md->status & RMD_STP)) {
        DEBUG_LOG("%s: Receive interrupt but not start of packet -- Resetting\n", getName());
        BUMP_ETHER_RX_COUNTER( resets );
        lnc_reset();
        return;
    }
#endif

    lookahead = 0;
    next = sc.recv_ring + sc.recv_next;
    while ((flags = next->md->status) & RMD_STP) {

        /* Make a note of the start of the packet */
        start_of_packet = sc.recv_next;

        /*
         * Find the end of the packet. Even if not data chaining,
         * jabber packets can overrun into a second descriptor.
         * If there is no error, then the ENP flag is set in the last
         * descriptor of the packet. If there is an error then the ERR
         * flag will be set in the descriptor where the error occured.
         * Therefore, to find the last buffer of a packet we search for
         * either ERR or ENP.
         */

        if (!(flags & (RMD_ENP | RMD_ERR))) {
            do {
                LNC_INC_MD_PTR(sc.recv_next, sc.nrdre)
                next = sc.recv_ring + sc.recv_next;
                flags = next->md->status;
            } while (!(flags & (RMD_STP | RMD_OWN | RMD_ENP | RMD_ERR)));

            if (flags & RMD_STP) {
                IOLog("%s: Start of packet found before end of previous in receive ring -- Resetting\n", getName());
                BUMP_NET_COUNTER( inputErrors );
                BUMP_ETHER_RX_COUNTER( resets );
                lnc_reset();
                return;
            }
            if (flags & RMD_OWN) {
                if (lookahead) {
                    /*
                     * Looked ahead into a packet still
                     * being received
                     */
                    sc.recv_next = start_of_packet;
                    break;
                }
                else {
                    IOLog("%s: End of received packet not found-- Resetting\n", getName());
                    BUMP_NET_COUNTER( inputErrors );
                    BUMP_ETHER_RX_COUNTER( resets );
                    lnc_reset();
                    return;
                }
            }
        }

        pkt_len = (next->md->misc.rx_msg_length & MDHEAD_LEN_MASK) - ETHER_CRC_LEN;

        /* Move pointer onto start of next packet */
        LNC_INC_MD_PTR(sc.recv_next, sc.nrdre)
        next = sc.recv_ring + sc.recv_next;

        if (flags & RMD_ERR) {
            const char *if_xname = getName();

            BUMP_NET_COUNTER( inputErrors );

            if (flags & RMD_BUFF) {
                //-LNCSTATS(rbuff)
                IOLog("%s: Receive buffer error\n", if_xname);
            }
            if (flags & RMD_OFLO) {
                /* OFLO only valid if ENP is not set */
                if (!(flags & RMD_ENP)) {
                    //- LNCSTATS(oflo)
                    IOLog("%s: Receive overflow error \n", if_xname);
                }
            }
            else if (flags & RMD_ENP) {
                if ((sc.if_flags & LNC_IFF_PROMISC)==0) {
                    /*
                     * FRAM and CRC are valid only if ENP
                     * is set and OFLO is not.
                     */
                    if (flags & RMD_FRAM) {
                        //- LNCSTATS(fram)
                        BUMP_ETHER_COUNTER( alignmentErrors );
                        IOLog("%s: Framing error\n", if_xname);
                        /*
                         * FRAM is only set if there's a CRC
                         * error so avoid multiple messages
                         */
                    }
                    else if (flags & RMD_CRC) {
                        //-LNCSTATS(crc)
                        BUMP_ETHER_COUNTER( fcsErrors );
                        IOLog("%s: Receive CRC error\n", if_xname);
                    }
                }
            }

            /* Drop packet */
            //- LNCSTATS(rerr)
            //- ifp->if_ierrors++;
            while (start_of_packet != sc.recv_next) {
                start = sc.recv_ring + start_of_packet;
                start->md->length = -LNC_RECVBUFSIZE; /* XXX - shouldn't be necessary */
                start->md->status = RMD_OWN;
                LNC_INC_MD_PTR(start_of_packet, sc.nrdre)
            }
        }
        else { /* Valid packet */

            //-ifp->if_ipackets++;

            pkt = mbuf_packet(start_of_packet, pkt_len);

            if (pkt) {
                // First mbuf in packet holds the
                // ethernet and packet headers
                //- head->m_pkthdr.rcvif = ifp;
                //- head->m_pkthdr.len = pkt_len ;
                eh = (struct ether_header *) mbuf_data(pkt);

                // vmware ethernet hardware emulation loops
                // packets back to itself, violates IFF_SIMPLEX.
                // drop it if it is from myself.
                if (bcmp(eh->ether_shost,
                        sc.init_block->phys_addr, ETHER_ADDR_LEN) == 0) {

                    freePacket(pkt);
                    DEBUG_LOG("%s: Packet dropped, sent to its sender MAC\n", getName());

                }
                else {
                    assert ( netif );
                    netif->inputPacket( pkt, pkt_len, IONetworkInterface::
                            kInputOptionQueuePacket );
                    // TODO:
                    BUMP_NET_COUNTER( inputPackets );
                    *queued = true;
                }
            }
            else {
                BUMP_ETHER_RX_COUNTER( resourceErrors );
                IOLog("%s: Packet dropped, no mbufs\n", getName());
                //- LNCSTATS(drop_packet)

            }
        }

        lookahead++;
    }

    BUMP_ETHER_RX_COUNTER( interrupts );

    write_csr(CSR, CSR_RINT |  CSR_IENA);
}

mbuf_t PCNET::mbuf_packet(int start_of_packet, UInt16 pkt_len)
{
    mbuf_t pkt;
    pkt = allocatePacket( pkt_len );
    if ( 0 == pkt ) {
        return 0;
    }

    struct host_ring_entry *start;
    char *data;
    short blen;
    int amount;
    int pkt_offs = 0;

    start = sc.recv_ring + start_of_packet;
    blen = LNC_RECVBUFSIZE; /* XXX More PCnet-32 crap */
    data = start->data;

    while (start_of_packet != sc.recv_next) {
        /*
         * If the data left fits in a single buffer then set
         * blen to the size of the data left.
         */
        if (pkt_len < blen)
            blen = pkt_len;

        amount = blen;

        bcopy( data, ((char*)mbuf_data(pkt))+pkt_offs, amount );

        blen -= amount;
        data += amount;
        pkt_len -= amount;
        pkt_offs += amount;

        if (blen == 0) {
            start->md->status = RMD_OWN;
            start->md->length = -LNC_RECVBUFSIZE; /* XXX - shouldn't be necessary */
            LNC_INC_MD_PTR(start_of_packet, sc.nrdre)
            start = sc.recv_ring + start_of_packet;
            data = start->data;
            blen = LNC_RECVBUFSIZE; /* XXX More PCnet-32 crap */
        }
    }
    return pkt;
}


void PCNET::lnc_tint( bool* reclaimed )
{
    struct host_ring_entry *next, *start;
    int start_of_packet;
    int lookahead;

    /*
     * If the driver is reset in this routine then we return immediately to
     * the interrupt driver routine. Any interrupts that have occured
     * since the reset will be dealt with there. sc.trans_next
     * should point to the start of the first packet that was awaiting
     * transmission after the last transmit interrupt was dealt with. The
     * LANCE should have relinquished ownership of that descriptor before
     * the interrupt. Therefore, sc.trans_next should point to a
     * descriptor with STP set and OWN cleared. If not then the driver's
     * pointers are out of sync with the LANCE, which signifies a bug in
     * the driver. Therefore, the following two checks are really
     * diagnostic, since if the driver is working correctly they should
     * never happen.
     */

#ifdef DEBUG
    if ((sc.trans_ring + sc.trans_next)->md->status & TMD_OWN) {
        DEBUG_LOG("%s: Transmit interrupt with buffer still owned by controller -- Resetting\n", getName());
        BUMP_ETHER_TX_COUNTER( resets );
        lnc_reset();
        return;
    }
#endif


    /*
     * The LANCE will write the status information for the packet it just
     * tried to transmit in one of two places. If the packet was
     * transmitted successfully then the status will be written into the
     * last descriptor of the packet. If the transmit failed then the
     * status will be written into the descriptor that was being accessed
     * when the error occured and all subsequent descriptors in that
     * packet will have been relinquished by the LANCE.
     *
     * At this point we know that sc.trans_next points to the start
     * of a packet that the LANCE has just finished trying to transmit.
     * We now search for a buffer with either ENP or ERR set.
     */

    lookahead = 0;

    do {
        start_of_packet = sc.trans_next;
        next = sc.trans_ring + sc.trans_next;

#ifdef DEBUG
        if (!(next->md->status & TMD_STP)) {
            DEBUG_LOG("%s: Transmit interrupt but not start of packet -- Resetting\n", getName());
            BUMP_ETHER_TX_COUNTER( resets );
            lnc_reset();
            return;
        }
#endif

        /*
         * Find end of packet.
         */

        if (!(next->md->status & (TMD_ENP | TMD_ERR))) {
            do {
                LNC_INC_MD_PTR(sc.trans_next, sc.ntdre)
                next = sc.trans_ring + sc.trans_next;
            } while (!(next->md->status & (TMD_STP | TMD_OWN | TMD_ENP | TMD_ERR)));

            if (next->md->status & TMD_STP) {
                IOLog("%s: Start of packet found before end of previous in transmit ring -- Resetting\n", getName());
                BUMP_ETHER_TX_COUNTER( resets );
                lnc_reset();
                return;
            }
            if (next->md->status & TMD_OWN) {
                if (lookahead) {
                    /*
                     * Looked ahead into a packet still
                     * being transmitted
                     */
                    sc.trans_next = start_of_packet;
                    break;
                }
                else {
                    IOLog("%s: End of transmitted packet not found -- Resetting\n", getName());
                    BUMP_ETHER_TX_COUNTER( resets );
                    lnc_reset();
                    return;
                }
            }
        }
        /*
         * Check for ERR first since other flags are irrelevant if an
         * error occurred.
         */
        if (next->md->status & TMD_ERR) {

            BUMP_NET_COUNTER( outputErrors );

            u_short ext_status = next->md->misc.tx_misc.ext_status;
            //- LNCSTATS(terr)
            //-	sc.ifp->if_oerrors++;

            if (ext_status & TMD_LCOL) {
                //- LNCSTATS(lcol)
                BUMP_ETHER_COUNTER( lateCollisions );
                IOLog("%s: Transmit late collision  -- Net error?\n", getName());
                //- sc.ifp->if_collisions++;

                /*
                 * Clear TBUFF since it's not valid when LCOL
                 * set
                 */
                next->md->misc.tx_misc.ext_status &= ~TMD_TBUFF;
            }
            if (ext_status & TMD_LCAR) {
                //- LNCSTATS(lcar)
                BUMP_ETHER_COUNTER( carrierSenseErrors );
                IOLog("%s: Loss of carrier during transmit -- Net error?\n", getName());
            }
            if (ext_status & TMD_RTRY) {
                //- LNCSTATS(rtry)
                BUMP_ETHER_COUNTER( excessiveCollisions );
                IOLog("%s: Transmit of packet failed after 16 attempts\n", getName());

                //- sc.ifp->if_collisions += 16;
                /*
                 * Clear TBUFF since it's not valid when RTRY
                 * set
                 */
                next->md->misc.tx_misc.ext_status &= ~TMD_TBUFF;
            }
            /*
             * TBUFF is only valid if neither LCOL nor RTRY are set.
             * We need to check UFLO after LCOL and RTRY so that we
             * know whether or not TBUFF is valid. If either are
             * set then TBUFF will have been cleared above. A
             * UFLO error will turn off the transmitter so we
             * have to reset.
             *
             */

            if (ext_status & TMD_UFLO) {
                BUMP_ETHER_TX_COUNTER( underruns );
                //- LNCSTATS(uflo)
                /*
                 * If an UFLO has occured it's possibly due
                 * to a TBUFF error
                 */
                if (ext_status & TMD_TBUFF) {
                    //- LNCSTATS(tbuff)
                    IOLog("%s: Transmit buffer error -- Resetting\n", getName());
                }
                else
                    IOLog("%s: Transmit underflow error -- Resetting\n", getName());

                BUMP_ETHER_TX_COUNTER( resets );
                lnc_reset();
                return;
            }
            do {
                LNC_INC_MD_PTR(sc.trans_next, sc.ntdre)
                next = sc.trans_ring + sc.trans_next;
            } while (!(next->md->status & TMD_STP) && (sc.trans_next != sc.next_to_send));

        }
        else {
            /*
             * Since we check for ERR first then if we get here
             * the packet was transmitted correctly. There may
             * still have been non-fatal errors though.
             * Don't bother checking for DEF, waste of time.
             */

            //- sc.ifp->if_opackets++;

            if (next->md->status & TMD_MORE) {
                //- LNCSTATS(more)
                //- sc.ifp->if_collisions += 2;
                BUMP_ETHER_COUNTER( multipleCollisionFrames );
            }

            /*
             * ONE is invalid if LCOL is set. If LCOL was set then
             * ERR would have also been set and we would have
             * returned from lnc_tint above. Therefore we can
             * assume if we arrive here that ONE is valid.
             *
             */

            if (next->md->status & TMD_ONE) {
                //- LNCSTATS(one)
                //- sc.ifp->if_collisions++;
                BUMP_ETHER_COUNTER( singleCollisionFrames );
            }
            LNC_INC_MD_PTR(sc.trans_next, sc.ntdre)
            next = sc.trans_ring + sc.trans_next;
        }

        /*
         * Clear descriptors and free any mbufs.
         */

        do {
            start = sc.trans_ring + start_of_packet;
            start->md->status = 0;
            //- if (sc.nic.mem_mode == DMA_MBUF) {
            sc.pending_transmits--;
            LNC_INC_MD_PTR(start_of_packet, sc.ntdre)
        }while (start_of_packet != sc.trans_next);

        /*
         * There's now at least one free descriptor
         * in the ring so indicate that we can accept
         * more packets again.
         */

        sc.if_drv_flags &= ~LNC_IFF_DRV_OACTIVE;

        // TODO: is this ok here? (it's also in watchdog method!)
        if ( transmitQueue && sc.pending_transmits )
            transmitQueue->service( IOBasicOutputQueue::kServiceAsync );

        lookahead++;

    } while (sc.pending_transmits && !(next->md->status & TMD_OWN));

    *reclaimed = true;

    BUMP_ETHER_TX_COUNTER( interrupts );

    write_csr(CSR, CSR_TINT |  CSR_IENA);
}


void PCNET::lnc_release_resources( void )
{
    sc.memDescr->complete();
    sc.memDescr->release();
    sc.memDescr = 0;
    sc.recv_ring = 0;
    sc.trans_ring = 0;
}


static inline u_long ether_crc32_le(const unsigned char *buf, int len)
{
    static const u_long crctab[] = {
        0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
        0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
        0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
        0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
    };
    int i;
    u_long crc;

    crc = 0xffffffff;       /* initial value */

    for (i = 0; i < len; i++) {
        crc ^= buf[i];
        crc = (crc >> 4) ^ crctab[crc & 0xf];
        crc = (crc >> 4) ^ crctab[crc & 0xf];
    }

    return(crc);
}

/*
 * Set up the logical address filter for multicast packets
 */
void PCNET::lnc_setladrf( IOEthernetAddress * addrs, UInt32 count )
{
    u_long index;

    if (sc.flags & LNC_IFF_ALLMULTI) {
        for (int i=0; i < MULTICAST_FILTER_LEN; i++)
            sc.ladrb[i] = PCNET_MULTI_INIT_ADDR;
    }
    else {
        /*
         * For each multicast address, calculate a crc for that address and
         * then use the high order 6 bits of the crc as a hash code where
         * bits 3-5 select the byte of the address filter and bits 0-2 select
         * the bit within that byte.
         */

        bzero(sc.ladrb, MULTICAST_FILTER_LEN);
        if (addrs) {
            //- IF_ADDR_LOCK(ifp);
            for ( UInt32 i = 0; i < count; i++, addrs++ ) {
                index = ether_crc32_le( (const unsigned char *) addrs,
                                        ETHER_ADDR_LEN) >> 26;
                sc.ladrb[index >> 3] |= 1 << (index & 7);
            }
            //- IF_ADDR_UNLOCK(ifp);
        }
    }
}

void PCNET::lnc_set_multicast(IOEthernetAddress * addrs, UInt32 count)
{
    pcnet32_stop();
    lnc_setladrf(addrs, count);
    for (int i = LAF_START; i < LAF_START+4; i++)
        write_csr(i, (sc.ladrb[i*2+1]<<8) | sc.ladrb[i*2]);
    pcnet32_start();
}


// TODO
void PCNET::lnc_watchdog( void )
{
    // if the IOOutputQueue has stalled (due to a kIOReturnStall from outputPacket),
    // then restart it
    if ( transmitQueue && sc.pending_transmits )
         //&& !(sc.if_drv_flags & LNC_IFF_DRV_OACTIVE) )
    {
        transmitQueue->service( IOBasicOutputQueue::kServiceAsync );
    }
}

void PCNET::lnc_reset( void )
{
    pcnet32_stop();
    pcnet32_reset();
    IOSleep(20);

    lnc_init();
}

bool PCNET::lnc_set_medium(MediumIndex mediumIndex)
{
    u_short val = read_bcr(FDCTL);
    switch (mediumIndex) {
        case MEDIUM_INDEX_10_FD:
            write_bcr(FDCTL, val | FDCTL_AUIFD | FDCTL_FDEN);
            break;
        case MEDIUM_INDEX_10_HD:
            write_bcr(FDCTL, val & ~(FDCTL_AUIFD | FDCTL_FDEN));
            break;
        case MEDIUM_INDEX_NONE:
        default:
            break;
    }
    return true;
}

MediumIndex PCNET::lnc_get_medium( void )
{
    u_short val = read_bcr(FDCTL);
    if (val & FDCTL_FDEN)
        return MEDIUM_INDEX_10_FD;
    else
        return MEDIUM_INDEX_10_HD;
}

// Returns true if connected to medium (link state pass)
bool PCNET::lnc_is_medium_available( void )
{
    return read_bcr(LNKST) & LNKST_LEDOUT;
}

void PCNET::lnc_set_promiscuous_mode( bool enable )
{
    if (enable) {
        sc.if_flags |= LNC_IFF_PROMISC;
        sc.nic_mode |= MODE_PROM;
        lnc_init();
    }
    else if (sc.if_flags & LNC_IFF_PROMISC) {
        sc.if_flags &= ~LNC_IFF_PROMISC;
        sc.nic_mode &= ~MODE_PROM;
        lnc_init();
    }
}


void PCNET::lnc_dump_state( void )
{
#ifdef DEBUG
    int i;

    DEBUG_LOG("\nDriver/NIC [%s] state dump\n", getName());
    DEBUG_LOG("Host memory\n");
    DEBUG_LOG("-----------\n");

    DEBUG_LOG("Receive ring: base = %p, next = %d = %p\n",
            (void *)sc.recv_ring, sc.recv_next,
            (void *)(sc.recv_ring + sc.recv_next));

    for (i = 0; i < LNC_NDESC(sc.nrdre); i++)
        DEBUG_LOG("\t%d:%p md = %p buff = %p\n",
                i, (void *)(sc.recv_ring + i),
                (void *)(sc.recv_ring + i)->md,
                (void *)(sc.recv_ring + i)->data);

    DEBUG_LOG("Transmit ring: base = %p, next = %d = %p\n",
            (void *)sc.trans_ring, sc.trans_next,
            (void *)(sc.trans_ring + sc.trans_next));

    for (i = 0; i < LNC_NDESC(sc.ntdre); i++)
        DEBUG_LOG("\t%d:%p md = %p buff = %p\n",
                i, (void *)(sc.trans_ring + i),
                (void *)(sc.trans_ring + i)->md,
                (void *)(sc.trans_ring + i)->data);
    DEBUG_LOG("Lance memory (may be on host(DMA) or card(SHMEM))\n");
    DEBUG_LOG("Init block = %p\n", (void *)sc.init_block);
    DEBUG_LOG("\tmode = %x rlen:rdra = %x:%x tlen:tdra = %x:%x\n",
            sc.init_block->mode,
            (sc.init_block->tlen_rlen>>4)&0xf, sc.init_block->rx_ring,
            sc.init_block->tlen_rlen>>12, sc.init_block->tx_ring);
    DEBUG_LOG("Receive descriptor ring\n");
    for (i = 0; i < LNC_NDESC(sc.nrdre); i++)
        DEBUG_LOG("\t%d buffer = 0x%x, BCNT = %d,  MCNT = %u,  flags = %x\n",
                i, (sc.recv_ring + i)->md->buffer,
                -(short) (sc.recv_ring + i)->md->length,
                (sc.recv_ring + i)->md->misc.rx_msg_length,
                (sc.recv_ring + i)->md->status);
    DEBUG_LOG("Transmit descriptor ring\n");
    for (i = 0; i < LNC_NDESC(sc.ntdre); i++)
        DEBUG_LOG("\t%d buffer = 0x%x, BCNT = %d,   flags = %x %x\n",
                i, (sc.trans_ring + i)->md->buffer,
                -(short) (sc.trans_ring + i)->md->length,
                (sc.trans_ring + i)->md->status,
                (sc.trans_ring + i)->md->misc.tx_misc.ext_status);
    DEBUG_LOG("\nnext_to_send = %d, trans_next = %d, recv_next = %d\n",
            sc.next_to_send, sc.trans_next, sc.recv_next);
    DEBUG_LOG("\n CSR0 = %x CSR1 = %x CSR2 = %x CSR3 = %x\n\n",
            read_csr(CSR), read_csr(CSR+1),
            read_csr(CSR+2), read_csr(CSR+3));
#endif
}


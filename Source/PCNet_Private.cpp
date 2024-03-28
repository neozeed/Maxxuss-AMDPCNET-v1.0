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

void PCNET::enableHardwareInterrupts( void )
{
    interruptEnabled = true;
}

void PCNET::disableHardwareInterrupts( void )
{
    interruptEnabled = false;
}

//---------------------------------------------------------------------------

bool PCNET::allocateDescriptorMemory( void )
{
    return lnc_pci_attach();
}

//---------------------------------------------------------------------------

bool PCNET::initAdapter( IOOptionBits options )
{
    DEBUG_LOG("initAdapter() ===>\n");

    disableHardwareInterrupts();
    mediumIndexLastReported = MEDIUM_INDEX_UNDEF;
    if (options & kResetChip) {
        pcnet32_stop();
        pcnet32_reset();
        IOSleep(20);
    }
    else
        lnc_init();

    DEBUG_LOG("initAdapter() <===\n");

    return true;
}

//---------------------------------------------------------------------------

void PCNET::interruptOccurred( OSObject *owner,
	IOInterruptEventSource * src, int count )
{
	PCNET *self = (PCNET*) owner;
	self->handleInterrupt(src,count);
}


void PCNET::handleInterrupt( IOInterruptEventSource * src, int count )
{
    bool    flushInputQ    = false;
    bool	serviceOutputQ = false;
#ifdef KDB_SUPPORT
    IODebuggerLockState lockState;
#endif

    // PCI drivers must be prepared to handle spurious interrupts when the
    // interrupt line is shared with other devices. The interruptEnabled
    // flag prevents the driver from touching hardware before it is ready. 

    if ( false == interruptEnabled ) return;

#ifdef KDB_SUPPORT
    lockState = IODebuggerLock( this );
#endif

    lnc_intr(&flushInputQ, &serviceOutputQ);

#ifdef KDB_SUPPORT
    // If debugger reclaimed the tx buffer, then we will still expect a
    // hardware interrupt after returning from the debugger, but the tx
    // interrupt handler will not reclaim any buffer space. If the output
    // queue was previously stalled, then that could spell trouble. To
    // prevent this, service the output queue when this condition exists.
/*
    if ( tx_buf_reclaimed )
    {
        serviceOutputQ = true;
        tx_buf_reclaimed = false;
    }
*/

    IODebuggerUnlock( lockState );
#endif

    // Flush all inbound packets and pass them to the network stack.
    // Interrupts are not enabled until the network interface is enabled
    // by BSD, so netif must be valid.

    assert( netif );
    if ( flushInputQ ) netif->flushInputQueue();

    // Call service() without holding the debugger lock to prevent a
    // deadlock when service() calls our outputPacket() function.

    if ( serviceOutputQ ) transmitQueue->service();
}

//---------------------------------------------------------------------------
// Note: This function must not block. Otherwise it may expose
// re-entrancy issues in the BSD networking stack.

UInt32 PCNET::outputPacket( mbuf_t pkt, void * param )
{
#ifdef KDB_SUPPORT
    IODebuggerLockState lockState;
#endif

    //DEBUG_LOG("outputPacket() ===>\n");

#ifdef KDB_SUPPORT
    lockState = IODebuggerLock( this );
#endif

    UInt32 ret = lnc_output_packet(pkt, param);

#ifdef KDB_SUPPORT
	IODebuggerUnlock( lockState );
#endif

    if (ret == kIOReturnOutputSuccess) {
        freePacket( pkt );
        BUMP_NET_COUNTER( outputPackets );
    }

	//DEBUG_LOG("outputPacket() <===\n");

    return ret;
}






















#ifdef KDB_SUPPORT

//---------------------------------------------------------------------------
// Send KDP packets in polled mode.

void PCNET::sendPacket( void * pkt, UInt32 pkt_len )
{
/*
    UInt8 * dest_ptr;

    if ( pkt_len > MAXPACK ) return;

    // Poll until a transmit buffer becomes available.

    while ( kOwnedByChip == tx_buf_ownership[tx_send_index] )
    {
        reclaimTransmitBuffer();
        // kprintf("PCNET: transmitterInterrupt poll\n");
    }

    dest_ptr = tx_buf_ptr[ tx_send_index ];

    // Copy the debugger packet to the transmit buffer.
    
    bcopy( pkt, dest_ptr, pkt_len );

    // Pad small frames.

    if ( pkt_len < MINPACK )
    {
        memset( dest_ptr + pkt_len, 0x55, MINPACK - pkt_len );
        pkt_len = MINPACK;
    }

    csrWrite32( RTL_TSD0 + (4 * tx_send_index), pkt_len | R_TSD_ERTXTH );

    tx_send_count++;
    tx_buf_ownership[tx_send_index] = kOwnedByChip;

    if ( 4 == ++tx_send_index ) tx_send_index = 0;
    
    // kprintf("PCNET: sendPacket len %d\n", pkt_len);
*/
}

void PCNET::reclaimTransmitBuffer( void )
{
/*
    UInt32  tx_status;
    UInt32  tmp_ul;

    tx_buf_reclaimed = true;

    while ( tx_send_count )
    {
        tx_status = csrRead32( RTL_TSD0 + tx_ack_index * 4 );

        if ( (tx_status & (R_TSD_TABT | R_TSD_TOK | R_TSD_TUN)) == 0 )
            break;  // not owned by host (check OWN bit instead?)

        if ( tx_status & R_TSD_TABT ) // transmit abort
        {
            tmp_ul = csrRead32( RTL_TCR );
            tmp_ul |= R_TCR_CLRABT;   // resend packet 
            csrWrite32( RTL_TCR, tmp_ul );
            break;  // wait for retransmission
        }

        tx_send_count--;
        tx_buf_ownership[tx_ack_index] = kOwnedByHost;

        if ( 4 == ++tx_ack_index ) tx_ack_index = 0;
    }
*/
}

//---------------------------------------------------------------------------
// Receive (KDP) packets in polled mode. This is essentially a simplified
// version of the receiverInterrupt() function.

void PCNET::receivePacket( void * pkt, UInt32 * pkt_len, UInt32 timeout )
{
/*
	rbuf_hdr_t  rbh;     // 4 byte rx packet packet
    UInt16      offset;
	UInt16	    capr;
	UInt16	    rcv_len;

    *pkt_len = 0;
    timeout *= 1000;  // convert ms to us

    while ( timeout && *pkt_len == 0 )
    {
        if ( (csrRead8( RTL_CM ) & R_CM_BUFE) == R_CM_BUFE )
        {
            // Receive buffer empty, wait and retry.
            IODelay( 50 );
            timeout -= 50;
            continue;
        }

        *((UInt32 *)&rbh) = OSReadLittleInt32( rx_buf_ptr, 0 );

        rcv_len = rbh.rb_count;

        //kprintf("PCNET::receivePacket status %x len %d\n",
        //        rbh.rb_status, rcv_len);

        if ( rbh.rb_status & (R_RSR_FAE | R_RSR_CRC | R_RSR_LONG |
                              R_RSR_RUNT | R_RSR_ISE) )
        {
            rbh.rb_status &= ~R_RSR_ROK;
        }

        if ( (rbh.rb_status & R_RSR_ROK) == 0 )
        {
            restartReceiver();
            continue;
        }

        if ( (rcv_len >= MINPACK + 4) && (rcv_len <= MAXPACK + 4) ) 
        {
            bcopy( rx_buf_ptr + sizeof(rbh), pkt, rcv_len );
            *pkt_len = rcv_len;
        }

        // Advance the receive ring buffer to the start of the next packet.

        offset = IORound(rcv_len, 4) + sizeof(rbh);
        rx_buf_ptr += offset;

        if (rx_buf_ptr >= (rx_buf_end - 1))
            rx_buf_ptr -= RX_BUF_SIZE;

        capr = csrRead16( RTL_CAPR );
        capr += offset;
        csrWrite16Slow( RTL_CAPR, capr );
    }
*/
}

#endif  // KDB_SUPPORT


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

#ifndef _PCNET_H
#define _PCNET_H

// Uncomment to enable system.log Debug Informations
//#define DEBUG 1

// Not yet implemented, so leave it commented!!!!
//#define KDB_SUPPORT 1

#include <IOKit/assert.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IODeviceMemory.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/network/IOEthernetController.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IOGatedOutputQueue.h>
#include <IOKit/network/IOMbufMemoryCursor.h>
#include <libkern/OSByteOrder.h>
extern "C" {
#include <sys/kpi_mbuf.h>
}

// ---------------- PCNET stuff
#define u_long UInt32
#define u_short UInt16
#define u_char UInt8

#include "PCNet_HW.h"
#include "PCNet_Var.h"
#include "PCNet_lnc.h"
// ----------------

#ifdef  DEBUG
    #warning ***************************************************
    #warning ** DEBUG defined - turn off for deployment build **
    #warning ***************************************************
    #define DEBUG_LOG(args...)  IOLog(args)
#else
    #define DEBUG_LOG(args...)
#endif

#ifdef  KDB_SUPPORT
    #warning *************************************************************
    #warning ** KDB_SUPPORT defined - not yet supported --> turn off!   **
    #warning *************************************************************
#endif

#define BUMP_NET_COUNTER(x) \
        do { netStats->x += 1; } while(0)

#define BUMP_ETHER_COUNTER(x) \
        do { etherStats->dot3StatsEntry.x += 1; } while(0)

#define BUMP_ETHER_RX_COUNTER(x) \
        do { etherStats->dot3RxExtraEntry.x += 1; } while(0)

#define BUMP_ETHER_TX_COUNTER(x) \
        do { etherStats->dot3TxExtraEntry.x += 1; } while(0)

#define kWatchdogTimerPeriod    4000  // milliseconds
#define kTransmitQueueCapacity  384


#define PCNET maxxuss_driver_AMDPCNET

class PCNET : public IOEthernetController {
    OSDeclareDefaultStructors( maxxuss_driver_AMDPCNET )

    protected:
    IOEthernetInterface *      netif;
    IOPCIDevice *              pciNub;
    IOWorkLoop *               workLoop;
    IOInterruptEventSource *   interruptSrc;
    IOOutputQueue *            transmitQueue;
    IOTimerEventSource *       timerSrc;
#ifdef KDB_SUPPORT
    IOKernelDebugger *         debugger;
#endif
    IONetworkStats *           netStats;
    IOEthernetStats *          etherStats;
    IOMemoryMap *              csrMap;
    volatile void *            csrBase;
    OSDictionary *             mediumDict;
    const IONetworkMedium *    mediumTable[MEDIUM_INDEX_COUNT];

    UInt32                     currentLevel;
    bool                       enabledByBSD;
    bool                       enabledByKDP;
    bool                       interruptEnabled;
    MediumIndex                currentMediumIndex;
    MediumIndex                mediumIndexLastReported;

    void handleInterrupt( IOInterruptEventSource * src, int count );

    bool initEventSources( IOService * provider );

    bool initPCIConfigSpace( IOPCIDevice * provider );

    bool enableAdapter( UInt32 level );

    bool disableAdapter( UInt32 level );

    enum {
        kActivationLevel0 = 0,
        kActivationLevel1,
        kActivationLevel2
    };

    bool setActivationLevel( UInt32 newLevel );

    void handleTimeout( IOTimerEventSource * timer );

    bool allocateDescriptorMemory( void );

    enum {
        kFullInitialization = 0,
        kResetChip          = 1
    };

    bool initAdapter( IOOptionBits options );

    void disableHardwareInterrupts( void );

    void enableHardwareInterrupts( void );

    // PHY functions

    bool phyAddMediumType( IOMediumType type, UInt32 bps, MediumIndex index );

    void phyProbeMediaCapability( void );

    bool phyReset( void );

    //- bool phyWaitForAutoNegotiation( void );

    bool phySetMedium( MediumIndex mediumIndex );

    bool phySetMedium( const IONetworkMedium * medium );

    void phyReportLinkStatus( bool forceStatusReport = false );

    const IONetworkMedium * phyGetMediumWithIndex( MediumIndex index ) const;

    // Access hardware registers

    inline void csrWrite32( UInt16 offset, UInt32 value )
    { pciNub->ioWrite32(offset, value, csrMap);}

    inline void csrWrite16( UInt16 offset, UInt16 value )
    { pciNub->ioWrite16(offset, value, csrMap);}

    inline void csrWrite8(  UInt16 offset, UInt8 value )
    { pciNub->ioWrite8(offset, value, csrMap);}

    inline void csrWrite32Slow( UInt16 offset, UInt32 value )
    { pciNub->ioWrite32(offset, value, csrMap); pciNub->ioRead32(offset, csrMap);}

    inline void csrWrite16Slow( UInt16 offset, UInt16 value )
    { pciNub->ioWrite16(offset, value, csrMap); pciNub->ioRead16(offset, csrMap);}

    inline void csrWrite8Slow(  UInt16 offset, UInt8 value )
    { pciNub->ioWrite8(offset, value, csrMap); pciNub->ioRead8(offset, csrMap);}

    inline UInt32 csrRead32( UInt16 offset )
    { return pciNub->ioRead32(offset, csrMap);}

    inline UInt16 csrRead16( UInt16 offset )
    { return pciNub->ioRead16(offset, csrMap);}

    inline UInt8  csrRead8(  UInt16 offset )
    { return pciNub->ioRead8(offset, csrMap);}

public:
    static void timeoutOccurred( OSObject *owner, IOTimerEventSource * timer );
    static void interruptOccurred( OSObject *owner, IOInterruptEventSource * src, int count );

    virtual bool init( OSDictionary * properties );

    virtual bool start( IOService * provider );

    virtual void stop( IOService * provider );

    virtual void free( void );

    virtual IOReturn enable( IONetworkInterface * netif );

    virtual IOReturn disable( IONetworkInterface * netif );

#ifdef KDB_SUPPORT
    virtual IOReturn enable( IOKernelDebugger * netif );

    virtual IOReturn disable( IOKernelDebugger * netif );
#endif

    virtual UInt32 outputPacket( mbuf_t m, void * param );

    virtual void getPacketBufferConstraints(
                                           IOPacketBufferConstraints * constraints ) const;

    virtual IOOutputQueue * createOutputQueue( void );

    virtual const OSString * newVendorString( void ) const;

    virtual const OSString * newModelString( void ) const;

    virtual IOReturn selectMedium( const IONetworkMedium * medium );

    virtual bool configureInterface( IONetworkInterface * interface );

    virtual bool createWorkLoop( void );

    virtual IOWorkLoop * getWorkLoop( void ) const;

    virtual IOReturn getHardwareAddress( IOEthernetAddress * addr );

    virtual IOReturn setPromiscuousMode( bool enabled );

    virtual IOReturn setMulticastMode( bool enabled );

    virtual IOReturn setMulticastList( IOEthernetAddress * addrs,
                                       UInt32              count );

    virtual IOReturn registerWithPolicyMaker( IOService * policyMaker );

    virtual IOReturn setPowerState( unsigned long powerStateOrdinal,
                                    IOService *   policyMaker );

#ifdef KDB_SUPPORT
    virtual void sendPacket( void * pkt, UInt32 pkt_len );

    virtual void receivePacket( void * pkt, UInt32 * pkt_len, UInt32 timeout );
#endif



    // ---------------- PCNET stuff (adapted from FreeBSD's Lance driver (if_lnc.c)

protected:

    lnc_softc_t sc;

    // ---- Low-level Helper Methods
    inline UInt16 read_csr_dwio(UInt16 port)
    {
        lnc_outdw(PCNET32_DWIO_RAP, port);
        return lnc_indw(PCNET32_DWIO_RDP) & 0xffff;
    }

    inline UInt16 read_csr_wio(UInt16 port)
    {
        lnc_outw(PCNET32_WIO_RAP, port);
        return lnc_inw(PCNET32_WIO_RDP);
    }

    inline UInt16 read_csr(UInt16 port)
    {
        if (sc.dwio)
            return read_csr_dwio(port);
        else
            return read_csr_wio(port);
    }

    inline void write_csr(UInt16 port, UInt16 val)
    {
        if (sc.dwio) {
            lnc_outdw(PCNET32_DWIO_RAP, port);
            lnc_outdw(PCNET32_DWIO_RDP, val);
        }
        else {
            lnc_outw(PCNET32_WIO_RAP, port);
            lnc_outw(PCNET32_WIO_RDP, val);
        }
    }

    inline UInt16 read_bcr(UInt16 port)
    {
        if (sc.dwio) {
            lnc_outdw(PCNET32_DWIO_RAP, port);
            return lnc_indw(PCNET32_DWIO_BDP) & 0xffff;
        }
        else {
            lnc_outw(PCNET32_WIO_RAP, port);
            return lnc_inw(PCNET32_WIO_BDP);
        }
    }

    inline void write_bcr(UInt16 port, UInt16 val)
    {
        if (sc.dwio) {
            lnc_outdw(PCNET32_DWIO_RAP, port);
            lnc_outdw(PCNET32_DWIO_BDP, val);
        }
        else {
            lnc_outw(PCNET32_WIO_RAP, port);
            lnc_outw(PCNET32_WIO_BDP, val);
        }
    }

    inline void pcnet32_reset()
    {
        lnc_indw(PCNET32_DWIO_RESET);
        lnc_inw(PCNET32_WIO_RESET);
    }

    inline void pcnet32_stop()
    {
        write_csr(CSR, CSR_STOP);
    }

    void pcnet32_start( void )
    {
        write_csr(CSR, CSR_STRT | CSR_IENA);
    }

    inline bool pcnet32_check_wio() {
        lnc_outw(PCNET32_WIO_RAP, 88);
        return lnc_inw(PCNET32_WIO_RAP) == 88;
    }

    inline bool pcnet32_check_dwio() {
        lnc_outdw(PCNET32_DWIO_RAP, 88);
        return lnc_indw(PCNET32_DWIO_RAP) == 88;
    }

    // ---- self written helper methods

    inline u_long iokit_kvtop(IOBufferMemoryDescriptor *md, char *start, char *adr) {
        return md->getPhysicalSegment( (IOByteCount) (adr-start), (IOByteCount*) 0L );
    }

	// ---- Utility Methods lnc_*

    mbuf_t mbuf_packet(int start_of_packet, UInt16 pkt_len);

    long mbuf_to_buffer(mbuf_t pkt, char *buffer);

    void lnc_release_resources( void );
    void lnc_reset( void );
    void lnc_watchdog( void );

    void lnc_setladrf( IOEthernetAddress * addrs, UInt32 count );

    void lnc_dump_state( void );

    // ---- Higher Methods lnc_*

    bool lnc_pci_attach( void );
    bool lnc_attach_common( void );

    void lnc_init( void );

    void lnc_intr( bool* flushInputQ, bool* serviceOutputQ);
    void lnc_rint( bool * queued );
    void lnc_tint( bool * reclaimed );

    // ---- self written higher methods
    UInt32 lnc_output_packet(mbuf_t pkt, void * param);
    inline void lnc_down() {
        pcnet32_stop();
        if (sc.if_drv_flags & LNC_IFF_DRV_RUNNING != 0) {
			sc.if_drv_flags &= ~LNC_IFF_DRV_RUNNING;
        }
    }

    bool lnc_set_medium(MediumIndex mediumIndex);
    MediumIndex lnc_get_medium( void );
    bool lnc_is_medium_available( void );

    void lnc_set_promiscuous_mode( bool enable );

    void lnc_set_multicast(IOEthernetAddress * addrs, UInt32 count);
};

#endif /* !_PCNET_H */


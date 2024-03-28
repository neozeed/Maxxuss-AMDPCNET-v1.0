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

#ifndef _PCNET_HW_H
#define _PCNET_HW_H

/*
 * Some basic Ethernet constants.
 */
#define ETHER_ADDR_LEN          6       /* length of an Ethernet address */
#define ETHER_TYPE_LEN          2       /* length of the Ethernet type field */
#define ETHER_CRC_LEN           4       /* length of the Ethernet CRC */
#define ETHER_HDR_LEN           (ETHER_ADDR_LEN*2+ETHER_TYPE_LEN)
#define ETHER_MIN_LEN           64      /* minimum frame len, including CRC */
#define ETHER_MAX_LEN           1544    /* maximum frame len, including CRC */
#define ETHER_MAX_LEN_JUMBO     9018    /* max jumbo frame len, including CRC */
 
#define MULTICAST_FILTER_LEN 8

struct ether_header {
    u_char  ether_dhost[ETHER_ADDR_LEN];
    u_char  ether_shost[ETHER_ADDR_LEN];
    u_short ether_type;
 };

#define PCNET_MULTI_INIT_ADDR 0xff


// PCI IDs

#define PCI_DEVICE_PCNET32      0x2000
#define PCI_VENDOR_AMD          0x1022

// Offsets from base I/O address

#define PCN_IO32_APROM00        0x00	// ethernet station adr
#define PCN_IO32_APROM01        0x04	// ethernet station adr
#define PCN_IO32_APROM02        0x08
#define PCN_IO32_APROM03        0x0C

#define PCNET32_WIO_RDP         0x10 // Register data port
#define PCNET32_WIO_RAP         0x12 // Register address port
#define PCNET32_WIO_RESET       0x14
#define PCNET32_WIO_BDP         0x16

#define PCNET32_DWIO_RDP        0x10
#define PCNET32_DWIO_RAP        0x14
#define PCNET32_DWIO_RESET      0x18
#define PCNET32_DWIO_BDP        0x1C

// Controller Status Registers

#define CSR                     0
#define INIT_BLOCK_ADDRESS_LOW  1
#define INIT_BLOCK_ADDRESS_HIGH 2
#define INTERRUPT_MASK          3
#define FEATURE_CONTROL         4
#define LAF_START               8   // Logical Address Filter (CSR8-CSR11)
#define MODE_CONTROL            15
#define CHIP_ID_LOWER           88
#define CHIP_ID_UPPER           89

// Controller Status Register (CSR0) Bits

#define CSR_ERR                 0x8000
#define CSR_BABL                0x4000
#define CSR_CERR                0x2000
#define CSR_MISS                0x1000
#define CSR_MERR                0x0800
#define CSR_RINT                0x0400
#define CSR_TINT                0x0200
#define CSR_IDON                0x0100
#define CSR_INTR                0x0080
#define CSR_IENA                0x0040
#define CSR_RXON                0x0020
#define CSR_TXON                0x0010
#define CSR_TDMD                0x0008
#define CSR_STOP                0x0004
#define CSR_STRT                0x0002
#define CSR_INIT                0x0001

// Feature Control Register (CSR4)
#define CSR4_APAD_XMT           0x0800
#define CSR4_MFCOM              0x0100
#define CSR4_RCVCCOM            0x0010
#define CSR4_TXSTRTM            0x0004
#define CSR4_JABM               0x0001

// Mode Control (CSR15) -- also Mode in Init Block
#define MODE_PROM              0x8000
#define MODE_NORMAL            0x0080 
   // Portsel 01, meaning T-BASE instead of AUI selected

// Miscellaneous Configuration (BCR2)

#define MISCCFG                 2
#define MISCCFG_TMAULOOP        0x4000
#define MISCCFG_APROMWE         0x0100
#define MISCCFG_INTLEVEL        0x0080
#define MISCCFG_DXCVRCTL        0x0020
#define MISCCFG_DXCVRPOL        0x0010
#define MISCCFG_EADISEL         0x0008
#define MISCCFG_AWAKE           0x0004
#define MISCCFG_ASEL            0x0002
#define MISCCFG_XMAUSEL         0x0001
#define MISCCFG_                0x0000
#define MISCCFG_                0x0000

// Link Status LED (BCR4)
#define LNKST                   4
#define LNKST_LEDOUT            0x8000
#define LNKST_FDLSE             0x0100
#define LNKST_LNKSTE            0x0040

// Full-Duplex Control (BCR9)
#define FDCTL                   9
#define FDCTL_AUIFD             0x0002
#define FDCTL_FDEN              0x0001

// Transmit Descriptor (TMD) Status

#define TMD_OWN                 0x8000
#define TMD_ERR                 0x4000
#define TMD_ADD_FCS             0x2000
    // ADD_FCS and NO_FCS is controlled through the same bit
#define TMD_NO_FCS              0x2000
#define TMD_MORE                0x1000
    // MORE and LTINT is controlled through the same bit
#define TMD_LTINT               0x1000
#define TMD_ONE                 0x0800
#define TMD_DEF                 0x0400
#define TMD_STP                 0x0200
#define TMD_ENP                 0x0100
#define TMD_BPE                 0x0080
#define TMD_RES                 0x007F

/* Transmit status flags for ext_status */

#define TMD_TBUFF	0x8000		/* Buffer error         */
#define TMD_UFLO	0x4000		/* Silo underflow       */
#define TMD_LCOL	0x1000		/* Late collision       */
#define TMD_LCAR	0x0800		/* Loss of carrier      */
#define TMD_RTRY	0x0400		/* Tried 16 times       */
#define TMD_TDR 	0x03FF		/* Time domain reflectometry */

// Receive Descriptor (RMD) Status

#define RMD_OWN                 0x8000
#define RMD_ERR                 0x4000
#define RMD_FRAM                0x2000
#define RMD_OFLO                0x1000
#define RMD_CRC                 0x0800
#define RMD_BUFF                0x0400
#define RMD_STP                 0x0200
#define RMD_ENP                 0x0100
#define RMD_BPE                 0x0080
#define RMD_PAM                 0x0040
#define RMD_LAFM                0x0020
#define RMD_BAM                 0x0010

// Rx and Tx ring descriptors
// for SWSTYLE=1,2 [See PDF page 159]

struct pcnet32_mds
{
  u_long buffer;
  short length;
  u_short status;
  union {
      u_long rx_msg_length;    // for rx
      struct {
          u_short misc;
          u_short ext_status;
      } tx_misc;               // for tx
  } misc;
  u_long reserved;
};
#define MDHEAD_LEN_MASK	0x0FFF

// Initialization block
// for SSIZE32=1 [See PDF page 156]
struct pcnet32_init_block
{
  u_short mode;         // mirrors CSR15
  u_short tlen_rlen;
  u_char phys_addr[6];
  u_short reserved;
  union {               // Logical address filter (multicast)
      u_long ladrl[MULTICAST_FILTER_LEN/4];
      u_char ladrb[MULTICAST_FILTER_LEN];
  } ladr;
  // Receive and transmit ring base, along with extra bits
  u_long rx_ring;
  u_long tx_ring;
};


typedef UInt32 MediumIndex;

enum {
    MEDIUM_INDEX_10_HD = 0,
    MEDIUM_INDEX_10_FD = 1,
    //MEDIUM_INDEX_TX_HD = 2,
    //MEDIUM_INDEX_TX_FD = 3,
    //MEDIUM_INDEX_T4    = 4,
    //MEDIUM_INDEX_AUTO  = 5,
    MEDIUM_INDEX_NONE  = MEDIUM_INDEX_10_FD+1,
    MEDIUM_INDEX_COUNT = MEDIUM_INDEX_NONE,
    MEDIUM_INDEX_DEFAULT = MEDIUM_INDEX_10_FD,
    MEDIUM_INDEX_UNDEF = 42
};

#endif // _PCNET_HW_H


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

#ifndef _PCNET_VAR_H
#define _PCNET_VAR_H

/*
 * Initialize multicast address hashing registers to accept
 * all multicasts (only used when in promiscuous mode)
 */

#define LNC_NTDRE 5
#define LNC_NRDRE 5
#define LNC_RECVBUFSIZE ETHER_MAX_LEN	/* Packet size rounded to dword boundary */
#define LNC_TRANSBUFSIZE ETHER_MAX_LEN

#define LNC_MEM_SLEW 16

/* LNC Flags */
#define LNC_INITIALISED 1
#define LNC_ALLMULTI 2

#endif



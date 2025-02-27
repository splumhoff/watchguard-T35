/*
 * Copyright (c) 1993, 1994, 1995, 1996, 1997
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * savefile.c - supports offline use of tcpdump
 *	Extraction/creation by Jeffrey Mogul, DECWRL
 *	Modified by Steve McCanne, LBL.
 *
 * Used to save the received packet headers, after filtering, to
 * a file, and then read them later.
 * The first record in the file contains saved values for the machine
 * dependent values so we can print the dump file on any architecture.
 */

#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header$ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WIN32
#include <pcap-stdinc.h>
#else /* WIN32 */
#if HAVE_INTTYPES_H
#include <inttypes.h>
#elif HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_SYS_BITYPES_H
#include <sys/bitypes.h>
#endif
#include <sys/types.h>
#endif /* WIN32 */

#include "pcap-int.h"
#include "pcap/usb.h"

#include "pcap-common.h"

/*
 * We don't write DLT_* values to capture files, because they're not the
 * same on all platforms.
 *
 * Unfortunately, the various flavors of BSD have not always used the same
 * numerical values for the same data types, and various patches to
 * libpcap for non-BSD OSes have added their own DLT_* codes for link
 * layer encapsulation types seen on those OSes, and those codes have had,
 * in some cases, values that were also used, on other platforms, for other
 * link layer encapsulation types.
 *
 * This means that capture files of a type whose numerical DLT_* code
 * means different things on different BSDs, or with different versions
 * of libpcap, can't always be read on systems other than those like
 * the one running on the machine on which the capture was made.
 *
 * Instead, we define here a set of LINKTYPE_* codes, and map DLT_* codes
 * to LINKTYPE_* codes when writing a savefile header, and map LINKTYPE_*
 * codes to DLT_* codes when reading a savefile header.
 *
 * For those DLT_* codes that have, as far as we know, the same values on
 * all platforms (DLT_NULL through DLT_FDDI), we define LINKTYPE_xxx as
 * DLT_xxx; that way, captures of those types can still be read by
 * versions of libpcap that map LINKTYPE_* values to DLT_* values, and
 * captures of those types written by versions of libpcap that map DLT_
 * values to LINKTYPE_ values can still be read by older versions
 * of libpcap.
 *
 * The other LINKTYPE_* codes are given values starting at 100, in the
 * hopes that no DLT_* code will be given one of those values.
 *
 * In order to ensure that a given LINKTYPE_* code's value will refer to
 * the same encapsulation type on all platforms, you should not allocate
 * a new LINKTYPE_* value without consulting
 * "tcpdump-workers@lists.tcpdump.org".  The tcpdump developers will
 * allocate a value for you, and will not subsequently allocate it to
 * anybody else; that value will be added to the "pcap.h" in the
 * tcpdump.org Git repository, so that a future libpcap release will
 * include it.
 *
 * You should, if possible, also contribute patches to libpcap and tcpdump
 * to handle the new encapsulation type, so that they can also be checked
 * into the tcpdump.org Git repository and so that they will appear in
 * future libpcap and tcpdump releases.
 *
 * Do *NOT* assume that any values after the largest value in this file
 * are available; you might not have the most up-to-date version of this
 * file, and new values after that one might have been assigned.  Also,
 * do *NOT* use any values below 100 - those might already have been
 * taken by one (or more!) organizations.
 */
#define LINKTYPE_NULL		DLT_NULL
#define LINKTYPE_ETHERNET	DLT_EN10MB	/* also for 100Mb and up */
#define LINKTYPE_EXP_ETHERNET	DLT_EN3MB	/* 3Mb experimental Ethernet */
#define LINKTYPE_AX25		DLT_AX25
#define LINKTYPE_PRONET		DLT_PRONET
#define LINKTYPE_CHAOS		DLT_CHAOS
#define LINKTYPE_TOKEN_RING	DLT_IEEE802	/* DLT_IEEE802 is used for Token Ring */
#define LINKTYPE_ARCNET		DLT_ARCNET	/* BSD-style headers */
#define LINKTYPE_SLIP		DLT_SLIP
#define LINKTYPE_PPP		DLT_PPP
#define LINKTYPE_FDDI		DLT_FDDI

/*
 * LINKTYPE_PPP is for use when there might, or might not, be an RFC 1662
 * PPP in HDLC-like framing header (with 0xff 0x03 before the PPP protocol
 * field) at the beginning of the packet.
 *
 * This is for use when there is always such a header; the address field
 * might be 0xff, for regular PPP, or it might be an address field for Cisco
 * point-to-point with HDLC framing as per section 4.3.1 of RFC 1547 ("Cisco
 * HDLC").  This is, for example, what you get with NetBSD's DLT_PPP_SERIAL.
 *
 * We give it the same value as NetBSD's DLT_PPP_SERIAL, in the hopes that
 * nobody else will choose a DLT_ value of 50, and so that DLT_PPP_SERIAL
 * captures will be written out with a link type that NetBSD's tcpdump
 * can read.
 */
#define LINKTYPE_PPP_HDLC	50		/* PPP in HDLC-like framing */

#define LINKTYPE_PPP_ETHER	51		/* NetBSD PPP-over-Ethernet */

#define LINKTYPE_SYMANTEC_FIREWALL 99		/* Symantec Enterprise Firewall */

#define LINKTYPE_ATM_RFC1483	100		/* LLC/SNAP-encapsulated ATM */
#define LINKTYPE_RAW		101		/* raw IP */
#define LINKTYPE_SLIP_BSDOS	102		/* BSD/OS SLIP BPF header */
#define LINKTYPE_PPP_BSDOS	103		/* BSD/OS PPP BPF header */
#define LINKTYPE_C_HDLC		104		/* Cisco HDLC */
#define LINKTYPE_IEEE802_11	105		/* IEEE 802.11 (wireless) */
#define LINKTYPE_ATM_CLIP	106		/* Linux Classical IP over ATM */
#define LINKTYPE_FRELAY		107		/* Frame Relay */
#define LINKTYPE_LOOP		108		/* OpenBSD loopback */
#define LINKTYPE_ENC		109		/* OpenBSD IPSEC enc */

/*
 * These three types are reserved for future use.
 */
#define LINKTYPE_LANE8023	110		/* ATM LANE + 802.3 */
#define LINKTYPE_HIPPI		111		/* NetBSD HIPPI */
#define LINKTYPE_HDLC		112		/* NetBSD HDLC framing */

#define LINKTYPE_LINUX_SLL	113		/* Linux cooked socket capture */
#define LINKTYPE_LTALK		114		/* Apple LocalTalk hardware */
#define LINKTYPE_ECONET		115		/* Acorn Econet */

/*
 * Reserved for use with OpenBSD ipfilter.
 */
#define LINKTYPE_IPFILTER	116

#define LINKTYPE_PFLOG		117		/* OpenBSD DLT_PFLOG */
#define LINKTYPE_CISCO_IOS	118		/* For Cisco-internal use */
#define LINKTYPE_PRISM_HEADER	119		/* 802.11+Prism II monitor mode */
#define LINKTYPE_AIRONET_HEADER	120		/* FreeBSD Aironet driver stuff */

/*
 * Reserved for Siemens HiPath HDLC.
 */
#define LINKTYPE_HHDLC		121

#define LINKTYPE_IP_OVER_FC	122		/* RFC 2625 IP-over-Fibre Channel */
#define LINKTYPE_SUNATM		123		/* Solaris+SunATM */

/*
 * Reserved as per request from Kent Dahlgren <kent@praesum.com>
 * for private use.
 */
#define LINKTYPE_RIO		124		/* RapidIO */
#define LINKTYPE_PCI_EXP	125		/* PCI Express */
#define LINKTYPE_AURORA		126		/* Xilinx Aurora link layer */

#define LINKTYPE_IEEE802_11_RADIO 127		/* 802.11 plus BSD radio header */

/*
 * Reserved for the TZSP encapsulation, as per request from
 * Chris Waters <chris.waters@networkchemistry.com>
 * TZSP is a generic encapsulation for any other link type,
 * which includes a means to include meta-information
 * with the packet, e.g. signal strength and channel
 * for 802.11 packets.
 */
#define LINKTYPE_TZSP		128		/* Tazmen Sniffer Protocol */

#define LINKTYPE_ARCNET_LINUX	129		/* Linux-style headers */

/*
 * Juniper-private data link types, as per request from
 * Hannes Gredler <hannes@juniper.net>.  The corresponding
 * DLT_s are used for passing on chassis-internal
 * metainformation such as QOS profiles, etc..
 */
#define LINKTYPE_JUNIPER_MLPPP  130
#define LINKTYPE_JUNIPER_MLFR   131
#define LINKTYPE_JUNIPER_ES     132
#define LINKTYPE_JUNIPER_GGSN   133
#define LINKTYPE_JUNIPER_MFR    134
#define LINKTYPE_JUNIPER_ATM2   135
#define LINKTYPE_JUNIPER_SERVICES 136
#define LINKTYPE_JUNIPER_ATM1   137

#define LINKTYPE_APPLE_IP_OVER_IEEE1394 138	/* Apple IP-over-IEEE 1394 cooked header */

#define LINKTYPE_MTP2_WITH_PHDR	139
#define LINKTYPE_MTP2		140
#define LINKTYPE_MTP3		141
#define LINKTYPE_SCCP		142

#define LINKTYPE_DOCSIS		143		/* DOCSIS MAC frames */

#define LINKTYPE_LINUX_IRDA	144		/* Linux-IrDA */

/*
 * Reserved for IBM SP switch and IBM Next Federation switch.
 */
#define LINKTYPE_IBM_SP		145
#define LINKTYPE_IBM_SN		146

/*
 * Reserved for private use.  If you have some link-layer header type
 * that you want to use within your organization, with the capture files
 * using that link-layer header type not ever be sent outside your
 * organization, you can use these values.
 *
 * No libpcap release will use these for any purpose, nor will any
 * tcpdump release use them, either.
 *
 * Do *NOT* use these in capture files that you expect anybody not using
 * your private versions of capture-file-reading tools to read; in
 * particular, do *NOT* use them in products, otherwise you may find that
 * people won't be able to use tcpdump, or snort, or Ethereal, or... to
 * read capture files from your firewall/intrusion detection/traffic
 * monitoring/etc. appliance, or whatever product uses that LINKTYPE_ value,
 * and you may also find that the developers of those applications will
 * not accept patches to let them read those files.
 *
 * Also, do not use them if somebody might send you a capture using them
 * for *their* private type and tools using them for *your* private type
 * would have to read them.
 *
 * Instead, in those cases, ask "tcpdump-workers@lists.tcpdump.org" for a
 * new DLT_ and LINKTYPE_ value, as per the comment in pcap/bpf.h, and use
 * the type you're given.
 */
#define LINKTYPE_USER0		147
#define LINKTYPE_USER1		148
#define LINKTYPE_USER2		149
#define LINKTYPE_USER3		150
#define LINKTYPE_USER4		151
#define LINKTYPE_USER5		152
#define LINKTYPE_USER6		153
#define LINKTYPE_USER7		154
#define LINKTYPE_USER8		155
#define LINKTYPE_USER9		156
#define LINKTYPE_USER10		157
#define LINKTYPE_USER11		158
#define LINKTYPE_USER12		159
#define LINKTYPE_USER13		160
#define LINKTYPE_USER14		161
#define LINKTYPE_USER15		162

/*
 * For future use with 802.11 captures - defined by AbsoluteValue
 * Systems to store a number of bits of link-layer information
 * including radio information:
 *
 *	http://www.shaftnet.org/~pizza/software/capturefrm.txt
 *
 * but could and arguably should also be used by non-AVS Linux
 * 802.11 drivers; that may happen in the future.
 */
#define LINKTYPE_IEEE802_11_RADIO_AVS 163	/* 802.11 plus AVS radio header */

/*
 * Juniper-private data link type, as per request from
 * Hannes Gredler <hannes@juniper.net>.  The corresponding
 * DLT_s are used for passing on chassis-internal
 * metainformation such as QOS profiles, etc..
 */
#define LINKTYPE_JUNIPER_MONITOR 164

/*
 * Reserved for BACnet MS/TP.
 */
#define LINKTYPE_BACNET_MS_TP	165

/*
 * Another PPP variant as per request from Karsten Keil <kkeil@suse.de>.
 *
 * This is used in some OSes to allow a kernel socket filter to distinguish
 * between incoming and outgoing packets, on a socket intended to
 * supply pppd with outgoing packets so it can do dial-on-demand and
 * hangup-on-lack-of-demand; incoming packets are filtered out so they
 * don't cause pppd to hold the connection up (you don't want random
 * input packets such as port scans, packets from old lost connections,
 * etc. to force the connection to stay up).
 *
 * The first byte of the PPP header (0xff03) is modified to accomodate
 * the direction - 0x00 = IN, 0x01 = OUT.
 */
#define LINKTYPE_PPP_PPPD	166

/*
 * Juniper-private data link type, as per request from
 * Hannes Gredler <hannes@juniper.net>.  The DLT_s are used
 * for passing on chassis-internal metainformation such as
 * QOS profiles, cookies, etc..
 */
#define LINKTYPE_JUNIPER_PPPOE     167
#define LINKTYPE_JUNIPER_PPPOE_ATM 168

#define LINKTYPE_GPRS_LLC	169		/* GPRS LLC */
#define LINKTYPE_GPF_T		170		/* GPF-T (ITU-T G.7041/Y.1303) */
#define LINKTYPE_GPF_F		171		/* GPF-T (ITU-T G.7041/Y.1303) */

/*
 * Requested by Oolan Zimmer <oz@gcom.com> for use in Gcom's T1/E1 line
 * monitoring equipment.
 */
#define LINKTYPE_GCOM_T1E1	172
#define LINKTYPE_GCOM_SERIAL	173

/*
 * Juniper-private data link type, as per request from
 * Hannes Gredler <hannes@juniper.net>.  The DLT_ is used
 * for internal communication to Physical Interface Cards (PIC)
 */
#define LINKTYPE_JUNIPER_PIC_PEER    174

/*
 * Link types requested by Gregor Maier <gregor@endace.com> of Endace
 * Measurement Systems.  They add an ERF header (see
 * http://www.endace.com/support/EndaceRecordFormat.pdf) in front of
 * the link-layer header.
 */
#define LINKTYPE_ERF_ETH	175	/* Ethernet */
#define LINKTYPE_ERF_POS	176	/* Packet-over-SONET */

/*
 * Requested by Daniele Orlandi <daniele@orlandi.com> for raw LAPD
 * for vISDN (http://www.orlandi.com/visdn/).  Its link-layer header
 * includes additional information before the LAPD header, so it's
 * not necessarily a generic LAPD header.
 */
#define LINKTYPE_LINUX_LAPD	177

/*
 * Juniper-private data link type, as per request from
 * Hannes Gredler <hannes@juniper.net>. 
 * The Link Types are used for prepending meta-information
 * like interface index, interface name
 * before standard Ethernet, PPP, Frelay & C-HDLC Frames
 */
#define LINKTYPE_JUNIPER_ETHER  178
#define LINKTYPE_JUNIPER_PPP    179
#define LINKTYPE_JUNIPER_FRELAY 180
#define LINKTYPE_JUNIPER_CHDLC  181

/*
 * Multi Link Frame Relay (FRF.16)
 */
#define LINKTYPE_MFR            182

/*
 * Juniper-private data link type, as per request from
 * Hannes Gredler <hannes@juniper.net>. 
 * The DLT_ is used for internal communication with a
 * voice Adapter Card (PIC)
 */
#define LINKTYPE_JUNIPER_VP     183

/*
 * Arinc 429 frames.
 * DLT_ requested by Gianluca Varenni <gianluca.varenni@cacetech.com>.
 * Every frame contains a 32bit A429 label.
 * More documentation on Arinc 429 can be found at
 * http://www.condoreng.com/support/downloads/tutorials/ARINCTutorial.pdf
 */
#define LINKTYPE_A429           184

/*
 * Arinc 653 Interpartition Communication messages.
 * DLT_ requested by Gianluca Varenni <gianluca.varenni@cacetech.com>.
 * Please refer to the A653-1 standard for more information.
 */
#define LINKTYPE_A653_ICM       185

/*
 * USB packets, beginning with a USB setup header; requested by
 * Paolo Abeni <paolo.abeni@email.it>.
 */
#define LINKTYPE_USB		186

/*
 * Bluetooth HCI UART transport layer (part H:4); requested by
 * Paolo Abeni.
 */
#define LINKTYPE_BLUETOOTH_HCI_H4	187

/*
 * IEEE 802.16 MAC Common Part Sublayer; requested by Maria Cruz
 * <cruz_petagay@bah.com>.
 */
#define LINKTYPE_IEEE802_16_MAC_CPS	188

/*
 * USB packets, beginning with a Linux USB header; requested by
 * Paolo Abeni <paolo.abeni@email.it>.
 */
#define LINKTYPE_USB_LINUX		189

/*
 * Controller Area Network (CAN) v. 2.0B packets.
 * DLT_ requested by Gianluca Varenni <gianluca.varenni@cacetech.com>.
 * Used to dump CAN packets coming from a CAN Vector board.
 * More documentation on the CAN v2.0B frames can be found at
 * http://www.can-cia.org/downloads/?269
 */
#define LINKTYPE_CAN20B         190

/*
 * IEEE 802.15.4, with address fields padded, as is done by Linux
 * drivers; requested by Juergen Schimmer.
 */
#define LINKTYPE_IEEE802_15_4_LINUX	191

/*
 * Per Packet Information encapsulated packets.
 * LINKTYPE_ requested by Gianluca Varenni <gianluca.varenni@cacetech.com>.
 */
#define LINKTYPE_PPI			192

/*
 * Header for 802.16 MAC Common Part Sublayer plus a radiotap radio header;
 * requested by Charles Clancy.
 */
#define LINKTYPE_IEEE802_16_MAC_CPS_RADIO	193

/*
 * Juniper-private data link type, as per request from
 * Hannes Gredler <hannes@juniper.net>. 
 * The DLT_ is used for internal communication with a
 * integrated service module (ISM).
 */
#define LINKTYPE_JUNIPER_ISM    194

/*
 * IEEE 802.15.4, exactly as it appears in the spec (no padding, no
 * nothing); requested by Mikko Saarnivala <mikko.saarnivala@sensinode.com>.
 */
#define LINKTYPE_IEEE802_15_4	195

/*
 * Various link-layer types, with a pseudo-header, for SITA
 * (http://www.sita.aero/); requested by Fulko Hew (fulko.hew@gmail.com).
 */
#define LINKTYPE_SITA		196

/*
 * Various link-layer types, with a pseudo-header, for Endace DAG cards;
 * encapsulates Endace ERF records.  Requested by Stephen Donnelly
 * <stephen@endace.com>.
 */
#define LINKTYPE_ERF		197

/*
 * Special header prepended to Ethernet packets when capturing from a
 * u10 Networks board.  Requested by Phil Mulholland
 * <phil@u10networks.com>.
 */
#define LINKTYPE_RAIF1		198

/*
 * IPMB packet for IPMI, beginning with the I2C slave address, followed
 * by the netFn and LUN, etc..  Requested by Chanthy Toeung
 * <chanthy.toeung@ca.kontron.com>.
 */
#define LINKTYPE_IPMB		199

/*
 * Juniper-private data link type, as per request from
 * Hannes Gredler <hannes@juniper.net>. 
 * The DLT_ is used for capturing data on a secure tunnel interface.
 */
#define LINKTYPE_JUNIPER_ST     200

/*
 * Bluetooth HCI UART transport layer (part H:4), with pseudo-header
 * that includes direction information; requested by Paolo Abeni.
 */
#define LINKTYPE_BLUETOOTH_HCI_H4_WITH_PHDR	201

/*
 * AX.25 packet with a 1-byte KISS header; see
 *
 *	http://www.ax25.net/kiss.htm
 *
 * as per Richard Stearn <richard@rns-stearn.demon.co.uk>.
 */
#define LINKTYPE_AX25_KISS	202

/*
 * LAPD packets from an ISDN channel, starting with the address field,
 * with no pseudo-header.
 * Requested by Varuna De Silva <varunax@gmail.com>.
 */
#define LINKTYPE_LAPD		203

/*
 * Variants of various link-layer headers, with a one-byte direction
 * pseudo-header prepended - zero means "received by this host",
 * non-zero (any non-zero value) means "sent by this host" - as per
 * Will Barker <w.barker@zen.co.uk>.
 */
#define LINKTYPE_PPP_WITH_DIR	204	/* PPP */
#define LINKTYPE_C_HDLC_WITH_DIR 205	/* Cisco HDLC */
#define LINKTYPE_FRELAY_WITH_DIR 206	/* Frame Relay */
#define LINKTYPE_LAPB_WITH_DIR	207	/* LAPB */

/*
 * 208 is reserved for an as-yet-unspecified proprietary link-layer
 * type, as requested by Will Barker.
 */

/*
 * IPMB with a Linux-specific pseudo-header; as requested by Alexey Neyman
 * <avn@pigeonpoint.com>.
 */
#define LINKTYPE_IPMB_LINUX		209

/*
 * FlexRay automotive bus - http://www.flexray.com/ - as requested
 * by Hannes Kaelber <hannes.kaelber@x2e.de>.
 */
#define LINKTYPE_FLEXRAY		210

/*
 * Media Oriented Systems Transport (MOST) bus for multimedia
 * transport - http://www.mostcooperation.com/ - as requested
 * by Hannes Kaelber <hannes.kaelber@x2e.de>.
 */
#define LINKTYPE_MOST			211

/*
 * Local Interconnect Network (LIN) bus for vehicle networks -
 * http://www.lin-subbus.org/ - as requested by Hannes Kaelber
 * <hannes.kaelber@x2e.de>.
 */
#define LINKTYPE_LIN			212

/*
 * X2E-private data link type used for serial line capture,
 * as requested by Hannes Kaelber <hannes.kaelber@x2e.de>.
 */
#define LINKTYPE_X2E_SERIAL		213

/*
 * X2E-private data link type used for the Xoraya data logger
 * family, as requested by Hannes Kaelber <hannes.kaelber@x2e.de>.
 */
#define LINKTYPE_X2E_XORAYA		214

/*
 * IEEE 802.15.4, exactly as it appears in the spec (no padding, no
 * nothing), but with the PHY-level data for non-ASK PHYs (4 octets
 * of 0 as preamble, one octet of SFD, one octet of frame length+
 * reserved bit, and then the MAC-layer data, starting with the
 * frame control field).
 *
 * Requested by Max Filippov <jcmvbkbc@gmail.com>.
 */
#define LINKTYPE_IEEE802_15_4_NONASK_PHY	215

/* 
 * David Gibson <david@gibson.dropbear.id.au> requested this for
 * captures from the Linux kernel /dev/input/eventN devices. This
 * is used to communicate keystrokes and mouse movements from the
 * Linux kernel to display systems, such as Xorg. 
 */
#define LINKTYPE_LINUX_EVDEV			216

/*
 * GSM Um and Abis interfaces, preceded by a "gsmtap" header.
 *
 * Requested by Harald Welte <laforge@gnumonks.org>.
 */
#define LINKTYPE_GSMTAP_UM			217
#define LINKTYPE_GSMTAP_ABIS			218

/*
 * MPLS, with an MPLS label as the link-layer header.
 * Requested by Michele Marchetto <michele@openbsd.org> on behalf
 * of OpenBSD.
 */
#define LINKTYPE_MPLS				219

/*
 * USB packets, beginning with a Linux USB header, with the USB header
 * padded to 64 bytes; required for memory-mapped access.
 */
#define LINKTYPE_USB_LINUX_MMAPPED		220

/*
 * DECT packets, with a pseudo-header; requested by
 * Matthias Wenzel <tcpdump@mazzoo.de>.
 */
#define LINKTYPE_DECT				221

/*
 * From: "Lidwa, Eric (GSFC-582.0)[SGT INC]" <eric.lidwa-1@nasa.gov>
 * Date: Mon, 11 May 2009 11:18:30 -0500
 *
 * DLT_AOS. We need it for AOS Space Data Link Protocol.
 *   I have already written dissectors for but need an OK from
 *   legal before I can submit a patch.
 *
 */
#define LINKTYPE_AOS				222

/*
 * Wireless HART (Highway Addressable Remote Transducer)
 * From the HART Communication Foundation
 * IES/PAS 62591
 *
 * Requested by Sam Roberts <vieuxtech@gmail.com>.
 */
#define LINKTYPE_WIHART				223

/*
 * Fibre Channel FC-2 frames, beginning with a Frame_Header.
 * Requested by Kahou Lei <kahou82@gmail.com>.
 */
#define LINKTYPE_FC_2				224

/*
 * Fibre Channel FC-2 frames, beginning with an encoding of the
 * SOF, and ending with an encoding of the EOF.
 *
 * The encodings represent the frame delimiters as 4-byte sequences
 * representing the corresponding ordered sets, with K28.5
 * represented as 0xBC, and the D symbols as the corresponding
 * byte values; for example, SOFi2, which is K28.5 - D21.5 - D1.2 - D21.2,
 * is represented as 0xBC 0xB5 0x55 0x55.
 *
 * Requested by Kahou Lei <kahou82@gmail.com>.
 */
#define LINKTYPE_FC_2_WITH_FRAME_DELIMS		225

/*
 * Solaris ipnet pseudo-header; requested by Darren Reed <Darren.Reed@Sun.COM>.
 *
 * The pseudo-header starts with a one-byte version number; for version 2,
 * the pseudo-header is:
 *
 * struct dl_ipnetinfo {
 *     u_int8_t   dli_version;
 *     u_int8_t   dli_family;
 *     u_int16_t  dli_htype;
 *     u_int32_t  dli_pktlen;
 *     u_int32_t  dli_ifindex;
 *     u_int32_t  dli_grifindex;
 *     u_int32_t  dli_zsrc;
 *     u_int32_t  dli_zdst;
 * };
 *
 * dli_version is 2 for the current version of the pseudo-header.
 *
 * dli_family is a Solaris address family value, so it's 2 for IPv4
 * and 26 for IPv6.
 *
 * dli_htype is a "hook type" - 0 for incoming packets, 1 for outgoing
 * packets, and 2 for packets arriving from another zone on the same
 * machine.
 *
 * dli_pktlen is the length of the packet data following the pseudo-header
 * (so the captured length minus dli_pktlen is the length of the
 * pseudo-header, assuming the entire pseudo-header was captured).
 *
 * dli_ifindex is the interface index of the interface on which the
 * packet arrived.
 *
 * dli_grifindex is the group interface index number (for IPMP interfaces).
 *
 * dli_zsrc is the zone identifier for the source of the packet.
 *
 * dli_zdst is the zone identifier for the destination of the packet.
 *
 * A zone number of 0 is the global zone; a zone number of 0xffffffff
 * means that the packet arrived from another host on the network, not
 * from another zone on the same machine.
 *
 * An IPv4 or IPv6 datagram follows the pseudo-header; dli_family indicates
 * which of those it is.
 */
#define LINKTYPE_IPNET				226

/*
 * CAN (Controller Area Network) frames, with a pseudo-header as supplied
 * by Linux SocketCAN.  See Documentation/networking/can.txt in the Linux
 * source.
 *
 * Requested by Felix Obenhuber <felix@obenhuber.de>.
 */
#define LINKTYPE_CAN_SOCKETCAN			227

/*
 * Raw IPv4/IPv6; different from DLT_RAW in that the DLT_ value specifies
 * whether it's v4 or v6.  Requested by Darren Reed <Darren.Reed@Sun.COM>.
 */
#define LINKTYPE_IPV4				228
#define LINKTYPE_IPV6				229


static struct linktype_map {
	int	dlt;
	int	linktype;
} map[] = {
	/*
	 * These DLT_* codes have LINKTYPE_* codes with values identical
	 * to the values of the corresponding DLT_* code.
	 */
	{ DLT_NULL,		LINKTYPE_NULL },
	{ DLT_EN10MB,		LINKTYPE_ETHERNET },
	{ DLT_EN3MB,		LINKTYPE_EXP_ETHERNET },
	{ DLT_AX25,		LINKTYPE_AX25 },
	{ DLT_PRONET,		LINKTYPE_PRONET },
	{ DLT_CHAOS,		LINKTYPE_CHAOS },
	{ DLT_IEEE802,		LINKTYPE_TOKEN_RING },
	{ DLT_ARCNET,		LINKTYPE_ARCNET },
	{ DLT_SLIP,		LINKTYPE_SLIP },
	{ DLT_PPP,		LINKTYPE_PPP },
	{ DLT_FDDI,	 	LINKTYPE_FDDI },

	/*
	 * These DLT_* codes have different values on different
	 * platforms; we map them to LINKTYPE_* codes that
	 * have values that should never be equal to any DLT_*
	 * code.
	 */
#ifdef DLT_FR
	/* BSD/OS Frame Relay */
	{ DLT_FR,		LINKTYPE_FRELAY },
#endif

	{ DLT_SYMANTEC_FIREWALL, LINKTYPE_SYMANTEC_FIREWALL },
	{ DLT_ATM_RFC1483, 	LINKTYPE_ATM_RFC1483 },
	{ DLT_RAW,		LINKTYPE_RAW },
	{ DLT_SLIP_BSDOS,	LINKTYPE_SLIP_BSDOS },
	{ DLT_PPP_BSDOS,	LINKTYPE_PPP_BSDOS },

	/* BSD/OS Cisco HDLC */
	{ DLT_C_HDLC,		LINKTYPE_C_HDLC },

	/*
	 * These DLT_* codes are not on all platforms, but, so far,
	 * there don't appear to be any platforms that define
	 * other codes with those values; we map them to
	 * different LINKTYPE_* values anyway, just in case.
	 */

	/* Linux ATM Classical IP */
	{ DLT_ATM_CLIP,		LINKTYPE_ATM_CLIP },

	/* NetBSD sync/async serial PPP (or Cisco HDLC) */
	{ DLT_PPP_SERIAL,	LINKTYPE_PPP_HDLC },

	/* NetBSD PPP over Ethernet */
	{ DLT_PPP_ETHER,	LINKTYPE_PPP_ETHER },

	/* IEEE 802.11 wireless */
	{ DLT_IEEE802_11,	LINKTYPE_IEEE802_11 },

	/* Frame Relay */
	{ DLT_FRELAY,		LINKTYPE_FRELAY },

	/* OpenBSD loopback */
	{ DLT_LOOP,		LINKTYPE_LOOP },

	/* OpenBSD IPSEC enc */
	{ DLT_ENC,		LINKTYPE_ENC },

	/* Linux cooked socket capture */
	{ DLT_LINUX_SLL,	LINKTYPE_LINUX_SLL },

	/* Apple LocalTalk hardware */
	{ DLT_LTALK,		LINKTYPE_LTALK },

	/* Acorn Econet */
	{ DLT_ECONET,		LINKTYPE_ECONET },

	/* OpenBSD DLT_PFLOG */
	{ DLT_PFLOG,		LINKTYPE_PFLOG },

	/* For Cisco-internal use */
	{ DLT_CISCO_IOS,	LINKTYPE_CISCO_IOS },

	/* Prism II monitor-mode header plus 802.11 header */
	{ DLT_PRISM_HEADER,	LINKTYPE_PRISM_HEADER },

	/* FreeBSD Aironet driver stuff */
	{ DLT_AIRONET_HEADER,	LINKTYPE_AIRONET_HEADER },

	/* Siemens HiPath HDLC */
	{ DLT_HHDLC,		LINKTYPE_HHDLC },

	/* RFC 2625 IP-over-Fibre Channel */
	{ DLT_IP_OVER_FC,	LINKTYPE_IP_OVER_FC },

	/* Solaris+SunATM */
	{ DLT_SUNATM,		LINKTYPE_SUNATM },

	/* RapidIO */
	{ DLT_RIO,		LINKTYPE_RIO },

	/* PCI Express */
	{ DLT_PCI_EXP,		LINKTYPE_PCI_EXP },

	/* Xilinx Aurora link layer */
	{ DLT_AURORA,		LINKTYPE_AURORA },

	/* 802.11 plus BSD radio header */
	{ DLT_IEEE802_11_RADIO,	LINKTYPE_IEEE802_11_RADIO },

	/* Tazmen Sniffer Protocol */
	{ DLT_TZSP,		LINKTYPE_TZSP },

	/* Arcnet with Linux-style link-layer headers */
	{ DLT_ARCNET_LINUX,	LINKTYPE_ARCNET_LINUX },

        /* Juniper-internal chassis encapsulation */
        { DLT_JUNIPER_MLPPP,    LINKTYPE_JUNIPER_MLPPP },
        { DLT_JUNIPER_MLFR,     LINKTYPE_JUNIPER_MLFR },
        { DLT_JUNIPER_ES,       LINKTYPE_JUNIPER_ES },
        { DLT_JUNIPER_GGSN,     LINKTYPE_JUNIPER_GGSN },
        { DLT_JUNIPER_MFR,      LINKTYPE_JUNIPER_MFR },
        { DLT_JUNIPER_ATM2,     LINKTYPE_JUNIPER_ATM2 },
        { DLT_JUNIPER_SERVICES, LINKTYPE_JUNIPER_SERVICES },
        { DLT_JUNIPER_ATM1,     LINKTYPE_JUNIPER_ATM1 },

	/* Apple IP-over-IEEE 1394 cooked header */
	{ DLT_APPLE_IP_OVER_IEEE1394, LINKTYPE_APPLE_IP_OVER_IEEE1394 },

	/* SS7 */
	{ DLT_MTP2_WITH_PHDR,	LINKTYPE_MTP2_WITH_PHDR },
	{ DLT_MTP2,		LINKTYPE_MTP2 },
	{ DLT_MTP3,		LINKTYPE_MTP3 },
	{ DLT_SCCP,		LINKTYPE_SCCP },

	/* DOCSIS MAC frames */
	{ DLT_DOCSIS,		LINKTYPE_DOCSIS },

	/* IrDA IrLAP packets + Linux-cooked header */
	{ DLT_LINUX_IRDA,	LINKTYPE_LINUX_IRDA },

	/* IBM SP and Next Federation switches */
	{ DLT_IBM_SP,		LINKTYPE_IBM_SP },
	{ DLT_IBM_SN,		LINKTYPE_IBM_SN },

	/* 802.11 plus AVS radio header */
	{ DLT_IEEE802_11_RADIO_AVS, LINKTYPE_IEEE802_11_RADIO_AVS },

	/*
	 * Any platform that defines additional DLT_* codes should:
	 *
	 *	request a LINKTYPE_* code and value from tcpdump.org,
	 *	as per the above;
	 *
	 *	add, in their version of libpcap, an entry to map
	 *	those DLT_* codes to the corresponding LINKTYPE_*
	 *	code;
	 *
	 *	redefine, in their "net/bpf.h", any DLT_* values
	 *	that collide with the values used by their additional
	 *	DLT_* codes, to remove those collisions (but without
	 *	making them collide with any of the LINKTYPE_*
	 *	values equal to 50 or above; they should also avoid
	 *	defining DLT_* values that collide with those
	 *	LINKTYPE_* values, either).
	 */

	/* Juniper-internal chassis encapsulation */
	{ DLT_JUNIPER_MONITOR,	LINKTYPE_JUNIPER_MONITOR },

	/* BACnet MS/TP */
	{ DLT_BACNET_MS_TP,	LINKTYPE_BACNET_MS_TP },

	/* PPP for pppd, with direction flag in the PPP header */
	{ DLT_PPP_PPPD,		LINKTYPE_PPP_PPPD},

	/* Juniper-internal chassis encapsulation */
        { DLT_JUNIPER_PPPOE,    LINKTYPE_JUNIPER_PPPOE },
        { DLT_JUNIPER_PPPOE_ATM,LINKTYPE_JUNIPER_PPPOE_ATM },

	/* GPRS LLC */
	{ DLT_GPRS_LLC,		LINKTYPE_GPRS_LLC },

	/* Transparent Generic Framing Procedure (ITU-T G.7041/Y.1303) */
	{ DLT_GPF_T,		LINKTYPE_GPF_T },

	/* Framed Generic Framing Procedure (ITU-T G.7041/Y.1303) */
	{ DLT_GPF_F,		LINKTYPE_GPF_F },

	{ DLT_GCOM_T1E1,	LINKTYPE_GCOM_T1E1 },
	{ DLT_GCOM_SERIAL,	LINKTYPE_GCOM_SERIAL },

        /* Juniper-internal chassis encapsulation */
        { DLT_JUNIPER_PIC_PEER, LINKTYPE_JUNIPER_PIC_PEER },

	/* Endace types */
	{ DLT_ERF_ETH,		LINKTYPE_ERF_ETH },
	{ DLT_ERF_POS,		LINKTYPE_ERF_POS },

	/* viSDN LAPD */
	{ DLT_LINUX_LAPD,	LINKTYPE_LINUX_LAPD },

        /* Juniper meta-information before Ether, PPP, Frame Relay, C-HDLC Frames */
        { DLT_JUNIPER_ETHER, LINKTYPE_JUNIPER_ETHER },
        { DLT_JUNIPER_PPP, LINKTYPE_JUNIPER_PPP },
        { DLT_JUNIPER_FRELAY, LINKTYPE_JUNIPER_FRELAY },
        { DLT_JUNIPER_CHDLC, LINKTYPE_JUNIPER_CHDLC },

        /* Multi Link Frame Relay (FRF.16) */
        { DLT_MFR,              LINKTYPE_MFR },

        /* Juniper Voice PIC */
        { DLT_JUNIPER_VP,       LINKTYPE_JUNIPER_VP },

	/* Controller Area Network (CAN) v2.0B */
	{ DLT_A429,		LINKTYPE_A429 },

	/* Arinc 653 Interpartition Communication messages */
	{ DLT_A653_ICM,         LINKTYPE_A653_ICM },

	/* USB */
	{ DLT_USB,		LINKTYPE_USB },

	/* Bluetooth HCI UART transport layer */
	{ DLT_BLUETOOTH_HCI_H4,	LINKTYPE_BLUETOOTH_HCI_H4 },

	/* IEEE 802.16 MAC Common Part Sublayer */
	{ DLT_IEEE802_16_MAC_CPS,	LINKTYPE_IEEE802_16_MAC_CPS },

	/* USB with Linux header */
	{ DLT_USB_LINUX,	LINKTYPE_USB_LINUX },

	/* Controller Area Network (CAN) v2.0B */
	{ DLT_CAN20B,		LINKTYPE_CAN20B },

	/* IEEE 802.15.4 with address fields padded */
	{ DLT_IEEE802_15_4_LINUX,	LINKTYPE_IEEE802_15_4_LINUX },

	/* Per Packet Information encapsulated packets */
	{ DLT_PPI,			LINKTYPE_PPI },

	/* IEEE 802.16 MAC Common Part Sublayer plus radiotap header */
	{ DLT_IEEE802_16_MAC_CPS_RADIO, LINKTYPE_IEEE802_16_MAC_CPS_RADIO },

        /* Juniper Voice ISM */
        { DLT_JUNIPER_ISM,      LINKTYPE_JUNIPER_ISM },

	/* IEEE 802.15.4 exactly as it appears in the spec */
        { DLT_IEEE802_15_4,	LINKTYPE_IEEE802_15_4 },

	/* Various link-layer types for SITA */
	{ DLT_SITA,		LINKTYPE_SITA },

	/* Various link-layer types for Endace */
	{ DLT_ERF,		LINKTYPE_ERF },

	/* Special header for u10 Networks boards */
	{ DLT_RAIF1,		LINKTYPE_RAIF1 },

	/* IPMB */
	{ DLT_IPMB,		LINKTYPE_IPMB },

        /* Juniper Secure Tunnel */
        { DLT_JUNIPER_ST,       LINKTYPE_JUNIPER_ST },

	/* Bluetooth HCI UART transport layer, with pseudo-header */
	{ DLT_BLUETOOTH_HCI_H4_WITH_PHDR, LINKTYPE_BLUETOOTH_HCI_H4_WITH_PHDR },

	/* AX.25 with KISS header */
	{ DLT_AX25_KISS,	LINKTYPE_AX25_KISS },

	/* Raw LAPD, with no pseudo-header */
	{ DLT_LAPD,		LINKTYPE_LAPD },

	/* PPP with one-byte pseudo-header giving direction */
	{ DLT_PPP_WITH_DIR,	LINKTYPE_PPP_WITH_DIR },

	/* Cisco HDLC with one-byte pseudo-header giving direction */
	{ DLT_C_HDLC_WITH_DIR,	LINKTYPE_C_HDLC_WITH_DIR },

	/* Frame Relay with one-byte pseudo-header giving direction */
	{ DLT_FRELAY_WITH_DIR,	LINKTYPE_FRELAY_WITH_DIR },

	/* LAPB with one-byte pseudo-header giving direction */
	{ DLT_LAPB_WITH_DIR,	LINKTYPE_LAPB_WITH_DIR },

	/* IPMB with Linux pseudo-header */
	{ DLT_IPMB_LINUX,	LINKTYPE_IPMB_LINUX },

	/* FlexRay */
	{ DLT_FLEXRAY,		LINKTYPE_FLEXRAY },

	/* MOST */
	{ DLT_MOST,		LINKTYPE_MOST },

	/* LIN */
	{ DLT_LIN,		LINKTYPE_LIN },

	/* X2E-private serial line capture */
	{ DLT_X2E_SERIAL,	LINKTYPE_X2E_SERIAL },

	/* X2E-private for Xoraya data logger family */
	{ DLT_X2E_XORAYA,	LINKTYPE_X2E_XORAYA },

	/* IEEE 802.15.4 with PHY data for non-ASK PHYs */
	{ DLT_IEEE802_15_4_NONASK_PHY, LINKTYPE_IEEE802_15_4_NONASK_PHY },

	/* Input device events from Linux /dev/input/eventN devices */
	{ DLT_LINUX_EVDEV,	LINKTYPE_LINUX_EVDEV },

	/* GSM types */
	{ DLT_GSMTAP_UM,	LINKTYPE_GSMTAP_UM },
	{ DLT_GSMTAP_ABIS,	LINKTYPE_GSMTAP_ABIS },

	/* MPLS, with an MPLS label as the link-layer header */
	{ DLT_MPLS,		LINKTYPE_MPLS },

	/* USB with padded Linux header */
	{ DLT_USB_LINUX_MMAPPED, LINKTYPE_USB_LINUX_MMAPPED },

	/* DECT packets with a pseudo-header */
	{ DLT_DECT,		LINKTYPE_DECT },

	/* AOS Space Data Link Protocol */
	{ DLT_AOS,		LINKTYPE_AOS },

	/* Wireless HART */
	{ DLT_WIHART,		LINKTYPE_WIHART },

	/* Fibre Channel FC-2 frames without SOF or EOF */
	{ DLT_FC_2,		LINKTYPE_FC_2 },

	/* Fibre Channel FC-2 frames with SOF and EOF */
	{ DLT_FC_2_WITH_FRAME_DELIMS, LINKTYPE_FC_2_WITH_FRAME_DELIMS },

	/* Solaris IPNET */
	{ DLT_IPNET,		LINKTYPE_IPNET },

	/* CAN frames with SocketCAN headers */
	{ DLT_CAN_SOCKETCAN,	LINKTYPE_CAN_SOCKETCAN },

	/* Raw IPv4/IPv6 */
	{ DLT_IPV4,		LINKTYPE_IPV4 },
	{ DLT_IPV6,		LINKTYPE_IPV6 },

	{ -1,			-1 }
};

int
dlt_to_linktype(int dlt)
{
	int i;

	for (i = 0; map[i].dlt != -1; i++) {
		if (map[i].dlt == dlt)
			return (map[i].linktype);
	}

	/*
	 * If we don't have a mapping for this DLT_ code, return an
	 * error; that means that the table above needs to have an
	 * entry added.
	 */
	return (-1);
}

int
linktype_to_dlt(int linktype)
{
	int i;

	for (i = 0; map[i].linktype != -1; i++) {
		if (map[i].linktype == linktype)
			return (map[i].dlt);
	}

	/*
	 * If we don't have an entry for this link type, return
	 * the link type value; it may be a DLT_ value from an
	 * older version of libpcap.
	 */
	return linktype;
}

/*
 * The DLT_USB_LINUX and DLT_USB_LINUX_MMAPPED headers are in host
 * byte order when capturing (it's supplied directly from a
 * memory-mapped buffer shared by the kernel).
 *
 * When reading a DLT_USB_LINUX or DLT_USB_LINUX_MMAPPED capture file,
 * we need to convert it from the capturing host's byte order to
 * the reading host's byte order.
 */
void
swap_linux_usb_header(const struct pcap_pkthdr *hdr, u_char *buf,
    int header_len_64_bytes)
{
	pcap_usb_header_mmapped *uhdr = (pcap_usb_header_mmapped *)buf;

	/*
	 * The URB id is a totally opaque value; do we really need to 
	 * convert it to the reading host's byte order???
	 */
	if (hdr->caplen < 8)
		return;
	uhdr->id = SWAPLL(uhdr->id);
	if (hdr->caplen < 14)
		return;
	uhdr->bus_id = SWAPSHORT(uhdr->bus_id);
	if (hdr->caplen < 24)
		return;
	uhdr->ts_sec = SWAPLL(uhdr->ts_sec);
	if (hdr->caplen < 28)
		return;
	uhdr->ts_usec = SWAPLONG(uhdr->ts_usec);
	if (hdr->caplen < 32)
		return;
	uhdr->status = SWAPLONG(uhdr->status);
	if (hdr->caplen < 36)
		return;
	uhdr->urb_len = SWAPLONG(uhdr->urb_len);
	if (hdr->caplen < 40)
		return;
	uhdr->data_len = SWAPLONG(uhdr->data_len);

	if (header_len_64_bytes) {
		/*
		 * This is either the "version 1" header, with
		 * 16 bytes of additional fields at the end, or
		 * a "version 0" header from a memory-mapped
		 * capture, with 16 bytes of zeroed-out padding
		 * at the end.  Byte swap them as if this were
		 * a "version 1" header.
		 */
		if (hdr->caplen < 52)
			return;
		uhdr->interval = SWAPLONG(uhdr->interval);
		if (hdr->caplen < 56)
			return;
		uhdr->start_frame = SWAPLONG(uhdr->start_frame);
		if (hdr->caplen < 60)
			return;
		uhdr->xfer_flags = SWAPLONG(uhdr->xfer_flags);
		if (hdr->caplen < 64)
			return;
		uhdr->ndesc = SWAPLONG(uhdr->ndesc);
	}	
}

/*
 * Marko Kiiskila carnil@cs.tut.fi
 *
 * Tampere University of Technology - Telecommunications Laboratory
 *
 * Permission to use, copy, modify and distribute this
 * software and its documentation is hereby granted,
 * provided that both the copyright notice and this
 * permission notice appear in all copies of the software,
 * derivative works or modified versions, and any portions
 * thereof, that both notices appear in supporting
 * documentation, and that the use of this software is
 * acknowledged in any publications resulting from using
 * the software.
 *
 * TUT ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION AND DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS
 * SOFTWARE.
 *
 */

#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header$ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#include <stdio.h>
#include <pcap.h>

#include "interface.h"
#include "addrtoname.h"
#include "extract.h"
#include "ether.h"
#include "lane.h"

static const struct tok lecop2str[] = {
	{ 0x0001,	"configure request" },
	{ 0x0101,	"configure response" },
	{ 0x0002,	"join request" },
	{ 0x0102,	"join response" },
	{ 0x0003,	"ready query" },
	{ 0x0103,	"ready indication" },
	{ 0x0004,	"register request" },
	{ 0x0104,	"register response" },
	{ 0x0005,	"unregister request" },
	{ 0x0105,	"unregister response" },
	{ 0x0006,	"ARP request" },
	{ 0x0106,	"ARP response" },
	{ 0x0007,	"flush request" },
	{ 0x0107,	"flush response" },
	{ 0x0008,	"NARP request" },
	{ 0x0009,	"topology request" },
	{ 0,		NULL }
};

static void
lane_hdr_print(const u_char *bp)
{
	(void)printf("lecid:%x ", EXTRACT_16BITS(bp));
}

/*
 * This is the top level routine of the printer.  'p' points
 * to the LANE header of the packet, 'h->ts' is the timestamp,
 * 'h->len' is the length of the packet off the wire, and 'h->caplen'
 * is the number of bytes actually captured.
 *
 * This assumes 802.3, not 802.5, LAN emulation.
 */
void
lane_print(const u_char *p, u_int length, u_int caplen)
{
	struct lane_controlhdr *lec;

	if (caplen < sizeof(struct lane_controlhdr)) {
		printf("[|lane]");
		return;
	}

	lec = (struct lane_controlhdr *)p;
	if (EXTRACT_16BITS(&lec->lec_header) == 0xff00) {
		/*
		 * LE Control.
		 */
		printf("lec: proto %x vers %x %s",
		    lec->lec_proto, lec->lec_vers,
		    tok2str(lecop2str, "opcode-#%u", EXTRACT_16BITS(&lec->lec_opcode)));
		return;
	}

	/*
	 * Go past the LE header.
	 */
	length -= 2;
	caplen -= 2;
	p += 2;

	/*
	 * Now print the encapsulated frame, under the assumption
	 * that it's an Ethernet frame.
	 */
	ether_print(p, length, caplen, lane_hdr_print, p - 2);
}

u_int
lane_if_print(const struct pcap_pkthdr *h, const u_char *p)
{
	lane_print(p, h->len, h->caplen);

	return (sizeof(struct lecdatahdr_8023));
}

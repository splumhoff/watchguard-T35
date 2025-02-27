/*
 * Copyright (c) 1998-2006 The TCPDUMP project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code
 * distributions retain the above copyright notice and this paragraph
 * in its entirety, and (2) distributions including binary code include
 * the above copyright notice and this paragraph in its entirety in
 * the documentation or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND
 * WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, WITHOUT
 * LIMITATION, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 *
 * support for the IEEE MPCP protocol as per 802.3ah
 *
 * Original code by Hannes Gredler (hannes@juniper.net)
 */

#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header$";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "interface.h"
#include "extract.h"
#include "addrtoname.h"
#include "ether.h"

#define MPCP_TIMESTAMP_LEN        4
#define MPCP_TIMESTAMP_DURATION_LEN 2

struct mpcp_common_header_t {
    u_int8_t opcode[2];
    u_int8_t timestamp[MPCP_TIMESTAMP_LEN];
};

#define	MPCP_OPCODE_PAUSE   0x0001
#define	MPCP_OPCODE_GATE    0x0002
#define	MPCP_OPCODE_REPORT  0x0003
#define	MPCP_OPCODE_REG_REQ 0x0004
#define	MPCP_OPCODE_REG     0x0005
#define	MPCP_OPCODE_REG_ACK 0x0006

static const struct tok mpcp_opcode_values[] = {
    { MPCP_OPCODE_PAUSE, "Pause" },
    { MPCP_OPCODE_GATE, "Gate" },
    { MPCP_OPCODE_REPORT, "Report" },
    { MPCP_OPCODE_REG_REQ, "Register Request" },
    { MPCP_OPCODE_REG, "Register" },
    { MPCP_OPCODE_REG_ACK, "Register ACK" },
    { 0, NULL}
};

#define MPCP_GRANT_NUMBER_LEN 1
#define	MPCP_GRANT_NUMBER_MASK 0x7 
static const struct tok mpcp_grant_flag_values[] = {
    { 0x08, "Discovery" },
    { 0x10, "Force Grant #1" },
    { 0x20, "Force Grant #2" },
    { 0x40, "Force Grant #3" },
    { 0x80, "Force Grant #4" },
    { 0, NULL}
};

struct mpcp_grant_t {
    u_int8_t starttime[MPCP_TIMESTAMP_LEN];
    u_int8_t duration[MPCP_TIMESTAMP_DURATION_LEN];
};

struct mpcp_reg_req_t {
    u_int8_t flags;
    u_int8_t pending_grants;
};


static const struct tok mpcp_reg_req_flag_values[] = {
    { 1, "Register" },
    { 3, "De-Register" },
    { 0, NULL}
};

struct mpcp_reg_t {
    u_int8_t assigned_port[2];
    u_int8_t flags;
    u_int8_t sync_time[MPCP_TIMESTAMP_DURATION_LEN];
    u_int8_t echoed_pending_grants;
};

static const struct tok mpcp_reg_flag_values[] = {
    { 1, "Re-Register" },
    { 2, "De-Register" },
    { 3, "ACK" },
    { 4, "NACK" },
    { 0, NULL}
};

#define MPCP_REPORT_QUEUESETS_LEN    1
#define MPCP_REPORT_REPORTBITMAP_LEN 1
static const struct tok mpcp_report_bitmap_values[] = {
    { 0x01, "Q0" },
    { 0x02, "Q1" },
    { 0x04, "Q2" },
    { 0x08, "Q3" },
    { 0x10, "Q4" },
    { 0x20, "Q5" },
    { 0x40, "Q6" },
    { 0x80, "Q7" },
    { 0, NULL}
};

struct mpcp_reg_ack_t {
    u_int8_t flags;
    u_int8_t echoed_assigned_port[2];
    u_int8_t echoed_sync_time[MPCP_TIMESTAMP_DURATION_LEN];
};

static const struct tok mpcp_reg_ack_flag_values[] = {
    { 0, "NACK" },
    { 1, "ACK" },
    { 0, NULL}
};

void
mpcp_print(register const u_char *pptr, register u_int length) {

    union {
        const struct mpcp_common_header_t *common_header;
        const struct mpcp_grant_t *grant;
        const struct mpcp_reg_req_t *reg_req;
        const struct mpcp_reg_t *reg;
        const struct mpcp_reg_ack_t *reg_ack;
    } mpcp;


    const u_char *tptr;
    u_int16_t opcode;
    u_int8_t grant_numbers, grant;
    u_int8_t queue_sets, queue_set, report_bitmap, report;

    tptr=pptr;
    mpcp.common_header = (const struct mpcp_common_header_t *)pptr;

    if (!TTEST2(*tptr, sizeof(const struct mpcp_common_header_t)))
        goto trunc;
    opcode = EXTRACT_16BITS(mpcp.common_header->opcode);
    printf("MPCP, Opcode %s", tok2str(mpcp_opcode_values, "Unknown (%u)", opcode));
    if (opcode != MPCP_OPCODE_PAUSE) {
        printf(", Timestamp %u ticks", EXTRACT_32BITS(mpcp.common_header->timestamp));
    }
    printf(", length %u", length);

    if (!vflag)
        return;

    tptr += sizeof(const struct mpcp_common_header_t);

    switch (opcode) {
    case MPCP_OPCODE_PAUSE:
        break;

    case MPCP_OPCODE_GATE:
	if (!TTEST2(*tptr, MPCP_GRANT_NUMBER_LEN))
	    goto trunc;
        grant_numbers = *tptr & MPCP_GRANT_NUMBER_MASK;
        printf("\n\tGrant Numbers %u, Flags [ %s ]",
               grant_numbers,
               bittok2str(mpcp_grant_flag_values,
                          "?",
                          *tptr &~ MPCP_GRANT_NUMBER_MASK));
        tptr++;

        for (grant = 1; grant <= grant_numbers; grant++) {
            if (!TTEST2(*tptr, sizeof(const struct mpcp_grant_t)))
                goto trunc;
            mpcp.grant = (const struct mpcp_grant_t *)tptr;        
            printf("\n\tGrant #%u, Start-Time %u ticks, duration %u ticks",
                   grant,
                   EXTRACT_32BITS(mpcp.grant->starttime),
                   EXTRACT_16BITS(mpcp.grant->duration));
            tptr += sizeof(const struct mpcp_grant_t);
        }

	if (!TTEST2(*tptr, MPCP_TIMESTAMP_DURATION_LEN))
	    goto trunc;
        printf("\n\tSync-Time %u ticks", EXTRACT_16BITS(tptr));
        break;


    case MPCP_OPCODE_REPORT:
	if (!TTEST2(*tptr, MPCP_REPORT_QUEUESETS_LEN))
	    goto trunc;
        queue_sets = *tptr;
        tptr+=MPCP_REPORT_QUEUESETS_LEN;
        printf("\n\tTotal Queue-Sets %u", queue_sets);

        for (queue_set = 1; queue_set < queue_sets; queue_set++) {
            if (!TTEST2(*tptr, MPCP_REPORT_REPORTBITMAP_LEN))
                goto trunc;
            report_bitmap = *(tptr);
            printf("\n\t  Queue-Set #%u, Report-Bitmap [ %s ]",
                   queue_sets,
                   bittok2str(mpcp_report_bitmap_values, "Unknown", report_bitmap));
            tptr++;

            report=1;
            while (report_bitmap != 0) { 
                if (report_bitmap & 1) {
                    if (!TTEST2(*tptr, MPCP_TIMESTAMP_DURATION_LEN))
                        goto trunc;
                    printf("\n\t    Q%u Report, Duration %u ticks",
                           report,
                           EXTRACT_16BITS(tptr));
                    tptr+=MPCP_TIMESTAMP_DURATION_LEN;
                }
                report++;
                report_bitmap = report_bitmap >> 1;
            }
        }
        break;

    case MPCP_OPCODE_REG_REQ:
        if (!TTEST2(*tptr, sizeof(const struct mpcp_reg_req_t)))
            goto trunc;
        mpcp.reg_req = (const struct mpcp_reg_req_t *)tptr;        
        printf("\n\tFlags [ %s ], Pending-Grants %u",
               bittok2str(mpcp_reg_req_flag_values, "Reserved", mpcp.reg_req->flags),
               mpcp.reg_req->pending_grants);
        break;

    case MPCP_OPCODE_REG:
        if (!TTEST2(*tptr, sizeof(const struct mpcp_reg_t)))
            goto trunc;
        mpcp.reg = (const struct mpcp_reg_t *)tptr;        
        printf("\n\tAssigned-Port %u, Flags [ %s ]" \
               "\n\tSync-Time %u ticks, Echoed-Pending-Grants %u",
               EXTRACT_16BITS(mpcp.reg->assigned_port),
               bittok2str(mpcp_reg_flag_values, "Reserved", mpcp.reg->flags),
               EXTRACT_16BITS(mpcp.reg->sync_time),
               mpcp.reg->echoed_pending_grants);
        break;

    case MPCP_OPCODE_REG_ACK:
        if (!TTEST2(*tptr, sizeof(const struct mpcp_reg_ack_t)))
            goto trunc;
        mpcp.reg_ack = (const struct mpcp_reg_ack_t *)tptr;        
        printf("\n\tEchoed-Assigned-Port %u, Flags [ %s ]" \
               "\n\tEchoed-Sync-Time %u ticks",
               EXTRACT_16BITS(mpcp.reg_ack->echoed_assigned_port),
               bittok2str(mpcp_reg_ack_flag_values, "Reserved", mpcp.reg_ack->flags),
               EXTRACT_16BITS(mpcp.reg_ack->echoed_sync_time));
        break;

    default:
        /* unknown opcode - hexdump for now */
        print_unknown_data(pptr, "\n\t", length);
        break;
    }

    return;

trunc:
    printf("\n\t[|MPCP]");
}

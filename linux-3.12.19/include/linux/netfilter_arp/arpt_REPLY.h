#ifndef _ARPT_REPLY_H
#define _ARPT_REPLY_H
#include <linux/netfilter_arp/arp_tables.h>

struct arpt_reply
{
	char mac[ARPT_DEV_ADDR_LEN_MAX];
	u_int8_t flags;
	int target;
};

#define ARPT_REPLY_MAC 0x01

#endif /* _ARPT_REPLY_H */

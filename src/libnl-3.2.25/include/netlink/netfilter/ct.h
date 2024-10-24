/*
 * netlink/netfilter/ct.h	Conntrack
 *
 *	This library is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU Lesser General Public
 *	License as published by the Free Software Foundation version 2.1
 *	of the License.
 *
 * Copyright (c) 2003-2008 Thomas Graf <tgraf@suug.ch>
 * Copyright (c) 2007 Philip Craig <philipc@snapgear.com>
 * Copyright (c) 2007 Secure Computing Corporation
 */

#ifndef NETLINK_CT_H_
#define NETLINK_CT_H_

#include <netlink/netlink.h>
#include <netlink/addr.h>
#include <netlink/cache.h>
#include <netlink/msg.h>

#ifdef __cplusplus
extern "C" {
#endif

struct nfnl_ct;

struct nfnl_ct_timestamp {
	uint64_t		start;
	uint64_t		stop;
};

#define CT_TIMEOUT_OP_EQUAL 0
#define CT_TIMEOUT_OP_GT    1
#define CT_TIMEOUT_OP_LT    2

extern struct nl_object_ops ct_obj_ops;

extern struct nfnl_ct *	nfnl_ct_alloc(void);
extern int	nfnl_ct_alloc_cache(struct nl_sock *, struct nl_cache **);

extern int	nfnlmsg_ct_group(struct nlmsghdr *);
extern int	nfnlmsg_ct_parse(struct nlmsghdr *, struct nfnl_ct **);

extern void	nfnl_ct_get(struct nfnl_ct *);
extern void	nfnl_ct_put(struct nfnl_ct *);

extern int	nfnl_ct_dump_request(struct nl_sock *);

extern int	nfnl_ct_build_add_request(const struct nfnl_ct *, int,
					  struct nl_msg **);
extern int	nfnl_ct_add(struct nl_sock *, const struct nfnl_ct *, int);

extern int	nfnl_ct_build_delete_request(const struct nfnl_ct *, int,
					     struct nl_msg **);
extern int	nfnl_ct_del(struct nl_sock *, const struct nfnl_ct *, int);

extern int	nfnl_ct_build_query_request(const struct nfnl_ct *, int,
					    struct nl_msg **);
extern int	nfnl_ct_query(struct nl_sock *, const struct nfnl_ct *, int);

extern void	nfnl_ct_set_family(struct nfnl_ct *, uint8_t);
extern uint8_t	nfnl_ct_get_family(const struct nfnl_ct *);

extern void	nfnl_ct_set_proto(struct nfnl_ct *, uint8_t);
extern int	nfnl_ct_test_proto(const struct nfnl_ct *);
extern uint8_t	nfnl_ct_get_proto(const struct nfnl_ct *);

extern void	nfnl_ct_set_tcp_state(struct nfnl_ct *, uint8_t);
extern int	nfnl_ct_test_tcp_state(const struct nfnl_ct *);
extern uint8_t	nfnl_ct_get_tcp_state(const struct nfnl_ct *);
extern char *	nfnl_ct_tcp_state2str(uint8_t, char *, size_t);
extern int	nfnl_ct_str2tcp_state(const char *name);

extern void	nfnl_ct_set_status(struct nfnl_ct *, uint32_t);
extern void	nfnl_ct_unset_status(struct nfnl_ct *, uint32_t);
extern int	nfnl_ct_test_status(const struct nfnl_ct *ct);
extern uint32_t	nfnl_ct_get_status(const struct nfnl_ct *);
extern char *	nfnl_ct_status2str(int, char *, size_t);
extern int	nfnl_ct_str2status(const char *);

extern void	nfnl_ct_set_timeout(struct nfnl_ct *, uint32_t);
extern void	nfnl_ct_set_timeout_op(struct nfnl_ct *, uint8_t);
extern int	nfnl_ct_test_timeout(const struct nfnl_ct *);
extern uint32_t	nfnl_ct_get_timeout(const struct nfnl_ct *);

extern void	nfnl_ct_set_mark(struct nfnl_ct *, uint32_t);
extern int	nfnl_ct_test_mark(const struct nfnl_ct *);
extern uint32_t	nfnl_ct_get_mark(const struct nfnl_ct *);

extern void	nfnl_ct_set_use(struct nfnl_ct *, uint32_t);
extern int	nfnl_ct_test_use(const struct nfnl_ct *);
extern uint32_t	nfnl_ct_get_use(const struct nfnl_ct *);

extern void	nfnl_ct_set_id(struct nfnl_ct *, uint32_t);
extern int	nfnl_ct_test_id(const struct nfnl_ct *);
extern uint32_t	nfnl_ct_get_id(const struct nfnl_ct *);

extern void	nfnl_ct_set_zone(struct nfnl_ct *, uint16_t);
extern int	nfnl_ct_test_zone(const struct nfnl_ct *);
extern uint16_t	nfnl_ct_get_zone(const struct nfnl_ct *);

extern int	nfnl_ct_set_src(struct nfnl_ct *, int, struct nl_addr *);
extern struct nl_addr *	nfnl_ct_get_src(const struct nfnl_ct *, int);

extern int	nfnl_ct_set_dst(struct nfnl_ct *, int, struct nl_addr *);
extern struct nl_addr *	nfnl_ct_get_dst(const struct nfnl_ct *, int);

extern void	nfnl_ct_set_src_port(struct nfnl_ct *, int, uint16_t);
extern int	nfnl_ct_test_src_port(const struct nfnl_ct *, int);
extern uint16_t	nfnl_ct_get_src_port(const struct nfnl_ct *, int);

extern void	nfnl_ct_set_dst_port(struct nfnl_ct *, int, uint16_t);
extern int	nfnl_ct_test_dst_port(const struct nfnl_ct *, int);
extern uint16_t	nfnl_ct_get_dst_port(const struct nfnl_ct *, int);

extern void	nfnl_ct_set_icmp_id(struct nfnl_ct *, int, uint16_t);
extern int	nfnl_ct_test_icmp_id(const struct nfnl_ct *, int);
extern uint16_t	nfnl_ct_get_icmp_id(const struct nfnl_ct *, int);

extern void	nfnl_ct_set_icmp_type(struct nfnl_ct *, int, uint8_t);
extern int	nfnl_ct_test_icmp_type(const struct nfnl_ct *, int);
extern uint8_t	nfnl_ct_get_icmp_type(const struct nfnl_ct *, int);

extern void	nfnl_ct_set_icmp_code(struct nfnl_ct *, int, uint8_t);
extern int	nfnl_ct_test_icmp_code(const struct nfnl_ct *, int);
extern uint8_t	nfnl_ct_get_icmp_code(const struct nfnl_ct *, int);

extern void	nfnl_ct_set_packets(struct nfnl_ct *, int, uint64_t);
extern int	nfnl_ct_test_packets(const struct nfnl_ct *, int);
extern uint64_t	nfnl_ct_get_packets(const struct nfnl_ct *,int);

extern void	nfnl_ct_set_bytes(struct nfnl_ct *, int, uint64_t);
extern int	nfnl_ct_test_bytes(const struct nfnl_ct *, int);
extern uint64_t	nfnl_ct_get_bytes(const struct nfnl_ct *, int);

extern void nfnl_ct_set_timestamp(struct nfnl_ct *, uint64_t, uint64_t);
extern int nfnl_ct_test_timestamp(const struct nfnl_ct *);
extern const struct nfnl_ct_timestamp *nfnl_ct_get_timestamp(const struct nfnl_ct *);

extern uint32_t nfnl_ct_get_wg_policy(const struct nfnl_ct *);
extern void     nfnl_ct_set_wg_policy(struct nfnl_ct *,uint32_t);

extern uint32_t nfnl_ct_get_wg_src_user(const struct nfnl_ct *);
extern void     nfnl_ct_set_wg_src_user(struct nfnl_ct *,uint32_t);

extern uint32_t nfnl_ct_get_wg_dst_user(const struct nfnl_ct *); 
extern void     nfnl_ct_set_wg_dst_user(struct nfnl_ct *,uint32_t);

extern uint32_t nfnl_ct_get_wg_src_session(const struct nfnl_ct *); 
extern void     nfnl_ct_set_wg_src_session(struct nfnl_ct *,uint32_t);

extern uint32_t nfnl_ct_get_wg_dst_session(const struct nfnl_ct *); 
extern void     nfnl_ct_set_wg_dst_session(struct nfnl_ct *,uint32_t);

extern uint32_t nfnl_ct_get_wg_src_domain_id(const struct nfnl_ct *ct);
extern void     nfnl_ct_set_wg_src_domain_id( struct nfnl_ct *ct, uint32_t sdid);

extern uint32_t nfnl_ct_get_wg_dst_domain_id(const struct nfnl_ct *ct);
extern void     nfnl_ct_set_wg_dst_domain_id( struct nfnl_ct *ct, uint32_t ddid);

extern uint32_t nfnl_ct_get_wg_src_country(const struct nfnl_ct *ct);
extern void     nfnl_ct_set_wg_src_country( struct nfnl_ct *ct, uint32_t country_id);

extern uint32_t nfnl_ct_get_wg_dst_country(const struct nfnl_ct *ct);
extern void     nfnl_ct_set_wg_dst_country( struct nfnl_ct *ct, uint32_t country_id);

extern uint32_t nfnl_ct_get_wg_in_if(const struct nfnl_ct *); 
extern void     nfnl_ct_set_wg_in_if(struct nfnl_ct *,uint32_t);

extern uint32_t nfnl_ct_get_wg_out_if(const struct nfnl_ct *); 
extern void     nfnl_ct_set_wg_out_if(struct nfnl_ct *,uint32_t);

uint16_t nfnl_ct_get_wg_cat_id(const struct nfnl_ct *ct);
void nfnl_ct_set_wg_cat_id( struct nfnl_ct *ct,uint16_t id);

uint16_t nfnl_ct_get_wg_app_id(const struct nfnl_ct *ct);
void nfnl_ct_set_wg_app_id( struct nfnl_ct *ct,uint16_t id);

uint8_t nfnl_ct_get_wg_beh_id(const struct nfnl_ct *ct);
void nfnl_ct_set_wg_beh_id( struct nfnl_ct *ct,uint8_t id);

uint32_t nfnl_ct_get_wg_orig_fwmark(const struct nfnl_ct *ct);
void nfnl_ct_set_wg_orig_fwmark( struct nfnl_ct *ct, uint32_t mark);

uint32_t nfnl_ct_get_wg_repl_fwmark(const struct nfnl_ct *ct);
void nfnl_ct_set_wg_repl_fwmark( struct nfnl_ct *ct, uint32_t mark);

time_t nfnl_ct_get_wg_start(const struct nfnl_ct *ct);
void nfnl_ct_set_wg_start( struct nfnl_ct *ct,time_t t);

time_t nfnl_ct_get_wg_end(const struct nfnl_ct *ct);
void nfnl_ct_set_wg_end( struct nfnl_ct *ct,time_t t);

/* WG:EN RFE56793 */
void *nfnl_ct_get_wg_ptr(const struct nfnl_ct *ct);
void nfnl_ct_set_wg_ptr(struct nfnl_ct *ct, void *ptr, void (*free_cb)(struct nfnl_ct *));
/* WG:EN RFE56793 */

uint32_t nfnl_ct_get_wg_flags(const struct nfnl_ct *ct);
void nfnl_ct_set_wg_flags( struct nfnl_ct *ct, uint32_t flags);

uint8_t nfnl_ct_get_wg_worker_id(const struct nfnl_ct *ct);
void nfnl_ct_set_wg_worker_id( struct nfnl_ct *ct, uint8_t worker_id);

#ifdef __cplusplus
}
#endif

#endif

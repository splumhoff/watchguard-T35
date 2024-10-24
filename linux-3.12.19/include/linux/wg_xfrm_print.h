/**
 * @file wg_xfrm_print.h
 *
 * @brief 
 *
 * @par Module: netkey
 *  
 * WatchGuard Technologies, Inc.
 *
 * @date 2010 Dec, 27 11:55:26 AM
 *
 */

#ifndef _LINUX_WG_XFRM_PRINT_H
#define _LINUX_WG_XFRM_PRINT_H


/* Include files */

#include <uapi/linux/wg_xfrm_print.h>
#include <uapi/linux/ip.h>
#include <uapi/linux/ipv6.h>

#ifdef __KERNEL__
#include <linux/ratelimit.h>

extern atomic_t wgXfrmLogCnt;

#define wgXfrmLogCrit(fmt, args...)                                         \
    printk_ratelimited (KERN_ERR "WG:[%s] "fmt, __FUNCTION__, ##args)
#define wgXfrmLogErr    wgXfrmLogCrit

/* jlv: 8/27/2014
 *
 * wgXfrmLog() does not have any filter option, we need to avoid calling it when
 * printing the per-pkt log!!
 *
 **/
#define wgXfrmLogObj(type, fmt, args...)                                    \
    wgXfrmLog(type, WGLOG_XFRM_OBJDUMP, fmt, ##args)
#define wgXfrmLogMsg(level, fmt, args...)                                   \
    wgXfrmLog(WGLOG_XFRM_MSG, level, fmt, ##args)
#define wgXfrmLogCluster(level, fmt, args...)                               \
    wgXfrmLog(WGLOG_XFRM_CLST, level, fmt, ##args)
#define wgXfrmLogPkt(level, fmt, args...)                                   \
    wgXfrmLog(WGLOG_XFRM_PKT, level, fmt, ##args)
    
#define wgXfrmLog(t, d, fmt, args...) do {                                  \
        if ((wgXfrmDebug.type & (t))                                        \
            && (wgXfrmDebug.debug & (d))                                    \
            && atomic_read(&wgXfrmLogCnt) > 0) {                            \
            printk (KERN_ALERT "WG:[%s()] "fmt, __FUNCTION__, ##args);      \
            atomic_dec(&wgXfrmLogCnt);                                      \
        }                                                                   \
    } while (0)

#define wgXfrm4LogIPv4PktLog(src_ip, dst_ip, proto, level, fmt, args...) do { \
        if (unlikely((wgXfrmDebug.type & WGLOG_XFRM_PKT)                    \
            && (wgXfrmDebug.debug >= (level))                               \
            && atomic_read(&wgXfrmLogCnt) > 0                               \
            && wgXfrmIPV4PktMatch((src_ip), (dst_ip)))) {                   \
            printk(KERN_ALERT "WG:[%s():pkt(src_ip=%pI4 dst_ip=%pI4 proto=%d)] "fmt, \
                    __FUNCTION__, &(src_ip), &(dst_ip), (proto), ##args);   \
            atomic_dec(&wgXfrmLogCnt);                                      \
        }                                                                   \
    } while (0)

#define wgXfrm4LogPktByFlowi(fl, level, fmt, args...)                        \
    wgXfrmLogIPv4PktLog((fl)->u.ip4.saddr, (fl)->u.ip4.daddr, (fl)->flowi_proto, level, fmt, ##args)
     
/* JLV: 8/27/2014
 *
 * We check if a skb should be marked as XFRM_PKTTRACE at the earliest point - 
 * ip_rcv()->wg_xfrm4_early_gre_decap(). In the bvpn vif case, for the ingress 
 * pkt, we will mark the skb after we strip the gre header - wg_xfrm_ipgre_decap()
 *
 **/
#define wgXfrm4LogSetPktTrace(src_ip, dst_ip, proto, skb, level, caller) do { \
        if (unlikely((wgXfrmDebug.type & WGLOG_XFRM_PKT)                    \
            && (wgXfrmDebug.debug >= (level))                               \
            && atomic_read(&wgXfrmLogCnt) > 0                               \
            && wgXfrmIPV4PktMatch((src_ip), (dst_ip)))) {                   \
            IPCB(skb)->flags |= IPSKB_XFRM_PKTTRACE;                        \
            printk(KERN_ALERT "##WG:[%s():pkt(src_ip=%pI4 dst_ip=%pI4 proto=%d) skb(%p mark=0x%x len=%u)] receive a matched skb\n", \
                    caller, &(src_ip), &(dst_ip), (proto), skb, skb->mark, skb->len); \
            atomic_dec(&wgXfrmLogCnt);                                      \
        }                                                                   \
    } while (0)

#define wgXfrm6LogSetPktTrace(src_ip, dst_ip, proto, skb, level, caller) do { \
        if (unlikely((wgXfrmDebug.type & WGLOG_XFRM_PKT)                    \
            && (wgXfrmDebug.debug >= (level))                               \
            && atomic_read(&wgXfrmLogCnt) > 0                               \
            && wgXfrmIPV6PktMatch((src_ip), (dst_ip)))) {                   \
            IP6CB(skb)->flags |= IPSKB_XFRM_PKTTRACE;                        \
            printk(KERN_ALERT "##WG:[%s():pkt(src_ip=%pI6c dst_ip=%pI6c proto=%d) skb(%p mark=0x%x len=%u)] receive a matched skb\n", \
                    caller, &(src_ip), &(dst_ip), (proto), skb, skb->mark, skb->len); \
            atomic_dec(&wgXfrmLogCnt);                                      \
        }                                                                   \
    } while (0)

#define wgXfrm4LogSetPktTraceWithMsg(src_ip, dst_ip, proto, skb, level, fmt, args...) do { \
        if (unlikely((wgXfrmDebug.type & WGLOG_XFRM_PKT)                    \
            && (wgXfrmDebug.debug >= (level))                               \
            && atomic_read(&wgXfrmLogCnt) > 0                               \
            && wgXfrmIPV4PktMatch((src_ip), (dst_ip)))) {                   \
            IPCB(skb)->flags |= IPSKB_XFRM_PKTTRACE;                        \
            printk(KERN_ALERT "WG:[%s():pkt(src_ip=%pI4 dst_ip=%pI4 proto=%d) skb(%p mark=0x%x len=%u)] "fmt, \
                    __FUNCTION__, &(src_ip), &(dst_ip), (proto), skb, skb->mark, skb->len, ##args); \
            atomic_dec(&wgXfrmLogCnt);                                      \
        }                                                                   \
    } while (0)
#define wgXfrm6LogSetPktTraceWithMsg(src_ip, dst_ip, proto, skb, level, fmt, args...) do { \
        if (unlikely((wgXfrmDebug.type & WGLOG_XFRM_PKT)                    \
            && (wgXfrmDebug.debug >= (level))                               \
            && atomic_read(&wgXfrmLogCnt) > 0                               \
            && wgXfrmIPV6PktMatch((src_ip), (dst_ip)))) {                   \
            IP6CB(skb)->flags |= IPSKB_XFRM_PKTTRACE;                       \
            printk(KERN_ALERT "WG:[%s():pkt(src_ip=%pI6c dst_ip=%pI6c proto=%d) skb(%p mark=0x%x len=%u)] "fmt, \
                    __FUNCTION__, &(src_ip), &(dst_ip), (proto), skb, skb->mark, skb->len, ##args); \
            atomic_dec(&wgXfrmLogCnt);                                      \
        }                                                                   \
    } while (0) 

/* JLV: 8/27/2014
 *
 * We use this macro to print the per-pkt log according to the skb's wg_xfrm_pktrace
 * mark
 *
 **/
#define wgXfrmLogPktBySkbPktTraceMark(skb, level, fmt, args...) do {        \
        if(unlikely((wgXfrmDebug.type & WGLOG_XFRM_PKT)                     \
            && (wgXfrmDebug.debug >= (level))                               \
            && skb                                                          \
            && (IPCB(skb)->flags & IPSKB_XFRM_PKTTRACE)                     \
            && atomic_read(&wgXfrmLogCnt) > 0)) {                           \
            printk(KERN_ALERT "WG:[%s():skb(%p mark=0x%x len=%u)] "fmt, __FUNCTION__, skb, skb->mark, skb->len, ##args); \
            atomic_dec(&wgXfrmLogCnt);                                      \
        }                                                                   \
    } while(0)

#define wgXfrm6LogPktBySkbPktTraceMark(skb, level, fmt, args...) do {       \
        if(unlikely((wgXfrmDebug.type & WGLOG_XFRM_PKT)                     \
            && (wgXfrmDebug.debug >= (level))                               \
            && skb                                                          \
            && (IP6CB(skb)->flags & IPSKB_XFRM_PKTTRACE)                    \
            && atomic_read(&wgXfrmLogCnt) > 0)) {                           \
            printk(KERN_ALERT "WG:[%s():skb(%p mark=0x%x len=%u)] "fmt, __FUNCTION__, skb, skb->mark, skb->len, ##args); \
            atomic_dec(&wgXfrmLogCnt);                                      \
        }                                                                   \
    } while(0)

/*
 * We use this macro to print the skb by the given caller instead of __FUNCTION__
 */
#define wgXfrm4LogPktCallerBySkbPktTraceMark(caller, skb, level, fmt, args...) do { \
        if(unlikely((wgXfrmDebug.type & WGLOG_XFRM_PKT)                     \
            && (wgXfrmDebug.debug >= (level))                               \
            && skb                                                          \
            && (IPCB(skb)->flags & IPSKB_XFRM_PKTTRACE)                     \
            && atomic_read(&wgXfrmLogCnt) > 0)) {                           \
            printk(KERN_ALERT "WG:[%s():skb(%p mark=0x%x len=%u)] "fmt, caller, skb, skb->mark, skb->len, ##args); \
            atomic_dec(&wgXfrmLogCnt);                                      \
        }                                                                   \
    } while(0)

#define wgXfrm6LogPktCallerBySkbPktTraceMark(caller, skb, level, fmt, args...) do { \
        if(unlikely((wgXfrmDebug.type & WGLOG_XFRM_PKT)                     \
            && (wgXfrmDebug.debug >= (level))                               \
            && skb                                                          \
            && (IP6CB(skb)->flags & IPSKB_XFRM_PKTTRACE)                     \
            && atomic_read(&wgXfrmLogCnt) > 0)) {                           \
            printk(KERN_ALERT "WG:[%s():skb(%p mark=0x%x len=%u)] "fmt, caller, skb, skb->mark, skb->len, ##args); \
            atomic_dec(&wgXfrmLogCnt);                                      \
        }                                                                   \
    } while(0)

/*
 * We use this macro to print the per-pkt log according to fl's wg_xfrmtrace mark
 * 
 * instructions:
 * If fmt/args DO NOT print any address info, we can use the marco - wgXfrmLogPktByFlowTraceMark()
 * Otherwise, we need to use either wgXfrm4LogPktByFlowTraceMark() or wgXfrm6LogPktByFlowTraceMark()
 * 
 */
#define wgXfrm4LogPktByFlowTraceMark(fl, level, fmt, args...) do {          \
        if(unlikely((wgXfrmDebug.type & WGLOG_XFRM_PKT)                     \
            && (wgXfrmDebug.debug >= (level))                               \
            && ((fl)->flowi_flags & FLOWI_FLAG_WG_XFRMTRACE)                \
            && atomic_read(&wgXfrmLogCnt) > 0)) {                           \
            printk(KERN_ALERT "WG:[%s():pkt(src_ip=%pI4 dst_ip=%pI4 proto=%d)] "fmt,\
                __FUNCTION__, &(fl)->u.ip4.saddr, &(fl)->u.ip4.daddr, (fl)->flowi_proto, ##args); \
            atomic_dec(&wgXfrmLogCnt);                                      \
        }                                                                   \
    } while(0)
#define wgXfrm6LogPktByFlowTraceMark(fl, level, fmt, args...) do {          \
        if(unlikely((wgXfrmDebug.type & WGLOG_XFRM_PKT)                     \
            && (wgXfrmDebug.debug >= (level))                               \
            && ((fl)->flowi_flags & FLOWI_FLAG_WG_XFRMTRACE)                \
            && atomic_read(&wgXfrmLogCnt) > 0)) {                           \
            printk(KERN_ALERT "WG:[%s():pkt(src_ip=%pI6c dst_ip=%pI6c proto=%d)] "fmt, \
                __FUNCTION__, &(fl)->u.ip6.saddr, &(fl)->u.ip6.daddr, (fl)->flowi_proto, ##args); \
            atomic_dec(&wgXfrmLogCnt);                                      \
        }                                                                   \
    } while(0)

#define wgXfrmLogPktByFlowTraceMark(family, fl, level, fmt, args...) do {   \
        if(unlikely((wgXfrmDebug.type & WGLOG_XFRM_PKT)                     \
            && (wgXfrmDebug.debug >= (level))                               \
            && ((fl)->flowi_flags & FLOWI_FLAG_WG_XFRMTRACE)                \
            && atomic_read(&wgXfrmLogCnt) > 0)) {                           \
            if (AF_INET == family) {                                        \
                printk(KERN_ALERT "WG:[%s():pkt(src_ip=%pI4 dst_ip=%pI4 proto=%d)] "fmt, \
                    __FUNCTION__, &(fl)->u.ip4.saddr, &(fl)->u.ip4.daddr, (fl)->flowi_proto, ##args); \
            } else if (AF_INET6 == family) {                                     \
                printk(KERN_ALERT "WG:[%s():pkt(src_ip=%pI6c dst_ip=%pI6c proto=%d)] "fmt,\
                    __FUNCTION__, &(fl)->u.ip6.saddr, &(fl)->u.ip6.daddr, (fl)->flowi_proto, ##args); \
            }                                                               \
            atomic_dec(&wgXfrmLogCnt);                                      \
        }                                                                   \
    } while(0) 

/* JLV: 8/27/2014
 *
 * These wgXfrmLog*Match macros is used to print the log as long as "match" is set as true.
 * Usually, we can set the "match" at the begining of a function, then use it to print 
 * the log within the same function.
 * note: we do not check wgXfrmLogCnt, which means we always print the logs within the same func.
 * 
 **/
#define wgXfrmLogPktMatch(match, family, hdr, fmt, args...) do {            \
        if (unlikely(match)) {                                              \
            if (AF_INET == family) { \
                printk(KERN_ALERT "WG:[%s():pkt(src_ip=%pI4 dst_ip=%pI4 proto=%d)] "fmt, \
                    __FUNCTION__, &(((struct iphdr *)(hdr))->saddr), &(((struct iphdr *)(hdr))->daddr), \
                    (((struct iphdr *)(hdr))->protocol), ##args);           \
            } else if (AF_INET6 == family) {                                \
                printk(KERN_ALERT "WG:[%s():pkt(src_ip=%pI6c dst_ip=%pI6c proto=%d)] "fmt, \
                    __FUNCTION__, &(((struct ipv6hdr *)(hdr))->saddr), &(((struct ipv6hdr *)(hdr))->daddr), \
                    (((struct ipv6hdr *)(hdr))->nexthdr), ##args);          \
            }                                                               \
        }                                                                   \
    } while(0)

/*
 * this macro should work at both IPv4 & IPv6 cases
 */
#define wgXfrmLogPktMatchFlowi(match, family, fl, fmt, args...) do {        \
        if (unlikely(match)) {                                              \
            if (AF_INET == family) {                                        \
                printk(KERN_ALERT "WG:[%s():pkt(src_ip=%pI4 dst_ip=%pI4 proto=%d)] "fmt, \
                    __FUNCTION__, &((fl)->u.ip4.saddr), &((fl)->u.ip4.daddr), (fl)->flowi_proto, ##args); \
            } else if (AF_INET6 == family) {                                \
                printk(KERN_ALERT "WG:[%s():pkt(src_ip=%pI6c dst_ip=%pI6c proto=%d)] "fmt, \
                    __FUNCTION__, &((fl)->u.ip6.saddr), &((fl)->u.ip6.daddr), (fl)->flowi_proto, ##args); \
            }                                                               \
        }                                                                   \
    } while (0)
        
/* smv: 09 Oct, 2009
 *
 * Added new log type to log events.
 * This needs to be replaced with WGLOG API once it is available.
 *
 * This API logs messages at level 4 and messages will be logged to
 * /var/log/diagnostics automatically.
 * Note: klogd may need to be restarted inorder to see these messages in
 * /var/log/diagnostics log file.
 * */
#define wgXfrmLogDiag(fmt, args...) printk_ratelimited(KERN_WARNING fmt, ##args)

#define wgXfrmLogICSAMsg(t,args...)    do {                                 \
        if ((wgXfrmDebug.type & WGLOG_XFRM_ICSA) &&                         \
                (wgXfrmDebug.icsa & t)) {                                   \
                    wgXfrmLogRejectedPkt (args);                            \
        }                                                                   \
    } while(0)

/**
 * This can be used when we can not use wgPrintXfrmSelector() Function
 */
#define wgXfrmDumpSelector(t, d, p) do {                                    \
        wgXfrmLog (t, d,                                                    \
            "SEL[daddr:%pI4/%u end_daddr:%pI4 dport:%d "                    \
            "dmask:0x%x saddr:%pI4/%u end_sadd:%pI4 sport:%d "              \
            "smask:0x%x family:%s proto:%d ifindex:%d]\n",                  \
            &p->daddr.a4, p->prefixlen_d,                                   \
            &p->wg.end_daddr.a4, p->dport, p->dport_mask,                   \
            &p->saddr.a4, p->prefixlen_s,                                   \
            &p->wg.end_saddr.a4, p->sport, p->sport_mask,                   \
            AF_FAMILY_STR(p->family), p->proto, p->ifindex);                \
    } while (0)


#endif  /* __KERNEL__ */

#ifdef __KERNEL__

/* forward declarations */

struct xfrm_policy;
struct xfrm_state;
struct in6_addr;

extern struct wg_xfrm_print wgXfrmDebug;


/* Function Prototypes */

extern int wgXfrmLogSet (struct wg_xfrm_print * p);
extern int wgXfrmIPV4PktMatch (__be32 saddr, __be32 daddr);
extern int wgXfrmIPV6PktMatch (struct in6_addr saddr, struct in6_addr daddr);

extern void wgXfrmDumpBuff (char * buf, size_t bufLen,
       char * title);

extern void wgPrintXfrmPcy (const __u32 type,
        const struct xfrm_policy * xp, const char * msg);
extern void wgPrintXfrmPcyInfo (const __u32 type,
        const struct xfrm_userpolicy_info * p, const char * msg);
extern void wgPrintXfrmUsrSaInfo (const __u32 type,
        const struct xfrm_usersa_info * p, const char * msg);
extern void wgPrintXfrmState (const __u32 type,
        const struct xfrm_state * p, const char * msg);

extern void wgXfrmLogRejectedPkt (__u16 family,
        xfrm_address_t *t_saddr, xfrm_address_t *t_daddr,
        xfrm_address_t *saddr, xfrm_address_t *saddr_end,
        __u8 smask, xfrm_address_t *daddr, xfrm_address_t *daddr_end,
        __u8 dmask, __u32 spi, int proto, __u32 seq, char *reason);

#endif  /* __KERNEL__ */

#endif /* _LINUX_WG_XFRM_PRINT_H */



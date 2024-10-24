#ifndef _LINUX_WG_KERNEL_H
#define _LINUX_WG_KERNEL_H

#ifdef	CONFIG_WG_ARCH_FREESCALE
#ifdef  CONFIG_PPC32
#define	WG_TICKER(a) { a = mftbl(); }
#else
#ifdef	CONFIG_PPC64
#define	WG_TICKER(a) { a = mftb(); }
#endif
#endif
#endif

#ifdef	CONFIG_WG_ARCH_X86
#define	WG_TICKER(a) rdtscl(a)
#endif

#ifdef	CONFIG_WG_ARCH_IXP4XX
#define	WG_TICKER(a) { a = 0; }
#endif

#define	WG_TIMECALL(call) {                                                \
    static unsigned long high = 0; unsigned long start, used;              \
    WG_TICKER(start); {call;} WG_TICKER(used); used -= start;              \
    if (unlikely(used > high)) printk(KERN_EMERG "Highwater %9lu %s@%d\n", \
                                      high = used, __FUNCTION__, __LINE__); }

#define WG_CONSOLE(a, ...) { int old = console_loglevel; \
                             console_loglevel = 9;       \
                             printk(a, ##__VA_ARGS__);   \
                             console_loglevel = old; }

extern	void  wg_setup_arch(char*, int); // Setup WG boot stuff
extern	int   wg_fault_report(char*);	 // Record a fault report item

extern	char* wg_vm_name;		 // The VM vendor name
extern	char* wg_get_vm_name(void);	 // Get VM vendor name

extern	void  wg_noop(void);		 // NOOP function pointer

#define	WG_CPU_T2081	   2081		 // Freescale T2081
#define	WG_CPU_T1042	   1042		 // Freescale T1042
#define	WG_CPU_T1024	   1024		 // Freescale T1024

#define	WG_CPU_P2020	   2020		 // Freescale P2020
#define	WG_CPU_P1020	   1020		 // Freescale P1020
#define	WG_CPU_P1011	   1011		 // Freescale P1011
#define	WG_CPU_P1010	   1010		 // Freescale P1010

#define	WG_CPU_X86(m)   ( 86000000+(m))	 // Generic Intel X86
#define	WG_CPU_686(m)   (686000000+(m))	 // Generic Intel 686

#define	WG_CPU_686_440			 (WG_CPU_686( 440))
#define	WG_CPU_686_3400			 (WG_CPU_686(3400))
#define	WG_CPU_686_5300			 (WG_CPU_686(5300))
#define	WG_CPU_686_9400			 (WG_CPU_686(9400))
#define	WG_CPU_686_5410			 (WG_CPU_686(5410))
#define	WG_CPU_686_5645			 (WG_CPU_686(5645))
#define	WG_CPU_686_1225			 (WG_CPU_686(1225))
#define	WG_CPU_686_1275			 (WG_CPU_686(1275))
#define	WG_CPU_686_2658			 (WG_CPU_686(2658))
#define	WG_CPU_686_3558			 (WG_CPU_686(3558))
#define	WG_CPU_686_3538			 (WG_CPU_686(3538))
#define	WG_CPU_686_3338			 (WG_CPU_686(3338))
#define	WG_CPU_686_3508			 (WG_CPU_686(3508))
#define	WG_CPU_686_3308			 (WG_CPU_686(3308))
#define	WG_CPU_686_2758			 (WG_CPU_686(2758))
#define	WG_CPU_686_2558			 (WG_CPU_686(2558))
#define	WG_CPU_686_2358			 (WG_CPU_686(2358))
#define	WG_CPU_686_1820			 (WG_CPU_686(1820))
#define	WG_CPU_686_3420			 (WG_CPU_686(3420))
#define	WG_CPU_686_4360			 (WG_CPU_686(4360))
#define	WG_CPU_686_2630			 (WG_CPU_686(2630))
#define	WG_CPU_686_2680			 (WG_CPU_686(2680))
#define	WG_CPU_686_3050			 (WG_CPU_686(3050))
#define	WG_CPU_686_3060			 (WG_CPU_686(3060))
#define	WG_CPU_686_3150			 (WG_CPU_686(3150))
#define	WG_CPU_686_3160			 (WG_CPU_686(3160))
#define	WG_CPU_686_3900			 (WG_CPU_686(3900))
#define	WG_CPU_686_4400			 (WG_CPU_686(4400))
#define	WG_CPU_686_6100			 (WG_CPU_686(6100))

#define	isT2081		((wg_cpu_model == WG_CPU_T2081))
#define	isT1042		((wg_cpu_model == WG_CPU_T1042))
#define	isT1024		((wg_cpu_model == WG_CPU_T1024))

#define	isTx0xx		((isT2081) || (isT1042) || (isT1024))

#define	isC2xxx		((wg_cpu_model == WG_CPU_686(2758)))
#define	isC3xxx		((wg_cpu_model == WG_CPU_686(3308)) || (wg_cpu_model == WG_CPU_686(3508)) || (wg_cpu_model == WG_CPU_686(3558)))

#define	has88E6176	((isT1024) || (wg_westport))
#define	has88E6190	((isC3xxx))
#define	has98DX3035	((isC2xxx))

extern	int wg_get_cpu_model(void);	 // Get CPU model

extern	int wg_cpus;			 // Number of expected CPUs
extern	int wg_cpu_model;		 // The CPU model
extern	int wg_cpu_version;		 // The CPU version

#ifdef	CONFIG_WG_ARCH_X86

extern	int wg_vashon;			 // Vashon    model
extern	int wg_spokane;			 // Spokane   model
extern	int wg_seattle;			 // Seattle   model
extern	int wg_kirkland;		 // Kirkland  model
extern	int wg_colfax;			 // Colfax    model
extern	int wg_rangeley;		 // Rangeley  model
extern	int wg_westport;		 // Westport  model
extern	int wg_winthrop;		 // Winthrop  model

#else

#define	wg_vashon	0		 // Vashon    model
#define	wg_spokane	0		 // Spokane   model
#define	wg_seattle	0		 // Seattle   model
#define	wg_kirkland	0		 // Kirkland  model
#define	wg_colfax	0		 // Colfax    model
#define	wg_rangeley	0		 // Rangeley  model
#define	wg_westport	0		 // Westport  model
#define	wg_winthrop	0		 // Winthrop  model

#endif

#ifdef	CONFIG_WG_ARCH_FREESCALE

extern	int wg_boren;			 // Boren     model
extern	int wg_dpa_bug;			 // Set if the FMAN has the checksum bug

#else

#define	wg_boren	0		 // Boren     model
#define	wg_dpa_bug	0		 // No Freescale DPA on X86

#endif

#define	WG_DSA_NETH	(8)		 // Max ethn    we handle
#define	WG_DSA_PHY	(32)		 // Max phys    we handle
#define	WG_DSA_NIF	(WG_DSA_NETH*2)	 // Max ifindex we handle

extern	int wg_nitrox_model;		 // Nitrox    crypto chip model (if any)
extern	int wg_cavecreek_model;		 // Cavecreek crypto chip model (if any)

extern	int wg_talitos_model;		 // Freescale talitos crypto
extern	int wg_caam_model;		 // Freescale caam    crypto

extern	int wg_dsa_type;		 // DSA switch type
extern	int wg_dsa_count;		 // DSA switch chip count
extern	int wg_dsa_debug;		 // DSA switch chip debug flags
extern	int wg_dsa_smi_reg;		 // Base address for SMI commands
extern	int wg_dsa_phy_num;		 // Number of PHYs on switch chip


#ifdef	CONFIG_WG_ARCH_X86
#define	DSA_PHY		wg_dsa_phy_num
#define	DSA_PHY_MAP(x)	((x) + wg_dsa_phy_map[x])
#endif
#ifdef	CONFIG_WG_ARCH_FREESCALE
#define	DSA_PHY		5
#define	DSA_PHY_MAP(x)	((x))
#endif

#define	DSA_PORT	(DSA_PHY + 2)

extern	s8  wg_dsa_phy_map[WG_DSA_PHY];	 // Map normalized PHYs to actual

#define	MARVELL_HLEN	2		 // Marvell header length

// Pointer to DSA MII PHY Bus
extern	struct	mii_bus*    wg_dsa_bus;

// MDIO bus release function pointer
extern	void (*wg_dsa_mdio_release)(void);

// SGMII link poll function pointer
extern	int  (*wg_dsa_sgmii_poll) (int);
extern	int    wg_ixgbe_sgmii_poll(int);

// Global Mutex for DSA
extern	struct mutex wg_dsa_mutex;

extern	int wg_soft_lockup;		 // Count of CPUs in soft lockup

#ifdef	CONFIG_32BIT
typedef	u32 wg_br_map;
typedef	u32 wg_sl_map;
#define	WG_BR_MAP(x)	(((x) < 32) ? (((wg_br_map)1) << (x)) : 0)
#else
typedef	u64 wg_br_map;
typedef	u64 wg_sl_map;
#define	WG_BR_MAP(x)	(((x) < 64) ? (((wg_br_map)1) << (x)) : 0)
#endif

extern	wg_br_map wg_br_bridged;	 // HW bridging         interfaces
extern	wg_br_map wg_br_primary;	 // HW bridging primary interfaces
extern	wg_br_map wg_br_dropped;	 // HW bridging dropped interfaces
extern	wg_sl_map wg_sl_slaved;		 // Slaved eth  device  interfaces

// Both of these map to a common queue vector

#define	wg_pss_recv	wg_que_recv
#define	wg_dsa_recv	wg_que_recv

extern	int	  wg_fips_sha;		 // SHA type
extern	int	  wg_fips_sha_err;	 // SHA auth errors
extern	int	  wg_fips_sha_len;	 // SHA key  length
extern	u8*	  wg_fips_sha_key;	 // SHA key  addrees
extern	int	  wg_fips_sha_mode0;	 // SHA QAT  mode  0

extern	u8*	  wg_fips_iv;		 // Deterministic FIPS IV

extern	int	  wg_fips_aad_len;	 // AAD length

// Get the  current PC
extern	void* wg_pc(void);

// Do a mutex lock with timeout
extern	int	 wg_mutex_lock(void* lock, int timeout);

#define	PSS_HLEN	4	   // PSS header tag size

#define	PSS_TAG_HI	0x8E	   // Special tag 0x8E8E
#define	PSS_TAG_LO	0x8E

#define	ETH_P_PSS	((PSS_TAG_HI << 8) | PSS_TAG_LO)

struct	sk_buff;

extern	int  (*wg_pss_untag)(struct sk_buff*, __u8*);

struct	pt_regs;

extern	int    register_timer_hook(int (*hook)(struct pt_regs*));
extern	void unregister_timer_hook(int (*hook)(struct pt_regs*));

// Define deprecated __dev values for older drivers
#define	__devinit
#define	__devinitdata
#define	__devexit
#define	__devexit_p(x)	(x)

#define	VM_RESERVED	(0)

struct	wg_counter	{
	struct	list_head list;
	char		  name[16];
	atomic_t	  count;
};

#define	WG_INC_COUNT(x)	    do { extern struct wg_counter* x; if (likely(x)) atomic_inc(&(x)->count); } while(0)
#define	WG_DEC_COUNT(x)	    do { extern struct wg_counter* x; if (likely(x)) atomic_dec(&(x)->count); } while(0)

extern	struct	wg_counter* wg_add_counter(char*);
extern	void                wg_del_counter(struct wg_counter**);

#define	WG_DUMP			(0x40000000)

#define	WG_TRACE		wg_trace_dump(__FUNCTION__, __LINE__,  0,      0);
#define	WG_TRACE_DUMP		wg_trace_dump(__FUNCTION__, __LINE__, WG_DUMP, 0);
#define	WG_TRACE_CODE(x)	wg_trace_dump(__FUNCTION__, __LINE__,  0,     (x));
#define	WG_TRACE_AT(x)		wg_trace_dump(__FUNCTION__, __LINE__, (x),     0);

#define	WG_TRACE_SGL(s,t)	wg_dump_sgl(__FUNCTION__, __LINE__,  0,         (s), (t));
#define	WG_TRACE_SGL_DUMP(s,t)	wg_dump_sgl(__FUNCTION__, __LINE__, 16|WG_DUMP, (s), (t));

//#define	CONFIG_WG_PLATFORM_TRACE	1

#ifdef	CONFIG_WG_PLATFORM_TRACE

struct	scatterlist;

extern	int	wg_trace_dump(const char*, int, int, int);
extern	void	wg_dump_hex(const u8*, u32, const u8*);
extern	void	wg_dump_sgl(const char*, int, int, struct scatterlist*, const char*);

#else

#define	wg_trace_dump(func, line, flag, code)
#define	wg_dump_hex(buf, len, tag)
#define	wg_dump_sgl(func, line, flag, sgl, tag)

#endif

#define	WG_SAMPLE_RETURN_PC(x)	wg_sample_pc((unsigned long)__builtin_return_address(x))

extern	void   wg_sample_nop(unsigned long);
extern	void (*wg_sample_pc)(unsigned long);

#ifdef	CONFIG_WG_ARCH_X86
extern	void   wg_kernel_get_x86_model(struct cpuinfo_x86*);
#endif

extern	int    wg_crash_memory;

// Stubs for older kernsls

#define	crypto_aead_set_reqsize(tfm, size) crypto_aead_crt(tfm)->reqsize = size

//#define	CONFIG_WG_PLATFORM_GCM_AUTHENC	1

#endif	// _LINUX_WG_KERNEL_H

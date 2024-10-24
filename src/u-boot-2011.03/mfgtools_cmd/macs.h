
/*
 * MAC related headers: mainly for switch based platforms. NON-switch based platforms
 *  might never need this.
 *
 * Split macs strctures adn functions from flash to make avoid #defining MACROS
 *  specific to mac related funtionality(i.e. usage of struct mac_addr_info_st,
 *  calls to mfg_write/read_macs).
 * This will make the Makefiles more cleaner and just have as necessary MACROS as needed.
 */

#ifndef __MACS_H__
#define __MACS_H__

#define DIXIE_MAX_NUM_MACS 9
#define DIXIE_MIN_NUM_MACS 3
#define MAX_NUM_MACS DIXIE_MAX_NUM_MACS
#define MIN_NUM_MACS DIXIE_MIN_NUM_MACS

#ifndef NUM_MACS
 #define NUM_MACS 8 /* some default value */
#endif

typedef struct mac_string_st {
    char    mac[ 18 ];  /* big enough for a null terminate mac address string */
    char    __pad[ 2];  /* Stupid alignment */
} mac_string;


/*
 * Internal struture to read/write macs to different interface in a
 * platform specific way.
 */
struct mac_addr_info_st {

    union {
        /* This array always starts with ethernets e.g. eth0->interface[0],
            eth3->interface[3] followed by special interfaces if needed like
            wireless on Edge and XTM2 
        */
        struct mac_string_st interface[MAX_NUM_MACS];

	/*
	 * The generic interface to which allows no change in API.
	 * This has to exit @ all times.
	 * Typically, the macs should fall right in order. But, under some 
	 * special circumstances, they won't. Only platform will be agnostic 
	 * of such details and it bears the responsibily of merging those gaps.
	 *
	 * Example: richland has only 6 wired macs and 2 wireless MACs
	 * In such a case, richland impl of platform_mfg_read_macs / platform_mfg_write_macs
	 * should be the overlay functions that convert data read from flash to generic
	 * struct (which is the user facing data)
	 */
        struct {
            struct mac_string_st eth0;
            struct mac_string_st eth1;
            struct mac_string_st eth2;
            struct mac_string_st eth3;
            struct mac_string_st eth4;
            struct mac_string_st eth5;
            struct mac_string_st eth6;
            struct mac_string_st eth7;
            struct mac_string_st eth8;

            struct mac_string_st ath0;
            struct mac_string_st ath1;
	};

	/*
	 * Allow XTM 2-series(richland) to have 6 MACs for wired interfaces and
	 * 2 MACs for wireless interfaces.
	 */
        struct {
            struct mac_string_st eth0;
            struct mac_string_st eth1;
            struct mac_string_st eth2;
            struct mac_string_st eth3;
            struct mac_string_st eth4;
            struct mac_string_st eth5;
            struct mac_string_st ath0;
            struct mac_string_st ath1;
	} richland;

	/*
	 * Allow XTM 25-series(newcastle) to have 5 MACs for wired interfaces and
	 * 2 MACs for wireless interfaces.
	 */
        struct {
            struct mac_string_st eth0;
            struct mac_string_st eth1;
            struct mac_string_st eth2;
            struct mac_string_st eth3;
            struct mac_string_st eth4;
            struct mac_string_st eth5; /* present for backward compatible reasons:
					  check again and try to remove later */
            struct mac_string_st ath0;
            struct mac_string_st ath1;
	} newcastle;

	/*
	 * Allow XTM 33-series(newport) to have 5 MACs for wired interfaces and
	 * 2 MACs for wireless interfaces.
	 */
        struct {
            struct mac_string_st eth0;
            struct mac_string_st eth1;
            struct mac_string_st eth2;
            struct mac_string_st eth3;
            struct mac_string_st eth4;
            struct mac_string_st eth5; /* present for backward compatible reasons:
					  check again and try to remove later */
            struct mac_string_st ath0;
            struct mac_string_st ath1;
	} newport;

	/*
	 * Allow XTM 330-series(chelan2) to have 7 MACs for wired interfaces
	 */
        struct {
            struct mac_string_st eth0;
            struct mac_string_st eth1;
            struct mac_string_st eth2;
            struct mac_string_st eth3;
            struct mac_string_st eth4;
            struct mac_string_st eth5;
            struct mac_string_st eth6;
	} chelan2;

	/*
	 * Allow T10 (dixie) to have 3 MACs for wired interfaces
	 */
        struct {
            struct mac_string_st eth0;
            struct mac_string_st eth1;
            struct mac_string_st eth2;
	} dixie;
    };
};

/* External/Exported Functions */
int mfg_write_macs(struct mac_addr_info_st *macs); 

/** This function either returns MAC adresses for all interfaces or it will fail 
    if there is problem or given platform has no way how to read them back

    - @retval 0 in case of success or -1 in case of error. (errno may be set)
 */
int mfg_read_macs(struct mac_addr_info_st *macs); 

#endif // #endof __MACS_H__

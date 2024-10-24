
#ifndef __MFGTOOLS_PRIVATE__
#define __MFGTOOLS_PRIVATE__

#include "macs.h"

#define SERIAL_LENGTH               (100)
#define WG_SERIAL_LENGTH            13
#define OEM_SERIAL_LENGTH           18

#define MFG_BLOCK_DATA_SIZE     ( 6 * 1024 )          // The size of the data entry for each block
#define MFG_MAGIC_HEADER_SIG    ( 0x12611920 )        // The magic number to look for
#define MFG_MAGIC_HEADER_CLIENT_AUTHSIG ( 0x12611925 )  // magic number to serve as CLIENT_AUTH version number (serves for both client_auth and client_auth_key)

/*! 
 * Each info block in the mfg partition has a header
 */
typedef struct {
    char checksum[16];              /*! This is the md5 checksum */
    int  length;                    /*! The length of the data to follow */
    int  signature;                 /*! The 'magic' signature */
} mfg_blk_hdr_st;


/*! 
 * Each info block are stored with a header + max 6K of data 
 */
typedef struct {
    mfg_blk_hdr_st hdr;             /*! Header  */
    char data[MFG_BLOCK_DATA_SIZE]; /*! Storage */
} mfg_info_blk_st;


/*!
 * Allthough this isn't used anywhere, it is used to show the layout of 
 * the manufactoring file (or partition), we use the offsetof macro to 
 * find the correct offset into the file
 * 
 * don't go bigger than 64k or you'll have to do some debugging....
 * (this is a little less than 33KB, 0x9060)
 *
 * This file is re-opened and data re-read, or written every time a call
 * requires it. This is because each entry may be written and updated in the 
 * mean time by other entities like httpd / ftpd. 
 *
 * TODO: Does this file/partition need some synchronization?
 */
typedef struct {
    mfg_info_blk_st serial;          /*! Serial Number */
    mfg_info_blk_st macs;            /*! HW addresses for network interfaces */
    mfg_info_blk_st client_auth;     /*! ssl authentication data */
    mfg_info_blk_st client_auth_key; /*! ssl authentication key */
} mfg_param_blk_st;

struct mfg_ctx_st;
typedef struct mfg_ctx_st mfg_ctx_t;

typedef enum {
    MFG_CONFIG_SERIAL  = 0, 
    MFG_CONFIG_MACS    = 1,
    MFG_CONFIG_CLIENT_AUTH  = 2,
    MFG_CONFIG_CLIENT_AUTH_KEY  = 3
} mfg_config_t;


#if 0
typedef struct mfg_ctx_st *(*mfg_config_init_fn)( void );

typedef void (*mfg_config_cleanup_fn) ( struct mfg_ctx_st *ctx );

typedef int (*mfg_config_read_fn)     ( struct mfg_ctx_st *ctx, 
        mfg_config_t cfg, char *data, int *length );

typedef int (*mfg_config_update_fn)   ( struct mfg_ctx_st *ctx, 
        mfg_config_t cfg, const char *data, int length );

typedef int (*mfg_config_have_fn)     ( struct mfg_ctx_st *ctx, 
        mfg_config_t cfg );

typedef int (*mfg_config_invalidate_fn) ( struct mfg_ctx_st *ctx, 
        mfg_config_t cfg );
#endif

typedef enum {
    MFG_SUCCESS         =  0,
    MFG_ERROR           = -1,
    MFG_BAD_CHECKSUM    = -2,
    MFG_BAD_MAGIC       = -3,
    MFG_BAD_SIZE        = -4,
    MFG_BAD_DEVICE      = -5
} mfg_return_t;

#if 0
typedef struct {
    mfg_config_init_fn       config_init;
    mfg_config_cleanup_fn    config_cleanup;
    mfg_config_read_fn       config_read;
    mfg_config_update_fn     config_update;
    mfg_config_have_fn       config_have;
    mfg_config_invalidate_fn config_invalidate;
} mfg_callback_st;
#endif

/*  struct to represet the Ethernet Chip and routine to read/extract HW MAC ADDRESS from that Ethernet Chip */
struct ether_mac_tools {

    const char *vendor;

    uint32_t  vendor_id;
    uint32_t  device_id;

    int (*fetch_mac) (unsigned char *mac);
};


#if 0
mfg_callback_st *mfg_config_funcs( void );

/* local platform exports: __attribute__ ((visibility("hidden"))) */
int platform_mfg_read_macs( struct mac_addr_info_st *macs );
int platform_mfg_write_macs( struct mac_addr_info_st *macs );
int platform_mfg_read_serial(char *serial, int len);
int platform_mfg_write_serial(char *serial);
int platform_mfg_read_oem_serial(char *serial, int len);
int platform_mfg_write_oem_serial(char *serial);
int mfg_valid_serial(const char *str);
#endif

/* local mfgtools exports: __attribute__ ((visibility("hidden"))) */
int mfg_valid_serial(const char *str);
int read_config_block( mfg_config_t cfg, mfg_info_blk_st *block );
int update_config_block(mfg_config_t cfg, char *mfg_block, char *data, int len);
int mfg_update_flashpart(mfg_config_t cfg, char *buff, int len);

#if 0
int mfg_read_config( mfg_ctx_t *ctx, mfg_config_t cfg, char *data, int *len );
int mfg_update_config( mfg_ctx_t *ctx, mfg_config_t cfg, const char *data, int length);
int mfg_invalidate_config( mfg_ctx_t *ctx, mfg_config_t cfg );
int mfg_have_config( mfg_ctx_t *ctx, mfg_config_t cfg );
void mfg_cleanup_config( mfg_ctx_t *ctx );
#endif

/*
 * stuff imported from __MFG_CONFIG_FLASHMEDIA__.h file
 * 
 */
#define DEFAULT_CONFIG_DEVICE			FLASH_DEV_CFG0_BLOCK
#define DEFAULT_FLASHMEDIA_CONFIGURATION_OFFSET	( 0x0 )

/*! 
 * \brief  
 * 
 * \vislibility Exported Data structure
 */
typedef struct cfg_media_info {
    char device[20];	/* device file "PATH/name" */
    int  base_offset;	/* base location of config params */

} cfg_media_info_st;

#if 0
/* Exported Functions */
mfg_callback_st *mfg_flashmedia_config_funcs( struct cfg_media_info *);
#endif





#endif // ififdef __MFGTOOLS_PRIVATE__

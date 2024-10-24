/*******************************************************************
 * $Id:
 *
 * @file mfgtols-dixie.c
 *
 * @brief Flash handling function for retreiving manufactoring data.
 *
 * @author Mukund Jampala
 * Copyright &copy; 2005, WatchGuard Technologies, Inc.
 * All Rights Reserved
 *
 * @date Thurday Feb 12, 2014
 *
 * @par Function:
 *     We store manufacturing data in fixed size structures that are
 *     stored in the flash config partition. The fixed size makes
 *     it very simple to read/write these and by NOT storing them
 *     in the data partition they will survive even if the data
 *     partition gets wiped out
 *******************************************************************/

#include <malloc.h>
#include <common.h>
#include <spi_flash.h>
#if 0
#include <u-boot/md5.h>
#endif

#include "mfgtools-private.h"
#include "md5.h"
#if 0
#include "mfg_config_flashmedia.h"
#endif

/*
 * -----------------------------------------------------------------------
 * Generic framework to support reading/writing sections of configuration
 * data to block media devices.
 * -----------------------------------------------------------------------
 */

/*! 
 * \brief Hidden data structure used for the config 
 */
typedef struct {
    int fd;			/*! file handler */
    int commit;                 /*! Commit changes back on close? */
    mfg_param_blk_st *mfg;      /*! Temporary storage for config data */
} mfg_ctx_st;

/*
 * Layered approach for reading config blocks out of /dev/wgrd.cfg0
 *
 * So the layered approach is a little complex, but it needs to be there
 * since writing the flash partition needs to be done in one go on chelan. 
 * And it may differ on Tacoma w/others. 
 *
 * We don't want too many writes, and when we update it then committing 
 * changes should only require one pass of write_paramblock(). On chelan
 * this read / write cycle to update the config takes ~3-4 seconds.
 *
 * On reads we should only read what we need to and nothing else. This is 
 * not how it worked in yavin where read_paramblock() was used to read the 
 * entire configuration. Here it will read the specified block, header and 
 * size of data stored there.
 *
 */

static int get_blockoffset( mfg_config_t cfg) 
{
    int retval = -1;
    
    switch (cfg) {
    case MFG_CONFIG_SERIAL:
        retval = offsetof(mfg_param_blk_st, serial);
        break;
    case MFG_CONFIG_MACS:
        retval = offsetof(mfg_param_blk_st, macs);
        break;
    case MFG_CONFIG_CLIENT_AUTH:
        retval = offsetof(mfg_param_blk_st, client_auth);
        break;
    case MFG_CONFIG_CLIENT_AUTH_KEY:
        retval = offsetof(mfg_param_blk_st, client_auth_key);
        break;
    default: /* Bail! */
        return retval;
    }

    return retval;
}

static struct spi_flash *mfg_flash;
char *mfg_block = NULL;


static int validate_config_block(mfg_config_t cfg, char *mfg_block, mfg_info_blk_st *block)
{
	int offset = get_blockoffset(cfg);
	int retval = 0;
	unsigned char chk[16];

#if 0
	int blk_size = sizeof( mfg_blk_hdr_st );
#endif

	if ( !mfg_block || offset == -1 ) {
		retval = -1;
		return retval;
	}

	/* make a copy */
	memmove(block, mfg_block + offset, sizeof(mfg_info_blk_st));

	/* Avoid endianess mess */
	block->hdr.signature = ntohl(block->hdr.signature);
	block->hdr.length    = ntohl(block->hdr.length   );
            
	/* 
	* Check signature 
	*/
	if ( block->hdr.signature != MFG_MAGIC_HEADER_SIG ) {
		printf ("**Error: Invalid magic number");
		retval = -2;
	}

	if (retval != 0) 
		return (retval);

	/* 
	* Final check: the checksum!
	*/
	wgut_md5buf(chk, block->data, block->hdr.length);
	if (memcmp(chk, block->hdr.checksum, 16)) {
		printf ("**Error: Checksum failure in data segment \
			of ");
		return MFG_BAD_CHECKSUM;
	}

	/* Null Terminate */
	*(block->data + block->hdr.length) = '\0';
	block->hdr.length++;

	return (retval);
}

int read_config_block( mfg_config_t cfg, mfg_info_blk_st *block ) 
{
	int ret = 1;
	u32 saved_size, saved_offset;

	int mfg_blk_size = sizeof(mfg_param_blk_st);
#if 0
	char	*res;
	ssize_t	len;
#endif

	if(!block)
		return 1;

	mfg_flash = spi_flash_probe(CONFIG_ENV_SPI_BUS,
		CONFIG_ENV_SPI_CS,
		CONFIG_ENV_SPI_MAX_HZ, CONFIG_ENV_SPI_MODE);

	/* Is the sector larger than the mfg_blk_size */
	if (CONFIG_MFG_SECT_SIZE > mfg_blk_size) {
		saved_size = CONFIG_MFG_SECT_SIZE - mfg_blk_size;
		saved_offset = CONFIG_MFG_OFFSET + mfg_blk_size;
		mfg_block = malloc(saved_size);
		if (!mfg_block) {
			goto done;
		}
		ret = spi_flash_read(mfg_flash, saved_offset,
			saved_size, mfg_block);
		if (ret)
			goto done;
	}

	/* update should happen here */
	validate_config_block(cfg, mfg_block, block);

	ret = 0;
	puts("done\n");

 done:
	if (mfg_block)
		free(mfg_block);
	return ret;
}

int update_config_block(mfg_config_t cfg, char *mfg_block, char *data, int len)
{
	int offset = get_blockoffset(cfg);
	int retval = (-1);
    
	/* limit the lenght to max datalength supported */
	if (len > MFG_BLOCK_DATA_SIZE)
		return MFG_BAD_SIZE;

        if ( !mfg_block )
		return 1;

	mfg_info_blk_st *block =
		(mfg_info_blk_st*) ((char*)mfg_block + offset);

	/* Update the signature in the header */
	block->hdr.length    = htonl( len );
	block->hdr.signature = htonl( MFG_MAGIC_HEADER_SIG );

	/* Update data + checksum */
	memmove( block->data, data, len );
	wgut_md5buf( (unsigned char*)block->hdr.checksum, data, len );

	retval      = 0;
	return retval;
}

/*!
 * \brief This function updates the flashpart with new data
 * \param rawdev    [IN]    The raw data partition, e.g. "/dev/wgrd.sysa.kernel
 * \param mfg_blk   [OUT]   Dat Buffer to be stored
 * \param len       [IN]    The length of the buffer 'mfg_blk' to be stored
 *
 * \return 0 on success, -1 otherwise
 */
int mfg_update_flashpart(mfg_config_t cfg, char *mfg_blk, int len)
{
	int ret = 1;
	u32 saved_size, saved_offset, sector = 1;

	int mfg_blk_size = sizeof(mfg_param_blk_st);
#if 0
	char	*res;
#endif

	mfg_flash = spi_flash_probe(CONFIG_ENV_SPI_BUS,
		CONFIG_ENV_SPI_CS,
		CONFIG_ENV_SPI_MAX_HZ, CONFIG_ENV_SPI_MODE);

	/* Is the sector larger than the mfg_blk_size */
	if (CONFIG_MFG_SECT_SIZE > mfg_blk_size) {
		saved_size = CONFIG_MFG_SECT_SIZE - mfg_blk_size;
		saved_offset = CONFIG_MFG_OFFSET + mfg_blk_size;
		mfg_block = malloc(saved_size);
		if (!mfg_block) {
			goto done;
		}
		ret = spi_flash_read(mfg_flash, saved_offset,
			saved_size, mfg_block);
		if (ret)
			goto done;
	}

	if (mfg_blk_size > CONFIG_MFG_SECT_SIZE) {
		sector = mfg_blk_size / CONFIG_MFG_SECT_SIZE;
		if (mfg_blk_size % CONFIG_MFG_SECT_SIZE)
			sector++;
	}

	/* update should happen here */
	update_config_block(cfg, mfg_block, mfg_blk, len);

	puts("Erasing MFG portion of SPI flash...");
	ret = spi_flash_erase(mfg_flash, CONFIG_MFG_OFFSET,
		sector * CONFIG_MFG_SECT_SIZE);
	if (ret)
		goto done;

	puts("Writing MFG portion of SPI flash...");
	ret = spi_flash_write(mfg_flash, CONFIG_MFG_OFFSET,
		mfg_blk_size, mfg_block);
	if (ret)
		goto done;

	if (CONFIG_MFG_SECT_SIZE > mfg_blk_size) {
		ret = spi_flash_write(mfg_flash, saved_offset,
			saved_size, mfg_block);
		if (ret)
			goto done;
	}

	ret = 0;
	puts("done\n");

 done:
	if (mfg_block)
		free(mfg_block);
	return ret;
}


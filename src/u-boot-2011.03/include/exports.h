#ifndef __EXPORTS_H__
#define __EXPORTS_H__

#ifndef __ASSEMBLY__

#include <common.h>
#if defined (CONFIG_SERCOMM_MFG)
#include <nand.h>
#include "../drivers/rtc/s35390.h"
#endif

/* These are declarations of exported functions available in C code */
unsigned long get_version(void);
int  getc(void);
int  tstc(void);
void putc(const char);
void puts(const char*);
int printf(const char* fmt, ...);
void install_hdlr(int, interrupt_handler_t*, void*);
void free_hdlr(int);
void *malloc(size_t);
void free(void*);
void __udelay(unsigned long);
unsigned long get_timer(unsigned long);
int vprintf(const char *, va_list);
unsigned long simple_strtoul(const char *cp,char **endp,unsigned int base);
char *getenv (char *name);
int setenv (char *varname, char *varvalue);
long simple_strtol(const char *cp,char **endp,unsigned int base);
int strcmp(const char * cs,const char * ct);
int ustrtoul(const char *cp, char **endp, unsigned int base);
#if defined(CONFIG_CMD_I2C)
int i2c_write (uchar, uint, int , uchar* , int);
int i2c_read (uchar, uint, int , uchar* , int);
#endif
#if defined (CONFIG_SERCOMM_MFG)
int do_go (cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[]);
void * memcpy(void *,const void *,__kernel_size_t);
void pciinfo(int, int);
unsigned long long get_ticks(void);
void * memset(void *,int,__kernel_size_t);
int nand_erase_opts(nand_info_t *meminfo, const nand_erase_options_t *opts);
int nand_read_skip_bad(nand_info_t *nand, loff_t offset, size_t *length,
		       u_char *buffer);
int nand_write_skip_bad(nand_info_t *nand, loff_t offset, size_t *length,
			u_char *buffer, int withoob);
int flash_sect_protect(int flag, ulong addr_first, ulong addr_last);
int flash_write(char *, ulong, ulong);
int flash_sect_erase(ulong addr_first, ulong addr_last);
ulong usec2ticks(unsigned long usec);
void flush_dcache(void);
void invalidate_icache(void);
void icache_disable(void);
void dcache_disable(void);
int memcmp(const void *,const void *,__kernel_size_t);
ulong get_tbclk(void);
nand_info_t *sc_get_nand_info_ptr(void);
void sc_i2c_init(int speed, int slaveadd);
void sc_rtc_init(void);
int sc_rtc_get_time(SC_RTC_TIME * time);
int sc_rtc_set_time(SC_RTC_TIME * time);
#endif
#include <spi.h>

void app_startup(char * const *);

#endif    /* ifndef __ASSEMBLY__ */

enum {
#define EXPORT_FUNC(x) XF_ ## x ,
#include <_exports.h>
#undef EXPORT_FUNC

	XF_MAX
};

#define XF_VERSION	6

#if defined(CONFIG_I386)
extern gd_t *global_data;
#endif

#endif	/* __EXPORTS_H__ */

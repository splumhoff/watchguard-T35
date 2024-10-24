#ifndef _RTC_S35390_H_
#define _RTC_S35390_H_

/**************************************************************************
 * 	This is for RTC Chip Seiko S-35390 definitions
 **************************************************************************/
struct i2c_msg {
        unsigned short addr;     /* slave address                        */
        unsigned short flags;
#define I2C_M_TEN               0x0010  /* this is a ten bit chip address */
#define I2C_M_RD                0x0001  /* read data, from slave to master */
#define I2C_M_NOSTART           0x4000  /* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_REV_DIR_ADDR      0x2000  /* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_IGNORE_NAK        0x1000  /* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_NO_RD_ACK         0x0800  /* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_RECV_LEN          0x0400  /* length will be first received byte */
        unsigned short len;              /* msg length                           */
        unsigned char *buf;              /* pointer to msg data                  */
};
/* dev code */
#define RTC_DEV_S35390 (0x6)

/* register */
#define CMD_STATUS_1ST	(0x0)
#define CMD_STATUS_2ND	(0x1)
#define CMD_TIME_YEAR	(0x2)
#define CMD_TIME_HOUR	(0x3)
#define CMD_INTREG_1ST	(0x4)
#define CMD_INTREG_2ND	(0x5)
#define CMD_CLK_CORRECT	(0x6)
#define CMD_FREE_REG	(0x7)

/* status flag */
#define S35390A_FLAG_POC	0x01
#define S35390A_FLAG_BLD	0x02
#define S35390A_FLAG_24H	0x40
#define S35390A_FLAG_RESET	0x80
#define S35390A_FLAG_TEST	0x01

/* address shift */
#define RTC_DEV_ADDR_SHIFT	3
#define RTC_CMD_ADDR_SHIFT	0

/* Settings */
#define RTC_STAT_1_RST_CHIP	(0x80)
#define RTC_STAT_1_EXPR_24H	(0x40)

#define RTC_STAT_2_TESTMODE				(0x01)
#define RTC_STAT_2_INT2_USER_SET_FREQ	(0x08)
#define RTC_STAT_2_INT2_PER_MIN_EDGE	(0x04)
#define RTC_STAT_2_INT2_MIN_PERIODICAL	(0x0C)
#define RTC_STAT_2_INT2_ALARM2			(0x02)
#define RTC_STAT_2_INT1_USER_SET_FREQ	(0x80)
#define RTC_STAT_2_INT1_PER_MIN_EDGE	(0x40)
#define RTC_STAT_2_INT1_MIN_PERIODICAL1	(0xC0)
#define RTC_STAT_2_INT1_ALARM1			(0x20)
#define RTC_STAT_2_INT1_MIN_PERIODICAL2	(0xE0)
#define RTC_STAT_2_32KE					(0x10)

#define RTC_TIME_YEAR_LEN		(0x7)

#define RTC_OP_OK				(0x0)
#define RTC_OP_FAIL				(0x1)

typedef struct
{
	unsigned char year;
	unsigned char month;
	unsigned char day;
	unsigned char week;
	unsigned char hour;
	unsigned char min;
	unsigned char sec;
} SC_RTC_TIME;

#define swap_bits_in_char(x)  (((x&0x01) <<7) | \
					 ((x&0x02) <<5) | \
					 ((x&0x04) <<3) | \
					 ((x&0x08) <<1) | \
					 ((x&0x10) >>1) | \
					 ((x&0x20) >>3) | \
					 ((x&0x40) >>5) | \
					 ((x&0x80) >>7) )

/* Function Protypes */
void sc_i2c_init(int speed, int slaveadd);
void sc_rtc_init(void);
int sc_rtc_get_time(SC_RTC_TIME * time);
int sc_rtc_set_time(SC_RTC_TIME * time);

int s35390a_get_reg(struct fsl_i2c *i2c, int reg, char *buf, int len);
int s35390a_set_reg(struct fsl_i2c *i2c, int reg, char *buf, int len);





#endif /* _RTC_S35390_H_ */


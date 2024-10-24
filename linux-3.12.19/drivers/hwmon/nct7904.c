/*
    nct7904.c - Linux kernel driver for hardware monitoring
    Copyright (C) 2008 Nuvoton Technology Corp.
                  Wei Song

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation - version 2.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301 USA.


    Supports following chips:

    Chip       #vin   #fanin #temp #dts wchipid  			vendid  i2c  ISA
    nct7904d    17     12    5     8    0x79 or 0xC5     0x50    yes   no
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/hwmon-vid.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/delay.h>

/* Addresses to scan */
static unsigned short normal_i2c[] = { 0x2e, I2C_CLIENT_END };


I2C_CLIENT_INSMOD_1(nct7904);


/* Insmod parameters */
static unsigned short force_subclients[4];
module_param_array(force_subclients, short, NULL, 0);
MODULE_PARM_DESC(force_subclients, "List of subclient addresses: "
		       "{bus, clientaddr, subclientaddr1, subclientaddr2}");



static int reset;
module_param(reset, bool, 0);
MODULE_PARM_DESC(reset, "Set to 1 to reset chip, not recommended");

#define FIX_BANK_PROBLEM	/* Define it to cover bank bug of early nct7904 */

#define NCT7904_REG_BANK_MASK	 0x7
#define NCT7904_REG_BANKSEL    0xFF
#define NCT7904_REG_VENDORID   0xD
#define NCT7904_REG_CHIPID     0xE
#define NCT7904_REG_DEVICEID   0xF
#define NCT7904_REG_VENDORID2   0x7A
#define NCT7904_REG_CHIPID2     0x7B
#define NCT7904_REG_DEVICEID2   0x7C

#define NCT7904_REG_I2C_ADDR          	0xC
#define NCT7904_REG_GLOBAL_CONTROL  0x00


/* Multi-Function Pin Ctrl Registers */
#define NCT7904_REG_VOLT_CTRL1   0x20
#define NCT7904_REG_VOLT_CTRL2   0x21
#define NCT7904_REG_VOLT_CTRL3   0x22

#define NCT7904_REG_FANIN_CTRL1  0x24
#define NCT7904_REG_FANIN_CTRL2  0x25


#define NCT7904_REG_VOLT_TEMP_CTRL	0x2E
#define VAL_VOLT_TEMP_CTRL_VOLT_MONITOR	0x0  /* value definition in NCT7904_REG_VOLT_TEMP_CTRL */
#define VAL_VOLT_TEMP_CTRL_DIODE_CURRENT	0x1
#define VAL_VOLT_TEMP_CTRL_DIODE_VOLT		0x2
#define VAL_VOLT_TEMP_CTRL_THERMISTOR		0x3
/* shift bits of TR1~TR4 in NCT7904_REG_VOLT_TEMP_CTRL */
static u16 NCT7904_TEMP_CTRL_SHIFT[] = {0,2,4,6};



#define TEMP_READ       0
#define TEMP_CRIT       1
#define TEMP_CRIT_HYST  2
#define TEMP_WARN       3
#define TEMP_WARN_HYST  4
#define TEMP_LSB_MASK	0x7
/* only crit and crit_hyst affect real-time alarm status.
   Feild: current, crit, crit_hyst, warn, warn_hyst 
   Attention: Current reading has decimal fraction. */
static u16 NCT7904_REG_TEMP[][5] = {
	{0x42, 0x104, 0x105, 0x106, 0x107}, // TR1/TD1
	{0x46, 0x10C, 0x10D, 0x10E, 0x10F}, // TR2/TD2
	{0x4A, 0x114, 0x115, 0x116, 0x117}, // TR3
	{0x4E, 0x11C, 0x11D, 0x11E, 0x11F}, // TR4
	{0x62, 0x144, 0x145, 0x146, 0x147}, // LTD
};

#define NCT7904_REG_TEMP_LSB(index)	(NCT7904_REG_TEMP[index][TEMP_READ] + 1)

#define IN_READ				0
#define IN_MAX					1
#define IN_LOW					2
static const u16 NCT7904_REG_IN[][3] = {
	/* Current, HL, LL */
	{0x40, 0x100, 0x102},	/* VSEN1	*/
	{0x42, 0x104, 0x106},	/* VSEN2	*/
	{0x44, 0x108, 0x10A},	/* VSEN3	*/
	{0x46, 0x10C, 0x10E},	/* VSEN4	*/
	{0x48, 0x110, 0x112},	/* VSEN5	*/
	{0x4A, 0x114, 0x116},	/* VSEN6	*/
	{0x4C, 0x118, 0x11A},	/* VSEN7	*/
	{0x4E, 0x11C, 0x11E},	/* VSEN8	*/
	{0x50, 0x120, 0x122},	/* VSEN9	*/
	{0x52, 0x124, 0x126},	/* VSEN10*/
	{0x54, 0x128, 0x12A},	/* VSEN11*/
	{0x56, 0x12C, 0x12E},	/* VSEN12*/
	{0x58, 0x130, 0x132},	/* VSEN13*/
	{0x5A, 0x134, 0x136},	/* VSEN14*/
	{0x5C, 0x138, 0x13A},	/* 3VDD	*/
	{0x5E, 0x13C, 0x13E},	/* VBAT	*/
	{0x60, 0x140, 0x142},	/* 3VSB	*/
									/* LTD is a temperature sensor	*/
};

#define IN_LSB_REG(index)	(NCT7904_REG_IN[index][IN_READ]+1)
#define IN_HL_LSB_REG(index)	(NCT7904_REG_IN[index][IN_MAX]+1)
#define IN_LL_LSB_REG(index)	(NCT7904_REG_IN[index][IN_LOW]+1)
#define IN_LSB_MASK	0x7

u8 IN_MULTIPLIER[] = { 2, 2, 2, 2, 2, 
			2, 2, 2, 2, 2, 
			2, 2, 2, 2, 6,
			6, 6};




#define NCT7904_REG_FAN(index)    (0x80 + (index)*2 )
//lanner++
#define NCT7904_REG_FOV(index)    (0x310 + index )
#define NCT7904_REG_TFMR(index)    (0x300 + index )
//lanner--
#define NCT7904_REG_FAN_LSB(index)    (0x81 + (index)*2 )
#define NCT7904_REG_FAN_MIN(index)     (0x160 + (index)*2 )
#define NCT7904_REG_FAN_MIN_LSB(index)    (0x161 + (index)*2 )
#define NCT7904_FAN_LSB_MASK	0x1F

#define ALARM_REG_NUM        10
#define NCT7904_REG_ALARM(index)   (0xC1 + (index))



#define NCT7904_REG_PECI_ENABLE	0x200
#define NCT7904_REG_TSI_CTRL		0x250

#define NCT7904_REG_DTSC0			0x26
#define NCT7904_REG_DTSC1			0x27
/*
#define W83795_REG_DTSE                0x302
#define W83795_REG_DTS(index)          (0x26 + (index))
*/
#define DTS_READ        0
#define DTS_CRIT        1
#define DTS_CRIT_HYST   2
#define DTS_WARN        3
#define DTS_WARN_HYST   4
#define DTS_LSB_MASK		0x7
static u16 NCT7904_REG_DTS[][5] = {
	{0xA0, 0x190, 0x191, 0x192, 0x193},
	{0xA2, 0x194, 0x195, 0x196, 0x197},
	{0xA4, 0x198, 0x199, 0x19A, 0x19B},
	{0xA6, 0x19C, 0x19D, 0x19E, 0x19F},
	{0xA8, 0x1A0, 0x1A1, 0x1A2, 0x1A3},
	{0xAA, 0x1A4, 0x1A5, 0x1A6, 0x1A7},
	{0xAC, 0x1A8, 0x1A9, 0x1AA, 0x1AB},
	{0xAE, 0x1AC, 0x1AD, 0x1AE, 0x1AF},
};
#define NCT7904_REG_DTS_LSB(index)	(NCT7904_REG_DTS[index][DTS_READ] + 1)

	




static inline u16 IN_FROM_REG(u8 index, u16 val)
{
	return (val * IN_MULTIPLIER[index]);
}

static inline u16 IN_TO_REG(u8 index, u16 val)
{
	return (val / IN_MULTIPLIER[index]);
}

static inline unsigned long FAN_FROM_REG(u16 val)
{
	if ((val >= 0x1fff) || (val == 0))
		return	0;
	return (1350000UL / val);
}

static inline u16 FAN_TO_REG(long rpm)
{
	if (rpm <= 0)
		return 0x1fff;
	return SENSORS_LIMIT((1350000 + (rpm >> 1)) / rpm, 1, 0x1fff);
}

static inline unsigned long TIME_FROM_REG(u8 reg)
{
	return (reg * 100);
}

static inline u8 TIME_TO_REG(unsigned long val)
{
	return SENSORS_LIMIT((val + 50) / 100, 0, 0xff);
}

/* For 8-bit 2's complement integer portion */
static inline long TEMP_FROM_REG(s8 reg)
{
	return (reg * 1000);
}


/* For limitation */
static inline s8 TEMP_TO_REG(long val, s8 min, s8 max)
{
//	return SENSORS_LIMIT((val < 0 ? -val : val) / 1000, min, max);
	return SENSORS_LIMIT(val / 1000, min, max);
}


enum chip_types {nct7904d};  //For chip types in nct7904_data

struct nct7904_data {
	struct device *hwmon_dev;
	struct mutex update_lock;
	unsigned long last_updated;	/* In jiffies */
	enum chip_types chip_type; /* For recording what the chip is */ 

	u8 bank;
	
//	u8 vrm;
//	u8 vid[3];
//	u8 has_vid;    /* Enable monitor VID or not, affected by CR 0x6A */

	u32 has_in;    /* Enable monitor VIN or not, affected by CR 0x02, 0x03 */
	u16 in[17][3];		/* Register value, [VSEN1~14,3VDD,VBAT,3VSB][read/high/low] */

//	u8 has_gain;        /* has gain: in17 -in20 * 8 */

	u16 has_fan;		/* Enable fan1-14, 3VDD, VBAT, 3VSB */
	u16 fan[12];		/* Register value combine */
	u16 fan_min[12];	/* Register value combine */
//lanner++
	u16 fov[4];		/* Fan Output Value */
	u16 tfmr[4];		/* Temperature to Fan Mapping Relationships */
//lanner--

	u8 has_temp;      /* Enable monitor temp4-1 or not, affected by CR 0x04, 0x05 */
	u8 temp[5][5];		/* [ch1~ch4,LTD][current, crit, crit_hyst, warn, warn_hyst] */
	u8 temp_read_lsb[5]; /* The LSB value corresponded to temp[][TEMP_READ] */
	u8 temp_mode;	/* 0: TR mode, 1: TD mode */


	u8 enable_dts;    /* Enable PECI and SB-TSI, 
	* bit 0: =1 enable, =0 disable , 
	* bit 1: =1 AMD SB-TSI, =0 Intel PECI */
	u8 has_dts;      /* Enable monitor DTS temp: T_CPU1~T_CPU8 */
	u8 dts[8][5];       /* [DTS_CPU1~DTS_CPU8][current, crit, crit_hyst, warn, warn_hyst] */
	u8 dts_read_lsb[8];  /* The LSB value corresponded to dts[][DTS_READ] */

 

	u8 alarms[10];     /* Register value */



	char valid;
};

static u8 nct7904_read_value(struct i2c_client *client, u16 reg);
static int nct7904_write_value(struct i2c_client *client, u16 reg, u8 value);
static int nct7904_probe(struct i2c_client *client,
			const struct i2c_device_id *id);
//static int nct7904_detect(struct i2c_client *client, int kind,
static int nct7904_detect(struct i2c_client *client,
			 struct i2c_board_info *info);
static int nct7904_remove(struct i2c_client *client);

static void nct7904_init_client(struct i2c_client *client);
static struct nct7904_data *nct7904_update_device(struct device *dev);

static const struct i2c_device_id nct7904_id[] = {
//	{ "nct7904", nct7904 },
	{ "nct7904", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, nct7904_id);

static struct i2c_driver nct7904_driver = {
	.class		= I2C_CLASS_HWMON,
	.driver = {
		   .name = "nct7904",
	},
	.probe		= nct7904_probe,
	.remove		= nct7904_remove,
	.id_table	= nct7904_id,
	.detect		= nct7904_detect,
//   .address_data   = &addr_data,
	.address_list	= normal_i2c,
};


/*
static ssize_t
show_vid(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct w83795_data *data = w83795_update_device(dev);
	struct sensor_device_attribute_2 *sensor_attr =
	    to_sensor_dev_attr_2(attr);
	int nr = sensor_attr->nr;

	return sprintf(buf, "%d\n", vid_from_reg(data->vid[nr], data->vrm));
}
*/


#define ALARM_STATUS      0
//#define BEEP_ENABLE       1
#define ALARM_DTS_STATUS	2

static ssize_t
show_alarm_beep(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct nct7904_data *data = nct7904_update_device(dev);
	struct sensor_device_attribute_2 *sensor_attr =
	    to_sensor_dev_attr_2(attr);
	int nr = sensor_attr->nr;
	int index = sensor_attr->index >> 3;
	int bit = sensor_attr->index & 0x07;
	u8 val;

	if (ALARM_DTS_STATUS == nr){ //For 7904 DTS alarm
		if (sensor_attr->index < 4){
			val = (data->alarms[6] >> (bit)) & 1;
		}
		else{
			val = (data->alarms[7] >> (bit-4)) & 1;
		}
	}
	else{
		val = (data->alarms[index] >> (bit)) & 1;
	}


	return sprintf(buf, "%u\n", val);
}


#define FAN_INPUT     0
#define FAN_MIN       1
static ssize_t
show_fan(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sensor_device_attribute_2 *sensor_attr =
	    to_sensor_dev_attr_2(attr);
	int nr = sensor_attr->nr;
	int index = sensor_attr->index;
	struct nct7904_data *data = nct7904_update_device(dev);
	u16 val;

	if (FAN_INPUT == nr) {
		val = data->fan[index] & 0x1fff;
	} else {
		val = data->fan_min[index] & 0x1fff;
	}

	return sprintf(buf, "%lu\n", FAN_FROM_REG(val));
}
//lanner++
static ssize_t
show_fov(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sensor_device_attribute_2 *sensor_attr =
	    to_sensor_dev_attr_2(attr);
	int index = sensor_attr->index;
	struct nct7904_data *data = nct7904_update_device(dev);

	return sprintf(buf, "%d\n", data->fov[index]);
}
static ssize_t
store_fov(struct device *dev, struct device_attribute *attr,
	      const char *buf, size_t count)
{
	struct sensor_device_attribute_2 *sensor_attr =
	    to_sensor_dev_attr_2(attr);
	int index = sensor_attr->index;
	struct i2c_client *client = to_i2c_client(dev);
	struct nct7904_data *data = i2c_get_clientdata(client);
	u16 val = simple_strtoul(buf, NULL, 10);

	mutex_lock(&data->update_lock);
	data->fov[index] = val;
	nct7904_write_value(client, NCT7904_REG_FOV(index), val&0xff);
	mutex_unlock(&data->update_lock);

	return count;
}
static ssize_t
show_tfmr(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sensor_device_attribute_2 *sensor_attr =
	    to_sensor_dev_attr_2(attr);
	int index = sensor_attr->index;
	struct nct7904_data *data = nct7904_update_device(dev);

	return sprintf(buf, "%d\n", data->tfmr[index]);
}
static ssize_t
store_tfmr(struct device *dev, struct device_attribute *attr,
	      const char *buf, size_t count)
{
	struct sensor_device_attribute_2 *sensor_attr =
	    to_sensor_dev_attr_2(attr);
	int index = sensor_attr->index;
	struct i2c_client *client = to_i2c_client(dev);
	struct nct7904_data *data = i2c_get_clientdata(client);
	u16 val = simple_strtoul(buf, NULL, 10);

	mutex_lock(&data->update_lock);
	data->tfmr[index] = val;
	nct7904_write_value(client, NCT7904_REG_TFMR(index), val&0xff);
	mutex_unlock(&data->update_lock);

	return count;
}
//lanner--

static ssize_t
store_fan_min(struct device *dev, struct device_attribute *attr,
	      const char *buf, size_t count)
{
	struct sensor_device_attribute_2 *sensor_attr =
	    to_sensor_dev_attr_2(attr);
	int index = sensor_attr->index;
	struct i2c_client *client = to_i2c_client(dev);
	struct nct7904_data *data = i2c_get_clientdata(client);
	u16 val = FAN_TO_REG(simple_strtoul(buf, NULL, 10));

	mutex_lock(&data->update_lock);
	data->fan_min[index] = val;
	nct7904_write_value(client, NCT7904_REG_FAN_MIN(index),
			   (val >> 5) & 0xff);

	nct7904_write_value(client, NCT7904_REG_FAN_MIN_LSB(index), val & NCT7904_FAN_LSB_MASK);
	mutex_unlock(&data->update_lock);

	return count;
}



/* Only for storing integer portion */
static ssize_t
show_temp(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sensor_device_attribute_2 *sensor_attr =
	    to_sensor_dev_attr_2(attr);
	int nr = sensor_attr->nr;
	int index = sensor_attr->index;
	struct nct7904_data *data = nct7904_update_device(dev);
	long temp = TEMP_FROM_REG(data->temp[index][nr]);

	if (TEMP_READ == nr){
		temp += (data->temp_read_lsb[index] & TEMP_LSB_MASK) * 125;
	}

	return sprintf(buf, "%ld\n", temp);
}

static ssize_t
store_temp(struct device *dev, struct device_attribute *attr,
	   const char *buf, size_t count)
{
	struct sensor_device_attribute_2 *sensor_attr =
	    to_sensor_dev_attr_2(attr);
	int nr = sensor_attr->nr;
	int index = sensor_attr->index;
	struct i2c_client *client = to_i2c_client(dev);
	struct nct7904_data *data = i2c_get_clientdata(client);
	long tmp = simple_strtol(buf, NULL, 10);

	mutex_lock(&data->update_lock);
	data->temp[index][nr] = TEMP_TO_REG(tmp, -128, 127);
	nct7904_write_value(client, NCT7904_REG_TEMP[index][nr],
			   data->temp[index][nr]);
	mutex_unlock(&data->update_lock);
	return count;
}


static ssize_t
show_dts_mode(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct nct7904_data *data = i2c_get_clientdata(client);
	struct sensor_device_attribute_2 *sensor_attr =
	    to_sensor_dev_attr_2(attr);
	int index = sensor_attr->index;
	u8 tmp;

	if (data->enable_dts == 0)
		return sprintf(buf, "%d\n", 0);
	
	if ((data->has_dts >> index) & 0x01) {
		if (data->enable_dts & 2)
			tmp = 5; //TSI
		else
			tmp = 6; //PECI
	} else {
		tmp = 0;
	}

	return sprintf(buf, "%d\n", tmp);
}



static ssize_t
show_dts(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sensor_device_attribute_2 *sensor_attr =
	    to_sensor_dev_attr_2(attr);
	int nr = sensor_attr->nr;
	int index = sensor_attr->index;
	struct nct7904_data *data = nct7904_update_device(dev);
	long temp = TEMP_FROM_REG(data->dts[index][nr]);
	
	if (DTS_READ == nr){
		temp += (data->dts_read_lsb[index] & DTS_LSB_MASK) * 125;
	}

	return sprintf(buf, "%ld\n", temp);
}


/* Only for storing integer portion */
static ssize_t
store_dts(struct device *dev, struct device_attribute *attr,
	   const char *buf, size_t count)
{
	struct sensor_device_attribute_2 *sensor_attr =
	    to_sensor_dev_attr_2(attr);
	int nr = sensor_attr->nr;
	int index = sensor_attr->index;
	struct i2c_client *client = to_i2c_client(dev);
	struct nct7904_data *data = i2c_get_clientdata(client);
	long tmp = simple_strtol(buf, NULL, 10);

	mutex_lock(&data->update_lock);
	data->dts[index][nr] = TEMP_TO_REG(tmp, -128, 127);
	nct7904_write_value(client, NCT7904_REG_DTS[index][nr],
			   data->dts[index][nr]);
	mutex_unlock(&data->update_lock);
	return count;
}

/*
	Type 3:  Thermal diode
  Type 4:  Thermistor
*/
static ssize_t
show_temp_mode(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct nct7904_data *data = i2c_get_clientdata(client);
	struct sensor_device_attribute_2 *sensor_attr =
	    to_sensor_dev_attr_2(attr);
	int index = sensor_attr->index;
	u8 tmp;

	if ((data->has_temp >> index) & 0x01) {
		if ((data->temp_mode >> index) & 0x01) {
			tmp = 3;	//TD			
		} else {
			tmp = 4;	//TR
		}
	} else {
		tmp = 0;
	}

	return sprintf(buf, "%d\n", tmp);
}


/* show/store VIN */
static ssize_t
show_in(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sensor_device_attribute_2 *sensor_attr =
	    to_sensor_dev_attr_2(attr);
	int nr = sensor_attr->nr;
	int index = sensor_attr->index;
	struct nct7904_data *data = nct7904_update_device(dev);
	u16 val = data->in[index][nr];
	

	val = IN_FROM_REG(index, val);

	return sprintf(buf, "%d\n", val);
}

static ssize_t
store_in(struct device *dev, struct device_attribute *attr,
	 const char *buf, size_t count)
{
	struct sensor_device_attribute_2 *sensor_attr =
	    to_sensor_dev_attr_2(attr);
	int nr = sensor_attr->nr;
	int index = sensor_attr->index;
	struct i2c_client *client = to_i2c_client(dev);
	struct nct7904_data *data = i2c_get_clientdata(client);
	u16 val = IN_TO_REG(index, simple_strtoul(buf, NULL, 10));
	u8 tmp;
	
	val = SENSORS_LIMIT(val, 0, 0x7FF);
	mutex_lock(&data->update_lock);

	switch (nr){
	case IN_MAX:
		tmp = nct7904_read_value(client, IN_HL_LSB_REG(index));
		tmp &= ~IN_LSB_MASK;
		tmp |= val & IN_LSB_MASK;
		nct7904_write_value(client, IN_HL_LSB_REG(index), tmp);
		break;
	case IN_LOW:
		tmp = nct7904_read_value(client, IN_LL_LSB_REG(index));
		tmp &= ~IN_LSB_MASK;
		tmp |= val & IN_LSB_MASK;
		nct7904_write_value(client, IN_LL_LSB_REG(index), tmp);
		break;
	}

	tmp = (val >> 3) & 0xff;
	nct7904_write_value(client, NCT7904_REG_IN[index][nr], tmp);

	data->in[index][nr] = val;

	mutex_unlock(&data->update_lock);
	return count;
}



#define NOT_USED			-1

#define SENSOR_ATTR_IN(index)		\
	SENSOR_ATTR_2(in##index##_input, S_IRUGO, show_in, NULL,	\
		IN_READ, index), \
	SENSOR_ATTR_2(in##index##_max, S_IRUGO | S_IWUSR, show_in,	\
		store_in, IN_MAX, index),		\
	SENSOR_ATTR_2(in##index##_min, S_IRUGO | S_IWUSR, show_in,	\
		store_in, IN_LOW, index),	\
	SENSOR_ATTR_2(in##index##_alarm, S_IRUGO, show_alarm_beep,	\
		NULL, ALARM_STATUS,	index)




#define SENSOR_ATTR_FAN(index)						\
	SENSOR_ATTR_2(fan##index##_input, S_IRUGO, show_fan,		\
		NULL, FAN_INPUT, index - 1), \
	SENSOR_ATTR_2(fan##index##_min, S_IWUSR | S_IRUGO,		\
		show_fan, store_fan_min, FAN_MIN, index - 1),	\
	SENSOR_ATTR_2(fan##index##_alarm, S_IRUGO, show_alarm_beep,	\
		NULL, ALARM_STATUS, index + 31)

#define SENSOR_ATTR_DTS(index)						\
	SENSOR_ATTR_2(temp##index##_type, S_IRUGO ,		\
		show_dts_mode, NULL, NOT_USED, index - 6),	\
	SENSOR_ATTR_2(temp##index##_input, S_IRUGO, show_dts,		\
		NULL, DTS_READ, index - 6),				\
	SENSOR_ATTR_2(temp##index##_max, S_IRUGO | S_IWUSR, show_dts,	\
		store_dts, DTS_CRIT, index - 6),			\
	SENSOR_ATTR_2(temp##index##_max_hyst, S_IRUGO | S_IWUSR,	\
		show_dts, store_dts, DTS_CRIT_HYST, index - 6),	\
	SENSOR_ATTR_2(temp##index##_warn, S_IRUGO | S_IWUSR, show_dts,	\
		store_dts, DTS_WARN, index - 6),			\
	SENSOR_ATTR_2(temp##index##_warn_hyst, S_IRUGO | S_IWUSR,	\
		show_dts, store_dts, DTS_WARN_HYST, index - 6),	\
	SENSOR_ATTR_2(temp##index##_alarm, S_IRUGO,			\
		show_alarm_beep, NULL, ALARM_DTS_STATUS, index - 6)
		

#define SENSOR_ATTR_TEMP(index)						\
	SENSOR_ATTR_2(temp##index##_type, S_IRUGO,		\
		show_temp_mode, NULL, NOT_USED, index - 1),	\
	SENSOR_ATTR_2(temp##index##_input, S_IRUGO, show_temp,		\
		NULL, TEMP_READ, index - 1),				\
	SENSOR_ATTR_2(temp##index##_max, S_IRUGO | S_IWUSR, show_temp,	\
		store_temp, TEMP_CRIT, index - 1),			\
	SENSOR_ATTR_2(temp##index##_max_hyst, S_IRUGO | S_IWUSR,	\
		show_temp, store_temp, TEMP_CRIT_HYST, index - 1),	\
	SENSOR_ATTR_2(temp##index##_warn, S_IRUGO | S_IWUSR, show_temp,	\
		store_temp, TEMP_WARN, index - 1),			\
	SENSOR_ATTR_2(temp##index##_warn_hyst, S_IRUGO | S_IWUSR,	\
		show_temp, store_temp, TEMP_WARN_HYST, index - 1),	\
	SENSOR_ATTR_2(temp##index##_alarm, S_IRUGO,			\
		show_alarm_beep, NULL, ALARM_STATUS, index * 2 - 1 )



static struct sensor_device_attribute_2 nct7904_in[] = {
	SENSOR_ATTR_IN(0),
	SENSOR_ATTR_IN(1),
	SENSOR_ATTR_IN(2),
	SENSOR_ATTR_IN(3),
	SENSOR_ATTR_IN(4),
	SENSOR_ATTR_IN(5),
	SENSOR_ATTR_IN(6),
	SENSOR_ATTR_IN(7),
	SENSOR_ATTR_IN(8),
	SENSOR_ATTR_IN(9),
	SENSOR_ATTR_IN(10),
	SENSOR_ATTR_IN(11),
	SENSOR_ATTR_IN(12),
	SENSOR_ATTR_IN(13),
	SENSOR_ATTR_IN(14),
	SENSOR_ATTR_IN(15),
	SENSOR_ATTR_IN(16),

};

static struct sensor_device_attribute_2 nct7904_fan[] = {
	SENSOR_ATTR_FAN(1),
	SENSOR_ATTR_FAN(2),
	SENSOR_ATTR_FAN(3),
	SENSOR_ATTR_FAN(4),
	SENSOR_ATTR_FAN(5),
	SENSOR_ATTR_FAN(6),
	SENSOR_ATTR_FAN(7),
	SENSOR_ATTR_FAN(8),
	SENSOR_ATTR_FAN(9),
	SENSOR_ATTR_FAN(10),
	SENSOR_ATTR_FAN(11),
	SENSOR_ATTR_FAN(12),

};
//lanner++
#define SENSOR_ATTR_FOV(index)						\
	SENSOR_ATTR_2(fov##index##_value, S_IWUSR|S_IRUGO, show_fov, store_fov, FAN_INPUT, index - 1)
#define SENSOR_ATTR_TFMR(index)						\
	SENSOR_ATTR_2(tfmr##index##_value, S_IWUSR|S_IRUGO, show_tfmr, store_tfmr, FAN_INPUT, index - 1)
static struct sensor_device_attribute_2 nct7904_fov[] = {
	SENSOR_ATTR_FOV(1),
	SENSOR_ATTR_FOV(2),
	SENSOR_ATTR_FOV(3),
	SENSOR_ATTR_FOV(4),
};
static struct sensor_device_attribute_2 nct7904_tfmr[] = {
	SENSOR_ATTR_TFMR(1),
	SENSOR_ATTR_TFMR(2),
	SENSOR_ATTR_TFMR(3),
	SENSOR_ATTR_TFMR(4),
};
//lanner--

static struct sensor_device_attribute_2 nct7904_temp[] = {
	SENSOR_ATTR_TEMP(1),
	SENSOR_ATTR_TEMP(2),
	SENSOR_ATTR_TEMP(3),
	SENSOR_ATTR_TEMP(4),
	SENSOR_ATTR_TEMP(5),
};

static struct sensor_device_attribute_2 nct7904_dts[] = {
	SENSOR_ATTR_DTS(6),
	SENSOR_ATTR_DTS(7),
	SENSOR_ATTR_DTS(8),
	SENSOR_ATTR_DTS(9),
	SENSOR_ATTR_DTS(10),
	SENSOR_ATTR_DTS(11),
	SENSOR_ATTR_DTS(12),
	SENSOR_ATTR_DTS(13),
};


/*
static struct sensor_device_attribute_2 w83795_vid[] = {
	SENSOR_ATTR_2(cpu0_vid, S_IRUGO, show_vid, NULL, NOT_USED, 0),
	SENSOR_ATTR_2(cpu1_vid, S_IRUGO, show_vid, NULL, NOT_USED, 1),
	SENSOR_ATTR_2(cpu2_vid, S_IRUGO, show_vid, NULL, NOT_USED, 2),
};
*/





static void nct7904_init_client(struct i2c_client *client)
{
	if (reset) {
		nct7904_write_value(client, NCT7904_REG_GLOBAL_CONTROL, 0x80);
	}

	/* Start monitoring: nct7904 has no this register */

}

/* Return 0 if detection is successful, -ENODEV otherwise */
//static int nct7904_detect(struct i2c_client *client, int kind,
static int nct7904_detect(struct i2c_client *client,
			 struct i2c_board_info *info)
{
	u8 bank;
	struct i2c_adapter *adapter = client->adapter;
	unsigned short address = client->addr;

	printk("nct7904: nct7904_detecting...\n");

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		return -ENODEV;
	}
	bank = i2c_smbus_read_byte_data(client, NCT7904_REG_BANKSEL);


	/* If Nuvoton chip, address of chip and W83795_REG_I2C_ADDR
	   should match */
	if ((bank & NCT7904_REG_BANK_MASK) == 0
	 && (i2c_smbus_read_byte_data(client, NCT7904_REG_I2C_ADDR) & 0x7f) !=
	    address<<1) {
		printk("nct7904: Detection failed at check "
			 "i2c addr\n");
		return -ENODEV;
	}


	/* We have either had a force parameter, or we have already detected the
	   Nuvoton. Determine the chip type now */

#if 0
	if (kind <= 0) {
		/* Check Nuvoton vendor ID */
		if (0x50 != i2c_smbus_read_byte_data(client,
							NCT7904_REG_VENDORID)) {
			if (0x50 != i2c_smbus_read_byte_data(client,
							NCT7904_REG_VENDORID2)) {

				if (kind == 0){
					dev_warn(&adapter->dev, "w83795: Ignoring "
					 "'force' parameter for unknown chip "
					 "at address 0x%02x\n", address);
				}

				printk("nct7904: Detection failed at check "
					 "vendor id\n");
				return -ENODEV;
			}
		}

		if (0x79 != i2c_smbus_read_byte_data(client,
			        NCT7904_REG_CHIPID)) {
			   if (0xC5 != i2c_smbus_read_byte_data(client,
			        NCT7904_REG_CHIPID2)) {

					printk("nct7904: Detection failed at check "
					 "chip id\n");
					return -ENODEV;
			   }
		} 

		kind = nct7904;
		
	}
#endif

	/* set bank as 0 */
	i2c_smbus_write_byte_data(client, NCT7904_REG_BANKSEL, 0);


	printk("nct7904: nct7904 is found.\n");

	/* Fill in the remaining client fields and put into the global list */
	strlcpy(info->type, "nct7904", I2C_NAME_SIZE);

	return 0;
}

static int nct7904_probe(struct i2c_client *client, 
			const struct i2c_device_id *id)
{
	int i;
	u8 tmp;
	u16 u16tmp;
	struct device *dev = &client->dev;
	struct nct7904_data *data;
	int err = 0;

	if (!(data = kzalloc(sizeof(struct nct7904_data), GFP_KERNEL))) {
		err = -ENOMEM;
		goto exit;
	}

	i2c_set_clientdata(client, data);
	data->bank = i2c_smbus_read_byte_data(client, NCT7904_REG_BANKSEL) & NCT7904_REG_BANK_MASK;
	mutex_init(&data->update_lock);

	/* Initialize the chip */
	nct7904_init_client(client);

	/* Check chip type: Only one package-48 pins. So assign it directly. */
	data->chip_type = nct7904d;
	
	data->has_in = nct7904_read_value(client, NCT7904_REG_VOLT_CTRL1);
	data->has_in |= nct7904_read_value(client, NCT7904_REG_VOLT_CTRL2) << 8;
	data->has_in |= nct7904_read_value(client, NCT7904_REG_VOLT_CTRL3) << 16;

	data->has_fan = nct7904_read_value(client, NCT7904_REG_FANIN_CTRL1);
	data->has_fan |= nct7904_read_value(client, NCT7904_REG_FANIN_CTRL2) << 8;

	data->has_temp = 0;
	tmp = nct7904_read_value(client, NCT7904_REG_VOLT_CTRL1);
	if ((tmp & 0x6) == 0x6){
		data->has_temp |= 1;
	}
	if ((tmp & 0x18) == 0x18){
		data->has_temp |= 2;
	}
	if ((tmp & 0x20) == 0x20){
		data->has_temp |= 4;
	} 
	if ((tmp & 0x80) == 0x80){
		data->has_temp |= 8;
	}

	/* LTD */
	tmp = nct7904_read_value(client, NCT7904_REG_VOLT_CTRL3);
	if ((tmp & 0x02) == 0x02){
		data->has_temp |= 0x10;
	}

	/* PECI */
	tmp = nct7904_read_value(client, NCT7904_REG_PECI_ENABLE);
	if (tmp & 0x80) {
		data->enable_dts = 1;	//bit1=0,bit0=1 => Enable DTS & PECI
	} else {
		tmp = nct7904_read_value(client, NCT7904_REG_TSI_CTRL);
		if (tmp & 0x80) {
			data->enable_dts = 0x3; //bit1=1, bit0=1, Enable DTS & TSI
		}
		else{
			data->enable_dts = 0;
		}
	}


	/* VDSEN2-9 and TR1-4, TD1-2 use the same pin and is exclusive. */	
	data->temp_mode = 0;
	tmp = nct7904_read_value(client, NCT7904_REG_VOLT_TEMP_CTRL);

	/* Multi-Function detecting for Volt and TR/TD.
	   Just deal with the DISABLE in has_xxxx because
	   if it is not monitored, multi-function selection is useless.*/
	#define TMP_MASK 0x3
	u8 val;
	u8 TempIdxBit; //Tmp using. The index bit coresponding to has_temp and "TD" in has_mode
	u32 VoltIdxBit; //Tmp using. The index bit coresponding to has_in

	//VSEN2~3, TEMP_CH1
	val = (tmp & (TMP_MASK<<NCT7904_TEMP_CTRL_SHIFT[0]))>>NCT7904_TEMP_CTRL_SHIFT[0];
	TempIdxBit = (1<<0);
	VoltIdxBit = 0x6;

	if (val == VAL_VOLT_TEMP_CTRL_VOLT_MONITOR){
		data->has_temp &= ~TempIdxBit;
	}
	else if ((val == VAL_VOLT_TEMP_CTRL_DIODE_CURRENT) || 
		(val == VAL_VOLT_TEMP_CTRL_DIODE_VOLT)){
		data->has_in &= ~VoltIdxBit;
		data->temp_mode |= TempIdxBit;
	}
	else{
		data->has_in &= ~VoltIdxBit;
	}

	//VSEN4~5, TEMP_CH2
	val = (tmp & (TMP_MASK<<NCT7904_TEMP_CTRL_SHIFT[1]))>>NCT7904_TEMP_CTRL_SHIFT[1];
	TempIdxBit = (1<<1);
	VoltIdxBit = 0x18;
	if (val == VAL_VOLT_TEMP_CTRL_VOLT_MONITOR){
		data->has_temp &= ~TempIdxBit;
	}
	else if ((val == VAL_VOLT_TEMP_CTRL_DIODE_CURRENT) || 
		(val == VAL_VOLT_TEMP_CTRL_DIODE_VOLT)){
		data->has_in &= ~VoltIdxBit;
		data->temp_mode |= TempIdxBit;
	}
	else{
		data->has_in &= ~VoltIdxBit;
	}

	//VSEN6, TEMP_CH3
	val = (tmp & (TMP_MASK<<NCT7904_TEMP_CTRL_SHIFT[2]))>>NCT7904_TEMP_CTRL_SHIFT[2];
	TempIdxBit = (1<<2);
	VoltIdxBit = 0x20;
	if (val == VAL_VOLT_TEMP_CTRL_VOLT_MONITOR){
		data->has_temp &= ~TempIdxBit;
	}
	else{ //TR
		data->has_in &= ~VoltIdxBit;
	}

	//VSEN8, TEMP_CH4
	val = (tmp & (TMP_MASK<<NCT7904_TEMP_CTRL_SHIFT[3]))>>NCT7904_TEMP_CTRL_SHIFT[3];
	TempIdxBit = (1<<3);
	VoltIdxBit = 0x80;
	if (val == VAL_VOLT_TEMP_CTRL_VOLT_MONITOR){
		data->has_temp &= ~TempIdxBit;
	}
	else{ //TR
		data->has_in &= ~VoltIdxBit;
	}


	/* Check DTS enable status */
	if (data->enable_dts == 0) {
		data->has_dts = 0;
	} else {
		
		data->has_dts = 
			nct7904_read_value(client, NCT7904_REG_DTSC0) & 0xF;
		if (data->enable_dts & 0x2){
			data->has_dts |=
			(nct7904_read_value(client, NCT7904_REG_DTSC1) & 0xF) << 4;
		}

	}

	/* First update the voltages measured value and limits */
	for (i = 0; i < ARRAY_SIZE(data->in); i++) {
		if (!(data->has_in & (1 << i))) {
			continue;
		}

		u16tmp = nct7904_read_value(client, NCT7904_REG_IN[i][IN_MAX]) << 3;
		u16tmp |= nct7904_read_value(client, IN_HL_LSB_REG(i));
		data->in[i][IN_MAX] = u16tmp;

		u16tmp = nct7904_read_value(client, NCT7904_REG_IN[i][IN_LOW]) << 3;
		u16tmp |= nct7904_read_value(client, IN_LL_LSB_REG(i));
		data->in[i][IN_LOW] = u16tmp;
		
		u16tmp = nct7904_read_value(client, NCT7904_REG_IN[i][IN_READ]) << 3;
		u16tmp |= nct7904_read_value(client, IN_LSB_REG(i)) & IN_LSB_MASK;
		data->in[i][IN_READ] = u16tmp;
	}

	

	/* First update fan and limits */
	for (i = 0; i < ARRAY_SIZE(data->fan); i++) {
		if (!(data->has_fan & (1 << i))) {
			continue;
		}
		data->fan_min[i] =
			((u16)nct7904_read_value(client, NCT7904_REG_FAN_MIN(i))) << 5;
		data->fan_min[i] |=
		  nct7904_read_value(client, NCT7904_REG_FAN_MIN_LSB(i)) & NCT7904_FAN_LSB_MASK;
		data->fan[i] =
			((u16)nct7904_read_value(client, NCT7904_REG_FAN(i))) << 5;
		data->fan[i] |=
		  nct7904_read_value(client, NCT7904_REG_FAN_LSB(i)) & NCT7904_FAN_LSB_MASK;
	}
//lanner++
    for(i=0; i<4; i++){
        data->fov[i] = ((u16)nct7904_read_value(client, NCT7904_REG_FOV(i)));
        data->tfmr[i] = ((u16)nct7904_read_value(client, NCT7904_REG_TFMR(i)));
    }
//lanner--

	/* temperature and limits */
	for (i = 0; i < ARRAY_SIZE(data->temp); i++) {
		if (!(data->has_temp & (1 << i)))
			continue;
		data->temp[i][TEMP_CRIT] = 
			nct7904_read_value(client, NCT7904_REG_TEMP[i][TEMP_CRIT]);
		data->temp[i][TEMP_CRIT_HYST] = 
			nct7904_read_value(client, NCT7904_REG_TEMP[i][TEMP_CRIT_HYST]);
		data->temp[i][TEMP_WARN] = 
			nct7904_read_value(client, NCT7904_REG_TEMP[i][TEMP_WARN]);
		data->temp[i][TEMP_WARN_HYST] = 
			nct7904_read_value(client, NCT7904_REG_TEMP[i][TEMP_WARN_HYST]);
		data->temp[i][TEMP_READ] = 
			nct7904_read_value(client, NCT7904_REG_TEMP[i][TEMP_READ]);
		data->temp_read_lsb[i] =
			nct7904_read_value(client, NCT7904_REG_TEMP_LSB(i));
	}

	/* dts temperature and limits */
	if (data->enable_dts != 0) {
		for (i = 0; i < ARRAY_SIZE(data->dts); i++) {
			data->dts[i][DTS_CRIT] = 
				nct7904_read_value(client, NCT7904_REG_DTS[i][DTS_CRIT]);
			data->dts[i][DTS_CRIT_HYST] = 
				nct7904_read_value(client, NCT7904_REG_DTS[i][DTS_CRIT_HYST]);
			data->dts[i][DTS_WARN] = 
				nct7904_read_value(client, NCT7904_REG_DTS[i][DTS_WARN]);
			data->dts[i][DTS_WARN_HYST] = 
				nct7904_read_value(client, NCT7904_REG_DTS[i][DTS_WARN_HYST]);
			
			if (!(data->has_dts & (1 << i)))
				continue;
			data->dts[i][DTS_READ] = 
				nct7904_read_value(client, NCT7904_REG_DTS[i][DTS_READ]);
			data->dts_read_lsb[i] =
				nct7904_read_value(client, NCT7904_REG_DTS_LSB(i));
			
		}
	}



	/* vid */
	/*
	data->vrm = vid_which_vrm();
	if (data->chip_type == w83795adg) {
		data->has_vid = 0;
	} else {
		data->has_vid = 
			(w83795_read_value(client, W83795_REG_VID_CTRL) >> 3) & 0x07;
	}
	*/
	
	/* alarm */
	for (i = 0; i < ALARM_REG_NUM; i ++) {
		data->alarms[i] = 
			nct7904_read_value(client, NCT7904_REG_ALARM(i));

	}




	/* Register sysfs hooks */
	for (i = 0; i < ARRAY_SIZE(nct7904_in); i++) {
		if (!(data->has_in & (1 << (i / 4)))) 
			continue;
		err = device_create_file(dev, &nct7904_in[i].dev_attr);
		if (err)
			goto exit_remove;
	}

	for (i = 0; i < ARRAY_SIZE(nct7904_fan); i++) {
		if (!(data->has_fan & (1 << (i / 3))))
			continue;
		err = device_create_file(dev, &nct7904_fan[i].dev_attr);
		if (err)
			goto exit_remove;
	}
//lanner++
	for (i = 0; i < ARRAY_SIZE(nct7904_fov); i++) {
		err = device_create_file(dev, &nct7904_fov[i].dev_attr);
		if (err)
			goto exit_remove;
	}
	for (i = 0; i < ARRAY_SIZE(nct7904_tfmr); i++) {
		err = device_create_file(dev, &nct7904_tfmr[i].dev_attr);
		if (err)
			goto exit_remove;
	}
//lanner--

/*
	for (i = 0; i < ARRAY_SIZE(w83795_vid); i++) {
		if (!((data->has_vid >> i) & 1))
			continue;
		err = device_create_file(dev, &w83795_vid[i].dev_attr);
		if (err)
			goto exit_remove;
	}
*/	



	for (i = 0; i < ARRAY_SIZE(nct7904_temp); i++) {
		if (!(data->has_temp & (1 << (i/7))))
			continue;
		err = device_create_file(dev, &nct7904_temp[i].dev_attr);
		if (err)
			goto exit_remove;
	}


	if (data->enable_dts != 0) {
		for (i = 0; i < ARRAY_SIZE(nct7904_dts); i++) {
			if (!(data->has_dts & (1 << (i / 7))))
				continue;
			err = device_create_file(dev, &nct7904_dts[i].dev_attr);
			if (err)
				goto exit_remove;
		}
	}



	data->hwmon_dev = hwmon_device_register(dev);
	if (IS_ERR(data->hwmon_dev)) {
		err = PTR_ERR(data->hwmon_dev);
		goto exit_remove;
	}

	return 0;

	/* Unregister sysfs hooks */
exit_remove:
	for (i = 0; i < ARRAY_SIZE(nct7904_in); i++)
		device_remove_file(dev, &nct7904_in[i].dev_attr);

	for (i = 0; i < ARRAY_SIZE(nct7904_fan); i++)
		device_remove_file(dev, &nct7904_fan[i].dev_attr);

//lanner++
    for (i = 0; i < ARRAY_SIZE(nct7904_fov); i++)
        device_remove_file(dev, &nct7904_fov[i].dev_attr);
    for (i = 0; i < ARRAY_SIZE(nct7904_tfmr); i++)
        device_remove_file(dev, &nct7904_tfmr[i].dev_attr);
//lanner--


/*
	for (i = 0; i < ARRAY_SIZE(w83795_vid); i++) {
		if (!((data->has_vid >> i) & 1))
			continue;
		device_remove_file(dev, &w83795_vid[i].dev_attr);
	}
	*/



	for (i = 0; i < ARRAY_SIZE(nct7904_temp); i++){
		if (!(data->has_temp & (1 << i)))
			continue;
		device_remove_file(dev, &nct7904_temp[i].dev_attr);
	}

	for (i = 0; i < ARRAY_SIZE(nct7904_dts); i++)
		device_remove_file(dev, &nct7904_dts[i].dev_attr);



	kfree(data);
exit:
	return err;
}

static struct nct7904_data *nct7904_update_device(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct nct7904_data *data = i2c_get_clientdata(client);
	u16 tmp;
	int i;

	mutex_lock(&data->update_lock);

	if (!(time_after(jiffies, data->last_updated + HZ * 2)
	      || !data->valid))
		goto END;

	/* Update the voltages value */
	for (i = 0; i < ARRAY_SIZE(data->in); i++) {
		if (!(data->has_in & (1 << i))) {
			continue;
		}
		tmp = nct7904_read_value(client, NCT7904_REG_IN[i][IN_READ]) << 3;
		tmp |= nct7904_read_value(client, IN_LSB_REG(i)) & IN_LSB_MASK;
		data->in[i][IN_READ] = tmp;
	}

	/* Update fan */
	for (i = 0; i < ARRAY_SIZE(data->fan); i++) {
		if (!(data->has_fan & (1 << i))) {
			continue;
		}
		data->fan[i] =
			((u16)nct7904_read_value(client, NCT7904_REG_FAN(i))) << 5;
		data->fan[i] |=
		  nct7904_read_value(client, NCT7904_REG_FAN_LSB(i)) & NCT7904_FAN_LSB_MASK;
	}
//lanner++
    for(i=0; i<4; i++){
        data->fov[i] = ((u16)nct7904_read_value(client, NCT7904_REG_FOV(i)));
        data->tfmr[i] = ((u16)nct7904_read_value(client, NCT7904_REG_TFMR(i)));
    }
//lanner--

	/* Update temperature */
	for (i = 0; i < ARRAY_SIZE(data->temp); i++) {
		/* even stop monitor, register still keep value, just read out it */
		if (!(data->has_temp & (1 << i))) {
			data->temp[i][TEMP_READ] = 0;
			data->temp_read_lsb[i] = 0;
			continue;
		}
		data->temp[i][TEMP_READ] = 
			nct7904_read_value(client, NCT7904_REG_TEMP[i][TEMP_READ]);
		data->temp_read_lsb[i] =
			nct7904_read_value(client, NCT7904_REG_TEMP_LSB(i));
	}

	/* Update dts temperature */
	/* dts temperature and limits */
	if (data->enable_dts != 0) {
		for (i = 0; i < ARRAY_SIZE(data->dts); i++) {
			
			data->dts[i][DTS_CRIT] = 
				nct7904_read_value(client, NCT7904_REG_DTS[i][DTS_CRIT]);
			data->dts[i][DTS_CRIT_HYST] = 
				nct7904_read_value(client, NCT7904_REG_DTS[i][DTS_CRIT_HYST]);
			data->dts[i][DTS_WARN] = 
				nct7904_read_value(client, NCT7904_REG_DTS[i][DTS_WARN]);
			data->dts[i][DTS_WARN_HYST] = 
				nct7904_read_value(client, NCT7904_REG_DTS[i][DTS_WARN_HYST]);

			if (!(data->has_dts & (1 << i)))
				continue;
			data->dts[i][DTS_READ] = 
				nct7904_read_value(client, NCT7904_REG_DTS[i][DTS_READ]);
			data->dts_read_lsb[i] =
				nct7904_read_value(client, NCT7904_REG_DTS_LSB(i));
			
		}
	}

	

	/* retrieve vid  */
	/*
	if (0 != data->has_vid) {
		tmp = w83795_read_value(client, W83795_REG_VID_CTRL);
		w83795_write_value(client, W83795_REG_VID_CTRL, tmp | 0x80);
		for (i = 0; i < 3; i ++) {
			data->vid[i] = 
				w83795_read_value(client, W83795_REG_VSEN_VIDIN(i));
		}
	}
	*/
	
	/* update alarm and beep */
	for (i = 0; i < ALARM_REG_NUM; i ++) {
		data->alarms[i] = 
			nct7904_read_value(client, NCT7904_REG_ALARM(i));
	}

	data->last_updated = jiffies;
	data->valid = 1;

END:
	mutex_unlock(&data->update_lock);
	return data;
}

static int nct7904_remove(struct i2c_client *client)
{
	struct nct7904_data *data = i2c_get_clientdata(client);
	struct device *dev = &client->dev;
	int i,res;

	hwmon_device_unregister(data->hwmon_dev);

	for (i = 0; i < ARRAY_SIZE(nct7904_in); i++)
		device_remove_file(dev, &nct7904_in[i].dev_attr);

	for (i = 0; i < ARRAY_SIZE(nct7904_fan); i++)
		device_remove_file(dev, &nct7904_fan[i].dev_attr);

//lanner++
    for (i = 0; i < ARRAY_SIZE(nct7904_fov); i++)
        device_remove_file(dev, &nct7904_fov[i].dev_attr);
    for (i = 0; i < ARRAY_SIZE(nct7904_tfmr); i++)
        device_remove_file(dev, &nct7904_tfmr[i].dev_attr);
//lanner--

/*
	for (i = 0; i < ARRAY_SIZE(w83795_vid); i++) {
		if (!((data->has_vid >> i) & 1))
			continue;
		device_remove_file(dev, &w83795_vid[i].dev_attr);
	}
*/
	

	for (i = 0; i < ARRAY_SIZE(nct7904_temp); i++){
		if (!(data->has_temp & (1 << (i/7))))
			continue;
		device_remove_file(dev, &nct7904_temp[i].dev_attr);
	}

	for (i = 0; i < ARRAY_SIZE(nct7904_dts); i++){
		device_remove_file(dev, &nct7904_dts[i].dev_attr);
	}

	/* To make sure bank is set as 0.
		Only bank 0 can provide Chip ID and Vendor ID in some early nct7904.
		So we always reset bank to 0 before removing driver. */
	res = i2c_smbus_write_byte_data(client, NCT7904_REG_BANKSEL, 0);

	kfree(data);

	return 0;
}



/* Ignore the possibility that somebody change bank outside the driver
   Must be called with data->update_lock held, except during initialization */
static u8 nct7904_read_value(struct i2c_client *client, u16 reg)
{
	struct nct7904_data *data = i2c_get_clientdata(client);
	u8 res = 0xff;
	u8 new_bank = reg >> 8;

//new_bank |= data->bank & 0xf8;

#ifndef FIX_BANK_PROBLEM /* #if NOT def */
	/* For fixing bank's bug of early verion nct7904, we always 
	   over-write bank reg before accessing */

	if (data->bank != new_bank) {
#endif		
		if (i2c_smbus_write_byte_data
		    (client, NCT7904_REG_BANKSEL, new_bank) >= 0){
			data->bank = new_bank;
		}
		else {
			dev_err(&client->dev,
				"set bank to %d failed, fall back "
				"to bank %d, read reg 0x%x error\n",
				new_bank, data->bank, reg);
			res = 0x0;	/* read 0x0 from the chip */
			goto END;
		}
		
#ifndef FIX_BANK_PROBLEM /* #if NOT def */		
	}
#endif

	res = i2c_smbus_read_byte_data(client, reg & 0xff);
END:
	return res;
}

/* Must be called with data->update_lock held, except during initialization */
static int nct7904_write_value(struct i2c_client *client, u16 reg, u8 value)
{
	struct nct7904_data *data = i2c_get_clientdata(client);
	int res;
	u8 new_bank = reg >> 8;


//new_bank |= data->bank & 0xf8;

#ifndef FIX_BANK_PROBLEM /* #if NOT def */
	
	/* For fixing bank's bug of early verion nct7904, we always 
	   over-write bank reg before accessing */
	if (data->bank != new_bank) {
#endif

		if ((res = i2c_smbus_write_byte_data
		    (client, NCT7904_REG_BANKSEL, new_bank)) >= 0){
			data->bank = new_bank;
		}
		else {
			dev_err(&client->dev,
				"set bank to %d failed, fall back "
				"to bank %d, write reg 0x%x error\n",
				new_bank, data->bank, reg);
			goto END;
		}
		
#ifndef FIX_BANK_PROBLEM /* #if NOT def */		
	}
#endif	
 

	res = i2c_smbus_write_byte_data(client, reg & 0xff, value);
END:
	return res;
}

static int __init sensors_nct7904_init(void)
{
	printk("nct7904: sensors_nct7904_init\n");

	return i2c_add_driver(&nct7904_driver);
}

static void __exit sensors_nct7904_exit(void)
{
	printk("nct7904: sensors_nct7904_exit\n");

	i2c_del_driver(&nct7904_driver);
}

MODULE_AUTHOR("Sheng-Yuan Huang");
MODULE_DESCRIPTION("NCT7904 driver");
MODULE_LICENSE("GPL");

module_init(sensors_nct7904_init);
module_exit(sensors_nct7904_exit);


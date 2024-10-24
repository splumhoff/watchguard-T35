/*
 * (C) Copyright 2001
 * Erik Theisen, Wave 7 Optics, etheisen@mindspring.com
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <config.h>
#include <command.h>

#include <linux/types.h>

#include <dtt.h>
#include <i2c.h>

struct register_desc {
	int offset;
	char desc[40];
};

struct register_desc reg_table[] =  {
{ 0, "Local Temperature"},
{ 1, "Rmt Temp MSB"},
{ 2, "ALERT Status"},
{ 3, "Configuration"},
{ 4, "Conversion Rate"},
{ 5, "Local High Setpoint"},
{ 7, "Rmt High Setpoint MSB"},
{ 8, "Rmt LOW Setpoint MSB"},
{ 9, "Same as 03"},		// skip later
{ 0xA, "Same as 04"},		// skip later
{ 0xB, "Same as 05"},		// skip later
{ 0xD, "Same as 07"},		// skip later
{ 0xE, "Same as 08"},		// skip later
{ 0xF, "One Shot (WO)"},
{ 0x10, "Rmt Temp LSB"},
{ 0x11, "Rmt Temp Offset MSB"},
{ 0x12, "Rmt Temp Offset LSB"},
{ 0x13, "Rmt High Setpoint LSB"},
{ 0x14, "Rmt Low Setpoint LSB"},
{ 0x16, "ALERT Mask"},
{ 0x19, "Rmt TCRIT Setpoint"},
{ 0x21, "Rmt TCRIT Hysteresis"},
{ 0x46, "Tach Count LSB"},
{ 0x47, "Tach Count MSB"},
{ 0x48, "Tach Limit LSB"},
{ 0x49, "Tach Limit MSB"},
{ 0x4A, "PWM and RPM"},
{ 0x4B, "Fan Spin-Up Config"},
{ 0x4C, "PWM Value"},
{ 0x4D, "PWM Frequency"},
{ 0x4F, "Lookup tbl Hysteresis"},
};

int sizeof_table = 0;

struct pwm_lookup_entry {
        u8 temp;
        u8 pwm;
};
struct pwm_lookup_entry lookup[10]; /* max 10*/

int lookup_curr = 0;
int lookup_reset = 1;

int
find_regs_in_table(int reg)
{
	int i;
	for (i=0;i<sizeof_table;i++) {
		if (reg_table[i].offset == reg)
			return i;
	}
	return -1;
}


int
dump_lm63_registers(unsigned char sensor)
{
	int i;

	printf("dump all registers\n");
	for (i=0;i<sizeof_table;i++) {
		printf ("Reg: (%x: %s): 0x%x\n",
			reg_table[i].offset, reg_table[i].desc, dtt_read(sensor, reg_table[i].offset));
	}
	return 0;
}

int
lm63_init(void)
{
	/* TODO: setup the lm63 controller now */
	int i;
	unsigned char sensors[] = CONFIG_DTT_SENSORS;
	const char *const header = "LM63:   ";

	for (i = 0; i < sizeof(sensors); i++) {
		if (_dtt_init(sensors[i], 1, (void *)lookup) != 0)
			printf("%s%d FAILED INIT\n", header, i + 1);
		else
			printf("%s%d is %i C\n", header, i + 1,
			       dtt_get_temp(sensors[i]));
	}

	return 0;
}


typedef enum {false, true} bool;
int do_lm63 (cmd_tbl_t * cmdtp, int flag, int argc, char * const argv[])
{
	int i;
	int old_bus;
	bool alert_status_read = false;
	bool valid_setup_detect = false;
	unsigned char sensors[] = CONFIG_DTT_SENSORS;

	/* switch to correct I2C bus */
	old_bus = I2C_GET_BUS();
	I2C_SET_BUS(CONFIG_SYS_DTT_BUS_NUM);

	sizeof_table = sizeof(reg_table)/sizeof(struct register_desc);
	if (argc == 2 && strcmp(argv[1], "dumpregs") == 0)
		return dump_lm63_registers(sensors[0]);
	if (argc == 2 && strcmp(argv[1], "init") == 0) {
		if (!valid_setup_detect)
			printf ("ERR: no valid setup detected, init however ...\n");
		return lm63_init();
	}
	if (argc == 2 && strcmp(argv[1], "lookupreset") == 0) {
		lookup_reset = 1;
		return 0;
	}
	if (argc == 2 && strcmp(argv[1], "lookupprint") == 0) {
		printf ("lookup table print\n");
		for (i=0; i<5; i++)
			printf ("entry %d: t=%d, p=%d\n", i, lookup[i].temp, lookup[i].pwm);
		return 0;
	}

	switch (argc) {
	case 0:
	case 1:
	case 2:
	case 3:
		if (strcmp(argv[1], "readreg") == 0) {
			int reg = (int)simple_strtoul(argv[2], NULL, 10);
			int r = find_regs_in_table(reg);
			if (r)
				printf ("Reg: (%x: %s): 0x%x\n", reg, reg_table[r].desc,
					dtt_read(sensors[0], reg_table[r].offset));
			else
				printf ("Invalid reg read req\n");
		}
		if (strcmp(argv[1], "print") == 0) {
			if (argc == 3 && strcmp(argv[1], "alert") == 0)
				alert_status_read = true;
			if (alert_status_read == true) {
				printf ("Alert stat = 0x%0x\n", dtt_read(sensors[0], 0x02));
			} else { /* default case: print regtable */
				for (i=0;i<sizeof_table;i++)
					printf ("table[%i] entry = Reg: (%x: %s)\n",
						i, reg_table[i].offset, reg_table[i].desc);
			}
		}
		return 0;
	case 4:
		if (strcmp(argv[1], "setup") == 0) {
			int reg = (int)simple_strtoul(argv[2], NULL, 10);
			int val = (int)simple_strtoul(argv[3], NULL, 10);
			int r = find_regs_in_table(reg);
			if (r) {
				printf ("setup: Reg:%x to value %x)\n", reg, val);
				printf ("You are setting up Reg:%x which is \"%s\"\n",
					reg, reg_table[r].desc);
				dtt_write(sensors[0], reg, val);
				valid_setup_detect = true;
			} else {
				printf ("setup: Reg:%x Not Valid \
					(might have to update the table)\n", reg);
			}
		}
		if (strcmp(argv[1], "addlookup") == 0) {
			int temp = (int)simple_strtoul(argv[2], NULL, 10);
			int pwm = (int)simple_strtoul(argv[3], NULL, 10);
			if (lookup_reset) {
				lookup_reset = 0;
				lookup_curr = 0;
			}
			if (lookup_curr >= 5) {
				printf ("ERR: lookup_curr too high: reset the lookup");
				return 0;
			}
			printf ("adding lookup entry: temp=%d, pwm =%d\n", temp, pwm);
			lookup[lookup_curr].temp = temp;
			lookup[lookup_curr].pwm = pwm;
			lookup_curr++;
		}
		return 0;
	default:
		break;
	}

	/*
	 * Loop through sensors, read
	 * temperature, and output it.
	 */
	for (i = 0; i < sizeof (sensors); i++)
		printf ("DTT%d: %i C\n", i + 1, dtt_get_temp (sensors[i]));

	/* switch back to original I2C bus */
	I2C_SET_BUS(old_bus);

	return 0;
}
/* do_lm63() */

/***************************************************/

U_BOOT_CMD(
	  lm63,	5,	1,	do_lm63,
	  "Read temperature from Digital Thermometer and Thermostat",
	  "lm63                     - reads the temp from sensors(default operation)\n"
	  "lm63 dumpregs            - dump all registers\n"
	  "lm63 print [regtable]    - print regtable\n"
	  "          [alert]       - print alert status registers\n"
	  "lm63 readreg [reg]     - read a specific register from the regtable\n"
	  "lm63 addlookup [temp] [pwm] - add lookup entries\n"
	  "lm63 lookupreset       - reset the lookup table\n"
	  "lm63 lookupprint       - print the lookup table\n"
	  "lm63 setup [reg] [val] - setup the registers\n"
	  "lm63 init              - init lm63 controller (should be followed by setup)"
);


/*
 * Status LED driver for grub 0.97
 *
 * Author: Mukund J
 * Changes:
 *      MukundJ: Jul10, 2012	// seattle (extreacted from wglinux)
 */

#include <seattle_gpio.h>
#include <linux-asm-io.h>
#include <panel.h>
#include <timer.h>
#include <lcm.h>
#include <shared.h>

#define udelay(n)       waiton_timer2(((n)*TICKS_PER_MS)/1000)

static unsigned char Driver_Version[] = "0.2 2015-10-05"; 
extern int lcd_module;

/*
 *------------------------------------------------------------------------------
 * MB-UP2010W Version V1.0
 *
 * The IO interface for Arm LED is connected to GPIO 37 52:
 *      GPIO37  GPIO52  LED_STATUS
 *      0       0       Off
 *      1       0       Green
 *      0       1       Red
 *      1       1       Off
 * (Two LEDs back-to-back in parallel between the pins.)
 *
 * The remainder are single-color, 0 = ON
 *      GPIO44          ATTN 
 *      GPIO45          MODE
 *      GPIO46          STATUS
 *
 * The reset button
 *      GPIO36          RESET 1 = pressed, 0 = released
 *------------------------------------------------------------------------------
 */

/*
 * Device Depend Definition : Winbond 83627 connected to LEDs/Reset button
*/

/* platform 'Seattle' related GPIO definitions */
#define INDEX_PORT      0x2e
#define DATA_PORT       0x2f

#define GPIO44_BIT      (1 << 4)
#define GPIO45_BIT      (1 << 5)
#define GPIO46_BIT      (1 << 6)
#define GPIO_GPIO44_46_MASK     (GPIO44_BIT | GPIO45_BIT | GPIO46_BIT)

#define GPIO3X          (1 << 1)
#define GPIO36_BIT      (1 << 6)
#define GPIO37_BIT      (1 << 7)
#define GPIO52_BIT      (1 << 2)

/* 1 bit sets LED OFF */
/* ARM LED is two back-to-back diodes between GPIO37 and GPIO52. */
#define ARM_GREEN       GPIO52_BIT
#define ARM_RED         GPIO37_BIT
#define ATTN_LED        GPIO44_BIT
#define MODE_LED        GPIO45_BIT
#define STATUS_LED      GPIO46_BIT

/* platform 'Kirkland' related GPIO definitions */
struct wgpci_device {
	const char	*name;
	unsigned short	ioaddr;
	unsigned char	devfn;
	unsigned char	slot;
	unsigned char	bus;
};

struct wgpci_device pci_lpc;
#define btn_gpi     17     // GPI 20

#define WESTPORT_MUKILTEO_SUPPORT
#ifdef WESTPORT_MUKILTEO_SUPPORT
/* 2016/07/07, Talor Lin */
/*** Port Westport T70 LED and reset button driver from the following source path ***/
	//tasks/utm-mainline-westport_mukilteo/components/wg_linux/mainline/src/modules/platform/spokane/sled_drv-t70.c
/***********************************************************************************************************/	
/*=========== Hardware-specific code to init/configure GPIO controller, read/write to GPIO devices ===========*/
/*
 * Based on pentium-celeron-n-series-datasheet-vol-1.pdf (Volume 1 of 3)
 * 16.12.5 Register Address Mapping
 */
#define WESTPORT_MUKILTEO_FAMILY_PAD_REGS_OFF		0x4400
#define WESTPORT_MUKILTEO_FAMILY_PAD_REGS_SIZE		0x400
#define WESTPORT_MUKILTEO_MAX_FAMILY_PAD_GPIO_NO		15
#define WESTPORT_MUKILTEO_GPIO_REGS_SIZE			    8

/*
 * Based on pentium-celeron-n-series-datasheet-vol-3.pdf (Volume 3 of 3)
 * 33.6.54 Pad Control Register 0 (GPIO_DFX0_PAD_CFG0) ...
 */
#define BIT(x)              (0x01 << x)
#define ARRAY_SIZE(arr)     (sizeof(arr) / sizeof(arr[0]))
#define WESTPORT_MUKILTEO_GPIO_PADCTRL0			0x000
#define WESTPORT_MUKILTEO_GPIO_PADCTRL0_GPIOEN		BIT(15)
#define WESTPORT_MUKILTEO_GPIO_PADCTRL0_GPIOCFG_SHIFT	8
#define WESTPORT_MUKILTEO_GPIO_PADCTRL0_GPIOCFG_MASK	(7 << WESTPORT_MUKILTEO_GPIO_PADCTRL0_GPIOCFG_SHIFT)
#define WESTPORT_MUKILTEO_GPIO_PADCTRL0_GPIOCFG_GPO	1
#define WESTPORT_MUKILTEO_GPIO_PADCTRL0_GPIOCFG_GPI	2
#define WESTPORT_MUKILTEO_GPIO_PADCTRL0_GPIOTXSTATE	BIT(1)
#define WESTPORT_MUKILTEO_GPIO_PADCTRL0_GPIORXSTATE	BIT(0)

#define WESTPORT_MUKILTEO_GPIO_PADCTRL1			0x004
#define WESTPORT_MUKILTEO_GPIO_PADCTRL1_CFGLOCK		BIT(31)
#define WESTPORT_MUKILTEO_GPIO_PADCTRL1_INVRXTX_SHIFT	4
#define WESTPORT_MUKILTEO_GPIO_PADCTRL1_INVRXTX_MASK	(0xf << WESTPORT_MUKILTEO_GPIO_PADCTRL1_INVRXTX_SHIFT)
#define WESTPORT_MUKILTEO_GPIO_PADCTRL1_INTWAKECFG_MASK	7
#define readl(addr) (*(unsigned int *)addr)
#define writel(value, addr) (*(unsigned int *)addr = value)

#define GPIO_WG_RESET_BTN 		 0

/**
 * struct gpio_pin_desc - boards/machines provide information on their
 * pins, pads or other muxable units in this struct
 * @number: unique pin number from the global pin number space
 * @name: a name for this pin
 */
struct gpio_pin_desc {
	unsigned number;
	const char *name;
};

/**
 * struct westport_mukilteo_chv_community - A community specific configuration
 * @uid: ACPI _UID used to match the community
 * @pins: All pins in this community
 * @npins: Number of pins
 * @ngpios: Number of GPIOs in this community
 */
struct westport_mukilteo_chv_community {
	const char *uid;
	const struct gpio_pin_desc *pins;
	unsigned npins;
	unsigned ngpios;
};

/**
 * struct westport_mukilteo_chv_pinctrl - CHV pinctrl private structure
 * @dev: Pointer to the parent device
 * @regs: MMIO registers
 * @lock: Lock to serialize register accesses
 * @community: Community this pinctrl instance represents
 *
 * The first group in @groups is expected to contain all pins that can be
 * used as GPIOs.
 */
struct westport_mukilteo_chv_pinctrl {
	//struct device *dev;
	char *regs;
	//spinlock_t lock;
	const struct westport_mukilteo_chv_community *community;
};

static const struct gpio_pin_desc westport_mukilteo_north_pins[] = {
	{0, "GPIO_DFX_0"},
	{1, "GPIO_DFX_3"},
	{2, "GPIO_DFX_7"},
	{3, "GPIO_DFX_1"},
	{4, "GPIO_DFX_5"},
	{5, "GPIO_DFX_4"},
	{6, "GPIO_DFX_8"},
	{7, "GPIO_DFX_2"},
	{8, "GPIO_DFX_6"},
};

static const struct westport_mukilteo_chv_community westport_mukilteo_north_community = {
	.uid = "2",
	.pins = westport_mukilteo_north_pins,
	.npins = ARRAY_SIZE(westport_mukilteo_north_pins),
	.ngpios = ARRAY_SIZE(westport_mukilteo_north_pins),
};

static struct westport_mukilteo_chv_pinctrl glb_pctrl = {0xfed88000 /* lpcres start address */, &westport_mukilteo_north_community};

static int *westport_mukilteo_chv_padreg(struct westport_mukilteo_chv_pinctrl *pctrl, unsigned offset,
				unsigned reg)
{
	unsigned family_no = offset / WESTPORT_MUKILTEO_MAX_FAMILY_PAD_GPIO_NO;
	unsigned pad_no = offset % WESTPORT_MUKILTEO_MAX_FAMILY_PAD_GPIO_NO;

	offset = WESTPORT_MUKILTEO_FAMILY_PAD_REGS_OFF + WESTPORT_MUKILTEO_FAMILY_PAD_REGS_SIZE * family_no +
		 WESTPORT_MUKILTEO_GPIO_REGS_SIZE * pad_no;
	
	return (int *)(pctrl->regs + offset + reg);
}

static void westport_mukilteo_chv_writel(unsigned int value, int *reg)
{
	writel(value, reg);
	/* simple readback to confirm the bus transferring done */
	readl(reg);
}

/* When Pad Cfg is locked, driver can only change GPIOTXState or GPIORXState */
static int westport_mukilteo_chv_pad_locked(struct westport_mukilteo_chv_pinctrl *pctrl, unsigned offset)
{
	int *reg;

	reg = westport_mukilteo_chv_padreg(pctrl, offset, WESTPORT_MUKILTEO_GPIO_PADCTRL1);
	return readl(reg) & WESTPORT_MUKILTEO_GPIO_PADCTRL1_CFGLOCK;
}

static int westport_mukilteo_chv_gpio_request_enable(struct westport_mukilteo_chv_pinctrl *pctrl, unsigned offset)
{
	unsigned long flags;
	int *reg;
	unsigned int value;

	if (westport_mukilteo_chv_pad_locked(pctrl, offset)) {
		value = readl(westport_mukilteo_chv_padreg(pctrl, offset, WESTPORT_MUKILTEO_GPIO_PADCTRL0));
		if (!(value & WESTPORT_MUKILTEO_GPIO_PADCTRL0_GPIOEN)) {
			return -1/*-EBUSY*/;
		}
	} else {
		/* Disable interrupt generation */
		reg = westport_mukilteo_chv_padreg(pctrl, offset, WESTPORT_MUKILTEO_GPIO_PADCTRL1);
		value = readl(reg);
		value &= ~WESTPORT_MUKILTEO_GPIO_PADCTRL1_INTWAKECFG_MASK;
		value &= ~WESTPORT_MUKILTEO_GPIO_PADCTRL1_INVRXTX_MASK;
		westport_mukilteo_chv_writel(value, reg);

		/* Switch to a GPIO mode */
		reg = westport_mukilteo_chv_padreg(pctrl, offset, WESTPORT_MUKILTEO_GPIO_PADCTRL0);
		value = readl(reg) | WESTPORT_MUKILTEO_GPIO_PADCTRL0_GPIOEN;
		westport_mukilteo_chv_writel(value, reg);
	}

	return 0;
}

static int westport_mukilteo_chv_gpio_set_direction(struct westport_mukilteo_chv_pinctrl *pctrl,
				  unsigned offset, int input)
{
	int *reg = westport_mukilteo_chv_padreg(pctrl, offset, WESTPORT_MUKILTEO_GPIO_PADCTRL0);
	unsigned long flags;
	unsigned int ctrl0;

	ctrl0 = readl(reg) & ~WESTPORT_MUKILTEO_GPIO_PADCTRL0_GPIOCFG_MASK;
	if (input)
		ctrl0 |= WESTPORT_MUKILTEO_GPIO_PADCTRL0_GPIOCFG_GPI << WESTPORT_MUKILTEO_GPIO_PADCTRL0_GPIOCFG_SHIFT;
	else
		ctrl0 |= WESTPORT_MUKILTEO_GPIO_PADCTRL0_GPIOCFG_GPO << WESTPORT_MUKILTEO_GPIO_PADCTRL0_GPIOCFG_SHIFT;
	westport_mukilteo_chv_writel(ctrl0, reg);

	return 0;
}

static unsigned westport_mukilteo_chv_gpio_offset_to_pin(struct westport_mukilteo_chv_pinctrl *pctrl,
				       unsigned offset)
{
	return pctrl->community->pins[offset].number;
}

static int westport_mukilteo_chv_gpio_get(struct westport_mukilteo_chv_pinctrl *pctrl, unsigned offset)
{
	int pin = westport_mukilteo_chv_gpio_offset_to_pin(pctrl, offset);
	unsigned int ctrl0, cfg;

	ctrl0 = readl(westport_mukilteo_chv_padreg(pctrl, pin, WESTPORT_MUKILTEO_GPIO_PADCTRL0));

	cfg = ctrl0 & WESTPORT_MUKILTEO_GPIO_PADCTRL0_GPIOCFG_MASK;
	cfg >>= WESTPORT_MUKILTEO_GPIO_PADCTRL0_GPIOCFG_SHIFT;

	if (cfg == WESTPORT_MUKILTEO_GPIO_PADCTRL0_GPIOCFG_GPO)
		return !!(ctrl0 & WESTPORT_MUKILTEO_GPIO_PADCTRL0_GPIOTXSTATE);
	return !!(ctrl0 & WESTPORT_MUKILTEO_GPIO_PADCTRL0_GPIORXSTATE);
}

static void westport_mukilteo_chv_gpio_set(struct westport_mukilteo_chv_pinctrl *pctrl, unsigned offset, int value)
{
	unsigned pin = westport_mukilteo_chv_gpio_offset_to_pin(pctrl, offset);
	unsigned long flags;
	int *reg;
	unsigned int ctrl0;

	reg = westport_mukilteo_chv_padreg(pctrl, pin, WESTPORT_MUKILTEO_GPIO_PADCTRL0);
	ctrl0 = readl(reg);

	if (value)
		ctrl0 |= WESTPORT_MUKILTEO_GPIO_PADCTRL0_GPIOTXSTATE;
	else
		ctrl0 &= ~WESTPORT_MUKILTEO_GPIO_PADCTRL0_GPIOTXSTATE;

	westport_mukilteo_chv_writel(ctrl0, reg);
}

static int westport_mukilteo_chv_gpio_get_direction(struct westport_mukilteo_chv_pinctrl *pctrl, unsigned offset)
{
	unsigned pin = westport_mukilteo_chv_gpio_offset_to_pin(pctrl, offset);
	unsigned int ctrl0, direction;

	ctrl0 = readl(westport_mukilteo_chv_padreg(pctrl, pin, WESTPORT_MUKILTEO_GPIO_PADCTRL0));

	direction = ctrl0 & WESTPORT_MUKILTEO_GPIO_PADCTRL0_GPIOCFG_MASK;
	direction >>= WESTPORT_MUKILTEO_GPIO_PADCTRL0_GPIOCFG_SHIFT;

	return direction != WESTPORT_MUKILTEO_GPIO_PADCTRL0_GPIOCFG_GPO;
}

/*=========== Code for exposing the IOCTL interface ============*/
/*
 * GPIO init:
 * initialize the default state of the GPIO pins
 */
static void westport_mukilteo_gpio_init(struct westport_mukilteo_chv_pinctrl *pctrl)
{
	// 1 X
	// X 0 			0 	(amber/yellow) Attn-LED
	/* none needed */
	// echo 456 > export equivalant
	westport_mukilteo_chv_gpio_request_enable(pctrl, 3);
	westport_mukilteo_chv_gpio_get_direction(pctrl, 3);

	// echo "out" > gpio456/direction
	westport_mukilteo_chv_gpio_set_direction(pctrl, 3, 0);

	// 0 1
	// X 0  		0 	(Red) Status-LED
	// echo 500 > export equivalant
	westport_mukilteo_chv_gpio_request_enable(pctrl, 7);
	westport_mukilteo_chv_gpio_get_direction(pctrl, 7);

	// echo "out" > gpio500/direction
	westport_mukilteo_chv_gpio_set_direction(pctrl, 7, 0);

	// 0 X
	// X 1			0 	(green) Mode-LED
	// echo 454 > export equivalant
	westport_mukilteo_chv_gpio_request_enable(pctrl, 1);
	westport_mukilteo_chv_gpio_get_direction(pctrl, 1);

	// echo "out" > gpio454/direction
	westport_mukilteo_chv_gpio_set_direction(pctrl, 1, 0);

	// 0 X
	// X 0			1	(green)	FailOver-LED
	// echo 458 > export equivalant
	westport_mukilteo_chv_gpio_request_enable(pctrl, 5);
	westport_mukilteo_chv_gpio_get_direction(pctrl, 5);

	// echo "out" > gpio458/direction
	westport_mukilteo_chv_gpio_set_direction(pctrl, 5, 0);

	// 0 X
	// X 0			1	(reset) button
	// echo 453 > export equivalant
	westport_mukilteo_chv_gpio_request_enable(pctrl, 0);
	westport_mukilteo_chv_gpio_get_direction(pctrl, 0);

	// echo "out" > gpio458/direction
	westport_mukilteo_chv_gpio_set_direction(pctrl, 0, 1);

	return;
}
#endif /* End of #ifdef WESTPORT_MUKILTEO_SUPPORT */

static void enter_w83627_config(void)
{
	outb_p(0x87, INDEX_PORT); // Lanner says we must write this twice
	outb_p(0x87, INDEX_PORT);
	return;
}

static void exit_w83627_config(void)
{
	outb_p(0xaa, INDEX_PORT);
	return;
}

static unsigned char read_w83627_reg(int ldn, int reg)
{
	unsigned char tmp = 0;

	enter_w83627_config();
	outb_p(0x07, INDEX_PORT); // LDN Register
	outb_p(ldn, DATA_PORT);   // Select LDNx
	outb_p(reg, INDEX_PORT);  // Select Register
	tmp = inb(DATA_PORT);     // Read Register
	exit_w83627_config();
	return tmp;
}

static void write_w83627_reg(int ldn, int reg, int value)
{
	enter_w83627_config();
	outb_p(0x07, INDEX_PORT); // LDN Register
	outb_p(ldn, DATA_PORT);   // Select LDNx
	outb_p(reg, INDEX_PORT);  // Select Register
	outb_p(value, DATA_PORT); // Write Register
	exit_w83627_config();
	return;
}

/*
 * Set ARM LED red
 */
void set_led_red(void)
{
	unsigned char tmp;

	grub_printf("set_led_red called\n");
	/* RED lo */
	tmp = read_w83627_reg(0x09, 0xf1);
	tmp &= ~ARM_RED;
	write_w83627_reg(0x09, 0xf1, tmp);

	/* GREEN hi */
	tmp = read_w83627_reg(0x09, 0xe1);
	tmp |= ARM_GREEN;
	write_w83627_reg(0x09, 0xe1, tmp);

    return;
}

#define BUTTON_DEPRESSED     1
#define BUTTON_NOT_DEPRESSED 0

static unsigned char get_seattle_reset_button_status(void)
{
	int btn_status;

	/* Bit clear means button is pressed */
	if ( read_w83627_reg(0x09, 0xf1) & GPIO36_BIT ) {
		btn_status = BUTTON_NOT_DEPRESSED; 
	} else {
		btn_status = BUTTON_DEPRESSED; 
	}

	/* convert the captured data into what makes sense for caller */
	if (btn_status == BUTTON_DEPRESSED)
		return BTN_RESET;
	else
		return 0;
}

/*
 * Functions for accessing PCI configuration space with type 1 accesses
 */
#define PCI_SUCCESSFUL	0x00
#define CONFIG_CMD(bus, slot, device_fn, where)   (0x8000f800 | (where & ~3))
#define LPC_GPIOBASE	0x48

int pci_read_config_dword (unsigned int bus, unsigned int slot, unsigned int device_fn,
				 unsigned int where, unsigned int *value)
{
	outl(CONFIG_CMD(bus, slot, device_fn, where), 0xCF8);
	*value = inl(0xCFC);
	return PCI_SUCCESSFUL;
}

static unsigned int kirkland_getgpio_base(void)
{
	unsigned int tmp;
	pci_read_config_dword(pci_lpc.bus, pci_lpc.slot, pci_lpc.devfn,
		(0x48), &tmp);
	return tmp;

}

static unsigned int colfax1_getgpio_base(void)
{
	unsigned int tmp;
	pci_read_config_dword(pci_lpc.bus, pci_lpc.slot, pci_lpc.devfn,
		(0x48), &tmp);
	return tmp;

}

static unsigned char get_kirkland_reset_button_status(void)
{
	unsigned int gpiobase;
	unsigned int reg;
	int btn_status;

	gpiobase = (kirkland_getgpio_base() & 0x0000fffe);
	reg = inl_p(gpiobase+0x0c);

	/* Bit clear means button is pressed */
	if ((reg)>>btn_gpi & 0x01){
		btn_status = BUTTON_NOT_DEPRESSED; 
	} else {
		btn_status = BUTTON_DEPRESSED; 
	}

	/* convert the captured data into what makes sense for caller */
	if (btn_status == BUTTON_DEPRESSED)
		return BTN_RESET;
	else
		return 0;
}

// Colfax-I/II  reset fetch begins
static unsigned char get_colfax1_reset_button_status(void)
{
	unsigned int gpiobase;
	unsigned int reg;
	int btn_status;

	gpiobase = (colfax1_getgpio_base() & 0x0000fffe);
	reg = inl_p(gpiobase+0x0c);

	/* Bit clear means button is pressed */
	if ((reg)>>btn_gpi & 0x01){
		btn_status = BUTTON_NOT_DEPRESSED; 
	} else {
		btn_status = BUTTON_DEPRESSED; 
	}

	/* convert the captured data into what makes sense for caller */
	if (btn_status == BUTTON_DEPRESSED)
		return BTN_RESET;
	else
		return 0;
}

/* Button */
#define CFXII_PCH_BASE           0x500

#define CFXII_PCH_BTN_USE_SEL    0x02
#define CFXII_PCH_BTN_IO_SEL     0x06
#define CFXII_PCH_BTN_OFFSET     0x0e
#define CFXII_PCH_BTN_REG        (CFXII_PCH_BASE+CFXII_PCH_BTN_OFFSET)

#define CFXII_RESET_BIT          (1 << 0)
#define CFXII_STATUS_BITMASK     CFXII_RESET_BIT

static unsigned char get_colfax2_reset_button_status(void)
{
	int btn_status;

	/* Bit clear means button is pressed */
	if ( inb_p(CFXII_PCH_BTN_REG) & CFXII_STATUS_BITMASK ) {
		btn_status = BUTTON_NOT_DEPRESSED; 
	} else {
		btn_status = BUTTON_DEPRESSED; 
	}

	/* convert the captured data into what makes sense for caller */
	if (btn_status == BUTTON_DEPRESSED)
		return BTN_RESET;
	else
		return 0;
}
// Colfax-I/II  reset fetch ends

static unsigned char get_westport_mukilteo_reset_button_status(void)
{
	int btn_status;

	/* Bit clear means button is pressed */
	if (westport_mukilteo_chv_gpio_get(&glb_pctrl, GPIO_WG_RESET_BTN) ) {
		btn_status = BUTTON_NOT_DEPRESSED;
	} else {
		btn_status = BUTTON_DEPRESSED;
	}

	/* convert the captured data into what makes sense for caller */
	if (btn_status == BUTTON_DEPRESSED)
		return BTN_RESET;
	else
		return 0;
}

static void seattle_gpio_init(void)
{
	unsigned char tmp;

	/* set multi-function for pin 73 gpio52*/
	tmp = read_w83627_reg(0x09, 0x2d);
	tmp |= 0x04;
	write_w83627_reg(0x09, 0x2d, tmp);

	/* set GPIO37/52 as Output function and enable GPIO3X */

	/* GPIO37 */
	tmp = read_w83627_reg(0x09, 0x30);
	tmp |= 0x0e|GPIO3X;  /* Yes we know 0x0e already contains 0x02 */
	write_w83627_reg(0x09, 0x30, tmp);

	/* GPIO52 */
	tmp = read_w83627_reg(0x09, 0xe0);
	tmp &= ~ARM_GREEN;
	write_w83627_reg(0x09, 0xe0, tmp);

	/* Set GPIO37 as output, GPIO36 as input */
	tmp = read_w83627_reg(0x09, 0xf0);
	tmp &= ~ARM_RED;
	tmp |= GPIO36_BIT;
	write_w83627_reg(0x09, 0xf0, tmp);

	/* ARM the device by turning it red */
	set_led_red();
	return;
}

static void kirkland_gpio_init(void)
{
	unsigned int gpiobase;
	gpiobase = (kirkland_getgpio_base() & 0x0000fffe);

	outl_p((inl_p(gpiobase+0x03))|0x00020000, (gpiobase+0x00));//set GP17 GPIO 
	outl_p((inl_p(gpiobase+0x04))|0x00020000, (gpiobase+0x04));//set GP17 Input 

	return;
}


// Colfax-I setup begins
static void colfax1_gpio_init(void)
{
	unsigned char tmp;
	unsigned int gpiobase;

	/* Button ======================= */
	gpiobase = (colfax1_getgpio_base() & 0x0000fffe);
	outl_p((inl_p(gpiobase+0x03))|0x00020000, (gpiobase+0x00));//set GP17 GPIO 
	outl_p((inl_p(gpiobase+0x04))|0x00020000, (gpiobase+0x04));//set GP17 Input 

        /* LED ========================== */
        tmp = read_w83627_reg(0x00, 0x27);
        tmp &= 0xff;
        tmp |= 0x80;
        write_w83627_reg(0x00, 0x27, tmp);

        tmp = read_w83627_reg(0x09, 0x30);
        tmp &= 0xff;
        tmp |= 0x80;
        write_w83627_reg(0x09, 0x30, tmp);

        tmp = read_w83627_reg(0x07, 0xe0);
        tmp &= 0xf3;
        tmp |= 0x00;
        write_w83627_reg(0x07, 0xe0, tmp);

	return;
}
// Colfax-I setup ends

// Colfax-II setup begins

/* LED */
#define CFXII_PCH_LED_USE_SEL     0x00
#define CFXII_PCH_LED_IO_SEL      0x04
#define CFXII_PCH_LED_OFFSET      0x0c
#define CFXII_PCH_LED_REG         (CFXII_CFXII_PCH_BASE+CFXII_PCH_LED_OFFSET)

/* Outputs active low.
 * Bits defined here are INACTIVE sense for LED.  */
#define CFXII_ARM_GREEN          (1<<4)
#define CFXII_ARM_RED            (1<<5)
#define CFXII_ARM_MASK           (CFXII_ARM_RED|CFXII_ARM_GREEN)


/*
 * Set ARM LED red
 */
static void colfax2_set_led_red()
{
	unsigned int tmp;

	/* RED lo, GREEN hi */
	tmp = inl_p(CFXII_PCH_BASE+CFXII_PCH_LED_OFFSET);
	tmp &= ~CFXII_ARM_RED;
	tmp |= CFXII_ARM_GREEN;
	outl_p(tmp, CFXII_PCH_BASE+CFXII_PCH_LED_OFFSET);

	udelay(1000);

	return;
}

static void colfax2_gpio_init(void)
{
	unsigned int tmp;

	/* Button ======================= */
	/* Select GPIO function */
	tmp = inl_p(CFXII_PCH_BASE+CFXII_PCH_BTN_USE_SEL);
	tmp |= CFXII_RESET_BIT;
	outl_p(tmp, CFXII_PCH_BASE+CFXII_PCH_BTN_USE_SEL);

	/* Set input mode */
	tmp = inl_p(CFXII_PCH_BASE+CFXII_PCH_BTN_IO_SEL);
	tmp |= CFXII_RESET_BIT;
	outl_p(tmp, CFXII_PCH_BASE+CFXII_PCH_BTN_IO_SEL);

	/* LED ========================== */
	/* Select GPIO function */
	tmp = inl_p(CFXII_PCH_BASE+CFXII_PCH_LED_USE_SEL);
	tmp |= CFXII_ARM_MASK;
	outl_p(tmp, CFXII_PCH_BASE+CFXII_PCH_LED_USE_SEL);

	/* Set output mode */
	tmp = inl_p(CFXII_PCH_BASE+CFXII_PCH_LED_IO_SEL);
	tmp &= ~(CFXII_ARM_MASK);
	outl_p(tmp, CFXII_PCH_BASE+CFXII_PCH_LED_IO_SEL);

	/* ARM the device by turning it red */
	colfax2_set_led_red();
}

// Colfax-II setup ends

#define WINTHROP_SUPPORT
#define TWISP_SUPPORT
#if defined(WINTHROP_SUPPORT) || defined(TWISP_SUPPORT)
/***************************************************************************************************************************************/
/* Winthrop M370/M470/M570/M670 GPIO definitions and functions */
/* This part is a fork of //tasks/utm-mainline-winthrop/components/wg_linux/mainline/src/modules/platform/spokane/sled_drv-m370-670.c  */
/*
 *  Platform Depend GPIOs for Reset Button and Status LEDs
 *
 *  The reset button on Intel PCH
 *      PCH GPP_G10      RESET 0 = pressed, 1 = released
 *
 *  Nuvoton NCT6776 SIO definitions for Arm LED
 *      GPIO#  I/O   Notes
 *      -------------------------------------------------
 *      GP72    O    LED_Green (Status LED Green Color)
 *      GP73    O    LED_Red   (Status LED Red Color)
 */

#define WG_ARM_LED                              1     /* ARM */

///////////////////SKYLAKE-PCH////////////////////////
//
//PCH GPIO definition
//
#define PCH_PCR_BASE_ADDRESS                    0xfd000000      // SBREG MMIO base address
#define SB_PCR_ADDRESS(Pid, Offset)             ( ((unsigned int)(Pid) << 16) | ((unsigned short)(Offset)) )
#define GPIOCOM1                                0xae
// GPIO Community 1 Private Configuration Registers
// SKL PCH-H
#define PCH_H_PCR_GPIO_GPP_G_PADCFG_OFFSET      0x6a8           // Reset Button uses GPIO G Group */

#define PADCFG_SET_MODE                         0x1c00          // bit10,11,12 set 0 ,enable GPIO control the pad
#define PADCFG_SET_OUTPUT                       0x100           // bit8 set 0, enable output buffer
#define PADCFG_SET_INPUT                        0x200           // bit9 set 0, enable input buffer

/* Reset Button uses GPIO pin GPP_G10 */
#define SWBTN_GPIO1_NUM                         10

unsigned int gpio_gpp_g_offset(unsigned char gpio_num)
{
    return (SB_PCR_ADDRESS(GPIOCOM1, PCH_H_PCR_GPIO_GPP_G_PADCFG_OFFSET + (8 * gpio_num)));
}

unsigned int read_pch_gpio_reg(unsigned int offset)
{
    unsigned int reg;
    unsigned int addr_base;
    unsigned int *gpio_oe_addr =NULL;
    
    addr_base = PCH_PCR_BASE_ADDRESS;
    gpio_oe_addr = (addr_base + offset);
    reg = *gpio_oe_addr;
    
    return reg;
}

void write_pch_gpio_reg(unsigned int offset, unsigned int value)
{
    unsigned int addr_base;
    unsigned int *gpio_oe_addr =NULL;
    
    addr_base = PCH_PCR_BASE_ADDRESS;
    gpio_oe_addr = (addr_base + offset);
    *gpio_oe_addr = value;
    
    return;
}

void winthrop_btn_gpio_init(void)
{
    unsigned int tmp = 0;
    unsigned int addr;

    addr = gpio_gpp_g_offset(SWBTN_GPIO1_NUM);
    tmp = read_pch_gpio_reg(addr);
    tmp &= ~(PADCFG_SET_MODE);  //enable GPIO pad mode
    tmp |= (PADCFG_SET_OUTPUT | PADCFG_SET_INPUT);//disable output and input
    tmp &= ~PADCFG_SET_INPUT;  //enable input or output
    write_pch_gpio_reg(addr, tmp);
   
    return;
}

unsigned char get_winthrop_reset_button_status(void)
{
    unsigned int btn_status = 0;
    unsigned int addr;
    
    addr = gpio_gpp_g_offset(SWBTN_GPIO1_NUM);
    btn_status = read_pch_gpio_reg(addr);  
    btn_status &= 0x02;
    btn_status = btn_status/0x02; 
    if (btn_status) {
        btn_status = BUTTON_NOT_DEPRESSED;
    } else {
        btn_status = BUTTON_DEPRESSED;
	}

	/* convert the captured data into what makes sense for caller */
    if (btn_status == BUTTON_DEPRESSED) {
        return BTN_RESET;
    } else {
        return 0;
    }	
}

/* Winbond GPIO or something that looks sort of like it */

static void enter_io_config(void)
{
    outb_p(0x87, INDEX_PORT); // Lanner says we must write this twice
    outb_p(0x87, INDEX_PORT);
    return;
}

static void exit_io_config(void)
{
    outb_p(0xaa, INDEX_PORT);
    return;
}

static unsigned char read_io_reg(unsigned char ldn, unsigned char reg)
{
    outb_p(0x07, INDEX_PORT); // LDN Register
    outb_p(ldn, DATA_PORT);   // Select LDNx
    outb_p(reg, INDEX_PORT);  // Select Register
    return inb_p(DATA_PORT);  // Read Register
}

static void write_io_reg(unsigned char ldn, unsigned char reg, unsigned char value)
{
    outb_p(0x07, INDEX_PORT); // LDN Register
    outb_p(ldn, DATA_PORT);   // Select LDNx
    outb_p(reg, INDEX_PORT);  // Select Register
    outb_p(value, DATA_PORT); // Write Register
    return;
}

static void winthrop_sio_init(void)
{
    unsigned char tmp;

    enter_io_config();

    tmp = (read_io_reg(0x00, 0x27) & 0xff) | 0x80;
    write_io_reg(      0x00, 0x27, tmp);

    tmp = (read_io_reg(0x09, 0x30) & 0xff) | 0x80;
    write_io_reg(      0x09, 0x30, tmp);

    tmp = (read_io_reg(0x07, 0xe0) & 0xf3) | 0x00;
    write_io_reg(      0x07, 0xe0, tmp);

    exit_io_config();
}

static void winthrop_gpio_init(void)
{
    winthrop_btn_gpio_init();    /* Reset button GPIO initialization */
    winthrop_sio_init();         /* LED GPIO initialization */
}
#endif /* End of #if defined(WINTHROP_SUPPORT) || defined(TWISP_SUPPORT) */

#ifdef TWISP_SUPPORT
/*
 *  Platform Depend GPIOs for Reset Button and Status LEDs
 *
 *  Nuvoton NCT6776 SIO definitions for Arm LED
 *      GPIO#  I/O   Notes
 *      -------------------------------------------------
 *      GP72    O    LED_Green       (Status LED Green Color)
 *      GP73    O    LED_Red         (Status LED Red Color)
 *      GP74    I    Reset Button    RESET 0 = pressed, 1 = released
 */

static void twisp_gpio_init(void)
{
    unsigned char tmp;

    enter_io_config();

    tmp = (read_io_reg(0x00, 0x27) & 0xff) | 0x80;
    write_io_reg(      0x00, 0x27, tmp);

    tmp = (read_io_reg(0x09, 0x30) & 0xff) | 0x80;
    write_io_reg(      0x09, 0x30, tmp);

    tmp = (read_io_reg(0x07, 0xe0) & 0xf3) | 0x10; /* 0x10: M270 Reset GPIO pin GP74 is set as input */
    write_io_reg(      0x07, 0xe0, tmp);
    
    exit_io_config();
}

unsigned char get_twisp_reset_button_status(void)
{
    unsigned btn_status = 0;
    
    enter_io_config();
    btn_status = (read_io_reg(0x7, 0xe1) & 0x10) / 0x10;  
    exit_io_config();
    
    if (btn_status) {
        btn_status = BUTTON_NOT_DEPRESSED;
    } else {
        btn_status = BUTTON_DEPRESSED;
	}

	/* convert the captured data into what makes sense for caller */
    if (btn_status == BUTTON_DEPRESSED) {
        return BTN_RESET;
    } else {
        return 0;
    }	
}
#endif /* End of #ifdef TWISP_SUPPORT */

/* Find out if the reset button was pressed. */
unsigned char
get_reset_button (void)
{
	if (lcd_module == LCD_MODULE_NONE_SEATTLE) {
            return get_seattle_reset_button_status ();
	} else if (lcd_module == LCD_MODULE_NONE_KIRKLAND) {
            return get_kirkland_reset_button_status ();
	} else if (lcd_module == LCD_MODULE_NONE_COLFAX1) {
            return get_colfax1_reset_button_status ();
	} else if (lcd_module == LCD_MODULE_NONE_COLFAX2) {
            return get_colfax2_reset_button_status ();
	} else if ( (lcd_module == LCD_MODULE_NONE_WESTPORT) || (lcd_module == LCD_MODULE_NONE_MUKILTEO) ) {
            return get_westport_mukilteo_reset_button_status ();
	} else if (lcd_module == LCD_MODULE_NONE_WINTHROP) {
            return get_winthrop_reset_button_status ();
	} else if (lcd_module == LCD_MODULE_NONE_TWISP) {
            return get_twisp_reset_button_status ();
	}
	
	/* Should never be here: incase we are, return err(-1) */
	return -1;
}

int sled_init(void)
{
	if (lcd_module == LCD_MODULE_NONE_SEATTLE) {
	    seattle_gpio_init();
  	    // grub_print("LED/Reset Button Driver %s ready\n", Driver_Version);
	} else if (lcd_module == LCD_MODULE_NONE_KIRKLAND) {
	    /*
   	     * create configuration address as per Intel ® I/O
	     * Controller Hub 7 (ICH7) Family LPC (D31:F0)) is at
	     * bus0 dev31 fun3 Get SMBUS Port Address
  	     */
	    pci_lpc.name = "lpc";
	    pci_lpc.bus = 0x0;
	    pci_lpc.slot = 0x31;
	    pci_lpc.devfn = 0x0;

	    kirkland_gpio_init();
	    udelay(300);
	} else if (lcd_module == LCD_MODULE_NONE_COLFAX1) {
	    /*
   	     * create configuration address as per Intel ® I/O
	     * Controller Hub 7 (ICH7) Family LPC (D31:F0)) is at
	     * bus0 dev31 fun3 Get SMBUS Port Address
  	     */
	    pci_lpc.name = "lpc";
	    pci_lpc.bus = 0x0;
	    pci_lpc.slot = 0x31;
	    pci_lpc.devfn = 0x0;

	    colfax1_gpio_init();
	    udelay(300);
	} else if (lcd_module == LCD_MODULE_NONE_COLFAX2) {
	    colfax2_gpio_init();
	    udelay(300);
	} else if ( (lcd_module == LCD_MODULE_NONE_WESTPORT) || (lcd_module == LCD_MODULE_NONE_MUKILTEO) ) {
	    westport_mukilteo_gpio_init(&glb_pctrl);
	    udelay(300);
	} else if (lcd_module == LCD_MODULE_NONE_WINTHROP) {
	    winthrop_gpio_init();
	    udelay(300);
	} else if (lcd_module == LCD_MODULE_NONE_TWISP) {
	    twisp_gpio_init();
	    udelay(300);
	}
	
	return 0;
}
// vim: ai noet sw=4 ts=4

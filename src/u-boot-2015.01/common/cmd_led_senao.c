#include <common.h>
#include <command.h>

#define GPIO_BASE	0xfe130000
#define GPIO1		(GPIO_BASE+0x0000)
#define GPIO2		(GPIO_BASE+0x1000)
#define GPIO3		(GPIO_BASE+0x2000)
#define GPIO4		(GPIO_BASE+0x3000)
#define GPIO_DIRECTION  0x0
#define GPIO_DATA	0x8

#define GPIO4_DIRECTION (GPIO4+GPIO_DIRECTION)
#define GPIO4_DATA	(GPIO4+GPIO_DATA)
//#define LED_CLR_GPIO
#define LED_DATA_GPIO 	3
#define LED_CLOCK_GPIO 	2

#define LEDNUM 6

#define OUTPUT		1
#define INPUT		0

//#define DEBUG

int led_stat = 1;
int wifi_led_stat = 1;

int myatoi(char *str)
{
	int i = 0, result = 0;

	while(str[i] != '\0') {
		result += (result * 10) + (str[i] - '0');
		i++;
	}
	return result;
}

void gpio_write_bit(ulong addr, int pos, int value)
{
	ulong bytes = 1;
	uint32_t writeval, readval;

	const void *buf = map_sysmem(addr, bytes);

	readval = *(volatile uint32_t *)buf;
#ifdef DEBUG
	printf("0x%08lx - Before modify: 0x%08x\n", addr, readval);
#endif
	if (value == 1)
		writeval = readval | (0x1 << (32 - pos - 1));
	else
		writeval = readval & ~(0x1 << (32 - pos - 1));
#ifdef DEBUG
	printf("0x%08lx - After modify: 0x%08x\n", addr, writeval);
#endif
	*(volatile uint32_t *)buf = writeval;
	unmap_sysmem(buf);
}

int gpio_read_bit(ulong addr, int pos)
{
	ulong bytes = 1;
	uint32_t readvalue = 0;

	const void *buf = map_sysmem(addr, bytes);

	readvalue = *(volatile uint32_t *)buf;
	unmap_sysmem(buf);

	return (int)(readvalue & ( 0x1 << (32 - pos - 1))) >> (32 - pos - 1);
}

static int do_gpio_wb(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	if (argc < 4 || argc > 5)
		return CMD_RET_USAGE;

	gpio_write_bit(simple_strtoul(argv[1], NULL, 16), myatoi(argv[2]) - 1, myatoi(argv[3]));

	return 0;
}

static int do_gpio_rb(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	int value;
	if (argc < 3 || argc > 4)
		return CMD_RET_USAGE;

	value = gpio_read_bit(simple_strtoul(argv[1], NULL, 16), myatoi(argv[2]) - 1);
	if (value != 0 || value != 1)
		return 1;
#ifdef DEBUG
	printf("%d\n", value);
#endif
	return 0;
}

uint usdelay(int us)
{
    int count=0;
    for(count=0; count < us; count++)
    {
        udelay(1000);
    }
    return 0;
}

#ifdef LED_CLR_GPIO
void led_start_symbol(void)
{
	int default_val = gpio_read_bit(GPIO4_DIRECTION, LED_CLR_GPIO);

	gpio_write_bit(GPIO4_DIRECTION, LED_CLR_GPIO, OUTPUT);

	usdelay(1);
	gpio_write_bit(GPIO4_DATA, LED_CLR_GPIO, 1);
	usdelay(1);
	gpio_write_bit(GPIO4_DATA, LED_CLR_GPIO, 0);
	usdelay(1);
	gpio_write_bit(GPIO4_DATA, LED_CLR_GPIO, 1);

	gpio_write_bit(GPIO4_DIRECTION, LED_CLR_GPIO, default_val);
}
#endif

void led_init(void)
{
#ifdef LED_CLR_GPIO
	led_start_symbol();
#endif
}

void SDA(bool val)
{
	gpio_write_bit(GPIO4_DATA, LED_DATA_GPIO, val);
	gpio_write_bit(GPIO4_DIRECTION, LED_DATA_GPIO, OUTPUT);
        gpio_write_bit(GPIO4_DATA, LED_DATA_GPIO, val);
}

void SCL(bool val)
{
	gpio_write_bit(GPIO4_DATA, LED_CLOCK_GPIO, val);
	gpio_write_bit(GPIO4_DIRECTION, LED_CLOCK_GPIO, OUTPUT);
	gpio_write_bit(GPIO4_DATA, LED_CLOCK_GPIO, val);
}

void led_SDA(bool val)
{
	usdelay(1);
	SDA(val);
	usdelay(1);
	SCL(1);
	usdelay(2);
	SCL(0);
}

void led_all_on(void)
{
	led_stat=0;

	led_SDA(1);
	led_SDA(1);
	led_SDA(0);
	led_SDA(0);
	led_SDA(0);
	led_SDA(0);
}

void led_all_off(void)
{
	led_stat = 1;
	wifi_led_stat = 1;

	int i;
	for (i = 0; i < LEDNUM; i++) {
		led_SDA(1);
	}
}

void led_2G(void)
{
	wifi_led_stat = 2;
	led_SDA(1);
	led_SDA(0);
	led_SDA(1);
	led_SDA(1);
	led_SDA(1);
	led_SDA(1);
}

void led_5G(void)
{
	led_SDA(0);
	led_SDA(1);
	led_SDA(1);
	led_SDA(1);
	led_SDA(1);
	led_SDA(1);
}


static int do_led(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	led_init();
	if (argc != 2)
		return CMD_RET_USAGE;

	if (!strcmp(argv[1],"on"))
		led_all_on();
	else if (!strcmp(argv[1],"off") || !strcmp(argv[1],"reset"))
		led_all_off();
	else if (!strcmp(argv[1],"2G"))
		led_2G();	
	else if (!strcmp(argv[1],"5G"))
		led_5G();
	else
		return CMD_RET_USAGE;
	return 0;
}
static int do_ledtest(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	int i=0;
	for (i = 1; i < argc; i++) {
		led_SDA(myatoi(argv[i]));
		printf("%d\n", myatoi(argv[i]));
	}
	return 0;
}


U_BOOT_CMD(ledtest, 9, 1, do_ledtest,"test","test");
U_BOOT_CMD(led, 2, 1, do_led,
	   "led control",
	   " \n"
	   "\tled on \n"
	   "\tled off\n"
	   "\tled 2G \n"
	   "\tled 5G \n"
	   "\tled reset \n");
U_BOOT_CMD(gpiowrite, 4, 1, do_gpio_wb, "write a bit to gpio", "ex: gpiowrite 0xfe133000 15 1");
U_BOOT_CMD(gpioread, 3, 1, do_gpio_rb, "read a bit of gpio", "ex: gpioread 0xfe133000 15");

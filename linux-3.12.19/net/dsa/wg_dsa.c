#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kallsyms.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/device.h>
#include <linux/netdevice.h>
#include <linux/platform_device.h>
#include <linux/rtnetlink.h>
#include <linux/if_vlan.h>
#include <linux/phy.h>
#include <uapi/linux/ip.h>
#include <uapi/linux/ipv6.h>
#include <net/dsa.h>

#include "dsa_priv.h"

#if	defined(CONFIG_NET_DSA_MV88E6XXX) || defined(CONFIG_NET_DSA_MV88E6XXX_MODULE)

#define	TAG_MASK	0xFFFF	// Marvell tag mask (2 byte)
#define	TAG_BRIDGE	0x8000	// Marvell tag base for bridges

#define	MAXSW		2	// Max # of channels to switch chips
#define	MAXETH		8	// Max # of eth   ports

#define	SW0_NAME	"sw10"	// Physical switch device names
#define	SW1_NAME	"sw11"

#define	SW0_ADDR	(0x10)	// Physical switch device MDIO address
#define	SW1_ADDR	(0x11)

#define	map_t		unsigned

// Flag bits for split_mode

#define	F_SW0_ON	0x01
#define	F_SW0_OFF	0x02
#define	F_SW0_LRO	0x04
#define	F_SW0_LLTX	0x08
#define	F_SW0_MARVELL	0x00
#define	F_SW0_BITS	0x7F

#define	F_SW0_DEFAULT	(F_SW0_ON | F_SW0_LLTX | F_SW0_MARVELL)

#define	F_SW1_ON	(0x100 * F_SW0_ON)
#define	F_SW1_OFF	(0x100 * F_SW0_OFF)
#define	F_SW1_LRO	(0x100 * F_SW0_LRO)
#define	F_SW1_LLTX	(0x100 * F_SW0_LLTX)
#define	F_SW1_MARVELL	(0x100 * F_SW0_MARVELL)
#define	F_SW1_BITS	(0x100 * F_SW0_BITS)

#define	F_SW1_DEFAULT	(0x100 * F_SW0_DEFAULT)

// NOTE: For the Chelan the HW layout is like an H, with the two CPU
// RGMII links at the top, 2 switch chips in the middile, cross
// connected with each other. For the Newport/castle it is like a Y.
// The CPU RGMII links connect to the same switch chip. In both cases the
// external PHYs are at the bottom. For the Newport/castle we see the two
// RGMII links going to what seem to the CPU to be 2 switch chips but in
// fact are only 1. The magic to change the MDIO bus address from
// 17 to 16 is done in mv88e6xxx.c. The result of all this is that on
// Newport/castle the single switch chip is programmed twice with the
// same register settings. This was done to make DSA happy.

static	int	NETH;		// Number of eth   ports
static	int	NPORT;		// Number of total ports
static	int	NBRIDGE;	// Number of bridges
static	int	NSWITCH;	// Number of channels to switch chips

static	char*	SW10;		// sw10 original name
static	char*	SW11;		// sw11 original name

static	int	SWETH;		// First eth on switch

static	int	LOPHY;		// LO phy map
static	int	HIPHY;		// HI phy map
static	int	CPUPHY;		// CPU    map
static	int	PVLAN1;		// PVLAN base
static	int	PVLAN7;		// PVLAN mask
static	int	PVLANX;		// PVLAN flag
static	int	CROSS1;		// Cross ports 1
static	int	CROSS2;		// Cross ports 2

static	int	SHIFT;		// Shift for bit map conversions

static	int	PROMISC;	// Turn on PROMISC mode
static	int	RXVLAN;		// RX VLAN tag removal
static	int	USETSO;		// Use TSO

// Switch map data
static	u16	sw0_map[DSA_MAX_PORTS];
static	u16	sw1_map[DSA_MAX_PORTS];

// This layout affects the table of eth port_names below

// One Switch layout, only 1 channel  on CPU port  5
static	u16	one5_init[DSA_MAX_PORTS] = {0x0020, 0x1020, 0x2020, 0x3020, 0x4020,
					    0xF01F, 0xF01F};

// One Switch layout, only 1 channel  on CPU port  6
static	u16	one6_init[DSA_MAX_PORTS] = {0x0040, 0x1040, 0x2040, 0x3040, 0x4040,
					    0xF01F, 0xF01F};

// One Switch layout, dual 2 channels on CPU ports 5/6
static	u16	dual_init[DSA_MAX_PORTS] = {0x0040, 0x1020, 0x2040, 0x3020, 0x4040,
					    0xF01F, 0xF01F};

// Two Switch layout on CPU port 5
static	u16	two0_init[DSA_MAX_PORTS] = {0x0020, 0x1020, 0x2020, 0x3020, 0x7000,
					    0xF05F, 0x7000};

static	u16	two1_init[DSA_MAX_PORTS] = {0x7000, 0x4020, 0x5020, 0x6020, 0x7000,
					    0xF05F, 0x7000};

// One Switch layout, dual 2 channels on CPU ports 8/9
static	u16	phy8_init[DSA_MAX_PORTS] = {0x0100, 0x1200, 0x2100, 0x3200, 0x4100,
					    0x5200, 0x6100, 0x7200, 0xF0FF, 0xF0FF};

// Hook to classify packets packets for wg_queue
extern	int   (*wg_dsa_recv)(struct sk_buff*);

// Switch map data in the kernel itself
extern	u16	wg_dsa_sw0_map[DSA_MAX_PORTS];
extern	u16	wg_dsa_sw1_map[DSA_MAX_PORTS];

// Switch devices
extern	struct	dsa_switch* wg_dsa_sw[MAXSW];

#ifdef	CONFIG_X86
// Pointer to DSA ETH SW  Device
extern struct net_device* wg_dsa_dev[2];
#endif

// Flood and learning flags
extern int	wg_dsa_flood;
extern int	wg_dsa_learning;

// Map of cross mappings (all 0 on Newport/castle)
#define	NCROSS	2

static	map_t	cross_bits[NCROSS] = { 0, 0 };

// eth datebase
static	struct {
	struct	net_device* sw_dev;
	struct	net_device* eth_dev;
	u8	switch_index;
	u8	port_index;
}	eths[MAXETH];

// Mode flags from split_mode
int	flags;	// Mode flags

// Map virtual to actual ports
static u16 remap(u16 map)
{
  return has88E6190 ? map + (map & 0x3FF) : map;
}


// Push the PVLAN mappings into the kernel
static void export_marvell_maps(void)
{
  int j;

  for (j = 0; j < DSA_MAX_PORTS; j++) {
    wg_dsa_sw0_map[j] = remap(sw0_map[j]);
    wg_dsa_sw1_map[j] = remap(sw1_map[j]);
  }
}

#define to_platform_driver(drv)	(container_of((drv), \
				 struct platform_driver, driver))

// Chelan              has  two switch chips so wg_dsa_count == 2
// All other platforms have one switch chip  so wg_dsa_count == 1
//
// Note: The Newport/castle has two connections to the one switch chip
// so in order to utilize both RGMII links we need to pretend that there
// are actually two switch chips, at MDIO addresses 0x10 and 0x11.
// A patch in mv88e6xxx.c maps 0x11 to 0x10 for the actual I/O.

int split_mode = -1;		     // Use defaults
module_param(split_mode, int, 0444);
MODULE_PARM_DESC(split_mode, "Split_Mode -1=default");

// Kernel net_devices for eth devices
static	struct	net_device* net_eth0;
static	struct	net_device* net_eth1;

// Kernel generic devices for eth devices and mdio bus
static	struct	device*     dev_eth0;
static	struct	device*     dev_eth1;
static	struct	device*     dev_mdio;

static	void	mv88e6171_ge_release(struct device* dev)
{
}

// Set up for first switch differs for Newport/castle vs. Chelan
static	struct	dsa_chip_data mv88e6171_ge_switch0_chip_data = {
	.sw_addr	= SW0_ADDR,
};

static	struct	dsa_platform_data mv88e6171_ge_switch0_plat_data = {
	.nr_chips	= 1,
	.chip		= &mv88e6171_ge_switch0_chip_data,
};

static	struct	platform_device mv88e6171_ge_switch0 = {
	.name		= "switch0",
	.id		= -1,
	.num_resources	= 0,
	.dev		= {
	.platform_data	= &mv88e6171_ge_switch0_plat_data,
	.release	= &mv88e6171_ge_release,
	},
};

// Set up for first switch differs for Newport/castle vs. Chelan
static	struct	dsa_chip_data mv88e6171_ge_switch1_chip_data = {
	.sw_addr	= SW1_ADDR,
};

static	struct	dsa_platform_data mv88e6171_ge_switch1_plat_data = {
	.nr_chips	= 1,
	.chip		= &mv88e6171_ge_switch1_chip_data,
};

static	struct	platform_device mv88e6171_ge_switch1 = {
	.name		= "switch1",
	.id		= -1,
	.num_resources	= 0,
	.dev		= {
	.platform_data	= &mv88e6171_ge_switch1_plat_data,
	.release	= &mv88e6171_ge_release,
	},
};

static	int  dsa_change_master_mtu(struct net_device *master, int mtu)
{
  // Check for master
  if (!master) return -ENODEV;

  // Assure we can always handle standard frames
  if (mtu <= ETH_DATA_LEN) mtu = ETH_DATA_LEN;

  // Add in overhead
  mtu += VLAN_HLEN;
  if (strcmp(master->name, SW0_NAME) == 0) // Set SW10 MTU
	mtu += 4;	// 4-byte kernel dsa header
  else
  if (strcmp(master->name, SW1_NAME) == 0) // Set SW11 MTU
    mtu += 4;	// 4-byte kernel dsa header

  // Check if master MTU changed
  if (master->mtu != mtu) {
    dev_set_mtu(master, mtu);
    printk(KERN_INFO "%s: %s MTU %d\n",
           __FUNCTION__, master->name, master->mtu);
  }

  return 0;
}

static	int  dsa_change_mtu(struct net_device *dev, int mtu)
{
  int i, j;
  struct net_device* master_netdev;

  // Range check MTU
  if ((mtu < 68) || (mtu > 9000))
    return -EINVAL;

  // Check for changes
  if (dev->mtu == mtu)
    return -EEXIST;

  // Set MTU
  dev->mtu = mtu;

  master_netdev = NULL;
  for (i = 0; i < NSWITCH; i++) {
	  if (wg_dsa_sw[i]) {
		  for (j = 0; j < NPORT; j++) {
			  if (wg_dsa_sw[i]->ports[j] &&
				  wg_dsa_sw[i]->dst && wg_dsa_sw[i]->dst->master_netdev) {
				  if (strcmp(dev->name, wg_dsa_sw[i]->ports[j]->name) == 0) {
					  printk(KERN_WARNING "@@ netdev: %s found in dsa_switch_tree, master: %s\n",
							 dev->name, wg_dsa_sw[i]->dst->master_netdev->name);
					  master_netdev = wg_dsa_sw[i]->dst->master_netdev;
					  break;
				  }
			  }
		  }
	  }
  }
 
  // Set MTU of master if needed
  return dsa_change_master_mtu(master_netdev, mtu);
}

// Rename an eth device
static	void rename_eth(struct net_device* dev, char* name, int mode)
{
  int err;

  printk(KERN_INFO "wg_dsa_init: Rename %s -> %s\n", dev->name, name);

  rtnl_lock();    // Lock

  dev_close(dev); // Set it down

  err = dev_change_name(dev, name);
  if (err)
    printk(KERN_EMERG "wg_dsa_init: %s change name error %d\n",
           dev->name, err);

  if (mode) {     // Set promisc mode if sw device
    err = dev_set_promiscuity(dev, mode);
    if (err)
      printk(KERN_EMERG "wg_dsa_init: %s set_promiscuity error %d\n",
             dev->name, err);
  }

  // Set MTU if SW device
  if ((strcmp(name, SW0_NAME) == 0) || (strcmp(name, SW1_NAME) == 0))
	dsa_change_master_mtu(dev, dev->mtu);

  rtnl_unlock();  // Unlock
}

#ifdef	CONFIG_WG_ARCH_FREESCALE

// Find mdio bus via a scan walk of the device list
static	int  __init find_mdio(struct device* dev, void* data)
{
  char* name;

  if (wg_get_cpu_model() == WG_CPU_T1024)
    name = "ffe4fc000.mdio";
  else
  if (wg_get_cpu_model() == WG_CPU_T1042)
    name = "ffe4fc000.mdio";
  else
  if (wg_get_cpu_model() == WG_CPU_T2081)
    name = "ffe4fc000.mdio";
  else
  if (wg_get_cpu_model() == WG_CPU_P2020)
    name = "ffe24520.mdio";
  else
    name = "ffe24000.mdio";

  if (strcmp(name, dev_name(dev))) return 0;

  dev_mdio = dev;
  return 1;
}
#endif	// CONFIG_WG_ARCH_FREESCALE

#ifdef	CONFIG_WG_ARCH_X86

extern	struct mii_bus* wg_dsa_bus;

static	int  __init find_mdio(struct device* dev, void* data)
{
  int err;

  if (!wg_dsa_count)	return 1;
  if (!wg_dsa_bus)	return 1;

  // Return if already registered
  if (wg_dsa_bus->state != MDIOBUS_REGISTERED) {

    // Make sure phy functions exist
    if ((!wg_dsa_bus->read) || (!wg_dsa_bus->write)) {
      printk(KERN_EMERG "%s: phy functions missing\n", __FUNCTION__);
      return 1;
    }

    // Disable MDIO scan of upper PHYs for now
    wg_dsa_bus->phy_mask = ~(0x8000);

    // Register the PHY devices on MDIO bus
    if ((err = mdiobus_register(wg_dsa_bus)) < 0) {
      printk(KERN_EMERG "%s: mdiobus_register error %d\n", __FUNCTION__, err);
      return 1;
    }

    // Enable  MDIO scan for later
    wg_dsa_bus->phy_mask =  0;
  }


  printk(KERN_DEBUG "\n");
  printk(KERN_DEBUG "priv      %p\n", wg_dsa_bus->priv);
  printk(KERN_DEBUG "read      %p ",  wg_dsa_bus->read);
  print_symbol(     " %s\n",    (long)wg_dsa_bus->read);
  printk(KERN_DEBUG "write     %p ",  wg_dsa_bus->write);
  print_symbol(     " %s\n",    (long)wg_dsa_bus->write);
  printk(KERN_DEBUG "\n");

  dev_mdio = &wg_dsa_bus->dev;
  return 1;
}
#endif	// CONFIG_WG_ARCH_X86

// Set up the switch devices
static	int  __init wg_dsa_init(void)
{
  int    j, k, rc;
  struct device_driver*   dsa_drv  = NULL;
  struct platform_driver* plat_drv = NULL;

  // Check for at least one switch chip
  if (wg_dsa_count <= 0) {
    printk(KERN_EMERG "%s: No Switch Chips\n", __FUNCTION__);
    return -ENODEV;
  }

  NPORT		= 7;		// Switch has 7 total ports

  // Set up parameters based on number of switch chips
  if (wg_dsa_count > 1) {

    NETH	= 7;		// Chelan has 7 ports

    LOPHY	= 0x0000000F;	// eth0-3 map
    HIPHY	= 0x000E0000;	// eth4-6 map
    PVLAN1	= 0x10001000;	// PVLAN base
    PVLAN7	= 0x70007000;	// PVLAN mask
    PVLANX	= 0x80008000;	// PVLAN flag
    CROSS1	= 0x00400040;	// Cross ports 6 + 6
    CROSS2	= 0x00010010;	// Cross ports 4 + 0
    SHIFT	= (1+(16-4));	// Shift for bit map conversions

    CPUPHY	= 0x00200020;	// CPU on port 5 on both chips
    SW10	= "eth1";	// Lanner swapped these
    SW11	= "eth0";
    SWETH	=     0;
    NSWITCH	=     2;
    PROMISC	=     0;
    RXVLAN	=     0;	// FBX121X-57. For future new platforms, may still need to re-eval
    USETSO	=     0;

  } else {

    NETH	= 5;		// Newport/castle has 5 ports

    LOPHY	= 0x001F;	// eth0-4 map
    HIPHY	= 0x0000;
    PVLAN1	= 0x1000;	// PVLAN base
    PVLAN7	= 0x7000;	// PVLAN mask
    PVLANX	= 0x8000;	// PVLAN flag
    CROSS1	= 0x0000;
    CROSS2	= 0x0000;
    SHIFT	= 0x0000;	// Not used for single switch systems

#ifdef	CONFIG_WG_ARCH_X86

    if (has88E6190)	  {
    NETH	=      8;	// MV88E6190 has 8 ports
    NPORT	=     10;	// MV88E6190 switch has 10 total ports
    LOPHY	= 0x00FF;	// eth0-7 map
    CPUPHY	= 0x0300;	// CPU on ports 8 + 9
    SW10	= "eth0";	// MV88E6190 has two channels
    SW11	= "eth1";
    SWETH	=     0;
    NSWITCH	=     2;
    PROMISC	=     1;
    RXVLAN	=     0;
    USETSO	=     0;
    } else
    if (wg_westport == 2) {
    CPUPHY	= 0x0020;	// CPU on port  5
    SW10	= "eth3";	// Westport (T70) has only one channel
    SWETH	=     3;
    NSWITCH	=     1;
    PROMISC	=     1;
    RXVLAN	=     0;
    USETSO	=     0;
    } else
    if (wg_westport == 1) {
    CPUPHY	= 0x0020;	// CPU on port  5
    SW10	= "eth0";	// Mulkiteo (T55) has only one channel
    SWETH	=     0;
    NSWITCH	=     1;
    PROMISC	=     1;
    RXVLAN	=     0;
    USETSO	=     0;
    }
#endif	// CONFIG_WG_ARCH_X86

#ifdef	CONFIG_WG_ARCH_FREESCALE

    if (wg_boren == 1)	  {
    CPUPHY	= 0x0040;	// CPU on port  6
    SW10	= "eth0";	// Boren has only one channel
    SWETH	=     0;
    NSWITCH	=     1;
    PROMISC	=     0;
    RXVLAN	=     0;	// FBX-14022
    USETSO	=     0;
    } else
    if (wg_boren == 2)	  {
    CPUPHY	= 0x0040;	// CPU on port  6
    SW10	= "eth0";	// Boren has only one channel
    SWETH	=     2;
    NSWITCH	=     1;
    PROMISC	=     0;
    RXVLAN	=     0;	// FBX-14022
    USETSO	=     0;
    } else
    if (isT2081)	  {
    CPUPHY	= 0x0060;	// CPU on ports 5 + 6
    SW10	= "eth4";	// Lanner put switch on last 2 RGMIIs
    SW11	= "eth3";
    SWETH	=     3;
    NSWITCH	=     2;
    PROMISC	=     1;
    RXVLAN	=     1;	// FBX-15461, BUG90500
    USETSO	=     0;
    } else
    if (isT1042)	  {
    CPUPHY	= 0x0060;	// CPU on ports 5 + 6
    SW10	= "eth4";	// Lanner put switch on last 2 RGMIIs
    SW11	= "eth3";
    SWETH	=     3;
    NSWITCH	=     2;
    PROMISC	=     1;
    RXVLAN	=     1;	// FBX-15461, BUG90500
    USETSO	=     0;
    } else
    if (isT1024)	  {
    CPUPHY	= 0x0060;	// CPU on ports 5 + 6
    SW10	= "eth0";
    SW11	= "eth1";
    SWETH	=     0;
    NSWITCH	=     2;
    PROMISC	=     1;
    RXVLAN	=     1;	// FBX-15461
    USETSO	=     0;
    } else		  {
    CPUPHY	= 0x0060;	// CPU on ports 5 + 6
    SW10	= "eth0";	// Sercomm did not swap these
    SW11	= "eth1";
    SWETH	=     0;
    NSWITCH	=     2;
    PROMISC	=     0;
    RXVLAN	=     1;
    USETSO	=     0;
    }
#endif	// CONFIG_WG_ARCH_FREESCALE

    // Set SMI CMD base register
    if (has88E6176 || has88E6190) wg_dsa_smi_reg = 0x18;
  }

  // Check for errors
  if ((NSWITCH <= 0)) {
    printk(KERN_EMERG "%s: NSWITCH %d\n", __FUNCTION__, NSWITCH);
    return -ENODEV;
  }
  if ((NSWITCH >= 1) && !SW10) {
    printk(KERN_EMERG "%s: No SW10\n", __FUNCTION__);
    return -ENODEV;
  }
  if ((NSWITCH >= 2) && !SW11) {
    printk(KERN_EMERG "%s: No SW11\n", __FUNCTION__);
    return -ENODEV;
  }

  // Set number of bridges we can have
  NBRIDGE	= (NETH / 2);	// half the number of eth ports

  // Set up cross bit map
  cross_bits[0] = CROSS1;
  cross_bits[1] = CROSS2;

  // Set up for first  switch
  if (HIPHY)		  {
    mv88e6171_ge_switch0_chip_data.port_names[0] = "eth0";
    mv88e6171_ge_switch0_chip_data.port_names[1] = "eth1";
    mv88e6171_ge_switch0_chip_data.port_names[2] = "eth2";
    mv88e6171_ge_switch0_chip_data.port_names[3] = "eth3";
    mv88e6171_ge_switch0_chip_data.port_names[4] = "dsa";
    mv88e6171_ge_switch0_chip_data.port_names[5] = "cpu";
    mv88e6171_ge_switch0_chip_data.port_names[6] = "dsa";
  } else {
    if ((NETH > 7)) {
    mv88e6171_ge_switch0_chip_data.port_names[0] = "eth0";
    mv88e6171_ge_switch0_chip_data.port_names[1] = "eth1";
    mv88e6171_ge_switch0_chip_data.port_names[2] = "eth2";
    mv88e6171_ge_switch0_chip_data.port_names[3] = "eth3";
    mv88e6171_ge_switch0_chip_data.port_names[8] = "cpu";
    } else
    if ((SWETH == 3) && (NSWITCH == 2))	{
    mv88e6171_ge_switch0_chip_data.port_names[0] = "eth3";
    mv88e6171_ge_switch0_chip_data.port_names[1] = "eth4";
    mv88e6171_ge_switch0_chip_data.port_names[2] = "eth5";
    mv88e6171_ge_switch0_chip_data.port_names[6] = "cpu";
    } else
    if ((SWETH == 0) && (NSWITCH == 2))	{
    mv88e6171_ge_switch0_chip_data.port_names[0] = "eth0";
    mv88e6171_ge_switch0_chip_data.port_names[1] = "eth1";
    mv88e6171_ge_switch0_chip_data.port_names[2] = "eth2";
    mv88e6171_ge_switch0_chip_data.port_names[6] = "cpu";
    } else
    if ((SWETH == 3) && (NSWITCH == 1))	{
    mv88e6171_ge_switch0_chip_data.port_names[0] = "eth3";
    mv88e6171_ge_switch0_chip_data.port_names[1] = "eth4";
    mv88e6171_ge_switch0_chip_data.port_names[2] = "eth5";
    mv88e6171_ge_switch0_chip_data.port_names[3] = "eth6";
    mv88e6171_ge_switch0_chip_data.port_names[4] = "eth7";
    mv88e6171_ge_switch0_chip_data.port_names[6] = "cpu";
    } else
    if ((SWETH == 2) && (NSWITCH == 1))	{
    mv88e6171_ge_switch0_chip_data.port_names[0] = "eth2";
    mv88e6171_ge_switch0_chip_data.port_names[1] = "eth3";
    mv88e6171_ge_switch0_chip_data.port_names[2] = "eth4";
    mv88e6171_ge_switch0_chip_data.port_names[3] = "eth5";
    mv88e6171_ge_switch0_chip_data.port_names[4] = "eth6";
    mv88e6171_ge_switch0_chip_data.port_names[6] = "cpu";
    } else
    if ((SWETH == 0) && (NSWITCH == 1))	{
    mv88e6171_ge_switch0_chip_data.port_names[0] = "eth0";
    mv88e6171_ge_switch0_chip_data.port_names[1] = "eth1";
    mv88e6171_ge_switch0_chip_data.port_names[2] = "eth2";
    mv88e6171_ge_switch0_chip_data.port_names[3] = "eth3";
    mv88e6171_ge_switch0_chip_data.port_names[4] = "eth4";
    mv88e6171_ge_switch0_chip_data.port_names[6] = "cpu";
    }
  }    

  // Set up for second switch
  if (SW11)  {
  if (HIPHY) {
    mv88e6171_ge_switch1_chip_data.port_names[0] = "dsa";
    mv88e6171_ge_switch1_chip_data.port_names[1] = "eth4";
    mv88e6171_ge_switch1_chip_data.port_names[2] = "eth5";
    mv88e6171_ge_switch1_chip_data.port_names[3] = "eth6";
    mv88e6171_ge_switch1_chip_data.port_names[5] = "cpu";
    mv88e6171_ge_switch1_chip_data.port_names[6] = "dsa";
  } else {
    if ((NETH > 7)) {
    mv88e6171_ge_switch1_chip_data.port_names[4] = "eth4";
    mv88e6171_ge_switch1_chip_data.port_names[5] = "eth5";
    mv88e6171_ge_switch1_chip_data.port_names[6] = "eth6";
    mv88e6171_ge_switch1_chip_data.port_names[7] = "eth7";
    mv88e6171_ge_switch1_chip_data.port_names[9] = "cpu";
    } else
    if ((SWETH == 3) && (NSWITCH == 2))	{
    mv88e6171_ge_switch1_chip_data.port_names[3] = "eth6";
    mv88e6171_ge_switch1_chip_data.port_names[4] = "eth7";
    mv88e6171_ge_switch1_chip_data.port_names[5] = "cpu";
    } else
    if ((SWETH == 0) && (NSWITCH == 2))	{
    mv88e6171_ge_switch1_chip_data.port_names[3] = "eth3";
    mv88e6171_ge_switch1_chip_data.port_names[4] = "eth4";
    mv88e6171_ge_switch1_chip_data.port_names[5] = "cpu";
    }
  }
  }

  // Use split_mode of -1 to get the defaults
  if (split_mode < 0) {
    split_mode  = F_SW0_DEFAULT;
    if (SW11)
    split_mode |= F_SW1_DEFAULT;
  }

  // Allow only defined flags bits
  split_mode &= (F_SW0_BITS | (SW11 ? F_SW1_BITS : 0));

  // Set flags to split_mode
  flags = split_mode;

  printk(KERN_INFO "\n%s: Built " __DATE__ " " __TIME__
         "CPU %4d SW %d Flags %x\n\n", __FUNCTION__,
         wg_get_cpu_model() % 10000, wg_dsa_count, flags);

  // Rename the 2 offline interfaces if they exist
  if ((SWETH == 0) && (NSWITCH == 1)) {
    struct net_device* net_off;
    if ((net_off = dev_get_by_name(&init_net, "eth1")))
      rename_eth(net_off, "off1", 0);
    if ((net_off = dev_get_by_name(&init_net, "eth2")))
      rename_eth(net_off, "off2", 0);
  }

  // Rename the 2 offline interfaces if they exist
  if (NETH > 7)    {
    struct net_device* net_off;
    if ((net_off = dev_get_by_name(&init_net, "eth2")))
      rename_eth(net_off, "off2", 0);
    if ((net_off = dev_get_by_name(&init_net, "eth3")))
      rename_eth(net_off, "off3", 0);
  }

  // Get DSA driver
  dsa_drv = driver_find("dsa", &platform_bus_type);
  if (!dsa_drv) {
    printk(KERN_EMERG "%s: dsa driver not found\n", __FUNCTION__);
    return -ENOENT;
  }

  // Get DSA platform driver
  plat_drv = to_platform_driver(dsa_drv);
  if (!plat_drv) {
    printk(KERN_EMERG "%s: dsa platform driver not found\n", __FUNCTION__);
    return -ENOENT;
  }

  // Find the MDIO bus
  bus_find_device(&platform_bus_type, NULL, NULL, find_mdio);
  if (!dev_mdio) {
    printk(KERN_EMERG "%s: mdio not found\n", __FUNCTION__);
    return -ENOENT;
  }

  printk(KERN_INFO "%s: mdio found %s\n", __FUNCTION__, dev_name(dev_mdio));

  // Find the first eth device
  net_eth0 = dev_get_by_name(&init_net, SW10);
  if (net_eth0) {

    // Set LRO and LLTX if requested
    if (flags & F_SW0_LRO)
      net_eth0->features |= NETIF_F_LRO;

    if (flags & F_SW0_LLTX)
      net_eth0->features |= NETIF_F_LLTX;

    // net_eth0->vlan_features = net_eth0->features;

    dev_eth0 = &net_eth0->dev;
    dev_put(net_eth0); // Put hold on the device

  } else {
    printk(KERN_EMERG "%s: %s not found\n", __FUNCTION__, SW10);
    return -ENOENT;
  }

  // Find the second eth device
  if (SW11) {
  net_eth1 = dev_get_by_name(&init_net, SW11);
  if (net_eth1) {

    // Set LRO and LLTX if requested
    if (flags & F_SW1_LRO)
      net_eth1->features |= NETIF_F_LRO;

    if (flags & F_SW1_LLTX)
      net_eth1->features |= NETIF_F_LLTX;

    // net_eth1->vlan_features = net_eth1->features;

    dev_eth1 = &net_eth1->dev;
    dev_put(net_eth1); // Put hold on the device

  } else {
    printk(KERN_EMERG "%s: %s not found\n", __FUNCTION__, SW11);
    return -ENOENT;
  }
  }

  // Set up initial switch mapping
  if (HIPHY) {
      memcpy(sw0_map, two0_init, sizeof(sw0_map));
      memcpy(sw1_map, two1_init, sizeof(sw1_map));
  } else {
    if (NETH > 7) {
      memcpy(sw0_map, phy8_init, sizeof(sw0_map));
      memcpy(sw1_map, phy8_init, sizeof(sw1_map));
    } else
    if (SW11) {
      memcpy(sw0_map, dual_init, sizeof(sw0_map));
      memcpy(sw1_map, dual_init, sizeof(sw1_map));
    } else {
    if (CPUPHY == 0x20)
      memcpy(sw0_map, one5_init, sizeof(sw0_map));
    else
    if (CPUPHY == 0x40)
      memcpy(sw0_map, one6_init, sizeof(sw0_map));
    }
  }

  // Rename original eths to get them out of the way of new slave devices
  if (HIPHY) {
    rename_eth(net_eth1, "eth10", 0);
    rename_eth(net_eth0, "eth11", 0);
  } else {
    rename_eth(net_eth0, "eth10", 0);
    if (SW11)
    rename_eth(net_eth1, "eth11", 0);
  }

  // Rename eth1, eth2 down since eth0 was moved out of the way
  if ((SWETH == 2) && (NSWITCH == 1)) {
    struct net_device* net_eth;
    if ((net_eth = dev_get_by_name(&init_net, "eth1")))
      rename_eth(net_eth, "eth0", 0);
    if ((net_eth = dev_get_by_name(&init_net, "eth2")))
      rename_eth(net_eth, "eth1", 0);
  }

  if (HIPHY)
  if ((flags & 0x10000)) {

    // Special cross connect test case
    // Send data from one switch chip to
    // The other and then to the CPU

    sw0_map[3] = 0x3040;
    sw0_map[6] = 0x3008;
    sw1_map[3] = 0x6001;
    sw1_map[0] = 0x6008;

    mv88e6171_ge_switch0_chip_data.port_names[4] = "eth6";
    mv88e6171_ge_switch0_chip_data.port_names[3] = "dsa03";
    mv88e6171_ge_switch0_chip_data.port_names[6] = "dsa06";
    mv88e6171_ge_switch1_chip_data.port_names[6] = "eth3";
    mv88e6171_ge_switch1_chip_data.port_names[3] = "dsa13";
    mv88e6171_ge_switch1_chip_data.port_names[0] = "dsa10";
  }

  // Export the initial mapping
  if (flags & (F_SW0_ON | F_SW1_ON))
    export_marvell_maps();

  // Set up switch 0
  if (flags &  F_SW0_ON) {
#ifdef	CONFIG_X86
	wg_dsa_dev[0] = net_eth0;
#endif	  
    // Add devices to platform device data structs
    mv88e6171_ge_switch0_plat_data.netdev  = dev_eth0;
    mv88e6171_ge_switch0_chip_data.mii_bus = dev_mdio;

    // Insert the switch platform device
    platform_device_register(&mv88e6171_ge_switch0);
    rc = plat_drv->probe(&mv88e6171_ge_switch0);

    printk(KERN_INFO "%s: Split sw10 code %d dsa %p\n",
           __FUNCTION__, rc, wg_dsa_sw[0]);

    // Return if not found
    if ((rc < 0) || (!wg_dsa_sw[0])) {
      platform_device_unregister(&mv88e6171_ge_switch0);
      return -ENOLINK;
    }

    // Rename to sw10 to not conflict with slave names
    rename_eth(net_eth0, SW0_NAME, PROMISC);
  }

  if (flags &  F_SW1_ON) {
#ifdef	CONFIG_X86
	wg_dsa_dev[1] = net_eth1;
#endif
    // Add devices to platform device data structs
    mv88e6171_ge_switch1_plat_data.netdev  = dev_eth1;
    mv88e6171_ge_switch1_chip_data.mii_bus = dev_mdio;

    // Insert the switch platform device
    platform_device_register(&mv88e6171_ge_switch1);
    rc = plat_drv->probe(&mv88e6171_ge_switch1);

    printk(KERN_INFO "%s: Split sw11 code %d dsa %p\n",
           __FUNCTION__, rc, wg_dsa_sw[1]);

    // Return if not found
    if ((rc < 0) || (!wg_dsa_sw[1])) {
      platform_device_unregister(&mv88e6171_ge_switch0);
      platform_device_unregister(&mv88e6171_ge_switch1);
      return -ENOLINK;
    }

    // Rename to sw11 to not conflict with slave names
    rename_eth(net_eth1, SW1_NAME, PROMISC);

    // Optimize port layout by interleaving between the two channels
    if (!HIPHY) {
      if (NETH > 7)   {
        {
          mv88e6171_ge_switch0_chip_data.port_names[4] = "eth4";
          mv88e6171_ge_switch1_chip_data.port_names[1] = "eth1";
        }

        mv88e6171_ge_switch1_chip_data.port_names[4] =  NULL;
        mv88e6171_ge_switch0_chip_data.port_names[1] =  NULL;

        wg_dsa_sw[0]->ports[4] = wg_dsa_sw[1]->ports[4];
        wg_dsa_sw[1]->ports[4] = NULL;
        wg_dsa_sw[1]->ports[1] = wg_dsa_sw[0]->ports[1];
        wg_dsa_sw[0]->ports[1] = NULL;

        {
          mv88e6171_ge_switch0_chip_data.port_names[6] = "eth6";
          mv88e6171_ge_switch1_chip_data.port_names[3] = "eth3";
        }

        mv88e6171_ge_switch1_chip_data.port_names[6] =  NULL;
        mv88e6171_ge_switch0_chip_data.port_names[3] =  NULL;

        wg_dsa_sw[0]->ports[6] = wg_dsa_sw[1]->ports[6];
        wg_dsa_sw[1]->ports[6] = NULL;
        wg_dsa_sw[1]->ports[3] = wg_dsa_sw[0]->ports[3];
        wg_dsa_sw[0]->ports[3] = NULL;

      } else
      if (NSWITCH == 2) {
        if (SWETH == 3) {
          mv88e6171_ge_switch0_chip_data.port_names[4] = "eth7";
          mv88e6171_ge_switch1_chip_data.port_names[1] = "eth4";
        } else {
          mv88e6171_ge_switch0_chip_data.port_names[4] = "eth4";
          mv88e6171_ge_switch1_chip_data.port_names[1] = "eth1";
        }

        mv88e6171_ge_switch1_chip_data.port_names[4] =  NULL;
        mv88e6171_ge_switch0_chip_data.port_names[1] =  NULL;

        wg_dsa_sw[0]->ports[4] = wg_dsa_sw[1]->ports[4];
        wg_dsa_sw[1]->ports[4] = NULL;
        wg_dsa_sw[1]->ports[1] = wg_dsa_sw[0]->ports[1];
        wg_dsa_sw[0]->ports[1] = NULL;
      }
    }
  }

  // For each switch
  for (j = 0; j < NSWITCH; j++)
  if (wg_dsa_sw[j]) {
    struct dsa_switch* ds = wg_dsa_sw[j];
    struct net_device* sw_dev;

    // Check for dst field created
    if (!ds->dst) {
      wg_dsa_sw[j] = NULL;
      continue;
    }

    // Check for master device field created
    if (!(sw_dev = ds->dst->master_netdev)) {
      wg_dsa_sw[j] = NULL;
      continue;
    }

    sw_dev->flags |= IFF_NOARP; // WG:JB BUG92677/92760 No ARPs on sw devices

    // Some debug info of use
    printk(KERN_DEBUG "%s: %s master %p slave %p",
           __FUNCTION__, sw_dev->name,
           ds->master_mii_bus, ds->slave_mii_bus);
	if (console_loglevel >= 8)
    print_symbol(" %s\n", (long)ds->drv->setup);
    printk(KERN_DEBUG "%s: [%2d] %s dsa %02x  phy %02x features %8x\n",
           __FUNCTION__, sw_dev->ifindex, sw_dev->name,
           ds->dsa_port_mask,  ds->phys_port_mask,
           (unsigned)sw_dev->features);

    // For each port on switch
    for (k = 0; k < NPORT; k++)
    if (wg_dsa_sw[j]->ports[k]) {
      struct net_device* eth_dev = wg_dsa_sw[j]->ports[k];
      struct net_device_ops* ops = (struct net_device_ops*)eth_dev->netdev_ops;
      struct dsa_slave_priv*   p = netdev_priv(eth_dev);

      // Set dsa switch for this port
      p->parent = ds;

      // Set required flags for each slave device
      eth_dev->features |=  NETIF_F_SG;
      if (wg_dpa_bug) {
      eth_dev->features &= ~NETIF_F_IP_CSUM;
      eth_dev->features &= ~NETIF_F_IPV6_CSUM;
      } else {
      eth_dev->features |=  NETIF_F_IP_CSUM;
      eth_dev->features |=  NETIF_F_IPV6_CSUM;
      }
      eth_dev->features |=  NETIF_F_HIGHDMA;
      eth_dev->features |=  NETIF_F_GSO;

      // Set TSO off for X86 based platforms
      if (!USETSO)
      eth_dev->vlan_features &= ~NETIF_F_ALL_TSO;

      // Set VLAN RX offload since all platforms have or fake it
      eth_dev->features      |=  NETIF_F_HW_VLAN_CTAG_RX;
      eth_dev->vlan_features |=  NETIF_F_HW_VLAN_CTAG_RX;

      // Set slave flag on this device and all its VLANs
      eth_dev->features      |=  NETIF_F_SW_SLAVE;
      eth_dev->vlan_features |=  NETIF_F_SW_SLAVE;

      // Set change MTU function
      if (ops->ndo_change_mtu == NULL)
          ops->ndo_change_mtu =  dsa_change_mtu;
      else
      if (ops->ndo_change_mtu != dsa_change_mtu)
        printk(KERN_EMERG "%s: [%2d] %s change mtu exists\n",
               __FUNCTION__, eth_dev->ifindex, eth_dev->name);

      // Build the eths database
      if (strncmp(eth_dev->name, "eth", 3) == 0) {
	unsigned n = -1;

	sscanf(&eth_dev->name[3], "%d", &n);

	n -= SWETH;

        if ((n >= 0) && (n < NETH)) {
	  eths[n].switch_index = j;
	  eths[n].port_index   = k;
	  eths[n].eth_dev      = eth_dev;
	  eths[n].sw_dev       = sw_dev;
	}
      }

      // Some debug info of use
      printk(KERN_DEBUG "%s: sw1%d port%d  [%2d]  %-5s  features %8x\n",
             __FUNCTION__, j, k, eth_dev->ifindex, eth_dev->name,
             (unsigned)eth_dev->features);
    }

    // Turn off HW VLAN RX removal on sw interfaces due to Marvell tags
    if (!RXVLAN)  // JIRA FBX-2293
      sw_dev->wanted_features = sw_dev->features & ~NETIF_F_HW_VLAN_CTAG_RX;
	
    // Update changed features
    rtnl_lock(); // WG:JB FBX-2249
    netdev_update_features(sw_dev);
    rtnl_unlock();
  }

  // Print useful debug info from eths database
  for (j = 0; j < NETH; j++)
  if (eths[j].eth_dev)
    printk(KERN_DEBUG "%s: [%2d] %-5s  sw1%d  port%d  features %8x\n", __FUNCTION__,
	   eths[j].eth_dev->ifindex, eths[j].eth_dev->name,
	   eths[j].switch_index,     eths[j].port_index,
	   (unsigned)eths[j].eth_dev->features);

  // Return all done
  return 0;
}

module_init(wg_dsa_init);

MODULE_LICENSE("GPL");

#endif	// CONFIG_NET_DSA_MV88E6XXX

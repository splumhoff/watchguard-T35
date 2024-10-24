// If no kernel config options assume user mode unit test

#ifndef	CONFIG_MODULES
#define	UNIT_TEST
#endif

#ifdef	UNIT_TEST

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/types.h>

#define	CONFIG_NET	1

#define	u8		__u8
#define	u16		__u16
#define	u32		__u32
#define	u64		__u64

#define	VLAN_N_VID	4096
#define	VLAN_VID_MASK	4095

#define	printk		printf

#define	KERN_EMERG	""
#define	KERN_INFO	""
#define	KERN_DEBUG	""

#define	IFNAMSIZ	20

#define	cpu_to_be16(a)		htons(a)
#define	copy_from_user(a,b,c)	(strncpy((a),(b),(c))==NULL)
#define	simple_strtoul		simple_strtoull

struct	net_device {
	int		ifindex;
	char		name[IFNAMSIZ];
};

struct	file {
	char*		dummy;
};

struct	skbuff {
	struct net_device* dev;
};

static	inline u64 simple_strtoull(char* str, char** end, int base)
{
  u64    value;
  sscanf(str, "%llx", &value);
  return value;
}

#else	// UNIT_TEST

// This is the PSS kernel module

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/kallsyms.h>
#include <linux/console.h>
#include <linux/device.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/rtnetlink.h>
#include <linux/if_vlan.h>
#include <linux/phy.h>
#include <net/ip.h>

#endif	// UNIT_TEST

#ifdef	CONFIG_NET

#define	NPHY		(24)		// We have 24 port  switch
#define	NSGMII		(4)		// We have  4 SGMII ports
#define	NPORT		(NPHY+NSGMII)	// Total switch ports
#define	NBRIDGE		(NPHY/2)	// Max bridges we handle

#if     (NPORT < 24)
#define	map_t		u32
#else
#define	map_t		u64
#endif

#define	ONE		((map_t)1)

#define	PSS_HLEN	4	   // PSS header tag size

#define	PSS_TAG_HI	0x8E	   // Special tag 0x8E8E
#define	PSS_TAG_LO	0x8E

#define	PSS_PORT_MASK	((ONE<<NPHY)-1)  // Port mask

#define	PSS_PVID_BASE	(VLAN_N_VID-32)   // (2^5  == 32) > 24
#define	PSS_PVID_MASK	(VLAN_VID_MASK-PSS_PVID_BASE) // 31
#define	PSS_PVID_DROP	(VLAN_VID_MASK+1) // (2^12 == 4096)

#define	ETH_P_PSS	((PSS_TAG_HI << 8) | PSS_TAG_LO)

#define	I2P(i)		((i) - pss_ifx)
#define	P2S(p)		pss_portsw[(p)]
#define	I2S(i)		pss_portsw[I2P(i)]

#define	REG(p,x)	(0xA800000 + ((p) * 0x400) + (x))

// Debug flags
static	u32		pss_debug;

// Show register
static	u32		pss_show_reg;

// Delay for packet bursts in usecs
static	int		pss_udelay;

// ifindex of first PSS port
static	int		pss_ifx;

// Load balance
static	int		pss_balance;

// SGMII   bit map
static	map_t		pss_sgmii;

// ETHs  bit map
static	map_t		pss_eths;

// Bridged bit map
static	map_t		pss_bridged;

// Primary bit map
static	map_t		pss_primary;

// Dropped bit map
static	map_t		pss_dropped;

// HW bridge bit maps
static	map_t		pss_bridge[NBRIDGE];

// Switch load
static	int		pss_swload[NSGMII];

// Switch device table
struct	net_device*	pss_switch[NSGMII];

// Slave  device table
struct	net_device*	pss_device[NPHY];

// Header tags for each port
struct	tag_S	{
  u16	s_tag;
  u16	p_vid;
}			pss_header[NPHY];

// Map port to switch number
static	int		pss_portsw[NPHY];

// Map port to queue  number
static	int		pss_queue[NPHY];

// Spin lock for mdic access
static	spinlock_t	pss_mdic_lock;

// Data rates structure
struct	rates_S	{
  long	rx_packets;
  long	tx_packets;
  long	rx_bytes;
  long	tx_bytes;
};

// Counters for eth and sw ports
static	struct rates_S	pss_phy_rates[NPHY];
static	struct rates_S	pss_cpu_rates[NSGMII];

// PVID bit maps
static	map_t		pss_vidmap[NPHY];

// Event strings for phys and cpus
static	u8		pss_phys[NPHY+1];
static	u8		pss_cpus[NPHY+1];

// Count of bridge updates
static	unsigned long	pss_updates = 0;
static	unsigned long	pss_updated = 0;

// Event task
struct	task_struct*	pss_task;

// Return next load balanced switch device
static	inline int balance(void)
{
  if ( ++pss_balance >= NSGMII )
         pss_balance = 0;
  return pss_balance;
}

// Return lowest eth device number in a bit mask map
static	inline int first_eth(map_t map)
{
  int j;

  // Get the first bit and return its position is any
  for (j = 0; map; map = map >> 1, j++) if (map & 1) return j;

  return -1;
}

// Insert port
static	void pss_insert_port(int vlan, int port)
{
  if (!(pss_debug & 4)) return;

  if (vlan == port)
    printk(KERN_DEBUG "%s: vlan%02d create\n",     __FUNCTION__, vlan);
  else
    printk(KERN_DEBUG "%s: vlan%02d insert %2d\n", __FUNCTION__, vlan, port);
}

// Remove port
static	void pss_remove_port(int vlan, int port)
{
  if (!(pss_debug & 4)) return;

  if (vlan == port)
    printk(KERN_DEBUG "%s: vlan%02d delete\n",     __FUNCTION__, vlan);
  else
    printk(KERN_DEBUG "%s: vlan%02d remove %2d\n", __FUNCTION__, vlan, port);
}

// Add bridges
static	void pss_add_bridges(void)
{
  int j, k, v, x;
  map_t  b,    old[NPHY];
  struct tag_S tag[NPHY];

  // Make copy of tags
  memcpy(tag, pss_header, sizeof(tag));

  // Reset to nothing bridged
  for (j = 0; j < NPHY; j++)
  tag[j].p_vid = cpu_to_be16(PSS_PVID_BASE + j);

  // Clear maps
  pss_primary = pss_dropped = pss_bridged = 0;

  // Clear drop flag on first device, set on the rest
  for (k = 0; k < NBRIDGE; k++) if (pss_bridge[k])
  for (b = 1, v = j = 0; j < NPHY; j++, b += b)
  if (pss_bridge[k] & b) {
    if (v)
      pss_dropped |= b;
    else {
      pss_primary |= b;
      v = PSS_PVID_BASE + j;
    }
    tag[j].p_vid = cpu_to_be16(v);
  }

  pss_bridged = pss_primary | pss_dropped;

  // Set global HW bridging bit maps
  wg_br_primary = (((wg_br_map)pss_primary) << pss_ifx);
  wg_br_dropped = (((wg_br_map)pss_dropped) << pss_ifx);
  wg_br_bridged = (((wg_br_map)pss_bridged) << pss_ifx);

  // Set/Clear the DROP flag
  for (b = 1, j = 0; j < NPHY; j++, b += b)
  if (pss_dropped & b)
    tag[j].p_vid |=  cpu_to_be16(PSS_PVID_DROP);
  else
    tag[j].p_vid &= ~cpu_to_be16(PSS_PVID_DROP);

  // Copy new PVID tags
  memcpy(pss_header, tag, sizeof(tag));

  // Copy old PVID bit map
  memcpy(old, pss_vidmap, sizeof(old));

  // Build new PVID bit map
  for (k = 0; k < NPHY; k++) {
    pss_vidmap[k] = 0;
    for (b = 1, j = 0; j < NPHY; j++, b += b)
    if ((cpu_to_be16(tag[j].p_vid) & PSS_PVID_MASK) == k) {
      pss_vidmap[k] |= b;
    }
  }

  // Walk bit map to apply removes one by one
  for (k = 0; k < NPHY; k++)
  if (pss_vidmap[k] != old[k]) {
    for (b = 1, j = 0; j < NPHY; j++, b += b)
    if ((pss_vidmap[k] & b) == 0)
    if ((old[k] & b) != 0) {
      pss_swload[pss_portsw[j]]--;
      pss_remove_port(k, j);
    }
  }

  // Walk bit map to apply inserts one by one
  for (k = 0; k < NPHY; k++)
  if (pss_vidmap[k] != old[k]) {

    // Remove what we are keeping
    for (b = 1, j = 0; j < NPHY; j++, b += b)
    if ((pss_vidmap[k] & b) != 0)
    if ((old[k] & b) != 0)
      pss_swload[pss_portsw[j]]--;

    // Find least loaded switch
    for (v = (NPHY + 1), x = -1, j = 0; j < NSGMII; j++)
    if (pss_swload[j] < v)
      v = pss_swload[x = j];

    // Insert the port
    for (b = 1, j = 0; j < NPHY; j++, b += b)
    if ((pss_vidmap[k] & b) != 0) {
      pss_portsw[j] = x;
      pss_swload[x]++;
      if ((old[k] & b) == 0)
        pss_insert_port(k, j);
    }
  }

  // Construct new event maps
  for (j = 0; j < NPHY; j++) {
    k = cpu_to_be16(pss_header[j].p_vid);
    pss_phys[j] = 'A' + (k & PSS_PVID_MASK);
    pss_cpus[j] = '0' +  pss_portsw[j];
  }

  // Bump updates
  pss_updates++;

  if (pss_debug & 1)
  printk(KERN_DEBUG "%s: phys %s cpus %s\n",
         __FUNCTION__, pss_phys, pss_cpus);
}

// Add a bridge to the existing set
static	int pss_set_bridge(map_t map, int op)
{
  int   j, k;
  map_t new[NBRIDGE];

  // Mask off unused bits
  map &= PSS_PORT_MASK;

  // Make copy of bridges
  memcpy(new, pss_bridge, sizeof(new));

  for (k = -1, j = NBRIDGE; --j >= 0;) {

    // Clear new map bits from all existing bridges
    new[j] &= ~(map);

    // Clear bridge if bridged to self
    if ((new[j] & (new[j]-1)) == 0) new[j] = 0;

    // Remember first free slot
    if ((new[j])	      == 0) k = j;
  }

  // Insert a bridge, if there is room
  if (op >= 0)
  if (map & (map-1)) {
    if (k < 0)
      return -ENOMEM;
    new[k] = map;
  }

  // Did we actually change anything?
  if (memcmp(new, pss_bridge, sizeof(new)) != 0) {
    // Update active bridges
    memcpy(pss_bridge, new, sizeof(new));
    return 1;
  }

  return 0;
}

static	int pss_bridging_enabled = 1;

// Print the bridge data for testing purposes
static	int proc_read_bridge(char* page, char** start, off_t off,
			     int count,  int* eof,     void* data)
{
  int j, z = 0;

  if (pss_bridging_enabled <= 0)
    return sprintf(page, "Bridging disabled\n");

  if (pss_bridged)
    z += sprintf(&page[z], "bridged  %06lX\n", (long)pss_bridged);
  if (pss_primary)
    z += sprintf(&page[z], "primary  %06lX\n", (long)pss_primary);
  if (pss_dropped)
    z += sprintf(&page[z], "dropped  %06lX\n", (long)pss_dropped);

  for (j = 0; j < NBRIDGE; j++)
  if (pss_bridge[j])
    z += sprintf(&page[z], "bridge%-2d %06lX\n",
                 j, (long)pss_bridge[j]);

  for (j = 0; j < NPHY; j++)
  if (pss_header[j].p_vid != cpu_to_be16(PSS_PVID_BASE + j))
    z += sprintf(&page[z], "eth%-2d =  eth%-2d   sw%d   %04X\n",    j + 1,
                 (cpu_to_be16(pss_header[j].p_vid)  &  PSS_PVID_MASK) + 1,
                 pss_portsw[j], cpu_to_be16(pss_header[j].p_vid));

  return z;
}

// The user mode interface is:
//
// echo "hex bit map of bridged eths" >/proc/wg_pss/bridge
//
// Example: echo  6 >/proc/wg_pss/bridge will bridge eth2+eth3
// Example: echo 1C >/proc/wg_pss/bridge will bridge eth3+eth4+eth5
//
// Note: This will remove those interfaces from all other bridges.
// Thus if eth1+eth2 are bridged and you bridge eth2+eth3 eth1
// becomes unbridged. To add an interface to a bridge, send the
// complete bit map of the new bridge, i.e. eth1+eth2+eth3
//
// To remove an eth from all bridges use a bit map with just the
// single bit on for that interface, i.e bridge it to itself.
//
// Example: echo  4 >/proc/wg_pss/bridge removes eth3 from all bridges

// Set up a new bridge
static	int proc_write_bridge(struct file* file,   const char* buffer,
 			      unsigned long count, void* data)
{
  int    err;
  char   str[256], *strend;
  u64    bridge_map   = 0;
  int    len = (count > (sizeof(str)-1)) ? sizeof(str)-1 : count;
  static long changes = 0;

  // Get string from user and null terminate it
  if (copy_from_user(str, buffer, len))
    return -EFAULT;
  str[len] = 0;

  // Check if we are enabling/disabling bridging
  if (str[0] == 'Y') {
    pss_bridging_enabled = 1;
    return count;
  } else
  if (str[0] == 'N') {
    pss_bridging_enabled = 0;
    return count;
  }

  // Check if bridging is disabled
  if (pss_bridging_enabled <= 0)
    return -EPERM;

  // Parse the bridge map
  bridge_map = simple_strtoull(&str[(str[0] >= '0') ? 0 : 1], &strend, 16);

  // Check if caller requested a shift by the ifindex of eth1
  if (strchr(str, '^'))
    bridge_map >>= pss_ifx;

  // If we have a bridge map
  if (bridge_map) {
    // Setup the bridges
    err = pss_set_bridge((map_t)bridge_map, (str[0] == '-') ? -1 : 1);
    if (err < 0)
      return err;

    // Add the bridges and count up changes if any
    if (err > 0) {
      pss_add_bridges();
      changes++;
    }
  }

  // Wake up cpss daemon
  if (pss_task) wake_up_process(pss_task);

  // Return ENODATA if nothing changed
  if (changes == 0)
    return -ENODATA;
  else
    changes = 0;

  return count;
}

#ifdef	UNIT_TEST

static	inline int TEST(char* s)
{
  int    j, z;
  static char page[4096];

  printf("\nTest %s\n\n", s);

  z = proc_write_bridge(NULL, s, strlen(s), NULL);
  if (z > 0)
    z = proc_read_bridge(page, NULL, 0, 0, NULL, NULL);
  if (z > 0) {
    page[z] = 0;
    printf("%s\n", page);
  }

  printf("Load ");
  for (j = 0; j < NSGMII; j++) printf("%3d", pss_swload[j]);
  printf("\n");

  return z;
}

int main(int argc, char** argv)
{
  int  nphy, nsgmii, tag, id;
  int  fd = open("/proc/wg_pss/config", 0);
  char line[32] = {0};

  if (fd > 0) {
    if (read(fd, line, sizeof(line) - 1) > 0)
      if (sscanf(line, "%d %d %x %x %d",
                 &nphy, &nsgmii, &tag, &id, &pss_ifx) < 1)
        pss_ifx = 0;
    close(fd);
  }

  printf("NPHY %2d NSGMII %2d TAG %4X ID %4X IFX %2d\n",
         nphy, nsgmii, tag, id, pss_ifx);

  while (--argc > 0) TEST((++argv)[0]);
}

#else

// PSS setup registers from igb/src/e100_phy.c

#define	S INT_MIN

struct	{
  int off;
  u32 val;
} pss_reg[] = {
  {0x00000058 | 0, 0x04104002}, // Add this line from TTPan
  // Prefer Master, SMI1 AutoNeg preformed.
  {0x04004034 | 0, 0x00000140},
  {0x05004034 | 0, 0x00000140},
  // set SMI0 fast_mdc to div/64
  {0x04004200 | 0, 0x000B0000},
  // set SMI1 fast_mdc to div/64
  {0x05004200 | 0, 0x00030000},
  // set PHY polling address 88E1680 0x0~0xF, 0x10~0x17
  {0x04004030 | 0, 0x0A418820},
  {0x04804030 | 0, 0x16A4A0E6},
  {0x05004030 | 0, 0x2307B9AC},
  {0x05804030 | 0, 0x2F6AD272},
  // Pipe0 access DFX Setting- Pipe Select
  {0x308F8004 | 0, 0x00000001},
  // PCL TCAM-DFX Setting
  {0x30804050 | 0, 0x00020003},
  // RTR TCAM-DFX Setting
  {0x3080C050 | 0, 0x00020003},
  // changed in xCat2, GPP PAD control
  {0x008F8304 | 0, 0x00000048},
  // changed in xCat2, SSMII PAD control
  {0x008F8314 | 0, 0x00000048},
  // changed in xCat2, RGMII PAD control
  {0x008F8310 | 0, 0x00000048},
  // changed in xCat2, LED PAD control
  {0x008F8300 | 0, 0x00000048},
  // disable PECL receiver and common_0_PECL_EN=CMOS
  {0x0000009C | 0, 0x061B0CC3},
  // Set Extended Control Register - TTPan: SGMII 2.5G bit22 & bit14 = 1 -> Org=0x00807405 New=0x00C07405
  {0x0000005C | 0, 0x00807405},
  // Disable SSMII CLK
  {0x00000028 | 0, 0x2000000},
  {0x00000028 | 0, 0x2000000},
  // Start QSGMII Initial
  // Power up 5G SERDESs
  // Set SERDES ref clock register
  {0x09800000 | 0, 0x00003E80},
  {0x09800400 | 0, 0x00003E80},
  {0x09801000 | 0, 0x00003E80},
  {0x09801400 | 0, 0x00003E80},
  {0x09802000 | 0, 0x00003E80},
  {0x09802400 | 0, 0x00003E80},
  // Wait 10mSec,
  {0x000000F0 | 0, 0x00000000},
  {0x000000F0 | 0, 0x00000001},
  // Deactivate sd_reset
  {0x09800004 | 0, 0x00000008},
  {0x09800404 | 0, 0x00000008},
  {0x09801004 | 0, 0x00000008},
  {0x09801404 | 0, 0x00000008},
  {0x09802004 | 0, 0x00000008},
  {0x09802404 | 0, 0x00000008},
  // Wait for Calibration done (0x09800008 bit 3)
  // Wait 10mSec,
  {0x000000F0 | 0, 0x00000000},
  {0x000000F0 | 0, 0x00000001},
  // Reference Ferquency select = 62.5MHz ; Use registers bits to control speed configuration
  {0x0980020C | 0, 0x0000800A},
  {0x0980060C | 0, 0x0000800A},
  {0x0980120C | 0, 0x0000800A},
  {0x0980160C | 0, 0x0000800A},
  {0x0980220C | 0, 0x0000800A},
  {0x0980260C | 0, 0x0000800A},
  // Transmitter/Reciver Divider force, interpulator force; 1.25G: intpi = 25uA , VCO divided by 4 ; 2.5G: intpi = 25uA , VCO divided by 2  ; 3.125G: intpi = 30uA , VCO divided by 2 ; 3.75G: intpi = 20uA , VCO not divided; 6.25G: intpi = 30uA , VCO not divided; 5.15G: intpi = 25uA , VCO not divided
  {0x09800210 | 0, 0x00004414},
  {0x09800610 | 0, 0x00004414},
  {0x09801210 | 0, 0x00004414},
  {0x09801610 | 0, 0x00004414},
  {0x09802210 | 0, 0x00004414},
  {0x09802610 | 0, 0x00004414},
  // Force FbDiv/RfDiv
  {0x09800214 | 0, 0x0000A150},
  {0x09800614 | 0, 0x0000A150},
  {0x09801214 | 0, 0x0000A150},
  {0x09801614 | 0, 0x0000A150},
  {0x09802214 | 0, 0x0000A150},
  {0x09802614 | 0, 0x0000A150},
  // Force: PLL Speed, sel_v2i, loadcap_pll,sel_fplres
  {0x09800218 | 0, 0x0000BAAB},
  {0x09800618 | 0, 0x0000BAAB},
  {0x09801218 | 0, 0x0000BAAB},
  {0x09801618 | 0, 0x0000BAAB},
  {0x09802218 | 0, 0x0000BAAB},
  {0x09802618 | 0, 0x0000BAAB},
  // icp force
  {0x0980021C | 0, 0x0000882C},
  {0x0980061C | 0, 0x0000882C},
  {0x0980121C | 0, 0x0000882C},
  {0x0980161C | 0, 0x0000882C},
  {0x0980221C | 0, 0x0000882C},
  {0x0980261C | 0, 0x0000882C},
  //  0 = kvco-2
  {0x098003CC | 0, 0x00002000},
  {0x098007CC | 0, 0x00002000},
  {0x098013CC | 0, 0x00002000},
  {0x098017CC | 0, 0x00002000},
  {0x098023CC | 0, 0x00002000},
  {0x098027CC | 0, 0x00002000},
  // External TX/Rx Impedance changed from 6 to 0 while auto calibration results are used  - based on lab measurments it seems that we need to force the auto imedance calibration values
  {0x0980022C | 0, 0x00000000},
  {0x0980062C | 0, 0x00000000},
  {0x0980122C | 0, 0x00000000},
  {0x0980162C | 0, 0x00000000},
  {0x0980222C | 0, 0x00000000},
  {0x0980262C | 0, 0x00000000},
  // Auto KVCO,  PLL is not forced to max speed during power up sequence -
  {0x09800230 | 0, 0x00000000},
  {0x09800630 | 0, 0x00000000},
  {0x09801230 | 0, 0x00000000},
  {0x09801630 | 0, 0x00000000},
  {0x09802230 | 0, 0x00000000},
  {0x09802630 | 0, 0x00000000},
  // Sampler OS Scale was changed from 5mV/Step to 3.3mV/Step; RX_IMP_VTHIMCAL was chnge from 3 to 0
  {0x09800234 | 0, 0x00004000},
  {0x09800634 | 0, 0x00004000},
  {0x09801234 | 0, 0x00004000},
  {0x09801634 | 0, 0x00004000},
  {0x09802234 | 0, 0x00004000},
  {0x09802634 | 0, 0x00004000},
  // Use value wiritten to register for process calibration instead of th eauto calibration; Select process from register
  {0x0980023C | 0, 0x00000018},
  {0x0980063C | 0, 0x00000018},
  {0x0980123C | 0, 0x00000018},
  {0x0980163C | 0, 0x00000018},
  {0x0980223C | 0, 0x00000018},
  {0x0980263C, 0x00000018},
  // DCC should be dissabled at baud 3.125 and below = 8060
  {0x09800250 | 0, 0x0000A0C0},
  {0x09800650 | 0, 0x0000A0C0},
  {0x09801250 | 0, 0x0000A0C0},
  {0x09801650 | 0, 0x0000A0C0},
  {0x09802250 | 0, 0x0000A0C0},
  {0x09802650 | 0, 0x0000A0C0},
  // Wait 10mSec,
  {0x000000F0 | 0, 0x00000000},
  {0x000000F0 | 0, 0x00000001},
  // DCC should be dissabled at baud 3.125 and below = 8060
  {0x09800250 | 0, 0x0000A060},
  {0x09800650 | 0, 0x0000A060},
  {0x09801250 | 0, 0x0000A060},
  {0x09801650 | 0, 0x0000A060},
  {0x09802250 | 0, 0x0000A060},
  {0x09802650 | 0, 0x0000A060},
  // PE Setting
  {0x09800254 | 0, 0x00007F2D},
  {0x09800654 | 0, 0x00007F2D},
  {0x09801254 | 0, 0x00007F2D},
  {0x09801654 | 0, 0x00007F2D},
  {0x09802254 | 0, 0x00007F2D},
  {0x09802654 | 0, 0x00007F2D},
  // PE Type
  {0x09800258 | 0, 0x00000100},
  {0x09800658 | 0, 0x00000100},
  {0x09801258 | 0, 0x00000100},
  {0x09801658 | 0, 0x00000100},
  {0x09802258 | 0, 0x00000100},
  {0x09802658 | 0, 0x00000100},
  // selmupi/mupf - low value for lower baud
  {0x0980027C | 0, 0x000090AA},
  {0x0980067C | 0, 0x000090AA},
  {0x0980127C | 0, 0x000090AA},
  {0x0980167C | 0, 0x000090AA},
  {0x0980227C | 0, 0x000090AA},
  {0x0980267C | 0, 0x000090AA},
  // DTL_FLOOP_EN = Dis
  {0x09800280 | 0, 0x00000800},
  {0x09800680 | 0, 0x00000800},
  {0x09801280 | 0, 0x00000800},
  {0x09801680 | 0, 0x00000800},
  {0x09802280 | 0, 0x00000800},
  {0x09802680 | 0, 0x00000800},
  // FFE Setting DB 24G is 0x363
  {0x0980028C | 0, 0x00000377},
  {0x0980068C | 0, 0x00000377},
  {0x0980128C | 0, 0x00000377},
  {0x0980168C | 0, 0x00000377},
  {0x0980228C | 0, 0x00000377},
  {0x0980268C | 0, 0x00000377},
  // Slicer Enable; Tx  Imp was changed from 50ohm to 43ohm
  {0x0980035C | 0, 0x0000423F},
  {0x0980075C | 0, 0x0000423F},
  {0x0980135C | 0, 0x0000423F},
  {0x0980175C | 0, 0x0000423F},
  {0x0980235C | 0, 0x0000423F},
  {0x0980275C | 0, 0x0000423F},
  // Not need to be configure - Same as default
  {0x09800364 | 0, 0x00005555},
  {0x09800764 | 0, 0x00005555},
  {0x09801364 | 0, 0x00005555},
  {0x09801764 | 0, 0x00005555},
  {0x09802364 | 0, 0x00005555},
  {0x09802764 | 0, 0x00005555},
  // Disable ana_clk_det
  {0x0980036C | 0, 0x00000000},
  {0x0980076C | 0, 0x00000000},
  {0x0980136C | 0, 0x00000000},
  {0x0980176C | 0, 0x00000000},
  {0x0980236C | 0, 0x00000000},
  {0x0980276C | 0, 0x00000000},
  // Configure rx_imp_vthimpcal to 0x0 (default value = 0x3); Configure Sampler_os_scale to 3.3mV/step (default value = 5mV/step)
  {0x09800234 | 0, 0x00004000},
  {0x09800634 | 0, 0x00004000},
  {0x09801234 | 0, 0x00004000},
  {0x09801634 | 0, 0x00004000},
  {0x09802234 | 0, 0x00004000},
  {0x09802634 | 0, 0x00004000},
  // Configure IMP_VTHIMPCAL to 56.7ohm (default value = 53.3 ohm); Configure cal_os_ph_rd to 0x60 (default value = 0x0); Configure Cal_rxclkalign90_ext to use an external ovride value
  {0x09800228 | 0, 0x0000DAC0},
  {0x09800628 | 0, 0x0000DAC0},
  {0x09801228 | 0, 0x0000DAC0},
  {0x09801628 | 0, 0x0000DAC0},
  {0x09802228 | 0, 0x0000DAC0},
  {0x09802628 | 0, 0x0000DAC0},
  // Reset dtl_rx ; Enable ana_clk_det
  {0x0980036C | 0, 0x00008040},
  {0x0980076C | 0, 0x00008040},
  {0x0980136C | 0, 0x00008040},
  {0x0980176C | 0, 0x00008040},
  {0x0980236C | 0, 0x00008040},
  {0x0980276C | 0, 0x00008040},
  // Un reset dtl_rx
  {0x0980036C | 0, 0x00008000},
  {0x0980076C | 0, 0x00008000},
  {0x0980136C | 0, 0x00008000},
  {0x0980176C | 0, 0x00008000},
  {0x0980236C | 0, 0x00008000},
  {0x0980276C | 0, 0x00008000},
  // Wait 10mSec,
  {0x000000F0 | 0, 0x00000000},
  {0x000000F0 | 0, 0x00000001},
  // ?
  {0x09800224 | 0, 0x00000000},
  {0x09800624 | 0, 0x00000000},
  {0x09801224 | 0, 0x00000000},
  {0x09801624 | 0, 0x00000000},
  {0x09802224 | 0, 0x00000000},
  {0x09802624 | 0, 0x00000000},
  // CAL Start
  {0x09800224 | 0, 0x00008000},
  {0x09800624 | 0, 0x00008000},
  {0x09801224 | 0, 0x00008000},
  {0x09801624 | 0, 0x00008000},
  {0x09802224 | 0, 0x00008000},
  {0x09802624 | 0, 0x00008000},
  {0x09800224 | 0, 0x00000000},
  {0x09800624 | 0, 0x00000000},
  {0x09801224 | 0, 0x00000000},
  {0x09801624 | 0, 0x00000000},
  {0x09802224 | 0, 0x00000000},
  {0x09802624 | 0, 0x00000000},
  // Wait for RxClk_x2
  // Wait 10mSec
  {0x000000F0 | 0, 0x00000000},
  {0x000000F0 | 0, 0x00000001},
  // Set RxInit to 0x1 (remember that bit 3 is already set to 0x1)
  {0x09800004 | 0, 0x00000018},
  {0x09800404 | 0, 0x00000018},
  {0x09801004 | 0, 0x00000018},
  {0x09801404 | 0, 0x00000018},
  {0x09802004 | 0, 0x00000018},
  {0x09802404 | 0, 0x00000018},
  // Wait for p_clk = 1 and p_clk = 0
  // Wait 10mSec
  {0x000000F0 | 0, 0x00000000},
  {0x000000F0 | 0, 0x00000001},
  // Set RxInit to 0x0
  {0x09800004 | 0, 0x00000008},
  {0x09800404 | 0, 0x00000008},
  {0x09801004 | 0, 0x00000008},
  {0x09801404 | 0, 0x00000008},
  {0x09802004 | 0, 0x00000008},
  {0x09802404 | 0, 0x00000008},
  // Wait for ALL PHY_RDY = 1 (0x09800008 bit 0)
  // Wait 10mSec
  {0x000000F0 | 0, 0x00000000},
  {0x000000F0 | 0, 0x00000001},
  // ?
  {0x09800004 | 0, 0x00000028},
  {0x09800404 | 0, 0x00000028},
  {0x09801004 | 0, 0x00000028},
  {0x09801404 | 0, 0x00000028},
  {0x09802004 | 0, 0x00000028},
  {0x09802404 | 0, 0x00000028},
  // Wait 10mSec
  {0x000000F0 | 0, 0x00000000},
  {0x000000F0 | 0, 0x00000001},
  // MAC control
  {0x0a800000 | 0, 0x0000c801},
  {0x0a800400 | 0, 0x0000c801},
  {0x0a800800 | 0, 0x0000c801},
  {0x0a800c00 | 0, 0x0000c801},
  {0x0a801000 | 0, 0x0000c801},
  {0x0a801400 | 0, 0x0000c801},
  {0x0a801800 | 0, 0x0000c801},
  {0x0a801c00 | 0, 0x0000c801},
  {0x0a802000 | 0, 0x0000c801},
  {0x0a802400 | 0, 0x0000c801},
  {0x0a802800 | 0, 0x0000c801},
  {0x0a802c00 | 0, 0x0000c801},
  {0x0a803000 | 0, 0x0000c801},
  {0x0a803400 | 0, 0x0000c801},
  {0x0a803800 | 0, 0x0000c801},
  {0x0a803c00 | 0, 0x0000c801},
  {0x0a804000 | 0, 0x0000c801},
  {0x0a804400 | 0, 0x0000c801},
  {0x0a804800 | 0, 0x0000c801},
  {0x0a804c00 | 0, 0x0000c801},
  {0x0a805000 | 0, 0x0000c801},
  {0x0a805400 | 0, 0x0000c801},
  {0x0a805800 | 0, 0x0000c801},
  {0x0a805c00 | 0, 0x0000c801},
  //?
  {0x0A800008 | 0, 0x0000C008},
  {0x0A801008 | 0, 0x0000C008},
  {0x0A802008 | 0, 0x0000C008},
  {0x0A803008 | 0, 0x0000C008},
  {0x0A804008 | 0, 0x0000C008},
  {0x0A805008 | 0, 0x0000C008},
  {0x0A800408 | 0, 0x0000C008},
  {0x0A801408 | 0, 0x0000C008},
  {0x0A802408 | 0, 0x0000C008},
  {0x0A803408 | 0, 0x0000C008},
  {0x0A804408 | 0, 0x0000C008},
  {0x0A805408 | 0, 0x0000C008},
  {0x0A800808 | 0, 0x0000C008},
  {0x0A801808 | 0, 0x0000C008},
  {0x0A802808 | 0, 0x0000C008},
  {0x0A803808 | 0, 0x0000C008},
  {0x0A804808 | 0, 0x0000C008},
  {0x0A805808 | 0, 0x0000C008},
  {0x0A800C08 | 0, 0x0000C008},
  {0x0A801C08 | 0, 0x0000C008},
  {0x0A802C08 | 0, 0x0000C008},
  {0x0A803C08 | 0, 0x0000C008},
  {0x0A804C08 | 0, 0x0000C008},
  {0x0A805C08 | 0, 0x0000C008},
  // MAC AN speed/duplex/FC
  {0x0A80000C | 0, 0x0000BAE9},
  {0x0A80100C | 0, 0x0000BAE9},
  {0x0A80200C | 0, 0x0000BAE9},
  {0x0A80300C | 0, 0x0000BAE9},
  {0x0A80400C | 0, 0x0000BAE9},
  {0x0A80500C | 0, 0x0000BAE9},
  {0x0A80040C | 0, 0x0000BAE9},
  {0x0A80140C | 0, 0x0000BAE9},
  {0x0A80240C | 0, 0x0000BAE9},
  {0x0A80340C | 0, 0x0000BAE9},
  {0x0A80440C | 0, 0x0000BAE9},
  {0x0A80540C | 0, 0x0000BAE9},
  {0x0A80080C | 0, 0x0000BAE9},
  {0x0A80180C | 0, 0x0000BAE9},
  {0x0A80280C | 0, 0x0000BAE9},
  {0x0A80380C | 0, 0x0000BAE9},
  {0x0A80480C | 0, 0x0000BAE9},
  {0x0A80580C | 0, 0x0000BAE9},
  {0x0A800C0C | 0, 0x0000BAE9},
  {0x0A801C0C | 0, 0x0000BAE9},
  {0x0A802C0C | 0, 0x0000BAE9},
  {0x0A803C0C | 0, 0x0000BAE9},
  {0x0A804C0C | 0, 0x0000BAE9},
  {0x0A805C0C | 0, 0x0000BAE9},
  // back pressure en
  {0x0A800014 | 0, 0x000008D4},
  {0x0A801014 | 0, 0x000008D4},
  {0x0A802014 | 0, 0x000008D4},
  {0x0A803014 | 0, 0x000008D4},
  {0x0A804014 | 0, 0x000008D4},
  {0x0A805014 | 0, 0x000008D4},
  {0x0A800414 | 0, 0x000008D4},
  {0x0A801414 | 0, 0x000008D4},
  {0x0A802414 | 0, 0x000008D4},
  {0x0A803414 | 0, 0x000008D4},
  {0x0A804414 | 0, 0x000008D4},
  {0x0A805414 | 0, 0x000008D4},
  {0x0A800814 | 0, 0x000008D4},
  {0x0A801814 | 0, 0x000008D4},
  {0x0A802814 | 0, 0x000008D4},
  {0x0A803814 | 0, 0x000008D4},
  {0x0A804814 | 0, 0x000008D4},
  {0x0A805814 | 0, 0x000008D4},
  {0x0A800C14 | 0, 0x000008D4},
  {0x0A801C14 | 0, 0x000008D4},
  {0x0A802C14 | 0, 0x000008D4},
  {0x0A803C14 | 0, 0x000008D4},
  {0x0A804C14 | 0, 0x000008D4},
  {0x0A805C14 | 0, 0x000008D4},
  // EEE - Bit_0= 0: Disable LPI, 1: Enable LPI
  {0x0A8000C4 | 0, 0x00000101},
  {0x0A8004C4 | 0, 0x00000101},
  {0x0A8008C4 | 0, 0x00000101},
  {0x0A800CC4 | 0, 0x00000101},
  {0x0A8010C4 | 0, 0x00000101},
  {0x0A8014C4 | 0, 0x00000101},
  {0x0A8018C4 | 0, 0x00000101},
  {0x0A801CC4 | 0, 0x00000101},
  {0x0A8020C4 | 0, 0x00000101},
  {0x0A8024C4 | 0, 0x00000101},
  {0x0A8028C4 | 0, 0x00000101},
  {0x0A802CC4 | 0, 0x00000101},
  {0x0A8030C4 | 0, 0x00000101},
  {0x0A8034C4 | 0, 0x00000101},
  {0x0A8038C4 | 0, 0x00000101},
  {0x0A803CC4 | 0, 0x00000101},
  {0x0A8040C4 | 0, 0x00000101},
  {0x0A8044C4 | 0, 0x00000101},
  {0x0A8048C4 | 0, 0x00000101},
  {0x0A804CC4 | 0, 0x00000101},
  {0x0A8050C4 | 0, 0x00000101},
  {0x0A8054C4 | 0, 0x00000101},
  {0x0A8058C4 | 0, 0x00000101},
  {0x0A805CC4 | 0, 0x00000101},
  // ?
  {0x0A8000C8 | 0, 0x0000017D},
  {0x0A8004C8 | 0, 0x0000017D},
  {0x0A8008C8 | 0, 0x0000017D},
  {0x0A800CC8 | 0, 0x0000017D},
  {0x0A8010C8 | 0, 0x0000017D},
  {0x0A8014C8 | 0, 0x0000017D},
  {0x0A8018C8 | 0, 0x0000017D},
  {0x0A801CC8 | 0, 0x0000017D},
  {0x0A8020C8 | 0, 0x0000017D},
  {0x0A8024C8 | 0, 0x0000017D},
  {0x0A8028C8 | 0, 0x0000017D},
  {0x0A802CC8 | 0, 0x0000017D},
  {0x0A8030C8 | 0, 0x0000017D},
  {0x0A8034C8 | 0, 0x0000017D},
  {0x0A8038C8 | 0, 0x0000017D},
  {0x0A803CC8 | 0, 0x0000017D},
  {0x0A8040C8 | 0, 0x0000017D},
  {0x0A8044C8 | 0, 0x0000017D},
  {0x0A8048C8 | 0, 0x0000017D},
  {0x0A804CC8 | 0, 0x0000017D},
  {0x0A8050C8 | 0, 0x0000017D},
  {0x0A8054C8 | 0, 0x0000017D},
  {0x0A8058C8 | 0, 0x0000017D},
  {0x0A805CC8 | 0, 0x0000017D},
  // END QSGMII Initial
  // Start SGMII Initial
  // Power up 1.25G SERDESs
  // Set SERDES ref clock register
  {0x09803000 | 0, 0x0000BE80},
  {0x09803400 | 0, 0x0000BE80},
  {0x09804000 | 0, 0x0000BE80},
  {0x09804400 | 0, 0x0000BE80},
  // Wait 10mSec
  {0x000000F0 | 0, 0x00000000},
  {0x000000F0 | 0, 0x00000001},
  // Deactivate sd_reset
  {0x09803004 | 0, 0x00000008},
  {0x09803404 | 0, 0x00000008},
  {0x09804004 | 0, 0x00000008},
  {0x09804404 | 0, 0x00000008},
  // Wait for Calibration done (0x09800008 bit 3)
  // Wait 10mSec
  {0x000000F0 | 0, 0x00000000},
  {0x000000F0 | 0, 0x00000001},
  // Reference Ferquency select = 62.5MHz   ;Use registers bits to control speed configuration
  {0x0980320C | 0, 0x0000800A},
  {0x0980360C | 0, 0x0000800A},
  {0x0980420C | 0, 0x0000800A},
  {0x0980460C | 0, 0x0000800A},
  // Transmitter/Reciver Divider force, interpulator force; 1.25G: intpi = 25uA , VCO divided by 4 ; 2.5G: intpi = 25uA , VCO divided by 2  ; 3.125G: intpi = 30uA , VCO divided by 2 ; 3.75G: intpi = 20uA , VCO not divided; 6.25G: intpi = 30uA , VCO not divided; 5.15G: intpi = 25uA , VCO not divided
  {0x09803210 | 0, 0x00006614},
  {0x09803610 | 0, 0x00006614},
  {0x09804210 | 0, 0x00006614},
  {0x09804610 | 0, 0x00006614},
  // Force FbDiv/RfDiv
  {0x09803214 | 0, 0x0000A150},
  {0x09803614 | 0, 0x0000A150},
  {0x09804214 | 0, 0x0000A150},
  {0x09804614 | 0, 0x0000A150},
  // Force: PLL Speed, sel_v2i, loadcap_pll,sel_fplres
  {0x09803218 | 0, 0x0000BAAB},
  {0x09803618 | 0, 0x0000BAAB},
  {0x09804218 | 0, 0x0000BAAB},
  {0x09804618 | 0, 0x0000BAAB},
  // icp force
  {0x0980321C | 0, 0x00008B2C},
  {0x0980361C | 0, 0x00008B2C},
  {0x0980421C | 0, 0x00008B2C},
  {0x0980461C | 0, 0x00008B2C},
  //  0 = kvco-2
  {0x098033CC | 0, 0x00002000},
  {0x098037CC | 0, 0x00002000},
  {0x098043CC | 0, 0x00002000},
  {0x098047CC | 0, 0x00002000},
  // External TX/Rx Impedance changed from 6 to 0 while auto calibration results are used  - based on lab measurments it seems that we need to force the auto imedance calibration values
  {0x0980322C | 0, 0x00000000},
  {0x0980362C | 0, 0x00000000},
  {0x0980422C | 0, 0x00000000},
  {0x0980462C | 0, 0x00000000},
  // Auto KVCO,  PLL is not forced to max speed during power up sequence -
  {0x09803230 | 0, 0x00000000},
  {0x09803630 | 0, 0x00000000},
  {0x09804230 | 0, 0x00000000},
  {0x09804630 | 0, 0x00000000},
  // Sampler OS Scale was changed from 5mV/Step to 3.3mV/Step; RX_IMP_VTHIMCAL was chnge from 3 to 0
  {0x09803234 | 0, 0x00004000},
  {0x09803634 | 0, 0x00004000},
  {0x09804234 | 0, 0x00004000},
  {0x09804634 | 0, 0x00004000},
  // Use value wiritten to register for process calibration instead of th eauto calibration; Select process from register
  {0x0980323C | 0, 0x00000018},
  {0x0980363C | 0, 0x00000018},
  {0x0980423C | 0, 0x00000018},
  {0x0980463C | 0, 0x00000018},
  // DCC should be dissabled at baud 3.125 and below = 8060
  {0x09803250 | 0, 0x000080C0},
  {0x09803650 | 0, 0x000080C0},
  {0x09804250 | 0, 0x000080C0},
  {0x09804650 | 0, 0x000080C0},
  // Wait 10mSec
  {0x000000F0 | 0, 0x00000000},
  {0x000000F0 | 0, 0x00000001},
  // DCC should be dissabled at baud 3.125 and below = 8060
  {0x09803250 | 0, 0x00008060},
  {0x09803650 | 0, 0x00008060},
  {0x09804250 | 0, 0x00008060},
  {0x09804650 | 0, 0x00008060},
  // PE Setting
  {0x09803254 | 0, 0x0000770A},
  {0x09803654 | 0, 0x0000770A},
  {0x09804254 | 0, 0x0000770A},
  {0x09804654 | 0, 0x0000770A},
  // PE Type
  {0x09803258 | 0, 0x00000000},
  {0x09803658 | 0, 0x00000000},
  {0x09804258 | 0, 0x00000000},
  {0x09804658 | 0, 0x00000000},
  // selmupi/mupf - low value for lower baud
  {0x0980327C | 0, 0x0000905A},
  {0x0980367C | 0, 0x0000905A},
  {0x0980427C | 0, 0x0000905A},
  {0x0980467C | 0, 0x0000905A},
  // DTL_FLOOP_EN = Dis
  {0x09803280 | 0, 0x00000800},
  {0x09803680 | 0, 0x00000800},
  {0x09804280 | 0, 0x00000800},
  {0x09804680 | 0, 0x00000800},
  // FFE Setting
  {0x0980328C | 0, 0x00000266},
  {0x0980368C | 0, 0x00000266},
  {0x0980428C | 0, 0x00000266},
  {0x0980468C | 0, 0x00000266},
  // Slicer Enable; Tx  Imp was changed from 50ohm to 43ohm
  {0x0980335C | 0, 0x0000423F},
  {0x0980375C | 0, 0x0000423F},
  {0x0980435C | 0, 0x0000423F},
  {0x0980475C | 0, 0x0000423F},
  // Not need to be configure - Same as default
  {0x09803364 | 0, 0x00005555},
  {0x09803764 | 0, 0x00005555},
  {0x09804364 | 0, 0x00005555},
  {0x09804764 | 0, 0x00005555},
  // Disable ana_clk_det
  {0x0980336C | 0, 0x00000000},
  {0x0980376C | 0, 0x00000000},
  {0x0980436C | 0, 0x00000000},
  {0x0980476C | 0, 0x00000000},
  // Configure rx_imp_vthimpcal to 0x0 (default value = 0x3); Configure Sampler_os_scale to 3.3mV/step (default value = 5mV/step)
  {0x09803234 | 0, 0x00004000},
  {0x09803634 | 0, 0x00004000},
  {0x09804234 | 0, 0x00004000},
  {0x09804634 | 0, 0x00004000},
  // Configure IMP_VTHIMPCAL to 56.7ohm (default value = 53.3 ohm); Configure cal_os_ph_rd to 0x60 (default value = 0x0); Configure Cal_rxclkalign90_ext to use an external ovride value
  {0x09803228 | 0, 0x0000DAC0},
  {0x09803628 | 0, 0x0000DAC0},
  {0x09804228 | 0, 0x0000DAC0},
  {0x09804628 | 0, 0x0000DAC0},
  // Reset dtl_rx ; Enable ana_clk_det
  {0x0980336C | 0, 0x00008040},
  {0x0980376C | 0, 0x00008040},
  {0x0980436C | 0, 0x00008040},
  {0x0980476C | 0, 0x00008040},
  // Un reset dtl_rx
  {0x0980336C | 0, 0x00008000},
  {0x0980376C | 0, 0x00008000},
  {0x0980436C | 0, 0x00008000},
  {0x0980476C | 0, 0x00008000},
  // Wait 10mSec
  {0x000000F0 | 0, 0x00000000},
  {0x000000F0 | 0, 0x00000001},
  // ?
  {0x09803224 | 0, 0x00000000},
  {0x09803624 | 0, 0x00000000},
  {0x09804224 | 0, 0x00000000},
  {0x09804624 | 0, 0x00000000},
  // CAL Start
  {0x09803224 | 0, 0x00008000},
  {0x09803624 | 0, 0x00008000},
  {0x09804224 | 0, 0x00008000},
  {0x09804624 | 0, 0x00008000},
  {0x09803224 | 0, 0x00000000},
  {0x09803624 | 0, 0x00000000},
  {0x09804224 | 0, 0x00000000},
  {0x09804624 | 0, 0x00000000},
  // Wait for RxClk_x2
  // Wait 10mSec
  {0x000000F0 | 0, 0x00000000},
  {0x000000F0 | 0, 0x00000001},
  // Set RxInit to 0x1 (remember that bit 3 is already set to 0x1)
  {0x09803004 | 0, 0x00000018},
  {0x09803404 | 0, 0x00000018},
  {0x09804004 | 0, 0x00000018},
  {0x09804404 | 0, 0x00000018},
  // Wait for p_clk = 1 and p_clk = 0
  // Wait 10mSec
  {0x000000F0 | 0, 0x00000000},
  {0x000000F0 | 0, 0x00000001},
  // Set RxInit to 0x0
  {0x09803004 | 0, 0x00000008},
  {0x09803404 | 0, 0x00000008},
  {0x09804004 | 0, 0x00000008},
  {0x09804404 | 0, 0x00000008},
  // Wait for ALL PHY_RDY = 1 (0x09800008 bit 0)
  // Wait 10mSec
  {0x000000F0 | 0, 0x00000000},
  {0x000000F0 | 0, 0x00000001},
  // ?
  {0x09803004 | 0, 0x00000028},
  {0x09803404 | 0, 0x00000028},
  {0x09804004 | 0, 0x00000028},
  {0x09804404 | 0, 0x00000028},
  // Wait 10mSec
  {0x000000F0 | 0, 0x00000000},
  {0x000000F0 | 0, 0x00000001},
  // End SGMII Initial
  // port 24~27 SGMII force mode config
  {0x0a806000 | 0, 0x0000c801},
  {0x0a806400 | 0, 0x0000c801},
  {0x0a806800 | 0, 0x0000c801},
  {0x0a806C00 | 0, 0x0000c801},
  // ?
  {0x0a806008 | 0, 0x0000C009},
  {0x0a806408 | 0, 0x0000C009},
  {0x0a806808 | 0, 0x0000C009},
  {0x0a806C08 | 0, 0x0000C009},
  //  force FC enable
  {0x0a80600C | 0, 0x00009042},
  {0x0a80640C | 0, 0x00009042},
  {0x0a80680C | 0, 0x00009042},
  {0x0a806C0C | 0, 0x00009042},
  //==============88E1680 init Start===================
  // SMI_0 ========================
  // -88E1680 PHY init settings for A2
  //----- SMI0, PhyAddr=0x0~0xF
  //QSGMII power up
  {0x04004054 | 0, 0x02C0C004},
  {0x04004054 | 0, 0x02C4C004},
  {0x04004054 | 0, 0x02C8C004},
  {0x04004054 | 0, 0x02CCC004},
  {0x04004054 | 0, 0x03488000},
  // Global write
  // 0x0~0xF
  {0x04004054 | 0, 0x02C0C000},
  {0x04004054 | 0, 0x02C1C000},
  {0x04004054 | 0, 0x02C2C000},
  {0x04004054 | 0, 0x02C3C000},
  {0x04004054 | 0, 0x02C4C000},
  {0x04004054 | 0, 0x02C5C000},
  {0x04004054 | 0, 0x02C6C000},
  {0x04004054 | 0, 0x02C7C000},
  {0x04004054 | 0, 0x02C8C000},
  {0x04004054 | 0, 0x02C9C000},
  {0x04004054 | 0, 0x02CAC000},
  {0x04004054 | 0, 0x02CBC000},
  {0x04004054 | 0, 0x02CCC000},
  {0x04004054 | 0, 0x02CDC000},
  {0x04004054 | 0, 0x02CEC000},
  {0x04004054 | 0, 0x02CFC000},
  // Matrix LED fix
  {0x04004054 | 0, 0x02C0C004},
  {0x04004054 | 0, 0x03603FA0},
  // Set Page FD
  // Reg8=0b53 for QSGMII
  {0x04004054 | 0, 0x02C0C0FD},
  {0x04004054 | 0, 0x01000B53},
  //Reg7=200d
  {0x04004054 | 0, 0x00E0200D},
  // ?
  {0x04004054 | 0, 0x02C0C0FF},
  {0x04004054 | 0, 0x0220B030},
  {0x04004054 | 0, 0x0200215C},
  {0x04004054 | 0, 0x02C0C0FC},
  {0x04004054 | 0, 0x0300888C},
  {0x04004054 | 0, 0x0320888C},
  //-Advertise EEE ability
  {0x04004054 | 0, 0x02C0C000},
  {0x04004054 | 0, 0x01A00007},
  {0x04004054 | 0, 0x01C0003C},
  {0x04004054 | 0, 0x01A04007},
  {0x04004054 | 0, 0x01C00006},
  {0x04004054 | 0, 0x00009140},
  //Config Copper control register
  {0x04004054 | 0, 0x02C0c000},
  //Enable Pause(FC) advertisment.
  {0x04004054 | 0, 0x00800de1},
  {0x04004054 | 0, 0x00009140},
  //PHY LED
  // LED_0 = LINK/ACT
  // LED_[0:1] = Active high.(Reg_17(0x11)_[3:0]= 2'b0101)
  {0x04004054 | 0, 0x02C0C003},
  {0x04004054 | 0, 0x02001140},
  {0x04004054 | 0, 0x02403A00},
  // offset 0x04004054 0x02208800 ???
  // PHY Soft Reset and power up PHY
  {0x04004054 | 0, 0x02c0c000},
  {0x04004054 | 0, 0x02003360},
  {0x04004054 | 0, 0x00009140},
  {0x04004054 | 0, 0x02C00000},
  // SMI_0 ========================
  // SMI_1 ========================
  // -88E1680 PHY init settings for A2
  //----- SMI0, PhyAddr=0x10~17
  //QSGMII power up
  {0x05004054 | 0, 0x02D0C004},
  {0x05004054 | 0, 0x02D4C004},
  {0x05004054 | 0, 0x03508000},
  // Global write
  // 0x10~0x17
  {0x05004054 | 0, 0x02D0C000},
  {0x05004054 | 0, 0x02D1C000},
  {0x05004054 | 0, 0x02D2C000},
  {0x05004054 | 0, 0x02D3C000},
  {0x05004054 | 0, 0x02D4C000},
  {0x05004054 | 0, 0x02D5C000},
  {0x05004054 | 0, 0x02D6C000},
  {0x05004054 | 0, 0x02D7C000},
  // Matrix LED fix
  {0x05004054 | 0, 0x02C0C004},
  {0x05004054 | 0, 0x03603FA0},
  // Set Page FD
  // Reg8=0b53 for QSGMII
  {0x05004054 | 0, 0x02D0C0FD},
  {0x05004054 | 0, 0x01100B53},
  //Reg7=200d
  {0x05004054 | 0, 0x00F0200D},
  // ?
  {0x05004054 | 0, 0x02D0C0FF},
  {0x05004054 | 0, 0x0230B030},
  {0x05004054 | 0, 0x0210215C},
  {0x05004054 | 0, 0x02D0C0FC},
  {0x05004054 | 0, 0x0310888C},
  {0x05004054 | 0, 0x0330888C},
  //-Advertise EEE ability
  {0x05004054 | 0, 0x02D0C000},
  {0x05004054 | 0, 0x01B00007},
  {0x05004054 | 0, 0x01D0003C},
  {0x05004054 | 0, 0x01B04007},
  {0x05004054 | 0, 0x01D00006},
  {0x05004054 | 0, 0x00109140},
  //Config Copper control register
  {0x05004054 | 0, 0x02D0c000},
  //Enable Pause(FC) advertisment.
  {0x05004054 | 0, 0x00900de1},
  {0x05004054 | 0, 0x00109140},
  //PHY LED
  // LED_0 = LINK/ACT
  // LED_[0:1] = Active high.(Reg_17(0x11)_[3:0]= 2'b0101)
  {0x05004054 | 0, 0x02D0C003},
  {0x05004054 | 0, 0x02101140},
  {0x05004054 | 0, 0x02503A00},
  // offset 0x05004054 0x02308800 ???
  // PHY Soft Reset and power up PHY
  {0x05004054 | 0, 0x02D0c000},
  {0x05004054 | 0, 0x02103360},
  {0x05004054 | 0, 0x00109140},
  {0x05004054 | 0, 0x02D00000},
  // SMI_1 ========================
  //  VLAN MRU profile 0 and 1 = 9216
  {0x02000300 | 0, 0x24002400},
#if 000*000 // WG:JB Dropped packet fix from Marvell
  // We only need the first patch and we do that in cpssd
  { 0x3000000 | 0, 0x1F8FF9FF},
  { 0x1800084 | 0, 0x00000fc0},
  { 0x1800000 | 0, 0x0f828203},
  { 0x1800004 | 0, 0x33BC00C3},
  { 0x18001F0 | 0, 0x01200200},
  { 0x3000030 | 0, 0x55555555},
  { 0x3000034 | 0, 0x00555555},
  { 0x3000020 | 0, 0x01C02003},
  { 0x3000024 | 0, 0x01C02003},
  { 0x3000028 | 0, 0x01C02003},
  { 0x300002C | 0, 0x5FD7F2FE},
  { 0x1800030 | 0, 0x01890624},
  { 0x1940000 | 0, 0x00100040},
  { 0x1940008 | 0, 0x00100040},
  { 0x1940010 | 0, 0x00100040},
  { 0x1940018 | 0, 0x00100040},
  { 0x1940020 | 0, 0x00100040},
  { 0x1940028 | 0, 0x00100040},
  { 0x1940030 | 0, 0x00100040},
  { 0x1940038 | 0, 0x00100040},
  { 0x1803080 | 0, 0x0000FFFF},
  { 0x1803280 | 0, 0x0000FFFF},
  { 0x1803480 | 0, 0x0000FFFF},
  { 0x1803680 | 0, 0x0000FFFF},
#endif
  // Delay enable {0x00000058 | 0, 0x04104003},
  // 98DX3035 Initialized
  {0x000000F0 | 0, 0x12345678},
  // Done indication
  {0x00000000 | 0, 0x00000000}
};

// MDIO bus for the PSS device
extern	struct mii_bus* wg_pss_bus;

// PSS hardware function pointers
static	int (*pss_reset)(void*)                = NULL;
static	int (*pss_read_mdic)(void*, u32, u32*) = NULL;
static	int (*pss_write_mdic)(void*, u32, u32) = NULL;

#define	DX_DATA		0xFFFF

#define	DX_PHY_SHIFT	16
#define	DX_PHY_MASK	(31 <<  DX_PHY_SHIFT)
#define	DX_REG_SHIFT	21
#define	DX_REG_MASK	(31 <<  DX_REG_SHIFT)

#define	DX_ERROR	((INT_MIN))

#define	DX_BUSY		( 1 <<  28)
#define	DX_VALID	( 1 <<  27)
#define	DX_RD_OP	( 1 <<  26)

#define	PHY_OFFSET(p)	((p >= 16) ? 0x5004054 : 0x4004054)

static	int pss_read(void* bus, u32 offset, u32* data)
{
  int err;

  spin_lock(&pss_mdic_lock);
  err = pss_read_mdic  ? pss_read_mdic(bus, offset, data)  : -ENOSYS;
  spin_unlock(&pss_mdic_lock);

  return err;
}

static	int pss_write(void* bus, u32 offset, u32 data)
{
  int err;

  spin_lock(&pss_mdic_lock);
  err = pss_write_mdic ? pss_write_mdic(bus, offset, data) : -ENOSYS;
  spin_unlock(&pss_mdic_lock);

  return err;
}

static	int pss_phy_read(struct mii_bus* bus, int phy, int reg)
{
  int try;
  int err;
  u32 data;
  u32 offset = PHY_OFFSET(phy);

  for (try = 5; try > 0; --try) {
    data = (phy << DX_PHY_SHIFT) | (reg << DX_REG_SHIFT) | DX_RD_OP;

    spin_lock(&pss_mdic_lock);

    err = pss_write_mdic(bus->priv, offset, data);

    if (err) {
      spin_unlock(&pss_mdic_lock);
      printk(KERN_DEBUG "%s Phy %d Reg %d Write Error = %d\n",
             __FUNCTION__, phy, reg, err);
      data |= DX_ERROR;
      break;
    }

    err = pss_read_mdic(bus->priv, offset, &data);

    spin_unlock(&pss_mdic_lock);

    if (err) {
      printk(KERN_DEBUG "%s Phy %d Reg %d Read  Error = %d\n",
             __FUNCTION__, phy, reg, err);
      data |= DX_ERROR;
      break;
    }

    if (data & DX_VALID) break;
  }

  return data & (DX_DATA | DX_BUSY | DX_ERROR);
}

static	int pss_phy_write(struct mii_bus* bus, int phy, int reg, u16 val)
{
  int err;
  u32 offset = PHY_OFFSET(phy);
  u32 data   = val;

  data |= (phy << DX_PHY_SHIFT) | (reg << DX_REG_SHIFT);

  err = pss_write(bus->priv, offset, data);

  if (err) {
    printk(KERN_DEBUG "%s Phy %d Reg %d Write Error = %d\n",
           __FUNCTION__, phy, reg, err);
    data |= DX_ERROR;
  }

  return data & (DX_DATA | DX_BUSY | DX_ERROR);
}

static	int pss_chip_setup(int ix, int skip)
{
  int  j, k;

  if (skip) skip = S;

  for (k = 0, j = ix; pss_reg[j].off; j++, k++)
  if ((pss_reg[j].off & skip) == 0)
     pss_write(wg_pss_bus->priv, pss_reg[j].off & ~skip, pss_reg[j].val);

  return k;
}

// Get config info
static	int proc_read_config(char *page, char **start, off_t off,
                             int count, int *eof, void *data)
{
  return sprintf(page, "%3d %3d %4X %4X %3d\n",
                 NPHY, NSGMII, ETH_P_PSS, PSS_PVID_BASE, pss_ifx);
}

// Get data rates
static	int proc_read_rates(char *page, char **start, off_t off,
                            int count, int *eof, void *data)
{
  int      j, z = 0;
  long     secs;
  struct   rates_S phy[NPHY];
  struct   rates_S cpu[NSGMII];
  struct   net_device*   dev;
  static   unsigned long last;

  if ((secs = (jiffies - last) / HZ) <= 0) return 0;

  last = jiffies;

  memset(phy, 0, sizeof(phy));
  memset(cpu, 0, sizeof(cpu));

  for (j = 0; j < NPHY; j++)
  if ((dev = pss_device[j])) {
    int q = pss_portsw[j];
    phy[j].rx_packets  = dev->stats.rx_packets;
    phy[j].tx_packets  = dev->stats.tx_packets;
    phy[j].rx_bytes    = dev->stats.rx_bytes;
    phy[j].tx_bytes    = dev->stats.tx_bytes;
    cpu[q].rx_packets += dev->stats.rx_packets;
    cpu[q].tx_packets += dev->stats.tx_packets;
    cpu[q].rx_bytes   += dev->stats.rx_bytes;
    cpu[q].tx_bytes   += dev->stats.tx_bytes;
  }

  for (j = 0; j < NPHY; j++) {
    long rx_packets = (phy[j].rx_packets - pss_phy_rates[j].rx_packets) / secs;
    long rx_bytes   = (phy[j].rx_bytes   - pss_phy_rates[j].rx_bytes)   / secs;
    long tx_packets = (phy[j].tx_packets - pss_phy_rates[j].tx_packets) / secs;
    long tx_bytes   = (phy[j].tx_bytes   - pss_phy_rates[j].tx_bytes)   / secs;

    if (rx_packets <= 0) if (tx_packets <= 0) continue;

    z += sprintf(&page[z], "eth%-2d ", j + 1);
    z += sprintf(&page[z],
                 "RX %6ld pkts/s %7ld Kbits/s TX %6ld pkts/s %7ld Kbits/s\n",
                 rx_packets, rx_bytes / 125, tx_packets, tx_bytes / 125);
  }

  for (j = 0; j < NSGMII; j++) {
    long rx_packets = (cpu[j].rx_packets - pss_cpu_rates[j].rx_packets) / secs;
    long rx_bytes   = (cpu[j].rx_bytes   - pss_cpu_rates[j].rx_bytes)   / secs;
    long tx_packets = (cpu[j].tx_packets - pss_cpu_rates[j].tx_packets) / secs;
    long tx_bytes   = (cpu[j].tx_bytes   - pss_cpu_rates[j].tx_bytes)   / secs;

    if (rx_packets <= 0) if (tx_packets <= 0) continue;

    z += sprintf(&page[z], "sw%-2d  ", j);
    z += sprintf(&page[z],
                 "RX %6ld pkts/s %7ld Kbits/s TX %6ld pkts/s %7ld Kbits/s\n",
                 rx_packets, rx_bytes / 125, tx_packets, tx_bytes / 125);
  }

  memcpy(pss_phy_rates, phy, sizeof(pss_phy_rates));
  memcpy(pss_cpu_rates, cpu, sizeof(pss_cpu_rates));

  return z;
}

// Get brdige change events
static	int proc_read_event(char *page, char **start, off_t off,
                            int count, int *eof, void *data)
{
  if (pss_debug & 1)
  printk(KERN_DEBUG "%s: phys %s cpus %s\n",
         __FUNCTION__, pss_phys, pss_cpus);

  if (pss_task == current) pss_updated = pss_updates;

  return sprintf(page, "%s\n%s\n", pss_phys, pss_cpus);
}

// Wait for a bridge change event
static	int proc_write_event(struct file *file, const char *buffer,
                             unsigned long count, void *data)
{
  long timeout;
  char str[256], *strend;
  int  len = (count > (sizeof(str)-1)) ? sizeof(str)-1 : count;

  // Get string from user and null terminate it
  if (copy_from_user(str, buffer, len))
    return -EFAULT;
  str[len] = 0;

  timeout = simple_strtol(str, &strend, 10);
  if (HZ) timeout = (timeout * HZ) / 50;
  if (timeout <= 0) timeout = MAX_SCHEDULE_TIMEOUT;

  if (pss_task != current) pss_updated = 0;

  pss_task = current;

  if (pss_updated != pss_updates) {
    if (pss_debug & 2)
    printk(KERN_DEBUG "%s: updated %ld updates %ld\n",
           __FUNCTION__, pss_updated, pss_updates);
    return 0;
  }

  if (pss_debug & 2)
    printk(KERN_DEBUG "%s: timeout %d\n", __FUNCTION__, (int)timeout);

  set_current_state(TASK_INTERRUPTIBLE);
  schedule_timeout_interruptible(timeout);

  return count;
}

// Get debug flag
static	int proc_read_debug(char *page, char **start, off_t off,
                            int count, int *eof, void *data)
{
  return sprintf(page, "%x\n", pss_debug);
}

// Set debug flag
static	int proc_write_debug(struct file *file, const char *buffer,
                             unsigned long count, void *data)
{
  char str[256], *strend;
  int  len = (count > (sizeof(str)-1)) ? sizeof(str)-1 : count;

  // Get string from user and null terminate it
  if (copy_from_user(str, buffer, len))
    return -EFAULT;
  str[len] = 0;

  pss_debug = simple_strtoul(str, &strend, 16);

  return count;
}

// Get udelay
static	int proc_read_udelay(char *page, char **start, off_t off,
                             int count, int *eof, void *data)
{
  return sprintf(page, "%d\n", pss_udelay);
}

// Set udelay
static	int proc_write_udelay(struct file *file, const char *buffer,
                              unsigned long count, void *data)
{
  char str[256], *strend;
  int  len = (count > (sizeof(str)-1)) ? sizeof(str)-1 : count;

  // Get string from user and null terminate it
  if (copy_from_user(str, buffer, len))
    return -EFAULT;
  str[len] = 0;

  pss_udelay = simple_strtoul(str, &strend, 10);

  return count;
}

// Get queue list
static	int proc_read_queue(char *page, char **start, off_t off,
                            int count, int *eof, void *data)
{
  int j;

  for (page[NPHY+1] = j = 0; j < NPHY; j++)
  page[j] = (pss_queue[j] >= 0)  ? '0' + pss_queue[j] : '-';

  page[NPHY] = '\n';

  return NPHY+1;
}

// Set queue list
static	int proc_write_queue(struct file *file, const char *buffer,
                             unsigned long count, void *data)
{
  int  j;
  char str[NPHY];
  int  len = (count > NPHY) ? NPHY : count;

  // Get string from user and null terminate it
  if (copy_from_user(str, buffer, len))
    return -EFAULT;

  for (j = 0; j < len; j++) {
    unsigned  q = str[j] - '0';
    pss_queue[j] = (q < num_online_cpus()) ? q : -ENOKEY;
  }

  return count;
}

// Get debug register
static	int proc_read_register(char *page, char **start, off_t off,
                               int count, int *eof, void *data)
{
  u32 value = -1;

  pss_read(wg_pss_bus->priv, pss_show_reg, &value);

  return sprintf(page, "%08x: %08x\n", pss_show_reg, value);
}

// Set debug register
static	int proc_write_register(struct file *file, const char *buffer,
                                unsigned long count, void *data)
{
  char str[256], *strend;
  int  len = (count > (sizeof(str)-1)) ? sizeof(str)-1 : count;

  // Get string from user and null terminate it
  if (copy_from_user(str, buffer, len))
    return -EFAULT;
  str[len] = 0;

  pss_show_reg = simple_strtoul(str, &strend, 16);

  return count;
}

// Get global enable/disable bit
static	int proc_read_global(char *page, char **start, off_t off,
                             int count, int *eof, void *data)
{
  u32 value = -1;

  pss_read(wg_pss_bus->priv, 0x58, &value);

  return sprintf(page, "%x\n", value);
}

// Set global enable/disable bit
static	int proc_write_global(struct file *file, const char *buffer,
                              unsigned long count, void *data)
{
  char str[3];
  u32  old, new;
  int  len = (count > (sizeof(str)-1)) ? sizeof(str)-1 : count;

  // Get string from user and null terminate it
  if (copy_from_user(str, buffer, len))
    return -EFAULT;
  str[len] = 0;

  if (str[0] == '0')
    new = 0x04104002;
  else
  if (str[0] == '1')
    new = 0x04104003;
  else
    return -EINVAL;

  old = 0xEEEEEEEE;

  if (str[1] != '^')
    pss_read( wg_pss_bus->priv, 0x58, &old);
  if ((old & 0xFFF9FFFF) != new) {
    pss_write(wg_pss_bus->priv, 0x58,  new);
    mdelay(10);
    printk(KERN_DEBUG "%s: PSS device %sabled, %x -> %x\n",
           __FUNCTION__, (new & 1) ? "en" : "dis", old, new);
  }

  return count;
}

// Patch a PSS register
static	int proc_write_patch(struct file *file, const char *buffer,
                             unsigned long count, void *data)
{
  char str[256], *strend;
  int  len = (count > (sizeof(str)-1)) ? sizeof(str)-1 : count;
  u32  val;

  // Get string from user and null terminate it
  if (copy_from_user(str, buffer, len))
    return -EFAULT;
  str[len] = 0;

  if (!pss_show_reg) return -ENXIO;

  val = simple_strtoul(str, &strend, 16);
  pss_write(wg_pss_bus->priv, pss_show_reg, val);

  return count;
}

// Registers reset so far
static	int wg_ix = 0;

// Reset PSS chip
static	int proc_write_reset(struct file *file, const char *buffer,
                             unsigned long count, void *data)
{
  int    k;
  char   str[2];
  int    len = (count > (sizeof(str)-1)) ? sizeof(str)-1 : count;

  // Get string from user and null terminate it
  if (copy_from_user(str, buffer, len))
    return -EFAULT;
  str[len] = 0;

  switch (str[0]) {
  case '1':
  case 'S':
    k = pss_chip_setup(wg_ix, wg_ix);
    break;
  case '^':
    k = pss_chip_setup(0,     wg_ix);
    break;;
  case 'X':
    if (!pss_reset) return -ENOSYS;
    k = pss_reset(wg_pss_bus->priv);
    break;
  default:
    return -EINVAL;
    break;
  }

  if (k > 0) {
    if (wg_ix != k)
        wg_ix =  k;
    else {
      printk(KERN_INFO  "%s: PSS device updated %d registers\n",
             __FUNCTION__, k);
      return -EDEADLK;
    }

    printk(KERN_INFO    "%s: PSS device reset %d registers\n",
           __FUNCTION__, k);
    return wg_ix;
  }

  if (k < 0) {
    printk(KERN_EMERG   "%s: PSS device reset error %d\n",
           __FUNCTION__, k);
    return k;
  }

  printk(KERN_DEBUG     "%s: PSS device already reset\n",
         __FUNCTION__);
  return -EEXIST;
}

// Get phymap info
static	int proc_read_phymap(char *page, char **start, off_t off,
                             int count, int *eof, void *data)
{
  int j, z = 0;
  struct phy_device* phydev;

  // Display the PHY table
  for (j = 0; j < NPHY; j++)
  if ((phydev = wg_pss_bus->phy_map[j]))
    z += sprintf(&page[z], "PHY %2d ID %8X ETH %s\n",
                 phydev->addr, phydev->phy_id,
                 phydev->attached_dev ? phydev->attached_dev->name : "");

  return z;
}

// Probe PSS chip for PHYs
static	int proc_write_phymap(struct file *file, const char *buffer,
                              unsigned long count, void *data)
{
  int    j;
  int    err;
  struct phy_device* phydev;

  // Return if switch not set up yet or we'll get bogus results
  if (wg_ix <= 0)
    return -ENODEV;

  // Return if already registered
  if (wg_pss_bus->state == MDIOBUS_REGISTERED)
    return -EEXIST;

  // Set up the name, read/write functions, parent, and id
  wg_pss_bus->name   = "Marvell 98DX3035";
  wg_pss_bus->read   = pss_phy_read;
  wg_pss_bus->write  = pss_phy_write;
  wg_pss_bus->parent = NULL;

  strncpy(wg_pss_bus->id, "98DX3035", MII_BUS_ID_SIZE);

  // Register the PHY devices on MDIO bus
  err = mdiobus_register(wg_pss_bus);
  if (err < 0)  {
    printk(KERN_EMERG "%s: mdiobus_register error %d\n", __FUNCTION__, err);
    return err;
  }

  // Attach ETH devices to PHY
  for (j = 0; j < NPHY; j++)
  if ((phydev = wg_pss_bus->phy_map[j]))
       phydev->attached_dev = pss_device[j];

  return count;
}

// Rename an eth device
static	inline void rename_eth(struct net_device* dev, char* name)
{
  int err;

  printk(KERN_INFO "pss_create:  %s    ->   %s\n", dev->name, name);

  rtnl_lock();    // Lock
  dev_close(dev); // Set it down
  err = dev_change_name(dev, name); // Change name
  if (err)
    printk(KERN_EMERG "%s: %s error %d\n", __FUNCTION__, dev->name, err);
  rtnl_unlock();  // Unlock
}

// Ethtool operations

static	int pss_get_settings(struct net_device* dev,
                             struct ethtool_cmd* cmd)
{
  int err = -EOPNOTSUPP;
  struct phy_device* phy;
  unsigned port = I2P(dev->ifindex);

  if (likely(port < NPHY))
  if ((phy = wg_pss_bus->phy_map[port])) {
    err = phy_read_status(phy);
    if (err == 0)
      err = phy_ethtool_gset(phy, cmd);
  }

  return err;
}

// ANCR - Auto-Nego Config Register
#define ANCR_BASE				0x0A80000C
#define ANCR_AN_DUPLEX_EN		1 << 13
#define ANCR_SET_FULL_DUPLEX	1 << 12
#define ANCR_FC_EN				1 << 11
#define ANCR_SET_FC_EN			1 << 8
#define ANCR_AN_SPEED_EN		1 << 7
#define ANCR_SET_GMII_SPEED		1 << 6
#define ANCR_SET_MII_SPEED		1 << 5

/* wg_pss_set_mac() - adjust the settings for the MAC according to ethtool forced cmd
 * 1) no need for sanity checking for new settings. already done in phy_ethtool_sset()
 * 2) leave ANCR_SET_FC_EN unchanged in forced mode since no flow control settings
 *    in ethtool_cmd 
 */
static int wg_pss_set_mac(int port, struct ethtool_cmd* cmd)
{
  int err;
  u32 val;
  u32 speed;
  unsigned int addr;

  addr = ANCR_BASE + (0x400 * port);
  err = pss_read(wg_pss_bus->priv, addr, &val);
  if (err) {
	printk(KERN_WARNING "@@ %s, line: %d, pss_read failed\n", __FUNCTION__, __LINE__);
	return -EOPNOTSUPP;
  }

  if (cmd->autoneg != AUTONEG_ENABLE) {
	// disable auto-nego on duplex mode, flow control and speed
	val &= ~(ANCR_AN_DUPLEX_EN | ANCR_FC_EN | ANCR_AN_SPEED_EN);
	val &= ~(ANCR_SET_FULL_DUPLEX | ANCR_SET_GMII_SPEED | ANCR_SET_GMII_SPEED);
	speed = ethtool_cmd_speed(cmd);
	if (SPEED_1000 == speed)
	  val |= ANCR_SET_GMII_SPEED;
	else if (SPEED_100 == speed)
	  val |= ANCR_SET_MII_SPEED;
 
	if (DUPLEX_FULL == cmd->duplex)
	  val |= ANCR_SET_FULL_DUPLEX;
  }
  else {
	// Simply enable all auto-negos on MAC, rely on the update from PHY
	val |= (ANCR_AN_DUPLEX_EN | ANCR_FC_EN | ANCR_AN_SPEED_EN);
  }

  pss_write(wg_pss_bus->priv, addr, val);

  return 0;
}

static	int pss_set_settings(struct net_device* dev,
                             struct ethtool_cmd* cmd)
{
  struct phy_device* phy;
  unsigned port = I2P(dev->ifindex);

  if (likely(port < NPHY))
	if ((phy = wg_pss_bus->phy_map[port])) {
	  wg_pss_set_mac(port, cmd);	// for BUG87012
	  return phy_ethtool_sset(phy, cmd);
	}
  return -EOPNOTSUPP;
}

static	void pss_get_drvinfo(struct net_device* dev,
                             struct ethtool_drvinfo* drvinfo)
{
  strncpy(drvinfo->driver,     "pss", 32);
  strncpy(drvinfo->version,    "0.1", 32);
  strncpy(drvinfo->fw_version, "N/A", 32);
  strncpy(drvinfo->bus_info,   "N/A", 32);
}

static	int pss_nway_reset(struct net_device* dev)
{
  struct phy_device* phy;
  unsigned port = I2P(dev->ifindex);

  if (likely(port < NPHY))
  if ((phy = wg_pss_bus->phy_map[port]))
    return genphy_restart_aneg(phy);

  return -EOPNOTSUPP;
}

static	u32 pss_get_link(struct net_device* dev)
{
  struct phy_device* phy;
  unsigned port = I2P(dev->ifindex);

  if (unlikely(port >= NPHY))
    return -EOPNOTSUPP;

  if ((phy = wg_pss_bus->phy_map[port])) {
    genphy_update_link(phy);
    return phy->link;
  }

  return 0;
}

#define	ETH_SS_COUNTERS	34
#define	ITEM(s)		strncpy(data,s,len-1); data+=len


static	void pss_get_strings(struct net_device* dev,
                             uint32_t stringset, uint8_t* data)
{
  int len = ETH_GSTRING_LEN;

  if (stringset == ETH_SS_STATS) {
    ITEM("tx_packets");
    ITEM("tx_bytes");
    ITEM("rx_packets");
    ITEM("rx_bytes");
    ITEM("rx_good_octets");
    ITEM("rx_bad_octets");
    ITEM("tx_underrun");
    ITEM("rx_unicast");
    ITEM("tx_deferred");
    ITEM("rx_broadcast");
    ITEM("rx_multicast");
    ITEM("frames_min-64");
    ITEM("frames_65-127");
    ITEM("frames_128-255");
    ITEM("frames_256-511");
    ITEM("frames_512-1023");
    ITEM("frames_1024-max");
    ITEM("tx_good_octets");
    ITEM("tx_unicast");
    ITEM("tx_collisions");
    ITEM("tx_multicast");
    ITEM("tx_broadcast");
    ITEM("tx_multiple");
    ITEM("tx_pause");
    ITEM("rx_pause");
    ITEM("rx_overrun");
    ITEM("rx_undersize");
    ITEM("rx_fragments");
    ITEM("rx_oversize");
    ITEM("rx_jabber");
    ITEM("rx_error_frames");
    ITEM("badCRC");
    ITEM("collisons");
    ITEM("late_collisions");
  }
}

static	int pss_get_sset_count(struct net_device* dev, int sset)
{
  if (sset == ETH_SS_STATS) return ETH_SS_COUNTERS;

  return -EOPNOTSUPP;
}

static	void pss_get_ethtool_stats(struct net_device* dev,
                                   struct ethtool_stats* stats,
                                   uint64_t* data)
{
  int j, k;
  u32 base;
  u32 raw[32];
  u32 port = I2P(dev->ifindex);

  k = 0;

  data[k++] = dev->stats.tx_packets;
  data[k++] = dev->stats.tx_bytes;
  data[k++] = dev->stats.rx_packets;
  data[k++] = dev->stats.rx_bytes;

  base =  0x4010000;
  base += 0x0800000 * (port / 6);
  base += 0x0000080 * (port % 6);

  for (j = 0; j < 32; j++, base += 4) {
    int err = pss_read(wg_pss_bus->priv, base, &raw[j]);
    if (err) {
      printk(KERN_EMERG "%s Read  Error = %d\n", __FUNCTION__, err);
      return;
    }
  }

  for (j = 0; k < ETH_SS_COUNTERS; k++) {
    data[k]  = raw[j++];
    if ((j == 1) || (j == 15))
    data[k] |= ((u64)raw[j++]) << 32;
  }
}

static	void pss_get_ringparam(struct net_device* dev,
                               struct ethtool_ringparam* ring)
{
  ring->rx_max_pending	     = 0;
  ring->tx_max_pending	     = 0;
  ring->rx_mini_max_pending  = 0;
  ring->rx_jumbo_max_pending = 0;
  ring->rx_pending	     = 0;
  ring->tx_pending	     = 0;
  ring->rx_mini_pending	     = 0;
}

static	int  pss_set_ringparam(struct net_device* dev,
                               struct ethtool_ringparam* ring)
{
  return 0;
}

static	void pss_get_pauseparam(struct net_device* dev,
                                struct ethtool_pauseparam* pause)
{
  int err;
  u32 data;
  u32 port = I2P(dev->ifindex);

  if (unlikely(port >= NPHY))
    return;

  err = pss_read(wg_pss_bus->priv, REG(port, 0x0C), &data);
  if (err) {
    printk(KERN_EMERG "%s Read  Error = %d\n", __FUNCTION__, err);
    return;
  }

  pause->autoneg  = (data >> 11) & 1;

  err = pss_read(wg_pss_bus->priv, REG(port, 0x10), &data);
  if (err) {
    printk(KERN_EMERG "%s Read  Error = %d\n", __FUNCTION__, err);
    return;
  }

  pause->rx_pause = (data >>  4) & 1;
  pause->tx_pause = (data >>  5) & 1;
}

static	int  pss_set_pauseparam(struct net_device* dev,
                               struct ethtool_pauseparam* pause)
{
  int err;
  u32 old;
  u32 new;
  u32 reg;
  u32 port = I2P(dev->ifindex);

  if (unlikely(port >= NPHY))
    return -ENODEV;

  reg = REG(port, 0x0C);
  err = pss_read(wg_pss_bus->priv, reg, &old);
  if (err) {
    printk(KERN_EMERG "%s Read  Error = %d\n", __FUNCTION__, err);
    return err;
  }

  new = old & ~0xB00;
  if (pause->autoneg)
    new |= 0x800;
  else
  if (pause->rx_pause)
    new |= 0x100;
  else
  if (pause->tx_pause)
    new |= 0x100;
  if ((new & 0x900) == 0x900)
    new |= 0x200;
  if (new == old)
    return 0;

  if (pss_debug & 8)
  printk(KERN_DEBUG "%s: ANEG %04X -> %04X\n", __FUNCTION__, old, new);

  err = pss_write(wg_pss_bus->priv, reg, (old & ~3) | 1);
  if (err) {
    printk(KERN_EMERG "%s Write Error = %d\n", __FUNCTION__, err);
    return err;
  }

  err = pss_write(wg_pss_bus->priv, reg, (new & ~3) | 1);
  if (err) {
    printk(KERN_EMERG "%s Write Error = %d\n", __FUNCTION__, err);
    return err;
  }

  err = pss_write(wg_pss_bus->priv, reg, new);
  if (err) {
    printk(KERN_EMERG "%s Write Error = %d\n", __FUNCTION__, err);
    return err;
  }

  {
    struct phy_device* phy = wg_pss_bus->phy_map[port];
    if (phy) genphy_restart_aneg(phy);
  }

  return 0;
}

static	const struct ethtool_ops pss_ethtool_ops = {
  .get_settings		= pss_get_settings,
  .set_settings		= pss_set_settings,
  .get_drvinfo		= pss_get_drvinfo,
  .nway_reset		= pss_nway_reset,
  .get_link		= pss_get_link,
  .get_ringparam	= pss_get_ringparam,
  .set_ringparam	= pss_set_ringparam,
  .get_pauseparam	= pss_get_pauseparam,
  .set_pauseparam	= pss_set_pauseparam,
  .get_ethtool_stats	= pss_get_ethtool_stats,
  .get_sset_count	= pss_get_sset_count,
  .get_strings		= pss_get_strings,
};

#define	PSS_MRU_MASK	(0x7FFC) // Bits 14:2

static  int  pss_update_mru(int port, int mru)
{
  int err;
  u32 reg = REG(port, 0);
  u32 old = ~0;
  u32 new;

  // Add L2 overhead, round up to be even, shift, and mask
  mru += (ETH_HLEN + ETH_FCS_LEN + VLAN_HLEN + PSS_HLEN + 1);
  mru  = (mru << 1) & PSS_MRU_MASK; 

  // Get old MRU
  err = pss_read(wg_pss_bus->priv, reg, &old);
  if ((err != 0) || (old == ~0)) return -EIO;

  // Is there a change?
  if ((old & PSS_MRU_MASK) == mru) return 0;

  // Insert new value
  new = (old & ~PSS_MRU_MASK) | mru;

  // Write new MRU to switch chip
  err = pss_write(wg_pss_bus->priv, reg, new);
  if (err != 0) {
    printk(KERN_ERR   "%s: port%-2d MRU %4d %x: %x -> %x Error = %d\n",
           __FUNCTION__, port, mru / 2, reg, old, new, err);
    return err;
  }

  if (pss_debug & 16)
    printk(KERN_DEBUG "%s: port%-2d MRU %4d %x: %x -> %x\n",
           __FUNCTION__, port, mru / 2, reg, old, new);

  return 0;
}

static	void pss_switch_mru(void)
{
  int p;
  int s;
  int mtu;
  int sw_mtu[NSGMII] = {0};
  struct net_device *dev;

  // Set up eth devices as needed
  for (p = 0; p < NPHY; p++)
  if ((dev = pss_device[p])) {

    // Switch chip ports always handle at least 1500 bytes
    mtu = (dev->mtu <= ETH_DATA_LEN) ? ETH_DATA_LEN : dev->mtu;

    // Look for largest MTU on the eth devices
    if (mtu > sw_mtu[s = P2S(p)]) sw_mtu[s] = mtu;

    // Update PHY   MRU
    pss_update_mru(p, mtu);
  }

  // Set up switch devices as needed
  for (s = 0; s < NSGMII; s++)
  if ((dev = pss_switch[s]) && (sw_mtu[s] >= ETH_DATA_LEN)) {

    // Get largest MTU and add in PSS overhead
    mtu = sw_mtu[s] + PSS_HLEN;

    if (dev->mtu != mtu) {
      const struct net_device_ops *ops = dev->netdev_ops;

      if (ops->ndo_change_mtu)
        ops->ndo_change_mtu(dev, mtu);
      else
        dev->mtu = mtu;

      printk(KERN_INFO "%s: sw%-4d MTU %4d\n", __FUNCTION__, s, mtu);
    }

    // Update SGMII MRU
    pss_update_mru(NPHY + s, mtu);
  }
}

// Link polling

struct	work_struct	pss_link_poll_work;
struct	timer_list	pss_link_poll_timer;

static	void pss_link_poll_worker(struct work_struct* _unused)
{
  int j;

  for (j = 0; j < NPHY; j++) {
    int    link;
    struct net_device *dev = pss_device[j];

    if (dev == NULL) continue;

    if (dev->flags & IFF_UP) {
      if ((link = pss_get_link(dev)) < 0) continue;

      if (!link) {
        if (netif_carrier_ok(dev)) {
          printk(KERN_INFO "%s: link down\n", dev->name);
          netif_carrier_off(dev);
        }
      }
      else
      if (!netif_carrier_ok(dev)) {
        printk(KERN_INFO "%s: link up\n", dev->name);
        netif_carrier_on(dev);
      }
    }
  }

  // Update PHY and SGMII MRUs
  pss_switch_mru();

  mod_timer(&pss_link_poll_timer, round_jiffies(jiffies + HZ));
}

static	void pss_link_poll_schedule(unsigned long _unused)
{
  schedule_work(&pss_link_poll_work);
}

// Slave device handling

static	int pss_init(struct net_device* dev)
{
  return 0;
}

static	int pss_open(struct net_device* dev)
{
  int    err;
  struct net_device* swdev = pss_switch[I2S(dev->ifindex)];
  struct phy_device* phy;
  unsigned port = I2P(dev->ifindex);

  if (likely(port < NPHY))
  if ((phy = wg_pss_bus->phy_map[port]))
    genphy_resume(phy);

  if (!(swdev->flags & IFF_UP))
    return -ENETDOWN;

  if (compare_ether_addr(dev->dev_addr, swdev->dev_addr)) {
    err = dev_uc_add(swdev, dev->dev_addr);
    if (err < 0)
      goto out;
  }

  if (dev->flags & IFF_ALLMULTI) {
    err = dev_set_allmulti(swdev, 1);
    if (err < 0)
      goto del_unicast;
  }
  if (dev->flags & IFF_PROMISC) {
    err = dev_set_promiscuity(swdev, 1);
    if (err < 0)
      goto clear_allmulti;
  }

  return 0;

clear_allmulti:
  if (dev->flags & IFF_ALLMULTI)
    dev_set_allmulti(swdev, -1);
del_unicast:
  if (compare_ether_addr(dev->dev_addr, swdev->dev_addr))
    dev_uc_del(swdev, dev->dev_addr);
out:
  return err;
}

static	int pss_stop(struct net_device* dev)
{
  struct net_device* swdev = pss_switch[I2S(dev->ifindex)];
  struct phy_device* phy;
  unsigned port = I2P(dev->ifindex);

  if (likely(port < NPHY))
  if ((phy = wg_pss_bus->phy_map[port]))
    genphy_suspend(phy);

  dev_mc_unsync(swdev, dev);
  dev_uc_unsync(swdev, dev);
  if (dev->flags & IFF_ALLMULTI)
    dev_set_allmulti(swdev, -1);
  if (dev->flags & IFF_PROMISC)
    dev_set_promiscuity(swdev, -1);

  if (compare_ether_addr(dev->dev_addr, swdev->dev_addr))
    dev_uc_del(swdev, dev->dev_addr);

  return 0;
}

#define	IP_BIG_OFFSET	htons(IP_MF | ((IP_OFFSET + 1) / 2))

static	netdev_tx_t pss_xmit(struct sk_buff* skb, struct net_device* dev)
{
  unsigned port = I2P(dev->ifindex);

  // Check for valid port
  if (unlikely(port >= NPHY))
    goto out_drop;

  // Check if interface is an inactive member of a bridge
  if (unlikely(pss_header[port].p_vid & cpu_to_be16(PSS_PVID_DROP)))
    goto out_drop;

  // Check if we have room for PSS tag
  if (unlikely(skb_cow_head(skb, PSS_HLEN) < 0))
    goto out_drop;

  // Set switch device
  skb->dev = pss_switch[P2S(port)];

  // Update counters
  dev->stats.tx_packets++;
  dev->stats.tx_bytes += skb->len;

  // Make room for PSS tag
  skb_push(skb, PSS_HLEN);
  memmove(skb->data, skb->data + PSS_HLEN, 2 * ETH_ALEN);

  // Copy in PSS tag
  memmove(skb->data + 2 * ETH_ALEN, &pss_header[port], PSS_HLEN);

  // Check for large fragmented packets
  if (unlikely(((ip_hdr(skb)->frag_off) & IP_BIG_OFFSET) == IP_BIG_OFFSET))
  if (likely(skb->protocol == cpu_to_be16(ETH_P_IP)))
  if (unlikely(pss_udelay > 0))
    udelay(pss_udelay);

  // Set protocol type
  skb->protocol = cpu_to_be16(ETH_P_PSS);

  // Send it
  dev_queue_xmit(skb);

  return NETDEV_TX_OK;

out_drop:
  kfree_skb(skb);
  return NETDEV_TX_OK;
}

static	int  pss_change_mtu(struct net_device *dev, int mtu)
{
  // Range check MTU
  if ((mtu < 68) || (mtu > 9000))
    return -EINVAL;

  // Check for changes
  if (dev->mtu == mtu)
    return -EEXIST;

  dev->mtu = mtu;

  return 0;
}

static	void pss_change_rx_flags(struct net_device *dev, int change)
{
  struct net_device* swdev = pss_switch[I2S(dev->ifindex)];

  if (change & IFF_ALLMULTI)
    dev_set_allmulti(swdev,    dev->flags & IFF_ALLMULTI ? 1 : -1);
  if (change & IFF_PROMISC)
    dev_set_promiscuity(swdev, dev->flags & IFF_PROMISC  ? 1 : -1);
}

static	void pss_set_rx_mode(struct net_device* dev)
{
  struct net_device* swdev = pss_switch[I2S(dev->ifindex)];

  dev_mc_sync(swdev, dev);
  dev_uc_sync(swdev, dev);
}

static	int pss_set_mac_address(struct net_device* dev, void* a)
{
  int    err;
  struct net_device* swdev = pss_switch[I2S(dev->ifindex)];
  struct sockaddr  * addr   = a;

  if (!is_valid_ether_addr(addr->sa_data))
    return -EADDRNOTAVAIL;

  if (!(dev->flags & IFF_UP))
    goto out;

  if (compare_ether_addr(addr->sa_data, swdev->dev_addr)) {
    err = dev_uc_add(swdev, addr->sa_data);
    if (err < 0)
      return err;
  }

  if (compare_ether_addr(dev->dev_addr, swdev->dev_addr))
    dev_uc_del(swdev, dev->dev_addr);

out:
  memcpy(dev->dev_addr, addr->sa_data, ETH_ALEN);

  return 0;
}

static	int pss_ioctl(struct net_device* dev, struct ifreq* ifr, int cmd)
{
  struct phy_device* phy;
  unsigned port = I2P(dev->ifindex);

  if (likely(port < NPHY))
  if ((phy = wg_pss_bus->phy_map[port]))
    return phy_mii_ioctl(phy, ifr, cmd);

  return -EOPNOTSUPP;
}

static	const struct net_device_ops pss_netdev_ops = {
  .ndo_init		  = pss_init,
  .ndo_open	 	  = pss_open,
  .ndo_stop		  = pss_stop,
  .ndo_start_xmit	  = pss_xmit,
  .ndo_change_mtu	  = pss_change_mtu,
  .ndo_change_rx_flags	  = pss_change_rx_flags,
  .ndo_set_rx_mode	  = pss_set_rx_mode,
  .ndo_set_mac_address	  = pss_set_mac_address,
  .ndo_do_ioctl		  = pss_ioctl,
};

#define	TAG	(2 * ETH_ALEN)

static	int pss_untag(struct sk_buff* skb, __u8* eth)
{
  unsigned port;

  // Check for PSS tag in packet
  if (unlikely(eth[TAG+0] != PSS_TAG_HI))
    return -EPROTOTYPE;
  if (unlikely(eth[TAG+1] != PSS_TAG_LO))
    return -EPROTOTYPE;

  port =       eth[TAG+3] &  PSS_PVID_MASK;
  if (unlikely(port >= NPHY))
    return -EPROTOTYPE;

  if (unlikely(console_loglevel & 16)) {
    int len = (skb->len);
    printk("%-5s %4d %4d ", skb->dev->name, len, skb->data_len);
    printk("%02X:%02X:%02X:%02X:%02X:%02X  %02X:%02X:%02X:%02X:%02X:%02X ",
           eth[ 0], eth[ 1], eth[ 2], eth[ 3], eth[ 4], eth[ 5],
           eth[ 6], eth[ 7], eth[ 8], eth[ 9], eth[10], eth[11]);
    printk("%02X:%02X %02X:%02X %02X:%02X  %02X:%02X\n",
           eth[12], eth[13], eth[14], eth[15], eth[16], eth[17],
           eth[18], eth[19]);
  }

  // Make sure we have a valid slave device
  if (unlikely(pss_device[port] == NULL)) {
    printk(KERN_EMERG "%s: No device for eth%d\n", __FUNCTION__, port+1);
    return -ENOENT;
  }

  // Set slave device
  skb->dev = pss_device[port];
  skb->skb_iif = skb->dev->ifindex;
  skb_dst_drop(skb);

  // Update counters
  skb->dev->stats.rx_packets++;
  skb->dev->stats.rx_bytes += (skb->len - PSS_HLEN);

  // Remove PSS tag
  memmove(eth + PSS_HLEN, eth, TAG);

  if (unlikely(console_loglevel & 16)) {
    int len = (skb->len - PSS_HLEN);
    eth += PSS_HLEN;
    printk("%-5s %4d %4d ", skb->dev->name, len, skb->data_len);
    printk("%02X:%02X:%02X:%02X:%02X:%02X  %02X:%02X:%02X:%02X:%02X:%02X ",
           eth[ 0], eth[ 1], eth[ 2], eth[ 3], eth[ 4], eth[ 5],
           eth[ 6], eth[ 7], eth[ 8], eth[ 9], eth[10], eth[11]);
    printk("%02X:%02X %02X:%02X %02X:%02X  %02X:%02X\n",
           eth[12], eth[13], eth[14], eth[15], eth[16], eth[17],
           eth[18], eth[19]);
  }

  // Return success
  return PSS_HLEN;
}

static	int pss_recv(struct sk_buff* skb)
{
  int que = skb->dev->ifindex;

  // Device must actually be a PSS switch port
  if (unlikely(que >= (sizeof(map_t)*8)))
    return -ENOSYS;
  if (unlikely((pss_eths & (ONE << que)) == 0))
    return -ENOSYS;

  if (unlikely((skb = skb_unshare(skb, GFP_ATOMIC)) == NULL))
    return -ENODATA;

  if (000|000)
  if (unlikely(!pskb_may_pull(skb, PSS_HLEN)))
    goto out_drop;

  if (unlikely(console_loglevel & 32)) {
    __u8* eth = &skb->data[-ETH_HLEN];
    printk("%-5s %4d %4d ", skb->dev->name, skb->len, skb->data_len);
    printk("%02X:%02X:%02X:%02X:%02X:%02X  %02X:%02X:%02X:%02X:%02X:%02X ",
           eth[ 0], eth[ 1], eth[ 2], eth[ 3], eth[ 4], eth[ 5],
           eth[ 6], eth[ 7], eth[ 8], eth[ 9], eth[10], eth[11]);
    printk("%02X:%02X %02X:%02X %02X:%02X  %02X:%02X\n",
           eth[12], eth[13], eth[14], eth[15], eth[16], eth[17],
           eth[18], eth[19]);
  }

  // Linearize skb if broadcast/multicast MAC
  if (unlikely(skb->data[-ETH_HLEN] & 1))
  if (unlikely(skb_linearize(skb)))
    goto out_drop;

  // Get queue
  que = pss_queue[I2P(que)];

  // See if we want the current CPU
  if (unlikely(que < 0)) que = raw_smp_processor_id();

  // Receive it on specified queue
  return que;

 out_drop:
  printk(KERN_EMERG "%s: Drop on %s\n", __FUNCTION__, skb->dev->name);
  kfree_skb(skb);
  return -ENODATA;
}

static inline struct net_device* pss_create(int sw, int port, char* name)
{
  int    err;
  struct net_device* slave_dev;
  struct net_device* swdev = pss_switch[sw];

  if (!(slave_dev = alloc_netdev(0, name, ether_setup))) {
    printk(KERN_EMERG "%s: %s alloc %s failed\n",
           __FUNCTION__, swdev->name, name);
    return NULL;
  }

  slave_dev->features  = swdev->vlan_features;
  slave_dev->features &= ~NETIF_F_ALL_TSO;
  SET_ETHTOOL_OPS(slave_dev, &pss_ethtool_ops);
  memcpy(slave_dev->dev_addr, swdev->dev_addr, ETH_ALEN);
  slave_dev->tx_queue_len = 0;

  slave_dev->netdev_ops = &pss_netdev_ops;

  SET_NETDEV_DEV(slave_dev, &swdev->dev);
  slave_dev->vlan_features = swdev->vlan_features;

  if ((err = register_netdev(slave_dev)) != 0) {
    printk(KERN_EMERG "%s: %s registering %s failed, error %d\n",
           __FUNCTION__, swdev->name, slave_dev->name, err);
    free_netdev(slave_dev);
    return NULL;
  }

  if (pss_ifx <= 0) pss_ifx = slave_dev->ifindex;;

  pss_portsw[port] = sw;
  pss_device[port] = slave_dev;
  pss_header[port].s_tag = cpu_to_be16(ETH_P_PSS);
  pss_header[port].p_vid = cpu_to_be16(PSS_PVID_BASE + port);

  printk(KERN_INFO "%s:  %-6s [%2d]  %-6s tag %4d\n",
         __FUNCTION__, slave_dev->name, slave_dev->ifindex,
         swdev->name, PSS_PVID_BASE + port);

  netif_carrier_on(slave_dev);

  return slave_dev;
}

// Data storage for /proc
static	struct	proc_dir_entry* proc_wg_pss_dir;
static	struct	proc_dir_entry* proc_bridge_file;
static	struct	proc_dir_entry* proc_config_file;
static	struct	proc_dir_entry* proc_phymap_file;
static	struct	proc_dir_entry* proc_global_file;
static	struct	proc_dir_entry* proc_udelay_file;
static	struct	proc_dir_entry* proc_debug_file;
static	struct	proc_dir_entry* proc_queue_file;
static	struct	proc_dir_entry* proc_event_file;
static	struct	proc_dir_entry* proc_reset_file;
static	struct	proc_dir_entry* proc_rates_file;
static	struct	proc_dir_entry* proc_patch_file;
static	struct	proc_dir_entry* proc_register_file;

// Set up the pss devices
static	int  __init wg_pss_init(void)
{
  int    j;
  char   name[IFNAMSIZ];
  struct net_device* ethdev;
  int    e1000_read_98DX3035_reg_mdic(void*, int, int*);
  int    e1000_write_98DX3035_reg_mdic(void*, int, int);
  int    e1000_reset_98DX3035(void*);

  printk(KERN_INFO "\n%s: Built " __DATE__ " " __TIME__ "\n", __FUNCTION__);

  // Get HW function pointers
  pss_reset      = (void*)symbol_get(e1000_reset_98DX3035);
  pss_read_mdic  = (void*)symbol_get(e1000_read_98DX3035_reg_mdic);
  pss_write_mdic = (void*)symbol_get(e1000_write_98DX3035_reg_mdic);
  if ((!pss_read_mdic) || (!pss_write_mdic)) return -ENOSYS;

  // Check for MDIO bus
  if (!wg_pss_bus) {
    printk(KERN_EMERG "%s: MDIO bus not found\n", __FUNCTION__);
    return -ENOMEDIUM;
  }

  // Init PSS PHY spin lock
  spin_lock_init(&pss_mdic_lock);

  // Get pointers to the switch devices
  for (j = 0; j < NSGMII; j++) {
    sprintf(name, "eth%d", j);
    pss_switch[j] = dev_get_by_name(&init_net, name);
    if (!pss_switch[j]) {
      printk(KERN_EMERG "%s: %s not found\n", __FUNCTION__, name);
      return -ENOENT;
    }

    pss_switch[j]->flags |= IFF_NOARP; // WG:JB BUG92677/92760 No ARPs on sw devices

    sprintf(name, "sw%d", j);
    rename_eth(pss_switch[j], name);

    pss_sgmii |= (ONE << pss_switch[j]->ifindex);
  }

  ethdev = dev_get_by_name(&init_net, "eth4");
  if (ethdev)      rename_eth(ethdev, "eth0");

  ethdev = dev_get_by_name(&init_net, "eth5");
  if (ethdev)      rename_eth(ethdev, "eth25");
  ethdev = dev_get_by_name(&init_net, "eth6");
  if (ethdev)      rename_eth(ethdev, "eth26");

  // Create network devices for physical switch ports
  for (pss_balance = -1, j = 0; j < NPHY; j++) {
    struct net_device* slave_dev;

    sprintf(name, "eth%d", j + 1);
    slave_dev = pss_create(balance(), j, name);
    if (!slave_dev) {
      printk(KERN_EMERG "%s: %s not created\n", __FUNCTION__, name);
      return -ENODEV;
    }

    pss_queue[j] = j % num_online_cpus();

    pss_eths |= (ONE << slave_dev->ifindex);
  }

  // Add the PSS tag hooks
  {
    extern int (*wg_pss_untag)(struct sk_buff*, __u8*);
    extern int (*wg_pss_recv) (struct sk_buff*);
    wg_pss_untag = pss_untag;
    wg_pss_recv  = pss_recv;
  }

  // Add initial one to one bridges
  pss_add_bridges();

  // Create /proc/wg_pss directory
  proc_wg_pss_dir = proc_mkdir("wg_pss", NULL);
  if (proc_wg_pss_dir) {
    // Create /proc/wg_pss/bridge we need to configure from user mode
    proc_bridge_file   = create_proc_entry("bridge",   0666, proc_wg_pss_dir);
    if (proc_bridge_file) {
      proc_bridge_file->read_proc    = proc_read_bridge;
      proc_bridge_file->write_proc   = proc_write_bridge;
    }
    // Create /proc/wg_pss/config   to return the config info
    proc_config_file   = create_proc_entry("config",   0444, proc_wg_pss_dir);
    if (proc_config_file) {
      proc_config_file->read_proc    = proc_read_config;
    }
    // Create /proc/wg_pss/rates    to get data rates
    proc_rates_file    = create_proc_entry("rates",    0444, proc_wg_pss_dir);
    if (proc_rates_file) {
      proc_rates_file->read_proc     = proc_read_rates;
    }
    // Create /proc/wg_pss/phymap   to return the phymap info
    proc_phymap_file   = create_proc_entry("phymap",   0666, proc_wg_pss_dir);
    if (proc_phymap_file) {
      proc_phymap_file->read_proc    = proc_read_phymap;
      proc_phymap_file->write_proc   = proc_write_phymap;
    }
    // Create /proc/wg_pss/global   to set/clear the global device enable bit
    proc_global_file   = create_proc_entry("global",   0666, proc_wg_pss_dir);
    if (proc_global_file) {
      proc_global_file->read_proc    = proc_read_global;
      proc_global_file->write_proc   = proc_write_global;
    }
    // Create /proc/wg_pss/debug    to enable/disable debug flags
    proc_debug_file    = create_proc_entry("debug",    0666, proc_wg_pss_dir);
    if (proc_debug_file) {
      proc_debug_file->read_proc     = proc_read_debug;
      proc_debug_file->write_proc    = proc_write_debug;
    }
    // Create /proc/wg_pss/udelay   to get/set packet burst udelay
    proc_udelay_file   = create_proc_entry("udelay",   0666, proc_wg_pss_dir);
    if (proc_udelay_file) {
      proc_udelay_file->read_proc     = proc_read_udelay;
      proc_udelay_file->write_proc    = proc_write_udelay;
    }
    // Create /proc/wg_pss/queue    to get/set queue assignment
    proc_queue_file    = create_proc_entry("queue",    0666, proc_wg_pss_dir);
    if (proc_queue_file) {
      proc_queue_file->read_proc     = proc_read_queue;
      proc_queue_file->write_proc    = proc_write_queue;
    }
    // Create /proc/wg_pss/event    to return bridge events
    proc_event_file    = create_proc_entry("event",    0666, proc_wg_pss_dir);
    if (proc_event_file) {
      proc_event_file->read_proc     = proc_read_event;
      proc_event_file->write_proc    = proc_write_event;
    }
    // Create /proc/wg_pss/reset    to reset the PSS device
    proc_reset_file    = create_proc_entry("reset",    0222, proc_wg_pss_dir);
    if (proc_reset_file) {
      proc_reset_file->write_proc    = proc_write_reset;
    }
    // Create /proc/wg_pss/patch    to patch registers
    proc_patch_file    = create_proc_entry("patch",    0222, proc_wg_pss_dir);
    if (proc_patch_file) {
      proc_patch_file->write_proc    = proc_write_patch;
    }
    // Create /proc/wg_pss/register to enable/disable register flags
    proc_register_file = create_proc_entry("register", 0666, proc_wg_pss_dir);
    if (proc_register_file) {
      proc_register_file->read_proc  = proc_read_register;
      proc_register_file->write_proc = proc_write_register;
    }
  }

  // Set default udelay of 0 usec
  pss_udelay = 0;

  // Set up link polling
  INIT_WORK(&pss_link_poll_work, pss_link_poll_worker);
  init_timer(&pss_link_poll_timer);
  pss_link_poll_timer.function = pss_link_poll_schedule;
  pss_link_poll_timer.expires  = round_jiffies(jiffies + HZ);
  add_timer(&pss_link_poll_timer);

  // Return all done
  return 0;
}

module_init(wg_pss_init);

MODULE_LICENSE("GPL");

#endif	// UNIT_TEST
#endif	// CONFIG_NET

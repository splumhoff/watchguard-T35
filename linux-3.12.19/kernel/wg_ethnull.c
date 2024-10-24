#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>

// ethtool operations

static	int ethnull_get_settings(struct net_device* dev,
                                 struct ethtool_cmd* cmd)
{
  return -EOPNOTSUPP;
}

static	int ethnull_set_settings(struct net_device* dev,
                                 struct ethtool_cmd* cmd)
{
  return -EOPNOTSUPP;
}

static	void ethnull_get_drvinfo(struct net_device* dev,
                                 struct ethtool_drvinfo* drvinfo)
{
  strncpy(drvinfo->driver,     "ethnull", 32);
  strncpy(drvinfo->version,    "0.1", 32);
  strncpy(drvinfo->fw_version, "N/A", 32);
  strncpy(drvinfo->bus_info,   "N/A", 32);
}

static	int ethnull_nway_reset(struct net_device* dev)
{
  return -EOPNOTSUPP;
}

static	u32 ethnull_get_link(struct net_device* dev)
{
  return 0;
}

#define	ETH_SS_COUNTERS	4
#define	ITEM(s)		strncpy(data,s,len-1); data+=len

static	void ethnull_get_strings(struct net_device* dev,
                                 uint32_t stringset, uint8_t* data)
{
  int len = ETH_GSTRING_LEN;

  if (stringset == ETH_SS_STATS) {
    ITEM("tx_packets");
    ITEM("tx_bytes");
    ITEM("rx_packets");
    ITEM("rx_bytes");
  }
}

static	int ethnull_get_sset_count(struct net_device* dev, int sset)
{
  if (sset == ETH_SS_STATS) return ETH_SS_COUNTERS;
  return -EOPNOTSUPP;
}

static	void ethnull_get_ethtool_stats(struct net_device* dev,
                                       struct ethtool_stats* stats,
                                       uint64_t* data)
{
  data[0] = dev->stats.tx_packets;
  data[1] = dev->stats.tx_bytes;
  data[2] = dev->stats.rx_packets;
  data[3] = dev->stats.rx_bytes;
}

static	void ethnull_get_ringparam(struct net_device* dev,
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

static	int  ethnull_set_ringparam(struct net_device* dev,
                                   struct ethtool_ringparam* ring)
{
  return -EOPNOTSUPP;
}

static	void ethnull_get_pauseparam(struct net_device* dev,
                                    struct ethtool_pauseparam* pause)
{
  pause->autoneg  = 0;
  pause->rx_pause = 0;
  pause->tx_pause = 0;
}

static	int  ethnull_set_pauseparam(struct net_device* dev,
                                    struct ethtool_pauseparam* pause)
{
  return -EOPNOTSUPP;
}

static	const struct ethtool_ops ethnull_ethtool_ops = {
  .get_settings		= ethnull_get_settings,
  .set_settings		= ethnull_set_settings,
  .get_drvinfo		= ethnull_get_drvinfo,
  .nway_reset		= ethnull_nway_reset,
  .get_link		= ethnull_get_link,
  .get_ringparam	= ethnull_get_ringparam,
  .set_ringparam	= ethnull_set_ringparam,
  .get_pauseparam	= ethnull_get_pauseparam,
  .set_pauseparam	= ethnull_set_pauseparam,
  .get_ethtool_stats	= ethnull_get_ethtool_stats,
  .get_sset_count	= ethnull_get_sset_count,
  .get_strings		= ethnull_get_strings,
};

// ethnull device handling

static	int ethnull_init(struct net_device* dev)
{
  return 0;
}

static	int ethnull_open(struct net_device* dev)
{
  return -ENETDOWN;
}

static	int ethnull_stop(struct net_device* dev)
{
  return 0;
}

static	netdev_tx_t ethnull_xmit(struct sk_buff* skb, struct net_device* dev)
{
  kfree_skb(skb);
  return NETDEV_TX_OK;
}

static	int  ethnull_change_mtu(struct net_device *dev, int mtu)
{
  dev->mtu = mtu;
  return 0;
}

static	void ethnull_change_rx_flags(struct net_device *dev, int change)
{
}

static	void ethnull_set_rx_mode(struct net_device* dev)
{
}

static	int ethnull_set_mac_address(struct net_device* dev, void* a)
{
  memcpy(dev->dev_addr, ((struct sockaddr*)a)->sa_data, ETH_ALEN);
  return 0;
}

static	int ethnull_ioctl(struct net_device* dev, struct ifreq* ifr, int cmd)
{
  return -EOPNOTSUPP;
}

static	const struct net_device_ops ethnull_netdev_ops = {
  .ndo_init		  = ethnull_init,
  .ndo_open	 	  = ethnull_open,
  .ndo_stop		  = ethnull_stop,
  .ndo_start_xmit	  = ethnull_xmit,
  .ndo_change_mtu	  = ethnull_change_mtu,
  .ndo_change_rx_flags	  = ethnull_change_rx_flags,
  .ndo_set_rx_mode	  = ethnull_set_rx_mode,
  .ndo_set_mac_address	  = ethnull_set_mac_address,
  .ndo_do_ioctl		  = ethnull_ioctl,
};

static inline struct net_device* ethnull_create(char* name)
{
  int    err;
  struct net_device* ethnull_dev;

  if (!(ethnull_dev = alloc_netdev(0, name, ether_setup))) {
    return NULL;
  }

  ethnull_dev->features  = 0;
  SET_ETHTOOL_OPS(ethnull_dev, &ethnull_ethtool_ops);
  memset(ethnull_dev->dev_addr, 0, ETH_ALEN);
  ethnull_dev->tx_queue_len = 0;

  ethnull_dev->netdev_ops = &ethnull_netdev_ops;

  // SET_NETDEV_DEV(ethnull_dev, NULL);
  ethnull_dev->vlan_features = 0;

  if ((err = register_netdev(ethnull_dev)) != 0) {
    printk(KERN_EMERG "%s: registering %s failed, error %d\n",
           __FUNCTION__, ethnull_dev->name, err);
    free_netdev(ethnull_dev);
    return NULL;
  }

  netif_carrier_off(ethnull_dev);

  return ethnull_dev;
}

static	int  __init wg_ethnull_init(void)
{
  int    rc = -ENODEV;

#ifdef	CONFIG_X86_64
  int    j, k = 0;
  char   name[IFNAMSIZ];
  struct net_device* ethnull_dev;
  extern int wg_colfax;
  extern int wg_winthrop;

  if (wg_colfax   >= 1) k = 24;
  if (wg_colfax   >= 2) k = 33;
  if (wg_winthrop >= 4) k = 16;

  printk(KERN_INFO "\n%s: Built " __DATE__ " " __TIME__ " # Eths %d\n", __FUNCTION__, k);

  // create ethnull network devices to fill in gaps
  for (j = 0; j < k; j++) {
    sprintf(name, "eth%d", j);
    ethnull_dev = dev_get_by_name(&init_net, name);
    if (ethnull_dev) continue;

    ethnull_dev = ethnull_create(name);
    if (!ethnull_dev) {
      printk(KERN_EMERG "%s: %s not created\n", __FUNCTION__, name);
      return -ENODEV;
    } else
      rc = 0;

    printk(KERN_INFO "%s: %s created as an place holder device\n", __FUNCTION__, name);
  }
#endif

  return rc;
}

module_init(wg_ethnull_init);
MODULE_LICENSE("GPL");

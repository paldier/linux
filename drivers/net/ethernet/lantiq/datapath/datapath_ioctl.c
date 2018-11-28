#include "datapath_ioctl.h"

#if IS_ENABLED(CONFIG_LTQ_DATAPATH_PTP1588)
static int dp_ndo_ptp_ioctl(struct net_device *dev,
			    struct ifreq *ifr,
			    int cmd);
#endif

static int get_tsinfo(struct net_device *dev,
		      struct ethtool_ts_info *ts_info)
{
	struct mac_ops *ops;
	dp_subif_t subif = {0};
	int inst = 0;
	int err = 0;

	if (dp_get_netif_subifid(dev, NULL, NULL, NULL, &subif, 0)) {
		PR_ERR("%s dp_get_netif_subifid failed for %s\n",
				__func__, dev->name);
		return -EFAULT;
	}
	ops = dp_port_prop[inst].mac_ops[subif.port_id];
	if (!ops)
		return -EFAULT;
	err = ops->mac_get_ts_info(ops, ts_info);
	if (err < 0)
		return -EFAULT;
	DP_DEBUG(DP_DBG_FLAG_INST,
		 "get_tsinfo done:%s\n",
		 dev->name);
	return 0;
}

int dp_ops_set(void **dev_ops, int ops_cb_offset,
	       size_t ops_size, void **dp_orig_ops_cb,
	       void *dp_new_ops, void *new_ops_cb)
{
	void **dev_ops_cb = NULL;
	int i;

	if (!dev_ops) {
		PR_ERR("dev_ops NULL\n");
		return DP_FAILURE;
	}
	if (!dp_new_ops) {
		PR_ERR("dp_new_ops NULL\n");
		return DP_FAILURE;
	}
	if (*dev_ops != dp_new_ops) {
		if (*dev_ops) {
			*dp_orig_ops_cb = *dev_ops; /* save * old * ops * */
			for (i = 0; i < ops_size / sizeof(unsigned long *); i++)
				*((unsigned long *)(dp_new_ops) + i) =
					*((unsigned long *)(*dev_ops) + i);
		} else {
			*dp_orig_ops_cb = NULL;
		}
		*dev_ops = dp_new_ops;
			}
		/* callback for ops  */
		dev_ops_cb = (void **)((char *)dp_new_ops + ops_cb_offset);
		*dev_ops_cb = new_ops_cb;

	return	DP_SUCCESS;
}

#if IS_ENABLED(CONFIG_LTQ_DATAPATH_PTP1588)
static int dp_ndo_ptp_ioctl(struct net_device *dev,
			    struct ifreq *ifr, int cmd)
{
	int err = 0;
	struct mac_ops *ops;
	int inst = 0;
	struct pmac_port_info *port;

	port = get_port_info_via_dp_name(dev);
	if (!port)
		return -EFAULT;

	ops = dp_port_prop[inst].mac_ops[port->port_id];
	if (!ops)
		return -EFAULT;

	switch (cmd) {
		case SIOCSHWTSTAMP: {
			err = ops->set_hwts(ops, ifr);
			if (err < 0) {
				port->f_ptp = 0;
				break;
			}
			port->f_ptp = 1;
			DP_DEBUG(DP_DBG_FLAG_DBG,
				 "PTP in SIOCGHWTSTAMP done\n");
			}
			break;
		case SIOCGHWTSTAMP: {
			ops->get_hwts(ops, ifr);
			DP_DEBUG(DP_DBG_FLAG_DBG,
				 "PTP in SIOCGHWTSTAMP done\n");
			break;
		}
	}

	return err;
}

int dp_register_ptp_ioctl(struct dp_dev *dp_dev,
			  struct net_device *dev, int inst)
{
	struct dp_cap cap;
	int err = DP_SUCCESS;

	if (!dev)
		return DP_FAILURE;
	if (!dp_dev)
		return DP_FAILURE;
	cap.inst = inst;
	dp_get_cap(&cap, 0);
	if (!cap.hw_ptp)
		return DP_FAILURE;

	    /* netdev ops register */
	err = dp_ops_set((void **)&dev->netdev_ops,
			 offsetof(const struct net_device_ops, ndo_do_ioctl),
			 sizeof(*dev->netdev_ops),
			 (void **)&dp_dev->old_dev_ops,
			 &dp_dev->new_dev_ops,
			 &dp_ndo_ptp_ioctl);
	if (err)
		return DP_FAILURE;

	/* ethtool ops register */
	err = dp_ops_set((void **)&dev->ethtool_ops,
			 offsetof(const struct ethtool_ops, get_ts_info),
			 sizeof(*dev->ethtool_ops),
			 (void **)&dp_dev->old_ethtool_ops,
			 &dp_dev->new_ethtool_ops,
			 &get_tsinfo);
	if (err)
		return DP_FAILURE;

	DP_DEBUG(DP_DBG_FLAG_INST,
		 "dp_register_ptp_ioctl done:%s\n",
		 dev->name);
	return DP_SUCCESS;
}
#endif
int dp_ops_reset(struct dp_dev *dp_dev,
		 struct net_device *dev)
{
	if (dev->netdev_ops == &dp_dev->new_dev_ops) {
		dev->netdev_ops = dp_dev->old_dev_ops;
		dp_dev->old_dev_ops = NULL;
	}
	if (dev->ethtool_ops == &dp_dev->new_ethtool_ops) {
		dev->ethtool_ops = dp_dev->old_ethtool_ops;
		dp_dev->old_ethtool_ops = NULL;
	}
#if IS_ENABLED(CONFIG_LTQ_DATAPATH_SWITCHDEV)
	if (dev->switchdev_ops == &dp_dev->new_swdev_ops) {
		dev->switchdev_ops = dp_dev->old_swdev_ops;
		dp_dev->old_swdev_ops = NULL;
	}
#endif
	return DP_SUCCESS;
}


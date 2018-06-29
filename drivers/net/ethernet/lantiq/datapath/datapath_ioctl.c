#include "datapath_ioctl.h"

static int dp_ndo_ptp_ioctl(struct net_device *dev,
                 struct ifreq *ifr, int cmd);


static int dp_ndo_ptp_ioctl(struct net_device *dev,
                 struct ifreq *ifr, int cmd)
{
	int err = DP_SUCCESS;
	struct mac_ops *ops;
	int inst = 0;
	
	struct pmac_port_info *port = get_port_info_via_dp_name(dev);
	if(!port)
		return DP_FAILURE;
	
	ops = dp_port_prop[inst].mac_ops[port->port_id];
	if(!ops)
		return DP_FAILURE;
	
	switch(cmd) {
		case SIOCSHWTSTAMP: {
			port->f_ptp = ops->set_hwts(ops, ifr);
			if (port->f_ptp < 0) {
				err = DP_FAILURE;
				break;
			}
			DP_DEBUG(DP_DBG_FLAG_DBG,
				"PTP in SIOCGHWTSTAMP done\n");
			}
			break;
		case SIOCGHWTSTAMP:
			ops->get_hwts(ops, ifr);
			DP_DEBUG(DP_DBG_FLAG_DBG,
				"PTP in SIOCGHWTSTAMP done\n");
			break;
		}

	return err;
}

int dp_register_ptp_ioctl(struct dp_dev *dp_dev,
			struct net_device *dp_port, int inst)
{
	struct dp_cap cap;

	cap.inst = inst;
	dp_get_cap(&cap,0);
	if (!cap.hw_ptp)
		return DP_FAILURE;
	if (!dp_dev->old_dev_ops)
		dp_dev->old_dev_ops = dp_port->netdev_ops;
	if (dp_port->netdev_ops)
		dp_dev->new_dev_ops = *dp_port->netdev_ops;

	dp_dev->new_dev_ops.ndo_do_ioctl = dp_ndo_ptp_ioctl,
	dp_port->netdev_ops =
		(const struct net_device_ops *)&dp_dev->new_dev_ops;
	DP_DEBUG(DP_DBG_FLAG_INST,
		"dp_port_register_ptp_ioctl done:%s\n",
		dp_port->name);
	return DP_SUCCESS;
}

int dp_deregister_ptp_ioctl(struct dp_dev *dp_dev,
                  struct net_device *dev, int inst)
{
	struct dp_cap cap;

	cap.inst = inst;
	dp_get_cap(&cap,0);
	if (!cap.hw_ptp)
		return DP_FAILURE;

	if (dp_dev->old_dev_ops)
		dev->netdev_ops = dp_dev->old_dev_ops;

	return DP_SUCCESS;
}


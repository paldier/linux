#include <net/datapath_api.h>
#include <net/datapath_api_skb.h>
#include "datapath.h"
#include "datapath_instance.h"

int dp_register_ptp_ioctl(struct dp_dev *dp_dev,
                  struct net_device *dp_port, int inst);
int dp_deregister_ptp_ioctl(struct dp_dev *dp_dev,
                  struct net_device *dev, int inst);

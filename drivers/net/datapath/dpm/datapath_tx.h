#ifndef DATAPATH_TX_H_7PAJCKO6
#define DATAPATH_TX_H_7PAJCKO6

#include <net/datapath_api_tx.h>

int dp_tx_init(int inst);

void dp_tx_register_process(enum DP_TX_PRIORITY priority, tx_fn fn,
			    void *priv);

void dp_tx_register_preprocess(enum DP_TX_PRIORITY priority, tx_fn fn,
			       void *priv, bool always_enabled);

int dp_tx_update_list(void);

int dp_tx_start(struct sk_buff *skb, struct dp_tx_common *cmn);

int dp_tx_err(struct sk_buff *skb, struct dp_tx_common *cmn, int ret);

#endif /* end of include guard: DATAPATH_TX_H_7PAJCKO6 */

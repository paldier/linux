#include <linux/kernel.h>
#include <linux/types.h>

#include "datapath.h"
#include "datapath_tx.h"

struct tx_entry {
	tx_fn cb;
	void *priv;
};

/**
 * struct tx_stored - entries been set
 * @process: external process
 * @preprocess: internal preprocess
 * @preprocess_always_enabled: process even when hook is not registered
 */
struct tx_stored {
	struct tx_entry process;
	struct tx_entry preprocess;
	bool preprocess_always_enabled;
};

/**
 * struct tx_hook_list - datapath TX call chain
 * @fn: callback function, sorted by priority
 * @cnt: number of callback function
 */
struct tx_hook_list {
	struct rcu_head rcu_head;
	struct tx_entry chain[DP_TX_CNT * 2];
	u8 cnt;
};

/**
 * struct dp_tx_context - datapath TX runtime
 * @lock: Lock to pretect concurrent update
 * @en: TX list for reader
 * @stored: TX lists for updater
 */
struct dp_tx_context {
	spinlock_t lock;
	struct tx_hook_list __rcu *en;
	struct tx_stored stored[DP_TX_CNT];
};

static struct dp_tx_context *dp_tx_ctx;

int dp_tx_err(struct sk_buff *skb, struct dp_tx_common *cmn, int ret)
{
	switch (ret) {
	case DP_XMIT_ERR_NOT_INIT:
		PR_RATELIMITED("dp_xmit failed for dp no init yet\n");
		break;

	case DP_XMIT_ERR_IN_IRQ:
		PR_RATELIMITED("dp_xmit not allowed in interrupt context\n");
		break;

	case DP_XMIT_ERR_NULL_SUBIF:
		PR_RATELIMITED("dp_xmit failed for rx_subif null\n");
		UP_STATS(get_dp_port_info(0, 0)->tx_err_drop);
		break;

	case DP_XMIT_ERR_PORT_TOO_BIG:
		UP_STATS(get_dp_port_info(0, 0)->tx_err_drop);
		PR_RATELIMITED("rx_subif->port_id >= max_ports");
		break;

	case DP_XMIT_ERR_NULL_SKB:
		PR_RATELIMITED("skb NULL");
		UP_STATS(get_dp_port_info(0, cmn->dpid)->tx_err_drop);
		break;

	case DP_XMIT_ERR_NULL_IF:
		UP_STATS(cmn->mib->tx_pkt_dropped);
		PR_RATELIMITED("rx_if NULL");
		break;

	case DP_XMIT_ERR_REALLOC_SKB:
		PR_INFO_ONCE("dp_create_new_skb failed\n");
		break;

	case DP_XMIT_ERR_EP_ZERO:
		PR_ERR("Why ep zero in dp_xmit for %s\n",
		       skb->dev ? skb->dev->name : "NULL");
		break;

	case DP_XMIT_ERR_GSO_NOHEADROOM:
		PR_ERR("No enough skb headerroom(GSO). Need tune SKB buffer\n");
		break;

	case DP_XMIT_ERR_CSM_NO_SUPPORT:
		PR_RATELIMITED("dp_xmit not support checksum\n");
		break;

	case DP_XMIT_PTP_ERR:
		break;

	default:
		UP_STATS(cmn->mib->tx_pkt_dropped);
		PR_INFO_ONCE("Why come to here:%x\n",
			     get_dp_port_info(0, cmn->dpid)->status);
	}

	if (skb)
		dev_kfree_skb_any(skb);
	return DP_FAILURE;
}

int dp_tx_update_list(void)
{
	struct dp_tx_context *ctx = dp_tx_ctx;
	struct tx_hook_list *new_data;
	struct tx_hook_list *old_data;
	u8 cnt = 0;
	u8 i;

	/* update list for fast read */
	new_data = kmalloc(sizeof(struct tx_hook_list), GFP_ATOMIC);
	if (!new_data)
		return -ENOMEM;
	/* add spin lock to protect modules update TX list concurrently */
	spin_lock(&ctx->lock);
	for (i = 0; i < DP_TX_CNT; i++) {
		const struct tx_stored *s = &ctx->stored[i];
		bool en = s->preprocess_always_enabled || s->process.cb;

		if (s->preprocess.cb && en) {
			memcpy(&new_data->chain[cnt++], &s->preprocess,
			       sizeof(struct tx_entry));
		}
		if (s->process.cb) {
			memcpy(&new_data->chain[cnt++], &s->process,
			       sizeof(struct tx_entry));
		}
	}
	new_data->cnt = cnt;
	old_data = rcu_access_pointer(ctx->en);
	rcu_assign_pointer(ctx->en, new_data);
	spin_unlock(&ctx->lock);

	if (old_data)
		kfree_rcu(old_data, rcu_head);
	return 0;
}

void dp_tx_register_process(enum DP_TX_PRIORITY priority, tx_fn fn, void *priv)
{
	struct tx_stored *s;

	if (!dp_tx_ctx)
		return;
	s = &dp_tx_ctx->stored[priority];
	s->process.cb = fn;
	s->process.priv = priv;
}

void dp_tx_register_preprocess(enum DP_TX_PRIORITY priority, tx_fn fn,
			       void *priv, bool always_enabled)
{
	struct tx_stored *s;

	if (!dp_tx_ctx)
		return;
	s = &dp_tx_ctx->stored[priority];
	s->preprocess.cb = fn;
	s->preprocess.priv = priv;
	s->preprocess_always_enabled = always_enabled;
}

int dp_tx_start(struct sk_buff *skb, struct dp_tx_common *cmn)
{
	int ret = DP_XMIT_ERR_NOT_INIT;
	struct tx_hook_list *list;
	u8 cnt;
	u8 i;

	/* dp_tx_start() could be called in either normal context or softirq */
	rcu_read_lock();
	list = rcu_dereference(dp_tx_ctx->en);
	cnt = list->cnt;
	for (i = 0; i < cnt; i++) {
		const struct tx_entry *entry = &list->chain[i];

		ret = entry->cb(skb, cmn, entry->priv);
		if (ret != DP_TX_FN_CONTINUE)
			break;
	}
	rcu_read_unlock();
	return ret;
}

int dp_tx_init(int inst)
{
	if (inst)
		return 0;
	dp_tx_ctx = kzalloc(sizeof(struct dp_tx_context), GFP_ATOMIC);
	if (!dp_tx_ctx)
		return -ENOMEM;
	spin_lock_init(&dp_tx_ctx->lock);
	return 0;
}

int dp_register_tx(enum DP_TX_PRIORITY priority, tx_fn fn, void *priv)
{
	if (!dp_tx_ctx)
		return -ENOMEM;
	dp_tx_register_process(priority, fn, priv);
	return dp_tx_update_list();
}
EXPORT_SYMBOL(dp_register_tx);

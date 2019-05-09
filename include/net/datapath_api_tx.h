#ifndef DATAPATH_API_TX_H_34JWW7RR
#define DATAPATH_API_TX_H_34JWW7RR

/*! @{ */
/*!
 * @file datapath_api_tx.h
 *
 * @brief &nbsp; Datapath TX path API
 */
 /*! @} */

#ifndef DP_TX_FN_CONTINUE
#define DP_TX_FN_CONTINUE 1 /*!< @brief return this value in callback
			       function to continue execution */
#endif /* !DP_TX_FN_CONTINUE */

/**
 * enum DP_TX_PRIORITY - define TX chain priority from high to low
 */
/*! @brief define TX chain priority from high to low */
enum DP_TX_PRIORITY {
	DP_TX_PP, /*!< packet processor */
	DP_TX_TSO, /*!< traffic offload engine */
	DP_TX_VPNA, /*!< VPN adapter */
	DP_TX_CQM,  /*!< CQM CPU port */
	DP_TX_CNT,
};

/**
 * enum DP_TX_FLAGS - define TX path common flags
 */
/*! @brief define TX path common flags */
enum DP_TX_FLAGS {
	DP_TX_FLAG_INSERT_PMAC = BIT(0), /*!< insert PMAC header*/
	DP_TX_FLAG_STREAM_PORT = BIT(1), /*!< is stream port*/
	DP_TX_FLAG_SKIP_NEXT = BIT(2), /*!< skip next TX call*/
};

/**
 * struct dp_tx_common - datapath TX callback parameters
 * @flags: bitmap of DP_TX_FLAGS
 * @private: private value from dp_register_txpath()
 */
/*! @brief datapath TX callback parameters */
struct dp_tx_common {
	u8 *pmac; /*!< PMAC */
	u32 flags; /*!< bitmap of enum DP_TX_FLAGS */
	u8 len; /*!< length of PMAC */
	u8 dpid; /*!< datapath port ID */
	u8 vap; /*!< datapath port subif vap */
	u8 gpid; /*!< GPID */
	struct dev_mib *mib; /*!< per vap MIB */
};

/**
 * tx_fn() - callback prototype for TX path
 * @param[in,out] skb: skb to process
 * @param[in,out] cmn: common data which is persist across the call chain
 * @param[in,out] p: private parameter passed in from dp_register_tx()
 *
 * @Return 0 - skb consumed, stop further process;
 *         DP_TX_FN_CONTINUE skb - not consumed, continue for further process;
 *         others - skb not consumed with error code, stop further process
 */
typedef int (*tx_fn)(struct sk_buff *skb, struct dp_tx_common *cmn, void *p);

/**
 * dp_register_tx() - register datapath tx path function
 * @priority: TX priority
 * @tx_fn: callback function
 * @p: parameters
 */
int dp_register_tx(enum DP_TX_PRIORITY priority, tx_fn fn, void *p);

/**
 * dp_tx_skip_next_call() - set to skip next TX call
 */
static inline void dp_tx_skip_next_call(struct dp_tx_common *cmn)
{
	cmn->flags |= DP_TX_FLAG_SKIP_NEXT;
}

/**
 * dp_tx_call_skipped() - check if current call should be skipped
 */
static inline bool dp_tx_call_skipped(struct dp_tx_common *cmn)
{
	if (unlikely(cmn->flags & DP_TX_FLAG_SKIP_NEXT)) {
		cmn->flags &= ~DP_TX_FLAG_SKIP_NEXT;
		return true;
	} else {
		return false;
	}
}

#endif /* end of include guard: DATAPATH_API_TX_H_34JWW7RR */

/******************************************************************************
 *                Copyright (c) 2016, 2017 Intel Corporation
 *
 *
 * For licensing information, see the file 'LICENSE' in the root folder of
 * this software module.
 *
 ******************************************************************************/

#ifndef _MAC_OPS_H_
#define _MAC_OPS_H_

#include "gsw_irq.h"
#include "gsw_types.h"
#ifdef __KERNEL__
#include <linux/netdevice.h>
#endif

typedef enum  {
	/* Adaption layer does not insert FCS */
	TX_FCS_NO_INSERT = 0,
	/* Adaption layer insert FCS */
	TX_FCS_INSERT,
	/* Reserved 1 */
	TX_FCS_RES1,
	/* Reserved 2 */
	TX_FCS_RES2,
	/* FDMA does not remove FCS */
	TX_FCS_NO_REMOVE,
	/* FDMA remove FCS */
	TX_FCS_REMOVE,
	/* Reserved 3 */
	TX_FCS_RES3,
	/* Reserved 4 */
	TX_FCS_RES4,
	/* Packet does not have special tag and special tag is not removed */
	TX_SPTAG_NOTAG,
	/* Packet has special tag and special tag is replaced */
	TX_SPTAG_REPLACE,
	/* Packet has special tag and special tag is not removed */
	TX_SPTAG_KEEP,
	/* Packet has special tag ans special tag is removed */
	TX_SPTAG_REMOVE,
	/* Packet does not have FCS and FCS is not removed */
	RX_FCS_NOFCS,
	/* Reserved */
	RX_FCS_RES,
	/* Packet has FCS and FCS is not removed */
	RX_FCS_NO_REMOVE,
	/* Packet has FCS and FCS is removed */
	RX_FCS_REMOVE,
	/* Packet does not have time stamp and time stamp is not inserted */
	RX_TIME_NOTS,
	/* Packet does not have time stamp and time stamp is inserted */
	RX_TIME_INSERT,
	/* Packet has time stamp and time stamp is not inserted */
	RX_TIME_NO_INSERT,
	/* Reserved */
	RX_TIME_RES,
	/* Packet does not have special tag and special tag is not inserted. */
	RX_SPTAG_NOTAG,
	/* Packet does not have special tag and special tag is inserted. */
	RX_SPTAG_INSERT,
	/* Packet has special tag and special tag is not inserted. */
	RX_SPTAG_NO_INSERT,
	/* Reserved */
	RX_SPTAG_RES,
} MAC_OPER_CFG;

struct mac_ops {
	/* This function Sets the Flow Ctrl operation in Both XGMAC and LMAC.
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * param[in/out]IN:	mode	0 - Auto Mode based on GPHY/XPCS link.
	 *				1 - Flow Ctrl enabled only in RX
	 *				2 - Flow Ctrl enabled only in TX
	 *				3 - Flow Ctrl enabled both in RX & TX
	 *				4 - Flow Ctrl disabled both in RX & TX
	 * return	OUT  0:Flow Ctrl operation Set Successfully
	 * return	OUT  !0:Flow Ctrl operation Set Error
	 */
	int(*set_flow_ctl)(void *, u32);
	/* This function Sets the Mac Adress in xgmac.
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * param[in/out]IN:	*mac_addr MAC source address to Set
	 * return	OUT	-1:	Source Address Set Error
	 */
	int(*set_macaddr)(void *, u8 *);
	/* This function Enables/Disables Rx CRC check.
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * param[in/out]IN:	disable	Disable=1, Enable=0
	 * return	OUT	-1:	Set Failed
	 */
	int(*set_rx_crccheck)(void *, u8);
	/* This function configure treatment of special tag
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * param[in/out]IN:	mode	0 - packet does not have special tag
	 *				1 - packet has special tag and special tag is replaced
	 *				2 - packet has special tag and no modification
	 *				3 - packet has special tag and special tag is removed
	 * return	OUT	-1:	Set Failed
	 */
#define SPTAG_MODE_NOTAG	0
#define SPTAG_MODE_REPLACE	1
#define SPTAG_MODE_KEEP		2
#define SPTAG_MODE_REMOVE	3
	int(*set_sptag)(void *, u8);
	/* This function Gets the Flow Ctrl operation in Both XGMAC and LMAC.
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * return	OUT:	mode	0 - Auto Mode based on GPHY/XPCS link.
	 *				1 - Flow Ctrl enabled only in RX
	 *				2 - Flow Ctrl enabled only in TX
	 *				3 - Flow Ctrl enabled both in RX & TX
	 *				4 - Flow Ctrl disabled both in RX & TX
	 * return	OUT  -1:	Flow Ctrl operation Get Error
	 */
	int(*get_flow_ctl)(void *);
	/* This function Resets the MAC module.
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * return	OUT  0:	Reset of MAC module Done Successfully
	 * return	OUT  -1:	Reset of MAC module Error
	 */
	int(*mac_reset)(void *);
	/* This function Configures MAC Loopback.
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * param[in/out]IN:	val	1 - Loopback Enable, 0 - Loopback Dis
	 * return	OUT  -1:	Loopback Set Error
	 */
	int(*mac_config_loopback)(void *, u32);
	/* This function Configures MAC IPG.
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * param[in/out]IN:	IPG val	Value is from 0 - 7,
	 *			where 0 denotes the default 96 bits
	 * 			000: 96 bit
	 *			001: 128 bit
	 *			010: 160 bit
	 *			011: 192 bit
	 *			100: 224 bit
	 *			101 – 111: Reserved
	 * return	OUT  -1:	IPG Set Error
	 */
	int(*mac_config_ipg)(void *, u32);
	/* This function Configures the Speed
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * param[in/out]IN:	speed(Mbps)
	 *			10  	- 10Mbps
	 *			100 	- 100Mbps
	 *			1000  	- 1Gbps
	 *			25000  	- 2.5Gbps
	 *			100000  - 10Gbps
	 * return	OUT  -1:	Speed Set Error
	 */

	int(*set_speed)(void *, u32);
	/* This function Gets the Speed
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * return	OUT:	speed(Mbps)
	 *			10  	- 10Mbps
	 *			100 	- 100Mbps
	 *			1000  	- 1Gbps
	 *			25000  	- 2.5Gbps
	 *			100000  - 10Gbps
	 * return	OUT  -1:	Speed Get Error
	 */
	int(*get_speed)(void *);
	/* This function Configures the Duplex
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * param[in/out]IN:	duplex
	 *			0  	- Full Duplex
	 *			1 	- Half Duplex
	 *			2 	- Auto
	 * return	OUT  -1:	Duplex Set Error
	 */
	int(*set_duplex)(void *, u32);
	/* This function Gets the Duplex value
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * return	OUT:	duplex
	 *			0  	- Full Duplex
	 *			1 	- Half Duplex
	 *			2 	- Auto
	 * return	OUT  -1:	Duplex Get Error
	 */
	int(*get_duplex)(void *);
	/* This function Configures the LPI
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * param[in/out]IN:	LPI EN
	 * 			0  	- Disable
	 *			1 	- Enable
	 * param[in/out]IN:	LPI Wait time for 100M in usec
	 * param[in/out]IN:	LPI Wait time for 1G in usec
	 * return	OUT	-1:	LPI Set Error
	 */
	int(*set_lpi)(void *, u32, u32, u32);
	/* This function Gets the LPI Enable/Disable
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * return	OUT:	LPI EN
	 *			0  	- Disable
	 *			1 	- Enable
	 * return	OUT  -1:	LPI Get Error
	 */
	int(*get_lpi)(void *);
	/* This function Configures the MII Interface
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * param[in/out]IN:	MII Mode
	 *			0  	- MII
	 *			2 	- GMII
	 *			4 	- XGMII
	 *			5 	- LMAC GMII
	 * return	OUT  -1:	MII Interface Set Error
	 */
	int(*set_mii_if)(void *, u32);
	/* This function Gets the MII Interface
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * return	OUT:	MII Mode
	 *			0  	- MII
	 *			2 	- GMII
	 *			4 	- XGMII
	 *			5 	- LMAC GMII
	 * return	OUT  -1:	MII Interface Get Error
	 */
	int(*get_mii_if)(void *);
	/* This function Sets the Link Status
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * param[in/out]IN:	Link
	 *			0  	- LINK Force UP
	 *			1  	- LINK Force DOWN
	 *			Any - AUTO Mode
	 * return	OUT  -1:	Link Status Set Error
	 */
	int(*set_link_sts)(void *, u8);
	/* This function Gets the Link Status
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * return	OUT:	Link
	 *			0  	- LINK Force UP
	 *			1  	- LINK Force DOWN
	 *			Any - AUTO Mode
	 * return	OUT  -1:	Link Status Get Error
	 */
	int(*get_link_sts)(void *);
	/* This function Sets the MTU Configuration
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * param[in/out]IN:	MTU
	 *			Max MTU that can be set is 10000 for Falcon-Mx
	 * return	OUT	-1: 	MTU set exceed the Max limit
	 */
	int(*set_mtu)(void *, u32);
	/* This function Gets the MTU Configuration
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * return	OUT:	MTU
	 * 			MTU configured
	 * return	OUT  -1:	MTU Get Error
	 */
	int(*get_mtu)(void *);
	/* This function Sets the Pause frame Source Address
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * param[in/out]IN:	*mac_addr MAC source address to Set
	 * param[in/out]IN:	mode
	 *			1 - PORT specific MAC source address
	 *			0 - COMMON MAC source address
	 * return	OUT	-1: 	Pause frame Source Address Set Error
	 */
	int(*set_pfsa)(void *, u8 *, u32);
	/* This function Gets the Pause frame Source Address
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * return	OUT:	*mac_addr	MAC source address which is set
	 * return	OUT:	*mode		Mode configured
	 *			1 - PORT specific MAC source address
	 *			0 - COMMON MAC source address
	 * return	OUT	-1:	Pause frame Source Address Get Error
	 */
	int(*get_pfsa)(void *, u8 *, u32 *);
	/* This function Sets the FCS generation Configuration
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * param[in/out]IN:	val	FCS generation Configuration
	 *			0 - CRC and PAD insertion are enabled.
	 *			1 - CRC insert enable and PAD insert disable
	 *			2 - CRC and PAD are not insert and not replaced.
	 * return	OUT	-1:	FCS generation Set Error
	 */
	int(*set_fcsgen)(void *, u32);
	/* This function Gets the FCS generation Configuration
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * return	OUT:	val	FCS generation Configuration
	 *			0 - CRC and PAD insertion are enabled.
	 *			1 - CRC insert enable and PAD insert disable
	 *			2 - CRC and PAD are not insert and not replaced.
	 * return	OUT	-1:	FCS generation Get Error
	 */
	int(*get_fcsgen)(void *);
	/* This function Clears MAC Interrupt Status
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * param[in/out]IN:	event	Difffernt events to clear
	 * return	OUT	: 	Cleared return status
	 */
	int(*clr_int_sts)(void *, u32);
	/* This function Gets MAC Interrupt Status
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * return	OUT	: 	Interrupts Pending
	 */
	int(*get_int_sts)(void *);
	/* This function Initializes System time Configuration
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * param[in/out]IN:	sec	Initial seconds value to be configured
	 *				in register
	 * param[in/out]IN:	nsec	Initial nano-seconds value to be
	 *				configured in register
	 * return	OUT	-1: 	System time Configuration Set Error
	 */
	int(*init_systime)(void *, u32, u32);
	/* This function Configures addend value
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * param[in/out]IN:	addend
	 * The Timestamp Addend register is present only when the IEEE 1588
	 * Timestamp feature is selected without
	 * external timestamp input. This register value is used only when the
	 * system time is configured for Fine
	 * Update mode (TSCFUPDT bit in the MAC_Timestamp_Ctrl register).
	 * The content of this register is
	 * added to a 32-bit accumulator in every clock cycle and the
	 * system time is updated whenever the accumulator overflows.
	 * This field indicates the 32-bit time value to be added to the
	 * Accumulator register to achieve time synchronization.
	 * return	OUT	-1: 	Addend Configuration Set Error
	 */
	int(*config_addend)(void *, u32);
	/* This function Adjust System Time for PTP
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * param[in/out]IN:	sec	New seconds value to be configured
	 * param[in/out]IN:	nsec	New nano seconds value to be configured
	 * param[in/out]IN:	addsub	Add or Subtract Time
	 * When this bit is set, the time value is subtracted with the
	 * contents of the update register. When this bit is reset,
	 * the time value is added with the contents of the update register.
	 * param[in/out]IN:	one_nsec_accuracy
	 *	If the new nsec value need to be subtracted with
	 *	the system time, then MAC_STNSUR.TSSS field should be
	 *	programmed with,
	 *	(10^9 - <new_nsec_value>) if MAC_TX_CFG.TSCTRLSSR is set or
	 *	(2^31 - <new_nsec_value> if MAC_TX_CFG.TSCTRLSSR is reset)
	 * return	OUT	-1: 		Adjust System Time for PTP Error
	 */
	int(*adjust_systime)(void *, u32, u32, u32, u32);
	/* This sequence is used get 64-bit system time in nano sec
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * return	OUT	u64: 	64-bit system time in nano sec
	 */
	u64(*get_systime)(void *);
	/* This sequence is used get Transmitted 64-bit system time in nano sec
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * return	OUT	u64: 	Tx 64-bit system time in nano sec
	 */
	u64(*get_tx_tstamp)(void *);
	/* This sequence is used get Tx Transmitted capture count
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * return	OUT	u32: 	Tx Timestamp capture count
	 */
	int(*get_txtstamp_cap_cnt)(void *);
	/* This sequence is used Configure HW TimeStamping TX/RX filter Cfg
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * param[in/out]IN:	tx_type: 1/0 - ON/OFF
	 * param[in/out]IN:	rx_filter:	This is based on Table 7-146,
	 *			Receive side timestamp Capture scenarios
	 * return	OUT	-1:	Configure HW TimeStamping Set Error
	 */
	int(*config_hw_time_stamping)(void *, u32, u32);
	/* This sequence is used ConfigureSub Second Increment
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * param[in/out]IN:	ptp_clk: PTP Clock Value in Hz
	 *	The value programmed in this field is accumulated with the
	 *	contents of the sub-second register.
	 *	For example, when the PTP clock is 50 MHz (period is 20 ns),
	 *	you should program 20 (0x14)
	 *	when the System TimeNanoseconds register has an accuracy
	 *	of 1 ns [Bit 9 (TSCTRLSSR) is set in MAC_Timestamp_Ctrl].
	 *	When TSCTRLSSR is clear, the Nanoseconds register has a
	 *	resolution of ~0.465 ns. In this case, you
	 *	should program a value of 43 (0x2B)
	 *	which is derived by 20 ns/0.465.
	 * return	OUT	-1: 	Configure Sub Second Inccrement Error
	 */
	int(*config_subsec_inc)(void *, u32);
#ifdef __KERNEL__
	/* This sequence is used set Hardware timestamp
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 */
	int(*set_hwts)(void *, struct ifreq *);
	/* This sequence is used get Hardware timestamp
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 */
	int(*get_hwts)(void *, struct ifreq *);
	/* This sequence is used for Rx Hardware timestamp operations
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * return	OUT	-1: 	Initialize MAC Error
	 */
	int(*do_rx_hwts)(void *, struct sk_buff *);
	/* This sequence is used for Tx Hardware timestamp operations
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * return	OUT	-1: 	Initialize MAC Error
	 */
	int(*do_tx_hwts)(void *, struct sk_buff *);
	/* This sequence is get Timestamp info
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * return	OUT	-1: 	Initialize MAC Error
	 */
	int(*mac_get_ts_info)(void *, struct ethtool_ts_info *);
#endif
	/* This sequence is used Initialize MAC
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * return	OUT	-1: 	Initialize MAC Error
	 */
	int(*init)(void *);
	/* This sequence is used Exit MAC
	 * param[in/out]IN:	ops	MAC ops Struct registered for MAC 0/1/2.
	 * return	OUT	-1: 		Exit MAC Error
	 */
	int(*exit)(void *);
	/* This sequence is used for Xgmac Cli implementation
	 * param[in/out]IN:	argc - Number of args.
	 * param[in/out]IN:	argv - Argument value.
	 * return	OUT	-1: 		Exit MAC Error
	 */
	int(*xgmac_cli)(u32, u8 **);
	/* This sequence is used for Lmac Cli implementation
	 * param[in/out]IN:	argc - Number of args.
	 * param[in/out]IN:	argv - Argument value.
	 * return	OUT	-1: 		Exit MAC Error
	 */
	int(*lmac_cli)(u32, u8 **);
	/* This sequence is used for Reading XGMAC register
	 * param[in/out]IN:	ops -	MAC ops Struct registered for MAC 0/1/2.
	 * param[in/out]IN:	u16 -   Register Offset.
	 * return	OUT	u32 -	Register Value
	 */
	u32(*xgmac_reg_rd)(void *, u16);
	/* This sequence is used for Writing XGMAC register
	 * param[in/out]IN:	ops -	MAC ops Struct registered for MAC 0/1/2.
	 * param[in/out]IN:	u16 -   Register Offset.
	 * param[in/out]IN:	u32 -   Register Value.
	 */
	void(*xgmac_reg_wr)(void *, u16, u32);
	/* This sequence is used for Reading LMAC register
	 * param[in/out]IN:	ops -	MAC ops Struct registered for MAC 0/1/2.
	 * param[in/out]IN:	u16 -   Register Offset.
	 * return	OUT	u32 -	Register Value
	 */
	u32(*lmac_reg_rd)(void *, u32);
	/* This sequence is used for Writing LMAC register
	 * param[in/out]IN:	ops -	MAC ops Struct registered for MAC 0/1/2.
	 * param[in/out]IN:	u16 -   Register Offset.
	 * param[in/out]IN:	u32 -   Register Value.
	 */
	void(*lmac_reg_wr)(void *, u32, u32);
	/* This sequence is used for Registering IRQ Callback for a event
	 * param[in/out]IN:	ops -	MAC ops Struct registered for MAC 0/1/2.
	 * param[in/out]IN:	GSW_Irq_Op_t -   IRQ event info.
	 * return	OUT	int -	Success/Fail
	 */
	int (*IRQ_Register)(void *, GSW_Irq_Op_t *);
	/* This sequence is used for UnRegistering IRQ Callback for a event
	 * param[in/out]IN:	ops -	MAC ops Struct registered for MAC 0/1/2.
	 * param[in/out]IN:	GSW_Irq_Op_t -   IRQ event info.
	 * return	OUT	int -	Success/Fail
	 */
	int (*IRQ_UnRegister)(void *, GSW_Irq_Op_t *);
	/* This sequence is used for Enabling IRQ for a event
	 * param[in/out]IN:	ops -	MAC ops Struct registered for MAC 0/1/2.
	 * param[in/out]IN:	GSW_Irq_Op_t -   IRQ event info.
	 * return	OUT	int -	Success/Fail
	 */
	int (*IRQ_Enable)(void *, GSW_Irq_Op_t *);
	/* This sequence is used for Disabling IRQ for a event
	 * param[in/out]IN:	ops -	MAC ops Struct registered for MAC 0/1/2.
	 * param[in/out]IN:	GSW_Irq_Op_t -   IRQ event info.
	 * return	OUT	int -	Success/Fail
	 */
	int (*IRQ_Disable)(void *, GSW_Irq_Op_t *);
	/* This sequence is used for Enabling MAC Interrupt
	 * param[in/out]IN:	ops -	MAC ops Struct registered for MAC 0/1/2.
	 * return	OUT	int -	Success/Fail
	 */
	int (*mac_int_en)(void *);
	/* This sequence is used for Disabling MAC Interrupt
	 * param[in/out]IN:	ops -	MAC ops Struct registered for MAC 0/1/2.
	 * return	OUT	int -	Success/Fail
	 */
	int (*mac_int_dis)(void *);
	/* This sequence is used for Configuring Mac operation
	 * param[in/out]IN:	ops -	MAC ops Struct registered for MAC 0/1/2.
	 * param[in/out]IN:	MAC_OP_CFG - operation to perform
	 * return	OUT	int -	Success/Fail
	 */
	int (*mac_op_cfg)(void *, MAC_OPER_CFG);
};

#endif

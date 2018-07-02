/******************************************************************************
 *                Copyright (c) 2016, 2017, 2018 Intel Corporation
 *
 *
 * For licensing information, see the file 'LICENSE' in the root folder of
 * this software module.
 *
 ******************************************************************************/

#ifndef _MAC_TXFIFO_H
#define _MAC_TXFIFO_H

#define FIFO_TIMEOUT_IN_SEC         	100     /* 100 Sec */
#define MAX_FIFO_ENTRY			64	/* Max Hw Fifo available */
#define START_FIFO			1	/* */

enum {
	FIFO_FULL = -1,
	FIFO_ENTRY_AVAIL = -2,
	FIFO_ENTRY_NOT_AVAIL = -3,
};

/* GSWIP Tx Fifo to send signal from GSWIP to Xgmac
 * to do the action on the packet
 * Actions include
 * 1) Store Tx Timestamp
 * 2) One Step TImestamp update for Sync packets
 * 3) Packet ID update for the stored timestamp
 * 4) Timestamp correction needed
 * Entry will be cleared by the HW when packet is sent out
 */
struct mac_fifo_entry {
	u32 is_used;
	/* RecordID, denotes index to the 64 entry Fifo */
	u32 rec_id;
	/* Transmit Timestamp Store Enable */
	u8 ttse;
	/* One Step Timestamp Capture Enable */
	u8 ostc;
	/* One Step Timestamp Available/PacketID Available */
	u8 ostpa;
	/* Checksum Insertion or Update information */
	u8 cic;
	/* Lower 32 byte of time correction */
	u32 ttsl;
	/* Upper 32 byte of time correction */
	u32 ttsh;
	/* Timeout for this Fifo Entry */
	u32 timeout;
	u32 jiffies;
#ifdef __KERNEL__
	struct timer_list timer;
#endif
};


u32 fifo_init(void *pdev);
int fifo_entry_add(void *pdev, u8 ttse, u8 ostc, u8 ostpa, u8 cic,
		   u32 ttsl, u32 ttsh);
void fifo_entry_del(void *pdev, u32 rec_id);
void fifo_isr_handler(void *pdev);
void print_fifo(void *pdev);

#endif

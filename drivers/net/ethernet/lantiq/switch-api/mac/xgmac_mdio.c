/******************************************************************************
 *                Copyright (c) 2016, 2017 Intel Corporation
 *
 *
 * For licensing information, see the file 'LICENSE' in the root folder of
 * this software module.
 *
 ******************************************************************************/
/* =========================================================================
 * This file incorporates work covered by the following copyright and
 * permission notice:
 * The Synopsys DWC ETHER XGMAC Software Driver and documentation (hereinafter
 * "Software") is an unsupported proprietary work of Synopsys, Inc. unless
 * otherwise expressly agreed to in writing between Synopsys and you.
 *
 * The Software IS NOT an item of Licensed Software or Licensed Product under
 * any End User Software License Agreement or Agreement for Licensed Product
 * with Synopsys or any supplement thereto.  Permission is hereby granted,
 * free of charge, to any person obtaining a copy of this software annotated
 * with this license and the Software, to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS" BASIS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * ========================================================================= */


#include <xgmac_mdio.h>
#include <xgmac.h>
#ifdef __KERNEL__
#include <linux/phy.h>
#include <linux/mdio.h>
#endif

static void dump_phy_registers(void *pdev);

/* SCAR:
 * DA
 *	Device Address for indirect addressing of device for single read/write.
 * PA
 *	Port Address used for indirect single read/write access.
 * RA
 *	Register Address for single read /write
 *	When using Clause 22 frames, only the lower 5 bits of the field are used
 * SCCDR:
 * SADDR: Skip Address Frame
 *	The application sets this bit to prevent the SMA from sending address
 *	frames before read, write, or post-increment read frames.
 * CMD: Control Command
 *	SMA commands are written to these bits. The command codes are:
 *	11 SINGLE_READ
 *	01 SINGLE_WRITE
 *	10 POST INCREMENT READ OPERATION
 *	00 Reserved (Results are undefined)
 * SDATA: Single Write Data
 *	For single writes, this field contains data to be written.
 *	For single reads, this field contains data that has been read.
 */
int xgmac_mdio_single_rd(void *pdev,
			 u32 dev_adr,
			 u32 phy_id,
			 u32 phy_reg,
			 u32 *phy_reg_data)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mdio_sccdr = 0;
	u32 mdio_scar = 0;

	/* wait for any previous MDIO read/write operation to complete */

	/*Poll*/
	while (1) {
		if (XGMAC_RGRD_BITS(pdata, MDIO_SCCDR, BUSY) == 0)
			break;
	}

	/* initiate the MDIO read operation by updating desired bits
	 * PA - phy address/id (0 - 31)
	 * RA - phy register offset
	 */

	mdio_scar = XGMAC_RGRD(pdata, MDIO_SCAR);

	MAC_SET_VAL(mdio_scar, MDIO_SCAR, DA, dev_adr);
	MAC_SET_VAL(mdio_scar, MDIO_SCAR, PA, phy_id);
	MAC_SET_VAL(mdio_scar, MDIO_SCAR, RA, phy_reg);

	XGMAC_RGWR(pdata, MDIO_SCAR, mdio_scar);

	mdio_sccdr = XGMAC_RGRD(pdata, MDIO_SCCDR);

	MAC_SET_VAL(mdio_sccdr, MDIO_SCCDR, BUSY, 1);
	MAC_SET_VAL(mdio_sccdr, MDIO_SCCDR, SADDR, 0);
	MAC_SET_VAL(mdio_sccdr, MDIO_SCCDR, CMD, 3);
	MAC_SET_VAL(mdio_sccdr, MDIO_SCCDR, SDATA, 0);

	XGMAC_RGWR(pdata, MDIO_SCCDR, mdio_sccdr);

	/*DELAY IMPLEMENTATION USING udelay()*/
	//udelay(10);

	/* wait for MDIO read operation to complete */

	/*Poll*/
	while (1) {
		if (XGMAC_RGRD_BITS(pdata, MDIO_SCCDR, BUSY) == 0)
			break;
	}

	/* read the data */
	mdio_sccdr = XGMAC_RGRD(pdata, MDIO_SCCDR);
	*phy_reg_data = MAC_GET_VAL(mdio_sccdr, MDIO_SCCDR, SDATA);

	return 0;
}

void print_mdio_rd_cnt(void *pdev,
		       u32 dev_adr,
		       u32 phy_id,
		       u32 phy_reg_st,
		       u32 phy_reg_end)

{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	int clause;
	u32 i, phy_reg_data;

	clause = mdio_get_clause(pdata, pdata->mac_idx);
	mac_printf("OP    \tCL    \tDEVADR\tPHYID \tPHYREG\tDATA  \n");
	mac_printf("============================================\n");

	for (i = 0; i <= (phy_reg_end - phy_reg_st); i++) {
		xgmac_mdio_single_rd(pdev, dev_adr, phy_id, phy_reg_st + i,
				     &phy_reg_data);

		mac_printf("%s\t", "RD");
		mac_printf("%4s\t", clause ? "CL22" : "CL45");
		mac_printf("%6X\t", dev_adr);
		mac_printf("%5X\t", phy_id);
		mac_printf("%6X\t", phy_reg_st + i);
		mac_printf("%4X\t", phy_reg_data);
		mac_printf("\n");
	}
}

void xgmac_print_mdio(void *pdev,
		      u32 op,
		      u32 dev_adr,
		      u32 phy_id,
		      u32 phy_reg,
		      u32 phy_reg_data)

{
	int clause;

	clause = mdio_get_clause(pdev, phy_id);
	mac_printf("OP    \tCL    \tDEVADR\tPHYID \tPHYREG\tDATA  \n");
	mac_printf("============================================\n");

	mac_printf("%6s\t", "RD");
	mac_printf("%6s\t", clause ? "CL22" : "CL45");
	mac_printf("%6X\t", dev_adr);
	mac_printf("%6X\t", phy_id);
	mac_printf("%6X\t", phy_reg);
	mac_printf("%6X\t", phy_reg_data);
	mac_printf("\n");
}

/*brief This sequence is used to write into phy registers
 */
int xgmac_mdio_single_wr(void *pdev,
			 u32 dev_adr,
			 u32 phy_id,
			 u32 phy_reg,
			 u32 phy_reg_data)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mdio_sccdr = 0;
	u32 mdio_scar = 0;

	/* wait for any previous MDIO read/write operation to complete */

	/*Poll*/
	while (1) {
		if (XGMAC_RGRD_BITS(pdata, MDIO_SCCDR, BUSY) == 0)
			break;
	}

	mdio_scar = XGMAC_RGRD(pdata, MDIO_SCAR);

	MAC_SET_VAL(mdio_scar, MDIO_SCAR, DA, dev_adr);
	MAC_SET_VAL(mdio_scar, MDIO_SCAR, PA, phy_id);
	MAC_SET_VAL(mdio_scar, MDIO_SCAR, RA, phy_reg);

	XGMAC_RGWR(pdata, MDIO_SCAR, mdio_scar);

	mdio_sccdr = XGMAC_RGRD(pdata, MDIO_SCCDR);

	MAC_SET_VAL(mdio_sccdr, MDIO_SCCDR, SDATA, phy_reg_data);
	MAC_SET_VAL(mdio_sccdr, MDIO_SCCDR, BUSY, 1);
	MAC_SET_VAL(mdio_sccdr, MDIO_SCCDR, SADDR, 0);
	MAC_SET_VAL(mdio_sccdr, MDIO_SCCDR, CMD, 1);

	XGMAC_RGWR(pdata, MDIO_SCCDR, mdio_sccdr);

	/*DELAY IMPLEMENTATION USING udelay()*/
	//udelay(10);

	/* wait for MDIO read operation to complete */

	/*Poll*/
	while (1) {
		if (XGMAC_RGRD_BITS(pdata, MDIO_SCCDR, BUSY) == 0)
			break;
	}

	return 0;
}

/*	1 - Clause 22
 *	0 - Clause 45
 *	phy_id same as portid, 0/1/2/3
 */
int mdio_set_clause(void *pdev, u32 clause, u32 phy_id)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mdio_c22p = 0;

	mdio_c22p = XGMAC_RGRD(pdata, MDIO_C22P);

	SET_N_BITS(mdio_c22p, phy_id, MDIO_CL22P_PORT0_WIDTH, clause);

	mac_printf("MDIO: portID %d set to %s\n", phy_id,
		   clause ? "Clause 22" : "Clause 45");

	/* Select port 0, 1, 2 and 3 as Clause 22/45 ports */
	XGMAC_RGWR(pdata, MDIO_C22P, mdio_c22p);

	return 0;
}

int mdio_get_clause(void *pdev,  u32 phy_id)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mdio_c22p = 0;
	int clause;

	mdio_c22p = XGMAC_RGRD(pdata, MDIO_C22P);

	clause = GET_N_BITS(mdio_c22p, phy_id, MDIO_CL22P_PORT0_WIDTH);

	return clause;
}

int mdio_set_interrupt(void *pdev, u32 val)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mdio_ier = 0;

	if (val & MASK(MDIO_IER, CWCOMPIE))
		MAC_SET_VAL(mdio_ier, MDIO_IER, CWCOMPIE, val);

	if (val & MASK(MDIO_IER, SNGLCOMPIE))
		MAC_SET_VAL(mdio_ier, MDIO_IER, SNGLCOMPIE, val);

	if (val & MASK(MDIO_IER, PRT3ALIE))
		MAC_SET_VAL(mdio_ier, MDIO_IER, PRT3ALIE, val);

	if (val & MASK(MDIO_IER, PRT2ALIE))
		MAC_SET_VAL(mdio_ier, MDIO_IER, PRT2ALIE, val);

	if (val & MASK(MDIO_IER, PRT1ALIE))
		MAC_SET_VAL(mdio_ier, MDIO_IER, PRT1ALIE, val);

	if (val & MASK(MDIO_IER, PRT0ALIE))
		MAC_SET_VAL(mdio_ier, MDIO_IER, PRT0ALIE, val);

	if (val & MASK(MDIO_IER, PRT3LSIE))
		MAC_SET_VAL(mdio_ier, MDIO_IER, PRT3LSIE, val);

	if (val & MASK(MDIO_IER, PRT2LSIE))
		MAC_SET_VAL(mdio_ier, MDIO_IER, PRT2LSIE, val);

	if (val & MASK(MDIO_IER, PRT1LSIE))
		MAC_SET_VAL(mdio_ier, MDIO_IER, PRT1LSIE, val);

	if (val & MASK(MDIO_IER, PRT0LSIE))
		MAC_SET_VAL(mdio_ier, MDIO_IER, PRT0LSIE, val);

	if (val & MASK(MDIO_IER, PRT3CONIE))
		MAC_SET_VAL(mdio_ier, MDIO_IER, PRT3CONIE, val);

	if (val & MASK(MDIO_IER, PRT2CONIE))
		MAC_SET_VAL(mdio_ier, MDIO_IER, PRT2CONIE, val);

	if (val & MASK(MDIO_IER, PRT1CONIE))
		MAC_SET_VAL(mdio_ier, MDIO_IER, PRT1CONIE, val);

	if (val & MASK(MDIO_IER, PRT0CONIE))
		MAC_SET_VAL(mdio_ier, MDIO_IER, PRT0CONIE, val);

	XGMAC_RGWR(pdata, MDIO_IER, mdio_ier);

	return 0;
}

int xgmac_mdio_get_int_sts(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	u32 mdio_isr = XGMAC_RGRD(pdata, MDIO_ISR);
	u32 mdio_ier = XGMAC_RGRD(pdata, MDIO_IER);

	mac_printf("XGMAC %d: MDIO Interrupt Status\n", pdata->mac_idx);
	mac_printf("\tMDIO_IER interrupts  %s %08x\n",
		   mdio_ier ? "ENABLED" : "DISABLED", mdio_ier);

	if (mdio_ier & MASK(MDIO_IER, CWCOMPIE))
		mac_printf("Continuous Write Completion Interrupt Enabled\n");

	if (mdio_ier & MASK(MDIO_IER, SNGLCOMPIE))
		mac_printf("Single Command Completion Interrupt Enabled\n");

	if (mdio_ier & MASK(MDIO_IER, PRT3ALIE))
		mac_printf("Dev Present Sts Change Interrupt P3 Enabled\n");

	if (mdio_ier & MASK(MDIO_IER, PRT2ALIE))
		mac_printf("Dev Present Sts Change Interrupt P2 Enabled\n");

	if (mdio_ier & MASK(MDIO_IER, PRT1ALIE))
		mac_printf("Dev Present Sts Change Interrupt P1 Enabled\n");

	if (mdio_ier & MASK(MDIO_IER, PRT0ALIE))
		mac_printf("Dev Present Sts Change Interrupt P0 Enabled\n");

	if (mdio_ier & MASK(MDIO_IER, PRT3LSIE))
		mac_printf("Link Status Change Interrupt P3 Enabled\n");

	if (mdio_ier & MASK(MDIO_IER, PRT2LSIE))
		mac_printf("Link Status Change Interrupt P2 Enabled\n");

	if (mdio_ier & MASK(MDIO_IER, PRT1LSIE))
		mac_printf("Link Status Change Interrupt P1 Enabled\n");

	if (mdio_ier & MASK(MDIO_IER, PRT0LSIE))
		mac_printf("Link Status Change Interrupt P0 Enabled\n");

	if (mdio_ier & MASK(MDIO_IER, PRT3CONIE))
		mac_printf("Connect/Disconnect Event Interrupt P3 Enabled\n");

	if (mdio_ier & MASK(MDIO_IER, PRT2CONIE))
		mac_printf("Connect/Disconnect Event Interrupt P2 Enabled\n");

	if (mdio_ier & MASK(MDIO_IER, PRT1CONIE))
		mac_printf("Connect/Disconnect Event Interrupt P1 Enabled\n");

	if (mdio_ier & MASK(MDIO_IER, PRT0CONIE))
		mac_printf("Connect/Disconnect Event Interrupt P0 Enabled\n");

	if (!mdio_isr) {
		mac_printf("\tNo MDIO interrupt status available %08x\n",
			   mdio_isr);
	} else {
		mac_printf("\tMDIO interrupt status available %08x\n",
			   mdio_isr);

		if (mdio_isr & MASK(MDIO_ISR, CWCOMPINT))
			mac_printf("Continuous WR Completion Int Set\n");

		if (mdio_isr & MASK(MDIO_ISR, SNGLCOMPINT))
			mac_printf("Single Cmd Completion Int Set\n");

		if (mdio_isr & MASK(MDIO_ISR, PRT3ALINT))
			mac_printf("Dev Present Sts Change Int P3 Set\n");

		if (mdio_isr & MASK(MDIO_ISR, PRT2ALINT))
			mac_printf("Dev Present Sts Change Int P2 Set\n");

		if (mdio_isr & MASK(MDIO_ISR, PRT1ALINT))
			mac_printf("Dev Present Sts Change Int P1 Set\n");

		if (mdio_isr & MASK(MDIO_ISR, PRT0ALINT))
			mac_printf("Dev Present Sts Change Int P0 Set\n");

		if (mdio_isr & MASK(MDIO_ISR, PRT3LSINT))
			mac_printf("Link Sts Change Int P3 Set\n");

		if (mdio_isr & MASK(MDIO_ISR, PRT2LSINT))
			mac_printf("Link Sts Change Int P2 Set\n");

		if (mdio_isr & MASK(MDIO_ISR, PRT1LSINT))
			mac_printf("Link Sts Change Int P1 Set\n");

		if (mdio_isr & MASK(MDIO_ISR, PRT0LSINT))
			mac_printf("Link Sts Change Int P0 Set\n");

		if (mdio_isr & MASK(MDIO_ISR, PRT3CONINT))
			mac_printf("Connect/Disconnect Event Int P3 Set\n");

		if (mdio_isr & MASK(MDIO_ISR, PRT2CONINT))
			mac_printf("Connect/Disconnect Event Int P2 Set\n");

		if (mdio_isr & MASK(MDIO_ISR, PRT1CONINT))
			mac_printf("Connect/Disconnect Event Int P1 Set\n");

		if (mdio_isr & MASK(MDIO_ISR, PRT0CONINT))
			mac_printf("Connect/Disconnect Event Int P0 Set\n");
	}

	return 0;
}
#ifdef __KERNEL__
/* API to read MII PHY register
* \details This API is expected to write MII registers with the value being
* passed as the last argument which is done in write_phy_regs API
* called by this function.
*
* \param[in] bus - points to the mii_bus structure
* \param[in] phyadr - the phy address to write
* \param[in] phyreg - the phy register offset to write
* \param[in] phydata - the register value to write with
*
* \return 0 on success and -ve number on failure.
*/
static int xgmac_mdio_read(struct mii_bus *bus, int phyadr, int phyreg)
{
	struct mac_ops *pdev = bus->priv;
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	int phydata;

	mac_printf("XGMAC %d: MDIO Read phyadr = %d, phyreg = %d\n",
		   pdata->mac_idx, phyadr, phyreg);

	xgmac_mdio_single_rd(pdev, 0, phyadr, phyreg, &phydata);

	mac_printf("XGMAC %d: MDIO Read phydata = %#x\n",
		   pdata->mac_idx, phydata);

	return phydata;
}

/* API to write MII PHY register
* \details This API is expected to write MII registers with the value being
* passed as the last argument which is done in write_phy_regs API
* called by this function.
*
* \param[in] bus - points to the mii_bus structure
* \param[in] phyadr - the phy address to write
* \param[in] phyreg - the phy register offset to write
* \param[in] phydata - the register value to write with
*
* \return 0 on success and -ve number on failure.
*/

static int xgmac_mdio_write(struct mii_bus *bus, int phyadr, int phyreg,
			    u16 phydata)
{
	struct mac_ops *pdev = bus->priv;
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	int ret = 0;

	xgmac_mdio_single_wr(pdev, 0, phyadr, phyreg, phydata);

	mac_printf("XGMAC %d: MDIO Write"
		   "phyadr %x phyreg %x phydata %x Completed\n",
		   pdata->mac_idx, phyadr, phyreg, phydata);

	return ret;
}


/* API to reset PHY
 */
static int xgmac_mdio_reset(struct mii_bus *bus)
{
	struct mac_ops *pdev = bus->priv;
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	int phydata;

	mac_printf("XGMAC %d: MDIO Reset phyadr : %d\n", pdata->mac_idx,
		   pdata->phyadr);

	xgmac_mdio_single_rd(pdev, 0, pdata->phyadr, MII_BMCR,
			     &phydata);

	if (phydata < 0)
		return 0;

	/* issue soft reset to PHY */
	phydata |= BMCR_RESET;
	xgmac_mdio_single_wr(pdev, 0, pdata->phyadr, MII_BMCR, phydata);

	/* wait until software reset completes */
	do {
		xgmac_mdio_single_rd(pdev, 0, pdata->phyadr, MII_BMCR,
				     &phydata);
	} while ((phydata >= 0) && (phydata & BMCR_RESET));

	mac_printf("XGMAC %d: MDIO Reset Completed\n",
		   pdata->mac_idx);

	return 0;
}

/* API to register mdio.
 */
int xgmac_mdio_register(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);
	struct mii_bus *new_bus = NULL;
	int phyadr = 0;
	unsigned short phy_detected = 0;
	int ret = 0;
	int phy_reg_read_status, mii_status;

	mac_printf("XGMAC %d: mdio register\n", pdata->mac_idx);

	/* find the phy ID or phy address which is connected to our MAC */
	for (phyadr = 0; phyadr < 32; phyadr++) {

		phy_reg_read_status =
			xgmac_mdio_single_rd(&pdata->ops, 0, phyadr, MII_BMSR,
					     &mii_status);

		if (phy_reg_read_status == 0) {
			if (mii_status != 0x0000 && mii_status != 0xffff) {
				pr_err("Phy detected at"\
				       " ID/ADDR %d\n", phyadr);
				phy_detected = 1;
				break;
			}
		} else if (phy_reg_read_status < 0) {
			pr_err("Error reading the phy register"\
			       " MII_BMSR for phy ID/ADDR %d\n", phyadr);
		}
	}

	if (!phy_detected) {
		mac_printf("XGMAC %d: No phy could be detected\n",
			   pdata->mac_idx);
		return -ENOLINK;
	}

	pdata->phyadr = phyadr;
	pdata->bus_id = 0x1;

	dump_phy_registers(pdev);

	new_bus = mdiobus_alloc();

	if (new_bus == NULL) {
		mac_printf("XGMAC %d: Unable to allocate mdio bus\n",
			   pdata->mac_idx);
		return -ENOMEM;
	}

	new_bus->name = "xgmac_phy";
	new_bus->read = xgmac_mdio_read;
	new_bus->write = xgmac_mdio_write;
	new_bus->reset = xgmac_mdio_reset;
	snprintf(new_bus->id, MII_BUS_ID_SIZE, "%s-%x", new_bus->name,
		 pdata->bus_id);
	new_bus->priv = pdev;
	new_bus->phy_mask = 0;
	new_bus->parent = pdata->dev;
	ret = mdiobus_register(new_bus);

	if (ret != 0) {
		pr_err("%s: Cannot register as MDIO bus\n",
		       new_bus->name);
		mdiobus_free(new_bus);
		return ret;
	}

	pdata->mii = new_bus;

	mac_printf("XGMAC %d: MDIO register Successfull\n", pdata->mac_idx);

	return ret;
}

/* API to unregister mdio.
 */
void xgmac_mdio_unregister(void *pdev)
{
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);

	mac_printf("XGMAC %d: mdio unregister\n", pdata->mac_idx);

	if (pdata->phydev) {
		phy_stop(pdata->phydev);
		phy_disconnect(pdata->phydev);
		pdata->phydev = NULL;
	}

	mdiobus_unregister(pdata->mii);
	pdata->mii->priv = NULL;
	mdiobus_free(pdata->mii);
	pdata->mii = NULL;

	mac_printf("XGMAC %d: mdio_unregister Successfull\n", pdata->mac_idx);
}

static void dump_phy_registers(void *pdev)
{
	u32 phydata = 0;
	struct mac_prv_data *pdata = GET_MAC_PDATA(pdev);

	mac_printf(
		"\n************* PHY Reg dump *************************\n");
	xgmac_mdio_single_rd(pdev, 0, pdata->phyadr, MII_BMCR, &phydata);
	mac_printf(
		"Phy Control Reg(Basic Mode Control Reg) (%#x) = %#x\n",
		MII_BMCR, phydata);

	xgmac_mdio_single_rd(pdev, 0, pdata->phyadr, MII_BMSR, &phydata);
	mac_printf("Phy Status Reg(Basic Mode Status Reg) (%#x) = %#x\n",
		   MII_BMSR, phydata);

	xgmac_mdio_single_rd(pdev, 0, pdata->phyadr, MII_PHYSID1, &phydata);
	mac_printf("Phy Id (PHYS ID 1) (%#x)= %#x\n", MII_PHYSID1,
		   phydata);

	xgmac_mdio_single_rd(pdev, 0, pdata->phyadr, MII_PHYSID2,
			     &phydata);
	mac_printf("Phy Id (PHYS ID 2) (%#x)= %#x\n", MII_PHYSID2,
		   phydata);

	xgmac_mdio_single_rd(pdev, 0, pdata->phyadr, MII_ADVERTISE,
			     &phydata);
	mac_printf("Auto-nego Adv (Advertisement Control Reg)"\
		   " (%#x) = %#x\n", MII_ADVERTISE, phydata);

	/* read Phy Control Reg */
	xgmac_mdio_single_rd(pdev, 0, pdata->phyadr, MII_LPA,
			     &phydata);
	mac_printf("Auto-nego Lap (Link Partner Ability Reg)"\
		   " (%#x)= %#x\n", MII_LPA, phydata);

	xgmac_mdio_single_rd(pdev, 0, pdata->phyadr, MII_EXPANSION,
			     &phydata);
	mac_printf("Auto-nego Exp (Extension Reg)"\
		   "(%#x) = %#x\n", MII_EXPANSION, phydata);

	xgmac_mdio_single_rd(pdev, 0, pdata->phyadr, MII_ESTATUS,
			     &phydata);
	mac_printf("Extended Status Reg (%#x) = %#x\n", MII_ESTATUS,
		   phydata);

	xgmac_mdio_single_rd(pdev, 0, pdata->phyadr, MII_CTRL1000,
			     &phydata);
	mac_printf("1000 Ctl Reg (1000BASE-T Control Reg)"\
		   "(%#x) = %#x\n", MII_CTRL1000, phydata);

	xgmac_mdio_single_rd(pdev, 0, pdata->phyadr, MII_STAT1000, &phydata);
	mac_printf("1000 Sts Reg (1000BASE-T Status)(%#x) = %#x\n",
		   MII_STAT1000, phydata);

	mac_printf(
		"\n****************************************************\n");
}
#endif

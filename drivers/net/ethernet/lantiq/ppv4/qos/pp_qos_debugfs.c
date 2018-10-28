/*
 * GPL LICENSE SUMMARY
 *
 *  Copyright(c) 2017 Intel Corporation.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of version 2 of the GNU General Public License as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *  The full GNU General Public License is included in this distribution
 *  in the file called LICENSE.GPL.
 *
 *  Contact Information:
 *  Intel Corporation
 *  2200 Mission College Blvd.
 *  Santa Clara, CA  97052
 */
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include "pp_qos_common.h"
#include "pp_qos_utils.h"
#include "pp_qos_fw.h"
#include "pp_qos_kernel.h"

/* TODO is this the common way for driver's not device specific data */
static struct {
	struct dentry *dir;
} dbg_data = {NULL, };

#define PP_QOS_DEBUGFS_DIR "ppv4_qos"
#define PP_QOS_DBG_MAX_BUF	(1024)
#define PP_QOS_DBG_MAX_INPUT	(64)

static ssize_t add_shared_bwl_group(struct file *file, const char __user *buf,
				size_t count, loff_t *pos)
{
	char *lbuf;
	struct pp_qos_dev *qdev;
	u32 limit;
	struct platform_device *pdev;
	struct pp_qos_drv_data *pdata;
	u32 id = 0;

	pdev = (struct platform_device *)(file->private_data);
	pdata = platform_get_drvdata(pdev);
	qdev = pdata->qdev;

	if (count >= PP_QOS_DBG_MAX_INPUT)
		return count;

	lbuf = kzalloc(count, GFP_KERNEL);

	if (copy_from_user(lbuf, buf, count))
		goto add_shared_bwl_group_done;

	lbuf[count-1] = '\0';

	if (sscanf(lbuf, "%u", &limit) != 1) {
		pr_err("sscanf err\n");
		goto add_shared_bwl_group_done;
	}

	pp_qos_shared_limit_group_add(qdev, limit, &id);

	dev_info(&pdev->dev, "id %u, limit %u\n", id, limit);

	add_shared_bwl_group_done:
	kfree(lbuf);
	return count;
}

static ssize_t add_shared_bwl_group_help(struct file *file,
			char __user *user_buf, size_t count, loff_t *ppos)
{
	char *buff;
	u32  len = 0;
	ssize_t ret = 0;

	buff = kmalloc(PP_QOS_DBG_MAX_BUF, GFP_KERNEL);
	if (!buff)
		return -ENOMEM;

	len = scnprintf(buff, PP_QOS_DBG_MAX_BUF, "<limit>\n");
	ret = simple_read_from_buffer(user_buf, count, ppos, buff, len);
	kfree(buff);

	return ret;
}

static ssize_t remove_shared_bwl_group(struct file *file,
			const char __user *buf, size_t count, loff_t *pos)
{
	char *lbuf;
	struct pp_qos_dev *qdev;
	u32 id = 0;
	struct platform_device *pdev;
	struct pp_qos_drv_data *pdata;

	pdev = (struct platform_device *)(file->private_data);
	pdata = platform_get_drvdata(pdev);
	qdev = pdata->qdev;

	if (count >= PP_QOS_DBG_MAX_INPUT)
		return count;

	lbuf = kzalloc(count, GFP_KERNEL);

	if (copy_from_user(lbuf, buf, count))
		goto remove_shared_bwl_group_done;

	lbuf[count-1] = '\0';

	if (sscanf(lbuf, "%u", &id) != 1) {
		pr_err("sscanf err\n");
		goto remove_shared_bwl_group_done;
	}

	pp_qos_shared_limit_group_remove(qdev, id);

	dev_info(&pdev->dev, "id %u\n", id);

remove_shared_bwl_group_done:
	kfree(lbuf);
	return count;
}

static ssize_t remove_shared_bwl_group_help(struct file *file,
			char __user *user_buf, size_t count, loff_t *ppos)
{
	char *buff;
	u32 len = 0;
	ssize_t ret = 0;

	buff = kmalloc(PP_QOS_DBG_MAX_BUF, GFP_KERNEL);
	if (!buff)
		return -ENOMEM;

	len = scnprintf(buff, PP_QOS_DBG_MAX_BUF, "<bwl group id to remove>\n");
	ret = simple_read_from_buffer(user_buf, count, ppos, buff, len);
	kfree(buff);

	return ret;
}

static const struct file_operations debug_add_shared_bwl_group_fops = {
	.open    = simple_open,
	.read    = add_shared_bwl_group_help,
	.write   = add_shared_bwl_group,
	.llseek  = default_llseek,
};

static const struct file_operations debug_remove_shared_bwl_group_fops = {
	.open    = simple_open,
	.read    = remove_shared_bwl_group_help,
	.write   = remove_shared_bwl_group,
	.llseek  = default_llseek,
};

static int pp_qos_dbg_node_show(struct seq_file *s, void *unused)
{
	struct platform_device *pdev;
	struct pp_qos_dev *qdev;
	struct pp_qos_drv_data *pdata;
	const char *typename;
	unsigned int phy;
	unsigned int id;
	unsigned int i;
	static const char *const types[] = {"Port", "Sched", "Queue",
		"Unknown"};
	static const char *const yesno[] = {"No", "Yes"};
	int rc;
	struct pp_qos_node_info info = {0, };

	pdev = s->private;
	dev_info(&pdev->dev, "node_show called\n");
	if (pdev) {
		pdata = platform_get_drvdata(pdev);
		qdev = pdata->qdev;
		if (!qos_device_ready(qdev)) {
			seq_puts(s, "Device is not ready !!!!\n");
			return 0;
		}

		id = pdata->dbg.node;
		phy = get_phy_from_id(qdev->mapping, id);
		if (!QOS_PHY_VALID(phy)) {
			seq_printf(s, "Invalid id %u\n", id);
			return 0;
		}
		rc = pp_qos_get_node_info(qdev, id, &info);
		if (rc) {
			seq_printf(s, "Could not get info for node %u!!!!\n",
					id);
			return 0;
		}

		if (info.type >=  PPV4_QOS_NODE_TYPE_PORT &&
				info.type <= PPV4_QOS_NODE_TYPE_QUEUE)
			typename = types[info.type];
		else
			typename = types[3];

		seq_printf(s, "%u(%u) - %s: internal node(%s)\n",
				id, phy,
				typename,
				yesno[!!info.is_internal]);

		if (info.preds[0].phy != PPV4_QOS_INVALID) {
			seq_printf(s, "%u(%u)", id, phy);
			for (i = 0; i < 6; ++i) {
				if (info.preds[i].phy == PPV4_QOS_INVALID)
					break;
				seq_printf(s, " ==> %u(%u)",
						info.preds[i].id,
						info.preds[i].phy);
			}
			seq_puts(s, "\n");
		}

		if (info.children[0].phy != PPV4_QOS_INVALID) {
			for (i = 0; i < 8; ++i) {
				if (info.children[i].phy == PPV4_QOS_INVALID)
					break;
				seq_printf(s, "%u(%u) ",
						info.children[i].id,
						info.children[i].phy);
			}
			seq_puts(s, "\n");
		}

		if (info.type == PPV4_QOS_NODE_TYPE_QUEUE)  {
			seq_printf(s, "Physical queue: %u\n",
					info.queue_physical_id);
			seq_printf(s, "Port: %u\n", info.port);
		}

		seq_printf(s, "Bandwidth: %u Kbps\n", info.bw_limit);
	}
	return 0;
}

static int pp_qos_dbg_node_open(struct inode *inode, struct file *file)
{
	return single_open(file, pp_qos_dbg_node_show, inode->i_private);
}

static const struct file_operations debug_node_fops = {
	.open = pp_qos_dbg_node_open,
	.read = seq_read,
	.release = single_release,
};

static int pp_qos_dbg_stat_show(struct seq_file *s, void *unused)
{
	struct platform_device *pdev;
	struct pp_qos_dev *qdev;
	struct pp_qos_drv_data *pdata;
	struct pp_qos_queue_stat qstat;
	struct pp_qos_port_stat pstat;
	struct qos_node *node;
	unsigned int phy;
	unsigned int id;

	pdev = s->private;
	dev_info(&pdev->dev, "node_show called\n");
	if (pdev) {
		pdata = platform_get_drvdata(pdev);
		qdev = pdata->qdev;
		if (!qos_device_ready(qdev)) {
			seq_puts(s, "Device is not ready !!!!\n");
			return 0;
		}

		id = pdata->dbg.node;
		phy = get_phy_from_id(qdev->mapping, id);
		if (!QOS_PHY_VALID(phy)) {
			seq_printf(s, "Invalid id %u\n", id);
			return 0;
		}

		node = get_node_from_phy(qdev->nodes, phy);
		if (node_used(node)) {
			seq_printf(s, "%u(%u) - ", id, phy);
			if (node_queue(node)) {
				seq_puts(s, "Queue\n");
				memset(&qstat, 0, sizeof(qstat));
				if (pp_qos_queue_stat_get(qdev, id, &qstat)
						== 0) {
					seq_printf(s, "queue_packets_occupancy:%u\n",
						qstat.queue_packets_occupancy);
					seq_printf(s, "queue_bytes_occupancy:%u\n",
						qstat.queue_bytes_occupancy);
					seq_printf(s, "total_packets_accepted:%u\n",
						qstat.total_packets_accepted);
					seq_printf(s, "total_packets_dropped:%u\n",
						qstat.total_packets_dropped);
					seq_printf(
						s,
						"total_packets_red_dropped:%u\n",
						qstat.total_packets_red_dropped
						);
					seq_printf(s, "total_bytes_accepted:%llu\n",
						qstat.total_bytes_accepted);
					seq_printf(s, "total_bytes_dropped:%llu\n",
						qstat.total_bytes_dropped);
				} else {
					seq_puts(s, "Could not obtained statistics\n");
				}
			} else if (node_port(node)) {
				seq_puts(s, "Port\n");
				memset(&pstat, 0, sizeof(pstat));
				if (pp_qos_port_stat_get(qdev, id, &pstat)
						== 0) {
					seq_printf(
						s,
						"total_green_bytes in port's queues:%u\n",
						pstat.total_green_bytes);
					seq_printf(
						s,
						"total_yellow_bytes in port's queues:%u\n",
						pstat.total_yellow_bytes);
				} else {
					seq_puts(s, "Could not obtained statistics\n");
				}
			} else {
					seq_puts(s, "Node is not a queue or port, no statistics\n");
			}
		} else {
			seq_printf(s, "Node %u is unused\n", id);
		}
	}
	return 0;
}

static int pp_qos_dbg_stat_open(struct inode *inode, struct file *file)
{
	return single_open(file, pp_qos_dbg_stat_show, inode->i_private);
}

static const struct file_operations debug_stat_fops = {
	.open = pp_qos_dbg_stat_open,
	.read = seq_read,
	.release = single_release,
};

static int pp_qos_dbg_gen_show(struct seq_file *s, void *unused)
{
	unsigned int i;
	struct platform_device *pdev;
	struct pp_qos_drv_data *pdata;
	const struct qos_node *node;
	struct pp_qos_dev *qdev;
	unsigned int ports;
	unsigned int queues;
	unsigned int scheds;
	unsigned int internals;
	unsigned int used;
	unsigned int major;
	unsigned int minor;
	unsigned int build;

	pdev = s->private;
	if (pdev) {
		pdata = platform_get_drvdata(pdev);
		qdev = pdata->qdev;
		if (qos_device_ready(qdev)) {
			ports = 0;
			scheds = 0;
			queues = 0;
			internals = 0;
			used = 0;
			node = get_node_from_phy(qdev->nodes, 0);
			for (i = 0; i < NUM_OF_NODES; ++i) {
				if (node_used(node)) {
					++used;
					if (node_port(node))
						++ports;
					else if (node_queue(node))
						++queues;
					else if (node_internal(node))
						++internals;
					else if (node_sched(node))
						++scheds;
				}
				++node;
			}

			seq_printf(s, "Driver version: %s\n", PPV4_QOS_DRV_VER);
			if (pp_qos_get_fw_version(
						qdev,
						&major,
						&minor,
						&build) == 0)
				seq_printf(s, "FW version:\tmajor(%u) minor(%u) build(%u)\n",
						major, minor, build);
			else
				seq_puts(s, "Could not obtain FW version\n");

			seq_printf(s, "Used nodes:\t%u\nPorts:\t\t%u\nScheds:\t\t%u\nQueues:\t\t%u\nInternals:\t%u\n",
					used, ports, scheds, queues, internals);
		} else {
			seq_puts(s, "Device is not ready !!!!\n");
		}

	} else {
		pr_err("Error, platform device was not found\n");
	}

	return 0;
}

#define NUM_QUEUES_ON_QUERY (32U)
#define NUM_OF_TRIES (20U)
struct queue_stat_info {
	uint32_t qid;
	struct queue_stats_s qstat;
};
static int pp_qos_dbg_qstat_show(struct seq_file *s, void *unused)
{
	unsigned int i;
	struct platform_device *pdev;
	struct pp_qos_drv_data *pdata;
	struct pp_qos_dev *qdev;
	struct queue_stat_info *stat;
	unsigned int tries;
	uint32_t *dst;
	unsigned int j;
	uint32_t val;
	uint32_t num;
	volatile uint32_t *pos;

	pdev = s->private;
	if (!pdev) {
		seq_puts(s, "Error, platform device was not found\n");
		return 0;
	}

	pdata = platform_get_drvdata(pdev);
	qdev = pdata->qdev;

	if (!qos_device_ready(qdev)) {
		seq_puts(s, "Device is not ready !!!!\n");
		return 0;
	}
	seq_puts(s, "Queue\t\tQocc(p)\t\tAccept(p)\tDrop(p)\t\tRed dropped(p)\n");
	dst = (uint32_t *)(qdev->fwcom.cmdbuf);
	*dst++ = qos_u32_to_uc(
			UC_QOS_COMMAND_GET_ACTIVE_QUEUES_STATS);
	pos = dst;
	*dst++ = qos_u32_to_uc(UC_CMD_FLAG_IMMEDIATE);
	*dst++ = qos_u32_to_uc(3);

	for (i = 0; i < NUM_OF_QUEUES; i += NUM_QUEUES_ON_QUERY) {
		*pos = qos_u32_to_uc(UC_CMD_FLAG_IMMEDIATE);
		dst = (uint32_t *)(qdev->fwcom.cmdbuf) + 3;
		*dst++ = qos_u32_to_uc(i);
		*dst++ = qos_u32_to_uc(i + NUM_QUEUES_ON_QUERY - 1);
		*dst++ = qos_u32_to_uc(qdev->hwconf.fw_stat);
		signal_uc(qdev);
		val = qos_u32_from_uc(*pos);
		tries = 0;
		while ((
				val &
				(UC_CMD_FLAG_UC_DONE |
				 UC_CMD_FLAG_UC_ERROR))
				== 0) {
			qos_sleep(10);
			tries++;
			if (tries == NUM_OF_TRIES) {
				seq_puts(s, "firmware not responding !!!!\n");
				return 0;
			}
			val = qos_u32_from_uc(*pos);
		}
		if (val & UC_CMD_FLAG_UC_ERROR) {
			seq_puts(s, "firmware signaled error !!!!\n");
			return 0;
		}
		stat = (struct queue_stat_info *)(qdev->stat + 4);
		num =   *((uint32_t *)(qdev->stat));
		for (j = 0; j < num; ++j) {
			seq_printf(s, "%u\t\t%u\t\t%u\t\t%u\t\t%u\n",
					stat->qid,
					stat->qstat.queue_size_entries,
					stat->qstat.total_accepts,
					stat->qstat.total_drops,
					stat->qstat.total_red_dropped
				  );
			++stat;
		}
	}
	return 0;
}

static int pp_qos_dbg_pstat_show(struct seq_file *s, void *unused)
{
	unsigned int i;
	struct platform_device *pdev;
	struct pp_qos_drv_data *pdata;
	struct pp_qos_dev *qdev;
	struct pp_qos_port_stat statp;

	pdev = s->private;
	if (!pdev) {
		seq_puts(s, "Error, platform device was not found\n");
		return 0;
	}
	pdata = platform_get_drvdata(pdev);
	qdev = pdata->qdev;
	if (!qos_device_ready(qdev)) {
		seq_puts(s, "Device is not ready !!!!\n");
		return 0;
	}

	seq_puts(s, "Port\t\tGreen Bytes\tYellow Bytes\n");
	memset(&statp, 0, sizeof(statp));
	for (i = 0; i <= qdev->max_port; ++i) {
		create_get_port_stats_cmd(
				qdev,
				i,
				qdev->hwconf.fw_stat,
				&statp);
		transmit_cmds(qdev);
		if (statp.total_green_bytes || statp.total_yellow_bytes)
			seq_printf(s, "%u\t\t%u\t\t%u\t\t\n",
					i,
					statp.total_green_bytes,
					statp.total_yellow_bytes);
	}
	return 0;
}

static int pp_qos_dbg_gen_open(struct inode *inode, struct file *file)
{
	return single_open(file, pp_qos_dbg_gen_show, inode->i_private);
}

static int pp_qos_dbg_qstat_open(struct inode *inode, struct file *file)
{
	return single_open(file, pp_qos_dbg_qstat_show, inode->i_private);
}

static int pp_qos_dbg_pstat_open(struct inode *inode, struct file *file)
{
	return single_open(file, pp_qos_dbg_pstat_show, inode->i_private);
}

static const struct file_operations debug_gen_fops = {
	.open = pp_qos_dbg_gen_open,
	.read = seq_read,
	.release = single_release,
};

static const struct file_operations debug_qstat_fops = {
	.open = pp_qos_dbg_qstat_open,
	.read = seq_read,
	.release = single_release,
};

static const struct file_operations debug_pstat_fops = {
	.open = pp_qos_dbg_pstat_open,
	.read = seq_read,
	.release = single_release,
};

static int dbg_cmd_open(struct inode *inode, struct file *filep)
{
	filep->private_data = inode->i_private;
	return 0;
}

#define MAX_CMD_LEN 0x200
static ssize_t dbg_cmd_write(struct file *fp, const char __user *user_buffer,
			     size_t cnt, loff_t *pos)
{
	int rc;
	int i;
	int j;
	uint8_t cmd[MAX_CMD_LEN];
	struct platform_device *pdev;
	struct pp_qos_drv_data *pdata;
	struct pp_qos_dev *qdev;
	uint32_t *dst;
	uint32_t *src;
	unsigned long res;

	pdev = (struct platform_device *)(fp->private_data);
	pdata = platform_get_drvdata(pdev);
	qdev = pdata->qdev;

	pr_info("qos drv address is %p\n", qdev);

	if (cnt > MAX_CMD_LEN) {
		dev_err(&pdev->dev, "Illegal length %zu\n", cnt);
		return -EINVAL;
	}

	rc =  simple_write_to_buffer(cmd, MAX_CMD_LEN, pos, user_buffer, cnt);
	if (rc < 0) {
		dev_err(&pdev->dev, "Write failed with %d\n", rc);
		return rc;
	}

	dst = (uint32_t *)(qdev->fwcom.cmdbuf);
	src = (uint32_t *)cmd;
	dev_info(&pdev->dev, "Writing %d bytes into\n", rc);
	j = 0;
	for (i = 0; i < rc; ++i) {
		if (cmd[i] == '\n') {
			cmd[i] = 0;
			kstrtoul(cmd + j, 0, &res);
			*dst++ = qos_u32_to_uc(res);
			dev_info(&pdev->dev, "Wrote 0x%08lX\n", res);
			j = i + 1;
		}
	}
	signal_uc(qdev);

	return rc;
}

static const struct file_operations debug_cmd_fops = {
	.open = dbg_cmd_open,
	.write = dbg_cmd_write,
};

static void swap_msg(char *msg)
{
	unsigned int i;
	uint32_t *cur;

	cur = (uint32_t *)msg;

	for (i = 0; i < 32; ++i)
		cur[i] = le32_to_cpu(cur[i]);
}

static void print_fw_log(struct platform_device *pdev)
{
	char		msg[128];
	unsigned int	num;
	unsigned int    i;
	uint32_t	*addr;
	uint32_t	read;
	char		*cur;
	struct device	*dev;
	struct pp_qos_drv_data *pdata;

	pdata = platform_get_drvdata(pdev);
	addr = (uint32_t *)(pdata->dbg.fw_logger_addr);
	num = qos_u32_from_uc(*addr);
	read = qos_u32_from_uc(addr[1]);
	dev = &pdev->dev;
	cur = (char *)(pdata->dbg.fw_logger_addr + 8);

	dev_info(dev, "addr is 0x%08X num of messages is %u, read index is %u",
		 (unsigned int)(uintptr_t)cur,
		 num,
		 read);

	for (i = read; i < num; ++i) {
		memcpy((char *)msg, (char *)(cur + 128 * i), 128);
		swap_msg(msg);
		msg[127] = '\0';
		dev_info(dev, "[ARC]: %s\n", msg);
	}

	addr[1] = num;
}

static int phy2id_get(void *data, u64 *val)
{
	uint16_t id;
	struct platform_device *pdev;
	struct pp_qos_drv_data *pdata;
	struct pp_qos_dev *qdev;

	pdev = data;
	pdata = platform_get_drvdata(pdev);
	qdev = pdata->qdev;
	if (!qdev->initialized) {
		dev_err(&pdev->dev, "Device is not initialized\n");
		id =  QOS_INVALID_ID;
		goto out;
	}

	id = get_id_from_phy(qdev->mapping, pdata->dbg.node);
out:
	*val = id;
	return 0;
}

static int fw_logger_get(void *data, u64 *val)
{
	struct platform_device *pdev = data;

	print_fw_log(pdev);

	return 0;
}

static int check_sync_get(void *data, u64 *val)
{
	struct platform_device *pdev = data;
	struct pp_qos_drv_data *pdata;
	struct pp_qos_dev *qdev;

	QOS_LOG_INFO("Checking sync with FW\n");

	pdev = data;
	pdata = platform_get_drvdata(pdev);
	qdev = pdata->qdev;
	if (!qdev->initialized) {
		dev_err(&pdev->dev, "Device is not initialized\n");
		goto out;
	}

	check_sync_with_fw(pdata->qdev);
out:
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(dbg_fw_logger_fops, fw_logger_get, NULL, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(dbg_check_sync_fops, check_sync_get, NULL, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(dbg_phy2id_fops, phy2id_get, NULL, "%llu\n");

#define MAX_DIR_NAME 11

struct debugfs_file {
	const char			*name;
	const struct file_operations	*fops;
	mode_t				mode;
};

static struct debugfs_file qos_debugfs_files[] = {
	{"nodeinfo", &debug_node_fops, 0400},
	{"stat", &debug_stat_fops, 0400},
	{"phy2id", &dbg_phy2id_fops, 0400},
	{"fw_logger", &dbg_fw_logger_fops, 0400},
	{"check_fw_sync", &dbg_check_sync_fops, 0400},
	{"geninfo", &debug_gen_fops, 0400},
	{"qstat", &debug_qstat_fops, 0400},
	{"pstat", &debug_pstat_fops, 0400},
	{"cmd", &debug_cmd_fops, 0200},
	{"add_shared_bwl_group", &debug_add_shared_bwl_group_fops, 0400},
	{"remove_shared_bwl_group", &debug_remove_shared_bwl_group_fops, 0400},
};

int qos_dbg_dev_init(struct platform_device *pdev)
{
	struct pp_qos_drv_data *pdata;
	char   dirname[MAX_DIR_NAME];
	struct dentry *dent;
	int err;
	u32 idx;

	if (!pdev) {
		dev_err(&pdev->dev, "Invalid platform device\n");
		return -ENODEV;
	}

	pdata = platform_get_drvdata(pdev);

	snprintf(dirname, MAX_DIR_NAME, "qos%d", pdata->id);
	dent = debugfs_create_dir(dirname, dbg_data.dir);
	if (IS_ERR_OR_NULL(dent)) {
		err = (int) PTR_ERR(dent);
		dev_err(&pdev->dev, "debugfs_create_dir failed with %d\n", err);
		return err;
	}

	pdata->dbg.dir = dent;

	for (idx = 0 ; idx < ARRAY_SIZE(qos_debugfs_files) ; idx++) {
		dent = debugfs_create_file(qos_debugfs_files[idx].name,
					   qos_debugfs_files[idx].mode,
					   pdata->dbg.dir,
					   pdev,
					   qos_debugfs_files[idx].fops);
		if (unlikely(IS_ERR_OR_NULL(dent))) {
			err = (int) PTR_ERR(dent);
			goto fail;
		}
	}

	dent = debugfs_create_u16("node",
			0600,
			pdata->dbg.dir,
			&pdata->dbg.node);
	if (IS_ERR_OR_NULL(dent)) {
		err = (int) PTR_ERR(dent);
		dev_err(&pdev->dev,
			"debugfs_create_u16 failed creating nodeinfo with %d\n",
			err);
		goto fail;
	}

	return 0;

fail:
	debugfs_remove_recursive(pdata->dbg.dir);
	return err;
}

void qos_dbg_dev_clean(struct platform_device *pdev)
{
	struct pp_qos_drv_data *pdata;

	if (pdev) {
		pdata = platform_get_drvdata(pdev);
		if (pdata)
			debugfs_remove_recursive(pdata->dbg.dir);
	}
}

int qos_dbg_module_init(void)
{
	int rc;
	struct dentry *dir;

	dir = debugfs_create_dir(PP_QOS_DEBUGFS_DIR, NULL);
	if (IS_ERR_OR_NULL(dir)) {
		rc = (int) PTR_ERR(dir);
		pr_err("debugfs_create_dir failed with %d\n", rc);
		return rc;
	}
	dbg_data.dir = dir;
	return 0;
}

void qos_dbg_module_clean(void)
{
	debugfs_remove_recursive(dbg_data.dir);
}

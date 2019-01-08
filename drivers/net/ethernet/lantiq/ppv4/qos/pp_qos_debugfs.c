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
#include <linux/string.h>
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

	lbuf[count - 1] = '\0';

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
					 char __user *user_buf,
					 size_t count,
					 loff_t *ppos)
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

#define DBG_MAX_PROPS	(32)

struct dbg_prop {
	char		field[32];
	char		desc[128];
	unsigned int	*dest;
};

struct dbg_props_cbs {
	int (*first_prop_cb)(struct pp_qos_dev *qdev,
			     char *field,
			     unsigned int val,
			     void *user_data);

	int (*done_props_cb)(struct pp_qos_dev *qdev,
			     unsigned int val,
			     void *user_data);
};

static ssize_t qos_dbg_props(struct file *fp,
			     const char __user *user_buffer,
			     size_t cnt,
			     loff_t *pos,
			     struct dbg_props_cbs *cbs,
			     struct dbg_prop props[],
			     u16 num_props,
			     void *user_data)
{
	int rc;
	unsigned int first_prop = 1;
	uint8_t cmd[PP_QOS_DBG_MAX_INPUT];
	uint8_t field[PP_QOS_DBG_MAX_INPUT];
	struct platform_device *pdev;
	struct pp_qos_drv_data *pdata;
	struct pp_qos_dev *qdev;
	unsigned long res;
	unsigned int id = PP_QOS_INVALID_ID;
	char *tok;
	char *ptr;
	char *pval;
	u16 ind;
	u16 num_changed = 0;

	pdev = (struct platform_device *)(fp->private_data);
	pdata = platform_get_drvdata(pdev);
	qdev = pdata->qdev;

	if (cnt > PP_QOS_DBG_MAX_INPUT) {
		dev_err(&pdev->dev, "Illegal length %zu\n", cnt);
		return -EINVAL;
	}

	rc =  simple_write_to_buffer(cmd, PP_QOS_DBG_MAX_INPUT, pos,
				     user_buffer, cnt);
	if (rc < 0) {
		dev_err(&pdev->dev, "Write failed with %d\n", rc);
		return rc;
	}

	dev_info(&pdev->dev, "received %d bytes\n", rc);
	cmd[rc] = '\0';
	dev_info(&pdev->dev, "cmd->%s\n", cmd);
	ptr = (char *)cmd;

	while ((tok = strsep(&ptr, " \t\n\r")) != NULL) {
		if (tok[0] == '\0')
			continue;

		strcpy(field, tok);
		pval = strchr(field, '=');
		if (!pval) {
			dev_err(&pdev->dev, "Wrong format for prop %s\n", tok);
			return rc;
		}

		*pval = '\0';
		pval++;

		kstrtoul(pval, 0, &res);

		if (first_prop) {
			first_prop = 0;
			id = res;
			if (cbs && cbs->first_prop_cb &&
			    cbs->first_prop_cb(qdev, field, res, user_data)) {
				dev_err(&pdev->dev, "first_prop_cb failed\n");
				return rc;
			}
		}

		for (ind = 0; ind < num_props ; ind++) {
			if (!strncmp(field, props[ind].field,
				strlen(props[ind].field))) {
				*(props[ind].dest) = res;
				num_changed++;
				break;
			}
		}

		if (ind == num_props)
			dev_err(&pdev->dev, "Not supported field %s", field);
	}

	if (id != PP_QOS_INVALID_ID) {
		/* If only logical id was set, print current configuration */
		if (num_changed == 1) {
			pr_info("Current configuration:\n");

			for (ind = 0; ind < num_props ; ind++) {
				pr_info("%-30s%u\n",
					props[ind].field, *props[ind].dest);
			}

			return rc;
		}

		if (cbs && cbs->done_props_cb) {
			if (cbs->done_props_cb(qdev, id, user_data)) {
				dev_err(&pdev->dev, "done_props_cb failed\n");
				return rc;
			}
		}
	}

	return rc;
}

static ssize_t qos_dbg_props_help(struct file *file,
				  char __user *user_buf,
				  size_t count,
				  loff_t *ppos,
				  const char *name,
				  const char *format,
				  struct dbg_prop props[],
				  u16 num_props)
{
	char	*buff;
	u32	len = 0;
	u16	ind;
	ssize_t	ret = 0;
	u16	size = sizeof(props[0]) * num_props + 256;

	buff = kmalloc(size, GFP_KERNEL);
	if (!buff)
		return -ENOMEM;

	len += scnprintf(buff + len, size - len, "<---- %s---->\n", name);
	len += scnprintf(buff + len, size - len, "[FORMAT] %s\n", format);
	len += scnprintf(buff + len, size - len,
			 "[FORMAT] If only id is set, operation is get conf\n");
	len += scnprintf(buff + len, size - len, "Supported fields\n");
	len += scnprintf(buff + len, size - len, "================\n");

	for (ind = 0; ind < num_props ; ind++) {
		len += scnprintf(buff + len, size - len, "%-30s%s\n",
				 props[ind].field, props[ind].desc);
	}

	ret = simple_read_from_buffer(user_buf, count, ppos, buff, len);
	kfree(buff);

	return ret;
}

static void dbg_add_prop(struct dbg_prop *props, u16 *pos, u16 size,
		const char *name, const char *desc, unsigned int *dest)
{
	if (*pos >= size) {
		pr_err("pos %d >= size %d", *pos, size);
		return;
	}

	strncpy(props[*pos].field, name, sizeof(props[*pos].field));
	strncpy(props[*pos].desc, desc, sizeof(props[*pos].desc));
	props[*pos].dest = dest;

	(*pos)++;
}

static u16 create_port_props(struct dbg_prop *props, u16 size,
		unsigned int *id, struct pp_qos_port_conf *pconf)
{
	u16 num = 0;

	dbg_add_prop(props, &num, size, "port",
		"Logical id. Must exist as the first property!", id);
	dbg_add_prop(props, &num, size, "bw", "Limit in kbps (80kbps steps)",
		&pconf->common_prop.bandwidth_limit);
	dbg_add_prop(props, &num, size, "shared",
		"Shared bw group: 1-511 (0 for remove group)",
		&pconf->common_prop.shared_bandwidth_group);
	dbg_add_prop(props, &num, size, "arb",
		"Arbitration: 0 - WSP, 1 - WRR",
		&pconf->port_parent_prop.arbitration);
	dbg_add_prop(props, &num, size, "be",
		"Best effort enable: best effort scheduling is enabled",
		&pconf->port_parent_prop.best_effort_enable);
	dbg_add_prop(props, &num, size, "r_size",
		"Ring size", &pconf->ring_size);
	dbg_add_prop(props, &num, size, "pkt_cred",
		"Packet credit: 0 - byte credit, 1 - packet credit",
		&pconf->packet_credit_enable);
	dbg_add_prop(props, &num, size, "cred", "Port credit", &pconf->credit);
	dbg_add_prop(props, &num, size, "dis",
		"Disable port tx", &pconf->disable);

	return num;
}

static int port_first_prop_cb(struct pp_qos_dev *qdev,
			      char *field,
			      unsigned int val,
			      void *user_data)
{
	/* Make sure first property is the port id */
	if (strncmp(field, "port", strlen("port"))) {
		pr_err("First prop (%s) must be port\n", field);
		return -EINVAL;
	}

	if (pp_qos_port_conf_get(qdev, val, user_data) != 0) {
		pr_err("pp_qos_port_conf_get failed (id %u)", val);
		return -EINVAL;
	}

	return 0;
}

static int port_done_props_cb(struct pp_qos_dev *qdev,
			      unsigned int val,
			      void *user_data)
{
	if (pp_qos_port_set(qdev, val, user_data) != 0) {
		pr_err("pp_qos_port_set failed (id %u)", val);
		return -EINVAL;
	}

	return 0;
}

static ssize_t port(struct file *fp,
		    const char __user *user_buffer,
		    size_t cnt,
		    loff_t *pos)
{
	struct pp_qos_port_conf conf;
	struct dbg_props_cbs cbs = {port_first_prop_cb, port_done_props_cb};
	unsigned int id = PP_QOS_INVALID_ID;
	u16 num_props;
	ssize_t ret;
	struct dbg_prop *props;

	props = kmalloc_array(DBG_MAX_PROPS, sizeof(struct dbg_prop),
			      GFP_KERNEL);
	if (!props)
		return -ENOMEM;

	num_props = create_port_props(props, DBG_MAX_PROPS, &id, &conf);

	ret = qos_dbg_props(fp, user_buffer, cnt, pos, &cbs,
			    props, num_props, &conf);
	kfree(props);

	return ret;
}

static ssize_t port_help(struct file *file,
			 char __user *user_buf,
			 size_t count,
			 loff_t *ppos)
{
	unsigned int id = PP_QOS_INVALID_ID;
	struct pp_qos_port_conf conf;
	const char *name = "set port";
	const char *format =
		"echo port=[logical_id] [field]=[value]... > port";
	u16 num_props;
	ssize_t ret;
	struct dbg_prop *props;

	props = kmalloc_array(DBG_MAX_PROPS, sizeof(struct dbg_prop),
			      GFP_KERNEL);
	if (!props)
		return -ENOMEM;

	num_props = create_port_props(props, DBG_MAX_PROPS, &id, &conf);

	ret = qos_dbg_props_help(file, user_buf, count, ppos, name, format,
				 props, num_props);
	kfree(props);

	return ret;
}

static u16 create_sched_props(struct dbg_prop *props, u16 size,
		unsigned int *id, struct pp_qos_sched_conf *pconf)
{
	u16 num = 0;

	dbg_add_prop(props, &num, size, "sched",
		"Logical id. Must exist as the first property!", id);
	dbg_add_prop(props, &num, size, "bw", "Limit in kbps (80kbps steps)",
		&pconf->common_prop.bandwidth_limit);
	dbg_add_prop(props, &num, size, "shared",
		"Shared bw group: 1-511 (0 for remove group)",
		&pconf->common_prop.shared_bandwidth_group);
	dbg_add_prop(props, &num, size, "arb",
		"Arbitration: 0 - WSP, 1 - WRR",
		&pconf->sched_parent_prop.arbitration);
	dbg_add_prop(props, &num, size, "be",
		"Best effort enable: best effort scheduling is enabled",
		&pconf->sched_parent_prop.best_effort_enable);
	dbg_add_prop(props, &num, size, "parent", "logical parent id",
		&pconf->sched_child_prop.parent);
	dbg_add_prop(props, &num, size, "priority",
		"priority (0-7) in WSP", &pconf->sched_child_prop.priority);
	dbg_add_prop(props, &num, size, "bw_share", "percentage from parent",
		&pconf->sched_child_prop.bandwidth_share);

	return num;
}

static int sched_first_prop_cb(struct pp_qos_dev *qdev,
			       char *field,
			       unsigned int val,
			       void *user_data)
{
	/* Make sure first property is the sched id */
	if (strncmp(field, "sched", strlen("sched"))) {
		pr_err("First prop (%s) must be sched\n", field);
		return -EINVAL;
	}

	if (pp_qos_sched_conf_get(qdev, val, user_data) != 0) {
		pr_err("pp_qos_sched_conf_get failed (id %u)", val);
		return -EINVAL;
	}

	return 0;
}

static int sched_done_props_cb(struct pp_qos_dev *qdev,
			       unsigned int val,
			       void *user_data)
{
	if (pp_qos_sched_set(qdev, val, user_data) != 0) {
		pr_err("pp_qos_sched_set failed (id %u)", val);
		return -EINVAL;
	}

	return 0;
}

static ssize_t sched(struct file *fp,
		     const char __user *user_buffer,
		     size_t cnt,
		     loff_t *pos)
{
	struct pp_qos_sched_conf conf;
	struct dbg_props_cbs cbs = {sched_first_prop_cb, sched_done_props_cb};
	unsigned int id = PP_QOS_INVALID_ID;
	u16 num_props;
	ssize_t ret;
	struct dbg_prop *props;

	props = kmalloc_array(DBG_MAX_PROPS, sizeof(struct dbg_prop),
			      GFP_KERNEL);
	if (!props)
		return -ENOMEM;

	num_props = create_sched_props(props, DBG_MAX_PROPS, &id, &conf);

	ret = qos_dbg_props(fp, user_buffer, cnt, pos, &cbs,
			    props, num_props, &conf);
	kfree(props);

	return ret;
}

static ssize_t sched_help(struct file *file,
			  char __user *user_buf,
			  size_t count,
			  loff_t *ppos)
{
	unsigned int id = PP_QOS_INVALID_ID;
	struct pp_qos_sched_conf conf;
	const char *name = "set sched";
	const char *format =
		"echo sched=[logical_id] [field]=[value]... > sched";
	u16 num_props;
	ssize_t ret;
	struct dbg_prop *props;

	props = kmalloc_array(DBG_MAX_PROPS, sizeof(struct dbg_prop),
			      GFP_KERNEL);
	if (!props)
		return -ENOMEM;

	num_props = create_sched_props(props, DBG_MAX_PROPS, &id, &conf);

	ret = qos_dbg_props_help(file, user_buf, count, ppos, name, format,
				 props, num_props);
	kfree(props);

	return ret;
}

static u16 create_queue_props(struct dbg_prop *props, u16 size,
		unsigned int *id, struct pp_qos_queue_conf *pconf)
{
	u16 num = 0;

	dbg_add_prop(props, &num, size, "queue",
		"Logical id. Must exist as the first property!", id);
	dbg_add_prop(props, &num, size, "bw", "Limit in kbps (80kbps steps)",
		&pconf->common_prop.bandwidth_limit);
	dbg_add_prop(props, &num, size, "shared",
		"Shared bw group: 1-511 (0 for remove group)",
		&pconf->common_prop.shared_bandwidth_group);
	dbg_add_prop(props, &num, size, "parent", "logical parent id",
		&pconf->queue_child_prop.parent);
	dbg_add_prop(props, &num, size, "priority",
		"priority (0-7) in WSP", &pconf->queue_child_prop.priority);
	dbg_add_prop(props, &num, size, "bw_share", "percentage from parent",
		&pconf->queue_child_prop.bandwidth_share);
	dbg_add_prop(props, &num, size, "max_burst", "in kbps (4KB steps)",
		&pconf->max_burst);
	dbg_add_prop(props, &num, size, "blocked", "drop enqueued packets",
		&pconf->blocked);
	dbg_add_prop(props, &num, size, "wred_enable", "enable wred drops",
		&pconf->wred_enable);
	dbg_add_prop(props, &num, size, "wred_fixed_drop_prob",
		"fixed prob instead of slope",
		&pconf->wred_fixed_drop_prob_enable);
	dbg_add_prop(props, &num, size, "wred_min_avg_green",
		"Start of the slope area",
		&pconf->queue_wred_min_avg_green);
	dbg_add_prop(props, &num, size, "wred_max_avg_green",
		"End of the slope area",
		&pconf->queue_wred_max_avg_green);
	dbg_add_prop(props, &num, size, "wred_slope_green", "0-90 scale",
		&pconf->queue_wred_slope_green);
	dbg_add_prop(props, &num, size, "wred_fixed_drop_prob_green",
		"fixed drop rate",
		&pconf->queue_wred_fixed_drop_prob_green);
	dbg_add_prop(props, &num, size, "wred_min_avg_yellow",
		"Start of the slope area",
		&pconf->queue_wred_min_avg_yellow);
	dbg_add_prop(props, &num, size, "wred_max_avg_yellow",
		"End of the slope area",
		&pconf->queue_wred_max_avg_yellow);
	dbg_add_prop(props, &num, size, "wred_slope_yellow", "0-90 scale",
		&pconf->queue_wred_slope_yellow);
	dbg_add_prop(props, &num, size, "wred_fixed_drop_prob_yellow",
		"fixed drop rate",
		&pconf->queue_wred_fixed_drop_prob_yellow);
	dbg_add_prop(props, &num, size, "wred_min_guaranteed",
		"guaranteed for this queue",
		&pconf->queue_wred_min_guaranteed);
	dbg_add_prop(props, &num, size, "wred_max_allowed",
		"Max allowed for this queue",
		&pconf->queue_wred_max_allowed);

	return num;
}

static int queue_first_prop_cb(struct pp_qos_dev *qdev,
			       char *field,
			       unsigned int val,
			       void *user_data)
{
	/* Make sure first property is the queue id */
	if (strncmp(field, "queue", strlen("queue"))) {
		pr_err("First prop (%s) must be queue\n", field);
		return -EINVAL;
	}

	if (pp_qos_queue_conf_get(qdev, val, user_data) != 0) {
		pr_err("pp_qos_queue_conf_get failed (id %u)", val);
		return -EINVAL;
	}

	return 0;
}

static int queue_done_props_cb(struct pp_qos_dev *qdev,
			       unsigned int val,
			       void *user_data)
{
	if (pp_qos_queue_set(qdev, val, user_data) != 0) {
		pr_err("pp_qos_queue_set failed (id %u)", val);
		return -EINVAL;
	}

	return 0;
}

static ssize_t queue(struct file *fp,
		     const char __user *user_buffer,
		     size_t cnt,
		     loff_t *pos)
{
	struct pp_qos_queue_conf conf;
	struct dbg_props_cbs cbs = {queue_first_prop_cb, queue_done_props_cb};
	unsigned int id = PP_QOS_INVALID_ID;
	u16 num_props;
	ssize_t ret;
	struct dbg_prop *props;

	props = kmalloc_array(DBG_MAX_PROPS, sizeof(struct dbg_prop),
			      GFP_KERNEL);
	if (!props)
		return -ENOMEM;

	num_props = create_queue_props(props, DBG_MAX_PROPS, &id, &conf);

	ret = qos_dbg_props(fp, user_buffer, cnt, pos, &cbs,
			    props, num_props, &conf);
	kfree(props);

	return ret;
}

static ssize_t queue_help(struct file *file,
			  char __user *user_buf,
			  size_t count,
			  loff_t *ppos)
{
	unsigned int id = PP_QOS_INVALID_ID;
	struct pp_qos_queue_conf conf;
	const char *name = "set queue";
	const char *format =
		"echo queue=[logical_id] [field]=[value]... > queue";
	u16 num_props;
	ssize_t ret;
	struct dbg_prop *props;

	props = kmalloc_array(DBG_MAX_PROPS, sizeof(struct dbg_prop),
			      GFP_KERNEL);
	if (!props)
		return -ENOMEM;

	num_props = create_queue_props(props, DBG_MAX_PROPS, &id, &conf);

	ret = qos_dbg_props_help(file, user_buf, count, ppos, name, format,
				 props, num_props);
	kfree(props);

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

static const struct file_operations debug_port_fops = {
	.open    = simple_open,
	.read    = port_help,
	.write   = port,
	.llseek  = default_llseek,
};

static const struct file_operations debug_sched_fops = {
	.open    = simple_open,
	.read    = sched_help,
	.write   = sched,
	.llseek  = default_llseek,
};

static const struct file_operations debug_queue_fops = {
	.open    = simple_open,
	.read    = queue_help,
	.write   = queue,
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
			seq_printf(s, "Total Res:\t%u\n",
				   qdev->hwconf.wred_total_avail_resources);
			seq_printf(s, "QM ddr start:\t%#x\n",
				   qdev->hwconf.qm_ddr_start);
			seq_printf(s, "QM num pages:\t%u\n",
				   qdev->hwconf.qm_num_pages);
			seq_printf(s, "clock:\t\t%u\n", qdev->hwconf.qos_clock);
			seq_printf(s, "wred p const:\t%u\n",
				   qdev->hwconf.wred_const_p);
			seq_printf(s, "max q size:\t%u\n",
				   qdev->hwconf.wred_max_q_size);
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
				   stat->qstat.total_red_dropped);
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

#define ARB_STR(a)                             \
	((a) == PP_QOS_ARBITRATION_WSP ? "WSP" : \
	 (a) == PP_QOS_ARBITRATION_WRR ? "WRR" :  \
	 (a) == PP_QOS_ARBITRATION_WFQ ? "WFQ" :  \
	 "Unknown")

static void __dbg_dump_subtree(struct pp_qos_dev *qdev,
			       struct qos_node *node,
			       u32 depth,
			       struct seq_file *s)
{
	u32 idx, tab_idx, n = 0;
	u32 child_phy, node_id;
	char tabs_str[PP_QOS_DBG_MAX_INPUT];
	bool last_child;
	struct qos_node *child;

	if (depth > 6) {
		pr_err("Maximum depth of 6 exceeded\n");
		return;
	}

	tabs_str[0] = '\0';
	for (tab_idx = 0 ; tab_idx < depth ; tab_idx++)
		n += snprintf(tabs_str + n, PP_QOS_DBG_MAX_INPUT - n, "|\t");

	for (idx = 0; idx < node->parent_prop.num_of_children ; ++idx) {
		last_child = (idx == (node->parent_prop.num_of_children - 1));
		child_phy = node->parent_prop.first_child_phy + idx;
		node_id = get_id_from_phy(qdev->mapping, child_phy);
		child = get_node_from_phy(qdev->nodes, child_phy);

		if (last_child)
			seq_printf(s, "%s'-- ", tabs_str);
		else
			seq_printf(s, "%s|-- ", tabs_str);

		if (node_sched(child)) {
			seq_printf(s, "Sched-%u(%u)-%s\n",
				   node_id, child_phy,
				   ARB_STR(child->parent_prop.arbitration));
			__dbg_dump_subtree(qdev, child, depth + 1, s);
		} else if (node_queue(child)) {
			seq_printf(s, "Queue-%u(%u)-rlm-%u\n",
				   node_id, child_phy,
				   child->data.queue.rlm);
		}
	}
}

/**
 * @brief dump complete qos tree
 */
static int pp_qos_dbg_tree_show(struct seq_file *s, void *unused)
{
	struct platform_device *pdev;
	struct pp_qos_drv_data *pdata;
	struct pp_qos_dev *qdev;
	struct qos_node *node;
	u32 node_id, node_phy;

	pdev = s->private;

	pr_info("tree_show called\n");
	if (unlikely(!pdev)) {
		seq_puts(s, "pdev Null\n");
		return 0;
	}

	pdata = platform_get_drvdata(pdev);
	qdev = pdata->qdev;

	if (unlikely(!qos_device_ready(qdev))) {
		seq_puts(s, "Device is not ready\n");
		return 0;
	}

	/* Iterate through all port nodes */
	for (node_phy = 0; node_phy < NUM_OF_NODES; ++node_phy) {
		node = get_node_from_phy(qdev->nodes, node_phy);
		node_id = get_id_from_phy(qdev->mapping, node_phy);
		if (node_port(node)) {
			seq_printf(s, "|-- Port-%u(%u)-%s\n",
				   node_id,
				   get_phy_from_id(qdev->mapping, node_id),
				   ARB_STR(node->parent_prop.arbitration));
			__dbg_dump_subtree(qdev, node, 1, s);
		}
	}

	return 0;
}

static int pp_qos_dbg_tree_open(struct inode *inode, struct file *file)
{
	return single_open(file, pp_qos_dbg_tree_show, inode->i_private);
}

static const struct file_operations debug_tree_fops = {
	.open = pp_qos_dbg_tree_open,
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
	u16 id;
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

static int fw_logger_set(void *data, u64 val)
{
	struct platform_device *pdev = data;
	struct pp_qos_drv_data *pdata = platform_get_drvdata(pdev);
	struct pp_qos_dev *qdev = pdata->qdev;

	dev_info(&pdev->dev, "fw_logger_set setting new fw logger level %u\n",
		 (u32)val);

	switch (val) {
	case UC_LOGGER_LEVEL_FATAL:
	case UC_LOGGER_LEVEL_ERROR:
	case UC_LOGGER_LEVEL_WARNING:
	case UC_LOGGER_LEVEL_INFO:
	case UC_LOGGER_LEVEL_DEBUG:
	case UC_LOGGER_LEVEL_DUMP_REGS:
		create_init_logger_cmd(qdev, (int)val);
		break;
	default:
		dev_info(&pdev->dev, "Not supported fw logger level");
		dev_info(&pdev->dev, "Optional levels:\n");
		dev_info(&pdev->dev, "Fatal: %d\n", UC_LOGGER_LEVEL_FATAL);
		dev_info(&pdev->dev, "Error: %d\n", UC_LOGGER_LEVEL_ERROR);
		dev_info(&pdev->dev, "Warning: %d\n", UC_LOGGER_LEVEL_WARNING);
		dev_info(&pdev->dev, "Info: %d\n", UC_LOGGER_LEVEL_INFO);
		dev_info(&pdev->dev, "Debug: %d\n", UC_LOGGER_LEVEL_DEBUG);
		dev_info(&pdev->dev, "Register Dump: %d\n",
			 UC_LOGGER_LEVEL_DUMP_REGS);
		break;
	}

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

DEFINE_SIMPLE_ATTRIBUTE(dbg_fw_logger_fops, fw_logger_get,
			fw_logger_set, "%llu\n");
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
	{"tree", &debug_tree_fops, 0400},
	{"add_shared_bwl_group", &debug_add_shared_bwl_group_fops, 0400},
	{"remove_shared_bwl_group", &debug_remove_shared_bwl_group_fops, 0400},
	{"port", &debug_port_fops, 0400},
	{"sched", &debug_sched_fops, 0400},
	{"queue", &debug_queue_fops, 0400},
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
		err = (int)PTR_ERR(dent);
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
			err = (int)PTR_ERR(dent);
			goto fail;
		}
	}

	dent = debugfs_create_u16("node",
				  0600,
				  pdata->dbg.dir,
				  &pdata->dbg.node);
	if (IS_ERR_OR_NULL(dent)) {
		err = (int)PTR_ERR(dent);
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
		rc = (int)PTR_ERR(dir);
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

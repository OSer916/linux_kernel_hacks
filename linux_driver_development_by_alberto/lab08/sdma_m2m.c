/* ************ LDD4EP: listing8-1: sdma_m2m.c ************ */
/*
 * This code is distributed under Version 2 of the GNU General Public
 * License, which you should have received with the source.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/platform_data/dma-imx.h>
#include <linux/dmaengine.h>
#include <linux/miscdevice.h>

static char *wbuf;
static char *rbuf;
static struct dma_chan *dma_m2m_chan;
static struct completion dma_m2m_ok;

#define SDMA_BUF_SIZE  (1024*63)

static bool dma_m2m_filter(struct dma_chan *chan, void *param)
{
	if (!imx_dma_is_general_purpose(chan))
		return false;
	chan->private = param;
	return true;
}

static int sdma_open(struct inode *inode, struct file *filp)
{
	pr_info("sdma open is called.\n");
	return 0;
}

static int sdma_release(struct inode *inode, struct file *filp)
{
	pr_info("sdma release is called.\n");
	return 0;
}

static void dma_m2m_callback(void *data)
{
	pr_info("%s\n finished DMA transaction" ,__func__);
	complete(&dma_m2m_ok);
	if (*(rbuf) != *(wbuf)) {
		pr_info("buffer copy failed!\n");
		return;
	}
	pr_info("buffer copy passed!\n");
	pr_info("wbuf is %s\n", wbuf);
	pr_info("rbuf is %s\n", rbuf);
}

static ssize_t sdma_write(struct file *filp, const char __user *buf, size_t count,
		loff_t *offset)
{
	struct dma_async_tx_descriptor *dma_m2m_desc;
	struct dma_device *dma_dev;
	dma_addr_t dma_src, dma_dst;

	dma_dev = dma_m2m_chan->device;

	if (copy_from_user(wbuf, buf, count))
		return -EFAULT;

	pr_info("success copy\n");

	pr_info("The wbuf string is %s\n", wbuf);
	pr_info("The rbuf string is %s\n", wbuf);

	dma_src = dma_map_single(dma_dev->dev, wbuf, SDMA_BUF_SIZE, DMA_MEM_TO_MEM);
	dma_dst = dma_map_single(dma_dev->dev, rbuf, SDMA_BUF_SIZE, DMA_MEM_TO_MEM);

	dma_m2m_desc = dma_dev->device_prep_dma_memcpy(dma_m2m_chan, dma_dst, dma_src,
													SDMA_BUF_SIZE, 0);

	init_completion(&dma_m2m_ok);
	dma_m2m_desc->callback = dma_m2m_callback;
	dma_m2m_desc->callback_param = &dma_m2m_ok;
	dmaengine_submit(dma_m2m_desc);
	dma_async_issue_pending(dma_m2m_chan);
	wait_for_completion(&dma_m2m_ok);

	dma_unmap_single(dma_dev->dev, dma_src, SDMA_BUF_SIZE, DMA_MEM_TO_MEM);
	dma_unmap_single(dma_dev->dev, dma_dst, SDMA_BUF_SIZE, DMA_MEM_TO_MEM);

	return count; 
}

struct file_operations dma_fops = {
	open: sdma_open,
	release: sdma_release,
	write: sdma_write,
};

static struct miscdevice dma_miscdevice = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "sdma_test",
	.fops = &dma_fops,
};

static int __init sdma_init_module(void)
{
	int retval;
	dma_cap_mask_t dma_m2m_mask;
	struct imx_dma_data m2m_dma_data = {0};
	struct dma_slave_config dma_m2m_config = {0};

	pr_info("module enter\n");
	retval = misc_register(&dma_miscdevice);
	if (retval)
		return retval; 

	pr_info("mydev: got minor %i\n",dma_miscdevice.minor);

	wbuf = kzalloc(SDMA_BUF_SIZE, GFP_DMA);
	if (!wbuf) {
		pr_err("error allocating wbuf !!!!!!!!!!!\n");
		return -ENOMEM;
	}

	rbuf = kzalloc(SDMA_BUF_SIZE, GFP_DMA);
	if (!rbuf) {
		kfree(wbuf);
		pr_err("error allocating rbuf !!!!!!!!!!!\n");
		return -ENOMEM;
	}

	dma_cap_zero(dma_m2m_mask);
	dma_cap_set(DMA_MEMCPY, dma_m2m_mask);
	m2m_dma_data.peripheral_type = IMX_DMATYPE_MEMORY;
	m2m_dma_data.priority = DMA_PRIO_HIGH;

	dma_m2m_chan = dma_request_channel(dma_m2m_mask, dma_m2m_filter, &m2m_dma_data);
	if (!dma_m2m_chan) {
		pr_err("Error opening the SDMA memory to memory channel\n");
		return -EINVAL;
	}

	dma_m2m_config.direction = DMA_MEM_TO_MEM;
	dma_m2m_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	dmaengine_slave_config(dma_m2m_chan, &dma_m2m_config);

	return 0;
}

static void __exit sdma_cleanup_module(void)
{
	misc_deregister(&dma_miscdevice);
	dma_release_channel(dma_m2m_chan);
	kfree(wbuf);
	kfree(rbuf);
	pr_info("module exit\n");

}

module_init(sdma_init_module);
module_exit(sdma_cleanup_module);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Alberto Liberal <aliberal@arroweurope.com>");
MODULE_DESCRIPTION("This is a SDMA driver that copies memory to memory");

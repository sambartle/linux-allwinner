/*
 * Sun4i Camera Interface  driver
 * Author: raymonxiu
 */
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/videodev2.h>
#include <linux/clk.h>
#include <linux/delay.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20)
#include <linux/freezer.h>
#endif

#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-common.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <media/v4l2-subdev.h>
#include <media/videobuf-dma-contig.h>
#include <linux/moduleparam.h>

#include <mach/gpio_v2.h>
#include <mach/clock.h>
#include <mach/irqs.h>

#include "../include/sun4i_csi_core.h"
#include "../include/sun4i_dev_csi.h"
#include "sun4i_csi_reg.h"

#define CSI_MAJOR_VERSION 1
#define CSI_MINOR_VERSION 0
#define CSI_RELEASE 0
#define CSI_VERSION \
	KERNEL_VERSION(CSI_MAJOR_VERSION, CSI_MINOR_VERSION, CSI_RELEASE)
#define CSI_MODULE_NAME "sun4i_csi1"

//#define USE_DMA_CONTIG

//#define AJUST_DRAM_PRIORITY
#define REGS_pBASE					(0x01C00000)	 	      // register base addr
#define SDRAM_REGS_pBASE    (REGS_pBASE + 0x01000)    // SDRAM Controller

#define NUM_INPUTS 1
#define CSI_OUT_RATE      (24*1000*1000)
#define CSI_ISP_RATE			(100*1000*1000)
#define CSI_MAX_FRAME_MEM (32*1024*1024)
#define TWI_NO		 (1)

#define MIN_WIDTH  (32)
#define MIN_HEIGHT (32)
#define MAX_WIDTH  (4096)
#define MAX_HEIGHT (4096)

static unsigned video_nr = 1;
static unsigned first_flag = 0;

static char ccm[I2C_NAME_SIZE] = "ov7670";
static uint i2c_addr = 0x42;

module_param_string(ccm, ccm, sizeof(ccm), S_IRUGO|S_IWUSR);
module_param(i2c_addr,uint, S_IRUGO|S_IWUSR);


static struct i2c_board_info  dev_sensor[] =  {
	{
		//I2C_BOARD_INFO(ccm, i2c_addr),
		.platform_data	= NULL,
	},
};

//ccm support format
static struct csi_fmt formats[] = {
	{
		.name     		= "planar YUV 422",
		.ccm_fmt			= V4L2_PIX_FMT_YUYV,
		.fourcc   		= V4L2_PIX_FMT_YUV422P,
		.input_fmt		= CSI_YUV422,
		.output_fmt		= CSI_PLANAR_YUV422,
		.depth    		= 16,
		.planes_cnt		= 3,
	},
	{
		.name     		= "planar YUV 420",
		.ccm_fmt			= V4L2_PIX_FMT_YUYV,	
		.fourcc   		= V4L2_PIX_FMT_YUV420,
		.input_fmt		= CSI_YUV422,
		.output_fmt		= CSI_PLANAR_YUV420,
		.depth    		= 12,
		.planes_cnt		= 3,
	},
	{
		.name     		= "planar YUV 422 UV combined",
		.ccm_fmt			= V4L2_PIX_FMT_YUYV,	
		.fourcc   		= V4L2_PIX_FMT_NV16,
		.input_fmt		= CSI_YUV422,
		.output_fmt		= CSI_UV_CB_YUV422,
		.depth    		= 16,
		.planes_cnt		= 2,
	},
	{
		.name     		= "planar YUV 420 UV combined",
		.ccm_fmt			= V4L2_PIX_FMT_YUYV,	
		.fourcc   		= V4L2_PIX_FMT_NV12,
		.input_fmt		= CSI_YUV422,
		.output_fmt		= CSI_UV_CB_YUV420,
		.depth    		= 12,
		.planes_cnt		= 2,
	},
	{
		.name     		= "MB YUV420",
		.ccm_fmt			= V4L2_PIX_FMT_YUYV,	
		.fourcc   		= V4L2_PIX_FMT_HM12,
		.input_fmt		= CSI_YUV422,
		.output_fmt		= CSI_MB_YUV420,
		.depth    		= 12,
		.planes_cnt		= 2,
	},
	{
		.name     		= "RAW Bayer",
		.ccm_fmt			= V4L2_PIX_FMT_SBGGR8,	
		.fourcc   		= V4L2_PIX_FMT_SBGGR8,
		.input_fmt		= CSI_RAW,
		.output_fmt		= CSI_PASS_THROUTH,
		.depth    		= 8,
		.planes_cnt		= 1,
	},
//	{
//		.name     		= "planar RGB242",
//		.ccm_fmt			= V4L2_PIX_FMT_SBGGR8,	
//		.fourcc   		= V4L2_PIX_FMT_RGB32,		//can't find the appropriate format in V4L2 define,use this temporarily
//		.input_fmt		= CSI_BAYER,
//		.output_fmt		= CSI_PLANAR_RGB242,
//		.depth    		= 8,
//		.planes_cnt		= 3,
//	},
	{
		.name     		= "YUV422 YUYV",
		.ccm_fmt			= V4L2_PIX_FMT_YUYV,
		.fourcc   		= V4L2_PIX_FMT_YUYV,
		.input_fmt		= CSI_RAW,
		.output_fmt		= CSI_PASS_THROUTH,
		.depth    		= 16,
		.planes_cnt		= 1,
	},
	{
		.name     		= "YUV422 YVYU",
		.ccm_fmt			= V4L2_PIX_FMT_YVYU,
		.fourcc   		= V4L2_PIX_FMT_YVYU,
		.input_fmt		= CSI_RAW,
		.output_fmt		= CSI_PASS_THROUTH,
		.depth    		= 16,
		.planes_cnt		= 1,
	},
	{
		.name     		= "YUV422 UYVY",
		.ccm_fmt			= V4L2_PIX_FMT_UYVY,
		.fourcc   		= V4L2_PIX_FMT_UYVY,
		.input_fmt		= CSI_RAW,
		.output_fmt		= CSI_PASS_THROUTH,
		.depth    		= 16,
		.planes_cnt		= 1,
	},
	{
		.name     		= "YUV422 VYUY",
		.ccm_fmt			= V4L2_PIX_FMT_VYUY,
		.fourcc   		= V4L2_PIX_FMT_VYUY,
		.input_fmt		= CSI_RAW,
		.output_fmt		= CSI_PASS_THROUTH,
		.depth    		= 16,
		.planes_cnt		= 1,
	},
};

static struct csi_fmt *get_format(struct v4l2_format *f)
{
	struct csi_fmt *fmt;
	unsigned int k;

	for (k = 0; k < ARRAY_SIZE(formats); k++) {
		fmt = &formats[k];
		if (fmt->fourcc == f->fmt.pix.pixelformat) {
			break;
		}
	}

	if (k == ARRAY_SIZE(formats)) {
		return NULL;
	}	

	return &formats[k];
};

static inline void csi_set_addr(struct csi_dev *dev,struct csi_buffer *buffer)
{
	
	struct csi_buffer *buf = buffer;	
	dma_addr_t addr_org;
	
	csi_dbg(3,"buf ptr=%p\n",buf);
		
	addr_org = videobuf_to_dma_contig((struct videobuf_buffer *)buf);


	if(dev->fmt->input_fmt==CSI_RAW){
		dev->csi_buf_addr.y  = addr_org;
		dev->csi_buf_addr.cb = addr_org;
		dev->csi_buf_addr.cr = addr_org;
	
	}else if(dev->fmt->input_fmt==CSI_BAYER){
		//really rare here
		dev->csi_buf_addr.cb = addr_org;//for G channel
		dev->csi_buf_addr.y  = addr_org + dev->width*dev->height*1/2;//for B channel
		dev->csi_buf_addr.cr = addr_org + dev->width*dev->height*3/4;//for R channel

	}else if(dev->fmt->input_fmt==CSI_CCIR656){
	//TODO:

	}else if(dev->fmt->input_fmt==CSI_YUV422){
		
		switch (dev->fmt->output_fmt) {
			case CSI_PLANAR_YUV422:
				dev->csi_buf_addr.y  = addr_org;
				dev->csi_buf_addr.cb = addr_org + dev->width*dev->height;
				dev->csi_buf_addr.cr = addr_org + dev->width*dev->height*3/2;
				break;

			case CSI_PLANAR_YUV420:
				dev->csi_buf_addr.y  = addr_org;
				dev->csi_buf_addr.cb = addr_org + dev->width*dev->height;
				dev->csi_buf_addr.cr = addr_org + dev->width*dev->height*5/4;
				break;
				
			case CSI_UV_CB_YUV422:
			case CSI_UV_CB_YUV420:
			case CSI_MB_YUV422:
			case CSI_MB_YUV420:
				dev->csi_buf_addr.y  = addr_org;
				dev->csi_buf_addr.cb = addr_org + dev->width*dev->height;
				dev->csi_buf_addr.cr = addr_org + dev->width*dev->height;
				break;

			default:
				break;
		}
	}
	
	bsp_csi_set_buffer_address(dev, CSI_BUF_0_A, dev->csi_buf_addr.y);
	bsp_csi_set_buffer_address(dev, CSI_BUF_0_B, dev->csi_buf_addr.y);
	bsp_csi_set_buffer_address(dev, CSI_BUF_1_A, dev->csi_buf_addr.cb);
	bsp_csi_set_buffer_address(dev, CSI_BUF_1_B, dev->csi_buf_addr.cb);
	bsp_csi_set_buffer_address(dev, CSI_BUF_2_A, dev->csi_buf_addr.cr);
	bsp_csi_set_buffer_address(dev, CSI_BUF_2_B, dev->csi_buf_addr.cr);

	csi_dbg(3,"csi_buf_addr_y=%x\n",  dev->csi_buf_addr.y);
	csi_dbg(3,"csi_buf_addr_cb=%x\n", dev->csi_buf_addr.cb);
	csi_dbg(3,"csi_buf_addr_cr=%x\n", dev->csi_buf_addr.cr);
	
}

static int csi_clk_get(struct csi_dev *dev)
{
	int ret;

	dev->csi_ahb_clk=clk_get(NULL, "ahb_csi1");
	if (dev->csi_ahb_clk == NULL) {
       	csi_err("get csi1 ahb clk error!\n");	
		return -1;
    }

	if(dev->ccm_info.mclk==24000000)
	{
		dev->csi_clk_src=clk_get(NULL,"hosc");
		if (dev->csi_clk_src == NULL) {
       	csi_err("get csi1 hosc source clk error!\n");	
			return -1;
    }
  }
  else
  {
		dev->csi_clk_src=clk_get(NULL,"video_pll0");
		if (dev->csi_clk_src == NULL) {
       	csi_err("get csi1 video pll0 source clk error!\n");	
			return -1;
    }
	}  
    
	dev->csi_module_clk=clk_get(NULL,"csi1");
	if(dev->csi_module_clk == NULL) {
       	csi_err("get csi1 module clk error!\n");	
		return -1;
    }
    
	ret = clk_set_parent(dev->csi_module_clk, dev->csi_clk_src);
	if (ret == -1) {
        csi_err(" csi set parent failed \n");
	    return -1;
    }
     
	clk_put(dev->csi_clk_src);
	
	ret = clk_set_rate(dev->csi_module_clk,dev->ccm_info.mclk);
	if (ret == -1) {
        csi_err("set csi1 module clock error\n");
		return -1;
   	}
   
//	dev->csi_isp_src_clk=clk_get(NULL,"video_pll0");
//	if (dev->csi_isp_src_clk == NULL) {
//       	csi_err("get csi_isp source clk error!\n");	
//		return -1;
//    }  
//  
//  dev->csi_isp_clk=clk_get(NULL,"csi_isp");
//	if(dev->csi_isp_clk == NULL) {
//       	csi_err("get csi_isp clk error!\n");	
//		return -1;
//    } 
//	
//	ret = clk_set_parent(dev->csi_isp_clk, dev->csi_isp_src_clk);
//	if (ret == -1) {
//        csi_err(" csi_isp set parent failed \n");
//	    return -1;
//    }
//	
//	clk_put(dev->csi_isp_src_clk);
//	
//  ret = clk_set_rate(dev->csi_isp_clk, CSI_ISP_RATE);
//	if (ret == -1) {
//        csi_err("set csi_isp clock error\n");
//		return -1;
//   	} 	

	dev->csi_dram_clk = clk_get(NULL, "sdram_csi1");
	if (dev->csi_dram_clk == NULL) {
       	csi_err("get csi1 dram clk error!\n");
		return -1;
    }

	return 0;
}

static int csi_clk_out_set(struct csi_dev *dev)
{
	int ret;
	ret = clk_set_rate(dev->csi_module_clk, CSI_OUT_RATE);
	if (ret == -1) {
		csi_err("set csi1 module clock error\n");
		return -1;
	}

	return 0;
}

static void csi_reset_enable(struct csi_dev *dev)
{
	clk_reset(dev->csi_module_clk, 1);
}

static void csi_reset_disable(struct csi_dev *dev)
{
	clk_reset(dev->csi_module_clk, 0);
}

static int csi_clk_enable(struct csi_dev *dev)
{
	clk_enable(dev->csi_ahb_clk);
	clk_enable(dev->csi_module_clk);
	clk_enable(dev->csi_isp_clk);
	clk_enable(dev->csi_dram_clk);
	
	return 0;
}

static int csi_clk_disable(struct csi_dev *dev)
{
	clk_disable(dev->csi_ahb_clk);
	clk_disable(dev->csi_module_clk);
	clk_disable(dev->csi_isp_clk);
	clk_disable(dev->csi_dram_clk);

	return 0;
}

static int csi_clk_release(struct csi_dev *dev)
{
	clk_put(dev->csi_ahb_clk);        
    dev->csi_ahb_clk = NULL;

	clk_put(dev->csi_module_clk);        
    dev->csi_module_clk = NULL;

	clk_put(dev->csi_dram_clk);        
    dev->csi_dram_clk = NULL;

	return 0;
}

static int csi_is_generating(struct csi_dev *dev)
{
	return test_bit(0, &dev->generating);
}

static void csi_start_generating(struct csi_dev *dev)
{
	 set_bit(0, &dev->generating);
	 return;
}	

static void csi_stop_generating(struct csi_dev *dev)
{
	 first_flag = 0;
	 clear_bit(0, &dev->generating);
	 return;
}

static irqreturn_t csi_isr(int irq, void *priv)
{
	struct csi_buffer *buf;	
	struct csi_dev *dev = (struct csi_dev *)priv;
	struct csi_dmaqueue *dma_q = &dev->vidq;
//	__csi_int_status_t * status;
	
	csi_dbg(3,"csi_isr\n");
	
	bsp_csi_int_disable(dev,CSI_INT_FRAME_DONE);//CSI_INT_FRAME_DONE
	
	spin_lock(&dev->slock);
	
	if (first_flag == 0) {
		first_flag=1;
		goto set_next_addr;
	}
	
	if (list_empty(&dma_q->active)) {		
		csi_err("No active queue to serve\n");		
		goto unlock;	
	}
	
	buf = list_entry(dma_q->active.next,struct csi_buffer, vb.queue);
	csi_dbg(3,"buf ptr=%p\n",buf);

	/* Nobody is waiting on this buffer*/	

	if (!waitqueue_active(&buf->vb.done)) {
		csi_dbg(1," Nobody is waiting on this buffer,buf = 0x%p\n",buf);					
	}
	
	list_del(&buf->vb.queue);

	do_gettimeofday(&buf->vb.ts);
	buf->vb.field_count++;

	dev->ms += jiffies_to_msecs(jiffies - dev->jiffies);
	dev->jiffies = jiffies;

	buf->vb.state = VIDEOBUF_DONE;
	wake_up(&buf->vb.done);
	
	//judge if the frame queue has been written to the last
	if (list_empty(&dma_q->active)) {		
		csi_dbg(1,"No more free frame\n");		
		goto unlock;	
	}
	
	if ((&dma_q->active) == dma_q->active.next->next) {
		csi_dbg(1,"No more free frame on next time\n");		
		goto unlock;	
	}
	
	
set_next_addr:	
	buf = list_entry(dma_q->active.next->next,struct csi_buffer, vb.queue);
	csi_set_addr(dev,buf);

unlock:
	spin_unlock(&dev->slock);
	
//	bsp_csi_int_get_status(dev, status);
//	if((status->buf_0_overflow) || (status->buf_1_overflow) || (status->buf_2_overflow))
//	{
//		bsp_csi_int_clear_status(dev,CSI_INT_BUF_0_OVERFLOW);
//		bsp_csi_int_clear_status(dev,CSI_INT_BUF_1_OVERFLOW);
//		bsp_csi_int_clear_status(dev,CSI_INT_BUF_2_OVERFLOW);
//		csi_err("fifo overflow\n");
//	}
//	
//	if((status->hblank_overflow))
//	{
//		bsp_csi_int_clear_status(dev,CSI_INT_HBLANK_OVERFLOW);
//		csi_err("hblank overflow\n");
//	}
	
	bsp_csi_int_clear_status(dev,CSI_INT_FRAME_DONE);//CSI_INT_FRAME_DONE
	bsp_csi_int_enable(dev,CSI_INT_FRAME_DONE);//CSI_INT_FRAME_DONE
	
	return IRQ_HANDLED;
}

/*
 * Videobuf operations
 */
static int buffer_setup(struct videobuf_queue *vq, unsigned int *count, unsigned int *size)
{
	struct csi_dev *dev = vq->priv_data;

	csi_dbg(1,"buffer_setup\n");
	
	if(dev->fmt->input_fmt == CSI_RAW)
	{
		switch(dev->fmt->fourcc) {
			case 	V4L2_PIX_FMT_YUYV:
			case	V4L2_PIX_FMT_YVYU:
			case	V4L2_PIX_FMT_UYVY:
			case	V4L2_PIX_FMT_VYUY:
				*size = dev->width * dev->height * 2;
				break;
			default:
				*size = dev->width * dev->height;
				break;
		}
	}
	else if(dev->fmt->input_fmt == CSI_BAYER)
	{
		*size = dev->width * dev->height;
	}
	else if(dev->fmt->input_fmt == CSI_CCIR656)
	{
		//TODO
	}
	else if(dev->fmt->input_fmt == CSI_YUV422)
	{
		switch (dev->fmt->output_fmt) {
			case 	CSI_PLANAR_YUV422:
			case	CSI_UV_CB_YUV422:
			case 	CSI_MB_YUV422:
				*size = dev->width * dev->height * 2;
				break;
				
			case CSI_PLANAR_YUV420:
			case CSI_UV_CB_YUV420:
			case CSI_MB_YUV420:
				*size = dev->width * dev->height * 3/2;
				break;

			default:
				*size = dev->width * dev->height * 2;
				break;
		}
	}
	else
	{
		*size = dev->width * dev->height * 2;	
	}
	
	dev->frame_size = *size;
	
	if (*count < 3) {
		*count = 3;
		csi_err("buffer count is invalid, set to 3\n");
	} else if(*count > 5) {	
		*count = 5;
		csi_err("buffer count is invalid, set to 5\n");
	}

	while (*size * *count > CSI_MAX_FRAME_MEM) {
		(*count)--;
	}	
	csi_print("%s, buffer count=%d, size=%d\n", __func__,*count, *size);

	return 0;
}

static void free_buffer(struct videobuf_queue *vq, struct csi_buffer *buf)
{
	csi_dbg(1,"%s, state: %i\n", __func__, buf->vb.state);

#ifdef USE_DMA_CONTIG	
	videobuf_dma_contig_free(vq, &buf->vb);
#endif
	
	csi_dbg(1,"free_buffer: freed\n");
	
	buf->vb.state = VIDEOBUF_NEEDS_INIT;
}

static int buffer_prepare(struct videobuf_queue *vq, struct videobuf_buffer *vb,
						  enum v4l2_field field)
{
	struct csi_dev *dev = vq->priv_data;
	struct csi_buffer *buf = container_of(vb, struct csi_buffer, vb);
	int rc;

	csi_dbg(1,"buffer_prepare\n");

	BUG_ON(NULL == dev->fmt);

	if (dev->width  < MIN_WIDTH || dev->width  > MAX_WIDTH ||
	    dev->height < MIN_HEIGHT || dev->height > MAX_HEIGHT) {
		return -EINVAL;
	}
	
	buf->vb.size = dev->frame_size;
		
	if (0 != buf->vb.baddr && buf->vb.bsize < buf->vb.size) {
		return -EINVAL;
	}

	/* These properties only change when queue is idle, see s_fmt */
	buf->fmt       = dev->fmt;
	buf->vb.width  = dev->width;
	buf->vb.height = dev->height;
	buf->vb.field  = field;

	if (VIDEOBUF_NEEDS_INIT == buf->vb.state) {
		rc = videobuf_iolock(vq, &buf->vb, NULL);
		if (rc < 0) {
			goto fail;
		}
	}

	vb->boff= videobuf_to_dma_contig(vb);
	buf->vb.state = VIDEOBUF_PREPARED;

	return 0;

fail:
	free_buffer(vq, buf);
	return rc;
}

static void buffer_queue(struct videobuf_queue *vq, struct videobuf_buffer *vb)
{
	struct csi_dev *dev = vq->priv_data;
	struct csi_buffer *buf = container_of(vb, struct csi_buffer, vb);
	struct csi_dmaqueue *vidq = &dev->vidq;

	csi_dbg(1,"buffer_queue\n");
	buf->vb.state = VIDEOBUF_QUEUED;
	list_add_tail(&buf->vb.queue, &vidq->active);
}

static void buffer_release(struct videobuf_queue *vq,
			   struct videobuf_buffer *vb)
{
	struct csi_buffer *buf  = container_of(vb, struct csi_buffer, vb);

	csi_dbg(1,"buffer_release\n");
	
	free_buffer(vq, buf);
}

static struct videobuf_queue_ops csi_video_qops = {
	.buf_setup    = buffer_setup,
	.buf_prepare  = buffer_prepare,
	.buf_queue    = buffer_queue,
	.buf_release  = buffer_release,
};

/*
 * IOCTL vidioc handling
 */
static int vidioc_querycap(struct file *file, void  *priv,
					struct v4l2_capability *cap)
{
	struct csi_dev *dev = video_drvdata(file);

	strcpy(cap->driver, "sun4i_csi1");
	strcpy(cap->card, "sun4i_csi1");
	strlcpy(cap->bus_info, dev->v4l2_dev.name, sizeof(cap->bus_info));

	cap->version = CSI_VERSION;
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING | \
			    V4L2_CAP_READWRITE;
	return 0;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void  *priv,
					struct v4l2_fmtdesc *f)
{
	struct csi_fmt *fmt;

	csi_dbg(0,"vidioc_enum_fmt_vid_cap\n");

	if (f->index > ARRAY_SIZE(formats)-1) {
		return -EINVAL;
	}	
	fmt = &formats[f->index];

	strlcpy(f->description, fmt->name, sizeof(f->description));
	f->pixelformat = fmt->fourcc;
	return 0;
}

static int vidioc_g_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_format *f)
{
	struct csi_dev *dev = video_drvdata(file);

	f->fmt.pix.width        = dev->width;
	f->fmt.pix.height       = dev->height;
	f->fmt.pix.field        = dev->vb_vidq.field;
	f->fmt.pix.pixelformat  = dev->fmt->fourcc;
	f->fmt.pix.bytesperline = (f->fmt.pix.width * dev->fmt->depth) >> 3;
	f->fmt.pix.sizeimage    = f->fmt.pix.height * f->fmt.pix.bytesperline;

	return 0;
}

static int vidioc_try_fmt_vid_cap(struct file *file, void *priv,
			struct v4l2_format *f)
{
	struct csi_dev *dev = video_drvdata(file);
	struct csi_fmt *csi_fmt;
	struct v4l2_format ccm_fmt;
	int ret = 0;
	
	csi_dbg(0,"vidioc_try_fmt_vid_cap\n");

	/*judge the resolution*/
	if(f->fmt.pix.width > MAX_WIDTH || f->fmt.pix.height > MAX_HEIGHT) {
		csi_err("size is too large,automatically set to maximum!\n");
		f->fmt.pix.width = MAX_WIDTH;
		f->fmt.pix.height = MAX_HEIGHT;
	}

	csi_fmt = get_format(f);
	if (!csi_fmt) {
		csi_err("Fourcc format (0x%08x) invalid.\n",f->fmt.pix.pixelformat);
		return -EINVAL;
	}

	ccm_fmt.fmt.pix.pixelformat = csi_fmt->ccm_fmt;
	ccm_fmt.fmt.pix.width = f->fmt.pix.width;
	ccm_fmt.fmt.pix.height = f->fmt.pix.height;

	ret = v4l2_subdev_call(dev->sd,video,try_fmt,&ccm_fmt);
	if (ret < 0) {
		csi_err("v4l2 sub device try_fmt error!\n");
		return ret;
	}
	
	//info got from module
	f->fmt.pix.width = ccm_fmt.fmt.pix.width;
	f->fmt.pix.height = ccm_fmt.fmt.pix.height;
	f->fmt.pix.bytesperline = ccm_fmt.fmt.pix.bytesperline;
	f->fmt.pix.sizeimage = ccm_fmt.fmt.pix.sizeimage;

	csi_dbg(0,"pix->width=%d\n",f->fmt.pix.width);
	csi_dbg(0,"pix->height=%d\n",f->fmt.pix.height);

	return 0;
}

static int vidioc_s_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_format *f)
{
	struct csi_dev *dev = video_drvdata(file);
	struct videobuf_queue *q = &dev->vb_vidq;
	int ret,width_buf,height_buf,width_len;
	struct v4l2_format ccm_fmt;
	struct csi_fmt *csi_fmt;
	
	csi_dbg(0,"vidioc_s_fmt_vid_cap\n");	
	
	if (csi_is_generating(dev)) {
		csi_err("%s device busy\n", __func__);
		return -EBUSY;
	}
	
	mutex_lock(&q->vb_lock);
	
	ret = vidioc_try_fmt_vid_cap(file, priv, f);
	if (ret < 0) {
		csi_err("try format failed!\n");
		goto out;
	}
		
	csi_fmt = get_format(f);
	if (!csi_fmt) {
		csi_err("Fourcc format (0x%08x) invalid.\n",f->fmt.pix.pixelformat);
		ret	= -EINVAL;
		goto out;
	}
	
	ccm_fmt.fmt.pix.pixelformat = csi_fmt->ccm_fmt;
	ccm_fmt.fmt.pix.width = f->fmt.pix.width;
	ccm_fmt.fmt.pix.height = f->fmt.pix.width;
	
	ret = v4l2_subdev_call(dev->sd,video,s_fmt,f);
	if (ret < 0) {
		csi_err("v4l2 sub device s_fmt error!\n");
		goto out;
	}
	
	//save the current format info
	dev->fmt = csi_fmt;
	dev->vb_vidq.field = f->fmt.pix.field;
	dev->width  = f->fmt.pix.width;
	dev->height = f->fmt.pix.height;
	
	//set format
	dev->csi_mode.output_fmt = dev->fmt->output_fmt;
	dev->csi_mode.input_fmt = dev->fmt->input_fmt;
	
	switch(dev->fmt->ccm_fmt) {
	case V4L2_PIX_FMT_YUYV:
		dev->csi_mode.seq = CSI_YUYV;
		break;
	case V4L2_PIX_FMT_YVYU:
		dev->csi_mode.seq = CSI_YVYU;
		break;
	case V4L2_PIX_FMT_UYVY:
		dev->csi_mode.seq = CSI_UYVY;
		break;
	case V4L2_PIX_FMT_VYUY:
		dev->csi_mode.seq = CSI_VYUY;
		break;
	default:
		dev->csi_mode.seq = CSI_YUYV;
		break;
	}
	
	switch(dev->fmt->input_fmt){
	case CSI_RAW:
		if ( (dev->fmt->fourcc == V4L2_PIX_FMT_YUYV) || (dev->fmt->fourcc == V4L2_PIX_FMT_YVYU) || \
				 (dev->fmt->fourcc == V4L2_PIX_FMT_UYVY) || (dev->fmt->fourcc == V4L2_PIX_FMT_VYUY)) {
			
			width_len  = dev->width*2;
			width_buf = dev->width*2;
			height_buf = dev->height;

		} else {
			width_len  = dev->width;
			width_buf = dev->width;
			height_buf = dev->height;
		}
		break;
	case CSI_BAYER:
		width_len  = dev->width;
		width_buf = dev->width;
		height_buf = dev->height;
		break;
	case CSI_CCIR656://TODO
	case CSI_YUV422:
		width_len  = dev->width;
		width_buf = dev->width*2;
		height_buf = dev->height;
		break;
	default:
		width_len  = dev->width;
		width_buf = dev->width*2;
		height_buf = dev->height;
		break;
	}			

	bsp_csi_configure(dev,&dev->csi_mode);
	//horizontal and vertical offset are constant zero
	bsp_csi_set_size(dev,width_buf,height_buf,width_len);

	ret = 0;
out:
	mutex_unlock(&q->vb_lock);
	return ret;
}

static int vidioc_reqbufs(struct file *file, void *priv,
			  struct v4l2_requestbuffers *p)
{
	struct csi_dev *dev = video_drvdata(file);
	
	csi_dbg(0,"vidioc_reqbufs\n");
	
	return videobuf_reqbufs(&dev->vb_vidq, p);
}

static int vidioc_querybuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct csi_dev *dev = video_drvdata(file);
	
	return videobuf_querybuf(&dev->vb_vidq, p);
}

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct csi_dev *dev = video_drvdata(file);
	
	return videobuf_qbuf(&dev->vb_vidq, p);
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct csi_dev *dev = video_drvdata(file);

	return videobuf_dqbuf(&dev->vb_vidq, p, file->f_flags & O_NONBLOCK);
}

#ifdef CONFIG_VIDEO_V4L1_COMPAT
static int vidiocgmbuf(struct file *file, void *priv, struct video_mbuf *mbuf)
{
	struct csi_dev *dev = video_drvdata(file);

	return videobuf_cgmbuf(&dev->vb_vidq, mbuf, 8);
}
#endif


static int vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct csi_dev *dev = video_drvdata(file);
	struct csi_dmaqueue *dma_q = &dev->vidq;
	struct csi_buffer *buf;
	
	int ret;

	csi_dbg(0,"video stream on\n");
	if (i != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		return -EINVAL;
	}	
	
	if (csi_is_generating(dev)) {
		csi_err("stream has been already on\n");
		return 0;
	}
	
	/* Resets frame counters */
	dev->ms = 0;
	dev->jiffies = jiffies;

	dma_q->frame = 0;
	dma_q->ini_jiffies = jiffies;
	
	ret = videobuf_streamon(&dev->vb_vidq);
	if (ret) {
		return ret;
	}	
	
	buf = list_entry(dma_q->active.next,struct csi_buffer, vb.queue);
	csi_set_addr(dev,buf);
	
	bsp_csi_int_clear_status(dev,CSI_INT_FRAME_DONE);//CSI_INT_FRAME_DONE
	bsp_csi_int_enable(dev, CSI_INT_FRAME_DONE);//CSI_INT_FRAME_DONE
	bsp_csi_capture_video_start(dev);
	csi_start_generating(dev);
		
	return 0;
}


static int vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct csi_dev *dev = video_drvdata(file);
	struct csi_dmaqueue *dma_q = &dev->vidq;
	int ret;

	csi_dbg(0,"video stream off\n");
	
	if (!csi_is_generating(dev)) {
		csi_err("stream has been already off\n");
		return 0;
	}
	
	csi_stop_generating(dev);

	/* Resets frame counters */
	dev->ms = 0;
	dev->jiffies = jiffies;

	dma_q->frame = 0;
	dma_q->ini_jiffies = jiffies;
	
	bsp_csi_int_disable(dev,CSI_INT_FRAME_DONE);//CSI_INT_FRAME_DONE
	bsp_csi_int_clear_status(dev,CSI_INT_FRAME_DONE);//CSI_INT_FRAME_DONE
	bsp_csi_capture_video_stop(dev);
	
	if (i != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		return -EINVAL;
	}

	ret = videobuf_streamoff(&dev->vb_vidq);
	if (ret!=0) {
		csi_err("videobu_streamoff error!\n");
		return ret;
	}
	
	if (ret!=0) {
		csi_err("videobuf_mmap_free error!\n");
		return ret;
	}
	
	return 0;
}


/* only one input in this sample driver */
static int vidioc_enum_input(struct file *file, void *priv,
				struct v4l2_input *inp)
{
	if (inp->index > NUM_INPUTS-1) {
		csi_err("input index invalid!\n");
		return -EINVAL;
	}

	inp->type = V4L2_INPUT_TYPE_CAMERA;
	
	return 0;
}

static int vidioc_g_input(struct file *file, void *priv, unsigned int *i)
{
	struct csi_dev *dev = video_drvdata(file);

	*i = dev->input;
	return 0;
}

static int vidioc_s_input(struct file *file, void *priv, unsigned int i)
{
	struct csi_dev *dev = video_drvdata(file);

	if (i > NUM_INPUTS-1) {
		csi_err("set input error!\n");
		return -EINVAL;
	}
	dev->input = i;
	
	return 0;
}


static int vidioc_queryctrl(struct file *file, void *priv,
			    struct v4l2_queryctrl *qc)
{
	struct csi_dev *dev = video_drvdata(file);
	int ret;
		
	ret = v4l2_subdev_call(dev->sd,core,queryctrl,qc);
	if (ret < 0)
		csi_err("v4l2 sub device queryctrl error!\n");
	
	return ret;
}

static int vidioc_g_ctrl(struct file *file, void *priv,
			 struct v4l2_control *ctrl)
{
	struct csi_dev *dev = video_drvdata(file);
	int ret;
	
	ret = v4l2_subdev_call(dev->sd,core,g_ctrl,ctrl);
	if (ret < 0)
		csi_err("v4l2 sub device g_ctrl error!\n");
	
	return ret;
}


static int vidioc_s_ctrl(struct file *file, void *priv,
				struct v4l2_control *ctrl)
{
	struct csi_dev *dev = video_drvdata(file);
	struct v4l2_queryctrl qc;
	int ret;
	
	qc.id = ctrl->id;
	ret = vidioc_queryctrl(file, priv, &qc);
	if (ret < 0) {
		return ret;
	}

	if (ctrl->value < qc.minimum || ctrl->value > qc.maximum) {
		return -ERANGE;
	}

	ret = v4l2_subdev_call(dev->sd,core,s_ctrl,ctrl);
	if (ret < 0)
		csi_err("v4l2 sub device s_ctrl error!\n");
	
	return ret;
}

static int vidioc_g_parm(struct file *file, void *priv,
			 struct v4l2_streamparm *parms) 
{
	struct csi_dev *dev = video_drvdata(file);
	int ret;
	
	ret = v4l2_subdev_call(dev->sd,video,g_parm,parms);
	if (ret < 0)
		csi_err("v4l2 sub device g_parm error!\n");

	return ret;
}

static int vidioc_s_parm(struct file *file, void *priv,
			 struct v4l2_streamparm *parms)
{
	struct csi_dev *dev = video_drvdata(file);
	int ret;
	
	ret = v4l2_subdev_call(dev->sd,video,s_parm,parms);
	if (ret < 0)
		csi_err("v4l2 sub device s_parm error!\n");

	return ret;
}


static ssize_t csi_read(struct file *file, char __user *data, size_t count, loff_t *ppos)
{
	struct csi_dev *dev = video_drvdata(file);

	csi_start_generating(dev);
	return videobuf_read_stream(&dev->vb_vidq, data, count, ppos, 0,
					file->f_flags & O_NONBLOCK);
}

static unsigned int csi_poll(struct file *file, struct poll_table_struct *wait)
{
	struct csi_dev *dev = video_drvdata(file);
	struct videobuf_queue *q = &dev->vb_vidq;

	csi_start_generating(dev);
	return videobuf_poll_stream(file, q, wait);
}

static int csi_open(struct file *file)
{
	struct csi_dev *dev = video_drvdata(file);
	int ret;

	csi_dbg(0,"csi_open\n");

	if (dev->opened == 1) {
		csi_err("device open busy\n");
		return -EBUSY;
	}
	
  dev->csi_mode.vref       = dev->ccm_info.vref;
  dev->csi_mode.href       = dev->ccm_info.href;
  dev->csi_mode.clock      = dev->ccm_info.clock;
  
	csi_clk_enable(dev);
	csi_reset_disable(dev);
	
	bsp_csi_open(dev);
	bsp_csi_set_offset(dev,0,0);//h and v offset is initialed to zero
	
	
	ret = v4l2_subdev_call(dev->sd,core, init,0);
	if (ret!=0) {
		csi_err("sensor initial error when csi open!\n");
	} else {
		csi_print("sensor initial success when csi open!\n");
	}	

	
	dev->input=0;//default input
	dev->opened=1;
	
	return 0;		
}

static int csi_close(struct file *file)
{
	struct csi_dev *dev = video_drvdata(file);

	csi_dbg(0,"csi_close\n");

	bsp_csi_int_disable(dev,CSI_INT_FRAME_DONE);//CSI_INT_FRAME_DONE
	//bsp_csi_int_clear_status(dev,CSI_INT_FRAME_DONE);

	bsp_csi_capture_video_stop(dev);
	bsp_csi_close(dev);

	csi_clk_disable(dev);
	csi_reset_enable(dev);


	videobuf_stop(&dev->vb_vidq);
	videobuf_mmap_free(&dev->vb_vidq);

	dev->opened=0;
	csi_stop_generating(dev);

	return 0;
}

static int csi_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct csi_dev *dev = video_drvdata(file);
	int ret;

	csi_dbg(0,"mmap called, vma=0x%08lx\n", (unsigned long)vma);

	ret = videobuf_mmap_mapper(&dev->vb_vidq, vma);

	csi_dbg(0,"vma start=0x%08lx, size=%ld, ret=%d\n",
		(unsigned long)vma->vm_start,
		(unsigned long)vma->vm_end - (unsigned long)vma->vm_start,
		ret);
	return ret;
}

static const struct v4l2_file_operations csi_fops = {
	.owner	  = THIS_MODULE,
	.open	  = csi_open,
	.release  = csi_close,
	.read     = csi_read,
	.poll	  = csi_poll,
	.ioctl    = video_ioctl2,
	.mmap     = csi_mmap,
};

static const struct v4l2_ioctl_ops csi_ioctl_ops = {
	.vidioc_querycap          = vidioc_querycap,
	.vidioc_enum_fmt_vid_cap  = vidioc_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap     = vidioc_g_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap   = vidioc_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap     = vidioc_s_fmt_vid_cap,
	.vidioc_reqbufs           = vidioc_reqbufs,
	.vidioc_querybuf          = vidioc_querybuf,
	.vidioc_qbuf              = vidioc_qbuf,
	.vidioc_dqbuf             = vidioc_dqbuf,
	.vidioc_enum_input        = vidioc_enum_input,
	.vidioc_g_input           = vidioc_g_input,
	.vidioc_s_input           = vidioc_s_input,
	.vidioc_streamon          = vidioc_streamon,
	.vidioc_streamoff         = vidioc_streamoff,
	.vidioc_queryctrl         = vidioc_queryctrl,
	.vidioc_g_ctrl            = vidioc_g_ctrl,
	.vidioc_s_ctrl            = vidioc_s_ctrl,
	.vidioc_g_parm		 			  = vidioc_g_parm,
	.vidioc_s_parm		  			= vidioc_s_parm,

#ifdef CONFIG_VIDEO_V4L1_COMPAT
	.vidiocgmbuf              = vidiocgmbuf,
#endif
};

static struct video_device csi_template = {
	.name		= "csi",
	.fops       = &csi_fops,
	.ioctl_ops 	= &csi_ioctl_ops,
	.release	= video_device_release,
};

static int csi_probe(struct platform_device *pdev)
{
	struct csi_dev *dev;
	struct resource *res;
	struct video_device *vfd;
	struct i2c_adapter *i2c_adap;
	int ret = 0;

	csi_dbg(0,"csi_probe\n");

	/*request mem for dev*/	
	dev = kzalloc(sizeof(struct csi_dev), GFP_KERNEL);
	if (!dev) {
		csi_err("request dev mem failed!\n");
		return -ENOMEM;
	}
	dev->id = pdev->id;
	dev->pdev = pdev;
	
	spin_lock_init(&dev->slock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		csi_err("failed to find the registers\n");
		ret = -ENOENT;
		goto err_info;
	}

	dev->regs_res = request_mem_region(res->start, resource_size(res),
			dev_name(&pdev->dev));
	if (!dev->regs_res) {
		csi_err("failed to obtain register region\n");
		ret = -ENOENT;
		goto err_info;
	}
	
	dev->regs = ioremap(res->start, resource_size(res));
	if (!dev->regs) {
		csi_err("failed to map registers\n");
		ret = -ENXIO;
		goto err_req_region;
	}

  
  /*get irq resource*/
	
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		csi_err("failed to get IRQ resource\n");
		ret = -ENXIO;
		goto err_regs_unmap;
	}
	
	dev->irq = res->start;
	
	ret = request_irq(dev->irq, csi_isr, 0, pdev->name, dev);
	if (ret) {
		csi_err("failed to install irq (%d)\n", ret);
		goto err_clk;
	}

    /*pin resource*/
	dev->csi_pin_hd = gpio_request_ex("csi1_para",NULL);
	if (dev->csi_pin_hd==-1) {
		csi_err("csi1 pin request error!\n");
		ret = -ENXIO;
		goto err_irq;
	}
	
    /* v4l2 device register */
	ret = v4l2_device_register(&pdev->dev, &dev->v4l2_dev);	
	if (ret) {
		csi_err("Error registering v4l2 device\n");
		goto err_irq;
		
	}

	dev_set_drvdata(&(pdev)->dev, (dev));

    /* v4l2 subdev register	*/
	i2c_adap = i2c_get_adapter(TWI_NO);
	
	if (i2c_adap == NULL) {
		csi_err("request i2c adapter failed\n");
		ret = -EINVAL;
	}
	
	dev->sd = kmalloc(sizeof(struct v4l2_subdev *),GFP_KERNEL);
	if (dev->sd == NULL) {
		csi_err("unable to allocate memory for subdevice pointers\n");
		ret = -ENOMEM;
	}

	dev_sensor[0].addr = (unsigned short)(i2c_addr>>1);
	strcpy(dev_sensor[0].type,ccm);

	dev->sd = v4l2_i2c_new_subdev_board(&dev->v4l2_dev,
										i2c_adap,
										dev_sensor[0].type,
										dev_sensor,
										NULL);
	if (!dev->sd) {
		csi_err("Error registering v4l2 subdevice\n");
		goto free_dev;
		
	} else{
		csi_print("registered sub device\n");
	}
	
	dev->ccm_info.mclk = CSI_OUT_RATE;
	dev->ccm_info.vref = CSI_LOW;
	dev->ccm_info.href = CSI_LOW;
	dev->ccm_info.clock	 = CSI_FALLING;
	
	
	ret = v4l2_subdev_call(dev->sd,core,ioctl,CSI_SUBDEV_CMD_GET_INFO,&dev->ccm_info);
	if (ret < 0)
	{
		csi_err("Error when get ccm info,use default!\n");
	}
	
	dev->ccm_info.iocfg	 = 1;
	
	ret = v4l2_subdev_call(dev->sd,core,ioctl,CSI_SUBDEV_CMD_SET_INFO,&dev->ccm_info);
	if (ret < 0)
	{
		csi_err("Error when set ccm info,use default!\n");
	}
	
	/*clock resource*/
	if (csi_clk_get(dev)) {
		csi_err("csi clock get failed!\n");
		ret = -ENXIO;
		goto unreg_dev;
	}
//	csi_dbg("%s(): csi-%d registered successfully\n",__func__, dev->id);

	/*video device register	*/
	ret = -ENOMEM;
	vfd = video_device_alloc();
	if (!vfd) {
		goto err_clk;
	}	

	*vfd = csi_template;
	vfd->v4l2_dev = &dev->v4l2_dev;

	dev_set_name(&vfd->dev, "csi-0");
	ret = video_register_device(vfd, VFL_TYPE_GRABBER, video_nr);
	if (ret < 0) {
		goto rel_vdev;
	}	
	video_set_drvdata(vfd, dev);
	
	/*add device list*/
	/* Now that everything is fine, let's add it to device list */
	list_add_tail(&dev->csi_devlist, &csi_devlist);

	if (video_nr != -1) {
		video_nr++;
	}
	dev->vfd = vfd;

	csi_print("V4L2 device registered as %s\n",video_device_node_name(vfd));

	/*initial video buffer queue*/
	videobuf_queue_dma_contig_init(&dev->vb_vidq, &csi_video_qops,
			NULL, &dev->slock, V4L2_BUF_TYPE_VIDEO_CAPTURE,
			V4L2_FIELD_NONE,
			sizeof(struct csi_buffer), dev);
	
	/* init video dma queues */
	INIT_LIST_HEAD(&dev->vidq.active);
	//init_waitqueue_head(&dev->vidq.wq);

#ifdef AJUST_DRAM_PRIORITY
{
	void __iomem *regs_dram;
	volatile unsigned int tmpval = 0;
	volatile unsigned int tmpval_csi = 0;
	
	
	printk("Warning: we write the DRAM priority directely here, it will be fixed in the next version.");
	regs_dram = ioremap(SDRAM_REGS_pBASE, 0x2E0);
	
	tmpval = readl(regs_dram + 0x250 + 4 * 16);
	printk("cpu host port: %x\n", tmpval);
	tmpval |= 3 << 2;
	printk("cpu priority: %d\n", tmpval);
	
	tmpval_csi = readl(regs_dram + 0x250 + 4 * 20);		// csi0
	printk("csi0 host port: %x\n", tmpval_csi);
	tmpval_csi |= (!(3<<2));
	tmpval_csi |= tmpval;
	writel(tmpval_csi, regs_dram + 0x250 + 4 * 20);
	printk("csi0 host port(after): %x\n", tmpval_csi);
	
	tmpval_csi = readl(regs_dram + 0x250 + 4 * 27);		// csi1
	printk("csi1 host port: %x\n", tmpval_csi);
	tmpval_csi |= (!(3<<2));
	tmpval_csi |= tmpval;
	writel(tmpval_csi, regs_dram + 0x250 + 4 * 27);
	printk("csi1 host port(after): %x\n", tmpval_csi);
	
	iounmap(regs_dram);
}
#endif // AJUST_DRAM_PRIORITY

	return 0;

rel_vdev:
	video_device_release(vfd);
err_clk:
	csi_clk_release(dev);	
unreg_dev:
	v4l2_device_unregister(&dev->v4l2_dev);	
free_dev:
	kfree(dev);
err_irq:
	free_irq(dev->irq, dev);
err_regs_unmap:
	iounmap(dev->regs);
err_req_region:
	release_resource(dev->regs_res);
	kfree(dev->regs_res);
err_info:
	kfree(dev);
	csi_err("failed to install\n");

	return ret;
}

static int csi_release(void)
{
	struct csi_dev *dev;
	struct list_head *list;

	csi_dbg(0,"csi_release\n");
	while (!list_empty(&csi_devlist)) 
	{
		list = csi_devlist.next;
		list_del(list);
		dev = list_entry(list, struct csi_dev, csi_devlist);

		v4l2_info(&dev->v4l2_dev, "unregistering %s\n", video_device_node_name(dev->vfd));
		video_unregister_device(dev->vfd);
		v4l2_device_unregister(&dev->v4l2_dev);
		kfree(dev);
	}

	return 0;
}

static int __devexit csi_remove(struct platform_device *pdev)
{
	struct csi_dev *dev;
	dev=(struct csi_dev *)dev_get_drvdata(&(pdev)->dev);
	
	csi_dbg(0,"csi_remove\n");
	
	free_irq(dev->irq, dev);
	
	csi_clk_release(dev);
	iounmap(dev->regs);
	release_resource(dev->regs_res);
	kfree(dev->regs_res);
	kfree(dev);
	return 0;
}

static int csi_suspend(struct platform_device *pdev, pm_message_t state)
{
	
	struct csi_dev *dev=(struct csi_dev *)dev_get_drvdata(&(pdev)->dev);

	csi_dbg(0,"csi_suspend\n");
	
	csi_clk_disable(dev);

	return v4l2_subdev_call(dev->sd,core, s_power,0);//0=off;1=on
}

static int csi_resume(struct platform_device *pdev)
{
	int ret;
	struct csi_dev *dev=(struct csi_dev *)dev_get_drvdata(&(pdev)->dev);
	
	csi_dbg(0,"csi_resume\n");

	if (dev->opened==1) {
		csi_clk_out_set(dev);
	
		csi_clk_enable(dev);

		ret = v4l2_subdev_call(dev->sd,core, s_power,1);//0=off;1=on
		if (ret!=0) {
			csi_err("sensor power on error when resume from suspend!\n");
			return ret;
		} else {
			csi_dbg(0,"sensor power on success when resume from suspend!\n");
			
		}
		
		ret = v4l2_subdev_call(dev->sd,core, init,0);
		if (ret!=0) {
			csi_err("sensor initial error when resume from suspend!\n");
			return ret;
		} else {
			csi_dbg(0,"sensor initial success when resume from suspend!\n");
		}
		
		return 0;
		
	} else {
		return 0;
	}
}


static struct platform_driver csi_driver = {
	.probe		= csi_probe,
	.remove		= __devexit_p(csi_remove),
	.suspend	= csi_suspend,
	.resume		= csi_resume,
	//.id_table	= csi_driver_ids,
	.driver = {
		.name	= "sun4i_csi1",
		.owner	= THIS_MODULE,
	}
};

static struct resource csi1_resource[] = {
	[0] = {
		.start	= CSI1_REGS_BASE,
		.end	= CSI1_REGS_BASE + CSI1_REG_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= SW_INTC_IRQNO_CSI1,
		.end	= SW_INTC_IRQNO_CSI1,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device csi_device[] = {
	[0] = {
	.name           	= "sun4i_csi1",
    	.id             	= 0,
	.num_resources		= ARRAY_SIZE(csi1_resource),
    	.resource       	= csi1_resource,
	.dev            	= {
	    }
	}
};

static int __init csi_init(void)
{
	u32 ret;
	csi_print("Welcome to CSI driver\n");
	
	csi_dbg(0,"csi_init\n");
	
	ret = platform_device_register(&csi_device[0]);
	if (ret) {
		csi_err("platform device register failed\n");
		return -1;
	}
	
	ret = platform_driver_register(&csi_driver);
	
	if (ret) {
		csi_err("platform driver register failed\n");
		return -1;
	}
	return 0;
}

static void __exit csi_exit(void)
{
	csi_dbg(0,"csi_exit\n");
	csi_release();
	platform_driver_unregister(&csi_driver);
}

module_init(csi_init);
module_exit(csi_exit);

MODULE_AUTHOR("raymonxiu");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("CSI driver for sun4i");
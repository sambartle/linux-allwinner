/* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
*
* Copyright (c) 2009
*
* ChangeLog
*
*
*/
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <linux/fs.h> 
#include <asm/uaccess.h> 
#include <linux/mm.h> 
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/tick.h>
#include <asm-generic/cputime.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
    #include <linux/pm.h>
    #include <linux/earlysuspend.h>
#endif

//#undef CONFIG_HAS_EARLYSUSPEND

static int tp_flag = 0;

//tp status value
#define TP_INITIAL             (-1)
#define TP_DOWN                (0)
#define TP_UP                  (1) 
#define TP_DATA_VA             (2)  
//?
#define DUAL_TOUCH             (50)
#define TOUCH_CHANGE           (3)
#define TP_DATA_AV_NO          (0x3)

//#define PRINT_INT_INFO
//#define PRINT_FILTER_INFO
//#define PRINT_REPORT_STATUS_INFO
//#define PRINT_REPORT_DATA_INFO
//#define PRINT_TASKLET_INFO
#define CONFIG_TOUCHSCREEN_SUN4I_DEBUG
#define PRINT_SUSPEND_INFO

//#define TP_TEMP_DEBUG


#define IRQ_TP                 (29)
#define TP_BASSADDRESS         (0xf1c25000)
#define TP_CTRL0               (0x00)
#define TP_CTRL1               (0x04)
#define TP_CTRL2               (0x08)
#define TP_CTRL3               (0x0c)
#define TP_INT_FIFOC           (0x10)
#define TP_INT_FIFOS           (0x14)
#define TP_TPR                 (0x18)
#define TP_CDAT                (0x1c)
#define TEMP_DATA              (0x20)
#define TP_DATA                (0x24)


#define ADC_FIRST_DLY          (0x1<<24)
#define ADC_FIRST_DLY_MODE     (0x1<<23) 
#define ADC_CLK_SELECT         (0x0<<22)
#define ADC_CLK_DIVIDER        (0x2<<20)    
#define CLK                    (7)
#define FS_DIV                 (CLK<<16)
#define ACQ                    (0x3f)
#define T_ACQ                  (ACQ)

#define STYLUS_UP_DEBOUNCE     (0<<12)
#define STYLUS_UP_DEBOUCE_EN   (0<<9)
#define TOUCH_PAN_CALI_EN      (1<<6)
#define TP_DUAL_EN             (1<<5)
#define TP_MODE_EN             (1<<4)
#define TP_ADC_SELECT          (0<<3)
#define ADC_CHAN_SELECT        (0)

#define TP_SENSITIVE_ADJUST    (0xc<<28)
#define TP_MODE_SELECT         (0x1<<26)
#define PRE_MEA_EN             (0x1<<24)
#define PRE_MEA_THRE_CNT       (0xFFF<<0)

#define FILTER_EN              (1<<2)
#define FILTER_TYPE            (0x01<<0)

#define TP_DATA_IRQ_EN         (1<<16)
#define TP_DATA_XY_CHANGE      (1<<13)
#define TP_FIFO_TRIG_LEVEL     (3<<8)
#define TP_FIFO_FLUSH          (1<<4)
#define TP_UP_IRQ_EN           (1<<1)
#define TP_DOWN_IRQ_EN         (1<<0)

#define FIFO_DATA_PENDING      (1<<16)
#define TP_UP_PENDING          (1<<1)
#define TP_DOWN_PENDING        (1<<0)

#define SINGLE_TOUCH_MODE      (1)
#define DOUBLE_TOUCH_MODE      (2)
#define SINGLE_CNT_LIMIT       (25)
#define DOUBLE_CNT_LIMIT       (8)

                               
#define TPDATA_MASK            (0xfff)
#define FILTER_NOISE_LOWER_LIMIT  (2)
#define MAX_DELTA_X             (700-100)     //    avoid excursion
#define MAX_DELTA_Y             (1200-200)        
#define X_TURN_POINT            (330)         // x1 < (1647 - MAX_DELTA_X) /3
#define X_COMPENSATE            (4*X_TURN_POINT)
#define Y_TURN_POINT            (260)         // y1 < (1468 -MAX_DELTA_Y )
#define Y_COMPENSATE            (2*Y_TURN_POINT)

#define X_CENTER_COORDINATE     (2048)        //for construct two point 
#define Y_CENTER_COORDINATE     (2048)

#define CYCLE_BUFFER_SIZE       (64)          //must be 2^n
#define DELAY_PERIOD            (8)          //delay 90 ms, unit is 10ms  

#ifndef TRUE
#define TRUE   1
#define FALSE  0
#endif

#ifdef PRINT_INT_INFO 
#define print_int_info(fmt, args...)     \
        do{                              \
                printk(fmt, ##args);     \
        }while(0)
#else
#define print_int_info(fmt, args...)   //
#endif

#ifdef PRINT_FILTER_INFO 
#define print_filter_info(fmt, args...)     \
        do{                              \
                printk(fmt, ##args);     \
        }while(0)
#else
#define print_filter_info(fmt, args...)   //
#endif

#ifdef PRINT_REPORT_STATUS_INFO 
#define print_report_status_info(fmt, args...)     \
        do{                              \
                printk(fmt, ##args);     \
        }while(0)
#else
#define print_report_status_info(fmt, args...)   //
#endif

#ifdef PRINT_REPORT_DATA_INFO 
#define print_report_data_info(fmt, args...)     \
        do{                              \
                printk(fmt, ##args);     \
        }while(0)
#else
#define print_report_data_info(fmt, args...)   //
#endif

#ifdef PRINT_TASKLET_INFO 
#define print_tasklet_info(fmt, args...)     \
            do{                              \
                    printk(fmt, ##args);     \
            }while(0)
#else
#define print_tasklet_info(fmt, args...)   //
#endif


struct sun4i_ts_data {
	struct resource *res;
	struct input_dev *input;
	void __iomem *base_addr;
	int irq;
	char phys[32];
	unsigned int count; //for report threshold & touchmod(double to single touch mode) change
	unsigned long buffer_head; //for cycle buffer
	unsigned long buffer_tail;
	int ts_sample_status;                //for touchscreen status when sampling
	int ts_process_status;               //for record touchscreen status when process data 
	int double_point_cnt;            //for noise reduction when in double_touch_mode
	int single_touch_cnt;                //for noise reduction when change to double_touch_mode
	int ts_delay_period;                 //will determine responding sensitivity
	int touchflag;
#ifdef CONFIG_HAS_EARLYSUSPEND	
    struct early_suspend early_suspend;
#endif
};

struct ts_sample_data{
	unsigned int x1;
	unsigned int y1;
	unsigned int x;
	unsigned int y;
	unsigned int dx;
	unsigned int dy;
	unsigned int z1;
	unsigned int z2;
	int sample_status;                       //record the sample point status when sampling 
	unsigned int sample_time;
};

struct sun4i_ts_data * mtTsData =NULL;	
static int touch_mode = SINGLE_TOUCH_MODE;
static int change_mode = TRUE;
static int tp_irq_state = TRUE;

static cputime64_t cur_wall_time = 0L;
//static cputime64_t cur_idle_time = 0L;

static struct ts_sample_data cycle_buffer[CYCLE_BUFFER_SIZE] = {{0},};
static struct ts_sample_data *prev_sample;
static struct timer_list data_timer;
static int data_timer_status = 0;  //when 1, mean tp driver have recervied data, and begin to report data, and start timer to reduce up&down signal  


//for test
//static int tp_irq = 0;

void tp_do_tasklet(unsigned long data);
DECLARE_TASKLET(tp_tasklet,tp_do_tasklet,0);

//ͣ���豸
#ifdef CONFIG_HAS_EARLYSUSPEND
static void sun4i_ts_suspend(struct early_suspend *h)
{
	/*int ret;
	struct sun4i_ts_data *ts = container_of(h, struct sun4i_ts_data, early_suspend);
    */
    #ifdef PRINT_SUSPEND_INFO
        printk("enter earlysuspend: sun4i_ts_suspend. \n");
    #endif
    writel(0,TP_BASSADDRESS + TP_CTRL1);
	return ;
}

//���»���
static void sun4i_ts_resume(struct early_suspend *h)
{
	/*int ret;
	struct sun4i_ts_data *ts = container_of(h, struct sun4i_ts_data, early_suspend);
    */
    #ifdef PRINT_SUSPEND_INFO
        printk("enter laterresume: sun4i_ts_resume. \n");
    #endif    
    writel(STYLUS_UP_DEBOUNCE|STYLUS_UP_DEBOUCE_EN|TP_DUAL_EN|TP_MODE_EN,TP_BASSADDRESS + TP_CTRL1);
	return ;
}
#else
//ͣ���豸
#ifdef CONFIG_PM
static int sun4i_ts_suspend(struct platform_device *pdev, pm_message_t state)
{
    #ifdef PRINT_SUSPEND_INFO
        printk("enter: sun4i_ts_suspend. \n");
    #endif

    writel(0,TP_BASSADDRESS + TP_CTRL1);
	return 0;
}

static int sun4i_ts_resume(struct platform_device *pdev)
{
    #ifdef PRINT_SUSPEND_INFO
        printk("enter: sun4i_ts_resume. \n");
    #endif 

    writel(STYLUS_UP_DEBOUNCE|STYLUS_UP_DEBOUCE_EN|TP_DUAL_EN|TP_MODE_EN,TP_BASSADDRESS + TP_CTRL1);
	return 0;
}
#endif

#endif

	
static int  tp_init(void)
{
    //struct sun4i_ts_data *ts_data = (struct sun4i_ts_data *)platform_get_drvdata(pdev);
//    struct sun4i_ts_data *ts_data = mtTsData;	
    //TP_CTRL0: 0x0027003f
    writel(ADC_CLK_DIVIDER|FS_DIV|T_ACQ, TP_BASSADDRESS + TP_CTRL0);	   
    //TP_CTRL2: 0xc4000000
    writel(TP_SENSITIVE_ADJUST|TP_MODE_SELECT,TP_BASSADDRESS + TP_CTRL2);
    //TP_CTRL3: 0x05
    writel(FILTER_EN|FILTER_TYPE,TP_BASSADDRESS + TP_CTRL3);
    
    #ifdef TP_TEMP_DEBUG
        //TP_INT_FIFOC: 0x00010313
        writel(TP_DATA_IRQ_EN|TP_FIFO_TRIG_LEVEL|TP_FIFO_FLUSH|TP_UP_IRQ_EN|TP_DOWN_IRQ_EN|0x40000, TP_BASSADDRESS + TP_INT_FIFOC);
        writel(0x10fff, TP_BASSADDRESS + TP_TPR);
    #else
        //TP_INT_FIFOC: 0x00010313
        writel(TP_DATA_IRQ_EN|TP_FIFO_TRIG_LEVEL|TP_FIFO_FLUSH|TP_UP_IRQ_EN|TP_DOWN_IRQ_EN, TP_BASSADDRESS + TP_INT_FIFOC);
    #endif
    //TP_CTRL1: 0x00000070 -> 0x00000030
    writel(STYLUS_UP_DEBOUNCE|STYLUS_UP_DEBOUCE_EN|TP_DUAL_EN|TP_MODE_EN,TP_BASSADDRESS + TP_CTRL1);
    

  /*
       //put_wvalue(TP_CTRL0 ,0x02a6007f);  //512us TACQ  4ms FS 60 point
      //put_wvalue(TP_CTRL0 ,0x0aa7003f); 
      put_wvalue(TP_CTRL0 ,0x00a7003f);   //100 point
      put_wvalue(TP_CTRL1 ,0x00000030);
      //put_wvalue(TP_INT_FIFOC,0x12f13);
      put_wvalue(TP_INT_FIFOC,0x10313);
      //put_wvalue(TP_CTRL2,0x90003e8); 
      //put_wvalue(TP_CTRL2,0x9002710); 
      put_wvalue(TP_CTRL2,0xc4002710);
      put_wvalue(TP_CTRL3,0x00000005);
   */
   
   /*
    writel(0x00b70100,TP_BASSADDRESS + TP_CTRL0);
    writel(0xfd000000,TP_BASSADDRESS + TP_CTRL2);
    writel(0x4,TP_BASSADDRESS + TP_CTRL3);
    writel(0x10513,TP_BASSADDRESS +TP_INT_FIFOC);
    writel(0x00005230,TP_BASSADDRESS + TP_CTRL1);*/
    return (0);
}

static void report_single_point(struct sun4i_ts_data *ts_data, struct ts_sample_data *sample_data)
{
    input_report_abs(ts_data->input, ABS_MT_TOUCH_MAJOR,800);
    input_report_abs(ts_data->input, ABS_MT_POSITION_X, sample_data->x);
    input_report_abs(ts_data->input, ABS_MT_POSITION_Y, sample_data->y);   
/*
    print_report_data_info("report single point: x = %d, y = %d\n                  \
            sample_data->dx = %d, sample_data->dy = %d. \n",      \
            sample_data->x, sample_data->y, sample_data->dx, sample_data->dy);
   */
    print_report_data_info("report single point: x = %d \n", sample_data->x);
    
    input_mt_sync(ts_data->input);                 
    input_sync(ts_data->input);
   
    return;
}

static void report_double_point(struct sun4i_ts_data *ts_data, struct ts_sample_data *sample_data)
{
	int x1,x2,y1,y2;
	//int tmp;
    //tmp = X_TURN_POINT<<2;
	input_report_abs(ts_data->input, ABS_MT_TOUCH_MAJOR,800);
	if(sample_data->dx < X_TURN_POINT){
	    x1 = X_CENTER_COORDINATE - (sample_data->dx<<2);
        x2 = X_CENTER_COORDINATE + (sample_data->dx<<2);  
	}else{
        x1 = X_CENTER_COORDINATE - X_COMPENSATE - ((sample_data->dx) - X_TURN_POINT);
        x2 = X_CENTER_COORDINATE + X_COMPENSATE + ((sample_data->dx) - X_TURN_POINT); 
	}
   
    //printk("X_TURN_POINT is %d. \n", tmp);
    
	if(sample_data->dy < Y_TURN_POINT){
        y1 = Y_CENTER_COORDINATE - (sample_data->dy<<1);
        y2 = Y_CENTER_COORDINATE + (sample_data->dy<<1);

	}else{
        y1 = Y_CENTER_COORDINATE - Y_COMPENSATE - (sample_data->dy - Y_TURN_POINT);
        y2 = Y_CENTER_COORDINATE + Y_COMPENSATE + (sample_data->dy - Y_TURN_POINT);
	}

    //printk("dx:%d, dy: %d, x1:%d, y1:%d \n", sample_data->dx, sample_data->dy, x1, y1);

	input_report_abs(ts_data->input, ABS_MT_POSITION_X, x1);
	input_report_abs(ts_data->input, ABS_MT_POSITION_Y, y1);
	input_mt_sync(ts_data->input);
	
	input_report_abs(ts_data->input, ABS_MT_TOUCH_MAJOR,800);
	input_report_abs(ts_data->input, ABS_MT_POSITION_X, x2);
	input_report_abs(ts_data->input, ABS_MT_POSITION_Y, y2);
	input_mt_sync(ts_data->input);
	input_sync(ts_data->input);

    print_report_data_info("report two point: x1 = %d, y1 = %d; x2 = %d, y2 = %d. \n",x1, y1, x2, y2);
    print_report_data_info("sample_data->dx = %d, sample_data->dy = %d. \n", sample_data->dx, sample_data->dy);


    return;
}

static void report_data(struct sun4i_ts_data *ts_data, struct ts_sample_data *sample_data)
{
    //printk("calling report data. \n");
    if(TRUE == change_mode){                 //only up event happened, chang_mode is allowed.
        //remain in double touch mode    
        ts_data->single_touch_cnt++;        
        if(ts_data->single_touch_cnt > SINGLE_CNT_LIMIT){ 
            //change to single touch mode
            report_single_point(ts_data, sample_data);
            touch_mode = SINGLE_TOUCH_MODE;

            print_report_data_info("change touch mode to SINGLE_TOUCH_MODE");

        }
    }else if(FALSE == change_mode){
          //keep in double touch mode
          //igore single point                                                                                
    }  

    return;
}

static void report_up_event(unsigned long data)
{
    struct sun4i_ts_data *ts_data = (struct sun4i_ts_data *)data;

    /*when the time is out, and the buffer data can not affect the timer to re-timing immediately,
        *this will happen, 
        *from this we can conclude, the delay_time is not proper, need to be longer
        */
    if(ts_data->buffer_head != ts_data->buffer_tail){ 
        printk("warn: when report_up_event, the buffer is not empty. clear the buffer.\n");
        printk("ts_data->buffer_head = %lu,  ts_data->buffer_tail = %lu \n", ts_data->buffer_head, ts_data->buffer_tail);
        //ts_data->buffer_tail = ts_data->buffer_head;
        //do not discard the data, just let the tasklet to take care of it.
        mod_timer(&data_timer, jiffies + ts_data->ts_delay_period);
        return;
    }
    
    print_report_status_info("report_up_event. \n");

    //note: below operation may be interfere by intterrupt, but it does not matter
    input_report_abs(ts_data->input, ABS_MT_TOUCH_MAJOR,0);
    input_sync(ts_data->input);
    del_timer(&data_timer);
    data_timer_status = 0;
    ts_data->ts_process_status = TP_UP;
    ts_data->double_point_cnt = 0;
    //ts_data->buffer_head = 0;
    //ts_data->buffer_tail = 0;
    ts_data->touchflag = 0; 
    //ts_data->count     = 0;
    //touch_mode = SINGLE_TOUCH_MODE;
    change_mode = TRUE;

    return;
}

static void process_data(struct sun4i_ts_data *ts_data, struct ts_sample_data *sample_data)
{
    //printk("enter process_data. \n");
    ts_data->touchflag = 1;
    if(((sample_data->dx) > DUAL_TOUCH)&&((sample_data->dy) > DUAL_TOUCH)){
        ts_data->touchflag = 2;
        ts_data->double_point_cnt++;
        //printk("ts_data->double_point_cnt is %d. \n", ts_data->double_point_cnt);
        if(ts_data->double_point_cnt > DOUBLE_CNT_LIMIT){
            if(sample_data->dx < MAX_DELTA_X && sample_data->dy < MAX_DELTA_Y){
                    //ts_data->count = 0;  
                    touch_mode = DOUBLE_TOUCH_MODE;
                    change_mode = FALSE;
                    ts_data->single_touch_cnt = 0; //according this counter, change to single touch mode
                    report_double_point(ts_data, sample_data);
            }
        }
    
    }else  if(1 == ts_data->touchflag){
           if(DOUBLE_TOUCH_MODE == touch_mode ){
                 report_data(ts_data, sample_data);
           }else if(SINGLE_TOUCH_MODE == touch_mode ){//remain in single touch mode
                 report_single_point(ts_data, sample_data);
           }                    
    }
    
    return;

}



void tp_do_tasklet(unsigned long data)
{
    //struct sun4i_ts_data *ts_data = (struct sun4i_ts_data *)platform_get_drvdata(pdev);

	struct sun4i_ts_data *ts_data = mtTsData;
	struct ts_sample_data *sample_data;

    //
    int head = (int)data;
    int tail = (int)ts_data->buffer_tail; //!!! tail may have changed, while the data is remain?

    if((tail + CYCLE_BUFFER_SIZE*2) < head){ //tail have been modify to avoid overflow
        return;
    }
    
    print_tasklet_info("enter tasklet. head = %d, tail = %d\n", head, tail);
    
    while((tail) < (head)){ //when tail == head, mean the buffer is empty
        sample_data = &cycle_buffer[tail&(CYCLE_BUFFER_SIZE-1)];
        tail++;
        print_filter_info("sample_data->sample_status == %d, ts_data->ts_process_status == %d \n", \
                           sample_data->sample_status, ts_data->ts_process_status);
        if(TP_UP == sample_data->sample_status || TP_DOWN == sample_data->sample_status)
        {
            //when received up & down event, reinitialize ts_data->double_point_cnt to debounce for DOUBLE_TOUCH_MODE        
            ts_data->double_point_cnt = 0; 
            
            if((TP_DATA_VA == ts_data->ts_process_status || TP_DOWN == ts_data->ts_process_status) && data_timer_status){
                //delay   20ms , ignore up event & down event
                print_filter_info("(prev_sample->sample_time + ts_data->ts_delay_period) == %u, \
                        (sample_data->sample_time) == %u. \n", \
                        (prev_sample->sample_time + ts_data->ts_delay_period), \
                        (sample_data->sample_time));
      
                if(time_after_eq((unsigned long)(prev_sample->sample_time + ts_data->ts_delay_period), (unsigned long)(sample_data->sample_time))){
                    //notice: sample_time may overflow
                    print_filter_info("ignore up event & down event. \n");
                    mod_timer(&data_timer, jiffies + ts_data->ts_delay_period);
                    continue;
                }
                
            }            
        }
        
      	switch(sample_data->sample_status)
      	{
      		case TP_DOWN:
      		{
    			ts_data->touchflag = 0; 
    			//ts_data->count     = 0;
    			ts_data->ts_process_status = TP_DOWN;
    			ts_data->double_point_cnt = 0;
    			print_report_status_info("actuall TP_DOWN . \n");
      			break;
      		}
      		case TP_DATA_VA:
      		{
      		    //memcpy(prev_sample, sample_date, sizeof(ts_sample_data));
      		    prev_sample->sample_time = sample_data->sample_time;
                process_data(ts_data, sample_data);
                if(0 == data_timer_status){
                    mod_timer(&data_timer, jiffies + ts_data->ts_delay_period);
                    data_timer_status = 1;
                    print_report_status_info("timer is start up. \n");
                }else{
                    mod_timer(&data_timer, jiffies + ts_data->ts_delay_period);
                   // print_report_info("more ts_data->ts_delay_period ms delay. \n");
                }
                ts_data->ts_process_status = TP_DATA_VA;
      			break;
      		}      		
      		case TP_UP :
        	{
        	    //actually, this case will never be run
        	    if(1 == ts_data->touchflag || 2 == ts_data->touchflag)
        	    {
        	        print_report_status_info("actually TP_UP. \n");
                    ts_data->touchflag = 0; 
                    //ts_data->count     = 0;
                    //touch_mode = SINGLE_TOUCH_MODE;
                    change_mode = TRUE;
                    ts_data->ts_process_status = TP_UP;
                    report_up_event((unsigned long)ts_data);
                    

        	    }
    		    break;
      		}
      		
    		default:	
      			break;
      	
      	}
    }
    //update buffer_tail
    ts_data->buffer_tail = (unsigned long)tail;

    //avoid overflow
    if(ts_data->buffer_tail > (CYCLE_BUFFER_SIZE << 4)){
        writel(0, ts_data->base_addr + TP_INT_FIFOC);         //disable irq
        
        ts_data->buffer_tail -= (CYCLE_BUFFER_SIZE<<3);
        ts_data->buffer_head -= (CYCLE_BUFFER_SIZE<<3);        //head may have been change by interrupt       

        if(TRUE == tp_irq_state){
            //enable irq
            writel(TP_DATA_IRQ_EN|TP_FIFO_TRIG_LEVEL|TP_FIFO_FLUSH|TP_UP_IRQ_EN|TP_DOWN_IRQ_EN, ts_data->base_addr + TP_INT_FIFOC);
        }
    }
   
    if(FALSE == tp_irq_state){
        //enable irq
        writel(TP_DATA_IRQ_EN|TP_FIFO_TRIG_LEVEL|TP_FIFO_FLUSH|TP_UP_IRQ_EN|TP_DOWN_IRQ_EN, ts_data->base_addr + TP_INT_FIFOC);
        tp_irq_state = TRUE;
    }
    
}

static irqreturn_t sun4i_isr_tp(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct sun4i_ts_data *ts_data = (struct sun4i_ts_data *)platform_get_drvdata(pdev);

	unsigned int reg_val;
    int head_index = (int)(ts_data->buffer_head&(CYCLE_BUFFER_SIZE-1));
    int tail = (int)ts_data->buffer_tail;

#ifdef TP_TEMP_DEBUG
static unsigned int temp_cnt = 0;
static unsigned int temp_data = 0;
#define TOTAL_TIMES            4

#endif

	reg_val  = readl(TP_BASSADDRESS + TP_INT_FIFOS);
	if(!(reg_val&(TP_DOWN_PENDING | FIFO_DATA_PENDING | TP_UP_PENDING))){
	    //printk("non tp irq . \n");
	    #ifdef TP_TEMP_DEBUG
	    if(reg_val&0x40000)
		{
		    writel(reg_val&0x40000,TP_BASSADDRESS + TP_INT_FIFOS);
			reg_val = readl(TP_BASSADDRESS + TEMP_DATA);
			if(temp_cnt < (TOTAL_TIMES - 1))
			{
				temp_data += reg_val;
				temp_cnt++;
			}else{
				  temp_data += reg_val;
				  temp_data /= TOTAL_TIMES;
				  printk("temp = ");
				  printk("%d\n",temp_data);
				  temp_data = 0;
				  temp_cnt  = 0;
			}
			
			return IRQ_HANDLED;
		}
	    #endif
	    
        return IRQ_NONE;
	}

	if(((tail+CYCLE_BUFFER_SIZE)) <= (ts_data->buffer_head)){ //when head-tail == CYCLE_BUFFER_SIZE, mean the buffer is full.
        printk("warn: cycle buffer is full. \n");
        //ts_data->buffer_tail++; //ignore one point, increment tail is danger, when tasklet is using the tail.
        writel(0, ts_data->base_addr + TP_INT_FIFOC); //disable irq
        tp_irq_state = FALSE;
        writel(reg_val,TP_BASSADDRESS + TP_INT_FIFOS); //clear irq pending
        
        tp_tasklet.data = ts_data->buffer_head;	
        printk("schedule tasklet. ts_data->buffer_head = %lu, \
                ts_data->buffer_tail = %lu.\n", ts_data->buffer_head, ts_data->buffer_tail);
                
        tasklet_schedule(&tp_tasklet);
        return IRQ_HANDLED;
	}
	
	if(reg_val&TP_DOWN_PENDING)
	{
		writel(reg_val&TP_DOWN_PENDING,TP_BASSADDRESS + TP_INT_FIFOS);
	    print_int_info("press the screen: jiffies to ms == %u , jiffies == %lu, time: = %llu \n", \
	           jiffies_to_msecs((long)get_jiffies_64()), jiffies, get_cpu_idle_time_us(0, &cur_wall_time));

		ts_data->ts_sample_status = TP_DOWN;
		ts_data->count  = 0;
		cycle_buffer[head_index].sample_status = TP_DOWN;
		cycle_buffer[head_index].sample_time= jiffies;
		//update buffer_head
	    ts_data->buffer_head++;

	}else if(reg_val&FIFO_DATA_PENDING)        //do not report data on up status
	{   
	   // if((TP_DOWN == ts_data->ts_sample_status || TP_DATA_VA == ts_data->ts_sample_status)){
            ts_data->count++;
            if(ts_data->count > FILTER_NOISE_LOWER_LIMIT){
                cycle_buffer[head_index].x      = readl(TP_BASSADDRESS + TP_DATA);
                cycle_buffer[head_index].y      = readl(TP_BASSADDRESS + TP_DATA);      
                cycle_buffer[head_index].dx     = readl(TP_BASSADDRESS + TP_DATA);
                cycle_buffer[head_index].dy     = readl(TP_BASSADDRESS + TP_DATA); 
                cycle_buffer[head_index].sample_time= jiffies;
                //ts_data->z1     = readl(TP_BASSADDRESS + TP_DATA);
                //ts_data->z2     = readl(TP_BASSADDRESS + TP_DATA);        
                cycle_buffer[head_index].sample_status = TP_DATA_VA;
                ts_data->ts_sample_status = TP_DATA_VA;
                writel(TP_DATA_IRQ_EN|TP_FIFO_TRIG_LEVEL|TP_FIFO_FLUSH|TP_UP_IRQ_EN|TP_DOWN_IRQ_EN, ts_data->base_addr+TP_INT_FIFOC); 
                writel(reg_val&FIFO_DATA_PENDING,TP_BASSADDRESS + TP_INT_FIFOS);

    		    print_int_info("data coming, jiffies to ms == %u , jiffies == %lu, time: = %llu \n", \
		           jiffies_to_msecs((long)get_jiffies_64()), jiffies, get_cpu_idle_time_us(0, &cur_wall_time));

                //update buffer_head
                ts_data->buffer_head++;

            }else{
                //flush fifo, the data you do not want to reserved, need to be flush out fifo
                writel(TP_DATA_IRQ_EN|TP_FIFO_TRIG_LEVEL|TP_FIFO_FLUSH|TP_UP_IRQ_EN|TP_DOWN_IRQ_EN, ts_data->base_addr+TP_INT_FIFOC); 

            }    

	   /* }else{ //INITIAL or UP

    printk("err: data int when tp up. jiffies to ms == %u ,  jiffies == %llu, time: = %llu \n", \
		           jiffies_to_msecs((long)get_jiffies_64()), jiffies,  get_cpu_idle_time_us(0, &cur_wall_time));

	        //writel(TP_DATA_IRQ_EN|TP_FIFO_TRIG_LEVEL|TP_FIFO_FLUSH|TP_UP_IRQ_EN|TP_DOWN_IRQ_EN, ts_data->base_addr + TP_INT_FIFOC);
	        writel(TP_DATA_IRQ_EN|TP_FIFO_TRIG_LEVEL|TP_FIFO_FLUSH|TP_UP_IRQ_EN|TP_DOWN_IRQ_EN, ts_data->base_addr+TP_INT_FIFOC); 
            writel(reg_val&FIFO_DATA_PENDING,TP_BASSADDRESS + TP_INT_FIFOS);
            return IRQ_HANDLED;
	    }*/
	    
    }else if(reg_val&TP_UP_PENDING)	{
	        writel(reg_val&TP_UP_PENDING,TP_BASSADDRESS + TP_INT_FIFOS);
	        print_int_info("up the screen. jiffies to ms == %u ,  jiffies == %lu,  time: = %llu \n", \
	                   jiffies_to_msecs((long)get_jiffies_64()), jiffies, get_cpu_idle_time_us(0, &cur_wall_time));
            
	        cycle_buffer[head_index].sample_status = TP_UP;
	        ts_data->count  = 0;
	        cycle_buffer[head_index].sample_time= jiffies;
	        //update buffer_head
	        ts_data->buffer_head++;

	}

    tp_tasklet.data = ts_data->buffer_head;	
    //print_tasklet_info("schedule tasklet. ts_data->buffer_head = %lu, ts_data->buffer_tail = %lu\n", ts_data->buffer_head, ts_data->buffer_tail);
	tasklet_schedule(&tp_tasklet);
	return IRQ_HANDLED;
}

static int sun4i_ts_open(struct input_dev *dev)
{
	/* enable clock */
	return 0;
}

static void sun4i_ts_close(struct input_dev *dev)
{
	/* disable clock */
}




static struct sun4i_ts_data *sun4i_ts_data_alloc(struct platform_device *pdev)
{
	 
	struct sun4i_ts_data *ts_data = kzalloc(sizeof(*ts_data), GFP_KERNEL);

	if (!ts_data)
		return NULL;

	ts_data->input = input_allocate_device();
	if (!ts_data->input) {
		kfree(ts_data);
		return NULL;
	}

	
	ts_data->input->evbit[0] =  BIT(EV_SYN) | BIT(EV_KEY) | BIT(EV_ABS);
	set_bit(BTN_TOUCH, ts_data->input->keybit);
	
    input_set_abs_params(ts_data->input, ABS_MT_TOUCH_MAJOR, 0, 1000, 0, 0);
    input_set_abs_params(ts_data->input, ABS_MT_POSITION_X, 0, 4095, 0, 0);
    input_set_abs_params(ts_data->input, ABS_MT_POSITION_Y, 0, 4095, 0, 0);


	ts_data->input->name = pdev->name;
	ts_data->input->phys = "sun4i_ts/input0";
	ts_data->input->id.bustype = BUS_HOST ;
	ts_data->input->id.vendor = 0x0001;
	ts_data->input->id.product = 0x0001;
	ts_data->input->id.version = 0x0100;

	ts_data->input->open = sun4i_ts_open;
	ts_data->input->close = sun4i_ts_close;
	ts_data->input->dev.parent = &pdev->dev; 
	ts_data->ts_sample_status = TP_INITIAL;
	ts_data->ts_process_status = TP_INITIAL;
	ts_data->double_point_cnt = 0;
	ts_data->single_touch_cnt = 0;
	ts_data->ts_delay_period = DELAY_PERIOD;
	
	ts_data->buffer_head = 0;
	ts_data->buffer_tail = 0;
 
	return ts_data;
}




static void sun4i_ts_data_free(struct sun4i_ts_data *ts_data)
{
	if (!ts_data)
		return;
	if (ts_data->input)
		input_free_device(ts_data->input);
	kfree(ts_data);
}


static int __devinit sun4i_ts_probe(struct platform_device *pdev)
{
	int err =0;
	int irq = platform_get_irq(pdev, 0);	
	struct sun4i_ts_data *ts_data;	
	tp_flag = 0;

    #ifdef CONFIG_TOUCHSCREEN_SUN4I_DEBUG
	    printk( "sun4i-ts.c: sun4i_ts_probe: start...\n");
	#endif

	ts_data = sun4i_ts_data_alloc(pdev);
	if (!ts_data) {
		dev_err(&pdev->dev, "Cannot allocate driver structures\n");
		err = -ENOMEM;
		goto err_out;
	}

	mtTsData = ts_data; 
	
	#ifdef CONFIG_TOUCHSCREEN_SUN4I_DEBUG
	    printk("begin get platform resourec\n");
    #endif
    
	ts_data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!ts_data->res) {
		err = -ENOMEM;
		dev_err(&pdev->dev, "Can't get the MEMORY\n");
		goto err_out1;
	}

    ts_data->base_addr = (void __iomem *)TP_BASSADDRESS;

	ts_data->irq = irq;
	//tp_irq = irq;
    
	err = request_irq(irq, sun4i_isr_tp,
		IRQF_DISABLED, pdev->name, pdev);
	if (err) {
		dev_err(&pdev->dev, "Cannot request keypad IRQ\n");
		goto err_out2;
	}	  

	
	platform_set_drvdata(pdev, ts_data);	

	//printk("Input request \n");
	/* All went ok, so register to the input system */
	err = input_register_device(ts_data->input);
	if (err)
		goto err_out3;

	#ifdef CONFIG_TOUCHSCREEN_SUN4I_DEBUG    
        printk("tp init\n");
    #endif
    
    tp_init();

    #ifdef CONFIG_TOUCHSCREEN_SUN4I_DEBUG
	    printk( "sun4i-ts.c: sun4i_ts_probe: end\n");
    #endif

#ifdef CONFIG_HAS_EARLYSUSPEND	
    printk("==register_early_suspend =\n");	
    ts_data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;	
    ts_data->early_suspend.suspend = sun4i_ts_suspend;
    ts_data->early_suspend.resume	= sun4i_ts_resume;	
    register_early_suspend(&ts_data->early_suspend);
#endif

    init_timer(&data_timer);
    data_timer.expires = jiffies + ts_data->ts_delay_period;
    data_timer.data = (unsigned long)ts_data;
    data_timer.function = report_up_event;

    prev_sample = kzalloc(sizeof(*prev_sample), GFP_KERNEL);
    
	return 0;

 err_out3:
	if (ts_data->irq)
		free_irq(ts_data->irq, pdev);
err_out2:
err_out1:
	sun4i_ts_data_free(ts_data);
err_out: 	
    #ifdef CONFIG_TOUCHSCREEN_SUN4I_DEBUG     
	    printk( "sun4i-ts.c: sun4i_ts_probe: failed!\n");
	#endif
	
	return err;
}




static int __devexit sun4i_ts_remove(struct platform_device *pdev)
{
	
	struct sun4i_ts_data *ts_data = platform_get_drvdata(pdev);	
	#ifdef CONFIG_HAS_EARLYSUSPEND	
	    unregister_early_suspend(&ts_data->early_suspend);	
	#endif
	input_unregister_device(ts_data->input);
	free_irq(ts_data->irq, pdev);	
	sun4i_ts_data_free(ts_data);
	platform_set_drvdata(pdev, NULL);

	return 0;	
}
	

static struct platform_driver sun4i_ts_driver = {
	.probe		= sun4i_ts_probe,
	.remove		= __devexit_p(sun4i_ts_remove),
#ifdef CONFIG_HAS_EARLYSUSPEND

#else
#ifdef CONFIG_PM
	.suspend	= sun4i_ts_suspend,
	.resume		= sun4i_ts_resume,
#endif
#endif
	.driver		= {
		.name	= "sun4i-ts",
	},
};


static void sun4i_ts_nop_release(struct device *dev)
{
	/* Nothing */
}

static struct resource sun4i_ts_resource[] = {
	{
	.flags  = IORESOURCE_IRQ,
	.start  = IRQ_TP ,
	.end    = IRQ_TP ,
	},

	{
	.flags	= IORESOURCE_MEM,
	.start	= TP_BASSADDRESS,
	.end	= TP_BASSADDRESS + 0x100-1,
	},
};

struct platform_device sun4i_ts_device = {
	.name		= "sun4i-ts",
	.id		    = -1,
	.dev = {
		.release = sun4i_ts_nop_release,
		},
	.resource	= sun4i_ts_resource,
	.num_resources	= ARRAY_SIZE(sun4i_ts_resource),
};


static int __init sun4i_ts_init(void)
{
    #ifdef CONFIG_TOUCHSCREEN_SUN4I_DEBUG     
	    printk("sun4i-ts.c: sun4i_ts_init: start ...\n");
	#endif
	platform_device_register(&sun4i_ts_device);
	return platform_driver_register(&sun4i_ts_driver);
}

static void __exit sun4i_ts_exit(void)
{
	platform_driver_unregister(&sun4i_ts_driver);
	platform_device_unregister(&sun4i_ts_device);

}

module_init(sun4i_ts_init);
module_exit(sun4i_ts_exit);

MODULE_AUTHOR("zhengdixu <@>");
MODULE_DESCRIPTION("sun4i touchscreen driver");
MODULE_LICENSE("GPL");

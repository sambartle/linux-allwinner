
#include "drv_hdmi_i.h"
#include "hdmi_hal.h"
#include "dev_hdmi.h"


static struct semaphore *run_sem = NULL;
static struct task_struct * HDMI_task;

void hdmi_delay_ms(__u32 t)
{
    __u32 timeout = t*HZ/1000;
    
    set_current_state(TASK_INTERRUPTIBLE);
    schedule_timeout(timeout);
}


__s32 Hdmi_open(void)
{
    __inf("[Hdmi_open]\n");

    Hdmi_hal_video_enable(1);
	//if(ghdmi.bopen == 0)
	//{
	//	up(run_sem);
	//}
	ghdmi.bopen = 1;

	return 0;
}

__s32 Hdmi_close(void)
{
    __inf("[Hdmi_close]\n");
    
	Hdmi_hal_video_enable(0); 
	ghdmi.bopen = 0;

	return 0;
}

__s32 Hdmi_set_display_mode(__disp_tv_mode_t mode)
{
	__u8 hdmi_mode;

	__inf("[Hdmi_set_display_mode],mode:%d\n",mode);
	
	switch(mode)
	{
	case DISP_TV_MOD_480I:
		hdmi_mode = HDMI_V720x480i_60Hz_4x3;
		break;
		
	case DISP_TV_MOD_576I:
		hdmi_mode = HDMI_V720x576i_50Hz_4x3;
		break;
		
	case DISP_TV_MOD_480P:
		hdmi_mode = HDMI_V720x480p_60Hz_4x3;
		break;
		
	case DISP_TV_MOD_576P:
		hdmi_mode = HDMI_V720x576p_50Hz_4x3;
		break;  
		
	case DISP_TV_MOD_720P_50HZ:
		hdmi_mode = HDMI_V1280x720p_50Hz;
		break;
		
	case DISP_TV_MOD_720P_60HZ:
		hdmi_mode = HDMI_V1280x720p_60Hz;
		break;
		
	case DISP_TV_MOD_1080I_50HZ:
		hdmi_mode = HDMI_V1920x1080i_50Hz;
		break;
		
	case DISP_TV_MOD_1080I_60HZ:
		hdmi_mode = HDMI_V1920x1080i_60Hz;
		break;         
		
	case DISP_TV_MOD_1080P_24HZ:
		hdmi_mode = HDMI_V1920x1080p_24Hz;
		break;    
		
	case DISP_TV_MOD_1080P_50HZ:
		hdmi_mode = HDMI_V1920x1080p_50Hz;
		break;
		
	case DISP_TV_MOD_1080P_60HZ:
		hdmi_mode = HDMI_V1920x1080p_60Hz;
		break;  
		
	default:
	    __wrn("unsupported video mode %d when set display mode\n", mode);
		return -1;
	}

	ghdmi.mode = mode;
	return Hdmi_hal_set_display_mode(hdmi_mode);
}

__s32 Hdmi_Audio_Enable(__u8 mode, __u8 channel)
{
    __inf("[Hdmi_Audio_Enable],ch:%d\n",channel);
    
	return Hdmi_hal_audio_enable(mode, channel);
}

__s32 Hdmi_Set_Audio_Para(hdmi_audio_t * audio_para)
{
    __inf("[Hdmi_Set_Audio_Para]\n");
    
	return Hdmi_hal_set_audio_para(audio_para);
}

__s32 Hdmi_mode_support(__u8 mode)
{
	__u8 hdmi_mode;
	
	switch(mode)
	{
	case DISP_TV_MOD_480I:
		hdmi_mode = HDMI_V720x480i_60Hz_4x3;
		break;
		
	case DISP_TV_MOD_576I:
		hdmi_mode = HDMI_V720x576i_50Hz_4x3;
		break;
		
	case DISP_TV_MOD_480P:
		hdmi_mode = HDMI_V720x480p_60Hz_4x3;
		break;
		
	case DISP_TV_MOD_576P:
		hdmi_mode = HDMI_V720x576p_50Hz_4x3;
		break;  
		
	case DISP_TV_MOD_720P_50HZ:
		hdmi_mode = HDMI_V1280x720p_50Hz;
		break;
		
	case DISP_TV_MOD_720P_60HZ:
		hdmi_mode = HDMI_V1280x720p_60Hz;
		break;
		
	case DISP_TV_MOD_1080I_50HZ:
		hdmi_mode = HDMI_V1920x1080i_50Hz;
		break;
		
	case DISP_TV_MOD_1080I_60HZ:
		hdmi_mode = HDMI_V1920x1080i_60Hz;
		break;         
		
	case DISP_TV_MOD_1080P_24HZ:
		hdmi_mode = HDMI_V1920x1080p_24Hz;
		break;    
		
	case DISP_TV_MOD_1080P_50HZ:
		hdmi_mode = HDMI_V1920x1080p_50Hz;
		break;
		
	case DISP_TV_MOD_1080P_60HZ:
		hdmi_mode = HDMI_V1920x1080p_60Hz;
		break;  
		
	default:
		hdmi_mode = HDMI_V1280x720p_50Hz;
		break;
	}

	return Hdmi_hal_mode_support(hdmi_mode);
}

__s32 Hdmi_get_HPD_status(void)
{
	return Hdmi_hal_get_HPD();
}


__s32 Hdmi_set_pll(__u32 pll, __u32 clk)
{
    Hdmi_hal_set_pll(pll, clk);
    return 0;
}

int Hdmi_run_thread(void *parg)
{
	while (1)
	{
		//if(ghdmi.bopen == 0)
		//{
		//	down(run_sem);
		//}
		
		Hdmi_hal_main_task();

		if(ghdmi.bopen)
		{		    
			hdmi_delay_ms(200);
		}
		else
		{
			hdmi_delay_ms(1000);   
		}
	}

	return 0;
}


__s32 Hdmi_init(void)
{	    
	run_sem = kmalloc(sizeof(struct semaphore),GFP_KERNEL | __GFP_ZERO);
	sema_init((struct semaphore*)run_sem,0);
	
	HDMI_task = kthread_create(Hdmi_run_thread, (void*)0, "hdmi proc");
	if(IS_ERR(HDMI_task))
	{
	    __s32 err = 0;
	    
		__wrn("Unable to start kernel thread %s.\n","hdmi proc");
		err = PTR_ERR(HDMI_task);
		HDMI_task = NULL;
		return err;
	}
	wake_up_process(HDMI_task);

    Hdmi_set_reg_base((__u32)ghdmi.io);
	Hdmi_hal_init();

	return 0;
}

__s32 Hdmi_exit(void)
{
	Hdmi_hal_exit();

	if(run_sem)
	{
		kfree(run_sem);
		run_sem = 0;
	}
	
	if(HDMI_task)
	{
		kthread_stop(HDMI_task);
		HDMI_task = 0;
	}
	return 0;
}

EXPORT_SYMBOL(Hdmi_set_display_mode);
EXPORT_SYMBOL(Hdmi_mode_support);
EXPORT_SYMBOL(Hdmi_get_HPD_status);
EXPORT_SYMBOL(Hdmi_open);
EXPORT_SYMBOL(Hdmi_close);
EXPORT_SYMBOL(Hdmi_Audio_Enable);
EXPORT_SYMBOL(Hdmi_Set_Audio_Para);
EXPORT_SYMBOL(Hdmi_set_pll);

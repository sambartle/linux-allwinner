#include "../hdmi_hal.h"
#include "hdmi_interface.h"
#include "hdmi_core.h"

volatile __u32 HDMI_BASE = 0;

extern __s32            hdmi_state;
extern __bool           video_enable;
extern __s32            video_mode;
extern HDMI_AUDIO_INFO  audio_info;
extern __s32            HPD;

void Hdmi_set_reg_base(__u32 base)
{
    HDMI_BASE = base;
}

__s32 Hdmi_hal_video_enable(__bool enable)
{
    video_enable = enable;
    
    return 0;
}

__s32 Hdmi_hal_set_display_mode(__u8 hdmi_mode)
{
	if(hdmi_mode != video_mode)
	{
		if(hdmi_state >= HDMI_State_Video_config)
			hdmi_state = HDMI_State_Video_config;
		video_mode = hdmi_mode;
	}
    return 0;
}


__s32 Hdmi_hal_audio_enable(__u8 mode, __u8 channel)
{
	/////////????????????????????????
	if(hdmi_state >= HDMI_State_Audio_config)
	{
		hdmi_state 			= HDMI_State_Audio_config;
	}
	audio_info.channel_num  = 2;
	audio_info.audio_en     = (channel == 0)?0:1;

    return 0;
}

__s32 Hdmi_hal_set_audio_para(hdmi_audio_t * audio_para)
{
    if(!audio_para)
    {
        return -1;
    }

    if(audio_para->sample_rate != audio_info.sample_rate)
    {
    	hdmi_state 				= HDMI_State_Audio_config;
    	audio_info.sample_rate 	= audio_para->sample_rate;
    	audio_info.channel_num  = 2;

    	__inf("sample_rate:%d in Hdmi_hal_set_audio_para\n", audio_info.sample_rate);
    }

    return 0;
}

__s32 Hdmi_hal_mode_support(__u8 mode)
{
    if(HPD == 0)
    {
        return 0;
    }
    else
    {
        while(hdmi_state < HDMI_State_Wait_Video_config)
        {
	        hdmi_delay_ms(1);
	    }
	    return Device_Support_VIC[mode];
	}
}

__s32 Hdmi_hal_get_HPD(void)
{
	return HPD;
}

__s32 Hdmi_hal_get_state(void)
{
    return hdmi_state;
}

__s32 Hdmi_hal_set_pll(__u32 pll, __u32 clk)
{
    hdmi_pll = pll;
    hdmi_clk = clk;
    return 0;
}

__s32 Hdmi_hal_main_task(void)
{
    hdmi_main_task_loop();
    return 0;
}

__s32 Hdmi_hal_init(void)
{	
    //hdmi_audio_t audio_para;
    
	hdmi_core_initial();

//for audio test
#if 0
    audio_para.ch0_en = 1;
    audio_para.sample_rate = 44100;
	Hdmi_hal_set_audio_para(&audio_para);

	Hdmi_hal_audio_enable(0, 1);
#endif

    return 0;
}

__s32 Hdmi_hal_exit(void)
{
    return 0;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "disp_hdmi.h"
#include "disp_display.h"
#include "disp_event.h"
#include "disp_de.h"
#include "disp_tv.h"
#include "disp_lcd.h"
#include "disp_clk.h"


__s32 Display_Hdmi_Init(void)
{
    hdmi_clk_init();
    
	gdisp.screen[0].hdmi_mode = DISP_TV_MOD_720P_50HZ;
	gdisp.screen[1].hdmi_mode = DISP_TV_MOD_720P_50HZ;

	return DIS_SUCCESS;
}

__s32 Display_Hdmi_Exit(void)
{
    hdmi_clk_exit();
    
	return DIS_SUCCESS;
}

__s32 BSP_disp_hdmi_open(__u32 sel)
{
    if(!(gdisp.screen[sel].status & HDMI_ON))
    {
    	__disp_tv_mode_t     tv_mod;

    	tv_mod = gdisp.screen[sel].hdmi_mode;

        hdmi_clk_on();
    	lcdc_clk_on(sel);
    	image_clk_on(sel);
		Image_open(sel);//set image normal channel start bit , because every de_clk_off( )will reset this bit
    	disp_clk_cfg(sel,DISP_OUTPUT_TYPE_HDMI, tv_mod);
    	Disp_lcdc_pin_cfg(sel, DISP_OUTPUT_TYPE_HDMI, 1);

        BSP_disp_set_yuv_output(sel, FALSE);
    	DE_BE_set_display_size(sel, tv_mode_to_width(tv_mod), tv_mode_to_height(tv_mod));
    	DE_BE_Output_Select(sel, sel);
    	DE_BE_Set_Outitl_enable(sel, Disp_get_screen_scan_mode(tv_mod));
    	TCON1_set_hdmi_mode(sel,tv_mod);		 	 
    	TCON1_open(sel);
    	if(gdisp.init_para.Hdmi_open)
    	{
    	    gdisp.init_para.Hdmi_open();
    	}
    	else
    	{
    	    DE_WRN("gdisp.init_para.Hdmi_open is NULL\n");
    	    return -1;
    	}
    	
    	Disp_Switch_Dram_Mode(DISP_OUTPUT_TYPE_HDMI, tv_mod);

    	gdisp.screen[sel].b_out_interlace = Disp_get_screen_scan_mode(tv_mod);
    	gdisp.screen[sel].status |= HDMI_ON;
        gdisp.screen[sel].lcdc_status |= LCDC_TCON1_USED;
        gdisp.screen[sel].output_type = DISP_OUTPUT_TYPE_HDMI;
    }
    
    return DIS_SUCCESS;
}

__s32 BSP_disp_hdmi_close(__u32 sel)
{
    if(gdisp.screen[sel].status & HDMI_ON)
    {    
    	if(gdisp.init_para.Hdmi_close)
    	{
    	    gdisp.init_para.Hdmi_close();
    	}
    	else
    	{
    	    DE_WRN("gdisp.init_para.Hdmi_close is NULL\n");
    	    return -1;
    	}
        Image_close(sel);
    	TCON1_close(sel);

    	image_clk_off(sel);
    	lcdc_clk_off(sel);
    	hdmi_clk_off();
    	DE_BE_Set_Outitl_enable(sel, FALSE);
    	
        gdisp.screen[sel].lcdc_status &= LCDC_TCON1_USED_MASK;
    	gdisp.screen[sel].status &= HDMI_OFF;
    	gdisp.screen[sel].output_type = DISP_OUTPUT_TYPE_NONE;
		gdisp.screen[sel].pll_use_status &= ((gdisp.screen[sel].pll_use_status == VIDEO_PLL0_USED)? VIDEO_PLL0_USED_MASK : VIDEO_PLL1_USED_MASK);

		Disp_lcdc_pin_cfg(sel, DISP_OUTPUT_TYPE_HDMI, 0);
    }

	return DIS_SUCCESS;
}

__s32 BSP_disp_hdmi_set_mode(__u32 sel, __disp_tv_mode_t  mode)
{ 	
    if(mode > DISP_TV_MOD_1080P_60HZ)
    {
        DE_WRN("unsupported hdmi mode:%d in BSP_disp_hdmi_set_mode\n", mode);
        return DIS_FAIL;
    }

	if(gdisp.init_para.hdmi_set_mode)
	{
	    gdisp.init_para.hdmi_set_mode(mode);
	}
	else
	{
	    DE_WRN("gdisp.init_para.hdmi_set_mode is NULL\n");
	    return -1;
	}

	gdisp.screen[sel].hdmi_mode = mode;
	gdisp.screen[sel].output_type = DISP_OUTPUT_TYPE_HDMI;

	return DIS_SUCCESS;
}

__s32 BSP_disp_hdmi_get_mode(__u32 sel)
{   
    return gdisp.screen[sel].hdmi_mode;
}

__s32 BSP_disp_hdmi_check_support_mode(__u32 sel, __u8  mode)
{ 
	__s32          ret = 0;
	
	if(gdisp.init_para.hdmi_mode_support)
	{
	    ret = gdisp.init_para.hdmi_mode_support(mode);
	}
	else
	{
	    DE_WRN("gdisp.init_para.hdmi_set_mode is NULL\n");
	    return -1;
	}

	return ret;
}

__s32 BSP_disp_hdmi_get_hpd_status(__u32 sel)
{
	__s32          ret = 0;

	if(gdisp.init_para.hdmi_get_HPD_status)
	{
	    ret = gdisp.init_para.hdmi_get_HPD_status();
	}
	else
	{
	    DE_WRN("gdisp.init_para.hdmi_set_mode is NULL\n");
	    return -1;
	}

	return ret;
}

__s32 BSP_disp_hdmi_set_src(__u32 sel, __disp_lcdc_src_t src)
{
    switch (src)
    {
        case DISP_LCDC_SRC_DE_CH1:
            TCON1_select_src(sel, LCDC_SRC_DE1);
            break;

        case DISP_LCDC_SRC_DE_CH2:
            TCON1_select_src(sel, LCDC_SRC_DE2);
            break;
            
        case DISP_LCDC_SRC_BLUT:
            TCON1_select_src(sel, LCDC_SRC_BLUE);
            break;

        default:
            DE_WRN("not supported lcdc src:%d in BSP_disp_tv_set_src\n", src);
            return DIS_NOT_SUPPORT;
    }
    return DIS_SUCCESS;
}

#include "rtthread.h"
#include "bf0_hal.h"
#include "drv_io.h"
#include "littlevgl2rtt.h"
#include "lv_canvas.h"
#include "stdlib.h"
#include <math.h>
#include "string.h"
#include "tgx_c.h"

// 屏幕尺寸
#define DISP_HOR_RES 240
#define DISP_VER_RES 240

// 全局画布对象和缓冲区
static lv_obj_t *canvas = NULL;
static uint8_t *frame_buffer = NULL;

// 雪花屏刷新回调
static void snow_effect_cb(lv_timer_t *timer)
{
    // 获取画布缓冲区指针 (RGB565格式)
    uint16_t *buf = (uint16_t *)getFrame();
    const size_t total_pixels = DISP_HOR_RES * DISP_VER_RES;


    tgx_next();


    // 标记整个区域需要重绘
    lv_obj_invalidate(canvas);
}

// 创建雪花屏
void create_snow_effect(void)
{
    // 创建画布对象
    canvas = lv_canvas_create(lv_scr_act());

    // 创建帧缓冲区 (RGB565格式: 每个像素2字节)
    // frame_buffer = (uint8_t *)lv_malloc(DISP_HOR_RES * DISP_VER_RES * 2);

    // 配置画布缓冲区
    lv_canvas_set_buffer(
        canvas,
        getFrame(),
        DISP_HOR_RES,
        DISP_VER_RES,
        LV_COLOR_FORMAT_RGB565);

    // 设置画布全屏显示
    lv_obj_set_size(canvas, DISP_HOR_RES, DISP_VER_RES);
    lv_obj_align(canvas, LV_ALIGN_TOP_LEFT, 0, 0);

    tgx_setup();

    // 创建定时器刷新雪花效果 (30FPS)
    lv_timer_create(snow_effect_cb, 17, NULL); // 33ms ≈ 30FPS
}

/**
 * @brief  Main program
 * @param  None
 * @retval 0 if success, otherwise failure number
 */
int main(void)
{

    rt_err_t ret = RT_EOK;
    rt_uint32_t ms;
    {
        static PMU_ChgHandleTypeDef sifli_charge_handle;

        #define CHARGE_DEFAULT_CV_VCTRL     (0x27)
        #define CHARGE_DEFAULT_BANDGAP_VREF (0x7)
        #define CHARGE_DEFAULT_CC_MN        (0x8)
        #define CHARGE_DEFAULT_CC_MP        (0x14)
        #define CHARGE_DEFAULT_FULL_VOLTAGE (4200)     /*mv*/
        #define CHARGE_DEFAULT_REP_VOLTAGE  (CHARGE_DEFAULT_FULL_VOLTAGE - 135 )     /*mv*/
        #define CHARGE_DEFAULT_VBAT_HIGH_VOLTAGE (4800)     /*mv*/
        #define CHARGE_CC_CURRENT_LEVEL0   (65)
        #define CHARGE_CC_CURRENT_LEVEL1   (300)
        PMU_ChgCalParamTypeDef cal_param;
        FACTORY_CFG_CHARGER_T cfg;
        int len = sizeof(FACTORY_CFG_CHARGER_T);
    
        cal_param.cc_mn = CHARGE_DEFAULT_CC_MN;
        cal_param.cc_mp = CHARGE_DEFAULT_CC_MP;
        cal_param.cv_vctrl = CHARGE_DEFAULT_CV_VCTRL;
        cal_param.bg = CHARGE_DEFAULT_BANDGAP_VREF;
        cal_param.rep_vctrl = CHARGE_DEFAULT_CV_VCTRL - 7;
    
        if (BSP_CONFIG_get(FACTORY_CFG_ID_CHARGER, (uint8_t *)&cfg, len))
        {
            cal_param.cc_mn = cfg.cc_mn;
            cal_param.cc_mp = cfg.cc_mp;
            cal_param.cv_vctrl = cfg.cv_vctrl;
            cal_param.bg = cfg.prog_v1p2;
            cal_param.rep_vctrl = cfg.cv_vctrl - 7;
            rt_kprintf("cal data cc_mn:%d cc_mp:%d cv_vctrl:%d prog_vlp2:%d\n", cfg.cc_mn, cfg.cc_mp, cfg.cv_vctrl, cfg.prog_v1p2);
        }
        HAL_PMU_ChgInit(&sifli_charge_handle, &cal_param);
        //  HAL_HPAON_EnableWakeupSrc(HPAON_WAKEUP_SRC_PMUC, AON_PIN_MODE_DOUBLE_EDGE);
        HAL_PMU_EnableChgWakeup();
        HAL_PMU_ChgConfigTargetVolt(&sifli_charge_handle, CHARGE_DEFAULT_FULL_VOLTAGE);
        HAL_PMU_ChgConfigRepVolt(&sifli_charge_handle, CHARGE_DEFAULT_REP_VOLTAGE);
        HAL_PMU_ChgConfigVbatHighVolt(&sifli_charge_handle, CHARGE_DEFAULT_VBAT_HIGH_VOLTAGE);
        HAL_PMU_ChgConfigCcCurrent(&sifli_charge_handle, 300);
    }

    /* init littlevGL */
    ret = littlevgl2rtt_init("lcd");
    if (ret != RT_EOK)
    {
        return ret;
    }
    
    // lv_demo_main();
    create_snow_effect();

    HAL_PIN_Set(PAD_PA01, GPIO_A1, PIN_NOPULL, 1);
    BSP_GPIO_Set(1, 1, 1);
   
    while (1)
    {
        ms = lv_task_handler();
        rt_thread_mdelay(ms);
    }

    return RT_EOK;
}

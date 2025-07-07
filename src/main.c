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

    HAL_PMU_ConfigPeriLdo(PMU_PERI_LDO3_3V3, true, true);

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

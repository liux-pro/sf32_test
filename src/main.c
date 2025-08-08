#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <rtthread.h>
#include <rtdbg.h>
#include "board.h"

#include "bf0_hal.h"
#include "drv_io.h"

#include "littlevgl2rtt.h"
#include "lv_canvas.h"
#include "tgx_c.h"

#include "charge.h"

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


#define ADC_DEV_NAME        "bat1"      /* ADC1的设备，在rt_hw_adc_register函数内已注册，不能随便修改 */
#define ADC_DEV_CHANNEL     7           /* ADC通道选择 vbat固定为CH8(Channel 7) */

static rt_device_t s_adc_dev; /* 定义一个rt_device设备 */
static rt_adc_cmd_read_arg_t read_arg;

void adc_example(void)
{
    rt_err_t r;

    /* 把PA28配置为模拟输入IO，并且不能使能内部上下拉 */
    //HAL_PIN_Set_Analog(PAD_PA28, 1);

    /* 找到bat1设备，如果没有打开 BSP_USING_ADC1，会找不到设备，出现死机 */
    s_adc_dev = rt_device_find(ADC_DEV_NAME);

    /* 配置采样通道为 channel 0*/
    read_arg.channel = ADC_DEV_CHANNEL;

    r = rt_adc_enable((rt_adc_device_t)s_adc_dev, read_arg.channel);

    /* 这个接口会调用 sifli_adc_control 函数,只读取一次,用户可以自行对数据进行处理 */   
    /* 读出来的数乘2.01才是最终结果，vbat引脚芯片内部内置分压电阻，sdk源码里搜adc_vbat_factor了解详情 */
    r = rt_device_control(s_adc_dev, RT_ADC_CMD_READ, &read_arg.channel);
    /* log打印的value值，单位为0.1mV，20846等于2084.6mV或者2.0846V  */
    LOG_I("adc channel:%d,value:%d",read_arg.channel,read_arg.value); /* (0.1mV), 20846 is 2084.6mV or 2.0846V */

    /* 这里演示另一种方式进行ADC采样，这个接口会调用到sifli_get_adc_value函数,会进行默认22次平均  */
    rt_uint32_t value = rt_adc_read((rt_adc_device_t)s_adc_dev, ADC_DEV_CHANNEL);
    /* log打印的value值，单位为0.1mV，20700等于2070.0mV或者2.0700V  */
    LOG_I("rt_adc_read:%d,value:%d",read_arg.channel,value); /* (0.1mV), 20700 is 2070mV or 2.070V */

    /* 采样完成后，关闭 adc */
    rt_adc_disable((rt_adc_device_t)s_adc_dev, read_arg.channel);

}


#if RT_USING_DFS
    #include "dfs_file.h"
    #include "dfs_posix.h"
#endif
#include "drv_flash.h"

/* Common functions for RT-Thread based platform -----------------------------------------------*/

#ifndef FS_REGION_START_ADDR
    #error "Need to define file system start address!"
#endif

#define FS_ROOT "root"

/**
 * @brief Mount fs.
 */
int mnt_init(void)
{
    register_mtd_device(FS_REGION_START_ADDR, FS_REGION_SIZE, FS_ROOT);
    if (dfs_mount(FS_ROOT, "/", "elm", 0, 0) == 0) // fs exist
    {
        rt_kprintf("mount fs on flash to root success\n");
    }
    else
    {
        // auto mkfs, remove it if you want to mkfs manual
        rt_kprintf("mount fs on flash to root fail\n");
        if (dfs_mkfs("elm", FS_ROOT) == 0)//Format file system
        {
            rt_kprintf("make elm fs on flash sucess, mount again\n");
            if (dfs_mount(FS_ROOT, "/", "elm", 0, 0) == 0)
                rt_kprintf("mount fs on flash success\n");
            else
                rt_kprintf("mount to fs on flash fail\n");
        }
        else
            rt_kprintf("dfs_mkfs elm flash fail\n");
    }
    return RT_EOK;
}
INIT_ENV_EXPORT(mnt_init);

/**
 * @brief  Main program
 * @param  None
 * @retval 0 if success, otherwise failure number
 */
int main(void)
{

    rt_err_t ret = RT_EOK;
    rt_uint32_t ms;

    rt_charge_enable(true);
    rt_charge_set_cc_current(300);
    rt_thread_mdelay(10);
    adc_example();

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

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

#include "bf0_ble_gap.h"
#include "bf0_sibles.h"
#include "bf0_sibles_internal.h"
#include "bf0_sibles_advertising.h"
#include "ble_connection_manager.h"

// #include "log.h"

#define APP_MAX_PER_ADV_LEN (100)

typedef struct
{
    uint8_t is_power_on;
    // Periodic configure para
    uint8_t per_adv_change_inv;
    uint8_t per_adv_len;
    // Mbox thread
    rt_mailbox_t mb_handle;

} app_env_t;

static app_env_t g_app_env;
static rt_mailbox_t g_app_mb;

static app_env_t *ble_app_get_env(void)
{
    return &g_app_env;
}

SIBLES_ADVERTISING_CONTEXT_DECLAR(g_app_advertising_context);

static uint8_t ble_app_advertising_event(uint8_t event, void *context, void *data)
{
    app_env_t *env = ble_app_get_env();

    switch (event)
    {
    case SIBLES_ADV_EVT_ADV_STARTED:
    {
        sibles_adv_evt_startted_t *evt = (sibles_adv_evt_startted_t *)data;
#if 0
        if (!env->is_bg_adv_on)
        {
            env->is_bg_adv_on = 1;
            ble_app_bg_advertising_start();
        }
#endif
        LOG_I("ADV start resutl %d, mode %d\r\n", evt->status, evt->adv_mode);
        break;
    }
    case SIBLES_ADV_EVT_ADV_STOPPED:
    {
        sibles_adv_evt_stopped_t *evt = (sibles_adv_evt_stopped_t *)data;
        LOG_I("ADV stopped reason %d, mode %d\r\n", evt->reason, evt->adv_mode);
        break;
    }
    default:
        break;
    }
    return 0;
}

#define DEFAULT_LOCAL_NAME "SIFLI_APP"
#define EXAMPLE_LOCAL_NAME "SIFLI_EXAMPLE"
uint8_t g_peri_adv[APP_MAX_PER_ADV_LEN];

/* Enable advertise via advertising service. */
static void ble_app_advertising_start(void)
{
    sibles_advertising_para_t para = {0};
    uint8_t ret;

    para.own_addr_type = GAPM_STATIC_ADDR;
    para.config.adv_mode = SIBLES_ADV_BROADCAST_MODE;
    /* Keep advertising till disable it or connected. */
    para.config.mode_config.conn_config.duration = 0x0;
    para.config.mode_config.conn_config.interval = 20000 / 625; 
    para.config.max_tx_pwr =  0x7f;
    /* Advertising will re-start after disconnected. */
    para.config.is_auto_restart = 1;
    /* Scan rsp data is same as advertising data. */
    //para.config.is_rsp_data_duplicate = 1;

    /* Prepare name filed. Due to name is too long to put adv data, put it to rsp data.*/
    char local_name[] = "SF32";
    para.adv_data.completed_name = rt_malloc(rt_strlen(local_name) + sizeof(sibles_adv_type_name_t));
    para.adv_data.completed_name->name_len = rt_strlen(local_name);
    rt_memcpy(para.adv_data.completed_name->name, local_name, para.adv_data.completed_name->name_len);

    /* Prepare manufacturer filed .*/
    uint8_t manu_additnal_data[] = {0x20, 0xC4, 0x91, 0x91};
    uint16_t manu_company_id = SIG_SIFLI_COMPANY_ID;
    para.adv_data.manufacturer_data = rt_malloc(sizeof(sibles_adv_type_manufacturer_data_t) + sizeof(manu_additnal_data));
    para.adv_data.manufacturer_data->company_id = manu_company_id;
    para.adv_data.manufacturer_data->data_len = sizeof(manu_additnal_data);
    rt_memcpy(para.adv_data.manufacturer_data->additional_data, manu_additnal_data, sizeof(manu_additnal_data));

    para.evt_handler = ble_app_advertising_event;

    ret = sibles_advertising_init(g_app_advertising_context, &para);
    if (ret == SIBLES_ADV_NO_ERR)
    {
        sibles_advertising_start(g_app_advertising_context);
    }

    rt_free(para.adv_data.completed_name);
    rt_free(para.adv_data.manufacturer_data);
}


#ifndef NVDS_AUTO_UPDATE_MAC_ADDRESS_ENABLE
ble_common_update_type_t ble_request_public_address(bd_addr_t *addr)
{
    int ret = bt_mac_addr_generate_via_uid_v2(addr);

    if (ret != 0)
    {
        LOG_W("generate mac addres failed %d", ret);
        return BLE_UPDATE_NO_UPDATE;
    }

    return BLE_UPDATE_ONCE;
}
#endif // NVDS_AUTO_UPDATE_MAC_ADDRESS_ENABLE

/**
 * @brief  Main program
 * @param  None
 * @retval 0 if success, otherwise failure number
 */
int main(void)
{
    int count = 0;
    app_env_t *env = ble_app_get_env();
    env->mb_handle = rt_mb_create("app", 8, RT_IPC_FLAG_FIFO);
    sifli_ble_enable();

    env->per_adv_len = 80;

    while (1)
    {
        uint32_t value;
        int ret;
        rt_mb_recv(env->mb_handle, (rt_uint32_t *)&value, RT_WAITING_FOREVER);
        if (value == BLE_POWER_ON_IND)
        {
            env->is_power_on = 1;

            /* First enable connectable adv then enable non-connectable. */
            ble_app_advertising_start();
            LOG_I("receive BLE power on!\r\n");
        }
    }
    return RT_EOK;
}
int ble_app_event_handler(uint16_t event_id, uint8_t *data, uint16_t len, uint32_t context)
{
    app_env_t *env = ble_app_get_env();
    switch (event_id)
    {
    case BLE_POWER_ON_IND:
    {
        /* Handle in own thread to avoid conflict */
        if (env->mb_handle)
            rt_mb_send(env->mb_handle, BLE_POWER_ON_IND);
        break;
    }
    case BLE_GAP_CREATE_PERIODIC_ADV_SYNC_CNF:
    {
        ble_gap_create_per_adv_sync_cnf_t *cnf = (ble_gap_create_per_adv_sync_cnf_t *)data;
        LOG_I("Create PER_ADV_SYNC result %d", cnf->status);
        break;
    }
    default:
        break;
    }
    return 0;
}
BLE_EVENT_REGISTER(ble_app_event_handler, NULL);
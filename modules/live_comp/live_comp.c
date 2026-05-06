#include <dryos.h>
#include <config.h>
#include <bmp.h>
#include <menu.h>
#include <module.h>
#include <raw.h>
#include <shoot.h>

int lcomp_running;
int lcomp_frame_count;

uint16_t *lcomp_buffer; 
uint32_t lcomp_buffer_size;

CONFIG_INT("lcomp.enabled", lcomp_enabled, 0);
CONFIG_INT("lcomp.exposure", lcomp_exposure, 2);  
CONFIG_INT("lcomp.iso", lcomp_iso, 100);
CONFIG_INT("lcomp.max_frames", lcomp_max_frames, 100);

static int lcomp_alloc_buffer()
{
    raw_update_params();
    uint32_t w = raw_info.jpeg.width;
    uint32_t h = raw_info.jpeg.height;
    lcomp_buffer_size = w * h * sizeof(uint16_t) * 3;
    lcomp_buffer = fio_malloc(lcomp_buffer_size);
    if (!lcomp_buffer) return 0;
    memset(lcomp_buffer, 0, lcomp_buffer_size);
    return 1;
}

static void lcomp_free_buffer()
{
    if (lcomp_buffer)
    {
        fio_free(lcomp_buffer);
        lcomp_buffer = 0;
        lcomp_buffer_size = 0;
    }
}

static int lcomp_start()
{
    if (!lcomp_alloc_buffer()) return 0;
    lcomp_running = 1;
    lcomp_frame_count = 0;
    raw_lv_request();
    msleep(100);
    raw_update_params();
    return 1;
}

static void lcomp_process_frame()
{
    if (!lcomp_running || !lcomp_buffer) return;

    uint32_t w = raw_info.jpeg.width;
    uint32_t h = raw_info.jpeg.height;
    uint32_t pixels = w * h;

    uint16_t *framebuf = (uint16_t *)(uintptr_t)raw_info.buffer;
    if (!framebuf) return;

    for (uint32_t i = 0; i < pixels * 3; i++)
    {
        if (framebuf[i] > lcomp_buffer[i])
            lcomp_buffer[i] = framebuf[i];
    }

    lcomp_frame_count++;

    bmp_printf(FONT_MED, 20, 80, "LIVE COMP: frame %d/%d",
               lcomp_frame_count, lcomp_max_frames);
    redraw();
}

static int lcomp_stop()
{
    if (!lcomp_running) return 0;
    lcomp_running = 0;
    raw_lv_release();
    lcomp_free_buffer();
    return 1;
}

static MENU_UPDATE_FUNC(lcomp_display)
{
    if (lcomp_running)
    {
        MENU_SET_VALUE("Running (%d/%d)", lcomp_frame_count, lcomp_max_frames);
    }
}

static MENU_SELECT_FUNC(lcomp_action)
{
    if (lcomp_running)
    {
        lcomp_stop();
        NotifyBox(2000, "Live Composite stopped");
    }
    else
    {
        if (lcomp_start())
            NotifyBox(2000, "Live Composite started");
        else
            NotifyBox(2000, "Memory allocation failed");
    }
}

static struct menu_entry lcomp_menus[] = {
    {
        .name = "Live Composite",
        .priv = &lcomp_enabled,
        .max = 1,
        .update = lcomp_display,
        .select = lcomp_action,
        .help = "Start/stop live composite exposure stacking.",
        .children = (struct menu_entry[]) {
            {
                .name = "Exposure (sec)",
                .priv = &lcomp_exposure,
                .min = 1,
                .max = 60,
                .help = "Per-frame exposure time in seconds.",
            },
            {
                .name = "ISO",
                .priv = &lcomp_iso,
                .min = 100,
                .max = 6400,
                .help = "ISO for each frame.",
            },
            {
                .name = "Max frames",
                .priv = &lcomp_max_frames,
                .min = 1,
                .max = 999,
                .help = "Maximum number of frames to composite.",
            },
            MENU_EOL
        }
    },
    MENU_EOL
};

static void lcomp_task()
{
    while (lcomp_running)
    {
        msleep(lcomp_exposure * 1000);
        raw_update_params();
        lcomp_process_frame();

        if (lcomp_frame_count >= lcomp_max_frames)
        {
            lcomp_stop();
            NotifyBox(2000, "Live Composite complete");
            break;
        }
    }
}

static unsigned int lcomp_init()
{
    if (!lcomp_enabled) return 0;
    task_create("lcomp_task", 0x1c, 0x2000, lcomp_task, 0);
    menu_add("Expo", lcomp_menus, COUNT(lcomp_menus));
    return 0;
}

static unsigned int lcomp_deinit()
{
    lcomp_stop();
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(lcomp_init)
    MODULE_DEINIT(lcomp_deinit)
    MODULE_LONGNAME("Live Composite")
MODULE_INFO_END()

MODULE_CONFIGS_START()
    MODULE_CONFIG(lcomp_enabled)
    MODULE_CONFIG(lcomp_exposure)
    MODULE_CONFIG(lcomp_iso)
    MODULE_CONFIG(lcomp_max_frames)
MODULE_CONFIGS_END()

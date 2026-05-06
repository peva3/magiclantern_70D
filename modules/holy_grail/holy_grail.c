#include <dryos.h>
#include <config.h>
#include <bmp.h>
#include <menu.h>
#include <module.h>
#include <property.h>
#include <propvalues.h>
#include <shoot.h>

int hg_enabled;
int hg_total_frames;
int hg_interval;
int hg_iso_start;
int hg_iso_end;
int hg_shutter_start;
int hg_shutter_end;
int hg_aperture_start;
int hg_aperture_end;

int hg_running;
int hg_frame;

CONFIG_INT("hg.enabled", hg_enabled, 0);
CONFIG_INT("hg.total_frames", hg_total_frames, 200);
CONFIG_INT("hg.interval", hg_interval, 30);
CONFIG_INT("hg.iso_start", hg_iso_start, 100);
CONFIG_INT("hg.iso_end", hg_iso_end, 6400);
CONFIG_INT("hg.shutter_start", hg_shutter_start, 0);
CONFIG_INT("hg.shutter_end", hg_shutter_end, 15);
CONFIG_INT("hg.aperture_start", hg_aperture_start, 0);
CONFIG_INT("hg.aperture_end", hg_aperture_end, 0);

static int hg_start()
{
    if (hg_running) return 0;
    hg_running = 1;
    hg_frame = 0;
    NotifyBox(2000, "Holy Grail started");
    return 1;
}

static int hg_stop()
{
    if (!hg_running) return 0;
    hg_running = 0;
    NotifyBox(2000, "Holy Grail stopped");
    return 1;
}

static int lerp(int a, int b, int frame, int total)
{
    if (total <= 1) return a;
    return a + (b - a) * frame / (total - 1);
}

static int shutter_index_to_ms(int idx)
{
    int tbl[] = {0, 1, 2, 4, 8, 15, 30, 60, 125, 250, 500, 1000, 2000, 4000, 8000};
    if (idx < 0 || idx >= 15) return 125;
    return tbl[idx];
}

static void hg_apply_settings()
{
    int iso = lerp(hg_iso_start, hg_iso_end, hg_frame, hg_total_frames);
    int shutter = lerp(hg_shutter_start, hg_shutter_end, hg_frame, hg_total_frames);

    prop_request_change(PROP_ISO, &iso, 4);
    msleep(50);

    int shutter_ms = shutter_index_to_ms(shutter);
    prop_request_change(PROP_SHUTTER, &shutter_ms, 4);
    msleep(50);
}

static void hg_task()
{
    while (hg_running)
    {
        hg_apply_settings();
        msleep(500);

        call("shoot");

        hg_frame++;

        bmp_printf(FONT_MED, 20, 40,
            "HOLY GRAIL: %d/%d ISO:%d->%d",
            hg_frame, hg_total_frames,
            lerp(hg_iso_start, hg_iso_end, hg_frame, hg_total_frames),
            lerp(hg_iso_start, hg_iso_end, hg_total_frames, hg_total_frames));
        redraw();

        if (hg_frame >= hg_total_frames)
        {
            hg_stop();
            NotifyBox(2000, "Holy Grail complete");
            break;
        }

        msleep((hg_interval - 1) * 1000);
    }
}

static MENU_UPDATE_FUNC(hg_display)
{
    if (hg_running)
        MENU_SET_VALUE("Running (%d/%d)", hg_frame, hg_total_frames);
}

static MENU_SELECT_FUNC(hg_action)
{
    if (hg_running)
        hg_stop();
    else
        hg_start();
}

static struct menu_entry hg_menus[] = {
    {
        .name = "Holy Grail",
        .priv = &hg_enabled,
        .max = 1,
        .update = hg_display,
        .select = hg_action,
        .help = "Day-to-night timelapse with smooth exposure ramping.",
        .children = (struct menu_entry[]) {
            {
                .name = "Total frames",
                .priv = &hg_total_frames,
                .min = 10,
                .max = 999,
                .help = "Number of frames to capture.",
            },
            {
                .name = "Interval (sec)",
                .priv = &hg_interval,
                .min = 2,
                .max = 3600,
                .help = "Seconds between frames.",
            },
            {
                .name = "ISO start",
                .priv = &hg_iso_start,
                .min = 100,
                .max = 25600,
                .help = "ISO for the first frame.",
            },
            {
                .name = "ISO end",
                .priv = &hg_iso_end,
                .min = 100,
                .max = 25600,
                .help = "ISO for the last frame (ramps smoothly).",
            },
            {
                .name = "Shutter start idx",
                .priv = &hg_shutter_start,
                .min = 0,
                .max = 14,
                .help = "0=bulb,1=1s,2=1/2,3=1/4,4=1/8,5=1/15,6=1/30,7=1/60,8=1/125,9=1/250,10=1/500,11=1/1000,12=1/2000,13=1/4000,14=1/8000",
            },
            {
                .name = "Shutter end idx",
                .priv = &hg_shutter_end,
                .min = 0,
                .max = 14,
                .help = "Shutter at end frame (smooth ramp).",
            },
            MENU_EOL
        }
    },
    MENU_EOL
};

static unsigned int hg_init()
{
    if (!hg_enabled) return 0;
    task_create("hg_task", 0x1c, 0x2000, hg_task, 0);
    menu_add("Shoot", hg_menus, COUNT(hg_menus));
    return 0;
}

static unsigned int hg_deinit()
{
    hg_stop();
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(hg_init)
    MODULE_DEINIT(hg_deinit)
    MODULE_LONGNAME("Holy Grail Timelapse")
MODULE_INFO_END()

MODULE_CONFIGS_START()
    MODULE_CONFIG(hg_enabled)
    MODULE_CONFIG(hg_total_frames)
    MODULE_CONFIG(hg_interval)
    MODULE_CONFIG(hg_iso_start)
    MODULE_CONFIG(hg_iso_end)
    MODULE_CONFIG(hg_shutter_start)
    MODULE_CONFIG(hg_shutter_end)
    MODULE_CONFIG(hg_aperture_start)
    MODULE_CONFIG(hg_aperture_end)
MODULE_CONFIGS_END()

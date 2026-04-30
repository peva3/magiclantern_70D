#include <module.h>
#include <dryos.h>
#include <bmp.h>
#include <stdio.h>
#include <string.h>
#include <config.h>

static void gps_probe(void)
{
    bmp_fill(COLOR_BLACK, 0, 0, 720, 480);
    bmp_printf(FONT_LARGE, 20, 30, "GPS Probe (70D)");
    msleep(1000);

    int y = 80;
    int line_h = 20;

    #define TRY_CALL(name, ...) do { \
        bmp_printf(FONT_MED, 20, y, "call(\"" name "\")..."); \
        int r = call(name, ##__VA_ARGS__); \
        bmp_printf(FONT_MED, 300, y, " -> %d", r); \
        y += line_h; \
        msleep(200); \
    } while(0)

    bmp_printf(FONT_MED, 20, y, "--- GPS Initialization ---");
    y += line_h;

    TRY_CALL("GPS_Initialize");

    bmp_printf(FONT_MED, 20, y, "--- GPS Data ---");
    y += line_h;

    TRY_CALL("GetGPSTime");
    TRY_CALL("GetGPSListRecvCapability");
    TRY_CALL("GetGPSCaptureTimeList");

    bmp_printf(FONT_MED, 20, y, "--- GPS Control ---");
    y += line_h;

    TRY_CALL("GPSList");
    TRY_CALL("GPSClearList");

    bmp_printf(FONT_MED, 20, y, "--- WiFi (needed for GPS via phone) ---");
    y += line_h;

    TRY_CALL("NwLimeInit");
    TRY_CALL("NwLimeOn");

    y += line_h;
    bmp_printf(FONT_LARGE, 20, y, "Probe complete. See results above.");
    y += line_h;
    bmp_printf(FONT_SMALL, 20, y, "call() returns 0=OK, <0=error, >0=depends on function.");

    msleep(15000);
    bmp_off();
}

static unsigned int gps_init(void)
{
    if (config_flag_file_setting_load("ML/SETTINGS/GPS.RUN"))
    {
        gps_probe();
        config_flag_file_setting_save("ML/SETTINGS/GPS.RUN", 0);
    }
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(gps_init)
MODULE_INFO_END()

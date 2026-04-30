#include <module.h>
#include <dryos.h>
#include <bmp.h>
#include <stdio.h>
#include <string.h>
#include <config.h>

#define ISOS 7
#define COLS 2

static int rows[ISOS][COLS];

static void diprobe_auto(void)
{
    uint32_t photo_cmos0  = 0x404e5664;
    uint32_t photo_cmos3  = 0x404e5667;
    uint32_t movie_cmos0  = 0x404e77d6;
    uint32_t movie_cmos3  = 0x404e77d9;
    int stride_ph = 20;
    int stride_mv = 46;

    bmp_fill(COLOR_BLACK, 0, 0, 720, 480);

    #define DUMP(name, base, stride) do { \
        bmp_printf(FONT_MED, 0, y, name); \
        y += 16; \
        for (int i = 0; i < ISOS; i++) { \
            uint32_t addr = base + i * stride; \
            uint32_t val = MEM(addr); \
            bmp_printf(FONT_MED, 0, y, "  ISO[%d] 0x%08x = 0x%04x", i, addr, val); \
            y += 14; \
        } \
        y += 6; \
    } while(0)

    int y = 20;
    bmp_printf(FONT_LARGE, 0, 0, "Dual ISO Probe: CMOS[0] vs CMOS[3]");
    y += 20;

    bmp_printf(FONT_MED, 0, y, "--- Photo CMOS[0] (stride %d) ---", stride_ph);
    y += 16;
    for (int i = 0; i < ISOS; i++) {
        uint32_t addr = photo_cmos0 + i * stride_ph;
        uint32_t val = MEM(addr);
        rows[i][0] = val;
        bmp_printf(FONT_MED, 0, y, "  [%d] 0x%08x = 0x%04x (%sd)", i, addr, val, val & 3 ? "flag=" : "");
        y += 14;
    }
    y += 8;

    bmp_printf(FONT_MED, 0, y, "--- Photo CMOS[3] ---");
    y += 16;
    for (int i = 0; i < ISOS; i++) {
        uint32_t addr = photo_cmos3 + i * stride_ph;
        uint32_t val = MEM(addr);
        rows[i][1] = val;
        bmp_printf(FONT_MED, 0, y, "  [%d] 0x%08x = 0x%04x", i, addr, val);
        y += 14;
    }
    y += 8;

    bmp_printf(FONT_MED, 0, y, "--- Movie CMOS[0] (stride %d) ---", stride_mv);
    y += 16;
    for (int i = 0; i < ISOS; i++) {
        uint32_t addr = movie_cmos0 + i * stride_mv;
        uint32_t val = MEM(addr);
        bmp_printf(FONT_MED, 0, y, "  [%d] 0x%08x = 0x%04x", i, addr, val);
        y += 14;
    }
    y += 8;

    bmp_printf(FONT_MED, 0, y, "--- Movie CMOS[3] ---");
    y += 16;
    for (int i = 0; i < ISOS; i++) {
        uint32_t addr = movie_cmos3 + i * stride_mv;
        uint32_t val = MEM(addr);
        bmp_printf(FONT_MED, 0, y, "  [%d] 0x%08x = 0x%04x", i, addr, val);
        y += 14;
    }

    bmp_printf(FONT_SMALL, 0, y+10, "Expected photo pattern: 3 27 4b 6f 93 b7 db");
    bmp_printf(FONT_SMALL, 0, y+22, "If movie differs, CMOS[3] may be correct table");

    msleep(30000);
    bmp_off();
}

static unsigned int diprobe_init(void)
{
    if (config_flag_file_setting_load("ML/SETTINGS/DIPROBE.RUN"))
    {
        diprobe_auto();
        config_flag_file_setting_save("ML/SETTINGS/DIPROBE.RUN", 0);
    }
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(diprobe_init)
MODULE_INFO_END()

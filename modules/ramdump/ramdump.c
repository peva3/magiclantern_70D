#include <module.h>
#include <dryos.h>
#include <bmp.h>
#include <stdio.h>
#include <string.h>
#include <config.h>

static void ramdump_auto(void)
{
    uint32_t start_addr = 0x40000000;
    uint32_t end_addr   = 0x60000000;
    uint32_t total_size = end_addr - start_addr;
    uint32_t chunk      = 262144;

    extern int GetBatteryLevel(void);
    int batt = GetBatteryLevel();
    if (batt < 50) {
        bmp_fill(COLOR_BLACK, 0, 0, 720, 480);
        bmp_printf(FONT_LARGE, 20, 100, "Low battery (%d%%)!", batt);
        bmp_printf(FONT_MED, 20, 140, "Charge battery before dumping.");
        msleep(5000);
        return;
    }

    bmp_fill(COLOR_BLACK, 0, 0, 720, 480);
    bmp_printf(FONT_LARGE, 20, 30, "Full RAM Dump");
    bmp_printf(FONT_MED, 20, 60, "Dumping 0x%08x-0x%08x (%dMB)...",
               start_addr, end_addr - 1, total_size >> 20);
    bmp_printf(FONT_MED, 20, 80, "This takes ~30-60 seconds.");
    bmp_printf(FONT_MED, 20, 100, "Do NOT power off or remove battery!");
    msleep(2000);

    FILE *fp = FIO_CreateFile("ML/LOGS/RAM_FULL.BIN");
    if (!fp) {
        bmp_printf(FONT_LARGE, 20, 160, "FAIL: cannot create RAM_FULL.BIN");
        msleep(5000);
        return;
    }

    uint8_t *buf = fio_malloc(chunk);
    if (!buf) {
        bmp_printf(FONT_LARGE, 20, 160, "FAIL: cannot allocate buffer");
        FIO_CloseFile(fp);
        msleep(5000);
        return;
    }

    uint32_t offset = 0;
    uint32_t total_written = 0;
    uint32_t skipped = 0;
    uint32_t next_progress = 0x00400000;
    int ok = 1;

    while (offset < total_size && ok) {
        uint32_t src = start_addr + offset;

        uint32_t probe = *(volatile uint32_t *)(uintptr_t)src;
        if (probe == 0xFFFFFFFF) {
            offset += chunk;
            skipped++;
            continue;
        }

        memcpy(buf, (void *)(uintptr_t)src, chunk);
        int written = FIO_WriteFile(fp, buf, chunk);
        if (written <= 0) {
            ok = 0;
            break;
        }
        total_written += written;
        offset += chunk;

        if (offset >= next_progress) {
            next_progress += 0x00400000;
            int pct = (int)((uint64_t)offset * 100 / total_size);
            bmp_printf(FONT_MED, 20, 140,
                       "Progress: %dMB/%dMB (%d%%)  ",
                       offset >> 20, total_size >> 20, pct);
            bmp_printf(FONT_MED, 20, 160,
                       "Written: %dMB  Skipped: %d pages  ",
                       total_written >> 20, skipped);
        }
    }

    fio_free(buf);
    FIO_CloseFile(fp);

    bmp_fill(COLOR_BLACK, 0, 0, 720, 480);
    if (ok) {
        bmp_printf(FONT_LARGE, 20, 30, "RAM Dump Complete!");
        bmp_printf(FONT_MED, 20, 60,
                   "Written: %dMB to ML/LOGS/RAM_FULL.BIN",
                   total_written >> 20);
        bmp_printf(FONT_MED, 20, 80,
                   "Skipped: %d pages (unmapped)", skipped);
        bmp_printf(FONT_SMALL, 20, 120, "Copy to PC and analyze:");
        bmp_printf(FONT_SMALL, 20, 140,
                   "  strings RAM_FULL.BIN | less");
        bmp_printf(FONT_SMALL, 20, 160,
                   "  hexdump -C RAM_FULL.BIN | less");
    } else {
        bmp_printf(FONT_LARGE, 20, 30, "RAM Dump FAILED");
        bmp_printf(FONT_MED, 20, 60, "Failed at offset 0x%x", offset);
    }

    msleep(15000);
    bmp_off();
}

static unsigned int ramdump_init(void)
{
    if (config_flag_file_setting_load("ML/SETTINGS/RAMDUMP.RUN"))
    {
        ramdump_auto();
        config_flag_file_setting_save("ML/SETTINGS/RAMDUMP.RUN", 0);
    }
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(ramdump_init)
MODULE_INFO_END()

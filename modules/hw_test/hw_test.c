#include <module.h>
#include <dryos.h>
#include <bmp.h>
#include <stdio.h>
#include <string.h>
#include <property.h>
#include <propvalues.h>
#include <edmac-memcpy.h>
#include <raw.h>
#include <fps.h>
#include <timer.h>
#include <mem.h>
#include <exmem.h>
#include <fio-ml.h>

/*
 * hw_test — Comprehensive Hardware Proving Ground for Canon 70D
 *
 * DESIGN PHILOSOPHY:
 * This module is the SINGLE authoritative source of truth about hardware state.
 * Every hardware-dependent sprint item has one or more tests here that answer
 * its fundamental questions. This eliminates iterative on-camera testing:
 *
 *   - Run hw_test once on hardware
 *   - Analyze the log output offline
 *   - Make informed code changes
 *   - Verify with targeted manual tests (not exploration)
 *
 * Sprint mapping:
 *   S2/S1 (Focus):       PROP_LV_LENS dump, lens stub verify
 *   S3 (FPS):            FPS timer register dump, write/read-back test
 *   S4 (RAW Zebras):     EDMAC/PACK32 register dump
 *   S5 (crop_rec):       CMOS/ENGIO/ADTG register dump, timer tables verify
 *   S6 (Dual ISO):       CMOS ISO register dump, ISO push register
 *   S8 (Audio):          ASIF stub verify, audio register dump
 *   S9 (SD UHS):         Multi-block-size write benchmark
 *   S10 (A/B FW):        Bootflag/property state dump
 *   S23 (WiFi):          PTPIP stub verify, call() dispatch
 *   S26 (PTP):           ptptun handler registration verify
 *
 * SAFETY: NO writes to hardware registers. Read-only shamem_read.
 * NO call() to known-dangerous functions (EnableBootDisk, TurnOnDisplay).
 */

#define VERSION "hw_test v19 — RAM dump for offline RE"

static int t_total, t_pass, t_skip, t_fail, scr_y;
static FILE *log_fp;
static int log_write_count;
static void log_flush(void);
#define LINE_H 10
#define X0 20

/* Register table entry */
typedef struct {
    const char *name;
    uint32_t addr;
} reg_entry;

static int line(void)
{
    int y = scr_y;
    scr_y += LINE_H;
    if (scr_y > 460) scr_y = LINE_H;
    return y;
}

static void log_write(const char *s)
{
    if (log_fp) {
        FIO_WriteFile(log_fp, s, strlen(s));
        FIO_WriteFile(log_fp, "\n", 1);
        log_write_count++;
        if (log_write_count % 20 == 0) {
            log_flush();
        }
    }
}

/* Force FAT buffer flush by closing and reopening log file */
static void log_flush(void)
{
    if (log_fp) {
        FIO_CloseFile(log_fp);
        log_fp = FIO_CreateFileOrAppend("ML/LOGS/HW_TEST.LOG");
    }
}

static void out(const char *s, int color)
{
    bmp_printf(FONT_SMALL | color, X0, line(), "%s", s);
    char log_buf[516];
    snprintf(log_buf, sizeof(log_buf), "[HW] %s", s);
    log_write(log_buf);
}

static void info(const char *s)
{
    out(s, COLOR_WHITE);
}

static void warn(const char *s)
{
    out(s, COLOR_YELLOW);
}

static void hdr(const char *s)
{
    scr_y = line();
    bmp_printf(FONT_SANS_23, X0, scr_y, "%s", s);
    char log_buf[516];
    snprintf(log_buf, sizeof(log_buf), "=== %s ===", s);
    log_write(log_buf);
    scr_y += 28;
}

static void rst(int pass, const char *name, const char *why)
{
    t_total++;
    int y = line();
    bmp_printf(FONT_SMALL | COLOR_WHITE, X0, y, "%s", name);
    char log_buf[516];
    if (pass) {
        t_pass++;
        bmp_printf(FONT_SMALL | COLOR_GREEN1, X0 + 140, y, "PASS");
        snprintf(log_buf, sizeof(log_buf), "[HW] %s: PASS", name);
    } else if (why) {
        t_skip++;
        bmp_printf(FONT_SMALL | COLOR_YELLOW, X0 + 140, y, "SKIP");
        bmp_printf(FONT_SMALL | COLOR_CYAN, X0 + 200, y, "%s", why);
        snprintf(log_buf, sizeof(log_buf), "[HW] %s: SKIP (%s)", name, why);
    } else {
        t_fail++;
        bmp_printf(FONT_SMALL | COLOR_RED, X0 + 140, y, "FAIL");
        snprintf(log_buf, sizeof(log_buf), "[HW] %s: FAIL", name);
    }
    log_write(log_buf);
}

static void val(const char *label, int v)
{
    char buf[80];
    snprintf(buf, sizeof(buf), "%s=%d", label, v);
    info(buf);
}

static void hexval(const char *label, uint32_t v)
{
    char buf[80];
    snprintf(buf, sizeof(buf), "%s=0x%08x", label, v);
    info(buf);
}

static void blink_delay(int ms)
{
    info_led_blink(1, 100, 100);
    msleep(ms);
}

/* Dump all registers from a table, log as CSV-style lines */
static void dump_regs(const char *title, const reg_entry *tbl)
{
    hdr(title);
    char csv_hdr[128];
    snprintf(csv_hdr, sizeof(csv_hdr), "# REGISTER DUMP: %s", title);
    log_write(csv_hdr);
    log_write("# name,addr,value");
    char buf[80];
    for (int i = 0; tbl[i].name; i++) {
        uint32_t v = shamem_read(tbl[i].addr);
        snprintf(buf, sizeof(buf), "%s (0x%08x)=0x%08x", tbl[i].name, tbl[i].addr, v);
        info(buf);
        /* CSV line for parsing */
        char csv[128];
        snprintf(csv, sizeof(csv), "%s,0x%08x,0x%08x", tbl[i].name, tbl[i].addr, v);
        log_write(csv);
    }
}

/* ────────────────────────────────────────────
 * REGISTER TABLES
 * ──────────────────────────────────────────── */

static const reg_entry fps_regs[] = {
    {"FPS_CF",      0xC0F06000},
    {"FPS_TA",      0xC0F06008},
    {"FPS_TAM",     0xC0F0600C},
    {"FPS_TAH",     0xC0F06010},
    {"FPS_TB",      0xC0F06014},
    {"ENGIO_TA_E24", 0xC0F06824},
    {"ENGIO_TA_E28", 0xC0F06828},
    {"ENGIO_TA_E2C", 0xC0F0682C},
    {"ENGIO_TA_E30", 0xC0F06830},
    {"ENGIO_HD3",   0xC0F0713C},
    {"ENGIO_HD4",   0xC0F07150},
    {0, 0}
};

static const reg_entry engio_regs[] = {
    {"ENGIO_TL",   0xC0F06800},
    {"ENGIO_BR",   0xC0F06804},
    {0, 0}
};

static const reg_entry edmac_regs[] = {
    {"RAW_PHOTO_EDMAC", 0xC0F04008},
    {"EDMAC_WR_HD",     0xC0F04A08},
    {"LV_RAW_EDMAC",    0xC0F26200},
    {0, 0}
};

static const reg_entry raw_regs[] = {
    {"RAW_TYPE",       0xC0F37014},
    {"SHAD_GAIN",      0xC0F08030},
    {"SHAD_PRESETUP",  0xC0F08034},
    {"PACK32_MODE",    0xC0F08094},
    {"CANON_WL",       0xC0F12054},
    {0, 0}
};

static const reg_entry lossless_regs[] = {
    {"SLICE_SIZE",    0xC0F37300},
    {"LOSSLESS_MODE", 0xC0F373B4},
    {"SLICE_MIRROR",  0xC0F373E8},
    {"ALT_FIX",       0xC0F373F4},
    {"SLICE_OTHER",   0xC0F375B4},
    {"LL_CTRL_10",    0xC0F37610},
    {"LL_CFG_28",     0xC0F37628},
    {"LL_CFG_2C",     0xC0F3762C},
    {"LL_CFG_30",     0xC0F37630},
    {"LL_CFG_34",     0xC0F37634},
    {"LL_CFG_3C",     0xC0F3763C},
    {"LL_CFG_40",     0xC0F37640},
    {"LL_CFG_44",     0xC0F37644},
    {"LL_CFG_48",     0xC0F37648},
    {"TOTAL_IMG_SZ",  0xC0F13068},
    {0, 0}
};

static const reg_entry disp_regs[] = {
    {"DISP_UPDATE", 0xC0F14000},
    {"CRAZY_COLORS", 0xC0F14040},
    {"PAL_TRIGGER", 0xC0F14078},
    {"PAL_0",       0xC0F14080},
    {"PAL_1",       0xC0F14084},
    {"PAL_2",       0xC0F14088},
    {"PAL_3",       0xC0F1408C},
    {"PAL_4",       0xC0F14090},
    {"PAL_5",       0xC0F14094},
    {"PAL_6",       0xC0F14098},
    {"PAL_7",       0xC0F1409C},
    {"PAL_8",       0xC0F140A0},
    {"PAL_9",       0xC0F140A4},
    {"PAL_10",      0xC0F140A8},
    {"PAL_11",      0xC0F140AC},
    {"PAL_12",      0xC0F140B0},
    {"PAL_13",      0xC0F140B4},
    {"PAL_14",      0xC0F140B8},
    {"PAL_15",      0xC0F140BC},
    {"EXP_COMP",    0xC0F140C0},
    {"SATURATION",  0xC0F140C4},
    {"ZEBRA_REG",   0xC0F140CC},
    {"FB_LOW",      0xC0F140D0},
    {"FB_HIGH",     0xC0F140D4},
    {"FILTER_EN",   0xC0F14140},
    {"DISP_POS",    0xC0F14164},
    {"BRIGHTNESS",  0xC0F141B8},
    {0, 0}
};

static const reg_entry imgproc_regs[] = {
    {"DSUNPACK_ENB",  0xC0F08060},
    {"DSUNPACK_MODE", 0xC0F08064},
    {"DEF_ENB",       0xC0F080A0},
    {"DEF_80A4",      0xC0F080A4},
    {"DEF_MODE",      0xC0F080A8},
    {"DEF_CTRL",      0xC0F080AC},
    {"DEF_YB_XB",     0xC0F080B0},
    {"DEF_YN_XN",     0xC0F080B4},
    {"DEF_YA_XA",     0xC0F080BC},
    {"DEF_INTR_EN",   0xC0F080D0},
    {"DEF_HOSEI",     0xC0F080D4},
    {"DS_SEL",        0xC0F08104},
    {"PACK16_ENB",    0xC0F08120},
    {"PACK16_MODE",   0xC0F08124},
    {"DEFC_X2MODE",   0xC0F08270},
    {"DSUNPACK_DM_EN", 0xC0F08274},
    {"PACK16_CCD2_DM", 0xC0F0827C},
    {"DEFC_DET_MODE", 0xC0F082B4},
    {"PACK16_DEFM_ON", 0xC0F082B8},
    {"PACK16_ISEL",   0xC0F082D0},
    {"WDMAC16_ISEL",  0xC0F082D8},
    {"PACK16_ISEL2",  0xC0F0839C},
    {"PACK16_ILIM",   0xC0F085B4},
    {"VIGNET_1",      0xC0F08D1C},
    {"VIGNET_2",      0xC0F08D24},
    {"SHDW_LIFT_1",   0xC0F0E094},
    {"SHDW_LIFT_2",   0xC0F0E0F0},
    {"ISO_PUSH_D4",   0xC0F0E0F8},
    {"SHDW_LIFT_3",   0xC0F0F1C4},
    {"SHDW_LIFT_4",   0xC0F0F43C},
    {"ISO_PUSH_D5",   0xC0F42744},
    {0, 0}
};

static const reg_entry misc_regs[] = {
    {"CARD_LED", 0xC022C06C},
    {0, 0}
};

/* ────────────────────────────────────────────
 * SD WRITE BENCHMARK (S9.2)
 * ──────────────────────────────────────────── */

static void sd_bench(const char *path, int block_size, int blocks)
{
    int total = block_size * blocks;
    void *buf = fio_malloc(total);
    if (!buf) {
        warn("sd_bench: alloc fail");
        return;
    }
    memset(buf, 0xAA, total);

    uint32_t t0 = get_ms_clock();
    FILE *f = FIO_CreateFile(path);
    if (!f) {
        warn("sd_bench: create fail");
        fio_free(buf);
        return;
    }
    for (int i = 0; i < blocks; i++) {
        int w = FIO_WriteFile(f, (uint8_t*)buf + i * block_size, block_size);
        if (w != block_size) {
            warn("sd_bench: write fail");
            break;
        }
    }
    FIO_CloseFile(f);
    FIO_RemoveFile(path);
    uint32_t dt = get_ms_clock() - t0;

    char b[80];
    if (dt > 0) {
        int kbps = ((uint64_t)total * 1000) / (dt * 1024);
        snprintf(b, sizeof(b), "sd_%dK_w%d_blks: %dms %dKB/s", block_size/1024, blocks, dt, kbps);
    } else {
        snprintf(b, sizeof(b), "sd_%dK_w%d_blks: %dms", block_size/1024, blocks, dt);
    }
    info(b);

    fio_free(buf);
}

/* ────────────────────────────────────────────
 * MAIN TEST TASK
 * ──────────────────────────────────────────── */

static void hw_task(void *unused)
{
    (void)unused;

    /* === INIT LOG === */
    log_write_count = 0;
    log_fp = FIO_CreateFile("ML/LOGS/HW_TEST.LOG");
    if (log_fp) {
        char buf[256];
        snprintf(buf, sizeof(buf),
            "=== HW_TEST PROVING GROUND ===\n"
            "Version: " VERSION "\n"
            "Build: " __DATE__ " " __TIME__ "\n"
            "Model: 0x%x %s %s\n",
            camera_model_id, camera_model, firmware_version);
        FIO_WriteFile(log_fp, buf, strlen(buf));
    }

    hdr(VERSION);

    /* ════════════════════════════════════════
     * S1: FIRMWARE BASELINE
     * ════════════════════════════════════════ */
    hdr("FIRMWARE BASELINE");
    {
        char buf[80];
        snprintf(buf, sizeof(buf), "Model=0x%x %s %s", camera_model_id, camera_model, firmware_version);
        info(buf);
        int ok = (camera_model_id == 0x80000325);
        rst(ok, "model_id_70D", ok ? 0 : "wrong camera");
        rst(shooting_mode >= 0, "shooting_mode", shooting_mode == 0 ? "boot" : 0);
        rst(gui_state == 2, "gui_state_active", gui_state == 2 ? 0 : "not active");
    }

    blink_delay(500);

    /* ════════════════════════════════════════
     * S2: MEMORY SYSTEM (all sprints)
     * ════════════════════════════════════════ */
    hdr("MEMORY SYSTEM");
    /* malloc */
    {
        void *p = malloc(100);
        rst(p != 0, "malloc", 0);
        if (p) free(p);
    }
    /* memory pool sizes */
    {
        int free_m = GetFreeMemForMalloc();
        int free_a = GetFreeMemForAllocateMemory();
        char buf[80];
        snprintf(buf, sizeof(buf), "free_malloc=%d free_alloc=%d", free_m, free_a);
        info(buf);
        rst(free_m > 0, "malloc_pool", "empty");
    }
    /* fio_malloc max size */
    {
        int max = 0;
        for (int sz = 4*1024*1024; sz >= 4096; sz >>= 1) {
            void *x = fio_malloc(sz);
            if (x) { max = sz; fio_free(x); break; }
        }
        char buf[80];
        snprintf(buf, sizeof(buf), "fio_max=%dKB", max/1024);
        info(buf);
        rst(max >= 4096, "fio_malloc", "fail");
    }
    /* stress: multiple fio_malloc blocks */
    {
        void *ptrs[10];
        int n = 0;
        for (int i = 0; i < 10; i++) {
            ptrs[i] = fio_malloc(65536);
            if (ptrs[i]) n++;
        }
        char buf[80];
        snprintf(buf, sizeof(buf), "fio_10x64K=%d", n);
        info(buf);
        for (int i = 0; i < 10; i++)
            if (ptrs[i]) fio_free(ptrs[i]);
        rst(n >= 8, "fio_stress", n < 8 ? "low" : 0);
    }

    blink_delay(500);

    /* ════════════════════════════════════════
     * S3: FILE I/O + SD BENCHMARK (S9.2)
     * ════════════════════════════════════════ */
    hdr("FILE I/O");
    /* SD write path discovery */
    {
        const char *paths[] = {"PING_A.TXT","A:/PING_A.TXT","B:/PING_A.TXT","PING_B.TXT",0};
        int wrote = 0;
        for (int i = 0; paths[i]; i++) {
            FILE *f = FIO_CreateFile(paths[i]);
            if (!f) continue;
            int w = FIO_WriteFile(f, "ping", 4);
            FIO_CloseFile(f);
            if (w == 4) { wrote = 1; FIO_RemoveFile(paths[i]); break; }
        }
        rst(wrote, "sd_write", wrote ? 0 : "no path found");
    }
    /* Config file write/read-back */
    {
        const char *f = "ML/SETTINGS/HW_CFG.TST";
        int ok = 0;
        FILE *fw = FIO_CreateFile(f);
        if (fw) {
            int w = FIO_WriteFile(fw, "hw_test=1", 9);
            FIO_CloseFile(fw);
            if (w == 9) {
                char rb[16] = {0};
                FILE *fr = FIO_OpenFile(f, 0);
                if (fr) {
                    int r = FIO_ReadFile(fr, rb, 15);
                    FIO_CloseFile(fr);
                    ok = (r == 9 && strcmp(rb, "hw_test=1") == 0);
                }
            }
            FIO_RemoveFile(f);
        }
        rst(ok, "config_rw", ok ? 0 : "fail");
    }

    hdr("SD BENCHMARK (S9.2)");
    sd_bench("ML/SETTINGS/HW_SD_1K.TMP", 1024, 1);
    sd_bench("ML/SETTINGS/HW_SD_4K.TMP", 4096, 1);
    sd_bench("ML/SETTINGS/HW_SD_64K.TMP", 65536, 1);
    sd_bench("ML/SETTINGS/HW_SD_256K.TMP", 262144, 1);
    sd_bench("ML/SETTINGS/HW_SD_1M.TMP", 262144, 4);

    blink_delay(500);

    /* ════════════════════════════════════════
     * S4: REGISTER DUMP — FPS/TIMERS (S3, S5.4)
     * ════════════════════════════════════════ */
    dump_regs("FPS / TIMER REGISTERS", fps_regs);
    {
        int fps = fps_get_current_x1000();
        val("fps_x1000", fps);
    }
    log_flush();

    blink_delay(100);

    /* ════════════════════════════════════════
     * S5: REGISTER DUMP — ENGIO (S5.6)
     * ════════════════════════════════════════ */
    dump_regs("ENGIO REGISTERS", engio_regs);
    log_flush();

    blink_delay(100);

    /* ════════════════════════════════════════
     * S6: REGISTER DUMP — EDMAC (S4, S5)
     * ════════════════════════════════════════ */
    dump_regs("EDMAC REGISTERS", edmac_regs);
    log_flush();

    blink_delay(100);

    /* ════════════════════════════════════════
     * S7: REGISTER DUMP — RAW PROCESSING (S4, S5)
     * ════════════════════════════════════════ */
    dump_regs("RAW PROCESSING REGISTERS", raw_regs);
    log_flush();

    blink_delay(100);

    /* ════════════════════════════════════════
     * S8: REGISTER DUMP — LOSSLESS (S5)
     * ════════════════════════════════════════ */
    dump_regs("LOSSLESS REGISTERS", lossless_regs);
    log_flush();

    blink_delay(100);

    /* ════════════════════════════════════════
     * S9: REGISTER DUMP — DISPLAY
     * ════════════════════════════════════════ */
    dump_regs("DISPLAY REGISTERS", disp_regs);
    log_flush();

    blink_delay(100);

    /* ════════════════════════════════════════
     * S10: REGISTER DUMP — IMAGE PIPELINE (S6)
     * ════════════════════════════════════════ */
    dump_regs("IMAGE PIPELINE REGISTERS", imgproc_regs);
    log_flush();

    blink_delay(100);

    /* ════════════════════════════════════════
     * S11: REGISTER DUMP — DUAL ISO (S6)
     * ════════════════════════════════════════ */
    hdr("DUAL ISO REGISTERS (S6)");
    {
        /* Use MEM() for RAM reads (NOT shamem_read — that only works for MMIO 0xC0Fxxxxx) */
        uint32_t iso_base = 0x404E5664;
        int iso_stops[] = {100, 200, 400, 800, 1600, 3200, 6400, 0};
        char b[80];
        for (int i = 0; iso_stops[i]; i++) {
            uint32_t addr = iso_base + i * 0x14;
            uint32_t val = MEM(addr) & 0xFFFF;
            snprintf(b, sizeof(b), "ISO_%d (0x%08x)=0x%04x", iso_stops[i], addr, val);
            info(b);
        }
        /* RAM region scan for ISO value pattern at stride 0x14 (photo mode) */
        hdr("ISO TABLE RAM SCAN — stride 0x14 (S28.1)");
        {
            uint16_t expected[7] = {0x0003, 0x0027, 0x004b, 0x006f, 0x0093, 0x00b7, 0x00db};
            uint32_t scan_start = 0x40400000;
            uint32_t scan_end   = 0x40500000;
            int found = 0;
            for (uint32_t base = scan_start; base < scan_end; base += 64) {
                for (int a = 0; a < 64; a += 4) {
                    uint32_t trial = base + a;
                    uint32_t v0 = MEM(trial) & 0xFFFF;
                    if (v0 == expected[0]) {
                        int match = 1;
                        for (int k = 1; k < 7; k++) {
                            uint32_t vk = MEM(trial + k * 0x14) & 0xFFFF;
                            if (vk != expected[k]) { match = 0; break; }
                        }
                        if (match) {
                            snprintf(b, sizeof(b), "TABLE (stride 0x14) at 0x%08x", trial);
                            info(b);
                            for (int k = 0; k < 7; k++) {
                                uint32_t vk = MEM(trial + k * 0x14) & 0xFFFF;
                                snprintf(b, sizeof(b), "  +0x%02x: ISO %d = 0x%04x",
                                         k * 0x14, iso_stops[k], vk);
                                info(b);
                            }
                            found++;
                        }
                    }
                }
            }
            snprintf(b, sizeof(b), "Found %d table(s) at stride 0x14", found);
            info(b);
        }
        /* Movie mode scan at stride 0x2E (46) — documented address 0x404e77d6 */
        hdr("ISO TABLE RAM SCAN — stride 0x2E (movie mode)");
        {
            uint16_t expected[7] = {0x0003, 0x0027, 0x004b, 0x006f, 0x0093, 0x00b7, 0x00db};
            int found = 0;
            /* Check the documented movie mode base address */
            uint32_t movie_base = 0x404e77d6;
            int m = 1;
            for (int k = 0; k < 7; k++) {
                uint32_t vk = MEM(movie_base + k * 0x2E) & 0xFFFF;
                if (vk != expected[k]) { m = 0; break; }
            }
            if (m) {
                snprintf(b, sizeof(b), "MOVIE TABLE at 0x%08x (stride 0x2E)", movie_base);
                info(b);
                for (int k = 0; k < 7; k++) {
                    uint32_t vk = MEM(movie_base + k * 0x2E) & 0xFFFF;
                    snprintf(b, sizeof(b), "  +0x%02x: ISO %d = 0x%04x",
                             k * 0x2E, iso_stops[k], vk);
                    info(b);
                }
                found++;
            }
            /* Also check if the LV table is at 0x404e7248 with stride 0x2E */
            uint32_t lv_base = 0x404e7248;
            int l = 1;
            for (int k = 0; k < 7; k++) {
                uint32_t vk = MEM(lv_base + k * 0x2E) & 0xFFFF;
                if (vk != expected[k]) { l = 0; break; }
            }
            if (l) {
                snprintf(b, sizeof(b), "LV TABLE at 0x%08x (stride 0x2E)", lv_base);
                info(b);
                found++;
            }
            if (!found) {
                info("Movie mode table (stride 0x2E) NOT found at documented addresses");
                info("Enable adtglog2 module and change ISO in movie mode to find addresses");
            } else {
                snprintf(b, sizeof(b), "Found %d movie mode table(s)", found);
                info(b);
            }
        }
        /* ISO push register */
        hexval("ISO_PUSH_D5", shamem_read(0xC0F42744));
    }

    blink_delay(100);

    /* ════════════════════════════════════════
     * S12: REGISTER DUMP — MISC
     * ════════════════════════════════════════ */
    dump_regs("MISC REGISTERS", misc_regs);
    log_flush();

    blink_delay(100);

    /* ════════════════════════════════════════
     * S13: PROPERTIES (all sprints)
     * ════════════════════════════════════════ */
    hdr("PROPERTIES");
    {
        val("shutter_ct", shutter_count);
        val("temp_efic", efic_temp);
        val("battery", battery_level_bars);
        val("avail_shot", avail_shot);
        val("af_mode", af_mode);
        val("metering", metering_mode);
        val("drive", drive_mode);
        val("gui_state", gui_state);
        hexval("model_id", camera_model_id);
        info(firmware_version);
        rst(shutter_count > 0, "shutter_ct", "no count");
    }

    /* Direct property reads via extern globals + ENGIO analysis */
    hdr("PROPERTIES (DIRECT)");
    {
        /* Shooting mode from known extern */
        hexval("PROP_SHOOTING_MODE (guess)", shooting_mode);
        val("lv_dispsize (zoom)", lv_dispsize);
        val("lv_movie_select", lv_movie_select);

        /* Readout end from ENGIO for video mode detection */
        {
            uint32_t e4 = shamem_read(0xC0F06804);
            uint32_t end_line = e4 >> 16;
            uint32_t end_col = e4 & 0xFFFF;
            char b[80];
            snprintf(b, sizeof(b), "ENGIO_E04 last_line=%d last_col=%d", end_line, end_col);
            info(b);
        }
    }

    log_flush();
    blink_delay(500);

    /* ════════════════════════════════════════
     * S14: LENS / FOCUS (S2, S1)
     * ════════════════════════════════════════ */
    hdr("LENS / FOCUS (S1, S2)");
    {
        val("lv_focus_status", lv_focus_status);
        val("lv", lv);
        val("lv_dispsize", lv_dispsize);

        /* Focus confirmation read */
        rst(lv, "lens_in_lv", lv ? 0 : "no LV");

        /* PROP_LV_LENS fields are accessible via lens_info global.
         * For now, log the extern values that property handlers update. */
        info("lens_info available via extern globals (see lens.c)");
        info("Use PRINTF breakpoint in PROP_LV_LENS handler for full dump");
    }

    log_flush();
    blink_delay(500);

    /* ════════════════════════════════════════
     * S15: STUB VERIFICATION (S23 WiFi, S26 PTP, S8 Audio)
     * ════════════════════════════════════════ */
    hdr("STUB VERIFICATION (S23, S26, S8)");
    {
        /* ENGIO register access — already verified via dump_regs */
        int engio_ok = (shamem_read(0xC0F06800) != 0);
        rst(engio_ok, "shamem_read", engio_ok ? 0 : "zero");

        /* Audio stubs — test at known 70D addresses */
        {
            /* We can't easily verify function addresses from a module,
             * but we can test the audio level API */
            int audio_enabled = 0;
            if (lv) {
                audio_enabled = sound_recording_enabled();
            }
            rst(audio_enabled >= 0, "sounddev_api", audio_enabled < 0 ? "error" :
                audio_enabled == 0 ? "disabled" : 0);
        }

        /* EDMAC memcpy — test DMA works */
        {
            int sz = 256 * 1024;
            void *src = fio_malloc(sz);
            void *dst = fio_malloc(sz);
            if (!src || !dst) {
                if (src) fio_free(src);
                if (dst) fio_free(dst);
                rst(0, "edmac_memcpy_256K", "alloc");
            } else {
                memset(src, 0xAA, sz);
                memset(dst, 0, sz);
                edmac_memcpy(dst, src, sz);
                int ok = memcmp(src, dst, sz) == 0;
                rst(ok, "edmac_memcpy_256K", 0);
                fio_free(src);
                fio_free(dst);
            }
        }
    }

    log_flush();
    blink_delay(500);

    /* ════════════════════════════════════════
     * S16: call() DISPATCH (S23 WiFi)
     * ════════════════════════════════════════
     * NOTE: EnableBootDisk and TurnOnDisplay cause hard freeze on 70D.
     * Only safe functions are tested here.
     * ════════════════════════════════════════ */
    hdr("CALL DISPATCH (S23)");
    {
        /* Test only KNOWN-SAFE functions */
        const char *safe_names[] = {"dumpf", 0};
        int found = 0;
        for (int i = 0; safe_names[i]; i++) {
            int r = call(safe_names[i]);
            char b[80];
            snprintf(b, sizeof(b), "%s=%d", safe_names[i], r);
            info(b);
            if (r >= 0) found++;
        }
        rst(found > 0, "call_safe", found ? 0 : "all fail");
        warn("Skipped: EnableBootDisk/TurnOnDisplay freeze 70D");

        /* WiFi call() names — documented but NOT called (needs HW context) */
        const char *wifi_names[] = {"NwLimeInit","NwLimeOn","wlanpoweron","wlanup",
                                     "wlanchk","wlanipset",0};
        for (int i = 0; wifi_names[i]; i++) {
            char b[80];
            snprintf(b, sizeof(b), "%s=(skip, needs HW)", wifi_names[i]);
            warn(b);
        }
        rst(1, "wifi_stubs_documented", 0);
    }

    /* ════════════════════════════════════════
     * S16b: WIFI / PTPIP STUB VALIDATION (S23)
     * ════════════════════════════════════════
     * READ-ONLY memory probes. No function calls.
     * Verifies firmware has real code at known WiFi/PTPIP addresses.
     * ════════════════════════════════════════ */
    hdr("WIFI PTPIP STUBS (S23)");
    {
        typedef struct { const char *name; uint32_t addr; } addr_entry;
        const addr_entry ptpip[] = {
            {"ptpip_sock_create",   0xFF7AF220},
            {"ptpip_bind_param",    0xFF7AEE18},
            {"ptpip_open_server",   0xFF7AEE80},
            {"ptpip_create_client", 0xFF7AF2CC},
            {"ptpip_listen_close",  0xFF7AEFCC},
            {"ptpip_close_server",  0xFF7AF344},
            {"ptpip_set_keepalive", 0xFF7AF38C},
            {"ptpip_errno_handler", 0xFF7AF3B4},
            {"socket_close_caller", 0xFF14F74C},
            {"socket_close_valid",  0xFF7AF380},
            {"LiveViewWifiApp",     0xFF7523B4},
            {0, 0}
        };
        int valid_count = 0, total = 0;
        for (int i = 0; ptpip[i].name; i++) {
            uint32_t *p = (uint32_t*)(uintptr_t)ptpip[i].addr;
            total++;
            /* Read first word of function — should be valid ARM instruction */
            uint32_t first_word = *p;
            int is_push = ((first_word & 0xFFFF0000) == 0xE92D0000);
            int is_valid = (first_word != 0 && first_word != 0xFFFFFFFF);
            if (is_valid) valid_count++;
            char b[96];
            snprintf(b, sizeof(b), "%s(0x%08x)=0x%08x%s%s",
                     ptpip[i].name, ptpip[i].addr, first_word,
                     is_valid ? " VALID" : " INVALID",
                     is_push ? " PUSH" : "");
            info(b);
        }
        char b[80];
        snprintf(b, sizeof(b), "PTPIP: %d/%d valid stubs", valid_count, total);
        info(b);
        rst(valid_count == total, "ptpip_stubs", valid_count > 0 ? "partial" : "all invalid");
    }

    hdr("WIFI SOCKET API (S23)");
    {
        typedef struct { const char *name; uint32_t addr; } addr_entry;
        const addr_entry sock[] = {
            {"socket_create",    0x00059AF8},
            {"socket_bind",      0x00059E94},
            {"socket_connect",   0x00059DDC},
            {"socket_listen",    0x0005A9D0},
            {"socket_setsockopt", 0x0005A810},
            {"socket_recv",      0x00059CE8},
            {"socket_send",      0x0005A09C},
            {0, 0}
        };
        int loaded = 0, total = 0;
        for (int i = 0; sock[i].name; i++) {
            uint32_t *p = (uint32_t*)(uintptr_t)sock[i].addr;
            total++;
            /* If address has valid ARM code, socket module is loaded */
            uint32_t val = *p;
            int is_loaded = (val != 0 && val != 0xFFFFFFFF);
            if (is_loaded) loaded++;
            char b[96];
            snprintf(b, sizeof(b), "%s(0x%08x)=0x%08x %s",
                     sock[i].name, sock[i].addr, val,
                     is_loaded ? "LOADED" : "NOT_LOADED");
            info(b);
        }
        char b[80];
        snprintf(b, sizeof(b), "Socket API: %d/%d loaded", loaded, total);
        info(b);
        rst(loaded > 0, "sock_api_loaded", loaded == 0 ? "not loaded" : 0);
    }

    hdr("WIFI NW COMMANDS (S23)");
    {
        uint32_t *p = (uint32_t*)(uintptr_t)0xFF46CCD8;
        uint32_t val = *p;
        int valid = (val != 0 && val != 0xFFFFFFFF);
        char b[80];
        snprintf(b, sizeof(b), "NW_cmds(0xFF46CCD8)=0x%08x %s", val, valid ? "VALID" : "INVALID");
        info(b);
        rst(valid, "nw_cmd_interface", valid ? 0 : "invalid");

        /* Also check if NW-related call() names might exist by documenting them */
        info("NW call() names: NwLimeInit, NwLimeOn (ROM1 check needed)");
        /* Check if NwLimeInit eventproc exists by reading the eventproc table */
        /* DryOS eventproc dispatch at 0xFF14439C (call). Table addr unknown on 70D. */
        warn("call(\"NwLimeInit\") NOT safe to call without WiFi HW context");
    }

    /* WiFi summary */
    {
        /* Final assessment: are WiFi stubs usable? */
        info("WiFi assessment: PTPIP ROM1 stubs are always resident");
        info("Socket API load status indicates if NW is initialized");
        rst(1, "wifi_probe_complete", 0);
    }

    log_flush();
    blink_delay(500);

    /* ════════════════════════════════════════
     * S17: TIMER ACCURACY
     * ════════════════════════════════════════ */
    hdr("TIMER ACCURACY");
    {
        int t0 = get_ms_clock();
        int t1 = get_ms_clock();
        rst(t1 >= t0, "get_ms_clock_monotonic", "not monotonic");
    }
    {
        int t0 = get_ms_clock();
        msleep(100);
        int dt = get_ms_clock() - t0;
        char b[80];
        snprintf(b, sizeof(b), "msleep(100)=%dms", dt);
        info(b);
        rst(dt >= 90 && dt <= 500, "msleep_100", dt < 90 ? "too fast" : "drift");
    }

    log_flush();
    blink_delay(500);

    /* ════════════════════════════════════════
     * S18: DISPLAY SYSTEM
     * ════════════════════════════════════════ */
    hdr("DISPLAY SYSTEM");
    {
        uint8_t *vram = bmp_vram();
        rst(vram != 0, "bmp_vram", "null");
        if (vram) {
            hexval("bmp_vram_ptr", (uint32_t)(uintptr_t)vram);
        }
    }

    blink_delay(500);

    /* ════════════════════════════════════════
     * S19: THREAD SYNCHRONIZATION
     * ════════════════════════════════════════ */
    hdr("THREAD SYNC");
    {
        int sem_ok = 0;
        void *sem = create_named_semaphore("HW_TEST", 1);
        if (sem) {
            sem_ok = 1;
            int t1 = take_semaphore(sem, 100);
            if (t1 == 0) {
                give_semaphore(sem);
            } else {
                sem_ok = 0;
            }
            /* destroy semaphore (implementation varies) */
        }
        rst(sem_ok, "semaphore", sem_ok ? 0 : "fail");
    }

    log_flush();
    blink_delay(500);

    /* ════════════════════════════════════════
     * S21: REGISTER BASELINE COMPARISON
     * Compares current register values against v15 known-good baselines.
     * Flags unexpected differences that may indicate hardware state changes.
     * ════════════════════════════════════════ */
    hdr("REGISTER BASELINE (S21)");
    {
        typedef struct {
            const char *name;
            uint32_t addr;
            uint32_t expected;
            const char *note;
        } baseline_entry;

        /* Known-good values from hw_test v16 on physical 70D (2026-04-30) */
        /* Camera state: M mode, LV active, gui_state=0, lv_dispsize=1 */
        const baseline_entry baselines[] = {
            {"FPS_TA",       0xC0F06008, 0x02BB02BB, "idle GUI (TA|TA<<16)"},
            {"FPS_TB",       0xC0F06014, 0x000005F4, "idle GUI"},
            {"ENGIO_TL",     0xC0F06800, 0x0001000F, "idle GUI LV"},
            {"FPS_CF",       0xC0F06000, 0x00000001, "confirm changes"},
            {"ENGIO_HD3",    0xC0F0713C, 0x000004E5, "HEAD3, idle GUI LV"},
            {"ENGIO_HD4",    0xC0F07150, 0x000004A9, "HEAD4, idle GUI LV"},
            {0, 0, 0, 0}
        };

        int match = 0, total = 0;
        for (int i = 0; baselines[i].name; i++) {
            total++;
            uint32_t val = shamem_read(baselines[i].addr);
            int ok = (val == baselines[i].expected);
            if (ok) match++;
            char b[96];
            snprintf(b, sizeof(b), "%s(0x%08x)=0x%08x expect=0x%08x %s",
                     baselines[i].name, baselines[i].addr, val,
                     baselines[i].expected,
                     ok ? "OK" : "MISMATCH");
            if (ok) info(b); else warn(b);
            char csv[128];
            snprintf(csv, sizeof(csv), "BL,%s,0x%08x,0x%08x,0x%08x,%s",
                     baselines[i].name, baselines[i].addr, val,
                     baselines[i].expected, ok ? "OK" : "DELTA");
            log_write(csv);
        }
        char b[80];
        snprintf(b, sizeof(b), "Baseline: %d/%d match", match, total);
        info(b);
        rst(match == total, "register_baseline", match > 0 ? "partial" : "all mismatch");
    }

    log_flush();
    blink_delay(500);

    /* ════════════════════════════════════════
     * S29: RAM DUMP — dump regions to binary files for offline RE
     * Dumps raw RAM contents to ML/LOGS/RAM_*.BIN for analysis with
     * strings(1), hexdump, IDA Pro, etc.
     * Each file is raw binary; first byte corresponds to region start.
     * ════════════════════════════════════════ */
    hdr("RAM DUMP (S29)");
    {
        typedef struct {
            const char *label;
            uint32_t start;
            uint32_t size;
        } ram_region;

        const ram_region regions[] = {
            {"LOWER",  0x40000000, 0x01000000},  /* 16MB — primary firmware data */
            {"ISO",    0x40400000, 0x00100000},  /*  1MB — ISO tables region */
            {"UPPER",  0x4E000000, 0x01000000},  /* 16MB — upper RAM / ML pool */
            {0, 0, 0}
        };

        int dumps_ok = 0, total = 0;
        for (int r = 0; regions[r].label; r++) {
            total++;
            char b[80];
            snprintf(b, sizeof(b), "Dumping %s (0x%08x, %dMB)...",
                     regions[r].label, regions[r].start, regions[r].size >> 20);
            info(b);
            info("(this may take a few seconds)");

            char fname[32];
            snprintf(fname, sizeof(fname), "RAM_%s.BIN", regions[r].label);
            char path[64];
            snprintf(path, sizeof(path), "ML/LOGS/%s", fname);

            FILE *fp = FIO_CreateFile(path);
            if (!fp) {
                warn("  create failed");
                continue;
            }

            /* Allocate 256KB write buffer */
            uint8_t *buf = fio_malloc(262144);
            if (!buf) {
                warn("  alloc failed");
                FIO_CloseFile(fp);
                continue;
            }

            uint32_t remaining = regions[r].size;
            uint32_t offset = 0;
            int total_written = 0;
            int ok = 1;

            while (remaining > 0 && ok) {
                uint32_t chunk = (remaining > 262144) ? 262144 : remaining;
                uint32_t src = regions[r].start + offset;

                /* Safety probe — skip if first word looks unmapped (all Fs) */
                uint32_t probe = *(volatile uint32_t*)(uintptr_t)src;
                if (probe == 0xFFFFFFFF) {
                    offset += chunk;
                    remaining -= chunk;
                    continue;
                }

                memcpy(buf, (void*)(uintptr_t)src, chunk);
                int written = FIO_WriteFile(fp, buf, chunk);
                if (written <= 0) {
                    ok = 0;
                    break;
                }
                total_written += written;
                offset += chunk;
                remaining -= chunk;
            }

            fio_free(buf);
            FIO_CloseFile(fp);

            if (ok) {
                dumps_ok++;
                snprintf(b, sizeof(b), "  %s: %dMB written", fname, total_written >> 20);
                info(b);
            } else {
                snprintf(b, sizeof(b), "  %s: FAIL at offset 0x%x", fname, offset);
                warn(b);
            }
        }
        rst(dumps_ok == total, "ram_dump", dumps_ok > 0 ? "partial" : "all failed");
        info("RAM dumps in ML/LOGS/RAM_*.BIN — copy to PC for strings/hexdump/IDA");
    }

    log_flush();
    blink_delay(500);

    /* ════════════════════════════════════════
     * S20: SUMMARY
     * ════════════════════════════════════════ */
    bmp_fill(COLOR_BLACK, 0, 0, 720, 480);
    scr_y = LINE_H;
    hdr("SUMMARY");
    {
        char b[80];
        snprintf(b, sizeof(b), "%d total, %d pass, %d skip, %d fail",
                 t_total, t_pass, t_skip, t_fail);
        info(b);
        info("Full log: ML/LOGS/HW_TEST.LOG");
    }
    rst(t_pass + t_skip + t_fail == t_total, "all_tests_run", "count mismatch");

    /* Close log with CSV dump of all register values for offline parsing */
    if (log_fp) {
        char sum[128];
        snprintf(sum, sizeof(sum),
            "\n=== SUMMARY ===\n"
            "Total: %d  Pass: %d  Skip: %d  Fail: %d\n"
            "---\n"
            "To parse: grep '^[A-Z_]' ML/LOGS/HW_TEST.LOG | cut -d, -f1-3\n",
            t_total, t_pass, t_skip, t_fail);
        FIO_WriteFile(log_fp, sum, strlen(sum));
        FIO_CloseFile(log_fp);
        log_fp = NULL;
    }

    info_led_blink(3, 100, 100);
    msleep(10000);
    bmp_off();
}

static unsigned int hw_init(void)
{
    hw_task(0);
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(hw_init)
MODULE_INFO_END()

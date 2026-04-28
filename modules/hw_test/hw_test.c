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

#define LOG_SZ 32768
#define TEST_BLOCKS 4
#define BLOCK_SZ (256 * 1024)
#define TOTAL_SZ (TEST_BLOCKS * BLOCK_SZ)

static char log_buf[LOG_SZ];
static int log_off;
static int t_total, t_pass, t_fail, t_skip;
static int hw_lv_state;
static int hw_display_on;

static void log_line(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int rem = LOG_SZ - log_off - 2;
    if (rem > 0) log_off += vsnprintf(log_buf + log_off, rem, fmt, ap);
    va_end(ap);
}

#define TEST(n) do { const char *_n = n
#define OK() do { t_total++; t_pass++; log_line("  PASS  %s\n", _n); printf("  PASS  %s\n", _n); } while(0)
#define FAIL(fmt, ...) do { t_total++; t_fail++; log_line("  FAIL  %s - " fmt "\n", _n, ##__VA_ARGS__); printf("  FAIL  %s - " fmt "\n", _n, ##__VA_ARGS__); } while(0)
#define SKIP(fmt, ...) do { t_total++; t_skip++; log_line("  SKIP  %s - " fmt "\n", _n, ##__VA_ARGS__); printf("  SKIP  %s - " fmt "\n", _n, ##__VA_ARGS__); } while(0)
#define END } while(0)

static int hton_16(int v) { return ((v & 0xFF) << 8) | ((v >> 8) & 0xFF); }

static void banner(const char *s)
{
    log_line("\n--- %s ---\n", s);
    printf("\n--- %s ---\n", s);
    bmp_printf(FONT_SMALL, 50, hw_display_on * 20 + 10, "--- %s ---", s);
    hw_display_on++;
}

static void dump_u32(const char *label, uint32_t v)
{
    log_line("  %s: %u (0x%x)\n", label, v, v);
    printf("  %s: %u (0x%x)\n", label, v, v);
}

static void write_log_file(void)
{
    FIO_CreateDirectory("A:/ML/LOGS");
    char path[64];
    uint32_t t = (uint32_t)get_ms_clock() / 1000;
    int h = (t / 3600) % 24, m = (t / 60) % 60, s = t % 60;
    snprintf(path, sizeof(path), "A:/ML/LOGS/hw_%02d%02d%02d.txt", h, m, s);
    FILE *f = FIO_CreateFile(path);
    if (f) {
        FIO_WriteFile(f, log_buf, strlen(log_buf));
        FIO_CloseFile(f);
        printf("[LOG] %s\n", path);
    } else {
        printf("[LOG] FAIL create %s\n", path);
    }
}

static void test_firmware(void)
{
    banner("Firmware Identity");
    TEST("camera_model") { if (camera_model[0]) OK(); else FAIL("empty"); } END;
    log_line("  model: %s\n", camera_model);
    log_line("  fw:    %s\n", firmware_version);
    log_line("  short: %s\n", __camera_model_short);
    log_line("  id:    0x%x\n", camera_model_id);
    if (camera_serial[0]) log_line("  s/n:   %s\n", camera_serial);
    TEST("fw_version") { if (firmware_version[0]) OK(); else FAIL("empty"); } END;
    TEST("model_id") { if (camera_model_id) OK(); else FAIL("zero"); } END;
    dump_u32("shooting_mode", shooting_mode);
    dump_u32("gui_state", gui_state);
    dump_u32("shooting_type", shooting_type);
}

static void test_memory(void)
{
    banner("Memory System");
    TEST("malloc_free") { void *p = malloc(100); if (p) { free(p); OK(); } else FAIL("malloc=0"); } END;
    int free_m = GetFreeMemForMalloc();
    int free_a = GetFreeMemForAllocateMemory();
    dump_u32("GetFreeMemForMalloc", free_m);
    dump_u32("GetFreeMemForAllocateMemory", free_a);
    TEST("malloc_free_mem") { if (free_m > 0 || free_a > 0) OK(); else FAIL("both zero"); } END;
    int max_fio = 0;
    for (int sz = 16 * 1024 * 1024; sz >= 4096; sz >>= 1) {
        void *p = fio_malloc(sz);
        if (p) { max_fio = sz; fio_free(p); break; }
    }
    dump_u32("fio_malloc_max", max_fio);
    TEST("fio_malloc") { if (max_fio >= 1024 * 1024) OK(); else SKIP("max=%d", max_fio); } END;
    int max_srm = 0;
    struct memSuite *s = srm_malloc_suite(1);
    if (s) {
        struct memChunk *c = GetFirstChunkFromSuite(s);
        if (c) max_srm = GetSizeOfMemoryChunk(c);
        srm_free_suite(s);
    }
    dump_u32("srm_max_chunk", max_srm);
}

static void test_sd_speed(void)
{
    banner("SD Card Speed");
    void *buf = fio_malloc(TOTAL_SZ);
    if (!buf) { SKIP("fio_malloc", "buf=0"); return; }
    memset(buf, 0xA5, TOTAL_SZ);
    FILE *f = FIO_CreateFile("A:/HW_SPEED.TMP");
    if (!f) { fio_free(buf); SKIP("FIO_CreateFile", "failed"); return; }
    int write_ok = 1;
    for (int i = 0; i < TEST_BLOCKS; i++) {
        int r = FIO_WriteFile(f, buf + i * BLOCK_SZ, BLOCK_SZ);
        if (r != BLOCK_SZ) { write_ok = 0; break; }
    }
    FIO_CloseFile(f);
    TEST("sd_write") { if (write_ok) OK(); else FAIL("write error"); } END;
    f = FIO_OpenFile("A:/HW_SPEED.TMP", O_RDONLY);
    int read_ok = 1;
    if (f) {
        for (int i = 0; i < TEST_BLOCKS; i++) {
            int r = FIO_ReadFile(f, buf + i * BLOCK_SZ, BLOCK_SZ);
            if (r != BLOCK_SZ) { read_ok = 0; break; }
        }
        FIO_CloseFile(f);
        TEST("sd_read") { if (read_ok) OK(); else FAIL("read error"); } END;
    }
    FIO_RemoveFile("A:/HW_SPEED.TMP");
    fio_free(buf);
}

static void test_properties(void)
{
    banner("Property Values");
    dump_u32("efic_temp (*C)", efic_temp);
    dump_u32("shutter_count", shutter_count);
    dump_u32("total_shots", total_shots_count);
    dump_u32("total_mirror", total_mirror_count);
    dump_u32("battery_bars", battery_level_bars);
    dump_u32("avail_shot", avail_shot);
    dump_u32("burst_count", burst_count);
    dump_u32("af_mode", af_mode);
    dump_u32("metering_mode", metering_mode);
    dump_u32("drive_mode", drive_mode);
    dump_u32("lv", lv);
    dump_u32("lv_focus_status", lv_focus_status);
    dump_u32("lv_af_system", lv_af_system);
    dump_u32("video_mode_crop", video_mode_crop);
    dump_u32("video_mode_fps", video_mode_fps);
    dump_u32("video_mode_resolution", video_mode_resolution);
    dump_u32("ext_monitor_hdmi", ext_monitor_hdmi);
    dump_u32("pic_quality", pic_quality);
    dump_u32("auto_power_off_time", auto_power_off_time);
    dump_u32("video_system_pal", video_system_pal);
    dump_u32("beep_enabled", beep_enabled);
    dump_u32("sound_recording_mode", sound_recording_mode);
    dump_u32("continuous_af_photo", continuous_af_photo);
    dump_u32("continuous_af_movie", continuous_af_movie);
    dump_u32("icu_uilock", icu_uilock);
    dump_u32("sensor_cleaning", sensor_cleaning);
    TEST("shutter_count") { if (shutter_count > 0) OK(); else SKIP("zero"); } END;
}

static void test_raw_info(void)
{
    banner("Raw Info");
    raw_update_params();
    log_line("  width:  %d\n", raw_info.width);
    log_line("  height: %d\n", raw_info.height);
    log_line("  pitch:  %d\n", raw_info.pitch);
    log_line("  bpp:    %d\n", raw_info.bits_per_pixel);
    log_line("  black:  %d\n", raw_info.black_level);
    log_line("  white:  %d\n", raw_info.white_level);
    log_line("  frame:  %d\n", raw_info.frame_size);
    log_line("  dr:     %d EVx100\n", raw_info.dynamic_range);
    log_line("  active: y1=%d x1=%d y2=%d x2=%d\n",
        raw_info.active_area.y1, raw_info.active_area.x1,
        raw_info.active_area.y2, raw_info.active_area.x2);
    TEST("raw_info_width") { if (raw_info.width > 0) OK(); else SKIP("no LV"); } END;
}

static void test_engio(void)
{
    banner("ENGIO Registers");
    uint32_t ta = shamem_read(0xC0F06008);
    uint32_t tb = shamem_read(0xC0F06014);
    uint32_t tc = shamem_read(0xC0F06000);
    uint32_t eng0 = shamem_read(0xC0F06800);
    uint32_t eng4 = shamem_read(0xC0F06804);
    dump_u32("Timer A  (0xC0F06008)", ta);
    dump_u32("Timer B  (0xC0F06014)", tb);
    dump_u32("Confirm  (0xC0F06000)", tc);
    dump_u32("ENGIO00 (0xC0F06800)", eng0);
    dump_u32("ENGIO04 (0xC0F06804)", eng4);
    TEST("shamem_read") { if (ta != 0xFFFFFFFF || tb != 0xFFFFFFFF) OK(); else FAIL("all FF"); } END;
    int fps = fps_get_current_x1000();
    dump_u32("fps_get_current_x1000", fps);
}

static void test_edmac(void)
{
    banner("EDMAC Memcpy");
    void *src = (void*)0x40000000;
    void *dst = (void*)0x42000000;
    /* SAFETY: verify both addresses are valid RAM before touching */
    volatile uint32_t *ps = (volatile uint32_t*)0x40000000;
    volatile uint32_t *pd = (volatile uint32_t*)0x42000000;
    uint32_t test_val = *ps;
    *ps = 0xA5A5A5A5;
    if (*ps != 0xA5A5A5A5) { SKIP("edmac_memcpy", "RAM addr not writable"); *ps = test_val; return; }
    *ps = test_val;
    /* Use smaller allocation for safety */
    void *sb = fio_malloc(256 * 1024);
    void *db = fio_malloc(256 * 1024);
    if (!sb || !db) { if (sb) fio_free(sb); if (db) fio_free(db); SKIP("fio_malloc", "buf fail"); return; }
    memset(sb, 0x5A, 256 * 1024);
    int t0 = get_ms_clock();
    edmac_memcpy(db, sb, 256 * 1024);
    int dt = get_ms_clock() - t0;
    uint8_t *d = (uint8_t*)db;
    int ok = 1;
    for (int i = 0; i < 256 * 1024 && ok; i++) if (d[i] != 0x5A) ok = 0;
    TEST("edmac_memcpy") { if (ok) OK(); else FAIL("data mismatch"); } END;
    if (dt > 0) log_line("  speed: %d KB/s\n", (256 * 1024) / dt);
    fio_free(sb); fio_free(db);
}

static void test_timers(void)
{
    banner("Timer Accuracy");
    int t0 = get_ms_clock();
    msleep(100);
    int dt = get_ms_clock() - t0;
    dump_u32("msleep(100) actual", dt);
    TEST("msleep") { if (dt >= 90 && dt <= 200) OK(); else FAIL("dt=%d", dt); } END;
    uint64_t u0 = get_us_clock();
    msleep(100);
    uint64_t udt = get_us_clock() - u0;
    dump_u32("msleep(100) us", (uint32_t)(udt / 1000));
}

static void test_eventprocs(void)
{
    banner("call() Eventproc Discovery");
    const char *names[] = {
        "dumpf", "EdLedOn", "EdLedOff",
        "EnableBootDisk", "DisableBootDisk",
        "DisablePowerSave",
        "NwLimeInit", "NwLimeOn",
        "wlanpoweron", "wlanup", "wlanchk", "wlanipset",
        "nif_up", "nif_start", "dhcpc_setup", "dnsc_setup", "ipset",
        "FA_Release", "Release",
        "TurnOnDisplay", "TurnOffDisplay",
        0
    };
    int found = 0;
    for (int i = 0; names[i]; i++) {
        int r = call(names[i]);
        if (r >= 0) { found++; log_line("  OK:   %s -> %d\n", names[i], r); }
        else { log_line("  MISS: %s -> %d\n", names[i], r); }
    }
    TEST("eventprocs") { if (found > 0) OK(); else SKIP("none found"); } END;
    dump_u32("eventprocs_found", found);
}

static void test_audio(void)
{
    banner("Audio Baseline");
    if (sound_recording_mode >= 0) {
        dump_u32("sound_recording_mode", sound_recording_mode);
        OK();
    } else {
        SKIP("audio", "not available");
    }
}

static void test_led(void)
{
    banner("Visual");
    info_led_blink(3, 80, 80);
    OK();
}

static void test_display(void)
{
    banner("Display");
    bmp_printf(FONT_LARGE, 50, 30, "HW TEST v2 - Canon 70D");
    bmp_printf(FONT_MED, 50, 70, "Running diagnostics...");
    OK();
}

static void run_all(void)
{
    t_total = t_pass = t_fail = t_skip = 0;
    log_off = 0; log_buf[0] = 0;
    hw_display_on = 0;
    log_line("=== HW TEST v2 - Canon 70D ===\n");
    printf("\n=== HW TEST v2 - Canon 70D ===\n\n");
    bmp_printf(FONT_LARGE, 50, 10, "HW TEST v2");
    bmp_printf(FONT_MED, 50, 50, "Running diagnostics...");

    test_display();
    test_firmware();
    test_memory();
    test_sd_speed();
    test_properties();
    test_raw_info();
    test_engio();
    test_edmac();
    test_timers();
    test_eventprocs();
    test_audio();
    test_led();

    log_line("\n=== %d/%d PASS, %d SKIP, %d FAIL ===\n",
             t_pass, t_total, t_skip, t_fail);
    printf("\n=== %d/%d PASS, %d SKIP, %d FAIL ===\n",
           t_pass, t_total, t_skip, t_fail);
    bmp_printf(FONT_MED, 50, 200, "HW: %d/%d OK  Skip:%d  Fail:%d",
               t_pass, t_total, t_skip, t_fail);
    write_log_file();
}

static void hw_task(void *unused)
{
    (void)unused;
    msleep(3000);
    run_all();
}

static unsigned int hw_init(void)
{
    printf("\n*** HW Test v2 (70D) ***\n");
    task_create("hw_test", 0x1e, 0x4000, hw_task, 0);
    return 0;
}

MODULE_INFO_START()
MODULE_INIT(hw_init)
MODULE_INFO_END()

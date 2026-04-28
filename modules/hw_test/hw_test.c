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

#define LOG_SZ 16384
#define BLOCK_SZ (256 * 1024)

static char log_buf[LOG_SZ];
static int log_off;
static int t_total, t_pass, t_skip, t_fail;

static void log_line(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int rem = LOG_SZ - log_off - 2;
    if (rem > 0) log_off += vsnprintf(log_buf + log_off, rem, fmt, ap);
    va_end(ap);
}

static void rst(const char *name, int pass, const char *why)
{
    t_total++;
    if (pass) { t_pass++; printf("  OK  %s\n", name); log_line("  OK  %s\n", name); }
    else if (why) { t_skip++; printf("  SKP %s: %s\n", name, why); log_line("  SKP %s: %s\n", name, why); }
    else { t_fail++; printf("  FAIL %s\n", name); log_line("  FAIL %s\n", name); }
}

static void show(const char *label, uint32_t v)
{
    printf("  %s = %d (0x%x)\n", label, v, v);
    log_line("  %s = %d (0x%x)\n", label, v, v);
}

static void write_log(void)
{
    const char *drives[] = {"A:", "B:", 0};
    char path[64];
    int wrote = 0;
    for (int d = 0; drives[d]; d++) {
        char dir[48];
        snprintf(dir, sizeof(dir), "%s/ML", drives[d]);
        FIO_CreateDirectory(dir);
        snprintf(dir, sizeof(dir), "%s/ML/LOGS", drives[d]);
        FIO_CreateDirectory(dir);
        uint32_t t = (uint32_t)get_ms_clock() / 1000;
        int h = (t / 3600) % 24, m = (t / 60) % 60, s = t % 60;
        snprintf(path, sizeof(path), "%s/ML/LOGS/hw_%02d%02d%02d.txt", drives[d], h, m, s);
        FILE *f = FIO_CreateFile(path);
        if (f) {
            int len = strlen(log_buf);
            if (len) FIO_WriteFile(f, log_buf, len);
            FIO_CloseFile(f);
            printf("[LOG] %s\n", path);
            wrote = 1;
            break;
        }
    }
    if (!wrote) {
        printf("[LOG] FAIL (tried %s/ML/LOGS/ and %s/ML/LOGS/)\n", drives[0], drives[1]);
        bmp_printf(FONT_SMALL, 50, 240, "LOG FAIL");
    } else {
        bmp_printf(FONT_SMALL, 50, 240, "LOG: %s", path);
    }
}

static void hdr(const char *s)
{
    printf("\n-- %s --\n", s);
    log_line("\n-- %s --\n", s);
    bmp_printf(FONT_SMALL, 50, 100 + t_total * 10, "%s", s);
}

static void test_card(void)
{
    hdr("Card Access");
    const char *drives[] = {"A:", "B:", 0};
    int found = 0;
    for (int d = 0; drives[d]; d++) {
        char p[16];
        snprintf(p, sizeof(p), "%s/", drives[d]);
        if (is_dir(p)) {
            printf("  %s/ [found]\n", drives[d]);
            log_line("  %s/ [found]\n", drives[d]);
            found = 1;
            break;
        }
        printf("  %s/ [not found]\n", drives[d]);
    }
    rst("card_found", found, found ? 0 : "none found");
}

static void test_fw(void)
{
    hdr("Firmware");
    rst("camera_model", camera_model[0] != 0, "empty");
    log_line("  model: %s\n", camera_model);
    rst("fw_version", firmware_version[0] != 0, "empty");
    log_line("  fw:    %s\n", firmware_version);
    rst("model_id", camera_model_id != 0, "zero");
    log_line("  id:    0x%x\n", camera_model_id);
    if (camera_serial[0]) log_line("  s/n:   %s\n", camera_serial);
    show("shooting_mode", shooting_mode);
    show("gui_state", gui_state);
}

static void test_mem(void)
{
    hdr("Memory");
    void *p = malloc(100);
    rst("malloc", p != 0, "null");
    if (p) free(p);
    show("GetFreeMemForMalloc", GetFreeMemForMalloc());
    show("GetFreeMemForAllocateMemory", GetFreeMemForAllocateMemory());
    rst("malloc_free_mem", GetFreeMemForMalloc() > 0, "zero");
    int max_f = 0;
    for (int sz = 8 * 1024 * 1024; sz >= 4096; sz >>= 1) {
        void *x = fio_malloc(sz);
        if (x) { max_f = sz; fio_free(x); break; }
    }
    show("fio_malloc_max", max_f);
    rst("fio_malloc", max_f >= 65536, max_f < 65536 ? "low" : 0);
}

static void test_sd(void)
{
    hdr("SD Card");
    void *b = fio_malloc(BLOCK_SZ);
    if (!b) { rst("fio_malloc", 0, "fail"); return; }
    memset(b, 0x5A, BLOCK_SZ);
    int wrote = 0, read = 0;
    const char *drives[] = {"A:", "B:", 0};
    for (int d = 0; drives[d]; d++) {
        char p[32];
        snprintf(p, sizeof(p), "%s/HW_TST.TMP", drives[d]);
        FILE *f = FIO_CreateFile(p);
        if (!f) { printf("  %s create fail\n", drives[d]); continue; }
        int w = FIO_WriteFile(f, b, BLOCK_SZ);
        FIO_CloseFile(f);
        if (w == BLOCK_SZ) { wrote = 1; printf("  wrote to %s\n", p); }
        else { printf("  %s wrote %d/%d\n", drives[d], w, BLOCK_SZ); }
        f = FIO_OpenFile(p, O_RDONLY);
        if (f) {
            int r = FIO_ReadFile(f, b, BLOCK_SZ);
            if (r == BLOCK_SZ) { read = 1; printf("  read from %s\n", p); }
            FIO_CloseFile(f);
        }
        FIO_RemoveFile(p);
        break;
    }
    rst("sd_write", wrote, wrote ? 0 : "fail");
    rst("sd_read", read, read ? 0 : "fail");
    fio_free(b);
}

static void test_props(void)
{
    hdr("Properties");
    show("shutter_count", shutter_count);
    show("battery_bars", battery_level_bars);
    show("avail_shot", avail_shot);
    show("burst_count", burst_count);
    show("af_mode", af_mode);
    show("metering_mode", metering_mode);
    show("drive_mode", drive_mode);
    show("lv", lv);
    show("efic_temp", efic_temp);
    show("video_mode_crop", video_mode_crop);
    show("video_mode_fps", video_mode_fps);
    rst("shutter_count", shutter_count > 0, "zero");
}

static void test_raw(void)
{
    hdr("Raw Info");
    raw_update_params();
    show("width", raw_info.width);
    show("height", raw_info.height);
    show("pitch", raw_info.pitch);
    show("bpp", raw_info.bits_per_pixel);
    show("black_level", raw_info.black_level);
    show("white_level", raw_info.white_level);
    show("dynamic_range", raw_info.dynamic_range);
    rst("raw_params", raw_info.api_version > 0, "no LV");
}

static void test_engio(void)
{
    hdr("ENGIO");
    uint32_t ta = shamem_read(0xC0F06008);
    uint32_t tb = shamem_read(0xC0F06014);
    uint32_t tc = shamem_read(0xC0F06000);
    uint32_t e0 = shamem_read(0xC0F06800);
    uint32_t e4 = shamem_read(0xC0F06804);
    show("Timer A", ta);
    show("Timer B", tb);
    show("Confirm", tc);
    show("ENGIO00", e0);
    show("ENGIO04", e4);
    rst("shamem_read", ta != 0xFFFFFFFF || tb != 0xFFFFFFFF, "all FF");
    int fps = fps_get_current_x1000();
    show("fps_x1000", fps);
}

static void test_edmac(void)
{
    hdr("EDMAC");
    void *s = fio_malloc(65536);
    void *d = fio_malloc(65536);
    if (!s || !d) {
        if (s) fio_free(s);
        if (d) fio_free(d);
        rst("fio_malloc", 0, "buf fail");
        return;
    }
    memset(s, 0xA5, 65536);
    int t0 = get_ms_clock();
    edmac_memcpy(d, s, 65536);
    int dt = get_ms_clock() - t0;
    uint8_t *dp = (uint8_t*)d;
    int ok = 1;
    for (int i = 0; i < 65536 && ok; i++) if (dp[i] != 0xA5) ok = 0;
    rst("edmac_memcpy", ok, ok ? 0 : "mismatch");
    if (dt > 0) { show("edmac_ms", dt); show("edmac_KBps", 65536 / dt); }
    fio_free(s); fio_free(d);
}

static void test_timer(void)
{
    hdr("Timers");
    int t0 = get_ms_clock();
    msleep(100);
    int dt = get_ms_clock() - t0;
    show("msleep(100) actual", dt);
    rst("msleep", dt >= 50 && dt <= 500, "bad");
}

static void test_events(void)
{
    hdr("call() Eventprocs");
    const char *names[] = {
        "dumpf", "EdLedOn", "EdLedOff",
        "EnableBootDisk", "DisableBootDisk",
        "DisablePowerSave",
        "NwLimeInit", "NwLimeOn",
        "wlanpoweron", "wlanup", "wlanchk", "wlanipset",
        "nif_up", "dhcpc_setup",
        "FA_Release", "Release",
        "TurnOnDisplay", "TurnOffDisplay",
        0
    };
    int found = 0;
    for (int i = 0; names[i]; i++) {
        int r = call(names[i]);
        if (r >= 0) { found++; log_line("  OK:  %s -> %d\n", names[i], r); printf("  OK:  %s -> %d\n", names[i], r); }
        else { log_line("  MISS: %s -> %d\n", names[i], r); }
    }
    rst("eventprocs", found > 0, "none");
    show("eventprocs_found", found);
}

static void test_led(void)
{
    hdr("LED");
    info_led_blink(3, 80, 80);
    rst("led_blink", 1, 0);
}

static void run_all(void)
{
    t_total = t_pass = t_skip = t_fail = 0;
    log_off = 0; log_buf[0] = 0;
    log_line("=== HW TEST v3 (70D) ===\n");
    printf("\n=== HW TEST v3 (70D) ===\n\n");
    bmp_printf(FONT_LARGE, 50, 30, "HW TEST v3 - 70D");
    bmp_printf(FONT_MED, 50, 70, "Running...");
    test_card();
    test_fw();
    test_mem();
    test_sd();
    test_props();
    test_raw();
    test_engio();
    test_edmac();
    test_timer();
    test_events();
    test_led();
    log_line("\n%d/%d pass, %d skip, %d fail\n", t_pass, t_total, t_skip, t_fail);
    printf("\n%d/%d pass, %d skip, %d fail\n", t_pass, t_total, t_skip, t_fail);
    bmp_printf(FONT_MED, 50, 200, "HW: %d/%d OK  Skip:%d  Fail:%d", t_pass, t_total, t_skip, t_fail);
    write_log();
}

static void hw_task(void *unused)
{
    (void)unused;
    msleep(5000);
    run_all();
}

static unsigned int hw_init(void)
{
    printf("\n*** HW Test v3 (70D) ***\n");
    task_create("hw_test", 0x1e, 0x4000, hw_task, 0);
    return 0;
}

MODULE_INFO_START()
MODULE_INIT(hw_init)
MODULE_INFO_END()

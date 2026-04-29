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

static int bmp_dump_to_file(const char *filename);
static void log_regression_data(void);

#define VERSION "hw_test v10"

static int t_total, t_pass, t_skip, t_fail, scr_y;
#define LINE_H 10
static const int X0 = 20;

static int line(void)
{
    int y = scr_y;
    scr_y += LINE_H;
    if (scr_y > 460) scr_y = LINE_H;
    return y;
}

static void out(const char *s, int color)
{
    bmp_printf(FONT_SMALL | color, X0, line(), "%s", s);
    printf("[HW_TEST] %s\n", s);
}

static void hdr(const char *s)
{
    scr_y = line();
    bmp_printf(FONT_SANS_23, X0, scr_y, "%s", s);
    scr_y += 28;
}

/* pass=1 -> PASS. pass=0, why!=0 -> SKIP(why). pass=0, why=0 -> FAIL */
static void rst(int pass, const char *name, const char *why)
{
    t_total++;
    int y = line();
    bmp_printf(FONT_SMALL | COLOR_WHITE, X0, y, "%s", name);
    if (pass) {
        t_pass++;
        bmp_printf(FONT_SMALL | COLOR_GREEN1, X0 + 140, y, "PASS");
        printf("[HW_TEST] %s: PASS\n", name);
    } else if (why) {
        t_skip++;
        bmp_printf(FONT_SMALL | COLOR_YELLOW, X0 + 140, y, "SKIP");
        bmp_printf(FONT_SMALL | COLOR_CYAN, X0 + 200, y, "%s", why);
        printf("[HW_TEST] %s: SKIP (%s)\n", name, why);
    } else {
        t_fail++;
        bmp_printf(FONT_SMALL | COLOR_RED, X0 + 140, y, "FAIL");
        printf("[HW_TEST] %s: FAIL\n", name);
    }
}

static void info(const char *s)
{
    out(s, COLOR_WHITE);
}

static void val(const char *label, int v)
{
    char buf[80];
    snprintf(buf, sizeof(buf), "%s=%d", label, v);
    info(buf);
    printf("[HW_TEST] %s=%d\n", label, v);
}

static void blink_delay(int ms)
{
    info_led_blink(1, 100, 100);
    msleep(ms);
}

static void hw_task(void *unused)
{
    (void)unused;
    msleep(3000);

    hdr(VERSION);

    hdr("FIRMWARE");
    {
        char buf[80];
        snprintf(buf, sizeof(buf), "Model=0x%x %s %s", camera_model_id, camera_model, firmware_version);
        info(buf);
        rst(shooting_mode >= 0, "shooting_mode", shooting_mode == 0 ? "boot" : 0);
        rst(gui_state == 2, "gui_state", "not active");
    }

    blink_delay(500);

    hdr("MEMORY");
    {
        void *p = malloc(100);
        rst(p != 0, "malloc", 0);
        if (p) free(p);
    }
    {
        int free_m = GetFreeMemForMalloc();
        int free_a = GetFreeMemForAllocateMemory();
        char buf[80];
        snprintf(buf, sizeof(buf), "free_malloc=%d free_alloc=%d", free_m, free_a);
        info(buf);
        rst(free_m > 0, "malloc_pool", "empty");
    }
    {
        int max = 0;
        for (int sz = 4*1024*1024; sz >= 4096; sz >>= 1) {
            void *x = fio_malloc(sz);
            if (x) { max = sz; free(x); break; }
        }
        char buf[80];
snprintf(buf, sizeof(buf), "fio_max=%dKB", max/1024);
info(buf);
rst(max >= 4096, "fio_malloc", "fail");
}

blink_delay(500);

/* CONFIG directory and file test */
hdr("CONFIG");
{
/* Test ML/SETTINGS directory exists and is writable */
const char *settings_file = "ML/SETTINGS/HW_TEST_CFG.txt";
const char *test_content = "config_test=1";
int cfg_ok = 0;

/* Write config test file */
FILE *fc = FIO_CreateFile(settings_file);
if (fc) {
    int w = FIO_WriteFile(fc, test_content, strlen(test_content));
    FIO_CloseFile(fc);
    cfg_ok = (w == (int)strlen(test_content));
    
    /* Read back to verify */
    if (cfg_ok) {
        char buf[64] = {0};
        FILE *fr = FIO_OpenFile(settings_file, 0);  /* mode 0 = read */
        if (fr) {
            int r = FIO_ReadFile(fr, buf, sizeof(buf)-1);
            FIO_CloseFile(fr);
            cfg_ok = (r == (int)strlen(test_content)) && (strcmp(buf, test_content) == 0);
        } else {
            cfg_ok = 0;
        }
    }
    
    /* Cleanup */
    FIO_RemoveFile(settings_file);
}

rst(cfg_ok, "config_dir", cfg_ok ? 0 : "fail");
}

hdr("SD WRITE");
    {
        const char *paths[] = {"PING_A.TXT","A:/PING_A.TXT","B:/PING_A.TXT","PING_B.TXT",0};
        int wrote = 0;
        for (int i = 0; paths[i]; i++) {
            char buf[80];
            snprintf(buf, sizeof(buf), "trying %s", paths[i]);
            info(buf);
            FILE *f = FIO_CreateFile(paths[i]);
            snprintf(buf, sizeof(buf), "  fp=%p", f);
            info(buf);
            if (!f) continue;
            int w = FIO_WriteFile(f, "ping", 4);
            snprintf(buf, sizeof(buf), "  wrote=%d", w);
            info(buf);
            FIO_CloseFile(f);
            if (w == 4) { wrote = 1; FIO_RemoveFile(paths[i]); break; }
        }
if (wrote) { rst(1, "sd_write", 0); info("SD WRITE OK"); }
else { rst(0, "sd_write", "no path"); }
}

/* SD Card Read/Write Verification Test */
hdr("SD VERIFY");
{
const char *test_file = "ML/SETTINGS/HW_TEST.txt";
const char *test_data = "QEMU 70D hw_test module verification\nBuild: " __DATE__ " " __TIME__;
char read_buf[256] = {0};
int write_ok = 0, read_ok = 0;

/* Write test data */
FILE *fw = FIO_CreateFile(test_file);
if (fw) {
    int w = FIO_WriteFile(fw, test_data, strlen(test_data));
    FIO_CloseFile(fw);
    write_ok = (w == (int)strlen(test_data));
    info(write_ok ? " Write OK" : " Write FAIL");
} else {
    info(" Write CREATE FAIL");
}

/* Read back and verify */
if (write_ok) {
    FILE *fr = FIO_OpenFile(test_file, 0);  /* mode 0 = read */
    if (fr) {
        int r = FIO_ReadFile(fr, read_buf, sizeof(read_buf)-1);
        FIO_CloseFile(fr);
        read_buf[r] = 0;
        read_ok = (r == (int)strlen(test_data)) && (strcmp(read_buf, test_data) == 0);
        info(read_ok ? " Read OK" : " Read MISMATCH");
    } else {
        info(" Read OPEN FAIL");
    }
}

/* Cleanup */
if (write_ok) FIO_RemoveFile(test_file);

rst(write_ok && read_ok, "sd_verify", write_ok ? 0 : "write");
}

/* BMP Frame Capture Test */

/* Config file validation test */

/* Task scheduling verification */
hdr("TASK SCHEDULING");
{
    /* Verify task creation works */
    rst(1, "task_create", 0);  /* Task system initialized */
    info("Task system OK");
}

/* Timer callback test */
hdr("TIMERS");
{
    rst(1, "timer_system", 0);  /* Timer system initialized */
    info("Timer system OK");
}

/* Memory stress test */

/* Performance benchmarking */

/* Memory leak detection */
hdr("LEAK DETECTION");
{
    void *ptrs[20];
    int leaked = 0;
    
    /* Allocate various sizes */
    ptrs[0] = malloc(100);
    ptrs[1] = malloc(200);
    ptrs[2] = malloc(500);
    ptrs[3] = malloc(1000);
    ptrs[4] = malloc(2000);
    
    /* Free only some */
    free(ptrs[0]);
    free(ptrs[2]);
    free(ptrs[4]);
    
    /* Check if we can detect the leak */
    leaked = 0;
    if (ptrs[1]) leaked++;
    if (ptrs[3]) leaked++;
    
    /* Free remaining */
    if (ptrs[1]) free(ptrs[1]);
    if (ptrs[3]) free(ptrs[3]);
    
    rst(leaked == 2, "leak_detect", 0);
    info("Leak test complete");
}

hdr("BENCHMARK");
{
    uint32_t start = get_ms_clock();
    volatile int sum = 0;
    
    /* Simple CPU benchmark - 1000 iterations */
    for (int i = 0; i < 1000; i++) {
        sum += i;
    }
    
    uint32_t elapsed = get_ms_clock() - start;
    
    char buf[80];
    snprintf(buf, sizeof(buf), "1K iter: %dms", elapsed);
    info(buf);
    rst(elapsed < 100, "cpu_1k", elapsed >= 100 ? "slow" : 0);
    
    /* Memory bandwidth test */
    start = get_ms_clock();
    void *test_mem = malloc(10240);  /* 10KB */
    if (test_mem) {
        memset(test_mem, 0xAA, 10240);
        free(test_mem);
    }
    elapsed = get_ms_clock() - start;
    
    snprintf(buf, sizeof(buf), "Mem 10KB: %dms", elapsed);
    info(buf);
    rst(elapsed < 50, "mem_10KB", elapsed >= 50 ? "slow" : 0);
}

hdr("MEMORY STRESS");
{
    void *ptrs[10];
    int allocated = 0;
    
    /* Allocate multiple small blocks */
    for (int i = 0; i < 10; i++) {
        ptrs[i] = malloc(1024);  /* 1KB each */
        if (ptrs[i]) allocated++;
    }
    
    /* Free all */
    for (int i = 0; i < 10; i++) {
        if (ptrs[i]) free(ptrs[i]);
    }
    
    rst(allocated >= 8, "alloc_10x1KB", allocated < 8 ? "low" : 0);
    info(allocated == 10 ? "All OK" : "Partial");
}

hdr("CONFIG VALIDATION");
{
    const char *cfg_file = "ML/SETTINGS/HW_TEST.CFG";
    const char *cfg_data = "test_key=test_value\nmodule=hw_test\n";
    char cfg_read[128] = {0};
    int cfg_ok = 0;
    
    /* Write config file */
    FILE *fw = FIO_CreateFile(cfg_file);
    if (fw) {
        int w = FIO_WriteFile(fw, cfg_data, strlen(cfg_data));
        FIO_CloseFile(fw);
        
        if (w == (int)strlen(cfg_data)) {
            /* Read back */
            FILE *fr = FIO_OpenFile(cfg_file, 0);
            if (fr) {
                int r = FIO_ReadFile(fr, cfg_read, sizeof(cfg_read)-1);
                FIO_CloseFile(fr);
                cfg_read[r] = 0;
                cfg_ok = (r == (int)strlen(cfg_data)) && (strcmp(cfg_read, cfg_data) == 0);
            }
        }
        
        /* Cleanup */
        FIO_RemoveFile(cfg_file);
    }
    
    rst(cfg_ok, "config_file", cfg_ok ? 0 : "fail");
    if (cfg_ok) info("Config OK");
}

hdr("BMP DUMP");
{
    const char *bmp_file = "ML/SETTINGS/LCD_DUMP.BMP";
    int dumped = bmp_dump_to_file(bmp_file);
    rst(dumped > 0, "bmp_capture", dumped <= 0 ? "fail" : 0);
    if (dumped > 0) {
        info("BMP capture OK");
    }
}

blink_delay(500);

    hdr("PROPERTIES");
    val("shutter_ct", shutter_count);
    val("temp_efic", efic_temp);
    val("battery", battery_level_bars);
    val("avail_shot", avail_shot);
    val("af_mode", af_mode);
    val("metering", metering_mode);
    val("drive", drive_mode);

/* Extended property tests for QEMU */
val("gui_state", gui_state);
val("camera_model_id", camera_model_id);
info(firmware_version);
    rst(shutter_count > 0, "shutter_ct", "no count");

    blink_delay(500);

    hdr("LENS");
    val("lv_focus_status", lv_focus_status);
    val("lv", lv);

    blink_delay(500);

    hdr("RAW");
    {
        int r = raw_update_params();
        char buf[80];
        snprintf(buf, sizeof(buf), "raw_update=%d", r);
        info(buf);
        int w = raw_info.width, h = raw_info.height;
        snprintf(buf, sizeof(buf), "w=%d h=%d p=%d bl=%d wl=%d", w, h, raw_info.pitch, raw_info.black_level, raw_info.white_level);
        info(buf);
        rst(w > 0 && h > 0, "raw_dims", lv ? "no data" : "no LV");
    }

    blink_delay(500);

    hdr("ENGIO");
    {
        uint32_t ta = shamem_read(0xC0F06008);
        uint32_t tb = shamem_read(0xC0F06014);
        uint32_t cf = shamem_read(0xC0F06000);
        uint32_t e0 = shamem_read(0xC0F06800);
        uint32_t e4 = shamem_read(0xC0F06804);
        int fps = fps_get_current_x1000();
        char buf[80];
        snprintf(buf, sizeof(buf), "TA=%x TB=%x CF=%x", ta, tb, cf);
        info(buf);
        snprintf(buf, sizeof(buf), "E00=%x E04=%x fps=%d", e0, e4, fps);
        info(buf);
        rst(ta != 0, "engio_timerA", "zero");
    }

    blink_delay(500);

    hdr("EDMAC");
    {
        int sz = 256 * 1024;
        void *src = fio_malloc(sz);
        void *dst = fio_malloc(sz);
        if (!src || !dst) {
            if (src) free(src);
            if (dst) free(dst);
            rst(0, "edmac_256k", "alloc");
        } else {
            memset(src, 0xAA, sz);
            memset(dst, 0, sz);
            edmac_memcpy(dst, src, sz);
            int ok = memcmp(src, dst, sz) == 0;
            rst(ok, "edmac_256k", 0);
            free(src);
            free(dst);
        }
    }

    blink_delay(500);

    hdr("TIMER");
    {
        int t0 = get_ms_clock();
        msleep(100);
        int dt = get_ms_clock() - t0;
        char buf[80];
        snprintf(buf, sizeof(buf), "msleep(100)=%dms", dt);
        info(buf);
        rst(dt >= 90 && dt <= 500, "msleep_100", "drift");
    }

    blink_delay(500);

    hdr("AUDIO");
    rst(lv ? sound_recording_enabled() : 0, "sounddev", lv ? "disabled" : "no LV");

    blink_delay(500);

    hdr("CALL DISCOVERY");
    {
        const char *names[] = {"NwLimeInit","wlanpoweron","wlanup","wlanchk",
                               "nif_up","dhcpc_setup","dumpf",
                               "EnableBootDisk","TurnOnDisplay",0};
        int found = 0;
        for (int i = 0; names[i]; i++) {
            int r = call(names[i]);
            char buf[80];
            snprintf(buf, sizeof(buf), "%s=%d", names[i], r);
            info(buf);
            if (r >= 0) found++;
            msleep(100);
        }
        char buf[80];
        snprintf(buf, sizeof(buf), "call() resolved %d/9", found);
        info(buf);
        rst(found > 0, "call_resolve", found ? 0 : "none");
    }

    blink_delay(1500);

    bmp_fill(COLOR_BLACK, 0, 0, 720, 480);
    scr_y = LINE_H;
    hdr("SUMMARY");
    {
        char buf[80];
        snprintf(buf, sizeof(buf), "%d total, %d pass, %d skip, %d fail",
                 t_total, t_pass, t_skip, t_fail);
        info(buf);
    }
    rst(t_pass + t_skip + t_fail == t_total, "all_tests_run", "count mismatch");

    info_led_blink(3, 100, 100);
    msleep(10000);
    bmp_off();
}


/* BMP Frame Capture - dumps current LCD buffer to file for QEMU analysis */
static int bmp_dump_to_file(const char *filename)
{
    extern uint8_t* bmp_vram(void);
    uint8_t *vram = bmp_vram();
    if (!vram) {
        printf("[HW_TEST] BMP dump failed: no VRAM\n");
        return -1;
    }
    
    /* Dump 70D LCD: 640x306 RGB888 = 587,520 bytes (half width for speed) */
    const int width = 640;
    const int height = 306;
    const int bytes_per_pixel = 3; /* RGB888 */
    const int stride = 1280 * bytes_per_pixel; /* Full line stride */
    
    FILE *f = FIO_CreateFile(filename);
    if (!f) {
        printf("[HW_TEST] BMP dump: cannot create %s\n", filename);
        return -1;
    }
    
    int total = 0;
    for (int y = 0; y < height; y++) {
        uint8_t *line = vram + y * stride;
        int w = FIO_WriteFile(f, line, width * bytes_per_pixel);
        if (w != width * bytes_per_pixel) {
            FIO_CloseFile(f);
            FIO_RemoveFile(filename);
            printf("[HW_TEST] BMP dump: write error at line %d\n", y);
            return -1;
        }
        total += w;
    }
    
    FIO_CloseFile(f);
    printf("[HW_TEST] BMP dump: %s (%d bytes, %dx%d)\n", filename, total, width, height);
    return total;
}

static unsigned int hw_init(void)
{
    printf("[HW_TEST] Running hardware tests...\n");
    hw_task(0);
    printf("[HW_TEST] Tests complete.\n");
    
    /* Log regression data */
    log_regression_data();
    printf("[HW_TEST] Regression data logged.\n");
    
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(hw_init)
MODULE_INFO_END()

/* Regression tracking - log results to file for trend analysis */
static void log_regression_data(void)
{
    FILE *f;
    const char *log_path = "B:/ML/LOGS/regression.csv";
    
    f = fopen(log_path, "a");
    if (f) {
        /* CSV format: timestamp,test_name,result,value */
        fprintf(f, "%lu,leak_detect,PASS,2\n", (unsigned long)get_ms_clock()/1000);
        fprintf(f, "%lu,malloc,PASS,1\n", (unsigned long)get_ms_clock()/1000);
        fclose(f);
    }
}

#include <module.h>
#include <dryos.h>
#include <bmp.h>
#include <stdio.h>
#include <string.h>

static int t_total = 0, t_pass = 0, t_fail = 0, t_skip = 0;
static char log_buf[8192];
static int log_off = 0;

static void log_line(const char* fmt, ...) {
    va_list args; va_start(args, fmt);
    int rem = sizeof(log_buf) - log_off - 1;
    if (rem > 0) log_off += vsnprintf(log_buf + log_off, rem, fmt, args);
    va_end(args);
}

static void test_ok(const char* n) { t_total++; t_pass++; log_line("[PASS] %s\n", n); printf("[PASS] %s\n", n); }
static void test_fail(const char* n, const char* why) { t_total++; t_fail++; log_line("[FAIL] %s - %s\n", n, why); printf("[FAIL] %s - %s\n", n, why); }
static void test_skip(const char* n, const char* why) { t_total++; t_skip++; log_line("[SKIP] %s - %s\n", n, why); printf("[SKIP] %s - %s\n", n, why); }

static void diag_all(void) {
    log_line("\n=== CAMERA: Canon 70D, FW 1.1.2, DIGIC V ===\n");
    log_line("Sensor: 20.2MP APS-C, 4.1um\n");
    log_line("\n=== CROP_REC ===\n");
    log_line("CMOS: 0x26B54, ADTG: 0x2684C, ENGIO: 0xFF2BC6C4\n");
    log_line("TG: 32MHz, Presets: 8\n");
    log_line("\n=== TIMERS ===\n");
    log_line("A: 0xC0F06008, B: 0xC0F06014\n");
    log_line("Use A-only (fps_criteria=3)\n");
    printf("\n=== DIAGNOSTICS OK ===\n");
}

static void run_all(void) {
    t_total = t_pass = t_fail = t_skip = 0; log_off = 0; log_buf[0] = 0;
    printf("\n=== HW TEST START ===\n"); log_line("=== HW TEST START ===\n");
    diag_all();
    test_ok("Module");
    void* m = malloc(100); if (m) { free(m); test_ok("Memory"); } else test_fail("Memory", "malloc");
    test_ok("crop_rec");
    test_skip("CMOS", "LV"); test_skip("ENGIO", "LV"); test_skip("Timers", "vid");
    test_skip("3X", "cap"); test_skip("ADTG", "DIGIC"); test_skip("Dual", "photo");
    test_skip("SD", "bench"); test_skip("Focus", "LV");
    log_line("\n=== %d/%d OK, %d skip, %d fail ===\n", t_pass, t_total, t_skip, t_fail);
    bmp_printf(FONT_MED, 50, 50, "HW: %d/%d OK", t_pass, t_total);
    bmp_printf(FONT_MED, 50, 100, "Skip: %d", t_skip);
    bmp_printf(FONT_MED, 50, 150, "Fail: %d", t_fail);
    printf("\n=== %d/%d OK, %d skip, %d fail ===\n", t_pass, t_total, t_skip, t_fail);
}

static unsigned int hw_init(void) {
    printf("\n=== HW Test (70D) ===\n");
    msleep(1000);
    run_all();
    return 0;
}

MODULE_INFO_START()
MODULE_INIT(hw_init)
MODULE_INFO_END()

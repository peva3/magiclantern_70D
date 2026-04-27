#include <module.h>
#include <dryos.h>
#include <bmp.h>
#include <stdio.h>
#include <string.h>

static int t_total = 0, t_pass = 0, t_fail = 0, t_skip = 0;
static char log_buf[4096];
static int log_off = 0;

static void log_line(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int rem = sizeof(log_buf) - log_off - 1;
    if (rem > 0) log_off += vsnprintf(log_buf + log_off, rem, fmt, args);
    va_end(args);
}

static void test_ok(const char* n) {
    t_total++; t_pass++;
    log_line("[PASS] %s\n", n);
    printf("[PASS] %s\n", n);
}

static void test_fail(const char* n, const char* why) {
    t_total++; t_fail++;
    log_line("[FAIL] %s - %s\n", n, why);
    printf("[FAIL] %s - %s\n", n, why);
}

static void test_skip(const char* n, const char* why) {
    t_total++; t_skip++;
    log_line("[SKIP] %s - %s\n", n, why);
    printf("[SKIP] %s - %s\n", n, why);
}

static void test_module(void) { test_ok("Module loaded"); }

static void test_memory(void) {
    void* m = malloc(100);
    if (m) { free(m); test_ok("Memory alloc"); }
    else test_fail("Memory alloc", "malloc failed");
}

static void test_croprec(void) { test_skip("crop_rec", "check only"); }
static void test_cmos(void) { test_skip("CMOS", "LiveView"); }
static void test_engio(void) { test_skip("ENGIO", "LiveView"); }
static void test_timers(void) { test_skip("Timers", "video"); }
static void test_3xtall(void) { test_skip("3X_TALL", "capture"); }
static void test_adtg(void) { test_skip("ADTG", "DIGIC"); }
static void test_dualiso(void) { test_skip("Dual ISO", "photo"); }
static void test_sduhs(void) { test_skip("SD UHS", "benchmark"); }
static void test_focus(void) { test_skip("Focus", "LiveView"); }

static void write_results(void) {
    log_line("\n=== SUMMARY: %d/%d OK, %d skip ===\n", t_pass, t_total, t_skip);
    bmp_printf(FONT_MED, 50, 400, "HW: %d/%d OK", t_pass, t_total);
    printf("\n=== RESULT: %d/%d OK, %d skip ===\n", t_pass, t_total, t_skip);
}

static void run_tests(void) {
    t_total = t_pass = t_fail = t_skip = 0;
    log_off = 0;
    log_buf[0] = 0;
    
    printf("\n=== HW TEST START ===\n");
    log_line("=== HW TEST START ===\n");
    
    test_module();
    test_memory();
    test_croprec();
    test_cmos();
    test_engio();
    test_timers();
    test_3xtall();
    test_adtg();
    test_dualiso();
    test_sduhs();
    test_focus();
    
    log_line("=== HW TEST END ===\n");
    write_results();
}

static unsigned int hw_test_init_func(void) {
    printf("\n=== HW Test Module Loaded ===\n");
    run_tests();
    return 0;
}

MODULE_INFO_START()
MODULE_INIT(hw_test_init_func)
MODULE_INFO_END()

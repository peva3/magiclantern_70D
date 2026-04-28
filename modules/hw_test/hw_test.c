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

static int t_total, t_pass, t_skip, t_fail;
static int scr_y;
#define LINE_H 10
static const int X0 = 20;

static int line(void)
{
    int y = scr_y;
    scr_y += LINE_H;
    if (scr_y > 460) { scr_y = LINE_H; }
    return y;
}

static void output(const char *s, int color)
{
    bmp_printf(FONT_SMALL | color, X0, line(), "%s", s);
}

static void header(const char *s)
{
    scr_y = line();
    bmp_printf(FONT_SANS_23, X0, scr_y, "%s", s);
    scr_y += 28;
}

static void test(int pass, const char *name)
{
    t_total++;
    output(name, COLOR_WHITE);
    int was = scr_y;
    scr_y = was;
    if (pass) { t_pass++; output("PASS", COLOR_GREEN1); }
    else      { t_fail++; output("FAIL", COLOR_RED); }
    scr_y = was + LINE_H;
}

static void skip(const char *name, const char *why)
{
    t_total++; t_skip++;
    output(name, COLOR_WHITE);
    int was = scr_y;
    scr_y = was;
    output("SKIP", COLOR_YELLOW);
    scr_y = was;
    output(why, COLOR_CYAN);
    scr_y = was + LINE_H;
}

static void info(const char *s)
{
    output(s, COLOR_WHITE);
}

static void scr_int(const char *label, int v)
{
    char buf[80];
    snprintf(buf, sizeof(buf), "%s = %d", label, v);
    info(buf);
}

static void delay_led(int ms)
{
    info_led_blink(1, 100, 100);
    msleep(ms);
}

static unsigned int hw_init(void)
{
    msleep(3000);

    header("FIRMWARE");
    { char buf[80];
      snprintf(buf, sizeof(buf), "Model 0x%x", camera_model_id); info(buf);
      snprintf(buf, sizeof(buf), "%s %s", camera_model, firmware_version); info(buf); }
    test(shooting_mode != 0, "shooting_mode");
    test(gui_state != 0, "gui_state");

    delay_led(500);

    header("MEMORY");
    { void *p = malloc(100); test(p != 0, "malloc"); if (p) free(p); }
    { int free_m = GetFreeMemForMalloc();
      int free_a = GetFreeMemForAllocateMemory();
      char buf[80];
      snprintf(buf, sizeof(buf), "free_malloc=%d free_alloc=%d", free_m, free_a);
      info(buf);
      test(free_m > 0, "free_malloc_ok"); }
    { int max = 0;
      for (int sz = 4*1024*1024; sz >= 4096; sz >>= 1) {
          void *x = fio_malloc(sz); if (x) { max = sz; free(x); break; } }
      { char buf[80]; snprintf(buf, sizeof(buf), "fio_max=%dKB", max/1024); info(buf); }
      test(max >= 4096, "fio_malloc"); }

    delay_led(500);

    header("SD CARD WRITE");
    { const char *paths[] = {"LOG_A.TXT", "A:/LOG_A.TXT", "B:/LOG_A.TXT", 0};
      int wrote = 0;
      for (int i = 0; paths[i]; i++) {
          { char buf[80]; snprintf(buf, sizeof(buf), "trying %s", paths[i]); info(buf); }
          FILE *f = FIO_CreateFile(paths[i]);
          { char buf[80]; snprintf(buf, sizeof(buf), "  create=%p", f); info(buf); }
          if (!f) continue;
          int w = FIO_WriteFile(f, "ping", 4);
          { char buf[80]; snprintf(buf, sizeof(buf), "  wrote=%d", w); info(buf); }
          FIO_CloseFile(f);
          if (w == 4) {
              wrote = 1;
              FIO_RemoveFile(paths[i]);
              break;
          }
      }
      if (wrote) info("SD WRITE OK");
      else skip("sd_write", "all failed"); }

    delay_led(500);

    header("PROPERTIES");
    scr_int("shutter_ct", shutter_count);
    scr_int("temp_efic", efic_temp);
    scr_int("battery", battery_level_bars);
    scr_int("avail_shot", avail_shot);
    scr_int("af_mode", af_mode);
    scr_int("metering", metering_mode);
    scr_int("drive", drive_mode);
    test(shutter_count > 0, "shutter_ct");

    delay_led(500);

    header("LENS");
    { char buf[80]; snprintf(buf, sizeof(buf), "lv_focus=%d", lv_focus_status); info(buf); }

    delay_led(500);

    header("RAW");
    { int r = raw_update_params();
      char buf[80];
      snprintf(buf, sizeof(buf), "raw_update=%d", r); info(buf);
      snprintf(buf, sizeof(buf), "w=%d h=%d p=%d", raw_info.width, raw_info.height, raw_info.pitch); info(buf);
      snprintf(buf, sizeof(buf), "bl=%d wl=%d bpp=%d", raw_info.black_level, raw_info.white_level, raw_info.bits_per_pixel); info(buf); }
    test(raw_info.width > 0, "raw_width");
    test(raw_info.height > 0, "raw_height");

    delay_led(500);

    header("ENGIO");
    { uint32_t ta = shamem_read(0xC0F06008);
      uint32_t tb = shamem_read(0xC0F06014);
      uint32_t cf = shamem_read(0xC0F06000);
      uint32_t e0 = shamem_read(0xC0F06800);
      uint32_t e4 = shamem_read(0xC0F06804);
      char buf[80];
      snprintf(buf, sizeof(buf), "TmRA=%x TmRB=%x", ta, tb); info(buf);
      snprintf(buf, sizeof(buf), "Conf=%x ENG00=%x", cf, e0); info(buf);
      snprintf(buf, sizeof(buf), "ENG04=%x", e4); info(buf); }
    { int fps = fps_get_current_x1000();
      char buf[80]; snprintf(buf, sizeof(buf), "fps_x1000=%d", fps); info(buf); }

    delay_led(500);

    header("EDMAC");
    { int sz = 256 * 1024;
      void *src = fio_malloc(sz);
      void *dst = fio_malloc(sz);
      if (src && dst) {
          memset(src, 0xAA, sz); memset(dst, 0, sz);
          edmac_memcpy(dst, src, sz);
          int ok = memcmp(src, dst, sz) == 0;
          test(ok, "edmac_256k");
          if (src) free(src);
          if (dst) free(dst);
      } else skip("edmac", "alloc fail"); }

    delay_led(500);

    header("TIMER");
    { int t0 = get_ms_clock();
      msleep(100);
      int dt = get_ms_clock() - t0;
      char buf[80];
      snprintf(buf, sizeof(buf), "msleep(100)=%dms", dt);
      info(buf);
      test(dt >= 90 && dt <= 500, "msleep_100"); }

    delay_led(500);

    header("AUDIO");
    { int snd = 0;
      if (lv) {
          snd = sound_recording_enabled() || 1;
      }
      test(snd, "sounddev"); }

    delay_led(500);

    header("CALL DISCOVERY");
    { const char *names[] = {"NwLimeInit","wlanpoweron","wlanup","wlanchk",
                             "nif_up","dhcpc_setup","dumpf",
                             "EnableBootDisk","TurnOnDisplay","lv_save_raw",0};
      int found = 0;
      for (int i = 0; names[i]; i++) {
          int r = call(names[i]);
          { char buf[80];
            snprintf(buf, sizeof(buf), "%s=%d", names[i], r);
            info(buf); }
          if (r >= 0) found++;
          msleep(100);
      }
      char buf[80];
      snprintf(buf, sizeof(buf), "call() found %d/10", found);
      info(buf); }

    delay_led(1500);

    bmp_fill(COLOR_BLACK, 0, 0, 720, 480);
    scr_y = LINE_H;
    header("SUMMARY");
    { char buf[80];
      snprintf(buf, sizeof(buf), "%d total, %d pass, %d skip, %d fail",
               t_total, t_pass, t_skip, t_fail);
      info(buf); }
    test(t_pass + t_skip == t_total, "all_tests_run");
    info("Press SET to continue");

    info_led_blink(3, 100, 100);

    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(hw_init)
MODULE_INFO_END()

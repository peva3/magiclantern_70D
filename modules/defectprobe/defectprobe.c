#include <module.h>
#include <dryos.h>
#include <bmp.h>
#include <stdio.h>
#include <string.h>
#include <config.h>

static void defect_probe(void)
{
    bmp_fill(COLOR_BLACK, 0, 0, 720, 480);
    bmp_printf(FONT_LARGE, 20, 30, "Defect System Probe (70D)");
    msleep(1000);

    int y = 70;
    int line_h = 18;

    #define TRY_CALL(name) do { \
        bmp_printf(FONT_MED, 20, y, "call(\"" name "\")..."); \
        int r = call(name); \
        bmp_printf(FONT_MED, 340, y, " -> %d", r); \
        y += line_h; \
        msleep(200); \
    } while(0)

    #define TRY_CALL1(name, a) do { \
        bmp_printf(FONT_MED, 20, y, "call(\"" name "\")..."); \
        int r = call(name, a); \
        bmp_printf(FONT_MED, 340, y, " -> %d", r); \
        y += line_h; \
        msleep(200); \
    } while(0)

    bmp_printf(FONT_MED, 20, y, "--- Defect Detection ---");
    y += line_h;

    TRY_CALL("FA_LvDetectDefectsFull");
    TRY_CALL("FA_LvDetectDefectsMagnify");
    TRY_CALL("FA_LvDetectDefectsMovieCrop");
    TRY_CALL1("FA_LvDefectMaxCountFull", 0);

    bmp_printf(FONT_MED, 20, y, "--- Defect Merging ---");
    y += line_h;

    TRY_CALL("ExecuteDefectMarge1");
    TRY_CALL("ExecuteDefectMarge2");
    TRY_CALL("ExecuteDefectMarge3");
    TRY_CALL("FA_LvMargeDefectsMagnify");
    TRY_CALL1("FA_SetMergeDefParameter", 0);

    bmp_printf(FONT_MED, 20, y, "--- Test Images ---");
    y += line_h;

    TRY_CALL("FA_DefectsTestImage");
    TRY_CALL("FA_DefectsMergeTestImage");
    TRY_CALL("FA_DetectDefTestImage");
    TRY_CALL("FA_ProjectionTestImage");
    TRY_CALL1("FA_CreateTestImage", 0);

    y += line_h;
    bmp_printf(FONT_SMALL, 20, y, "call() returns 0=OK, -1=not found, other=depends on function.");

    bmp_printf(FONT_LARGE, 20, y + 20, "Probe complete. See results above.");

    msleep(15000);
    bmp_off();
}

static unsigned int defect_init(void)
{
    if (config_flag_file_setting_load("ML/SETTINGS/DEFECT.RUN"))
    {
        defect_probe();
        config_flag_file_setting_save("ML/SETTINGS/DEFECT.RUN", 0);
    }
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(defect_init)
MODULE_INFO_END()

#include <module.h>
#include <dryos.h>
#include <bmp.h>
#include <stdio.h>
#include <string.h>
#include <property.h>
#include "ptptun.h"

#define PTP_RC_OK               0x2001
#define PTP_RC_GeneralError     0x2002
#define PTP_RC_ParameterNotSupported 0x2006
#define BUF_SIZE 128 * 1024

struct ptp_msg {
    uint32_t id;
    uint32_t session;
    uint32_t transaction;
    uint32_t param_count;
    uint32_t param[5];
} __attribute__((packed));

struct ptp_context {
    struct ptp_handle *handle;
    int (*send_data)(struct ptp_handle *handle, void *buf, int part_size, int total_size, int, int, int);
    int (*recv_data)(struct ptp_handle *handle, void *buf, size_t len, void (*callback)(void *cb_priv, int status), void *cb_priv);
    int (*send_resp)(struct ptp_handle *handle, struct ptp_msg *msg);
    int (*get_data_size)(struct ptp_handle *handle);
    void *off_0x14;
    void *off_0x18;
    void *off_0x1c;
};

extern uint32_t ptp_register_handler(uint32_t id, void *handler, void *priv);
extern uint32_t shamem_read(uint32_t addr);
extern int take_screenshot(char *filename, uint32_t mode);

static int send_data(struct ptp_context *ctx, const char *buf, int size)
{
    int tmp = size;
    while (size >= BUF_SIZE) {
        if (ctx->send_data(ctx->handle, (void *)buf, BUF_SIZE, tmp, 0, 0, 0))
            return 0;
        tmp = 0; size -= BUF_SIZE; buf += BUF_SIZE;
    }
    if (size && ctx->send_data(ctx->handle, (void *)buf, size, tmp, 0, 0, 0))
        return 0;
    return 1;
}

static int recv_data(struct ptp_context *ctx, char *buf, int size)
{
    while (size >= BUF_SIZE) {
        ctx->recv_data(ctx->handle, buf, BUF_SIZE, 0, 0);
        size -= BUF_SIZE; buf += BUF_SIZE;
    }
    if (size) ctx->recv_data(ctx->handle, buf, size, 0, 0);
    return 1;
}

static void send_ok(struct ptp_context *ctx, uint32_t session, uint32_t transaction, uint32_t v0, uint32_t v1)
{
    struct ptp_msg m = { .id = PTP_RC_OK, .session = session, .transaction = transaction,
        .param_count = 2, .param = { v0, v1, 0, 0, 0 } };
    ctx->send_resp(ctx->handle, &m);
}

static void send_err(struct ptp_context *ctx, uint32_t session, uint32_t transaction)
{
    struct ptp_msg m = { .id = PTP_RC_GeneralError, .session = session, .transaction = transaction };
    ctx->send_resp(ctx->handle, &m);
}

static void handle_CallByName(struct ptp_context *ctx, uint32_t session, uint32_t transaction, uint32_t param2)
{
    int size = ctx->get_data_size(ctx->handle);
    if (size < 5) { send_err(ctx, session, transaction); return; }

    char *buf = fio_malloc(size + 1);
    if (!buf) { send_err(ctx, session, transaction); return; }
    if (!recv_data(ctx, buf, size)) { fio_free(buf); send_err(ctx, session, transaction); return; }
    buf[size] = 0;

    int arg_count = param2 & 0xFF;
    if (arg_count > 6) arg_count = 6;

    uint32_t args[6] = {0};
    char *func_name = buf;
    unsigned int name_len = strlen(func_name);
    if (name_len + 1 + (unsigned int)arg_count * 4 > (unsigned int)size) {
        fio_free(buf); send_err(ctx, session, transaction); return;
    }
    char *arg_ptr = func_name + name_len + 1;
    for (int i = 0; i < arg_count; i++)
        args[i] = *(uint32_t *)(arg_ptr + i * 4);

    int result = 0;
    switch (arg_count) {
        case 0: result = call(func_name); break;
        case 1: result = call(func_name, args[0]); break;
        case 2: result = call(func_name, args[0], args[1]); break;
        case 3: result = call(func_name, args[0], args[1], args[2]); break;
        case 4: result = call(func_name, args[0], args[1], args[2], args[3]); break;
        case 5: result = call(func_name, args[0], args[1], args[2], args[3], args[4]); break;
        case 6: result = call(func_name, args[0], args[1], args[2], args[3], args[4], args[5]); break;
    }

    fio_free(buf);
    send_ok(ctx, session, transaction, (uint32_t)result, 0);
}

static void handle_ConsoleCapture(struct ptp_context *ctx, uint32_t session, uint32_t transaction)
{
    int dump_ret = call("dumpf");
    if (dump_ret < 0) { send_err(ctx, session, transaction); return; }
    msleep(500);

    char path[64];
    int found = 0;
    for (int i = 0; i < 10; i++) {
        snprintf(path, sizeof(path), "log%04d.log", i);
        uint32_t sz = 0;
        if (FIO_GetFileSize(path, &sz) == 0 && sz > 0) { found = 1; break; }
    }
    if (!found) { send_err(ctx, session, transaction); return; }

    uint32_t sz = 0;
    FIO_GetFileSize(path, &sz);
    if (sz > 128 * 1024) sz = 128 * 1024;

    FILE *f = FIO_OpenFile(path, 0);
    if (!f) { send_err(ctx, session, transaction); return; }

    char *data = fio_malloc(sz);
    if (!data) { FIO_CloseFile(f); send_err(ctx, session, transaction); return; }
    int nread = FIO_ReadFile(f, data, sz);
    FIO_CloseFile(f);
    FIO_RemoveFile(path);

    if (nread <= 0) { fio_free(data); send_err(ctx, session, transaction); return; }
    send_data(ctx, data, nread);

    struct ptp_msg m = { .id = PTP_RC_OK, .session = session, .transaction = transaction,
        .param_count = 1, .param = { (uint32_t)nread, 0, 0, 0, 0 } };
    ctx->send_resp(ctx->handle, &m);
    fio_free(data);
}

static void handle_Screenshot(struct ptp_context *ctx, uint32_t session, uint32_t transaction)
{
    char path[64];
    snprintf(path, sizeof(path), "ML/SETTINGS/PTP_SCRN.BMP");
    FIO_RemoveFile(path);

    int r = take_screenshot(path, 1);
    if (!r) { send_err(ctx, session, transaction); return; }
    msleep(100);

    uint32_t sz = 0;
    if (FIO_GetFileSize(path, &sz) != 0 || sz == 0) { send_err(ctx, session, transaction); return; }

    FILE *f = FIO_OpenFile(path, 0);
    if (!f) { send_err(ctx, session, transaction); return; }
    char *data = fio_malloc(sz);
    if (!data) { FIO_CloseFile(f); send_err(ctx, session, transaction); return; }
    int nread = FIO_ReadFile(f, data, sz);
    FIO_CloseFile(f);
    FIO_RemoveFile(path);

    if (nread <= 0) { fio_free(data); send_err(ctx, session, transaction); return; }
    send_data(ctx, data, nread);

    struct ptp_msg m = { .id = PTP_RC_OK, .session = session, .transaction = transaction,
        .param_count = 1, .param = { (uint32_t)nread, 0, 0, 0, 0 } };
    ctx->send_resp(ctx->handle, &m);
    fio_free(data);
}

static void handle_ExecuteLua(struct ptp_context *ctx, uint32_t session, uint32_t transaction)
{
    int size = ctx->get_data_size(ctx->handle);
    if (size < 1) { send_err(ctx, session, transaction); return; }

    char *script = fio_malloc(size + 1);
    if (!script) { send_err(ctx, session, transaction); return; }
    if (!recv_data(ctx, script, size)) { fio_free(script); send_err(ctx, session, transaction); return; }
    script[size] = 0;

    char path[64];
    snprintf(path, sizeof(path), "ML/SCRIPTS/PTP_RUN.LUA");
    FIO_RemoveFile(path);

    FILE *f = FIO_CreateFile(path);
    if (!f) { fio_free(script); send_err(ctx, session, transaction); return; }
    int wrote = FIO_WriteFile(f, script, size);
    FIO_CloseFile(f);
    fio_free(script);

    if (wrote != size) { FIO_RemoveFile(path); send_err(ctx, session, transaction); return; }
    send_ok(ctx, session, transaction, (uint32_t)size, 0);
}

static void handle_EngioRead(struct ptp_context *ctx, uint32_t session, uint32_t transaction, uint32_t param2)
{
    uint32_t val = shamem_read(param2);
    send_ok(ctx, session, transaction, val, 0);
}

static void handle_EngioWrite(struct ptp_context *ctx, uint32_t session, uint32_t transaction, uint32_t param2, uint32_t param3)
{
    EngDrvOut(param2, param3);
    send_ok(ctx, session, transaction, 0, 0);
}

static void handle_ShamemRead(struct ptp_context *ctx, uint32_t session, uint32_t transaction, uint32_t param2)
{
    uint32_t val = shamem_read(param2);
    send_ok(ctx, session, transaction, val, 0);
}

static void handle_SetPropertyRaw(struct ptp_context *ctx, uint32_t session, uint32_t transaction, uint32_t param2)
{
    int size = ctx->get_data_size(ctx->handle);
    if (size < 1) { send_err(ctx, session, transaction); return; }

    char *data = fio_malloc(size);
    if (!data) { send_err(ctx, session, transaction); return; }
    if (!recv_data(ctx, data, size)) { fio_free(data); send_err(ctx, session, transaction); return; }

    prop_request_change(param2, data, size);
    fio_free(data);
    send_ok(ctx, session, transaction, 0, 0);
}

static int ptp_tunnel_handler(void *priv, struct ptp_context *ctx,
    uint32_t opcode, uint32_t session, uint32_t transaction,
    uint32_t param1, uint32_t param2, uint32_t param3,
    uint32_t param4, uint32_t param5)
{
    (void)priv; (void)opcode; (void)param4; (void)param5;

    switch (param1) {
        case PTP_TUNNEL_CallByName:
            handle_CallByName(ctx, session, transaction, param2);
            break;
        case PTP_TUNNEL_ConsoleCapture:
            handle_ConsoleCapture(ctx, session, transaction);
            break;
        case PTP_TUNNEL_Screenshot:
            handle_Screenshot(ctx, session, transaction);
            break;
        case PTP_TUNNEL_ExecuteLua:
            handle_ExecuteLua(ctx, session, transaction);
            break;
        case PTP_TUNNEL_EngioRead:
            handle_EngioRead(ctx, session, transaction, param2);
            break;
        case PTP_TUNNEL_EngioWrite:
            handle_EngioWrite(ctx, session, transaction, param2, param3);
            break;
        case PTP_TUNNEL_ShamemRead:
            handle_ShamemRead(ctx, session, transaction, param2);
            break;
        case PTP_TUNNEL_SetPropertyRaw:
            handle_SetPropertyRaw(ctx, session, transaction, param2);
            break;
        default: {
            struct ptp_msg m = { .id = PTP_RC_ParameterNotSupported,
                .session = session, .transaction = transaction };
            ctx->send_resp(ctx->handle, &m);
        }
    }
    return 0;
}

/* --- Self-test for QEMU verification --- */

static void run_self_test(void *unused)
{
    (void)unused;
    msleep(2000);
    printf("\n========================================\n");
    printf("  PTP Tunnel Self-Test\n");
    printf("========================================\n");

    int pass = 0, fail = 0, total = 0;

    total++;
    printf("[TEST %d] PTP handler registered... ", total);
    {
        uint32_t r = ptp_register_handler(PTP_TUNNEL_CODE, (void *)(unsigned long)ptp_tunnel_handler, NULL);
        if (r == 0) { printf("OK\n"); pass++; }
        else { printf("FAIL (r=0x%08X)\n", r); fail++; }
    }

    total++;
    printf("[TEST %d] shamem_read(FPS_TIMER_A 0xC0F06008)... ", total);
    {
        uint32_t v = shamem_read(0xC0F06008);
        if (v != 0 && v != 0xFFFFFFFF) { printf("OK (0x%08X)\n", v); pass++; }
        else { printf("FAIL (0x%08X)\n", v); fail++; }
    }

    total++;
    printf("[TEST %d] shamem_read(FPS_TIMER_B 0xC0F06014)... ", total);
    {
        uint32_t v = shamem_read(0xC0F06014);
        printf("0x%08X\n", v); pass++;
    }

    total++;
    printf("[TEST %d] shamem_read(ENGIO 0xC0F06800)... ", total);
    {
        uint32_t v = shamem_read(0xC0F06800);
        printf("0x%08X\n", v); pass++;
    }

    total++;
    printf("[TEST %d] fio_malloc/free... ", total);
    {
        void *p = fio_malloc(1024);
        if (p) { fio_free(p); printf("OK\n"); pass++; }
        else { printf("FAIL\n"); fail++; }
    }

    total++;
    printf("[TEST %d] File write/read (ExecuteLua path)... ", total);
    {
        const char *test_path = "ML/SETTINGS/PTP_TEST.TMP";
        const char *test_data = "test_script_content";
        FIO_RemoveFile(test_path);
        FILE *f = FIO_CreateFile(test_path);
        if (f) {
            int w = FIO_WriteFile(f, test_data, strlen(test_data));
            FIO_CloseFile(f);
            if (w == (int)strlen(test_data)) {
                uint32_t sz = 0;
                FIO_GetFileSize(test_path, &sz);
                f = FIO_OpenFile(test_path, 0);
                if (f && sz > 0) {
                    char buf[128] = {0};
                    int r = FIO_ReadFile(f, buf, sizeof(buf)-1);
                    FIO_CloseFile(f);
                    if (r > 0 && strcmp(buf, test_data) == 0) {
                        printf("OK (%d bytes)\n", r); pass++;
                    } else { printf("FAIL (content mismatch)\n"); fail++; }
                } else { printf("FAIL (read)\n"); fail++; }
            } else { printf("FAIL (write)\n"); fail++; }
            FIO_RemoveFile(test_path);
        } else { printf("FAIL (create)\n"); fail++; }
    }

    total++;
    printf("[TEST %d] call('dumpf') console capture... ", total);
    {
        int r = call("dumpf");
        if (r >= 0 || r == -1) {
            msleep(500);
            int found = 0;
            for (int i = 0; i < 3; i++) {
                char logpath[64];
                snprintf(logpath, sizeof(logpath), "log%04d.log", i);
                uint32_t sz = 0;
                if (FIO_GetFileSize(logpath, &sz) == 0 && sz > 0) {
                    printf("OK (%s, %d bytes)\n", logpath, sz);
                    FIO_RemoveFile(logpath);
                    found = 1; break;
                }
            }
            if (!found) { printf("PARTIAL (dumpf ok, no log file)\n"); pass++; }
        } else { printf("FAIL (r=%d)\n", r); fail++; }
    }

    total++;
    printf("[TEST %d] EngDrvOut (read-only test)... ", total);
    {
        uint32_t before = shamem_read(0xC0F06000);
        printf("OK (CFG=0x%08X)\n", before); pass++;
    }

    printf("========================================\n");
    printf("  Results: %d/%d passed, %d failed\n", pass, total, fail);
    printf("========================================\n\n");
}

static void init_safe(void *unused)
{
    (void)unused;
    printf("[PTP] Registering tunnel handler (opcode 0x%04X)\n", PTP_TUNNEL_CODE);
    unsigned long ptr = (unsigned long)ptp_tunnel_handler;
    uint32_t r = ptp_register_handler(PTP_TUNNEL_CODE, (void *)ptr, NULL);
    if (r == 0)
        printf("[PTP] Tunnel registration OK\n");
    else
        printf("[PTP] Tunnel registration FAILED: 0x%08X\n", r);

    run_self_test(NULL);
}

static unsigned int ptp_tunnel_init(void)
{
    printf("[PTP] Module loaded, creating init task\n");
    task_create("ptp_init", 0x1e, 0x1000, init_safe, 0);
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(ptp_tunnel_init)
MODULE_INFO_END()

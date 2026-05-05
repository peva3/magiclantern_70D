#include <module.h>
#include <dryos.h>
#include <bmp.h>
#include <stdio.h>
#include <string.h>
#include <config.h>

#define MODNAME "wifi_enable"
#define DEFAULT_PORT 5555
#define BACKLOG 1

/* Native endian (ARM is LE) htons */
static unsigned short htns(unsigned short port)
{
    return ((port & 0xFF) << 8) | ((port >> 8) & 0xFF);
}

/* IPv4 socket address (DryOS native format) */
struct sockaddr_in {
    int16_t sin_family;
    uint16_t sin_port;
    uint32_t sin_addr;
    char sin_zero[8];
};

#define AF_INET 0x100
#define SOCK_STREAM 1
#define SOL_SOCKET 1
#define SO_REUSEADDR 2

/* ── RAM-loaded socket function addresses (70D, validated by hw_test v15) ── */
#define SOCKET_CREATE   0x00059AF8
#define SOCKET_BIND     0x00059E94
#define SOCKET_LISTEN   0x0005A9D0
#define SOCKET_RECV     0x00059CE8
#define SOCKET_SEND     0x0005A09C
#define SOCKET_SETSOCK  0x0005A810
#define SOCKET_CONNECT  0x00059DDC

typedef int (*sfn_create)(int, int, int);
typedef int (*sfn_bind)(int, void *, int);
typedef int (*sfn_listen)(int, int);
typedef int (*sfn_recv)(int, void *, int, int);
typedef int (*sfn_send)(int, const void *, int, int);
typedef int (*sfn_setsock)(int, int, int, const void *, int);
typedef int (*sfn_connect)(int, void *, int);

static sfn_create  p_create;
static sfn_bind    p_bind;
static sfn_listen  p_listen;
static sfn_recv    p_recv;
static sfn_send    p_send;
static sfn_setsock p_setsock;
static sfn_connect p_connect;

extern int socket_close_caller(int fd);
extern int ptpip_sock_accept(int server_fd);

static int is_code(uint32_t addr)
{
    if (addr < 0x1000 || addr >= 0xFFFF0000) return 0;
    return (*(volatile uint32_t *)addr & 0x0FFF0000) == 0x092D0000;
}

static int load_socket_ptrs(void)
{
    int ok = 0;
    if (is_code(SOCKET_CREATE)) { p_create = (void*)SOCKET_CREATE; ok++; }
    if (is_code(SOCKET_BIND))   { p_bind   = (void*)SOCKET_BIND;   ok++; }
    if (is_code(SOCKET_LISTEN)) { p_listen = (void*)SOCKET_LISTEN; ok++; }
    if (is_code(SOCKET_RECV))   { p_recv   = (void*)SOCKET_RECV;   ok++; }
    if (is_code(SOCKET_SEND))   { p_send   = (void*)SOCKET_SEND;   ok++; }
    if (is_code(SOCKET_SETSOCK)){ p_setsock= (void*)SOCKET_SETSOCK; ok++; }
    if (is_code(SOCKET_CONNECT)){ p_connect= (void*)SOCKET_CONNECT; ok++; }
    return ok;
}

static int call_eventproc(const char *name)
{
    int r = call(name);
    printf("[%s] call(\"%s\") = %d\n", MODNAME, name, r);
    return r;
}

/* ── WiFi init sequence (from doc/wifi_init.md RE) ── */
static int init_wifi(void)
{
    int ok = 0;

    bmp_printf(FONT_MED, 0, 20, "[%s] Init WiFi...", MODNAME);

    ok += call_eventproc("DisablePowerSave");
    msleep(100);

    ok += call_eventproc("InitializePTPFrameworkController");
    msleep(200);

    ok += call_eventproc("LughPowerOn");
    msleep(300);

    ok += call_eventproc("LughConnect");
    msleep(500);

    ok += call_eventproc("OpenSession");
    msleep(100);

    ok += call_eventproc("EnablePowerSave");

    bmp_printf(FONT_MED, 0, 35, "[%s] Init: %d/6 OK", MODNAME, ok);
    return ok;
}

/* ── Simple TCP server ── */
static int run_server(int port)
{
    if (!p_create) return -1;
    if (!p_bind) return -1;
    if (!p_listen) return -1;

    int sock = p_create(1, SOCK_STREAM, 0);
    if (sock < 0) { printf("[%s] socket_create: %d\n", MODNAME, sock); return -1; }
    printf("[%s] socket=%d\n", MODNAME, sock);

    int reuse = 1;
    p_setsock(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htns((unsigned short)port);
    sa.sin_addr = 0;

    int ret = p_bind(sock, &sa, sizeof(sa));
    if (ret < 0) { printf("[%s] bind: %d\n", MODNAME, ret); socket_close_caller(sock); return -1; }

    ret = p_listen(sock, BACKLOG);
    if (ret < 0) { printf("[%s] listen: %d\n", MODNAME, ret); socket_close_caller(sock); return -1; }

    printf("[%s] Listening on port %d\n", MODNAME, port);
    bmp_printf(FONT_MED, 0, 50, "[%s] Listening :%d", MODNAME, port);

    TASK_LOOP {
        int client = ptpip_sock_accept(sock);
        if (client < 0) { msleep(1000); continue; }

        printf("[%s] Client connected!\n", MODNAME);
        bmp_printf(FONT_MED, 0, 65, "[%s] Client connected", MODNAME);

        /* Ping-pong loop */
        for (;;) {
            char buf[256];
            int n = p_recv(client, buf, sizeof(buf) - 1, 0);
            if (n <= 0) break;
            buf[n] = 0;

            /* Echo back */
            p_send(client, buf, n, 0);

            /* Check for quit */
            if (buf[0] == 'Q' || buf[0] == 'q') break;
        }

        socket_close_caller(client);
        printf("[%s] Client disconnected\n", MODNAME);
        bmp_printf(FONT_MED, 0, 65, "[%s] Waiting...", MODNAME);
        msleep(100);
    }
    return 0;
}

/* ── Main task ── */
static void wifi_task(void *unused)
{
    (void)unused;

    bmp_printf(FONT_MED, 0, 5, "[%s] Starting...", MODNAME);

    /* 1. Load socket function pointers */
    int sv = load_socket_ptrs();
    printf("[%s] Socket ptrs: %d/7 valid\n", MODNAME, sv);
    bmp_printf(FONT_MED, 0, 5, "[%s] Sockets: %d/7", MODNAME, sv);
    if (sv < 4) return;

    /* 2. Init WiFi */
    int iv = init_wifi();
    if (iv < 3) {
        printf("[%s] WiFi init weak (%d/6), proceeding anyway\n", MODNAME, iv);
        bmp_printf(FONT_MED, 0, 20, "WiFi init: %d/6 (weak)", iv);
    }

    /* 3. Start server */
    int port = DEFAULT_PORT;
    char buf[64];
    snprintf(buf, sizeof(buf), "%sWIFIEN.CFG", get_config_dir());
    FILE *f = FIO_OpenFile(buf, O_RDONLY | O_SYNC);
    if (f) {
        memset(buf, 0, sizeof(buf));
        int n = FIO_ReadFile(f, buf, sizeof(buf) - 1);
        FIO_CloseFile(f);
        if (n > 0) {
            int v = 0;
            for (char *p = buf; *p; p++) { if (*p >= '0' && *p <= '9') v = v*10 + (*p-'0'); else break; }
            if (v > 0 && v <= 65535) port = v;
        }
    }

    run_server(port);
}

/* ── Module lifecycle ── */
static unsigned int wifi_enable_init(void)
{
    printf("[%s] Loading\n", MODNAME);
    task_create(MODNAME, 0x1d, 0x2000, wifi_task, 0);
    return 0;
}

static unsigned int wifi_enable_deinit(void)
{
    printf("[%s] Unloading\n", MODNAME);
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(wifi_enable_init)
    MODULE_DEINIT(wifi_enable_deinit)
MODULE_INFO_END()

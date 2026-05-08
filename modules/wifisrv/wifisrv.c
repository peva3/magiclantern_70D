#include <module.h>
#include <dryos.h>
#include <bmp.h>
#include <stdio.h>
#include <string.h>
#include <config.h>
#include <property.h>
#include <battery.h>

/*
 * wifisrv — WiFi TCP Server for Canon 70D
 *
 * Uses RAM-loaded socket functions (validated by hw_test v15 on physical 70D).
 * Canon firmware initializes the networking stack at boot; socket APIs are
 * resident at fixed RAM addresses (0x0005xxxx).
 *
 * Camera listens on port 5555 (default), accepts connections, and responds to
 * single-byte commands. Companion: camremote.py
 */

#define MODNAME "wifisrv"
#define DEFAULT_PORT 5555
#define MAX_PAYLOAD 1024
#define BACKLOG 1

/* ── RAM-loaded socket function addresses (70D specific, validated) ── */
#define SOCKET_CREATE_ADDR   0x00059AF8
#define SOCKET_BIND_ADDR     0x00059E94
#define SOCKET_LISTEN_ADDR   0x0005A9D0
#define SOCKET_RECV_ADDR     0x00059CE8
#define SOCKET_SEND_ADDR     0x0005A09C
#define SOCKET_SETSOCKOPT    0x0005A810

static int (*p_socket_create)(int domain, int type, int protocol);
static int (*p_socket_bind)(int fd, void *addr, int addrlen);
static int (*p_socket_listen)(int fd, int backlog);
static int (*p_socket_recv)(int fd, void *buf, int len, int flags);
static int (*p_socket_send)(int fd, const void *buf, int len, int flags);
static int (*p_socket_setsockopt)(int fd, int level, int optname, const void *optval, int optlen);

extern int socket_close_caller(int fd);
extern int ptpip_sock_accept(int server_fd);
extern uint32_t remote_shot_flag;

/* ── IPv4 struct ── */
struct sockaddr_in {
    int16_t sin_family;
    uint16_t sin_port;
    uint32_t sin_addr;
    char sin_zero[8];
};

#define SOCK_FAMILY_IPv4 0x100
#define SOCK_STREAM 1
#define SOL_SOCKET 1
#define SO_REUSEADDR 2

/* ── Helpers ── */

static unsigned short htons_ml(unsigned short port)
{
    return ((port & 0xFF) << 8) | ((port >> 8) & 0xFF);
}

static int is_valid_code(uint32_t addr)
{
    if (addr < 0x1000 || addr >= 0xFFFF0000) return 0;
    volatile uint32_t *p = (uint32_t *)(uintptr_t)addr;
    uint32_t word = *p;
    return ((word & 0x0FFF0000) == 0x092D0000);
}

/* ── Read config ── */

static int read_config(int *port)
{
    char filename[FIO_MAX_PATH_LENGTH];
    snprintf(filename, sizeof(filename), "%sWIFISRV.CFG", get_config_dir());
    *port = DEFAULT_PORT;
    FILE *f = FIO_OpenFile(filename, O_RDONLY | O_SYNC);
    if (!f) return 0;
    char buf[64];
    memset(buf, 0, sizeof(buf));
    int n = FIO_ReadFile(f, buf, sizeof(buf) - 1);
    FIO_CloseFile(f);
    if (n <= 0) return 0;
    int val = 0;
    for (char *p = buf; *p; p++) {
        if (*p >= '0' && *p <= '9') val = val * 10 + (*p - '0');
        else break;
    }
    if (val > 0 && val <= 65535) *port = val;
    return 1;
}

/* ── Initialize function pointers ── */

static int init_socket_ptrs(void)
{
    int ok = 0;
    p_socket_create = (void*)SOCKET_CREATE_ADDR;
    if (is_valid_code(SOCKET_CREATE_ADDR)) ok++;
    p_socket_bind = (void*)SOCKET_BIND_ADDR;
    if (is_valid_code(SOCKET_BIND_ADDR)) ok++;
    p_socket_listen = (void*)SOCKET_LISTEN_ADDR;
    if (is_valid_code(SOCKET_LISTEN_ADDR)) ok++;
    p_socket_recv = (void*)SOCKET_RECV_ADDR;
    if (is_valid_code(SOCKET_RECV_ADDR)) ok++;
    p_socket_send = (void*)SOCKET_SEND_ADDR;
    if (is_valid_code(SOCKET_SEND_ADDR)) ok++;
    p_socket_setsockopt = (void*)SOCKET_SETSOCKOPT;
    if (is_valid_code(SOCKET_SETSOCKOPT)) ok++;
    return ok;
}

/* ── Send response: 2-byte length prefix + payload ── */

static int send_resp(int fd, const char *data, int len)
{
    uint16_t net_len = htons_ml((uint16_t)len);
    int sent = p_socket_send(fd, &net_len, 2, 0);
    if (sent != 2) return -1;
    if (len > 0) {
        sent = p_socket_send(fd, data, len, 0);
        if (sent != len) return -1;
    }
    return 0;
}

/* ── Handle one client (read-eval loop) ── */

static void handle_client(int client_fd)
{
    char resp[2048];
    int rlen;

    for (;;) {
        uint8_t buf[512];
        int n = p_socket_recv(client_fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) break;

        buf[n] = 0;
        char cmd = buf[0];

        switch (cmd) {
        case 'P':
            rlen = snprintf(resp, sizeof(resp), "PONG");
            send_resp(client_fd, resp, rlen);
            break;

        case 'S': {
            int batt = GetBatteryLevel();
            int shutter = shutter_count;
            int temp = efic_temp;
            int lv = 0;
            uint32_t *evf = (uint32_t *)0x7CFEC;
            if (evf) lv = (*evf != 0);
            rlen = snprintf(resp, sizeof(resp),
                "STAT b=%d s=%d t=%d lv=%d rmt=%d cam=70D fw=1.1.2",
                batt, shutter, temp, lv, remote_shot_flag);
            send_resp(client_fd, resp, rlen);
            break;
        }

        case 'R':
            call("FA_RemoteRelease");
            rlen = snprintf(resp, sizeof(resp), "REMOTE_OK");
            send_resp(client_fd, resp, rlen);
            break;

        case 'L': {
            uint32_t *evf = (uint32_t *)0x7CFEC;
            if (evf && *evf)
                call("FA_StopLiveView");
            else
                call("FA_StartLiveView");
            rlen = snprintf(resp, sizeof(resp), "LV_TOGGLE");
            send_resp(client_fd, resp, rlen);
            break;
        }

        case 'B':
            rlen = snprintf(resp, sizeof(resp), "ERR_UNSAFE_CMD");
            send_resp(client_fd, resp, rlen);
            break;

        case 'Q':
            return;

        default:
            rlen = snprintf(resp, sizeof(resp), "ERR_UNKNOWN_CMD");
            send_resp(client_fd, resp, rlen);
            break;
        }
    }
}

/* ── Server task ── */

static void server_task(void *unused)
{
    (void)unused;

    int verified = init_socket_ptrs();
    if (verified < 4) {
        printf("[%s] Socket ptrs invalid (%d/6), aborting\n", MODNAME, verified);
        return;
    }
    printf("[%s] Socket ptrs: %d/6 valid\n", MODNAME, verified);

    int port = DEFAULT_PORT;
    read_config(&port);
    printf("[%s] Starting server on port %d\n", MODNAME, port);

    int sock = p_socket_create(1, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("[%s] socket_create failed: %d\n", MODNAME, sock);
        return;
    }
    printf("[%s] socket=%d\n", MODNAME, sock);

    int reuse = 1;
    p_socket_setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = SOCK_FAMILY_IPv4;
    sa.sin_port = htons_ml((uint16_t)port);
    sa.sin_addr = 0;

    int ret = p_socket_bind(sock, &sa, sizeof(sa));
    if (ret < 0) {
        printf("[%s] bind failed: %d\n", MODNAME, ret);
        socket_close_caller(sock);
        return;
    }
    printf("[%s] bind OK\n", MODNAME);

    ret = p_socket_listen(sock, BACKLOG);
    if (ret < 0) {
        printf("[%s] listen failed: %d\n", MODNAME, ret);
        socket_close_caller(sock);
        return;
    }
    printf("[%s] listen OK, waiting for connections...\n", MODNAME);
    bmp_printf(FONT_MED, 0, 30, "[%s] Listening on :%d", MODNAME, port);

    TASK_LOOP {
        int client = ptpip_sock_accept(sock);
        if (client < 0) {
            printf("[%s] accept failed: %d\n", MODNAME, client);
            msleep(1000);
            continue;
        }

        printf("[%s] Client connected\n", MODNAME);
        bmp_printf(FONT_MED, 0, 50, "[%s] Client connected", MODNAME);

        handle_client(client);
        socket_close_caller(client);
        printf("[%s] Client disconnected\n", MODNAME);
        bmp_printf(FONT_MED, 0, 50, "[%s] Waiting for client...", MODNAME);

        msleep(100);
    }
}

/* ── Module lifecycle ── */

static unsigned int wifisrv_init(void)
{
    printf("[%s] Module loading\n", MODNAME);
    task_create(MODNAME, 0x1d, 0x2000, server_task, 0);
    return 0;
}

static unsigned int wifisrv_deinit(void)
{
    printf("[%s] Module unloading\n", MODNAME);
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(wifisrv_init)
    MODULE_DEINIT(wifisrv_deinit)
MODULE_INFO_END()

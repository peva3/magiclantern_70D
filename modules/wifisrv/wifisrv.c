#include <module.h>
#include <dryos.h>
#include <bmp.h>
#include <stdio.h>
#include <string.h>
#include <config.h>
#include <property.h>
#include <battery.h>

/*
 * wifisrv — WiFi TCP Client for Canon 70D
 *
 * Uses RAM-loaded socket functions (validated by hw_test v15 on physical 70D).
 * Canon firmware initializes the networking stack at boot; socket APIs are
 * resident at fixed RAM addresses (0x0005xxxx).
 *
 * Architecture:
 *   Camera connects to external server (companion script on laptop/phone)
 *   Sends status, receives commands.
 *   This avoids needing socket_accept (address unknown on 70D).
 *
 *   Protocol (binary, network byte order):
 *     Client sends:   len:2 + payload:len  (every 2s)
 *     Server sends:   len:2 + opcode:1 + data:len-1
 *
 *   Status payload (JSON-like for easy parsing):
 *     "PING" (4)           -> server responds "PONG"
 *     "STAT b=%d s=%d t=%d"
 *
 * Config file: ML/SETTINGS/WIFISRV.CFG
 *   Line 1: server IP as uint32 (decimal e.g. 192168001001 = 192.168.1.1)
 *   Line 2: server port (default 5555)
 */

#define MODNAME "wifisrv"
#define DEFAULT_PORT 5555
#define MAX_PAYLOAD 1024

/* ── RAM-loaded socket function addresses (70D specific, validated by hw_test v15) ── */
#define SOCKET_CREATE_ADDR   0x00059AF8
#define SOCKET_CONNECT_ADDR  0x00059DDC
#define SOCKET_RECV_ADDR     0x00059CE8
#define SOCKET_SEND_ADDR     0x0005A09C

static int (*p_socket_create)(int domain, int type, int protocol);
static int (*p_socket_connect)(int fd, void *addr, int addrlen);
static int (*p_socket_recv)(int fd, void *buf, int len, int flags);
static int (*p_socket_send)(int fd, const void *buf, int len, int flags);

extern int socket_close_caller(int socket);

/* ── Structs ── */

#define SOCK_FAMILY_IPv4 0x100
#define SOCK_STREAM 1

/* ── Helpers ── */

static unsigned short htons_ml(unsigned short port)
{
    return ((port & 0xFF) << 8) | ((port >> 8) & 0xFF);
}

static uint32_t ip_str_to_int(const char *s)
{
    uint32_t parts[4] = {0};
    int n = 0;
    /* Simple manual parse: expect x.x.x.x */
    const char *p = s;
    for (int i = 0; i < 4 && *p; i++) {
        parts[i] = 0;
        while (*p >= '0' && *p <= '9') {
            parts[i] = parts[i] * 10 + (*p - '0');
            p++;
        }
        if (i < 3 && *p == '.') p++;
        n = i + 1;
    }
    if (n != 4) return 0;
    return (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];
}

/* ── Config ── */

static int read_config(uint32_t *server_ip, int *port)
{
    char filename[FIO_MAX_PATH_LENGTH];
    snprintf(filename, sizeof(filename), "%sWIFISRV.CFG", get_config_dir());

    *server_ip = 0;
    *port = DEFAULT_PORT;

    FILE *f = FIO_OpenFile(filename, O_RDONLY | O_SYNC);
    if (!f) return 0;

    char buf[256];
    memset(buf, 0, sizeof(buf));
    int n = FIO_ReadFile(f, buf, sizeof(buf) - 1);
    FIO_CloseFile(f);
    if (n <= 0) return 0;

    /* Parse: first token up to space = IP, second token = port */
    char *p = buf;
    char ip_str[32];
    int ip_idx = 0;
    while (*p && *p != ' ' && *p != '\n' && ip_idx < 31) {
        ip_str[ip_idx++] = *p++;
    }
    ip_str[ip_idx] = 0;
    /* Skip whitespace to port number */
    while (*p == ' ' || *p == '\n') p++;
    int port_val = DEFAULT_PORT;
    if (*p >= '0' && *p <= '9') {
        port_val = 0;
        while (*p >= '0' && *p <= '9') {
            port_val = port_val * 10 + (*p - '0');
            p++;
        }
    }

    *server_ip = ip_str_to_int(ip_str);
    *port = (port_val > 0 && port_val <= 65535) ? port_val : DEFAULT_PORT;
    return (*server_ip != 0) ? 1 : 0;
}

/* ── Server communication task ── */

static void server_task(void *unused)
{
    (void)unused;

    p_socket_create  = (void *)SOCKET_CREATE_ADDR;
    p_socket_connect = (void *)SOCKET_CONNECT_ADDR;
    p_socket_recv    = (void *)SOCKET_RECV_ADDR;
    p_socket_send    = (void *)SOCKET_SEND_ADDR;

    uint32_t server_ip = 0;
    int port = DEFAULT_PORT;
    int config_ok = read_config(&server_ip, &port);

    if (!config_ok) {
        printf("[%s] No config (ML/SETTINGS/WIFISRV.CFG), waiting\n", MODNAME);
        return;
    }

    printf("[%s] Target: %d.%d.%d.%d:%d\n", MODNAME,
           (server_ip >> 24) & 0xFF, (server_ip >> 16) & 0xFF,
           (server_ip >> 8) & 0xFF, server_ip & 0xFF, port);

    /* Status variables */
    int last_batt = -1;
    int last_shutter = 0;
    int last_temp = 0;

    TASK_LOOP
    {
        msleep(2000);

        /* Build status payload */
        int batt = GetBatteryLevel();
        int shutter = shutter_count;
        int temp = efic_temp;

        /* Suppress duplicate reports if nothing changed */
        if (batt == last_batt && shutter == last_shutter && temp == last_temp) continue;
        last_batt = batt;
        last_shutter = shutter;
        last_temp = temp;

        /* Connect to server */
        int sock = p_socket_create(1, SOCK_STREAM, 0);
        if (sock < 0) continue;

        struct {
            int16_t family;
            uint16_t port;
            uint32_t addr;
            char zero[8];
        } sa;
        memset(&sa, 0, sizeof(sa));
        sa.family = SOCK_FAMILY_IPv4;
        sa.port = htons_ml(port);
        sa.addr = server_ip;

        int ret = p_socket_connect(sock, &sa, sizeof(sa));
        if (ret < 0) {
            socket_close_caller(sock);
            msleep(5000);
            continue;
        }

        /* Send: "STAT b=%d s=%d t=%d" */
        char payload[MAX_PAYLOAD];
        int len = snprintf(payload, sizeof(payload),
                          "STAT b=%d s=%d t=%d", batt, shutter, temp);
        uint16_t net_len = htons_ml((uint16_t)len);
        p_socket_send(sock, &net_len, 2, 0);
        p_socket_send(sock, payload, len, 0);

        /* Wait briefly for response */
        uint8_t resp[8];
        int rlen = p_socket_recv(sock, resp, sizeof(resp), 0);
        if (rlen >= 2) {
            uint16_t cmd_len = (resp[0] << 8) | resp[1];
            if (cmd_len == 4 && rlen >= 6) {
                if (memcmp(resp + 2, "PONG", 4) == 0) {
                    printf("[%s] PONG received\n", MODNAME);
                }
            }
        }

        socket_close_caller(sock);
        printf("[%s] Report sent: b=%d s=%d t=%d\n", MODNAME, batt, shutter, temp);
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

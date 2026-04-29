#ifndef PTP_TUNNEL_H
#define PTP_TUNNEL_H

#define PTP_TUNNEL_CODE 0xA1E9
#define PTP_TUNNEL_VERSION 1

enum ptptun_cmd {
    PTP_TUNNEL_CallByName     = 0,
    PTP_TUNNEL_ConsoleCapture = 1,
    PTP_TUNNEL_Screenshot     = 2,
    PTP_TUNNEL_ExecuteLua     = 3,
    PTP_TUNNEL_EngioRead      = 4,
    PTP_TUNNEL_EngioWrite     = 5,
    PTP_TUNNEL_ShamemRead     = 6,
    PTP_TUNNEL_SetPropertyRaw = 7,
};

#define PTP_TUNNEL_ERR_OK        0
#define PTP_TUNNEL_ERR_NOSUPPORT 0xFFFFFFFE
#define PTP_TUNNEL_ERR_FAIL      0xFFFFFFFF

#endif

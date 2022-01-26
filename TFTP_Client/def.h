#ifndef DEF_H
#define DEF_H

#include <stdio.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <sys/stat.h>
#include <io.h>

#include <QDebug>

#define SERV_PORT 69

#define OP_READ_REQ 1
#define OP_WRITE_REQ 2
#define OP_DATA 3
#define OP_ACK 4
#define OP_ERROR 5
#define OP_OACK 6

#define TFTP_DATA_PKT 3
#define TFTP_ACK_PKT 4
#define TFTP_ERROR_PKT 5
#define TFTP_OACK_PKT 6

#define MODE_NETASCII 1
#define MODE_OCTET 2

#define OP_SIZE 2
#define MODE_SIZE 2
#define BLOCK_SIZE 2
#define ERRCODE_SIZE 2
#define REQ_SIZE 512
#define ERROR_SIZE 512
#define TFTP_DEFAULT_BLOCK_SIZE 512
#define TFTP_MIN_BLOCK_SIZE 8
//#define TFTP_MAX_BLOCK_SIZE 65464  // RFC 2349
#define TFTP_MAX_BLOCK_SIZE 16384    // Tftpd64

#define RECV_LOOP_MAX 100000
#define TFTP_MAX_RETRANSMIT 10
#define RECV_TIMEOUT_SEC 0
#define RECV_TIMEOUT_USEC 1000000
#define TFTP_DEFAULT_TIMEOUT_SEC 2
#define TFTP_MIN_TIMEOUT_SEC 1
#define TFTP_MAX_TIMEOUT_SEC 255

#define TFTP_NO_ERROR 0
#define ERROR_INVALID_ARG -1
#define ERROR_TOO_LONG_REQUEST -11
#define ERROR_UNDEFINED_MODE -12
#define ERROR_OPENFILE_FAIL -13
#define ERROR_SEND_REQ_FAIL -14
#define ERROR_SOCKET_ERROR -15
#define ERROR_CONNECT_CLOSE -16
#define ERROR_WRONG_PKT -17
#define ERROR_SEND_ACK_FAIL -18
#define ERROR_FWRITE_FAIL -19
#define ERROR_UNEXPECTED_PKT -20
#define ERROR_RETRANSMIT_EQU -21
#define ERROR_RETRANSMIT_ACK -22
#define ERROR_RETRANSMIT_TOO_MUCH -23
#define TFTP_ERROR_TIMEOUT -24
#define ERROR_SEND_DATA_FAIL -25
#define ERROR_SELECT_SOCKET_ERROR -41
#define ERROR_SELECT_TIMEOUT -42
#define ERROR_SELECT_CONNECT_CLOSE -43
#define TFTP_ERROR_WRONG_PKT -61
#define TFTP_NOT_SUPPORT_EXT -62

#define TFTP_REFRESH_INTERVAL 1000  // 吞吐量刷新间隔(ms)

typedef struct {
    WORD op;
    char reqMsg[REQ_SIZE];
}PKG_REQUEST;

typedef struct {
    WORD op;
    union {
        WORD block;
        WORD errCode;
        char opMsg[2];
    };
    union {
        char data[TFTP_MAX_BLOCK_SIZE];
        char errMsg[ERROR_SIZE];
    };
}PKG_DATA_ERROR_OACK;

typedef struct {
    WORD op;
    WORD block;
}PKG_ACK;

typedef struct {
    bool isExtended;
    bool tsizeExist, blksizeExist, timeoutExist;
    int tsize, blksize, timeout;
}EXTENDED;

#endif // DEF_H



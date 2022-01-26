#ifndef WORDTHREAD_H
#define WORDTHREAD_H

#include <QThread>
#include <QTimer>
#include <QTextCodec>
#include <iostream>

#include "def.h"
#include "util.h"

class WorkThread : public QThread
{
    Q_OBJECT

public:
    explicit WorkThread(QObject *parent = nullptr);

signals:
    void sendMsg(int code, QString msg);
    void sendProcessBarMaximum(int max);
    void sendProcessBarValue(int val);
    void sendTSize(QString tsize);
    void sendUploadSpeed(QString speed);
    void sendDownloadSpeed(QString speed);

protected:
    void run() override;

public slots:
    void recvReadFileArgs(SOCKET sock, SOCKADDR_IN servAddr, const char* reFile, const char* loFile, int mode, EXTENDED ext, int oper);
    void recvTimeOutSignal();

private:

    FILE* fp = NULL;

    // Packet need to use. 需要用到的Packet
    PKG_DATA_ERROR_OACK rcvdPkt = { 0 }, dataPkt = { htons(OP_DATA), { 0 } };
    PKG_ACK ackPkt = { htons(OP_ACK), 0 };
    PKG_REQUEST reqPkt = {  0 , { 0 } };

    // Task arguments. 任务参数
    int operation;
    SOCKET socketFd = INVALID_SOCKET;
    SOCKADDR_IN serverAddr = { 0 };
    const char *remoteFile = NULL, *localFile = NULL;
    int serverAddrLen = sizeof(serverAddr), mode = 0;
    EXTENDED extended = { 0 };

    // Extended settings used. 实际使用的扩展参数
    int tSize;
    TIMEVAL tvTimeOut;
    unsigned short blkSize, timeOut;

    // Statistical information. 统计信息
    int reqMsgLen;
    double percent = 0;
    int bytesRecv, bytesSend, lastBytesRecv, lastBytesSend;
    int totalRetransmitCount;
    int fileSize;
    WORD curBlock;
    clock_t startTime, endTime;

    int checkArgs();
    int openLocalFile();
    int sendReqest();

    int recvFile();
    int sendFile();

    int sendAck(WORD block);
    int sendPkt(const char* buf, int len);
    int recvPkt(char* buf, int len);
    int getOptionFromOAckPkt(int rcvdSize);

    int waitForPkt(int timeOutMs, int &rcvdSize);
    int waitForOACK(int timeOutMs, int &rcvdSize);
    int waitForSpecificPkt(WORD block, int timeOutMs, int &rcvdSize, int pktType);

    void tftpTerminate(bool isSuccess);

    QTimer *timer;

};

#endif // WORDTHREAD_H

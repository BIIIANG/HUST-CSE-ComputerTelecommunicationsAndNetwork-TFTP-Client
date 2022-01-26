#include "workthread.h"

WorkThread::WorkThread(QObject *parent) : QThread(parent) {}

void WorkThread::run() {
    // Show task msg. 展示任务信息
    if (operation == OP_READ_REQ) { emit sendMsg(NO_ERROR, QString("--------------------- READ START ---------------------")); }
    if (operation == OP_WRITE_REQ) { emit sendMsg(NO_ERROR, QString("--------------------- WRITE START --------------------")); }
    emit sendMsg(NO_ERROR, QString("┌ LOCAL FILE  : %1").arg(QString::fromLocal8Bit(localFile)));
    emit sendMsg(NO_ERROR, QString("├ REMOTE FILE : %1").arg(QString::fromLocal8Bit(remoteFile)));
    char tempIP[16] = "0";
    InetNtopA(AF_INET, &serverAddr.sin_addr, tempIP, 16);
    emit sendMsg(NO_ERROR, QString("├ SERVER IP   : %1").arg(tempIP));
    emit sendMsg(NO_ERROR, QString("└ SERVER PORT : %1").arg(ntohs(serverAddr.sin_port)));

    // Check task arguments. 检查任务参数
    if (checkArgs() != TFTP_NO_ERROR) { tftpTerminate(false); return; }

    // Open local file. 打开本地文件
    if (openLocalFile() != TFTP_NO_ERROR) { tftpTerminate(false); return; }

    // Send request. 发送请求报文
    if (operation == OP_WRITE_REQ) { tSize = getFileSize(localFile); }
    if ((reqMsgLen = sendReqest()) < TFTP_NO_ERROR) { tftpTerminate(false); return; }

    // Set process bar and tsize label. 设置进度条和tsize
    emit sendProcessBarMaximum(operation == OP_WRITE_REQ ? (tSize <= 0 ? 0 : tSize) : 0);
    emit sendTSize(operation == OP_WRITE_REQ ? QString::number(tSize <= 0 ? 0 : tSize) : "Unknown");

    // Execute the task. 执行任务
    startTime = clock();
    if (operation == OP_READ_REQ) { recvFile(); }
    if (operation == OP_WRITE_REQ) { sendFile(); }
}

/**
 * RECEIVE TASK ARGUMENTS FROM MAIN PROCESS 从主进程获得任务参数
 */
void WorkThread::recvReadFileArgs(SOCKET sock, SOCKADDR_IN servAddr, const char* reFile, const char* loFile, int mode, EXTENDED ext, int oper) {
    // Get args.
    //this->socketFd = sock;
    this->serverAddr = servAddr;
    this->remoteFile = reFile;
    this->localFile = loFile;
    this->mode = mode;
    this->extended = ext;
    this->operation = oper;

    // Init args.
    if (fp) { fclose(fp); fp = NULL; }
    tSize = 0;
    blkSize = TFTP_DEFAULT_BLOCK_SIZE;
    timeOut = TFTP_DEFAULT_TIMEOUT_SEC;
    tvTimeOut = { timeOut, 0 };
    lastBytesRecv = lastBytesSend = bytesRecv = bytesSend = 0;
    totalRetransmitCount = fileSize = 0;
    percent = 0;

    this->socketFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

/**
 * CHECK ARGUMENTS OF CURRENT READ/WRITE REQUEST 检查当前读/写请求的参数
 * #return : stauts code 状态码
 */
int WorkThread::checkArgs() {
    // Check reqest size. 检查请求报文长度
    size_t reqMsgLenCheck = FULLSTRLEN(remoteFile) + FULLSTRLEN(mode == MODE_NETASCII ? "netascii" : "octet");
    if (extended.isExtended) {
        int tempTSize = operation == OP_WRITE_REQ ? getFileSize(localFile) : 0;
        reqMsgLenCheck += extended.tsizeExist ? FULLSTRLEN("tsize") + FULLNUMLEN(0) : FULLNUMLEN(tempTSize <= 0 ? 0 : tempTSize);
        reqMsgLenCheck += extended.blksizeExist ? FULLSTRLEN("blksize") + FULLNUMLEN(extended.blksize) : 0;
        reqMsgLenCheck += extended.timeoutExist ? FULLSTRLEN("timeout") + FULLNUMLEN(extended.timeout) : 0;
    }
    if (reqMsgLenCheck > REQ_SIZE) {
        emit sendMsg(ERROR_TOO_LONG_REQUEST, QString("Args Check : Too long request : <%1>.").arg(reqMsgLenCheck + OP_SIZE));
        return ERROR_TOO_LONG_REQUEST;
    }

    // Check mode. 检查模式
    if (mode != MODE_NETASCII && mode != MODE_OCTET) {
        emit sendMsg(ERROR_UNDEFINED_MODE, QString("Args Check : Undefined mode : <%1>.").arg(mode));
        return ERROR_UNDEFINED_MODE;
    }

    emit sendMsg(TFTP_NO_ERROR, "Args Check : Valid.");
    return TFTP_NO_ERROR;
}

/**
 * OPEN LOCAL FILE ACCORDING TO THE OPERATION 根据当前操作用不同模式打开文件
 * #return : stauts code 状态码
 */
int WorkThread::openLocalFile() {
    // on windows, if "w", there will be a '\r' before '\n' while output
    if (fopen_s(&fp, localFile, operation == OP_READ_REQ ? "wb" : "rb") == 0) {
        emit sendMsg(TFTP_NO_ERROR, QString("Open file : Success : <%1>.").arg(QString::fromLocal8Bit(localFile)));
        return TFTP_NO_ERROR;
    } else {
        emit sendMsg(ERROR_OPENFILE_FAIL, QString("Open file : Fail : <%1>.").arg(QString::fromLocal8Bit(localFile)));
        return ERROR_OPENFILE_FAIL;
    }
}

/**
 * MAKE AND SEND REQUEST PACKET 组装并发送请求报文
 * #return : stauts code 状态码
 */
int WorkThread::sendReqest() {
    // Make packet. 组装请求报文
    int reqMsgLen = 0;
    reqPkt.op = htons(operation);
    appendMsg(reqPkt.reqMsg, REQ_SIZE, &reqMsgLen, "%s", remoteFile);
    appendMsg(reqPkt.reqMsg, REQ_SIZE, &reqMsgLen, mode == MODE_NETASCII ? "netascii" : "octet");
    if (extended.isExtended) {
        if (extended.tsizeExist) { appendMsg(reqPkt.reqMsg, REQ_SIZE, &reqMsgLen, "%s%c%d", "tsize", 0, tSize <= 0 ? 0 : tSize); }
        if (extended.blksizeExist) { appendMsg(reqPkt.reqMsg, REQ_SIZE, &reqMsgLen, "%s%c%d", "blksize", 0, extended.blksize); }
        if (extended.timeoutExist) { appendMsg(reqPkt.reqMsg, REQ_SIZE, &reqMsgLen, "%s%c%d", "timeout", 0, extended.timeout); }
    }

    // Send packet. 发送请求报文
    int res = sendPkt((char*)&reqPkt, OP_SIZE + reqMsgLen);
    if (res < 0) {
        emit sendMsg(ERROR_SEND_REQ_FAIL, QString("REQUEST : Send request packet fail with code : <%1>.").arg((WSAGetLastError())));
        return ERROR_SEND_REQ_FAIL;
    } else if (res < OP_SIZE + reqMsgLen) {
        emit sendMsg(TFTP_NO_ERROR, QString("REQUEST : Send request packet partically with length : <%1>.").arg(res));
    } else {
        emit sendMsg(TFTP_NO_ERROR, QString("REQUEST : Send request packet success with length : <%1>.").arg(res));
        if (extended.isExtended) {
            if (extended.tsizeExist) { emit sendMsg(TFTP_NO_ERROR, QString("└ Option tsize : <%1>.").arg(tSize <= 0 ? 0 : tSize)); }
            if (extended.blksizeExist) { emit sendMsg(TFTP_NO_ERROR, QString("└ Option blksize : <%1>.").arg(extended.blksize)); }
            if (extended.timeoutExist) { emit sendMsg(TFTP_NO_ERROR, QString("└ Option timeout : <%1>.").arg(extended.timeout)); }
        }
    }
    return reqMsgLen;
}

/**
 * RECEIVE FILE FROM TFTP SERVER 从TFTP服务器接收文件
 * #return : stauts code 状态码
 */
int WorkThread::recvFile() {
    bool notSupportExtended = false;
    int rcvdSize, retransmitCount = 0, ret, waitForDataRet = 0;
    curBlock = extended.isExtended ? 0 : 1;

    // 1. If extended, get OACK packet and set args. 如果使用扩展，接收OACK报文并且从中获得可选设置
    if (extended.isExtended) {
        while (retransmitCount <= TFTP_MAX_RETRANSMIT) {
            int waitForOACKRet = waitForOACK(timeOut * 1000, rcvdSize);

            if (waitForOACKRet == TFTP_OACK_PKT) { // Get the expected OACK packet. 得到了预期的OACK报文
                emit sendMsg(TFTP_NO_ERROR, "OACK : Support extended options.");
                getOptionFromOAckPkt(rcvdSize);
                if (sendAck(curBlock++) < 0) {
                    tftpTerminate(false);
                    return ERROR_SEND_ACK_FAIL;
                }
                break;
            } else if (waitForOACKRet == TFTP_DATA_PKT) { // Get the expected DATA<1> packet. 服务器端不支持扩展选项，获得第一个DATA报文
                emit sendMsg(TFTP_NOT_SUPPORT_EXT, QString("DATA <1> : Not support extended options. Use default."));
                curBlock = 1;
                notSupportExtended = true;
                break;
            } else if (waitForOACKRet == TFTP_ERROR_PKT) { // Get the expected error packet. 接收到ERROR报文
                if (rcvdSize < OP_SIZE + ERRCODE_SIZE) {
                    emit sendMsg(ERROR_WRONG_PKT, "ERRO : Wrong error packet.");
                } else {
                    emit sendMsg(ntohs(rcvdPkt.errCode), QString(rcvdPkt.errMsg));
                }
                tftpTerminate(false);
                return ntohs(rcvdPkt.errCode);
            } else { // Get a unexpected packet. 接收到其他报文
                retransmitCount++;
                totalRetransmitCount++;
                emit sendMsg(ERROR_RETRANSMIT_EQU, "Retransmit request packet.");
                if (sendPkt((char*)&reqPkt, OP_SIZE + reqMsgLen) < 0) {
                    emit sendMsg(ERROR_SEND_REQ_FAIL, "REQUEST : Send request packet fail.");
                    tftpTerminate(false);
                    return ERROR_SEND_REQ_FAIL;
                }
            }
        }

        if (retransmitCount > TFTP_MAX_RETRANSMIT) {
            emit sendMsg(ERROR_RETRANSMIT_TOO_MUCH, "Retransmission times too much.");
            tftpTerminate(false);
            return ERROR_RETRANSMIT_TOO_MUCH;
        }
    }

    // 2. Get DATA packet and write to local file. 接收DATA报文并将数据写入本地文件
    retransmitCount = 0;
    while (retransmitCount <= TFTP_MAX_RETRANSMIT) {
        // try to get data <curBlock> in timeOut(ms)
        // if get the expected packet => send file and write file or error
        //                       else => retransmit last ack and try another time
        if (notSupportExtended && curBlock == 1) {
            waitForDataRet = TFTP_DATA_PKT;
        } else {
            waitForDataRet = waitForSpecificPkt(curBlock, timeOut * 1000, rcvdSize, OP_DATA);
        }

        if (waitForDataRet == TFTP_DATA_PKT) {  // get the expected data packet 得到DATA报文
            // Write file. 写入文件
            ret = (int)fwrite(rcvdPkt.data, 1, rcvdSize - OP_SIZE - BLOCK_SIZE, fp);
            if (ret < (size_t)rcvdSize - OP_SIZE - BLOCK_SIZE) {
                emit sendMsg(ERROR_FWRITE_FAIL, "FWRITE : Fail.");
                tftpTerminate(false);
                return ERROR_FWRITE_FAIL;
            }
            // Send ACK. 发送ACK报文
            if (sendAck(curBlock) < 0) {
                tftpTerminate(false);
                return ERROR_SEND_ACK_FAIL;
            }
            // Update data. 更新统计数据
            curBlock++;
            retransmitCount = 0;
            fileSize += rcvdSize - OP_SIZE - BLOCK_SIZE;
            // Check if end. 检测是否结束
            if (rcvdSize == blkSize + OP_SIZE + BLOCK_SIZE) {   // not end => update the process bar 没有结束 => 更新进度条
                if (extended.tsizeExist && (double)fileSize / tSize > percent) {
                    percent = (double)fileSize / tSize + 0.01;
                    emit sendProcessBarValue(fileSize);
                }
                continue;
            } else {  // end => show message and set the process bar 结束了 => 显示统计信息并更新进度条
                tftpTerminate(true);
                break;
            }
        } else if (waitForDataRet == TFTP_ERROR_PKT) {  // Get a expected error packet. 接收到ERROR报文
            if (rcvdSize < OP_SIZE + ERRCODE_SIZE) {
                emit sendMsg(ERROR_WRONG_PKT, "ERRO : Wrong error packet.");
            } else {
                emit sendMsg(ntohs(rcvdPkt.errCode), QString(rcvdPkt.errMsg));
            }
            tftpTerminate(false);
            return ERROR_WRONG_PKT;
        } else {  // get a unexpected packet 接收到其他报文
            retransmitCount++;
            totalRetransmitCount++;
            if (!extended.isExtended && curBlock == 1) {
                emit sendMsg(ERROR_RETRANSMIT_EQU, "Retransmit request packet.");
                if (sendPkt((char*)&reqPkt, OP_SIZE + reqMsgLen) < 0) {
                    emit sendMsg(ERROR_SEND_REQ_FAIL, "REQUEST : Send request packet fail.");
                    tftpTerminate(false);
                    return ERROR_SEND_REQ_FAIL;
                }
            } else {
                emit sendMsg(ERROR_RETRANSMIT_ACK, QString("Retransmit ACK packet <%1>.").arg(curBlock - 1));
                if (sendAck(curBlock - 1) < 0) {
                    tftpTerminate(false);
                    return ERROR_SEND_ACK_FAIL;
                }
            }
        }
    }

    if (retransmitCount > TFTP_MAX_RETRANSMIT) {
        emit sendMsg(ERROR_RETRANSMIT_TOO_MUCH, "Retransmission times too much.");
        tftpTerminate(false);
        return ERROR_RETRANSMIT_TOO_MUCH;
    }

    if (mode == MODE_NETASCII) { decodeNetascii(localFile, PLATFORM_WINDOWS); }

    return TFTP_NO_ERROR;
}

/**
 * SEND FILE TO TFTP SERVER 向TFTP服务器发送文件
 * #return : stauts code 状态码
 */
int WorkThread::sendFile() {
    bool haveGetFirstAck = false, isFinished = false;
    int rcvdSize, dataSize = 0, retransmitCount = 0, waitForDataRet = 0;
    curBlock = 0;

    // 1. If extended, get OACK packet and set args. 如果使用扩展，接收OACK报文并且从中获得可选设置
    if (extended.isExtended) {
        while (retransmitCount <= TFTP_MAX_RETRANSMIT) {
            int waitForOACKRet = waitForOACK(timeOut * 1000, rcvdSize);

            if (waitForOACKRet == TFTP_OACK_PKT) {  // Get the expected OACK packet. 接收到预期的OACK报文
                emit sendMsg(TFTP_NO_ERROR, "OACK : Support extended options.");
                getOptionFromOAckPkt(rcvdSize);
                haveGetFirstAck = true;
                break;
            } else if (waitForOACKRet == TFTP_ACK_PKT) {  // Get the expected ACK<0> packet. 服务器端不支持扩展选项，获得第一个ACK报文
                emit sendMsg(TFTP_NOT_SUPPORT_EXT, QString("ACK  <0> : Not support extended options. Use default."));
                haveGetFirstAck = true;
                break;
            } else if (waitForOACKRet == TFTP_ERROR_PKT) {  // Get the expected error packet. 接收到ERROR报文
                if (rcvdSize < OP_SIZE + ERRCODE_SIZE) {
                    emit sendMsg(ERROR_WRONG_PKT, "ERRO : Wrong error packet.");
                } else {
                    emit sendMsg(ntohs(rcvdPkt.errCode), QString(rcvdPkt.errMsg));
                }
                tftpTerminate(false);
                return ntohs(rcvdPkt.errCode);
            } else {  // Get a unexpected packet. 接收到其他报文
                retransmitCount++;
                totalRetransmitCount++;
                emit sendMsg(ERROR_RETRANSMIT_EQU, "Retransmit request packet.");
                if (sendPkt((char*)&reqPkt, OP_SIZE + reqMsgLen) < 0) {
                    emit sendMsg(ERROR_SEND_REQ_FAIL, "REQUEST : Send request packet fail.");
                    tftpTerminate(false);
                    return ERROR_SEND_REQ_FAIL;
                }
            }
        }

        if (retransmitCount > TFTP_MAX_RETRANSMIT) {
            emit sendMsg(ERROR_RETRANSMIT_TOO_MUCH, "Retransmission times too much.");
            tftpTerminate(false);
            return ERROR_RETRANSMIT_TOO_MUCH;
        }
    }

    // 2. Read loacl file and sent DATA packet. 读取本地文件并且发送DATA报文
    retransmitCount = 0;
    while (retransmitCount <= TFTP_MAX_RETRANSMIT) {
        // try to get ack <curBlock> in timeOut(ms)
        // if get the expected packet => read file and send data or error
        //                       else => retransmit last data and try another time
        if (curBlock == 0 && haveGetFirstAck) {
            waitForDataRet = TFTP_ACK_PKT;
        } else {
            waitForDataRet = waitForSpecificPkt(curBlock, timeOut * 1000, rcvdSize, OP_ACK);
        }

        if (waitForDataRet == TFTP_ACK_PKT) {  // Get the expected ack packet. 接收到ACK报文
            if (!isFinished) {
                // Read file. 读取本地文件
                dataSize = (int)fread(dataPkt.data, 1, blkSize, fp);
                if (dataSize < blkSize) { isFinished = true; }
                // Send data. 发送DATA报文
                dataPkt.block = htons(++curBlock);
                if (sendPkt((char*)&dataPkt, dataSize + OP_SIZE + BLOCK_SIZE) < 0) {
                    emit sendMsg(ERROR_SEND_DATA_FAIL, QString("DATA <%1> : Send DATA packet fail.").arg(curBlock - 1));
                    tftpTerminate(false);
                    return ERROR_SEND_DATA_FAIL;
                }
                // Update data. 更新统计数据和进度条
                retransmitCount = 0;
                fileSize += dataSize;
                if ((double)fileSize / tSize > percent) {
                    percent = (double)fileSize / tSize + 0.01;
                    emit sendProcessBarValue(fileSize);
                }
            } else {  // Show message and set the process bar. 显示统计信息并更新进度条
                tftpTerminate(true);
                break;
            }
        } else if (waitForDataRet == TFTP_ERROR_PKT) {    // Get a expected error packet. 接收到ERROR报文
            if (rcvdSize < OP_SIZE + ERRCODE_SIZE) {
                emit sendMsg(ERROR_WRONG_PKT, "ERRO : Wrong error packet.");
            } else {
                emit sendMsg(ntohs(rcvdPkt.errCode), QString(rcvdPkt.errMsg));
            }
            tftpTerminate(false);
            return ntohs(rcvdPkt.errCode);
        }
        else {  // Get a unexpected packet. 接收到其他报文
            retransmitCount++;
            totalRetransmitCount++;
            if (!extended.isExtended && curBlock == 0) {
                emit sendMsg(ERROR_RETRANSMIT_EQU, "Retransmit request packet.");
                if (sendPkt((char*)&reqPkt, OP_SIZE + reqMsgLen) < 0) {
                    emit sendMsg(ERROR_SEND_REQ_FAIL, "REQUEST : Send request packet fail.");
                    tftpTerminate(false);
                    return ERROR_SEND_REQ_FAIL;
                }
            } else {
                emit sendMsg(ERROR_RETRANSMIT_ACK, QString("Retransmit DATA packet <%1>.").arg(curBlock));
                if (sendPkt((char*)&dataPkt, dataSize + OP_SIZE + BLOCK_SIZE) < 0) {
                    emit sendMsg(ERROR_SEND_DATA_FAIL, QString("DATA <%1> : Send DATA packet fail.").arg(curBlock));
                    tftpTerminate(false);
                    return ERROR_SEND_DATA_FAIL;
                }
            }
        }
    }

    if (retransmitCount > TFTP_MAX_RETRANSMIT) {
        emit sendMsg(ERROR_RETRANSMIT_TOO_MUCH, "Retransmission times too much.");
        tftpTerminate(false);
        return TFTP_MAX_RETRANSMIT;
    }

    return TFTP_NO_ERROR;
}

/**
 * MAKE AND SEND ACK PACKET 组装并发送ACK报文
 * @block  : block num that ACK reply to 当前ACK对应的块号
 * #return : the byte length sent 发送数据的字节长度
 */
int WorkThread::sendAck(WORD block) {
    ackPkt.block = htons(block);
    int ret = sendPkt((char*)&ackPkt, sizeof(PKG_ACK));
    if (ret < 0) {
        emit sendMsg(ERROR_SEND_ACK_FAIL, QString("ACK  <%1> : Send ACK packet fail.").arg(block));
    } else {
        // emit sendMsg(TFTP_NO_ERROR, QString("ACK  <%1> : Send ACK packet success.").arg(block));
    }
    return ret;
}

/**
 * SEND PACKET 发送报文
 * @buf    : data to be sent 需要发送的数据
 * @len    : length of the data 需要发送数据的长度
 * #return : the byte length sent 发送数据的字节长度
 */
int WorkThread::sendPkt(const char* buf, int len) {
    int ret = sendto(socketFd, buf, len, 0, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
    if (ret > 0) { bytesSend += ret; }
    return ret;
}

/**
 * RECEIVE PACKET 接收报文
 * @buf    : buffer to get data 缓冲区
 * @len    : length of the buffer 缓冲区长度
 * #return : the byte length received 接收数据的字节长度
 */
int WorkThread::recvPkt(char* buf, int len) {
    int ret = recvfrom(socketFd, buf, len, 0, (SOCKADDR*)&serverAddr, &serverAddrLen);
    if (ret > 0) { bytesRecv += ret; }
    return ret;
}

/**
 * GET OPTION SETTINGS FROM OACK PACKET 从OACK报文中接收可选设置
 * @rcvdSize : the length of the OACK packet OACK报文的长度
 * #return   : the num of optional settings 可选数据的个数
 */
int WorkThread::getOptionFromOAckPkt(int rcvdSize) {
    int count = 0;
    char opt[ERROR_SIZE] = { 0 }, val[ERROR_SIZE] = { 0 };
    for (int offset = 0; offset < rcvdSize - 2;) {
        getMsg(opt, ERROR_SIZE, rcvdPkt.opMsg, &offset);
        getMsg(val, ERROR_SIZE, rcvdPkt.opMsg, &offset);
        if (extended.tsizeExist && strcmp(opt, "tsize") == 0) {
            emit sendMsg(TFTP_NO_ERROR, QString("└ Option tsize : <%1>.").arg(val));
            emit sendTSize(QString::number(tSize = atoi(val)));
            count++;
        }
        if (extended.blksizeExist && strcmp(opt, "blksize") == 0) {
            emit sendMsg(TFTP_NO_ERROR, QString("└ Option blksize : <%1>.").arg(val));
            blkSize = atoi(val);
            count++;
        }
        if (extended.timeoutExist && strcmp(opt, "timeout") == 0) {
            emit sendMsg(TFTP_NO_ERROR, QString("└ Option timeout : <%1>.").arg(val));
            tvTimeOut.tv_sec = timeOut = atoi(val) + 1;
            count++;
        }
    }
    if (extended.tsizeExist) { emit sendProcessBarMaximum(tSize); }
    return count;
}

/**
 * TRY TO GET A PACKET IN TIMEOUTMS 尝试在TIMEOUTMS时间内接收一个报文
 * @timeOutMs : time out interval (ms) 超时时间(毫秒)
 * @rcvdSize  : used to return the size of packet received 用来返回接收报文的大小
 * #return    : stauts code 状态码
 */
int WorkThread:: waitForPkt(int timeOutMs, int &rcvdSize) {
    FD_SET readFds;
    TIMEVAL tv = {timeOutMs / 1000, timeOutMs % 1000 * 1000};
    FD_ZERO(&readFds);
    FD_SET(socketFd, &readFds);
    int selectRet = select(socketFd + 1, &readFds, NULL, NULL, &tv);

    if (selectRet == SOCKET_ERROR) {
        return ERROR_SOCKET_ERROR;
    } else if (selectRet == 0) {
        return TFTP_ERROR_TIMEOUT;
    } else {
        rcvdSize = recvPkt((char*)&rcvdPkt, sizeof(PKG_DATA_ERROR_OACK));
        if (rcvdSize == SOCKET_ERROR) {
            emit sendMsg(ERROR_SOCKET_ERROR, "RECV : SOCKET_ERROR.");
            return ERROR_SOCKET_ERROR;
        } else if (rcvdSize == 0) {
            emit sendMsg(ERROR_CONNECT_CLOSE, "RECV : The connection has been closed.");
            return ERROR_CONNECT_CLOSE;
        }
    }
    if (ntohs(rcvdPkt.op) == OP_DATA || ntohs(rcvdPkt.op) == OP_ACK || ntohs(rcvdPkt.op) == OP_ERROR) {
        if (rcvdSize < OP_SIZE + MODE_SIZE) { return TFTP_ERROR_WRONG_PKT; }
    } else if (ntohs(rcvdPkt.op) == OP_OACK) {
        if (rcvdSize < OP_SIZE) { return TFTP_ERROR_WRONG_PKT; }
    }
    return TFTP_NO_ERROR;
}

/**
 * TRY TO GET A OACK PACKET IN TIMEOUTMS 尝试在TIMEOUTMS时间内接收一个OACK报文
 * if operation == OP_READ_REQ and the server don't support extended options, there will be data<1>
 * if operation == OP_WRITE_REQ and the server don't support extended options, there will be ack<0>
 * @timeOutMs : time out interval (ms) 超时时间(毫秒)
 * @rcvdSize  : used to return the size of packet received 用来返回接收报文的大小
 * #return    : type of the packet received 接收到报文的类型
 */
int WorkThread::waitForOACK(int timeOutMs, int &rcvdSize) {
    int restTime = timeOutMs, waitForPktRet, startTime;

    do {
        startTime = clock();
        waitForPktRet = waitForPkt(restTime, rcvdSize);
        restTime -= (clock() - startTime);
    } while (waitForPktRet == TFTP_NO_ERROR && (operation == OP_READ_REQ ? (ntohs(rcvdPkt.op) == OP_DATA && ntohs(rcvdPkt.block) != 1) :
                                                (ntohs(rcvdPkt.op) == OP_ACK && ntohs(rcvdPkt.block) != 0)));

    if (restTime <= 0) { emit sendMsg(TFTP_ERROR_TIMEOUT, QString("OACK : Timeout.")); }

    return waitForPktRet != TFTP_NO_ERROR ? waitForPktRet : ntohs(rcvdPkt.op);
}

/**
 * TRY TO GET A SPECIFIC PACKET(IDENTIFIED BY PKTTYPE) IN TIMEOUTMS 尝试在TIMEOUTMS时间内接收一个由PKTTYPE指定类型的报文
 * @block     : block num of the packet 期待的报文的块号
 * @timeOutMs : time out interval (ms) 超时时间(毫秒)
 * @rcvdSize  : used to return the size of packet received 用来返回接收报文的大小
 * @pktType   : packet type want to get 期望的报文类型
 * #return    : type of the packet received 接收到报文的类型
 */
int WorkThread::waitForSpecificPkt(WORD block, int timeOutMs, int &rcvdSize, int pktType) {
    int restTime = timeOutMs, waitForPktRet, startTime;

    do {
        startTime = clock();
        waitForPktRet = waitForPkt(restTime, rcvdSize);
        restTime -= (clock() - startTime);
    } while (waitForPktRet == TFTP_NO_ERROR && (ntohs(rcvdPkt.op) != pktType || ntohs(rcvdPkt.block) != block));

    if (restTime <= 0) {
        emit sendMsg(TFTP_ERROR_TIMEOUT, (pktType == TFTP_ACK_PKT ? "ACK " : "DATA") + QString(" <%1> : Timeout.").arg(block));
    }

    return waitForPktRet != TFTP_NO_ERROR ? waitForPktRet : ntohs(rcvdPkt.op);
}

/**
 * TERMINATE CURRENT TASK AND OUTPUT THE STATISTICAL INFORMATION 结束当前任务并输出统计信息
 * @isSuccess : if the task is done successfully 当前任务是否成功结束
 */
void WorkThread::tftpTerminate(bool isSuccess) {
    emit sendProcessBarMaximum(1);
    emit sendProcessBarValue(isSuccess ? 1 : 0);
    if (isSuccess) {
        endTime = clock();
        double timeSec = (double)(endTime - startTime) / CLOCKS_PER_SEC;
        if (operation == OP_READ_REQ) {
            emit sendMsg(TFTP_NO_ERROR, QString("Read Success  : <%1> ==> <%2>").arg(QString::fromLocal8Bit(remoteFile), QString::fromLocal8Bit(localFile)));
        } else {
            emit sendMsg(TFTP_NO_ERROR, QString("WRITE Success : <%1> ==> <%2>").arg(QString::fromLocal8Bit(localFile), QString::fromLocal8Bit(remoteFile)));
        }
        emit sendMsg(TFTP_NO_ERROR, QString("├ Block      : <%1 blks | %2 B/blk> ").arg(curBlock - (operation == OP_READ_REQ ? 1 : 0)).arg(blkSize));
        emit sendMsg(TFTP_NO_ERROR, QString("├ Time       : <%1 s> ").arg(timeSec));
        emit sendMsg(TFTP_NO_ERROR, QString("├ File Size  : <%1 kB> ").arg((double)fileSize / 1024));
        emit sendMsg(TFTP_NO_ERROR, QString("├ Down       : <%1 kB> ").arg((double)bytesRecv / 1024));
        emit sendMsg(TFTP_NO_ERROR, QString("├ Down Speed : <%1 kB/s> ").arg(((double)bytesRecv / 1024 / timeSec)));
        emit sendMsg(TFTP_NO_ERROR, QString("├ Up         : <%1 kB> ").arg((double)bytesSend / 1024));
        emit sendMsg(TFTP_NO_ERROR, QString("├ Up Speed   : <%1 kB/s> ").arg(((double)bytesSend / 1024 / timeSec)));
        emit sendMsg(TFTP_NO_ERROR, QString("└ Retransmit : <%1 times> ").arg(totalRetransmitCount));
        emit sendMsg(TFTP_NO_ERROR, QString("---------------------- END SUCC ----------------------"));
    } else {
        emit sendMsg(TFTP_NO_ERROR, QString("---------------------- END FAIL ----------------------"));
    }
    if (fp) { fclose(fp); }
}

/**
 * UPDATE THE REAL-TIME SPEED INFORMATION 更新实时吞吐量
 */
void WorkThread::recvTimeOutSignal() {
    emit sendUploadSpeed(QString::number(((double)bytesSend - lastBytesSend) / TFTP_REFRESH_INTERVAL * 1000 / 1024));
    emit sendDownloadSpeed(QString::number(((double)bytesRecv - lastBytesRecv) / TFTP_REFRESH_INTERVAL * 1000 / 1024));
    lastBytesRecv = bytesRecv;
    lastBytesSend = bytesSend;
}

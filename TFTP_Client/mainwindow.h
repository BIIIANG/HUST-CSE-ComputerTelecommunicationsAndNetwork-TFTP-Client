#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDateTime>
#include <QFileDialog>
#include <QMessageBox>
#include <QHostAddress>
#include <QProcessEnvironment>

#include "def.h"
#include "util.h"
#include "workthread.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void sendReadFileArgs(SOCKET sock, SOCKADDR_IN servAddr, const char* reFile, const char* loFile, int mode, EXTENDED ext, int oper);

public slots:

private:
    Ui::MainWindow *ui;

    EXTENDED extended = { 0 };

    unsigned short blkSize = TFTP_DEFAULT_BLOCK_SIZE;

    int serverPort, mode = MODE_OCTET;
    SOCKET socketFd = INVALID_SOCKET;
    SOCKADDR_IN serverAddr = { 0 };
    int serverAddrLen = sizeof(serverAddr);

    const char *remoteFile, *localFile, *serverIP;
    std::string remoteFileStd, localFileStd, serverIPStd, remoteFileGB, localFileGB;
    QByteArray remoteFileByteArray, localFileByteArray;

    WorkThread *workThread = new WorkThread();

    int checkArgs(int op);

    void setModuleEnabled(bool isEnabled);

    QTimer *timer = new QTimer();

};
#endif // MAINWINDOW_H

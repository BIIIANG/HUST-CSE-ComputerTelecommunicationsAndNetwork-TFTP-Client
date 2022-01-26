#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // init modules 初始化UI组件
    ui->ProgressBar->setRange(0,1);
    ui->ProgressBar->setValue(0);
    ui->ModeComboBox->addItem("OCTET");
    ui->ModeComboBox->addItem("NETASCII");
    ui->BreakBtn->setEnabled(false);        // waiting to be done 待完成

    // for debug 测试用
//    ui->LocalFileLineEdit->setText("H:\\测试用例1.jpg");
//    ui->RemoteFileLineEdit->setText("测试用例1.jpg");

#ifdef WIN32
    // init WinSock 初始化WinSock
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) { QMessageBox::warning(this, "Init WinSock Fail.", "Init socket fail."); exit(-1); }
    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) { WSACleanup(); exit(-1); }
#endif

    // select local file 选择本地文件
    connect(ui->dirBtn,&QPushButton::clicked, this, [=]() {
        QString desktopPath = QProcessEnvironment::systemEnvironment().value("USERPROFILE") + "\\desktop";
        QString localFilePath = QFileDialog::getSaveFileName(this, "请选择本地文件", desktopPath);
        if(!localFilePath.isEmpty()) { ui->LocalFileLineEdit->setText(localFilePath); }
    });
    // receive file 接收文件
    connect(ui->GetBtn, &QPushButton::clicked, this, [=]() {
        if (checkArgs(OP_READ_REQ) == TFTP_NO_ERROR) {
            timer->start(TFTP_REFRESH_INTERVAL);
            setModuleEnabled(false);
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(SERV_PORT);
            InetPtonA(AF_INET, serverIP, (void*)&serverAddr.sin_addr);
            socketFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (socketFd == INVALID_SOCKET) { QMessageBox::warning(this, "Init Socket Fail.", "Init socket fail."); exit(-1); }
            emit sendReadFileArgs(socketFd, serverAddr, remoteFile, localFile, mode, extended, OP_READ_REQ);
            workThread->start();
        }
    });
    // send file 发送文件
    connect(ui->PutBtn, &QPushButton::clicked, this, [=]() {
        if (checkArgs(OP_WRITE_REQ) == TFTP_NO_ERROR) {
            timer->start(TFTP_REFRESH_INTERVAL);
            setModuleEnabled(false);
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(SERV_PORT);
            InetPtonA(AF_INET, serverIP, (void*)&serverAddr.sin_addr);
            if (mode == MODE_NETASCII) {    // encode to netascii
                encodeNetascii(localFile);
                localFileStd += ".netascii";
                localFile = localFileStd.c_str();
                QTextCodec *code = QTextCodec::codecForName("GB2312");
                localFileByteArray = code->fromUnicode(localFile);
                localFileGB = localFileByteArray.data();
                localFile = localFileGB.c_str();
            }
            socketFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (socketFd == INVALID_SOCKET) { QMessageBox::warning(this, "Init Socket Fail.", "Init socket fail."); exit(-1); }
            emit sendReadFileArgs(socketFd, serverAddr, remoteFile, localFile, mode, extended, OP_WRITE_REQ);
            workThread->start();
        }
    });
    connect(this, &MainWindow::sendReadFileArgs, workThread, &WorkThread::recvReadFileArgs);
    connect(workThread, &WorkThread::finished, this, [=]() {
        setModuleEnabled(true);
        timer->stop();
        if(socketFd != INVALID_SOCKET) { closesocket(socketFd); }
    });

    // set UI modules action 设置UI组件动作
    connect(workThread, &WorkThread::sendProcessBarMaximum, this, [=](int max) { ui->ProgressBar->setMaximum(max); });
    connect(workThread, &WorkThread::sendProcessBarValue, this, [=](int val) { ui->ProgressBar->setValue(val); });
    connect(workThread, &WorkThread::sendTSize, this, [=](QString tsize) { ui->tsizeLabel->setText(tsize); });
    connect(ui->tsizeCheckBox, &QCheckBox::stateChanged, this, [=](int state) { extended.tsizeExist = state == Qt::Checked ? true : false; });
    connect(ui->blksizeCheckBox, &QCheckBox::stateChanged, this, [=](int state) { extended.blksizeExist = state == Qt::Checked ? true : false; });
    connect(ui->timeoutCheckBox, &QCheckBox::stateChanged, this, [=](int state) { extended.timeoutExist = state == Qt::Checked ? true : false; });
    connect(ui->blksizeSpinBox, &QSpinBox::valueChanged, this, [=](int value) { extended.blksize = value; });
    connect(ui->timeoutSpinBox, &QSpinBox::valueChanged, this, [=](int value) { extended.timeout = value; });
    connect(ui->ModeComboBox, &QComboBox::currentIndexChanged, this, [=](int index) { mode = index == 0 ? MODE_OCTET : MODE_NETASCII; });

    ui->tsizeCheckBox->setCheckState(Qt::Checked);
    ui->blksizeCheckBox->setCheckState(Qt::Checked);
    extended.blksize = TFTP_DEFAULT_BLOCK_SIZE;
    extended.timeout = TFTP_DEFAULT_TIMEOUT_SEC;

    // real-time speed 实时吞吐量
    connect(timer, &QTimer::timeout, workThread, &WorkThread::recvTimeOutSignal);
    connect(workThread, &WorkThread::sendUploadSpeed, this, [=](QString speed) { ui->UploadLabel->setText(speed); });
    connect(workThread, &WorkThread::sendDownloadSpeed, this, [=](QString speed) { ui->DownloadLabel->setText(speed); });

    // log 记录log和保存log
    connect(workThread, &WorkThread::sendMsg, this, [=](int code, QString msg) {
        QString fullMsg = QDateTime::currentDateTime().toString("[MM/dd hh:mm:ss.zzz]");
        fullMsg += (code == TFTP_NO_ERROR ? " OK " : " ER ") + msg;
        ui->LogTextBrowser->append(fullMsg);
    });
    connect(ui->ClearBtn, &QPushButton::clicked, this, [=]() { ui->LogTextBrowser->clear(); });
    connect(ui->SaveBtn, &QPushButton::clicked, this, [=]() {
        if(!ui->LogTextBrowser->toPlainText().isEmpty()) {
            QString logFileName = "TFTP_log_" + QDateTime::currentDateTime().toString("yyyy_MM_dd_hh_mm_ss") + ".txt";
            QFile file("./" + logFileName);
            file.open(QIODevice::ReadWrite | QIODevice::Append);
            QTextStream out(&file);
            out << ui->LogTextBrowser->toPlainText();
            file.close();
            QMessageBox::information(this, "Save Log Success.", "Save log sucess as: \"" + logFileName + "\" in process directory.");
        }
        else {
            QMessageBox::information(this, "Save Log Fail.", "Log is empty.");
        }
    });

}

MainWindow::~MainWindow() {
    if(socketFd != INVALID_SOCKET) { closesocket(socketFd); }
#ifdef WIN32
    WSACleanup();
#endif
    delete ui;
}

/**
 * CHECK ARGUMENTS IN INPUT LABELS AND SHOW THE NOTICE MESSAGE 检查输入框内的参数并输出提示信息
 * @op     : operation 操作
 * #return : status code 状态码
 */
int MainWindow::checkArgs(int op) {
    if (ui->ServerIPLineEdit->text().isEmpty()) {
        QMessageBox::critical(this, "Server IP Error.", "Server IP missing. Please input the Server IP.");
        return ERROR_INVALID_ARG;
    }

    if (ui->ServerPortLineEdit->text().isEmpty()) {
        QMessageBox::critical(this, "Server Port Error.", "Server port missing. Please input the Server port.");
        return ERROR_INVALID_ARG;
    }

    if (ui->LocalFileLineEdit->text().isEmpty()) {
        QMessageBox::critical(this, "Local File Error.", "Local file missing. Please input the local file.");
        return ERROR_INVALID_ARG;
    }

    if (ui->RemoteFileLineEdit->text().isEmpty()) {
        QMessageBox::critical(this, "Remote File Error.", "Remote file missing. Please input the Remote file.");
        return ERROR_INVALID_ARG;
    }

    QHostAddress *ip = new QHostAddress(ui->ServerIPLineEdit->text());
    if (ip->isNull()) {
        QMessageBox::critical(this, "Server IP Error.", "Wrong Server IP. Please input the Server IP again.");
        return ERROR_INVALID_ARG;
    }
    serverIPStd = ui->ServerIPLineEdit->text().toStdString();
    serverIP = serverIPStd.c_str();

    QTextCodec *code = QTextCodec::codecForName("GB2312");

    localFileStd = ui->LocalFileLineEdit->text().toStdString();
    localFile = localFileStd.c_str();
    localFileByteArray = code->fromUnicode(localFile);
    localFileGB = localFileByteArray.data();
    localFile = localFileGB.c_str();
    if (op == OP_READ_REQ && _access(localFile, 0) != -1) {
        QMessageBox::StandardButton res = QMessageBox::question(this, "Local File Warning.", "File exist. Do you want to overlay it?");
        if (res == QMessageBox::No) { return ERROR_INVALID_ARG; }
    }

    remoteFileStd = ui->RemoteFileLineEdit->text().toStdString();
    remoteFile = remoteFileStd.c_str();
    remoteFileByteArray = code->fromUnicode(remoteFile);
    remoteFileGB = remoteFileByteArray.data();
    remoteFile = remoteFileGB.c_str();

    extended.isExtended = extended.tsizeExist || extended.blksizeExist || extended.timeoutExist;
    return TFTP_NO_ERROR;
}

/**
 * SET UI MODULE STATUS 设置UI组件状态
 * @isEnabled : enabled or disabled 启用或禁用
 */
void MainWindow::setModuleEnabled(bool isEnabled) {
    ui->GetBtn->setEnabled(isEnabled);
    ui->PutBtn->setEnabled(isEnabled);
    //ui->BreakBtn->setEnabled(!isEnabled);
    ui->tsizeCheckBox->setEnabled(isEnabled);
    ui->blksizeCheckBox->setEnabled(isEnabled);
    ui->timeoutCheckBox->setEnabled(isEnabled);
    ui->blksizeSpinBox->setEnabled(isEnabled);
    ui->timeoutSpinBox->setEnabled(isEnabled);
    if (isEnabled) { ui->UploadLabel->setText("0.00"); ui->DownloadLabel->setText("0.00"); }
    return;
}


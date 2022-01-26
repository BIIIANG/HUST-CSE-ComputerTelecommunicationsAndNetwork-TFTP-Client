#include "util.h"

/**
 * RECEIVE ARGUMENTS LIKE sprintf TO APPEND MESSAGE TO THE BUF AND ALTER THE OFFSET
 * 接收可变参数，将字符串附加在BUF的指定位置并维护当前写到位置的偏移OFFSET
 * @buf         : pointer to string buffer 字符串缓冲区首指针
 * @bufferCount : max length of the buf 字符串缓冲区最大长度
 * @offset      : current position of the string 当前位置
 * @format,...  : format like sprintf 类似sprintf函数的可变格式参数
 * #return      : length of the message 信息长度
 */
int appendMsg(char* buf, rsize_t bufferCount, int* offset, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int ret = vsprintf_s(buf + *offset, bufferCount - *offset, format, args);
    va_end(args);
    *offset += ret + 1;
    return ret;
}

/**
 * READ MESSAGE FORM SOURCE DATA TO BUF AND ALTER THE OFFSET TO GET THE NEXT MESSAGE
 * 从SOURCE数据中读取一条信息到BUF中并维护下一条信息的开始偏移OFFSET
 * @buf         : pointer to string buffer 字符串缓冲区首指针
 * @bufferCount : max length of the buf 字符串缓冲区最大长度
 * @source      : source data 源数据
 * @offset      : position of the current message 当前信息的位置
 * #return      : length of the message 信息长度
 */
int getMsg(char* buf, rsize_t bufferCount, char const* source, int* offset)
{
    int ret = sprintf_s(buf, bufferCount, "%s", source + *offset);
    *offset += ret + 1;
    return ret;
}

/**
 * GET THE BYTE SIZE OF A FILE 获得一个文件的字节长度
 * @fileName : name of the file 文件名
 * #return   : the byte size of the file 该文件的字节长度
 */
int getFileSize(const char* fileName)
{
    struct stat statBuf;
    if (stat(fileName, &statBuf) != 0) { return -1; }
    return statBuf.st_size;
}

/**
 * GET THE LENGTH OF A DECIMAL NUMBER 获得一个十进制数的长度
 * @num    : the decimal number 十进制数
 * #return : the length 长度
 */
int numlen(int num)
{
    if (num == 0) { return 1; }
    int count = 0;
    for (; num != 0; num /= 10) { count++; }
    return count;
}

/**
 * ENCODE FILE TO NETASCII 将本地编码的文件转换为NETASCII编码文件
 * LF(0x0A) => CRLF(0x0D,0x0A)
 * CR(0x0D) => CRNUL(0x0D,0x00)
 * EXAMPLE.FILE => EXAMPLE.FILE.netascii
 * @fileName : name of the file to be encoded 需要进行编码的文件名
 * #return   : status code (-1 for fail, 0 for succeed) 状态码
 */
int encodeNetascii(const char *fileName) {
    FILE *fp = NULL, *tfp = NULL;
    char tempFileName[1024];
    strcpy_s(tempFileName, 1023, fileName);
    strcat_s(tempFileName, 1023, ".netascii");
    fopen_s(&fp, fileName, "rb");
    fopen_s(&tfp, tempFileName, "wb");
    if (fp == NULL || tfp == NULL) {
        if (fp) { fclose(fp); }
        if (fp) { fclose(tfp); }
        return -1;
    }

    int ch = fgetc(fp), lch = NUL;
    while (true) {
        if (ch == LF && lch != CR) { fputc(CR, tfp); }  // LF => CRLF
        else if (lch == CR && ch != LF && ch != NUL) { fputc(NUL, tfp); }   // CR => CRNUL
        else if (ch == EOF) { break; }
        fputc(ch, tfp);
        lch = ch;
        ch = fgetc(fp);
    }

    fclose(fp);
    fclose(tfp);
    return 0;
}

/**
 * DECODE NETASCII TO LOCAL FORMAT 将NETASCII编码的文件转换为本地编码
 * ON WINDOWS : CRLF => CRLF  CRNUL => CR
 * ON LINUX   : CRLF => LF  CRNUL => CR
 * EXAMPLE.FILE => EXAMPLE.FILE.local
 * @fileName : name of the file to be decoded 需要进行解码的文件名
 * #return   : status code (-1 for fail, 0 for succeed) 状态码
 */
int decodeNetascii(const char *fileName, int platform) {
    FILE *fp = NULL, *tfp = NULL;
    char tempFileName[1024];
    strcpy_s(tempFileName, 1023, fileName);
    strcat_s(tempFileName, 1023, ".local");
    fopen_s(&fp, fileName, "rb");
    fopen_s(&tfp, tempFileName, "wb");
    if (fp == NULL || tfp == NULL) {
        if (fp) { fclose(fp); }
        if (fp) { fclose(tfp); }
        return -1;
    }

    int ch = 0, nch = 0;
    while (true) {
        ch = fgetc(fp);
        if (ch == CR) {
            nch = fgetc(fp);
            if ( platform != PLATFORM_WINDOWS && nch == LF) { ch = LF; }
            else if (nch == NUL) { ch = CR; }
            else { fseek(fp, -1L, SEEK_CUR); }
        } else if (ch == EOF) { break; }
        fputc(ch, tfp);
    }

    fclose(fp);
    fclose(tfp);
    return 0;
}

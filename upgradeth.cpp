#include "upgradeth.h"
#include <windows.h>
#include <QThread>
#include <QDebug>
#include <QFileInfo>
#include <QFile>

#include <istream>
#include <iostream>
#include "ControlCAN.h"
#include "crc16.h"

/* 获取所有选中的节点，区分全选和部分选择，
 * 所有选中节点都切换到Bootloader
 * 再次发送check命令 确保所有需要升级的节点都进入Bootloader
 * 擦除erase命令
 * 发送writeinfo命令 取得选择的文件*/

upgradeth::upgradeth(QObject *parent) : QObject(parent)
{

}

void upgradeth::setparam(int devicetype, int deviceindex, int canindex, bool in_allchoose, QStringList in_idlist, QStringList in_typelist, QString in_filename, int crclen)
{
    m_devicetype = devicetype;
    m_deviceindex = deviceindex;
    m_canindex = canindex;
    allchoose = in_allchoose;
    idlist = in_idlist;
    typelist = in_typelist;
    filename = in_filename;
    count = idlist.count();
    icrclen = crclen;
}

int upgradeth::erase_func(QString nodestr)
{
    int ret = 0, len;
    unsigned int erase_instruct = 0x0F00F000;
    unsigned int erase_failtime = 0;
    QFileInfo newfileinfo(filename);
    int filesize_i = newfileinfo.size();
    qDebug()<<"filesize:"<<filesize_i;
    QString filesize_str = QString().sprintf("%08X", filesize_i);

    VCI_CAN_OBJ recvframeinfo[50];
    VCI_CAN_OBJ sendframeinfo;
    sendframeinfo.SendType = 0;
    sendframeinfo.RemoteFlag = 0;
    sendframeinfo.ExternFlag = 1;

    memset(sendframeinfo.Data, 0, sizeof(sendframeinfo.Data));
    sendframeinfo.DataLen = 4;
    sendframeinfo.Data[0] = filesize_str.mid(0, 2).toUInt(nullptr, 16);
    sendframeinfo.Data[1] = filesize_str.mid(2, 2).toUInt(nullptr, 16);
    sendframeinfo.Data[2] = filesize_str.mid(4, 2).toUInt(nullptr, 16);
    sendframeinfo.Data[3] = filesize_str.mid(6, 2).toUInt(nullptr, 16);

    if (!nodestr.compare("*")) {
        /* 擦除所有节点数据 */
        sendframeinfo.ID = erase_instruct;
        ret = VCI_Transmit(m_devicetype, m_deviceindex, m_canindex, &sendframeinfo, 1);
        if (!ret) {
            qDebug()<<"擦除所有节点失败";
            emit back_upgradeinfo("*", "Erase", "Faild");
            return -1;
        }
        emit back_upgradeinfo("*", "Erase", "Success");
        return 0;
    }
    else {
        /*擦除选中节点*/
        sendframeinfo.ID = erase_instruct|(nodestr.toInt()<<16);
        while (1) {
            ret = VCI_Transmit(m_devicetype, m_deviceindex, m_canindex, &sendframeinfo, 1);
            if (!ret) {
                qDebug()<<"更新全部节点到BootLoader失败"<<nodestr;
            }
            while (1) {
                len = VCI_Receive(m_devicetype, m_deviceindex, m_canindex, recvframeinfo, 50, 200);
                if (len > 0) {
                    break;
                }
                else {
                    Sleep(1000);
                }
            }
            if ((recvframeinfo[0].ID&0x000000ff) == 0x08) {
                qDebug()<<"擦除"<<nodestr<<"success";
                emit back_upgradeinfo(nodestr, "Erase", "Success");
                break;
            }
            else if ((recvframeinfo[0].ID&0x000000ff) == 0x09) {
                if (recvframeinfo->DataLen == 4) {
                    erase_failtime = recvframeinfo[0].Data[3];
                    emit back_upgradeinfo(nodestr, "Erase", QString("Faild %d").arg(erase_failtime));
                    if (erase_failtime>= 100) {
                        qDebug()<<"失败次数超过规定值"<<erase_failtime;
                        return -2;
                    }
                }
            }
        }
    }
    return 0;
}

int upgradeth::writeinfo_func(QString nodestr, unsigned int deviation, unsigned int step_len, int crclen)
{
    QString step_str = QString().sprintf("%08x", step_len+crclen);
    QString sentdata_str = QString().sprintf("%08x", deviation);
    unsigned int writeinfo_instruct = 0x0F00F001;
    int ret , len,writeinfo_failtime;
    VCI_CAN_OBJ sendframeinfo;
    VCI_CAN_OBJ recvframeinfo[50];
    sendframeinfo.SendType = 0;
    sendframeinfo.RemoteFlag = 0;
    sendframeinfo.ExternFlag = 1;
    sendframeinfo.DataLen = 8;
    sendframeinfo.Data[0] = sentdata_str.mid(0, 2).toUInt(nullptr, 16);
    sendframeinfo.Data[1] = sentdata_str.mid(2, 2).toUInt(nullptr, 16);
    sendframeinfo.Data[2] = sentdata_str.mid(4, 2).toUInt(nullptr, 16);
    sendframeinfo.Data[3] = sentdata_str.mid(6, 2).toUInt(nullptr, 16);
    sendframeinfo.Data[4] = step_str.mid(0, 2).toUInt(nullptr, 16);
    sendframeinfo.Data[5] = step_str.mid(2, 2).toUInt(nullptr, 16);
    sendframeinfo.Data[6] = step_str.mid(4, 2).toUInt(nullptr, 16);
    sendframeinfo.Data[7] = step_str.mid(6, 2).toUInt(nullptr, 16);

    if (!nodestr.compare("*")) {
        //全选
        sendframeinfo.ID = writeinfo_instruct;
        ret = VCI_Transmit(m_devicetype, m_deviceindex, m_canindex, &sendframeinfo, 1);
        if (!ret) {
            qDebug()<<"writeinfo所有节点失败";
            emit back_upgradeinfo("*", "Writeinfo", "Faild Transmit");
            return -1;
        }
        emit back_upgradeinfo("*", "Writeinfo", QString("%1 %2").arg(deviation).arg(step_len));
    }
    else {
        sendframeinfo.ID = writeinfo_instruct|(nodestr.toUInt(nullptr, 16)<<16);
        while (1) {
            ret = VCI_Transmit(m_devicetype, m_deviceindex, m_canindex, &sendframeinfo, 1);
            if (!ret) {
                qDebug()<<"writeinfo单节点失败"<<nodestr;
                emit back_upgradeinfo(nodestr, "Writeinfo", "Faild Transmit");
            }
            while (1) {
                len = VCI_Receive(m_devicetype, m_deviceindex, m_canindex, recvframeinfo, 50, 200);
                if (len > 0) {
                    break;
                }
                else {
                    Sleep(1000);
                }
            }
            if ((recvframeinfo[0].ID&0x000000ff) == 0x08) {
                qDebug()<<"writeinfo"<<nodestr<<"success";
                emit back_upgradeinfo(nodestr, "Writeinfo", "Success");
                return 0;
            }
            else if ((recvframeinfo[0].ID&0x000000ff) == 0x09) {
                qDebug()<<"writeinfo"<<nodestr<<"faild";
                if (recvframeinfo->DataLen == 4) {
                    printf("writeinfo %02x %02x %02x %02x \n", recvframeinfo[0].Data[0], recvframeinfo[0].Data[1], recvframeinfo[0].Data[2], recvframeinfo[0].Data[3]);
                    writeinfo_failtime = recvframeinfo[0].Data[3];
                    emit back_upgradeinfo(nodestr, "Writeinfo", QString("Faild %1").arg(writeinfo_failtime));
                    if (writeinfo_failtime>= 100) {
                        qDebug()<<"失败次数超过规定值"<<writeinfo_failtime;
                        return writeinfo_failtime;
                    }
                }
                QThread::msleep(1);
            }
        }
    }


    return 0;
}

int upgradeth::write_func(QString nodestr, unsigned char *writedata, unsigned int datalen, int crclen)
{
    unsigned int writedata_instruct = 0x0F00F002;
    VCI_CAN_OBJ recvframeinfo[50];
    VCI_CAN_OBJ sendframeinfo;

    int i, j, len = 0, ret;
    sendframeinfo.SendType = 0;
    sendframeinfo.RemoteFlag = 0;
    sendframeinfo.ExternFlag = 1;
    if (!nodestr.compare("*")) {
        sendframeinfo.ID = writedata_instruct;
    }
    else {
        sendframeinfo.ID = writedata_instruct | (nodestr.toUInt(nullptr, 16)<<16);
    }

    unsigned char uchCRCHi=0xFF;    /* 高CRC字节初始化 */
    unsigned char uchCRCLo=0xFF;    /* 低CRC字节初始化 */
    unsigned int uIndex;            /* CRC循环中的索引 */

    int remainder = datalen%8;
    int cycle = datalen/8;

    for (i=0; i<cycle; i++) {
        sendframeinfo.DataLen = 8;
        memcpy(sendframeinfo.Data, writedata+(i*8), 8);
        for (j=0; j<8; j++) {
            uIndex  =uchCRCLo^*(writedata+(i*8)+j);
            uchCRCLo=uchCRCHi^auchCRCHi[uIndex];
            uchCRCHi=auchCRCLo[uIndex];
        }
        ret = VCI_Transmit(m_devicetype, m_deviceindex, m_canindex, &sendframeinfo, 1);
        qDebug()<<__LINE__<<ret;
        QThread::msleep(20);
    }

    for (j=0; j<remainder; j++) {
        uIndex = uchCRCLo^*(writedata+(cycle*8)+j);
        uchCRCLo = uchCRCHi^auchCRCHi[uIndex];
        uchCRCHi = auchCRCLo[uIndex];
    }

    /*为了兼容有无crc16逻辑，如果没有crc16 剩余多次字节发送多少字节
     * 如果有crc16，剩余0字节，组一帧发送crc16；在剩余6字节判断下，小于等于6 可加crc16的2个字节一帧发送，大于6 只能剩余7字节，需要发送两帧。一帧7字节数据和1字节的crc16 一帧1字节crc16
     * 如果没有crc16，剩余6字节判断没有实际意义。
     */
    if (remainder == 0) {
        if (crclen > 0) {
            sendframeinfo.DataLen = crclen;
            sendframeinfo.Data[0] = uchCRCHi;
            sendframeinfo.Data[1] = uchCRCLo;
            ret = VCI_Transmit(m_devicetype, m_deviceindex, m_canindex, &sendframeinfo, 1);
            qDebug()<<__LINE__<<ret;
            QThread::msleep(10);
        }
    }
    else if ( remainder > 0 && remainder <= 6) {
        sendframeinfo.DataLen = remainder+crclen;
        memcpy(sendframeinfo.Data, writedata+(cycle*8), remainder);
        if (crclen > 0) {
            (sendframeinfo.Data+remainder)[0] = uchCRCHi;
            (sendframeinfo.Data+remainder)[1] = uchCRCLo;
        }
        ret = VCI_Transmit(m_devicetype, m_deviceindex, m_canindex, &sendframeinfo, 1);
        qDebug()<<__LINE__<<ret;
        QThread::msleep(10);
    }
    else {
        if (crclen > 0) {
            sendframeinfo.DataLen = remainder+1;
            memcpy(sendframeinfo.Data, writedata+(cycle*8), remainder);
            (sendframeinfo.Data+remainder)[0] = uchCRCHi;
            ret = VCI_Transmit(m_devicetype, m_deviceindex, m_canindex, &sendframeinfo, 1);
            qDebug()<<__LINE__<<ret;
            QThread::msleep(10);

            sendframeinfo.DataLen = 1;
            sendframeinfo.Data[0] = uchCRCLo;
            ret = VCI_Transmit(m_devicetype, m_deviceindex, m_canindex, &sendframeinfo, 1);
            qDebug()<<__LINE__<<ret;
            QThread::msleep(10);
        }
        else {
            sendframeinfo.DataLen = remainder;
            memcpy(sendframeinfo.Data, writedata+(cycle*8), remainder);
            ret = VCI_Transmit(m_devicetype, m_deviceindex, m_canindex, &sendframeinfo, 1);
            qDebug()<<__LINE__<<ret;
            QThread::msleep(10);
        }
    }
    if (nodestr.compare("*")) {
        while (1) {
            len = VCI_Receive(m_devicetype, m_deviceindex, m_canindex, recvframeinfo, 50, 200);
            if (len > 0) {
                break;
            }
            else {
                Sleep(1000);
            }
        }

        if ((recvframeinfo[0].ID&0x000000ff) == 0x08) {
            qDebug()<<"write"<<nodestr<<"success";
            return 1;
        }
        else if ((recvframeinfo[0].ID&0x000000ff) == 0x09) {
            qDebug()<<"write"<<nodestr<<"faild";
            qDebug()<<"失败次数"<<int(recvframeinfo[0].Data[3]);
            return -1;

        }
    }
    return 0;
}

int upgradeth::exec_newapp(QString nodestr)
{
    unsigned int execnew_instruct = 0x0F00F005;
    VCI_CAN_OBJ recvframeinfo[50];
    VCI_CAN_OBJ sendframeinfo;
    int ret = 0, len;

    sendframeinfo.SendType = 0;
    sendframeinfo.RemoteFlag = 0;
    sendframeinfo.ExternFlag = 1;
    if (!nodestr.compare("*")) {
        sendframeinfo.ID = execnew_instruct;
    }
    else {
        sendframeinfo.ID = execnew_instruct | (nodestr.toUInt(nullptr, 16)<<16);
    }

    sendframeinfo.DataLen = 4;
    sendframeinfo.Data[0] = 0xaa;
    sendframeinfo.Data[1] = 0xaa;
    sendframeinfo.Data[2] = 0xaa;
    sendframeinfo.Data[3] = 0xaa;
    sendframeinfo.Data[4] = 0xaa;

    ret = VCI_Transmit(m_devicetype, m_deviceindex, m_canindex, &sendframeinfo, 1);
    if (!ret) {
        qDebug()<<"execnew所有节点失败";
        return -1;
    }
    if (nodestr.compare("*")) {
        while (1) {
            len = VCI_Receive(m_devicetype, m_deviceindex, m_canindex, recvframeinfo, 50, 200);
            if (len > 0) {
                break;
            }
            else {
                Sleep(1000);
            }
        }
        unsigned long long data = 0;
        data = (unsigned long long)(recvframeinfo[0].Data[0])<<56|(unsigned long long)(recvframeinfo[0].Data[1])<<48|\
               (unsigned long long)(recvframeinfo[0].Data[2])<<40|(unsigned long long)(recvframeinfo[0].Data[3])<<32|\
               (unsigned long long)(recvframeinfo[0].Data[4])<<24|(unsigned long long)(recvframeinfo[0].Data[5])<<16|\
               (unsigned long long)(recvframeinfo[0].Data[6])<<8|(unsigned long long)(recvframeinfo[0].Data[7]);
        QString bootnode = QString().sprintf("%02x", (recvframeinfo[0].ID&0x00ff0000)>>16);
        qDebug()<<bootnode<<int(recvframeinfo[0].ID&0x000000ff);
    }


    return 0;
}

void upgradeth::upgraderun()
{
    qDebug()<<allchoose<<idlist<<typelist<<filename<<icrclen;
    emit back_upgradeinfo("", "start", "");
    int count = idlist.count();
    int ret = 0, i, len;
    QString nodestr;
    VCI_CAN_OBJ recvframeinfo[50];
    unsigned int executeBoot = 0x0F00F005; /*切换BootLoader模式*/

    VCI_CAN_OBJ sendframeinfo;
    sendframeinfo.SendType = 0;
    sendframeinfo.RemoteFlag = 0;
    sendframeinfo.ExternFlag = 1;
    sendframeinfo.DataLen = 4;
    sendframeinfo.Data[0] = 0x55;
    sendframeinfo.Data[1] = 0x55;
    sendframeinfo.Data[2] = 0x55;
    sendframeinfo.Data[3] = 0x55;
    /* 更新所有节点到BootLoader模式 */
    if (allchoose) {
        /* 全选更新不单一发送 */
        sendframeinfo.ID = executeBoot;
        ret = VCI_Transmit(m_devicetype, m_deviceindex, m_canindex, &sendframeinfo, 1);
        if (!ret) {
            qDebug()<<"更新全部节点到BootLoader失败";
            emit back_upgradeinfo("*", "Excute Boot", "Faild");
            emit back_upgradeinfo("", "stop", "");
            return ;
        }
    }
    else
    {
        /*对不是app模式的更新节点*/
        for (i=0 ;i<count; i++) {
            if (!typelist.at(i).compare("App")) {
                nodestr = idlist.at(i);
                sendframeinfo.ID = executeBoot|(nodestr.toInt()<<16);
                qDebug()<<nodestr<<int(sendframeinfo.ID);
                ret = VCI_Transmit(m_devicetype, m_deviceindex, m_canindex, &sendframeinfo, 1);
                if (!ret) {
                    qDebug()<<"更新全部节点到BootLoader失败"<<nodestr;
                    emit back_upgradeinfo(nodestr, "Excute Boot", "Faild");
                    continue;
                }
                while (1) {
                    len = VCI_Receive(m_devicetype, m_deviceindex, m_canindex, recvframeinfo, 50, 200);
                    if (len > 0) {
                        break;
                    }
                    else {
                        Sleep(1000);
                    }
                }
                if ((recvframeinfo[0].ID&0x000000ff) == 0x08) {
                    qDebug()<<nodestr<<"success";
                    emit back_upgradeinfo(nodestr, "Excute Boot", "success");
                }
            }
        }
    }
    QThread::sleep(1);

    //校验节点状态，是否是都是BootLoader模式
    sendframeinfo.ID = 0x0700F003;
    sendframeinfo.DataLen = 0;
    ret = VCI_Transmit(m_devicetype, m_deviceindex, m_canindex, &sendframeinfo, 1);
    if (!ret) {
        qDebug()<<"校验所有节点类型出错";
    }
    memset(recvframeinfo, 0, sizeof(VCI_CAN_OBJ)*50);
    int check_count = count;
    int change_fail = 0;
    len = 0;
    while (1) {
        len = VCI_Receive(m_devicetype, m_deviceindex, m_canindex, recvframeinfo, 50, 200);
        check_count = check_count-len;
        qDebug()<<check_count;
        for (i=0; i<len; i++) {
            unsigned long long data;
            data = (unsigned long long)(recvframeinfo[i].Data[0])<<56|(unsigned long long)(recvframeinfo[i].Data[1])<<48|\
                   (unsigned long long)(recvframeinfo[i].Data[2])<<40|(unsigned long long)(recvframeinfo[i].Data[3])<<32|\
                   (unsigned long long)(recvframeinfo[i].Data[4])<<24|(unsigned long long)(recvframeinfo[i].Data[5])<<16|\
                   (unsigned long long)(recvframeinfo[i].Data[6])<<8|(unsigned long long)(recvframeinfo[i].Data[7]);

            QString bootnode = QString().sprintf("%02x", (recvframeinfo[i].ID&0x00ff0000)>>16);
            qDebug()<<bootnode<<(((data&0x00000000ffffffff) ==0x55555555)?"Bootloader":"App");
            if ((idlist.contains(bootnode)) && ((data&0x00000000ffffffff) != 0x55555555)) {
                change_fail = 1;
                emit back_upgradeinfo(bootnode, "Change Boot", "Faild");
            }
        }
        if (check_count <= 0) {
            break;
        }
    }

    if (change_fail == 1) {
        qDebug()<<"有节点转变失败";
        emit back_upgradeinfo("", "stop", "有节点转变失败");
        return ;
    }

    //擦除Flash

    QFileInfo newfileinfo(filename);
    int filesize_i = newfileinfo.size();
    qDebug()<<"filesize:"<<filesize_i;

    QFile filefd(filename);
    if (!filefd.open(QIODevice::ReadOnly)) {
        qDebug()<<filefd.errorString();
        emit back_upgradeinfo("", "stop", filefd.errorString());
        return ;
    }
    QByteArray filecont = filefd.readAll();
    filefd.close();
    char *data = filecont.data();

    unsigned int remindlen = filesize_i;
    unsigned int step_size = 0;
    unsigned int sent_datasize = 0;

    int successflag = 0;

    unsigned char writedata[1024] = {0};

    if (allchoose) {
        qDebug()<<"全部更新";
        /* erase_func all
         * writeinfo all
         * write data */
        while (1) {
            erase_func("*");

            do {
                if (remindlen > 1024) {
                    step_size = 1024;
                }
                else {
                    step_size = remindlen;
                }
                QThread::sleep(1);
                ret = writeinfo_func("*", sent_datasize, step_size, icrclen);
                if (ret < 0) {
                    break;
                }

                memcpy(writedata, data+sent_datasize, step_size);
                qDebug()<<"write data";
                QThread::sleep(1);
                write_func("*", writedata, step_size, icrclen);

                sent_datasize += step_size;
                emit back_upgradeinfo("*", "Write", QString("data %1").arg(sent_datasize));
                remindlen -= step_size;
            }while (remindlen);
            break;
        }
        exec_newapp("*");
    }
    else {
        qDebug()<<"部分更新";
        for (i=0; i<count; i++) {
            nodestr = idlist.at(i);
            /* erase_func nodestr
             * writeinfo nodestr
             * write data nodestr*/
            while (1) {
                ret = erase_func(nodestr);
                if (ret == -2) {
                    exec_newapp(nodestr);
                    break;
                }
                do {
                    if (remindlen > 1024) {
                        step_size = 1024;
                    }
                    else {
                        step_size = remindlen;
                    }
                    writeinfo_func(nodestr, sent_datasize, step_size, icrclen);
                    qDebug()<<"write data";
                    memcpy(writedata, data+sent_datasize, step_size);
                    ret = write_func(nodestr, writedata, step_size, icrclen);
                    if (ret<0) {
                        successflag = 0;
                        break;
                    }
                    else {
                        successflag = 1;
                    }

                    sent_datasize += step_size;
                    emit back_upgradeinfo(nodestr, "Write", QString("data %1").arg(sent_datasize));
                    remindlen -= step_size;

                }while (remindlen);
                if (successflag)
                    break;
            }
            exec_newapp(nodestr);
        }
    }
    emit back_upgradeinfo("", "stop", "");
}

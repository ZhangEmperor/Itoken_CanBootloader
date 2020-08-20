#include "refreshth.h"
#include <windows.h>
#include <QThread>
#include <QDebug>
#include "ControlCAN.h"

refreshth::refreshth(QObject *parent) : QObject(parent)
{

}

void refreshth::setparam(int devtype, int devindex, int canindex)
{
    m_devtype = devtype;
    m_devindex = devindex;
    m_canindex = canindex;
    runflag = true;
}

void refreshth::setrunflag(bool flag)
{
    runflag = flag;
}

void refreshth::refreshrun()
{
    int len;
    VCI_CAN_OBJ frameinfo[50];
    while (1) {
        if (!runflag)
            break;
        len = VCI_Receive(m_devtype, m_devindex, m_canindex, frameinfo, 50, 200);
        if (len <= 0) {
            QThread::sleep(1);
            continue;
        }
        int i;
        for (i = 0; i < len; i++) {

            unsigned long long data;
            data = (unsigned long long)(frameinfo[i].Data[0])<<56|(unsigned long long)(frameinfo[i].Data[1])<<48|\
                   (unsigned long long)(frameinfo[i].Data[2])<<40|(unsigned long long)(frameinfo[i].Data[3])<<32|\
                   (unsigned long long)(frameinfo[i].Data[4])<<24|(unsigned long long)(frameinfo[i].Data[5])<<16|\
                   (unsigned long long)(frameinfo[i].Data[6])<<8|(unsigned long long)(frameinfo[i].Data[7]);
            back_refreshinfo(frameinfo[i].ID, data);
        }
    }
}

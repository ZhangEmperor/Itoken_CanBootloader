#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QMessageBox>
#include <QFileDialog>
#include <QTableWidgetItem>
#include <QThread>

#include "ControlCAN.h"
#include "crc16.h"


/*执行步骤
    更新check所有节点信息
    excute 切换BootLoader模式
    check 确认所有信息都为BootLoader模式
    erase 擦除模式
    writeinfo 写数据
    write*/



MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow), connect_flag(false)
{
    ui->setupUi(this);
    this->ui->comboBox_bolter->setCurrentIndex(11);

    workth = new QThread;
    refreshhandle = new refreshth;
    connect(this,&MainWindow::refreshsig,refreshhandle,&refreshth::refreshrun);
    connect(refreshhandle, &refreshth::back_refreshinfo, this, &MainWindow::handle_refreshinfo);

    upgradehandle = new upgradeth;
    connect(this,&MainWindow::upgradesig,upgradehandle,&upgradeth::upgraderun);
    connect(upgradehandle, &upgradeth::back_upgradeinfo, this, &MainWindow::handle_updateinfo);

    refreshhandle->moveToThread(workth);
    upgradehandle->moveToThread(workth);
    workth->start();

    upgradebox = new QMessageBox(this);
    upgradebox->setStandardButtons(0);
    upgradebox->setText("升级中请勿关闭");
}

MainWindow::~MainWindow()
{
    delete ui;
}

int MainWindow::strtodata(unsigned char *str, unsigned char *data, int len, int flag)
{
    unsigned char cTmp = 0;
    int i = 0;
    for (int j = 0; j < len; j++)
    {
        if (chartoint(str[i++], &cTmp))
            return 1;
        data[j] = cTmp;
        if (chartoint(str[i++], &cTmp))
            return 1;
        data[j] = (data[j] << 4) + cTmp;
        if (flag == 1)
            i++;
    }
    return 0;
}

int MainWindow::chartoint(unsigned char chr, unsigned char *cint)
{
    unsigned char cTmp;
    cTmp = chr - 48;
    if (cTmp >= 0 && cTmp <= 9)
    {
        *cint = cTmp;
        return 0;
    }
    cTmp = chr - 65;
    if (cTmp >= 0 && cTmp <= 5)
    {
        *cint = (cTmp + 10);
        return 0;
    }
    cTmp = chr - 97;
    if (cTmp >= 0 && cTmp <= 5)
    {
        *cint = (cTmp + 10);
        return 0;
    }
    return 1;
}

void MainWindow::on_pushButton_connect_clicked()
{
    if (!connect_flag) {
        deviceindex = ui->comboBox_devicenum->currentIndex();
        canindex = ui->comboBox_canindex->currentIndex();
        QString acccodestr = ui->lineEdit_acccode->text();
        QString accmaskstr = ui->lineEdit_accmask->text();
        QString time0str = ui->lineEdit_timer0->text();
        QString time1str = ui->lineEdit_timer1->text();
        int filterindex = ui->comboBox_fiter->currentIndex();
        int modelindex = ui->comboBox_mode->currentIndex();
        int devicechoose = ui->comboBox_devicetype->currentIndex();
        on_comboBox_devicetype_currentIndexChanged(devicechoose);

        DWORD Acc, Mask, Timing0, Timing1;
        unsigned char sztmp[4];
        char szAcc[20], szMask[20], szBtr0[20], szBtr1[20];


        //init config
        VCI_INIT_CONFIG init_config;

        sprintf(szAcc, "%s", acccodestr.toLocal8Bit().constData());
        sprintf(szMask, "%s", accmaskstr.toLocal8Bit().constData());
        sprintf(szBtr0, "%s", time0str.toLocal8Bit().constData());
        sprintf(szBtr1, "%s", time1str.toLocal8Bit().constData());

        if (strtodata((unsigned char*)szAcc, sztmp, 4, 0) != 0)
        {
            printf("szAcc is error\n");
        }
        Acc = acccodestr.toUInt(nullptr, 16);
        if (strtodata((unsigned char*)szMask, sztmp, 4, 0) != 0)
        {
            // MessageBox("屏蔽码数据格式不对!","警告",MB_OK|MB_ICONQUESTION);
            // return;
            printf("szMask is error\n");
        }
        Mask = accmaskstr.toUInt(nullptr, 16);

        if (strtodata((unsigned char*)szBtr0, sztmp, 1, 0) != 0)
        {
            // MessageBox("定时器0数据格式不对!","警告",MB_OK|MB_ICONQUESTION);
            // return;
            printf("szstrBtr0 is error\n");
        }
        Timing0 = ((DWORD)sztmp[0]);
        if (strtodata((unsigned char*)szBtr1, sztmp, 1, 0) != 0)
        {
            // MessageBox("定时器1数据格式不对!","警告",MB_OK|MB_ICONQUESTION);
            // return;
            printf("szstrBtr1 is error\n");
        }
        Timing1 = ((DWORD)sztmp[0]);


        init_config.AccCode = Acc;
        init_config.AccMask = Mask;
        init_config.Timing0 = Timing0;
        init_config.Timing1 = Timing1;
        init_config.Filter = filterindex;
        init_config.Mode = modelindex;

        qDebug()<<devicetype<<deviceindex<<Timing0<<Timing1;
        DWORD ret = VCI_OpenDevice(devicetype, deviceindex, 0);
        if (ret != STATUS_OK)
        {
            QMessageBox::information(this, "提示", "打开设备失败", QMessageBox::Ok);
            return;
        }
        ret = VCI_InitCAN(devicetype, deviceindex, canindex, &init_config);
        if (ret != STATUS_OK)
        {
            QMessageBox::information(this, "提示", "初始化设备失败", QMessageBox::Ok);
            return;
        }
        ret = VCI_StartCAN(devicetype, deviceindex, canindex);
        if (ret != STATUS_OK)
        {
            QMessageBox::information(this, "提示", "开始设备失败", QMessageBox::Ok);
            return;
        }

        connect_flag = true;
        refreshhandle->setparam(devicetype, deviceindex, canindex);

        this->ui->pushButton_connect->setText(tr("断开连接"));
    }
    else {
        connect_flag = false;
        VCI_ResetCAN(devicetype, deviceindex, canindex);
        qDebug()<<VCI_CloseDevice(devicetype, deviceindex);
        this->ui->pushButton_connect->setText(tr("连接"));

    }
}

void MainWindow::on_pushButton_choosefile_clicked()
{
    QFileDialog filebrowse;
    filebrowse.setOption(QFileDialog::ReadOnly);
    filebrowse.setDirectory(QDir::homePath());
//    filebrowse.setDirectory("F:\\QT-pro\\CAN-bootloader\\doc\\CANBootloader协议说明");
    if (!filebrowse.exec()) {
        return ;
    }
    QStringList filelist = filebrowse.selectedFiles();
    ui->lineEdit_filename->setText(filelist.at(0));

}

void MainWindow::on_pushButton_upgrade_clicked()
{
    QList<QTableWidgetItem*> items = ui->tableWidget->selectedItems();
    int count = items.count();
    bool allchoose = false;
    QStringList idlist;
    QStringList typelist;
    QString filename = ui->lineEdit_filename->text();
    int i;
    int crcflag = 0;
    if (count <= 0) {
        return ;
    }
    if (ui->tableWidget->rowCount() == 1 && count == 1) {
        allchoose = false;
    }
    else {
        if (count < ui->tableWidget->rowCount()) {
            qDebug()<<"多选";
            allchoose = false;
        }
        else {
            qDebug()<<"全选";
            allchoose = true;
        }
    }
    for (i=0; i<count; i++) {
        idlist<<items.at(i)->text();
        typelist<<ui->tableWidget->item(items.at(i)->row(), 1)->text();
    }
    if (this->ui->checkBox_crc16->isChecked()) {
        crcflag = 2;
    }
    else {
        crcflag = 0;
    }
    upgradehandle->setparam(devicetype, deviceindex, canindex, allchoose, idlist, typelist, filename, crcflag);
    emit upgradesig();

}

void MainWindow::on_comboBox_devicetype_currentIndexChanged(int index)
{
    switch (index) {
    case 0:
        devicetype = VCI_USBCAN1;        break;
    case 1:
        devicetype = VCI_USBCAN2;        break;
    default:
        break;
    }
}

void MainWindow::on_comboBox_bolter_currentIndexChanged(int index)
{
    int time0_value = 0;
    int time1_value = 0;
    switch (index) {
    case 0://5K
        time0_value = 0xBF;
        time1_value = 0xFF;
        break;
    case 1://10K
        time0_value = 0x31;
        time1_value = 0x1C;
        break;
    case 2://20K;
        time0_value = 0x18;
        time1_value = 0x1C;
        break;
    case 3://40K;
        time0_value = 0x87;
        time1_value = 0xFF;
        break;
    case 4://50K;
        time0_value = 0x09;
        time1_value = 0x1C;
        break;
    case 5://80K;
        time0_value = 0x83;
        time1_value = 0xFF;
        break;
    case 6://100K;
        time0_value = 0x04;
        time1_value = 0x1C;
        break;
    case 7://125K;
        time0_value = 0x03;
        time1_value = 0x1C;
        break;
    case 8://200K;
        time0_value = 0x81;
        time1_value = 0xFA;
        break;
    case 9://250K;
        time0_value = 0x01;
        time1_value = 0x1C;
        break;
    case 10://400K
        time0_value = 0x80;
        time1_value = 0xFA;
        break;
    case 11://500K
        time0_value = 0x00;
        time1_value = 0x1C;
        break;
    case 12://666K
        time0_value = 0x80;
        time1_value = 0xB6;
        break;
    case 13://800K
        time0_value = 0x00;
        time1_value = 0x16;
        break;
    case 14://1000K;
        time0_value = 0x00;
        time1_value = 0x14;
        break;
    }
    this->ui->lineEdit_timer0->setText(QString::number(time0_value).sprintf("%02X",time0_value));
    this->ui->lineEdit_timer1->setText(QString::number(time1_value).sprintf("%02X",time1_value));
}

void MainWindow::on_pushButton_clicked()
{

}

void MainWindow::on_pushButton_fresh_clicked()
{
    if (!connect_flag) {
        QMessageBox::warning(this, "提示", "请先连接设备", QMessageBox::Ok);
        return ;
    }
    VCI_CAN_OBJ sendframeinfo;
    clean_tablewidget();
    sendframeinfo.ID = 0x0700F003;  /* 获取所有节点 */
    sendframeinfo.SendType = 0;
    sendframeinfo.RemoteFlag = 0;
    sendframeinfo.ExternFlag = 1;
    sendframeinfo.DataLen = 0;
    int ret = VCI_Transmit(devicetype, deviceindex, canindex, &sendframeinfo, 1);
    if (ret <0) {
        QMessageBox::warning(this, "提示", "请先连接设备", QMessageBox::Ok);
        return ;
    }

    refreshhandle->setrunflag(true);
    emit refreshsig();
    QMessageBox::warning(this, "提示", "停止", QMessageBox::Ok);
    refreshhandle->setrunflag(false);
}

void MainWindow::clean_tablewidget(void)
{
    for(int i = ui->tableWidget->rowCount();i > 0;i--)
    {
        ui->tableWidget->removeRow(0);
    }
}

void MainWindow::handle_refreshinfo(unsigned int id, unsigned long long data)
{
    QString firmware_type;
    QString firmware_version;
    QString software_version;
    QString resultstr;
    int result = id&0x000000ff;
    int addr = (id&0x00ff0000)>>16;

    if (result == 0x08) {
        resultstr = "success";
    }
    else if (result == 0x09) {
        resultstr = "fail";
    }

    int orirow = this->ui->tableWidget->rowCount();
    this->ui->tableWidget->insertRow(orirow);
    this->ui->tableWidget->setItem(orirow, 0, new QTableWidgetItem(QString().sprintf("%02X", addr)));
    this->ui->tableWidget->setItem(orirow, 4, new QTableWidgetItem("check"));
    if (result == 0x09) {
        this->ui->tableWidget->setItem(orirow, 5, new QTableWidgetItem(resultstr));
        return;
    }
    if ((data&0x00000000ffffffff) == 0x55555555) {
        firmware_type = "Bootloader";
    }
    else if ((data&0x00000000ffffffff) == 0xAAAAAAAA) {
        firmware_type = "App";
    }
    verinfo info;
    info.data = (data&0xffffffff00000000)>>32;
    firmware_version = QString("V%1.%2").arg(info.ver.hardmajor).arg(info.ver.hardminor);
    software_version = QString("V%1.%2.%3").arg(info.ver.softmajor).arg(info.ver.softminor).arg(info.ver.softbuild);


    QTableWidgetItem *ftypeitem, *fversionitem, *sversionitem;
    ftypeitem = new QTableWidgetItem(firmware_type);
    ftypeitem->setFlags(ftypeitem->flags() &=(~Qt::ItemIsSelectable));

    fversionitem = new QTableWidgetItem(firmware_version);
    fversionitem->setFlags(fversionitem->flags() &=(~Qt::ItemIsSelectable));

    sversionitem =  new QTableWidgetItem(software_version);
    sversionitem->setFlags(sversionitem->flags() &=(~Qt::ItemIsSelectable));

    this->ui->tableWidget->setItem(orirow, 1, ftypeitem);
    this->ui->tableWidget->setItem(orirow, 2, fversionitem);
    this->ui->tableWidget->setItem(orirow, 3, sversionitem);
    this->ui->tableWidget->setItem(orirow, 5, new QTableWidgetItem(resultstr));

}

void MainWindow::handle_updateinfo(QString nodestr, QString commond, QString result)
{
    if (!commond.compare("start")) {
        upgradebox->show();
        return ;
    }
    if (!commond.compare("stop")) {
        upgradebox->hide();
        return ;
    }
    if (!nodestr.compare("*")) {
        int rows = ui->tableWidget->rowCount();
        for (int i=0; i<rows; i++) {
            ui->tableWidget->setItem(i, 4, new QTableWidgetItem(commond));
            ui->tableWidget->setItem(i, 5, new QTableWidgetItem(result));
        }
    }
    else {
        QList<QTableWidgetItem*> itemlist = ui->tableWidget->findItems(nodestr, Qt::MatchContains);
        ui->tableWidget->setItem(itemlist.at(0)->row(), 4, new QTableWidgetItem(commond));
        ui->tableWidget->setItem(itemlist.at(0)->row(), 5, new QTableWidgetItem(result));
    }
}

void MainWindow::on_pushButton_reset_clicked()
{
    VCI_CAN_OBJ sendframeinfo;
    sendframeinfo.ID = 0x0F00F005;  /* 获取所有节点 */
    sendframeinfo.SendType = 0;
    sendframeinfo.RemoteFlag = 0;
    sendframeinfo.ExternFlag = 1;
    sendframeinfo.DataLen = 4;
    sendframeinfo.Data[0] = 0xAA;
    sendframeinfo.Data[1] = 0xAA;
    sendframeinfo.Data[2] = 0xAA;
    sendframeinfo.Data[3] = 0xAA;
    int ret = VCI_Transmit(devicetype, deviceindex, canindex, &sendframeinfo, 1);
    if (ret <0) {
        QMessageBox::warning(this, "提示", "请先连接设备", QMessageBox::Ok);
        return ;
    }
}

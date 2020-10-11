#include "mainwindow.h"

#include <QApplication>
#include <QTranslator> //新增

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QTranslator translator; //新建翻译类

    translator.load(":/i18n/CAN-bootloader_zh_CN.qm"); //导入生成的文件

    a.installTranslator(&translator); //装入
    MainWindow w;
    w.show();
    return a.exec();
}

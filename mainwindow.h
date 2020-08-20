#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTableWidget>
#include <QString>

#include <windows.h>
#include "refreshth.h"
#include "upgradeth.h"
#include <QMessageBox>

struct version {
    unsigned int softbuild:5;
    unsigned int softminor:5;
    unsigned int softmajor:6;
    unsigned int hardminor:8;
    unsigned int hardmajor:8;
};

typedef union {
    struct version ver;
    unsigned int data;
}verinfo;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    int strtodata(unsigned char *str, unsigned char *data, int len, int flag);
    int chartoint(unsigned char chr, unsigned char *cint);
    DWORD devicetype;
    DWORD deviceindex;
    int canindex;
    int boltvalue;

    refreshth *refreshhandle;
    upgradeth *upgradehandle;
    QThread *workth;
signals:
    void refreshsig(); // 信号
    void upgradesig();
public slots:
    void handle_refreshinfo(unsigned int  id, unsigned long long data);
    void handle_updateinfo(QString nodestr, QString commond, QString result);


private slots:
    void on_pushButton_connect_clicked();

    void on_pushButton_choosefile_clicked();

    void on_pushButton_upgrade_clicked();

    void on_comboBox_devicetype_currentIndexChanged(int index);

    void on_comboBox_bolter_currentIndexChanged(int index);

    void on_pushButton_clicked();

    void on_pushButton_fresh_clicked();

    void on_pushButton_reset_clicked();

private:

    void clean_tablewidget(void);
    Ui::MainWindow *ui;
    QMessageBox *upgradebox;

    bool connect_flag;
};
#endif // MAINWINDOW_H

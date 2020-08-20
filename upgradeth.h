#ifndef UPGRADETH_H
#define UPGRADETH_H

#include <QObject>

class upgradeth : public QObject
{
    Q_OBJECT
public:
    explicit upgradeth(QObject *parent = nullptr);

    void setparam(int devicetype, int deviceindex, int canindex, bool in_allchoose, QStringList in_idlist, QStringList in_typelist, QString in_filename);

    int erase_func(QString nodestr);
    int writeinfo_func(QString nodestr, unsigned int deviation, unsigned int step_len, unsigned int crclen);
    int write_func(QString nodestr, unsigned char *writedata, unsigned int datalen, unsigned int crclen);
    int exec_newapp(QString nodestr);

signals:
    void back_upgradeinfo(QString nodestr, QString commond, QString result);
public slots:
    void upgraderun();
private:
    int m_devicetype;
    int m_deviceindex;
    int m_canindex;
    bool allchoose;
    QStringList idlist;
    QStringList typelist;
    QString filename;
    int count;


};

#endif // UPGRADETH_H

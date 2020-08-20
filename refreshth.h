#ifndef REFRESHTH_H
#define REFRESHTH_H

#include <QObject>

class refreshth : public QObject
{
    Q_OBJECT
public:
    explicit refreshth(QObject *parent = nullptr);
    void setparam(int devtype, int devindex, int canindex);
    void setrunflag(bool flag);

signals:
    void back_refreshinfo(unsigned int id, unsigned long long data);

public slots:
    void refreshrun(void);
private:
    int m_devtype;
    int m_devindex;
    int m_canindex;
    bool runflag;

};

#endif // REFRESHTH_H

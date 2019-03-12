#ifndef TUNNELEVENT_H
#define TUNNELEVENT_H

#include <QEvent>
#include <QString>

class UEvent : public QEvent
{
public: 
    int     eventCode;
    void *  param;
    int     lens;
    QString message;

    UEvent(QEvent::Type type = QEvent::User, int eventCode = 0, QString msg = "" , void * data = nullptr, int lens = 0);
};

#endif // TUNNELEVENT_H

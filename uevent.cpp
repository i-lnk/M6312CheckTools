#include "uevent.h"

UEvent::UEvent(QEvent::Type type, int eventCode, QString msg, void * data, int lens) : QEvent(type)
{
    this->eventCode = eventCode;
    this->message = msg;
    this->param = data;
    this->lens = lens;
}

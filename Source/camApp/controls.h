#ifndef CONTROLS_H
#define CONTROLS_H

#include <QObject>
#include <QDebug>

class Buttons: public QObject
{
    Q_OBJECT

public slots:
     void ret_button1();
     void ret_button2();
     void ret_button();
};

#endif // CONTROLS_H

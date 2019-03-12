#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QFuture>
#include <QMainWindow>
#include <QSerialPort>
#include <QSettings>
#include <QSqlQuery>

namespace Ui {
class MainWindow;
}

#define EVENT_CHECK_DONE 1001
#define EVENT_CHECK_FAIL 1002
#define EVENT_CHECK_MSG  1003

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onButtonStartClicked();
    void onButtonCleanClicked();
    void onButtonDBSetClicked();
    void onComboBoxIndexChanged(const QString & str);
    void parseString();

private:
    Ui::MainWindow *ui;  
    QFuture<void> checkProcHandle;  // 线程句柄
    QSettings * settings;

    static int checkByDatabase(QObject *, QSqlQuery *, QString, QString);  // 通过数据库校验
    static int checkByFunction(QObject *, QSerialPort *);  // 通过功能校验
    static int checkSN(QObject *, QSerialPort *, QString);   // 校验 SN
    static int checkCCID(QObject *, QSerialPort *, QString); // 校验 CCID
    static int checkIMEI(QObject *, QSerialPort *, QString); // 校验 IMEI
    static int writeToPassinfo(QObject *, QSqlQuery *, QString, QString, QString, QString);  // 写入测试结果
    static void checkProc(QObject * receiver,void * args); // 检查线程

    static QSerialPort * serialOpen(QString name);
    static void postMessage(QObject * receiver,QString msg);
    static bool isCalibrated(QSerialPort * pcom);

    void onEventCheckDone();
    void onEventCheckFail();
    void onEventCheckMsg(QString msg);
    void customEvent(QEvent *e); // 处理自定义事件
};

#endif // MAINWINDOW_H

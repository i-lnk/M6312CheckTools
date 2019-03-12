#include "mainwindow.h"
#include "dbsetdialog.h"
#include "uevent.h"
#include "error.h"
#include "ui_mainwindow.h"
#include <QtConcurrent/QtConcurrent>
#include <QMessageBox>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QSqlDatabase>
#include <QSqlError>

#pragma execution_character_set("utf-8")

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    Qt::WindowFlags flags = 0;
    flags |= Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint;
    setWindowFlags(flags); // 设置禁止最大化
    setFixedSize(this->width(),this->height());

    settings = new QSettings("config.ini",QSettings::IniFormat);
    QString port = settings->value("setup/PORT").toString();
    QString OID = settings->value("setup/OID").toString();
    QString PID = settings->value("setup/PID").toString();
    ui->lineEditPID->setText(PID);

    QStringList comPorts;
    foreach(const QSerialPortInfo &info,QSerialPortInfo::availablePorts())
    {
        comPorts << info.portName();
        qDebug()<<"COM PORT:"<<info.portName();
    }
    ui->comboBoxCom->addItems(comPorts);
    ui->comboBoxCom->setCurrentText(port);

//    QRegExp regx("[0-9]+$");
//    QValidator *validator = new QRegExpValidator(regx, ui->lineEditScanIMEI );
//    ui->lineEditScanIMEI->setValidator( validator );

    ui->lineEditScanIMEI->setFocus();

    connect(ui->buttonStart,SIGNAL(clicked()),this,SLOT(onButtonStartClicked()));
    connect(ui->buttonClean,SIGNAL(clicked()),this,SLOT(onButtonCleanClicked()));
    connect(ui->buttonDBSet,SIGNAL(clicked()),this,SLOT(onButtonDBSetClicked()));

    connect(ui->lineEditScanSN,   SIGNAL(editingFinished()), this,SLOT(parseString()));
    connect(ui->lineEditScanIMEI, SIGNAL(editingFinished()), this,SLOT(parseString()));
    connect(ui->lineEditScanCCID, SIGNAL(editingFinished()), this,SLOT(parseString()));
    connect(ui->comboBoxCom, SIGNAL(currentIndexChanged(const QString &)), this,SLOT(onComboBoxIndexChanged(const QString &)));

    ui->lineEditScanCCID->setEnabled(false);
    ui->lineEditBurnCCID->setEnabled(false);

    if(!QFile::exists("dbset.ini")){
        onButtonDBSetClicked();
    }
}

void MainWindow::postMessage(QObject *receiver, QString msg){
    QApplication::postEvent(
        receiver,
        new UEvent(QEvent::User,EVENT_CHECK_MSG,msg)
        );
}

bool MainWindow::isCalibrated(QSerialPort * pcom){
    pcom->clear();
    pcom->write("at+cminfo\r\n");
    QThread::msleep(100);
    QByteArray bytes = pcom->readAll();
    while (pcom->waitForReadyRead(300)){
        bytes.append(pcom->readAll());
    }
    QString msg = QString().fromLatin1(bytes);
    qDebug() << msg;
    if(msg.contains("CALIBFLAG: ") && !msg.contains("0000")){
        return true;
    }
    return false;
}

int MainWindow::checkCCID(QObject * receiver, QSerialPort * pcom,QString ScanCCID){
    if(ScanCCID.isEmpty()) return 0;    // 如果没有扫描到CCID则跳过该检测

    pcom->clear();
    pcom->write("AT+CCID\r\n");
    QByteArray bytes = pcom->readAll();
    while (pcom->waitForReadyRead(300)){
        bytes.append(pcom->readAll());
    }
    QString msg = QString().fromLatin1(bytes);
    QStringList lines = msg.split("\r\n",QString::SkipEmptyParts);
    qDebug() << msg;
    if(lines.length() < 2){
        postMessage(receiver, "读取CCID失败.");
        return TERR_MD_READ_CCID;
    }

    msg = lines.at(1);
    msg.replace("+CCID: ","");

    QString burnCCID = msg;
    MainWindow * mwin = (MainWindow *)receiver;
    mwin->ui->lineEditBurnCCID->setText(burnCCID);

    if(burnCCID.compare(ScanCCID) != 0){
        postMessage(receiver, "片内 CCID 与标签不符 校验失败.");
        return TERR_MD_IMEI_NOT_MATCH;
    }

    return 0;
}

int MainWindow::checkIMEI(QObject * receiver, QSerialPort * pcom,QString ScanIMEI){
    pcom->clear();
    pcom->write("AT+GSN\r\n");
    QByteArray bytes = pcom->readAll();
    while (pcom->waitForReadyRead(300)){
        bytes.append(pcom->readAll());
    }
    QString msg = QString().fromLatin1(bytes);
    if(msg.contains("OK") == false){
        postMessage(receiver, "读取IMEI失败.");
        return TERR_MD_READ_IMEI;
    }

    msg.replace("OK","");
    msg.replace("+GSN: ","");
    msg.replace("\r","");
    msg.replace("\n","");
    msg.trimmed();

    QString burnIMEI = msg;
    MainWindow * mwin = (MainWindow *)receiver;
    mwin->ui->lineEditBurnIMEI->setText(burnIMEI);

    if(burnIMEI.compare(ScanIMEI) != 0){
        postMessage(receiver, "片内 IMEI 与标签不符 校验失败.");
        return TERR_MD_IMEI_NOT_MATCH;
    }

    return 0;
}

int MainWindow::checkSN(QObject * receiver, QSerialPort * pcom,QString ScanSN){
    pcom->clear();
    pcom->write("at^sn\r\n");
    QByteArray bytes = pcom->readAll();
    while (pcom->waitForReadyRead(300)){
        bytes.append(pcom->readAll());
    }
    QString msg = QString().fromLatin1(bytes);

    if(msg.contains("OK") == false){
        postMessage(receiver, "读取S/N失败.");
        return TERR_MD_READ_SN;
    }

    msg.replace("OK","");
    msg.replace("^SN: ","");
    msg.replace("\"","");
    msg.replace("\r","");
    msg.replace("\n","");
    msg.trimmed();

    QString BurnSN = msg;
    MainWindow * mwin = (MainWindow *)receiver;
    mwin->ui->lineEditBurnSN->setText(BurnSN);

    if(BurnSN.compare(ScanSN) != 0){
        postMessage(receiver, "片内 S/N 与标签不符 校验失败.");
        return TERR_MD_SN_NOT_MATCH;
    }

    return 0;
}

QSerialPort * MainWindow::serialOpen(QString name){
    QSerialPort * pcom = new QSerialPort(name);
    if(!pcom->open(QIODevice::ReadWrite))//用ReadWrite 的模式尝试打开串口
    {
        return nullptr;
    }

    pcom->setBaudRate(QSerialPort::Baud115200,QSerialPort::AllDirections);//设置波特率和读写方向
    pcom->setDataBits(QSerialPort::Data8);      //数据位为8位
    pcom->setFlowControl(QSerialPort::NoFlowControl);//无流控制
    pcom->setParity(QSerialPort::NoParity); //无校验位
    pcom->setStopBits(QSerialPort::OneStop); //一位停止位

    return pcom;
}

// 测试线程
void MainWindow::checkProc(QObject * receiver,void * args)
{
    MainWindow * mwin = (MainWindow *)receiver;
    QString comPortName = mwin->ui->comboBoxCom->currentText();

    QString ScanIMEIStr = mwin->ui->lineEditScanIMEI->text();
    QString ScanSNStr = mwin->ui->lineEditScanSN->text();
    QString ScanCCIDStr = mwin->ui->lineEditScanCCID->text();

    QString TempStr;
    bool initDone = false;

    QSqlDatabase db;
    QSqlQuery query;
    QSqlError dbErr;

    int err = -1;
    int cnt =  8;

    QString OID;  // 订单号码
    QString PID = mwin->ui->lineEditPID->text();  // 工位号码

    QSettings * dbSet = new QSettings("dbset.ini",QSettings::IniFormat);
    QString Server = dbSet->value("ODBC/Server").toString();
    QString DBName = dbSet->value("ODBC/DBName").toString();
    QString User = dbSet->value("ODBC/User").toString();
    QString Pass = dbSet->value("ODBC/Pass").toString();
    delete dbSet;

    // 打开串口
    QSerialPort * pcom = serialOpen(comPortName);
    if(pcom == nullptr){
        postMessage(receiver, "打开串口失败:" + comPortName + ".");
        goto _check_fail;
    }

    // 连接数据库
    db = QSqlDatabase::addDatabase("QODBC");
    db.setDatabaseName(QString("Driver={SQL Server};Server=%1;Database=%2;Uid=%3;Pwd=%4")
                       .arg(Server)
                       .arg(DBName)
                       .arg(User)
                       .arg(Pass)
                       );

    if(!db.open()){
        postMessage(receiver, "打开数据库失败:.");
        goto _check_fail;
    }

    query = QSqlQuery(db);

    postMessage(receiver, "请在1分钟内放入模块，锁死夹具并打开夹具电源.");

    // 等待夹具锁死上电
    while(cnt--){
        pcom->clear();
        pcom->write("at\r\n");
        QByteArray bytes = pcom->readAll();
        while (pcom->waitForReadyRead(1000)){
            bytes.append(pcom->readAll());
        }
        QString msg = QString().fromLatin1(bytes);
        if(msg.contains("OK\r\n")){
            initDone = true;
            break;
        }
        postMessage(receiver, QString("连接中，请稍后...%1").arg(cnt));
        QThread::sleep(1);
    }

    for(int i = 0;i < 10;i++){
        postMessage(receiver, QString("等待测试开始...%1").arg(i));
        QThread::sleep(1);
    }

    // 等待超时
    if(initDone == false){
        postMessage(receiver, "初始化超时，请检查是否锁死夹具并打开电源.");
        goto _check_fail;
    }

    // 核对标签和片内的信息
    if((err = checkIMEI(mwin,pcom,ScanIMEIStr)) != 0) goto _check_fail;
    if((err = checkCCID(mwin,pcom,ScanCCIDStr)) != 0) goto _check_fail;
    if((err = checkSN(mwin,pcom,ScanSNStr)) != 0)     goto _check_fail;

    // 核对标签和数据库的信息
    err = checkByDatabase(receiver,&query,ScanIMEIStr,ScanSNStr);
    if(err != 0){
        postMessage(receiver, "数据校验......失败.");
        goto _check_fail;
    }else{
        postMessage(receiver, "数据校验......成功.");
    }

    // 获取订单ID号
    TempStr = QString("SELECT orderid FROM %1 WHERE mbl = '%2';")
            .arg("indata")
            .arg(ScanSNStr);

    if(!query.exec(TempStr)){
        postMessage(receiver, "数据库操作错误:" + query.lastError().text() + ".");
        postMessage(receiver, "查询订单......失败.");
    }else{
        if(!query.next()){
            postMessage(receiver, "获取订单......失败.");
            goto _check_fail;
        }

        OID = query.value(0).toString();
        postMessage(receiver, "获取订单:[" + OID + "]......成功.");
    }

    query.clear();

    // 检查校准综合测试是否完成
    if(!isCalibrated(pcom)){
        err = TERR_MD_WITHOUT_CALIBRATE;
        postMessage(receiver, "模组没有进行校准综测.");
        goto _check_fail;
    }else{
        postMessage(receiver, "校准综测......通过.");
    }

    // 功能检查
    err = checkByFunction(receiver,pcom);
    if(err != 0){
        postMessage(receiver, "功能测试......失败.");
        goto _check_fail;
    }else{
        postMessage(receiver, "功能测试......成功.");
    }

    // 写入测试结果
    writeToPassinfo(receiver,&query,OID,PID,ScanSNStr,"1");

    postMessage(receiver, "模组测试完成，功能正常.");
    QApplication::postEvent(
        receiver,
        new UEvent(QEvent::User,EVENT_CHECK_DONE)
        );

    pcom->close();
    db.close();

    return;

_check_fail:

    // 写入测试结果
    writeToPassinfo(receiver,&query,OID,PID,ScanSNStr,"0");

    QApplication::postEvent(
        receiver,
        new UEvent(QEvent::User,EVENT_CHECK_FAIL)
        );

    pcom->close();
    db.close();

    return;
}

// 校验结果写入数据库
int MainWindow::writeToPassinfo(
    QObject * receiver,
    QSqlQuery * pQuery,
    QString OID,            // 订单号
    QString PID,            // 工位
    QString ScanSN,
    QString TestResult
){
    QDateTime local(QDateTime::currentDateTime());
    QString TestTime = local.toString("yyyy-MM-dd HH:mm:ss");
    QString sql;

    sql = QString("DELETE FROM %1 WHERE mbl = '%2';")
                .arg("passinfo")
                .arg(ScanSN);

    if(!pQuery->exec(sql)){
        qDebug() << pQuery->lastError().text();
    }
    pQuery->clear();

    sql = QString(
                "INSERT INTO "
                "%1(mbl,toolsindex,passtime,state,computerid,userid,factoryid,orderid) "
                "VALUES('%2','%3','%4','%5','%6','%7','%8','%9')"
                 )
                .arg("passinfo")
                .arg(ScanSN)
                .arg(PID)
                .arg(TestTime)
                .arg(TestResult)   // 测试结果
                .arg('0').arg('1').arg("888")
                .arg(OID);

    if(!pQuery->exec(sql)){
        postMessage(receiver, "数据库操作错误:" + pQuery->lastError().text() + ".");
        postMessage(receiver, "更新数据......失败.");
        return false;
    }else{
        postMessage(receiver, "更新数据......成功.");
    }

    return true;
}

// 通过数据库校验
int MainWindow::checkByDatabase(
    QObject * receiver,
    QSqlQuery * pQuery,
    QString ScanIMEI,
    QString ScanSN
){
    QString sql;
    QString msg;
    int err = -1;

    QString dbIMEI;
    QString dbSN;

    // 查询标签上的SN是否存在于数据库中

    sql = QString("SELECT sn FROM %1 WHERE mbl = '%2';")
            .arg("indata")
            .arg(ScanSN);

    if(!pQuery->exec(sql)){
        msg = "数据库操作错误:" + pQuery->lastError().text() + ".";
        goto _jump_err;
    }

    if(!pQuery->next()){
        msg = "数据库查询失败:[" + ScanSN + "] 不存在.";
        goto _jump_err;
    }

    pQuery->clear();

    // 查询数据库的IMEI和标签的IMEI是否匹配
    sql = QString("SELECT imei FROM %1 WHERE mbl = '%2';")
            .arg("indata")
            .arg(ScanSN);

    if(!pQuery->exec(sql)){
        msg = "数据库操作错误:" + pQuery->lastError().text() + ".";
        goto _jump_err;
    }

    if(!pQuery->next()){
        msg = "数据库查询失败:[" + ScanSN + "] 不存在.";
        goto _jump_err;
    }

    dbIMEI = pQuery->value(0).toString();

    if(dbIMEI.compare(ScanIMEI) == 0){
        msg = "数据库匹配失败:[" + dbIMEI + "][" + ScanIMEI + "] IMEI 不符合.";
        err = TERR_DB_IMEI_NOT_MATCH;
        goto _jump_err;
    }

    pQuery->clear();

    return 0;

_jump_err:
    pQuery->clear();
    postMessage(receiver, "" + msg + "");
    return err;
}

static bool checkATCSQ(QString msg, void * args){
    return msg.contains("OK");
}

static bool checkATCommon(QString msg, void * args){
    return msg.contains("OK");
}

// 通过功能校验
int MainWindow::checkByFunction(QObject * receiver,QSerialPort * pcom)
{
    typedef struct _T_CMD_CHECK{
        char cmd[128];
        bool (*check)(QString msg,void * arg);
    }T_CMD_CHECK;

    T_CMD_CHECK atCmds[] = {
        {"at^curc=0\r\n", checkATCommon},   // 0
        {"at+cfun=1\r\n", checkATCommon},   // 1
        {"at+cpin?\r\n", checkATCommon},    // 2
        {"at+cgatt?\r\n", checkATCommon},   // 3
        {"at^curc=0\r\n", checkATCommon},   // 4
        {"at+csq\r\n", checkATCSQ},         // 5
        {"at+cpin?\r\n", checkATCommon}     // 6
    };

    for(int i = 0; i < sizeof(atCmds)/sizeof(atCmds[0]); i++){
        pcom->clear();
        pcom->write(atCmds[i].cmd);
        QByteArray bytes = pcom->readAll();
        while (pcom->waitForReadyRead(500)){
            bytes.append(pcom->readAll());
        }
        QString cmd = QString::fromLatin1(atCmds[i].cmd).trimmed();
        QString msg = QString().fromLatin1(bytes);
        qDebug() << msg;

        bool done = false;

        for(int t = 0;t < 3;t++){
            done = atCmds[i].check(msg,nullptr);
            if(done){
                break;
            }
        }

        if(done){
            postMessage(receiver, "测试指令：[" + cmd + "]......成功.");
        }else{
            postMessage(receiver, "测试指令：[" + cmd + "]......返回.");
            postMessage(receiver, msg);
            return TERR_MD_TEST_FUNCTION;
        }
    }

    return 0;
}

void MainWindow::onComboBoxIndexChanged(const QString & str){
    settings->beginGroup("setup");
    settings->setValue("PORT",ui->comboBoxCom->currentText());
    settings->endGroup();
    settings->sync();
    ui->lineEditScanIMEI->setFocus();
}

// 清除按钮点击
void MainWindow::onButtonCleanClicked(){
    ui->textConsole->clear();
    onEventCheckDone();
    ui->textConsole->setStyleSheet("QTextEdit { background: white }");
}

// 开始按钮点击
void MainWindow::onButtonStartClicked(){
    ui->textConsole->clear();
    ui->lineEditBurnSN->clear();
    ui->lineEditBurnCCID->clear();
    ui->lineEditBurnIMEI->clear();

    ui->lineEditScanIMEI->setFocus();
    ui->textConsole->setStyleSheet("QTextEdit { background: white }");

    if(!QFile::exists("dbset.ini")){
        onButtonDBSetClicked();
        return;
    }

    if(ui->lineEditPID->text().isEmpty()){
        QMessageBox::critical(
                    this,
                    QString::fromUtf8("警告"),
                    QString::fromUtf8("工位号不能为空."),
                    QMessageBox::Ok);
        return;
    }

    settings->beginGroup("setup");
    settings->setValue("PID",ui->lineEditPID->text());
    settings->endGroup();
    settings->sync();

    if(ui->lineEditScanIMEI->text().isEmpty()
            || ui->lineEditScanSN->text().isEmpty()){
        QMessageBox::critical(
                    this,
                    QString::fromUtf8("警告"),
                    QString::fromUtf8("IMEI号码和S/N号码不能为空，请扫码后再开始测试."),
                    QMessageBox::Ok);
        return;
    }

    ui->buttonStart->setEnabled(false);
    ui->buttonClean->setEnabled(false);
    ui->buttonDBSet->setEnabled(false);
    checkProcHandle = QtConcurrent::run(checkProc,this,nullptr);
}

void MainWindow::onButtonDBSetClicked(){
    DBSetDialog * dbSetDlg = new DBSetDialog();
    dbSetDlg->setModal(true);
    dbSetDlg->show();
}

void MainWindow::parseString(){
    QString dataStr = ui->lineEditScanSN->text();
    if(dataStr.isEmpty()){
        dataStr = ui->lineEditScanIMEI->text();
        if(dataStr.isEmpty()) return;
    }

    dataStr.replace("；",";");
    QStringList lines = dataStr.split(";");

    switch(lines.length()){
        case 3:
            ui->lineEditBurnCCID->setEnabled(true);
            ui->lineEditScanCCID->setEnabled(true);
            ui->lineEditScanCCID->setText(lines.at(2));
        case 2:
            ui->lineEditScanIMEI->setText(lines.at(0));
            ui->lineEditScanSN->setText(lines.at(1));
            break;
        default:
            return;
    }

    onButtonStartClicked();
}

void MainWindow::onEventCheckDone(){
    ui->lineEditScanCCID->clear();
    ui->lineEditBurnCCID->clear();
    ui->lineEditScanIMEI->clear();
    ui->lineEditBurnIMEI->clear();
    ui->lineEditScanSN->clear();
    ui->lineEditBurnSN->clear();

    ui->lineEditScanIMEI->setFocus();

    ui->lineEditScanCCID->setEnabled(false);
    ui->lineEditBurnCCID->setEnabled(false);
    ui->buttonStart->setEnabled(true);
    ui->buttonClean->setEnabled(true);
    ui->buttonDBSet->setEnabled(true);
    ui->textConsole->setStyleSheet("QTextEdit { background: green }");
}

void MainWindow::onEventCheckFail(){
    ui->buttonStart->setEnabled(true);
    ui->buttonClean->setEnabled(true);
    ui->buttonDBSet->setEnabled(true);
    ui->textConsole->setStyleSheet("QTextEdit { background: red }");
}

void MainWindow::onEventCheckMsg(QString msg){
    ui->textConsole->append(msg);
}

// 自定义事件接收
void MainWindow::customEvent(QEvent *e){
    UEvent * uEvent = static_cast<UEvent *>(e);
    switch(uEvent->eventCode){         
        case EVENT_CHECK_DONE:
            onEventCheckDone();
            break;
        case EVENT_CHECK_FAIL:
            onEventCheckFail();
            break;
        case EVENT_CHECK_MSG:
            onEventCheckMsg(uEvent->message);
            break;
        default:
            break;
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

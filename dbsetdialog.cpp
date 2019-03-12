#include "dbsetdialog.h"
#include "uevent.h"
#include "ui_dbsetdialog.h"

#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>
#include <QtConcurrent/QtConcurrent>

#pragma execution_character_set("utf-8")

DBSetDialog::DBSetDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DBSetDialog)
{
    ui->setupUi(this);

    connect(ui->buttonTest,SIGNAL(clicked()),this,SLOT(onButtonTestClicked()));
    connect(ui->buttonSave,SIGNAL(clicked()),this,SLOT(onButtonSaveClicked()));
    connect(ui->buttonExit,SIGNAL(clicked()),this,SLOT(onButtonExitClicked()));

    dbSettings = new QSettings("dbset.ini",QSettings::IniFormat);
    ui->lineEditServer->setText(dbSettings->value("ODBC/Server").toString());
    ui->lineEditDBName->setText(dbSettings->value("ODBC/DBName").toString());
    ui->lineEditUser->setText(dbSettings->value("ODBC/User").toString());
    ui->lineEditPass->setText(dbSettings->value("ODBC/Pass").toString());
}

void DBSetDialog::dbConnectionTestProc(QObject * receiver,void * args){
    QSqlDatabase db;
    QSqlQuery query;
    QString msg;
    QString sql;
    bool isTBExists = false;

    DBSetDialog * pDlg = (DBSetDialog *)receiver;
    Ui::DBSetDialog * ui = pDlg->ui;

    db = QSqlDatabase::addDatabase("QODBC");
    db.setDatabaseName(QString("Driver={SQL Server};Server=%1;Database=%2;Uid=%3;Pwd=%4")
                       .arg(ui->lineEditServer->text())
                       .arg(ui->lineEditDBName->text())
                       .arg(ui->lineEditUser->text())
                       .arg(ui->lineEditPass->text())
                       );

    if(!db.open()){
        msg = db.lastError().text();
        goto _jump_err;
    }

    query = QSqlQuery(db);
    sql = QString("SELECT NAME FROM SYSOBJECTS WHERE XTYPE='U';");

    if(!query.exec(sql)){
        msg = db.lastError().text();
        goto _jump_err;
    }

    while(query.next()){
        QString tableName = query.value(0).toString();
        qDebug() << tableName;
        if(tableName.compare("indata") == 0){
            isTBExists = true;
            break;
        }
    }
    query.clear();

    if(!isTBExists){
        msg = QString::fromUtf8("当前项目不再数据库中，请联系管理人员。");
        goto _jump_err;
    }

    db.close();

    QApplication::postEvent(
        receiver,
        new UEvent(QEvent::User,EVENT_DB_TEST_DONE)
        );

    return;

_jump_err:
    QApplication::postEvent(
        receiver,
        new UEvent(QEvent::User,EVENT_DB_TEST_FAIL,msg)
        );

    return;
}

void DBSetDialog::onButtonTestClicked(){
    QString msg;

    if(ui->lineEditServer->text().isEmpty()){
        msg = QString::fromUtf8("服务器不能为空");
        ui->lineEditServer->setFocus();
        goto _jump_err;
    }

    if(ui->lineEditDBName->text().isEmpty()){
        msg = QString::fromUtf8("数据库不能为空");
        ui->lineEditDBName->setFocus();
        goto _jump_err;
    }

    ui->buttonTest->setEnabled(false);
    dbConnectionTestHandle = QtConcurrent::run(dbConnectionTestProc,this,nullptr);

    return;

_jump_err:

    QMessageBox::critical(
        this,
        QString::fromUtf8("错误"),
        msg,
        QMessageBox::Ok);
}

void DBSetDialog::onButtonSaveClicked(){
    dbSettings->beginGroup("ODBC");
    dbSettings->setValue("Server",ui->lineEditServer->text());
    dbSettings->setValue("DBName",ui->lineEditDBName->text());
    dbSettings->setValue("User",ui->lineEditUser->text());
    dbSettings->setValue("Pass",ui->lineEditPass->text());
    dbSettings->endGroup();
    dbSettings->sync();
    dbConnectionTestHandle.waitForFinished();
    close();
}

void DBSetDialog::onButtonExitClicked(){
    dbConnectionTestHandle.waitForFinished();
    close();
}

// 自定义事件接收
void DBSetDialog::customEvent(QEvent *e){
    UEvent * uEvent = static_cast<UEvent *>(e);
    switch(uEvent->eventCode){
        case EVENT_DB_TEST_FAIL:
            QMessageBox::critical(
                this,
                QString::fromUtf8("错误"),
                uEvent->message,
                QMessageBox::Ok);
            ui->buttonTest->setEnabled(true);
            break;
        case EVENT_DB_TEST_DONE:
            QMessageBox::information(
                this,
                QString::fromUtf8("成功"),
                QString::fromUtf8("数据库连接成功"),
                QMessageBox::Ok);
            ui->buttonTest->setEnabled(true);
            break;
        default:
            break;
    }
}

DBSetDialog::~DBSetDialog()
{
    delete ui;
}

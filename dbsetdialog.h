#ifndef DBSETDIALOG_H
#define DBSETDIALOG_H

#include <QDialog>
#include <QFuture>
#include <QSettings>

namespace Ui {
class DBSetDialog;
}

#define EVENT_DB_TEST_DONE 2001
#define EVENT_DB_TEST_FAIL 2002

class DBSetDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DBSetDialog(QWidget *parent = nullptr);
    ~DBSetDialog();

private slots:
    void onButtonTestClicked();
    void onButtonSaveClicked();
    void onButtonExitClicked();

private:
    Ui::DBSetDialog *ui;
    QSettings * dbSettings;
    QFuture<void> dbConnectionTestHandle;  // 线程句柄

    static void dbConnectionTestProc(QObject * receiver,void * args = nullptr);

    void customEvent(QEvent *e); // 处理自定义事件
};

#endif // DBSETDIALOG_H

#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QTcpServer>   /* QTcpServer类（负责通信监听） */
#include <QTcpSocket>   /* QTcpSocket类（负责通信传输） */
#include <QString>

namespace Ui {
class Widget;
}

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = 0);
    ~Widget();

    /* 定义TCP监听与传输 */
    QTcpServer *tcpServer;
    QTcpSocket *tcpSocket;

private slots:

    void on_OpenButton_clicked();

    void on_CloseButton_clicked();

    void on_SendButton_clicked();

    /* 声明新连接事件 */
    void newConnection_Slot();

    /* 声明准备读事件 */
    void readyRead_Slot();

    /* 服务器与客户端断连 */
    void clientDisconnected_Slot();

private:
    Ui::Widget *ui;
};

#endif // WIDGET_H

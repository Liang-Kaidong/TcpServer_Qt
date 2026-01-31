#include "widget.h"
#include "ui_widget.h"
#include <QMessageBox>
#include <QAbstractSocket>

Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget)
{
    ui->setupUi(this);
    setWindowTitle("服务端");
    setWindowIcon(QIcon(":/Icons/server.ico"));

    /* new 防内存泄漏 */
    tcpServer = new QTcpServer(this);
    tcpSocket = nullptr;    /* 服务端会接受多个客户端，创建多个tcpSocket */

    /* 提前绑定连接请求信号 */
    connect(tcpServer, SIGNAL(newConnection()), this, SLOT(newConnection_Slot()));
}

Widget::~Widget()
{
    delete ui;
}

/* 收到客户端的连接请求的槽函数 */
void Widget::newConnection_Slot()
{
    /**
     * 绑定数据接收信号前，需执行两次disconnect：分别清理旧对象、新对象的绑定，核心目的是杜绝"幽灵触发"和"重复绑定"
     * 分步解释（结合实操场景）：
     * 1. 第一次disconnect（清理旧客户端对象）：
     *    场景：客户端A连接→tcpSocket指向S1→绑定1次readyRead；客户端B连接→tcpSocket指向S2（S1被"抛弃"但仍在内存）
     *    风险：若客户端A异常断链，TCP协议/操作系统会触发S1的readyRead信号，而S1的绑定未清→槽函数执行时，
     *          会错误读取当前tcpSocket指向的S2数据（或空指针崩溃，即"幽灵触发"）
     *    作用：取消旧对象S1的readyRead绑定，让被抛弃的旧对象"干净离场"，避免幽灵触发
     *
     * 2. 第二次disconnect（清理新客户端对象）：
     *    场景：Qt会复用旧对象（如S1被放入对象池，客户端C连接时复用S1），复用的S1仍保留之前的readyRead绑定
     *    风险：若直接绑定新连接的S1，会导致绑定次数叠加（如S1原有1次绑定+新绑定1次=2次），客户端C发数据时槽函数执行2次（重复触发）
     *    作用：对新拿到的对象（无论是否复用）做"前置防御"，清空其可能存在的旧绑定，确保后续connect只建立1次绑定关系
     *
     * 完整风险流程示例：
     * ① 客户端A连接→tcpSocket=S1→绑定1次readyRead→发"hello"→槽函数执行1次（正常）
     * ② 客户端B连接→tcpSocket=S2→S1仍在内存且绑定未清；S2被创建/复用（绑定0次）
     * ③ 客户端A异常断链→S1的readyRead触发→槽函数执行→错误读取S2数据（崩溃）
     * ④ 客户端C连接→Qt复用S1→直接绑定→S1绑定次数变为2次→发"test"→槽函数执行2次（重复触发）
     *
     * 简化说明：
     * - 使用disconnect(obj, nullptr, this, nullptr)批量断开对象与当前类的所有绑定，替代逐个断开的冗余写法
     * - 效果等价且更彻底，避免漏断信号（如disconnected、readyRead等）
     * - 注：disconnect对无绑定关系的对象无副作用，是Qt防御性编程的标准操作
     */

    /* 断开旧客户端的绑定 */
    if (tcpSocket != nullptr) {
        disconnect(tcpSocket, nullptr, this, nullptr); /* 清空旧对象的所有绑定 */
        tcpSocket->abort();
    }

    /* 重新接收连接请求 */
    tcpSocket = tcpServer->nextPendingConnection();

    /* 清空新对象的所有旧绑定（防御复用），再绑定需要的信号 */
    disconnect(tcpSocket, nullptr, this, nullptr);
    connect(tcpSocket, SIGNAL(readyRead()), this, SLOT(readyRead_Slot()));
    connect(tcpSocket, SIGNAL(disconnected()), this, SLOT(clientDisconnected_Slot()));

    /* 消息提醒 */
    QMessageBox::information(this, "提示", "客户端已连接！");
    ui->OpenButton->setEnabled(false);
}

/* 接收数据的槽函数 */
void Widget::readyRead_Slot()
{
    /* 判断是否连接到客户端 */
    if (tcpSocket == nullptr || tcpSocket->state() != QTcpSocket::ConnectedState) {
        return;
    }
    QString buf = tcpSocket->readAll();

    /* 显示数据 */
    ui->RecieveEdit->appendPlainText(buf);
}

/* 点击打开服务器按钮 */
void Widget::on_OpenButton_clicked()
{
    /* 1.禁用打开按钮，防重复点击 */
    ui->OpenButton->setEnabled(false);

    /* 2.判断端口号是否为空 */
    QString portStr = ui->PortEdit->text();
    if (portStr.isEmpty()) {
        QMessageBox::warning(this, "提示", "端口号不能为空！");
        ui->OpenButton->setEnabled(true);
        return;
    }

    /**
     * 3.监听来自所有人的连接
     *
     * listen(const QHostAddress &address = QHostAddress::Any, quint16 port = 0)
     * Any：所有人
     * text()：获取输入的文本
     * .toUInt：强制转为无符号整数    quint16规定要
     */
    tcpServer->listen(QHostAddress::Any, ui->PortEdit->text().toUInt());
}

/* 点击关闭服务器的槽函数 */
void Widget::on_CloseButton_clicked()
{
    tcpServer->close();
    if (tcpSocket != nullptr) {
        tcpSocket->abort();
        disconnect(tcpSocket, SIGNAL(readyRead()), this, SLOT(readyRead_Slot()));
        tcpSocket = nullptr;
    }

    QMessageBox::information(this, "提示", "服务器已关闭！");
    ui->OpenButton->setEnabled(true);
}

/* 发送数据的槽函数 */
void Widget::on_SendButton_clicked()
{
    /* 1.校验连接状态 */
    if (tcpSocket == nullptr || tcpSocket->state() != QTcpSocket::ConnectedState) {
        QMessageBox::critical(this, "失败", "未连接客户端，无法发送！");
        return;
    }

    /* 2.校验内容非空 */
    QString sendText = ui->SendEdit->text();
    if (sendText.isEmpty()) {
        QMessageBox::warning(this, "提示", "发送内容不能为空！");
        return;
    }

    /* 3.发送数据 */
    tcpSocket->write(ui->SendEdit->text().toUtf8().data());
    ui->SendEdit->clear();
}

/* 客户端断开连接的槽函数 */
void Widget::clientDisconnected_Slot()
{
    if (tcpSocket == nullptr) {
        return;
    }
    /* 断连提醒 */
    QMessageBox::information(this, "提示", "客户端已断开连接！");
    /* 清理当前客户端的绑定（避免幽灵触发） */
    disconnect(tcpSocket, SIGNAL(readyRead()), this, SLOT(readyRead_Slot()));
    disconnect(tcpSocket, SIGNAL(disconnected()), this, SLOT(clientDisconnected_Slot()));
    tcpSocket->abort();
    tcpSocket = nullptr;
    ui->OpenButton->setEnabled(false);
}

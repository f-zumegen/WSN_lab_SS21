/**
 * Serial UART interface for data packets. This class receives and sends data packets over UART.
 * \author Paul-Émile Arnaly
 */

#ifndef UART_H
#define UART_H

#include <QObject>

#include "qextserialport.h"
#include "qextserialenumerator.h"

#define START_CHAR                  1
#define DEACTIVATION_CHAR           13//0x0d //254
#define END_CHAR                    10//0x0a //255

class Uart : public QObject {
    Q_OBJECT
public:
    explicit Uart(QObject *parent = 0);
    bool isOpen();

public slots:
    void open(QString path);
    void close();
    void send(QByteArray data);
    QList<QextPortInfo> getPorts();
    QList<QextPortInfo> getUSBPorts();

private slots:
    void receive();

signals:
    void debugReceived(QString str);
    void packetReceived(QByteArray data);

private:
    QextSerialPort port;

};

#endif // UART_H

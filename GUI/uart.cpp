/**
 * Serial UART interface for data packets. This class receives and sends data packets over UART.
 * \author Paul-Émile Arnaly
 */

#include "uart.h"
#include <QDebug>

Uart::Uart(QObject *parent) : QObject(parent) {
    port.setQueryMode(QextSerialPort::EventDriven);
    port.setBaudRate(BAUD115200);
    port.setFlowControl(FLOW_OFF);
    port.setParity(PAR_NONE);
    port.setDataBits(DATA_8);
    port.setStopBits(STOP_1);

    QObject::connect(&port, SIGNAL(readyRead()), this, SLOT(receive()));
}

void Uart::receive() {
    static bool processing;
    static QByteArray str;
    static QByteArray packetContent;

    if (processing) return;
    processing = true;

    static bool packet = false;
    static bool deactivate = false;

    char c;

    while (!port.atEnd()) {
        port.getChar(&c);

        if (packet) {
            qDebug() << "packet: char " << QString::number(c);
            if (deactivate || ((unsigned char) c != DEACTIVATION_CHAR && (unsigned char) c != END_CHAR)) {
                packetContent.append(c);
                deactivate = false;
            } else {
                if ((unsigned char) c == DEACTIVATION_CHAR) {
                    deactivate = true;
                } else if ((unsigned char) c == END_CHAR) {
                    qDebug() << "    end";
                    emit packetReceived(packetContent);
                    packet = false;
                    packetContent.clear();
                }
            }
        } else {
            qDebug() << "text: char " << QString::number(c);
            if (c == START_CHAR) {
                packet = true;
            } else {
                if (c != '\r' && c != '\n') {
                    str.append(c);
                    qDebug() << "    append";
                }
                if (c == '\n') {    // End of line, start decoding
                                qDebug() << "    end";
                    //str.replace('\r\n', "");
                    emit debugReceived(str);
                    str.clear();
                }
            }
        }
    }
    processing = false;
}

bool Uart::isOpen() {
    return this->port.isOpen();
}

void Uart::open(QString path) {
    port.setPortName(path);
    port.open(QIODevice::ReadWrite);
}

void Uart::close() {
    if (this->port.isOpen()) this->port.close();
}

void Uart::send(QByteArray data) {
    QByteArray dataDeactivated;
    dataDeactivated = data;

    dataDeactivated.replace(QString("%1").arg(QChar( DEACTIVATION_CHAR )), QString("%1%2").arg(QChar( DEACTIVATION_CHAR ), QChar( DEACTIVATION_CHAR )).toUtf8());
    dataDeactivated.replace(QString("%1").arg(QChar( END_CHAR )), QString("%1%2").arg(QChar( DEACTIVATION_CHAR ), QChar( END_CHAR )).toUtf8());

    dataDeactivated.append((char) END_CHAR);

    port.write(dataDeactivated);
}


QList<QextPortInfo> Uart::getPorts() {
    return QextSerialEnumerator::getPorts();
}


QList<QextPortInfo> Uart::getUSBPorts() {
    QList<QextPortInfo> ports = QextSerialEnumerator::getPorts();
    QList<QextPortInfo> usbPorts;
    for (int i = 0; i < ports.size(); i++) {
        if (ports.at(i).portName.left(6) == "ttyUSB") {
            usbPorts.append(ports.at(i));
        }
        //ui->comboBox_Interface->addItem(ports.at(i).portName.toLocal8Bit().constData());
    }
    return usbPorts;
}

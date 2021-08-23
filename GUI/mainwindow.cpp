/**
 * @file mainwindow.cpp
 */

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <qdebug.h>
#include <math.h>
#include <vector>

#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QFile>
#include <QDir>
#include <QFileDialog>

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QStyleOption>
#include <QtMath>
#include <QtWidgets>

#include <QMessageBox>
#include <QPixmap>

/**
 * @brief Main workspace for the GUI.
 */
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    /**
     * @brief UART*/
    this->uart = new Uart(this);
    /**
     * @brief Get all available COM Ports and store them in a QList.*/
    QList<QextPortInfo> ports = QextSerialEnumerator::getPorts();
    /**
     * @brief Read each element on the list, but
     * add only USB ports to the combo box.*/
    for (int i = 0; i < ports.size(); i++) {
        if (ports.at(i).portName.contains("USB")){
            ui->comboBox_Interface->addItem(ports.at(i).portName.toLocal8Bit().constData());
        }
    }
    /**
     * @brief Show a hint if no USB ports were found.*/
    if (ui->comboBox_Interface->count() == 0){
        ui->textEdit_Status->insertPlainText("No USB ports available.\nConnect a USB device and try again.");
    }

    widget = new GraphWidget;
    for (int i = 0; i<13; ++i){
        nodes.push_back(new Node(widget, this));
    }
    createDockWindows();
}
/**
 * @brief Close the GUI Window.*/
MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}
/**
 * @brief Connection to the Sink mote using USB. \n
 * Once the mote is inserted it is detected and then clicked on open one can see data on the window.*/
void MainWindow::on_pushButton_open_clicked()
{
    port.setQueryMode(QextSerialPort::EventDriven);
    port.setPortName("/dev/" + ui->comboBox_Interface->currentText());
    port.setBaudRate(BAUD115200);
    port.setFlowControl(FLOW_OFF);
    port.setParity(PAR_NONE);
    port.setDataBits(DATA_8);
    port.setStopBits(STOP_1);
    port.open(QIODevice::ReadWrite);

    /**
     * @brief To start the Communication click on open.*/

    if (!port.isOpen())
    {
        error.setText("Unable to open port!");
        error.show();
        return;
    }

    // UART
    QString portname = "/dev/" + ui->comboBox_Interface->currentText();
    uart->open(portname);
    if (!uart->isOpen())
    {
        error.setText("Unable to open UART port!");
        error.show();
        return;
    }

    QObject::connect(&port, SIGNAL(readyRead()), this, SLOT(receive()));

    ui->pushButton_close->setEnabled(true);
    ui->pushButton_open->setEnabled(false);
    ui->comboBox_Interface->setEnabled(false);
}

/**
 * @brief To close the Communication click on close.*/
void MainWindow::on_pushButton_close_clicked()
{
    if (port.isOpen())port.close();
    if (uart->isOpen()) uart->close();
    ui->pushButton_close->setEnabled(false);
    ui->pushButton_open->setEnabled(true);
    ui->comboBox_Interface->setEnabled(true);
}

void MainWindow::on_send_command_button_clicked()
{
    QString data = ui->textEdit_command->toPlainText();
    ui->textEdit_command->clear();
    this->uart_send(data.toUtf8());
}

void MainWindow::uart_send(QByteArray data) {
    uart->send(data);
}

/**
 * @brief Recieve data from sensors and display it on QLCD box*/

void MainWindow::receive(){

    static QString str;
    char ch;
    while (port.getChar(&ch)){
        str.append(ch);
        /**
         * @brief End of line, start decoding */
        if (ch == '\n'){
            str.remove("\n", Qt::CaseSensitive);
            ui->textEdit_Status->append(str);
            ui->textEdit_Status->ensureCursorVisible();


            if(str.contains("DataType:")){
                QStringList list = str.split(QRegExp("\\s"));
                qDebug() << "Received from Serial Link: " << str;

                  //SENSORS
                if(!list.isEmpty()){
                    qDebug() << "List size " << list.size();
                    double soil;
                    int i=0;
                    qDebug() << "List value "<< i <<" "<< list.at(i);
                    switch(list.at(i+1).toInt()){
                    case 2:
                        /**
                         * @brief Details for calculating the temperature */
                        double temperature;
                        temperature = list.at(i+3).toDouble();
                        /**
                         * @brief Adjust the temperature to Degrees */
                       temperature = temperature/1000;
                       printf("%f\n",temperature);
                       if(temperature < 5){
                           QPixmap image(":images/cold.jpg");
                           pop_up.setText("Too cold for your plants");
                           pop_up.setIconPixmap(image);
                           pop_up.show();
                       } else if(temperature > 30){
                           QPixmap image(":images/hot.png");
                           pop_up.setText("Too hot for your plants");
                           pop_up.setIconPixmap(image);
                           pop_up.show();
                       } else {
                           pop_up.hide();
                       }
                       /**
                        * @brief Debugging the temperature and displaying on the QLCD */
                       qDebug() << "Var temperature " << QString::number(temperature);
                       ui->value_temperature->display(temperature);
                        break;

                    case 4:
                        soil = list.at(i+3).toDouble();
                        printf("%f\n",soil);
                        if(soil < 10){
                            QPixmap image(":images/dry_plant.jpg");
                            pop_up.setText("Too dry for your plants");
                            pop_up.setIconPixmap(image);
                            pop_up.show();
                        } else if(soil > 80){
                            QPixmap image(":images/DrowningPlant.png");
                            pop_up.setText("Too wet for your plants");
                            pop_up.setIconPixmap(image);
                            pop_up.show();
                        } else {
                            pop_up.hide();
                        }
                        /**
                         * @brief Debugging the Soil Miosture and displaying on the QLCD */
                        qDebug() << "Var soil " << QString::number(soil);
                        ui->value_soil->display(soil);
                        break;
                    case 6:
                        soil = list.at(i+3).toDouble();
                        printf("%f\n",soil);
                        if(soil < 10){
                            QPixmap image(":images/dry_plant.jpg");
                            pop_up.setText("Too dry for your plants");
                            pop_up.setIconPixmap(image);
                            pop_up.show();
                        } else if(soil > 80){
                            QPixmap image(":images/DrowningPlant.png");
                            pop_up.setText("Too wet for your plants");
                            pop_up.setIconPixmap(image);
                            pop_up.show();
                        } else {
                            pop_up.hide();
                        }
                        /**
                         * @brief Debugging the Soil Miosture and displaying on the QLCD */
                        qDebug() << "Var soil " << QString::number(soil);
                        ui->value_soil->display(soil);
                        break;
                    case 8:
                        double light;
                        light = list.at(i+3).toDouble();
                        printf("%f\n",light);
                        if(light < 40){
                            QPixmap image(":images/night_time.jpg");
                            pop_up.setText("Too dark for your plants");
                            pop_up.setIconPixmap(image);
                            pop_up.show();
                        } else {
                            pop_up.hide();
                        }
                        /**
                         * @brief Debugging the light and displaying on the QLCD */
                        qDebug() << "Var light " << QString::number(light);
                        ui->value_light->display(light);
                        break;
                    case 10:
                        double pH;
                        pH = list.at(i+3).toDouble();
                        printf("%f\n",pH);
                        if(pH < 3){
                            QPixmap image(":images/acidic.jpg");
                            pop_up.setText("Too acidic for your plants");
                            pop_up.setIconPixmap(image);
                            pop_up.show();
                        } else if(pH > 9){
                            QPixmap image(":images/basic.jpg");
                            pop_up.setText("Too basic for your plants");
                            pop_up.setIconPixmap(image);
                            pop_up.show();
                        } else {
                            pop_up.hide();
                        }
                        /**
                         * @brief Debugging the pH Level and displaying on the QLCD */
                        qDebug() << "Var pH " << QString::number(pH);
                        ui->value_pH->display(pH);
                        break;
                    case 12:
                        double humidity;
                        humidity = list.at(i+3).toDouble();
                        printf("%f\n",humidity);
                        /**
                         * @brief Debugging the humidity and displaying on the QLCD */
                        qDebug() << "Var Humidity " << QString::number(humidity);
                        ui->value_humidity->display(humidity);
                        break;
                    }
                }
            }
            // NETWORK TOPOLOGY
            // New Link
            else if(str.contains("NewLink:")){
                int new_link_src;
                int new_link_dest;
                QStringList list = str.split(QRegExp("\\s"));
                QGraphicsScene *scene = widget->scene();

                qDebug() << "Received from Serial Link: " << str;
                if(!list.isEmpty()){
                    qDebug() << "List size " << list.size();
                    for (int i=0; i < list.size(); i++){
                        qDebug() << "List value "<< i <<" "<< list.at(i);
                        if (list.at(i) == "NewLink:") {
                            new_link_src = list.at(i+1).toInt()-1;
                            new_link_dest = list.at(i+3).toInt()-1;
                            printf("%d\n",new_link_src);
                            printf("%d\n",new_link_dest);
                            for(Edge *existing_edge: edges){
                                if((existing_edge->sourceNode() == nodes.at(new_link_src))
                                        && (existing_edge->destNode() == nodes.at(new_link_dest))){
                                    scene->removeItem(existing_edge);
                                }
                            }
                            Edge *edge = new Edge(nodes.at(new_link_src),
                                                       nodes.at(new_link_dest), 0);
                            scene->addItem(edge);
                            edges.push_back(edge);
                        }
                    }
                }
            }
            // Lost Link
            else if(str.contains("LostLink:")){
                int lost_link_src;
                int lost_link_dest;
                QStringList list = str.split(QRegExp("\\s"));
                QGraphicsScene *scene = widget->scene();

                qDebug() << "Received from Serial Link: " << str;
                if(!list.isEmpty()){
                    qDebug() << "List size " << list.size();
                    for (int i=0; i < list.size(); i++){
                        qDebug() << "List value "<< i <<" "<< list.at(i);
                        if (list.at(i) == "LostLink:") {
                            lost_link_src = list.at(i+1).toInt()-1;
                            lost_link_dest = list.at(i+3).toInt()-1;
                            printf("%d\n",lost_link_src);
                            printf("%d\n",lost_link_dest);
                            for(Edge *existing_edge: edges){
                                if((existing_edge->sourceNode() == nodes.at(lost_link_src))
                                        && (existing_edge->destNode() == nodes.at(lost_link_dest))){
                                    scene->removeItem(existing_edge);
                                }
                            }
                            Edge *edge = new Edge(nodes.at(lost_link_src),
                                                       nodes.at(lost_link_dest), 1);
                            scene->addItem(edge);
                            edges.push_back(edge);
                        }
                    }
                }
            }
            // Packet Path
            else if(str.contains("PacketPath:")){
                QStringList list = str.split(QRegExp("\\s"));
                qDebug() << "Received from Serial Link: " << str;
                QGraphicsScene *scene = widget->scene();

//                if(!list.isEmpty() && list!=last_path){
                if(!list.isEmpty()){
                    qDebug() << "List size " << list.size();
                    for(unsigned int i=0; i < last_path.size(); i++){
                        scene->removeItem(last_path.at(i));
                    }
                    last_path.clear();
                    for (int i=1; i < list.size()-2; i+=2){
                        int src = list.at(i).toInt()-1;
                        int dest = list.at(i+2).toInt()-1;
                        Edge* edge = new Edge(nodes.at(src), nodes.at(dest), 2);
                        scene->addItem(edge);
//                        last_path = list;
                        last_path.push_back(edge);
                    }
                }
            }
            this->repaint();    // Update content of window immediately
            str.clear();
        }
    }
}

/*!
 * \brief MainWindow::createDockWindows: Positions of nodes are set in here
 */
void MainWindow::createDockWindows()
{
    QDockWidget *dock = new QDockWidget(tr("Network"), this);
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    QGraphicsScene *scene = widget->scene();

    for (int i = 0; i<13; ++i){
        scene->addItem(nodes.at(i));
    }


    nodes.at(0)->setPos(0, 0);
    nodes.at(1)->setPos(300, -200);
    nodes.at(2)->setPos(100, 0);
    nodes.at(3)->setPos(300, 0);
    nodes.at(4)->setPos(200, -100);

    nodes.at(5)->setPos(300, 200);
    nodes.at(6)->setPos(200, 100);
    nodes.at(7)->setPos(-300, -200);
    nodes.at(8)->setPos(-100, -100);
    nodes.at(9)->setPos(-300, 0);

    nodes.at(10)->setPos(-100, 100);
    nodes.at(11)->setPos(-300, 200);
    nodes.at(12)->setPos(-200, 0);

    dock->setWidget(widget);
    addDockWidget(Qt::RightDockWidgetArea, dock);

}

GraphWidget::GraphWidget(QWidget *parent)
    : QGraphicsView(parent)
{
    QGraphicsScene *scene = new QGraphicsScene(this);
    scene->setItemIndexMethod(QGraphicsScene::NoIndex);
    scene->setSceneRect(-400, -400, 800, 800);
    setScene(scene);
    setCacheMode(CacheBackground);
    setViewportUpdateMode(BoundingRectViewportUpdate);
    setRenderHint(QPainter::Antialiasing);
    setTransformationAnchor(AnchorUnderMouse);
    scale(qreal(0.8), qreal(0.8));
    setMinimumSize(800, 800);
    setWindowTitle(tr("Network Topology"));

}

void GraphWidget::itemMoved()
{
    if (!timerId)
        timerId = startTimer(1000 / 25);
}

void GraphWidget::timerEvent(QTimerEvent *event)
{
    Q_UNUSED(event);

    QVector<Node *> nodes;
    const QList<QGraphicsItem *> items = scene()->items();
    for (QGraphicsItem *item : items) {
        if (Node *node = qgraphicsitem_cast<Node *>(item))
            nodes << node;
    }

    for (Node *node : qAsConst(nodes))
        node->calculateForces();

    bool itemsMoved = false;
    for (Node *node : qAsConst(nodes)) {
        if (node->advancePosition())
            itemsMoved = true;
    }

    if (!itemsMoved) {
        killTimer(timerId);
        timerId = 0;
    }
}

void GraphWidget::drawBackground(QPainter *painter, const QRectF &rect)
{
    Q_UNUSED(rect);

    // Shadow
    QRectF sceneRect = this->sceneRect();
    QRectF rightShadow(sceneRect.right(), sceneRect.top() + 5, 5, sceneRect.height());
    QRectF bottomShadow(sceneRect.left() + 5, sceneRect.bottom(), sceneRect.width(), 5);
    if (rightShadow.intersects(rect) || rightShadow.contains(rect))
        painter->fillRect(rightShadow, Qt::darkGray);
    if (bottomShadow.intersects(rect) || bottomShadow.contains(rect))
        painter->fillRect(bottomShadow, Qt::darkGray);

    // Fill
    QLinearGradient gradient(sceneRect.topLeft(), sceneRect.bottomRight());
    gradient.setColorAt(0, Qt::white);
    gradient.setColorAt(1, Qt::lightGray);
    painter->fillRect(rect.intersected(sceneRect), gradient);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(sceneRect);

}

// NODE
Node::Node(GraphWidget *graphWidget, MainWindow *w)
    : graph(graphWidget)
{
    parentWindow = w;
    setFlag(ItemSendsGeometryChanges);
    setCacheMode(DeviceCoordinateCache);
    setZValue(-1);
}

void Node::addEdge(Edge *edge)
{
    edgeList << edge;
    edge->adjust();
}

QVector<Edge *> Node::edges() const
{
    return edgeList;
}

void Node::calculateForces()
{
    if (!scene() || scene()->mouseGrabberItem() == this) {
        newPos = pos();
        return;
    }

    QRectF sceneRect = scene()->sceneRect();
    newPos = pos(); /*+ QPointF(xvel, yvel);*/
    newPos.setX(qMin(qMax(newPos.x(), sceneRect.left() + 10), sceneRect.right() - 10));
    newPos.setY(qMin(qMax(newPos.y(), sceneRect.top() + 10), sceneRect.bottom() - 10));
}

bool Node::advancePosition()
{
    if (newPos == pos())
        return false;

    setPos(newPos);
    return true;
}

QRectF Node::boundingRect() const
{
    qreal adjust = 2;
    return QRectF( -10 - adjust, -10 - adjust, 23 + adjust, 23 + adjust);
}

QPainterPath Node::shape() const
{
    QPainterPath path;
    path.addEllipse(-10, -10, 20, 20);
    return path;
}

void Node::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setPen(Qt::NoPen);
    painter->setBrush(Qt::darkGray);
    painter->drawEllipse(-7, -7, 20, 20);

    QRadialGradient gradient(-3, -3, 10);

    // Find index of node in the vector nodes, member of MainWindow object
    std::vector<Node*> nodes = parentWindow->nodes;
    std::vector<Node*>::iterator it = std::find(nodes.begin(), nodes.end(), this);
    int index = std::distance(nodes.begin(), it);
    // Decide which colour one node is
    if(index == 0) {
        gradient.setColorAt(0, Qt::red);
        gradient.setColorAt(1, Qt::darkRed);
        painter->setBrush(gradient);
    } else {
        switch(index % 2) {
            case 0: gradient.setColorAt(0, Qt::cyan);
                    gradient.setColorAt(1, Qt::darkCyan);
                    painter->setBrush(gradient);
                    break;
            case 1: gradient.setColorAt(0, Qt::yellow);
                    gradient.setColorAt(1, Qt::darkYellow);
                    painter->setBrush(gradient);
                    break;
        }
    }

    painter->setPen(QPen(Qt::black, 0));
    painter->drawEllipse(-10, -10, 20, 20);
}

QVariant Node::itemChange(GraphicsItemChange change, const QVariant &value)
{
    switch (change) {
    case ItemPositionHasChanged:
        for (Edge *edge : qAsConst(edgeList))
            edge->adjust();
        graph->itemMoved();
        break;
    default:
        break;
    };

    return QGraphicsItem::itemChange(change, value);
}

//EDGE
Edge::Edge(Node *sourceNode, Node *destNode, unsigned int edgeType)
    : source(sourceNode), dest(destNode)
{
    setAcceptedMouseButtons(Qt::NoButton);
    source->addEdge(this);
    dest->addEdge(this);
    edge_type = edgeType;
    adjust();
}

Node *Edge::sourceNode() const
{
    return source;
}

Node *Edge::destNode() const
{
    return dest;
}

void Edge::adjust()
{
    if (!source || !dest)
        return;

    QLineF line(mapFromItem(source, 0, 0), mapFromItem(dest, 0, 0));
    qreal length = line.length();

    prepareGeometryChange();

    if (length > qreal(20.)) {
        QPointF edgeOffset((line.dx() * 10) / length, (line.dy() * 10) / length);
        sourcePoint = line.p1() + edgeOffset;
        destPoint = line.p2() - edgeOffset;
    } else {
        sourcePoint = destPoint = line.p1();
    }
}

QRectF Edge::boundingRect() const
{
    if (!source || !dest)
        return QRectF();

    qreal penWidth = 1;
    qreal extra = (penWidth + arrowSize) / 2.0;

    return QRectF(sourcePoint, QSizeF(destPoint.x() - sourcePoint.x(),
                                      destPoint.y() - sourcePoint.y()))
        .normalized()
        .adjusted(-extra, -extra, extra, extra);
}

void Edge::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    if (!source || !dest)
        return;

    QLineF line(sourcePoint, destPoint);
    if (qFuzzyCompare(line.length(), qreal(0.)))
        return;

    // Draw the line itself
    switch (this->edge_type) {
    case 0: painter->setPen(QPen(Qt::green, 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        break;
    case 1: painter->setPen(QPen(Qt::red, 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        break;
    case 2: painter->setPen(QPen(Qt::blue, 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        break;
    }
    painter->drawLine(line);

    // Draw the arrows
    double angle = std::atan2(-line.dy(), line.dx());

    QPointF destArrowP1 = destPoint + QPointF(sin(angle - M_PI / 3) * arrowSize,
                                              cos(angle - M_PI / 3) * arrowSize);
    QPointF destArrowP2 = destPoint + QPointF(sin(angle - M_PI + M_PI / 3) * arrowSize,
                                              cos(angle - M_PI + M_PI / 3) * arrowSize);

    switch (this->edge_type) {
    case 0: painter->setBrush(Qt::green);
        break;
    case 1: painter->setBrush(Qt::red);
        break;
    case 2: painter->setBrush(Qt::blue);
        break;
    }
    painter->drawPolygon(QPolygonF() << line.p2() << destArrowP1 << destArrowP2);
}
















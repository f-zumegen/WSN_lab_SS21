/**
 @file mainwindow.h
*/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMessageBox>
#include <QGraphicsView>
#include <QGraphicsItem>
#include <QVector>
#include "qextserialport.h"
#include "qextserialenumerator.h"
#include <QtSql>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include "uart.h"

namespace Ui {
    class MainWindow;
}

class GraphWidget;
class Node;
class Edge;

//! MainWindow
/*!
 * \brief The MainWindow class: Contains text field for received communication from the GUI mote,
 * displays for all measured sensor values, a text field to send commands to the GUI mote, and
 * a dynamic graph of the network topology as a docked widget.
 */
class MainWindow : public QMainWindow {
    Q_OBJECT
    friend class Node;

public:
    //! Constructor
    MainWindow(QWidget *parent = 0);
    //! Destructor
    ~MainWindow();

protected:
    //! React to a language change
    void changeEvent(QEvent *e);

private:
    /*!
     * \brief Pointer to the UI designed as a form in Qt
     */
    Ui::MainWindow *ui;
    /*!
     * \brief Used for communication with the Mote via UART
     */
    QextSerialPort port;
    /*!
     * \brief Error message that pops up when no ports avialable
     */
    QMessageBox error;
    /*!
     * \brief pop_up used to give hints to the user on how to react to sensor values
     */
    QMessageBox pop_up;
    /*!
     * \brief widget to display the network topology
     */
    GraphWidget *widget;
    /*!
     * \brief Holds all existing nodes in the network
     */
    std::vector<Node *> nodes;
    /*!
     * \brief uart communication object
     */
    Uart *uart;
    /*!
     * \brief Holds all existing edges in the network
     */
    std::vector<Edge *> edges;
    /*!
     * \brief Holds edges of the last path taken by a data packet (data packet from a sensor mote)
     */
    std::vector<Edge *> last_path;
    /*!
     * \brief Adds the graph widget of the network topology to the MainWindow object
     */
    void createDockWindows();

private slots:
    /*!
     * \brief React on clicking the close button
     */
    void on_pushButton_close_clicked();
    /*!
     * \brief React on clicking the open button
     */
    void on_pushButton_open_clicked();
    /*!
     * \brief React on clicking the Send to Mote button
     */
    void on_send_command_button_clicked();
    /*!
     * \brief React to received communication from the GUI mote.
     * This includes reacting to sensor values and topology changes.
     */
    void receive();
    /*!
     * \brief Handles sending a command to the GUI mote via UART
     * \param data The data to send to the GUI mote
     */
    void uart_send(QByteArray data);
};

class GraphWidget : public QGraphicsView
{
    Q_OBJECT

public:
    //! Constructor
    GraphWidget(QWidget *parent = nullptr);

    /*!
     * \brief Decides on the frequency of how often it is checked if a node or
     * edge has moved
     */
    void itemMoved();

protected:
    /*!
     * \brief When timer expires, nodes and edges are repositioned
     * \param event The event object
     */
    void timerEvent(QTimerEvent *event) override;
    /*!
     * \brief Draw background of the newtork graph
     * \param painter
     * \param rect
     */
    void drawBackground(QPainter *painter, const QRectF &rect) override;

private:
    /*!
     * \brief timerId
     */
    int timerId = 0;
    /*!
     * \brief centerNode
     */
    Node *centerNode;
};

class Node : public QGraphicsItem
{
public:
    /*!
     * \brief Constructor
     * \param graphWidget The graph in which the node exists
     * \param w Window in which the graph exists
     */
    Node(GraphWidget *graphWidget, MainWindow *w);

    /*!
     * \brief addEdge
     * \param edge
     */
    void addEdge(Edge *edge);
    /*!
     * \brief edges
     * \return Returns list of edges of a node
     */
    QVector<Edge *> edges() const;

    enum { Type = UserType + 1 };
    int type() const override { return Type; }

    /*!
     * \brief Triggers redrawing of nodes if needed
     */
    void calculateForces();
    /*!
     * \brief Triggers redrawing of nodes if needed
     */
    bool advancePosition();

    /*!
     * \brief bounding rectangle of node
     * \return
     */
    QRectF boundingRect() const override;
    /*!
     * \brief shape of node
     * \return
     */
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    /*!
     * \brief edgeList
     */
    QVector<Edge *> edgeList;
    /*!
     * \brief New position of node if node was moved
     */
    QPointF newPos;
    /*!
     * \brief Graph of node
     */
    GraphWidget *graph;
    /*!
     * \brief parentWindow of node
     */
    MainWindow *parentWindow;
};

class Edge : public QGraphicsItem
{
public:
    /*!
     * \brief Constructor of Edge
     * \param sourceNode
     * \param destNode
     * \param edgeType Is the edge a new link, lost link or packet path
     */
    Edge(Node *sourceNode, Node *destNode, unsigned int edgeType);

    Node *sourceNode() const;
    Node *destNode() const;

    /*!
     * \brief Adjust the edge if edge was moved
     */
    void adjust();

    enum { Type = UserType + 2 };
    int type() const override { return Type; }

protected:
    /*!
     * \brief bounding rectangle of edge
     * \return
     */
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    Node *source, *dest;

    /*!
     * \brief edge_type: 0 = New Link; 1 = Lost Link; 2 = Packet Path
     */
    unsigned int edge_type;

    QPointF sourcePoint;
    QPointF destPoint;
    qreal arrowSize = 10;
};

#endif // MAINWINDOW_H

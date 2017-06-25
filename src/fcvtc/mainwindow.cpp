// mainwindow.cpp
//

#include <QTimer>
#include <QMessageBox>
#include <QTableWidget>
#include <QDateTime>


#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ltkcpp.h"
#include "creader.h"



MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Configure table


    try {

        // Open connection to reader

        int verbose = 0;
        int readerId = 1;
        readerList.append(new CReader("192.168.1.98", readerId, verbose));
        for (int i=0; i<readerList.size(); i++) {
            connect(readerList[0], SIGNAL(newTag(CTagInfo)), this, SLOT(onNewTag(CTagInfo)));
            connect(readerList[0], SIGNAL(newLogMessage(QString)), this, SLOT(onNewLogMessage(QString)));
        }


        // Start timer that will poll reader

        connect(&readerCheckTimer, SIGNAL(timeout()), this, SLOT(onReaderCheckTimeout()));
        readerCheckTimer.setInterval(1000);
        readerCheckTimer.start();

    }
    catch (QString s) {
        QMessageBox::critical(this, "fcvtc", s);
    }
    catch (...) {
        QMessageBox::critical(this, "fcvtc", "Unexpected exception");
    }
}

MainWindow::~MainWindow()
{
    delete ui;
    for (int i=0; i<readerList.size(); i++) {
        delete readerList[i];
    }
    readerList.clear();
}


void MainWindow::onReaderCheckTimeout(void) {
    readerCheckTimer.stop();
    for (int i=0; i<readerList.size(); i++) {
        readerList[i]->processRecentChipsSeen();
    }
    readerCheckTimer.start();
}


void MainWindow::onNewTag(CTagInfo tagInfo) {
    QString s, s2;
    QTableWidget *t = ui->lapTimesTableWidget;
    static int tableEntryCount = 0;
    static QList<double> previousTime;
    static QList<int> previousTagId;

    if (tagInfo.readerId != 1) return;

    // Current time (nearest second, not used for timing)

    QString time = QTime::currentTime().toString("hh:mm:ss");


    // Add string to console window

    s.sprintf("readerId=%d antennaId=%d timeStampSec=%lf", tagInfo.readerId, tagInfo.antennaId, tagInfo.timeStampSec);
    for (int i=0; i<tagInfo.data.size(); i++) {
        if (i == 0) s.append(s2.sprintf(" data=%02x", tagInfo.data[i]));
        else s.append(s2.sprintf(" %02x", tagInfo.data[i]));
    }
    ui->messageConsoleTextEdit->insertHtml(s + "<br>");


    // If necessary, increase size of table to accomodate new entry

    int r = tableEntryCount;
    if (tableEntryCount >= t->rowCount()) {
        for (int i=0; i<10; i++) {
            t->insertRow(tableEntryCount + i);
        }
    }


    // tagId

    int tagId = tagInfo.data[4] << 8 | tagInfo.data[5];

    // Get name from dBase for tagId

    QString name;
    if (tableEntryCount % 2) {
        name = "Chris";
    }
    else {
        name = "Peter";
        tagId++;
    }


    // Check to see if this tag is in previousTagId.  If it is, compute lap time.  If lap time is greater
    // than some min value, assume this is first lap after a rest so no lap time.
    // If not in list, assume first lap so add to list but no lap time.

    double lapTime = 0.;
    int index = previousTagId.indexOf(tagId);
    if (index >= 0) {
        lapTime = tagInfo.timeStampSec - previousTime[index];
        previousTime[index] = tagInfo.timeStampSec;
    }
    else {
        previousTagId.append(tagId);
        previousTime.append(tagInfo.timeStampSec);
        lapTime = 0.;
    }
    double minLapTime = 15.;
    if (lapTime > minLapTime) lapTime = 0.;


    // Add new entry to bottom of table

    QTableWidgetItem *item = new QTableWidgetItem();
    item->setData(Qt::EditRole, tagId);
    t->setItem(r, 0, item);
    t->setItem(r, 1, new QTableWidgetItem());
    t->item(r, 1)->setText(name);
    t->setItem(r, 2, new QTableWidgetItem());
    t->item(r, 2)->setText(time);
    if (lapTime > 0.) {
        item = new QTableWidgetItem();
        item->setData(Qt::EditRole, lapTime);
        t->setItem(r, 3, item);
    }

    tableEntryCount++;

}


void MainWindow::onNewLogMessage(QString s) {
    ui->messageConsoleTextEdit->insertHtml(s + "<br>");
}

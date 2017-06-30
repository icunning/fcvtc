// mainwindow.cpp
//

#include <QTimer>
#include <QMessageBox>
#include <QTableWidget>
#include <QDateTime>


#include "mainwindow.h"
#include "ui_mainwindow.h"
//#include "ltkcpp.h"
#include "creader.h"


extern double blackLineDistancem;


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Configure table

    QTableWidget *t = ui->lapTimesTableWidget;
    t->setColumnWidth(0, 250);
    t->setColumnWidth(1, 100);
    t->setColumnWidth(2, 150);
    t->setColumnWidth(3, 200);


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

    // Get name from dBase

    QString name;
    switch (tagId) {
    case 0:
        name = "Chris";
        break;
    case 1:
        name = "Peter";
        break;
    case 2:
        name = "Sue";
        break;
    case 3:
        name = "Cindy";
        break;
    }

    // If name not found, use tagId

    if (name.isEmpty()) name.setNum(tagId, 16);


    // Check to see if this tag is in previousTagId.  If it is, compute lap time.  If lap time is greater
    // than some min value, assume this is first lap after a rest so no lap time.
    // If not in list, assume first lap so add to list but no lap time.

    double lapSec = 0.;
    int index = previousTagId.indexOf(tagId);
    if (index >= 0) {
        lapSec = tagInfo.timeStampSec - previousTime[index];
        previousTime[index] = tagInfo.timeStampSec;
    }
    else {
        previousTagId.append(tagId);
        previousTime.append(tagInfo.timeStampSec);
        lapSec = 0.;
    }

    // If lap time is greater than 120 sec, rider must have taken a break so do not
    // calculate lap time

    double maxLapSec = 120.;
    if (lapSec > maxLapSec) lapSec = 0.;

    // Lap speed around black line

    double lapSpeed = 0.;
    if (lapSec > 0.) {
        lapSpeed = blackLineDistancem / (lapSec / 3600.) / 1000.;
    }

    // Add new entry to bottom of table

    QTableWidgetItem *item;
    t->setItem(r, 0, new QTableWidgetItem());
    t->item(r, 0)->setText(name);
    t->setItem(r, 1, new QTableWidgetItem());
    t->item(r, 1)->setText(time);
    if (lapSec > 0.) {
        item = new QTableWidgetItem();
        item->setData(Qt::EditRole, lapSec);
        t->setItem(r, 2, item);
        item = new QTableWidgetItem();
        item->setData(Qt::EditRole, lapSpeed);
        t->setItem(r, 3, item);
    }

    tableEntryCount++;
}


void MainWindow::onNewLogMessage(QString s) {
    ui->messageConsoleTextEdit->insertHtml(s + "<br>");
}

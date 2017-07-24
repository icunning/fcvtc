// mainwindow.cpp
//

#include <QTimer>
#include <QMessageBox>
#include <QTableWidget>
#include <QDateTime>
#include <QScrollBar>
#include <QThread>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QTableView>

#include <stdio.h>
#include <unistd.h>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ltkcpp.h"
#include "creader.h"



MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("FCV Lap System");

    initialTimeStampUSec = 0;

    blackLineDistancem = 138.;
    lapsTableTimeStampMaxAgeMin = 2;            // Start removing entries from lapsTable when this old (minutes)
    activeRidersTableTimeStampMaxAgeMin = 2;    // Start removing riders from activeRidersTable when this old (minutes)
    activeRidersTablePurgeIntervalSec = 10;     // interval between purge actions on activeRidersTable and activeRidersList


    // Create 1-sec timer for panel dateTime and possibly other things

    ui->dateTimeLabel->clear();
    connect(&clockTimer, SIGNAL(timeout()), this, SLOT(onClockTimerTimeout()));
    clockTimer.setInterval(1000);
    clockTimer.start();


    // Configure messages console

    QPlainTextEdit *m = ui->messagesPlainTextEdit;
    m->setReadOnly(true);

    // Configure laps table

    QTableWidget *t = ui->lapsTableWidget;
    t->setColumnWidth(0, 250);  // name
    t->setColumnWidth(1, 100);  // time
    t->setColumnWidth(2, 150);  // timeStamp
    t->setColumnWidth(3, 100);  // lapTime
    t->setSortingEnabled(true);
    t->setEnabled(false);


    // Configure riders table

    t = ui->activeRidersTableWidget;
    t->setColumnWidth(0, 250);  // name
    t->setColumnWidth(1, 100);  // time
    t->setColumnWidth(2, 100);  // lapCount
    t->setColumnWidth(3, 100);  // bestLapTime;
    t->setColumnWidth(4, 100);  // averageLapTime;
    t->setSortingEnabled(false);
    t->setEnabled(false);


    // Create a list of reader hostnames.  Leave list empty for test mode.

    QList<QString> readerHostnameList;
    //readerHostnameList.append("192.168.1.98");


    // If list is empty, add empty hostname for test mode

    if (readerHostnameList.isEmpty()) readerHostnameList.append("");


    // Create new CReader class for each reader and move each to new thread

    for (int i=0; i<readerHostnameList.size(); i++) {
        CReader *reader = new CReader(readerHostnameList[i], i+1);
        connect(reader, SIGNAL(newLogMessage(QString)), this, SLOT(onNewLogMessage(QString)));
        connect(reader, SIGNAL(connected(int)), this, SLOT(onReaderConnected(int)));
        connect(reader, SIGNAL(newTag(CTagInfo)), this, SLOT(onNewTag(CTagInfo)));

        QThread *readerThread = new QThread(this);
        reader->moveToThread(readerThread);
        connect(readerThread, SIGNAL(started(void)), reader, SLOT(onStarted(void)));
        //connect(reader, SIGNAL(finished()), reader, SLOT(deleteLater()));
        //connect(readerThread, SIGNAL(finished(void)), readerThread, SLOT(deleteLater(void)));
        readerThread->start();
        readerList.append(reader);
    }


    // Start timer that will purge old riders from activeRidersTable

    connect(&purgeActiveRidersListTimer, SIGNAL(timeout(void)), this, SLOT(onPurgeActiveRidersList(void)));
    purgeActiveRidersListTimer.setInterval(activeRidersTablePurgeIntervalSec * 1000);
    purgeActiveRidersListTimer.start();
}



MainWindow::~MainWindow()
{
    delete ui;
    for (int i=0; i<readerList.size(); i++) {
        delete readerList[i];
    }
    readerList.clear();
}



void MainWindow::onClockTimerTimeout(void) {
    ui->dateTimeLabel->setText(QDateTime::currentDateTime().toString("ddd MMMM d yyyy  hh:mm:ss"));
}



// onPurgeRidersList
// Loop through riders in activeRidersList and remove any that are getting old.
// Remove from activeRidersTable also.
// Then loop through lapsTable and remove old entries.
//
void MainWindow::onPurgeActiveRidersList(void) {
    QDateTime currentDateTime(QDateTime::currentDateTime());

    activeRidersTableMutex.lock();
    for (int i=activeRidersList.size()-1; i>=0; i--) {
        long long inactiveSec =  activeRidersList[i].mostRecentDateTime.secsTo(currentDateTime);
        if ((inactiveSec / 60) >= activeRidersTableTimeStampMaxAgeMin) {
            activeRidersList.removeAt(i);
            ui->activeRidersTableWidget->removeRow(i);
        }
    }
    activeRidersTableMutex.unlock();

    // Loop through lapsTable and remove any entries where the timeStamp is older than threshold

    lapsTableMutex.lock();
    ui->lapsTableWidget->setSortingEnabled(false);
    unsigned long long currentLocalMSecFromEpoch = currentDateTime.toMSecsSinceEpoch();
    for (int i=ui->lapsTableWidget->rowCount()-1; i>=0; i--) {
        unsigned long long timeStampUSec = ui->lapsTableWidget->item(i, 2)->text().toULongLong();
        long long timeStampAgeSec = (currentLocalMSecFromEpoch - initialLocalMSecFromEpoch - (timeStampUSec - initialTimeStampUSec) / 1000) / 1000;
        if ((timeStampAgeSec / 60) >= activeRidersTableTimeStampMaxAgeMin) {
            ui->lapsTableWidget->removeRow(i);
        }
    }
    lapsTableMutex.unlock();
    ui->lapsTableWidget->setSortingEnabled(true);
}



void MainWindow::onReaderConnected(int readerId) {
    QString s;
    onNewLogMessage(s.sprintf("Connected to reader %d", readerId));

    // Populate comboBox with available power settings for each reader (WIP)

    for (int i=0; i<readerList[readerId-1]->getTransmitPowerList()->size(); i++) {
        printf("INFO: Reader %d Power index %d, power %d\n", readerId, i, readerList[readerId-1]->getTransmitPowerList()->at(i));
        fflush(stdout);
    }

    ui->lapsTableWidget->setEnabled(true);
    ui->activeRidersTableWidget->setEnabled(true);
}



void MainWindow::onNewTag(CTagInfo tagInfo) {
    QString s;
    QTableWidget *t = ui->lapsTableWidget;
    static int tagCount = 0;

    tagCount++;

    // Current time (nearest second, used for display, not timing)

    QDateTime currentDateTime(QDateTime::currentDateTime());
    QString time = currentDateTime.toString("hh:mm:ss");

    // Keep track of timeStamp and time of first tag to be used to assess age of tag

    if (tagCount == 1) {
        initialTimeStampUSec = tagInfo.timeStampUSec;
        initialLocalMSecFromEpoch = QDateTime::currentMSecsSinceEpoch();
    }

    // Add string to messages window

    QPlainTextEdit *m = ui->messagesPlainTextEdit;
    s.sprintf("readerId=%d antennaId=%d timeStampUSec=%llu data=%s", tagInfo.readerId, tagInfo.antennaId, tagInfo.timeStampUSec, tagInfo.tagId.toLatin1().data());
    bool scrollToBottomRequired = false;
    if (m->verticalScrollBar()->sliderPosition() == m->verticalScrollBar()->maximum()) {
        scrollToBottomRequired = true;
    }
    m->appendPlainText(s);
    if (scrollToBottomRequired) {
        m->verticalScrollBar()->setValue(m->verticalScrollBar()->maximum());
    }


    // Turn off table sorting and lock mutex while we update tables

    ui->lapsTableWidget->setSortingEnabled(false);
    lapsTableMutex.lock();
    activeRidersTableMutex.lock();


    // Check to see if tag is in the ridersList and get index if it is

    int activeRidersListIndex = -1;
    for (int i=0; i<activeRidersList.size(); i++) {
        if (tagInfo.tagId == activeRidersList[i].tagId) {
            activeRidersListIndex = i;
            break;
        }
    }


    // Get name corresponding to tagId.  If in ridersList, use that.  Otherwise ask dBase.
    // Default to tagId.

    QString name;
    if (activeRidersListIndex >= 0) {
        name = activeRidersList[activeRidersListIndex].name;
    }
    else {
        bool ok;
        unsigned long long id = tagInfo.tagId.toULongLong(&ok, 16);
        name.clear();
        switch (id & 0x0000ffff) {
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
        case 4:
            name = "Alan";
            break;
        }
    }
    if (name.isEmpty()) name = tagInfo.tagId;

    // Determine lap time

    double lapSec = 0.;
    if (activeRidersListIndex >= 0) {
        lapSec = (double)(tagInfo.timeStampUSec - activeRidersList[activeRidersListIndex].previousTimeStampUSec) / 1.e6;
        activeRidersList[activeRidersListIndex].previousTimeStampUSec = tagInfo.timeStampUSec;
    }

    // If lap time is greater than 120 sec, rider must have taken a break so do not
    // calculate lap time

    double maxLapSec = 120.;
    if (lapSec > maxLapSec) lapSec = 0.;

    // Increase size of lapsTable and add new entry

    t = ui->lapsTableWidget;
    int r = t->rowCount();
    scrollToBottomRequired = false;
    if (t->verticalScrollBar()->sliderPosition() == t->verticalScrollBar()->maximum()) {
        scrollToBottomRequired = true;
    }
    t->insertRow(r);
    t->setRowHeight(r, 24);
    if (scrollToBottomRequired) {
        t->scrollToBottom();
    }

    // Add entry to new row

    QTableWidgetItem *item;
    t->setItem(r, 0, new QTableWidgetItem());
    t->item(r, 0)->setText(name);

    t->setItem(r, 1, new QTableWidgetItem());
    t->item(r, 1)->setText(time);

    t->setItem(r,2, new QTableWidgetItem());
    t->item(r, 2)->setText(s.setNum(tagInfo.timeStampUSec));

    if (lapSec > 0.) {
        item = new QTableWidgetItem();
        item->setData(Qt::EditRole, lapSec);
        t->setItem(r, 3, item);

        double lapSpeed = 0.;
        if (lapSec > 0.) {
            lapSpeed = blackLineDistancem / (lapSec / 3600.) / 1000.;
            item = new QTableWidgetItem();
            item->setData(Qt::EditRole, lapSpeed);
            t->setItem(r, 4, item);
        }
    }


    // If rider was not currently in activeRidersList, add to list and also activeRidersTable.

    // Add rider to activeRidersList if not in list

    if (activeRidersListIndex < 0) {
        rider_t rider;
        rider.name = name;
        rider.tagId = tagInfo.tagId;
        rider.previousTimeStampUSec = tagInfo.timeStampUSec;
        rider.mostRecentDateTime = currentDateTime;
        rider.lapCount = 0;
        rider.bestLapTimeSec = 1.e10;
        rider.lapTimeSumSec = 0.;
        activeRidersList.append(rider);
        activeRidersListIndex = activeRidersList.size();

        t = ui->activeRidersTableWidget;
        bool scrollToBottomRequired = false;
        if (t->verticalScrollBar()->sliderPosition() == t->verticalScrollBar()->maximum()) {
            scrollToBottomRequired = true;
        }
        activeRidersListIndex = t->rowCount();
        t->insertRow(activeRidersListIndex);
        t->setRowHeight(activeRidersListIndex, 24);
        if (scrollToBottomRequired) {
            t->scrollToBottom();
        }

        // First column is name

        QTableWidgetItem *item = new QTableWidgetItem;
        item->setText(name);
        t->setItem(activeRidersListIndex, 0, item);

        // Second column is current time

        item = new QTableWidgetItem;
        item->setText(time);
        t->setItem(activeRidersListIndex, 1, item);

        // Third column is lap count

        item = new QTableWidgetItem;
        item->setData(Qt::EditRole, 0);
        t->setItem(activeRidersListIndex, 2, item);

        // Fourth column is best lap time

        item = new QTableWidgetItem;
        item->setData(Qt::EditRole, 0.);
        t->setItem(activeRidersListIndex, 3, item);

        // Fifth column is average lap time

        item = new QTableWidgetItem;
        item->setData(Qt::EditRole, 0.);
        t->setItem(activeRidersListIndex, 4, item);

    }


    // Update rider time etc

    if (lapSec > 0.) {  // only include complete laps
        activeRidersList[activeRidersListIndex].lapCount += 1;
        activeRidersList[activeRidersListIndex].lapTimeSumSec += lapSec;
    }
    if (lapSec < activeRidersList[activeRidersListIndex].bestLapTimeSec) activeRidersList[activeRidersListIndex].bestLapTimeSec = lapSec;

    ui->activeRidersTableWidget->item(activeRidersListIndex, 1)->setText(time);
    ui->activeRidersTableWidget->item(activeRidersListIndex, 2)->setData(Qt::EditRole, activeRidersList[activeRidersListIndex].lapCount);

    if (activeRidersList[activeRidersListIndex].lapCount >= 1) {
        ui->activeRidersTableWidget->item(activeRidersListIndex, 3)->setData(Qt::EditRole, activeRidersList[activeRidersListIndex].bestLapTimeSec);
        ui->activeRidersTableWidget->item(activeRidersListIndex, 4)->setData(Qt::EditRole, activeRidersList[activeRidersListIndex].lapTimeSumSec / (float)activeRidersList[activeRidersListIndex].lapCount);
    }

    // Re-enable sorting only if lapsTable is not really large

    if (ui->lapsTableWidget->rowCount() < 10000) {
        ui->lapsTableWidget->setSortingEnabled(true);
    }

    // Unlock tables mutex

    lapsTableMutex.unlock();
    activeRidersTableMutex.unlock();


    // lapCount is total laps all riders

    ui->lapCountLineEdit->setText(s.setNum(tagCount));
    ui->riderCountLineEdit->setText(s.setNum(ui->activeRidersTableWidget->rowCount()));

}


void MainWindow::onNewLogMessage(QString s) {
    QPlainTextEdit *m = ui->messagesPlainTextEdit;
    bool scrollToBottomRequired = false;
    if (m->verticalScrollBar()->sliderPosition() == m->verticalScrollBar()->maximum()) {
        scrollToBottomRequired = true;
    }
    m->appendPlainText(s);
    if (scrollToBottomRequired) {
        m->verticalScrollBar()->setValue(m->verticalScrollBar()->maximum());
    }
}

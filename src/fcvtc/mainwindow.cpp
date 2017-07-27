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
#include <QToolBox>

#include <stdio.h>
#include <unistd.h>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ltkcpp.h"
#include "creader.h"


#define LT_NAME             0
#define LT_DATETIME         1
#define LT_TIMESTAMP        2
#define LT_LAPTIME          3
#define LT_LAPSPEED         4

#define AT_NAME             0
#define AT_DATETIME         1
#define AT_LAPCOUNT         2
#define AT_BESTLAPTIME      3
#define AT_AVERAGELAPTIME   4




MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);


    // Configuration variables (read from config file for general use?)

    QString trackName = "Forest City Velodrome";
    blackLineDistancem = 138.;
    maxLapSec = 120.;                               // max time allowable for lap.  If greater, rider must have left and return to track
    lapsTableTimeStampMaxAgeSec = 1 * 60;           // Start removing entries from lapsTable that are this age (minutes)
    activeRidersTableTimeStampMaxAgeSec = 1 * 60;   // Start removing riders from activeRidersTable that are this age (minutes)
    activeRidersTablePurgeIntervalSec = 1 * 60;     // interval between purge of tables for stale entries

    activeRidersSortingEnabled = true;
    lapsTableSortingEnabled = true;


    // Initialize

    initialTimeStampUSec = 0;
    initialSinceEpochMSec = 0;


    // Create 1-sec timer for panel dateTime and possibly other things

    ui->dateTimeLabel->clear();
    connect(&clockTimer, SIGNAL(timeout()), this, SLOT(onClockTimerTimeout()));
    clockTimer.setInterval(1000);
    clockTimer.start();


    // Configure gui

    ui->trackNameLabel->setText(trackName + " LLRPLaps");
    setWindowTitle("LLRPLaps: " + trackName);


    // Configure messages console

    QPlainTextEdit *m = ui->messagesPlainTextEdit;
    m->setReadOnly(true);


    // Configure laps table

    QTableWidget *t = ui->lapsTableWidget;
    t->setColumnWidth(LT_NAME, 200);  // name
    t->setColumnWidth(LT_DATETIME, 80);  // time
    t->setColumnWidth(LT_TIMESTAMP, 120);  // timeStamp
    t->setColumnWidth(LT_LAPTIME, 80);  // lapTime
    t->setSortingEnabled(lapsTableSortingEnabled);
    t->setEnabled(false);


    // Configure riders table

    t = ui->activeRidersTableWidget;
    t->setColumnWidth(AT_NAME, 200);  // name
    t->setColumnWidth(AT_DATETIME, 80);  // time
    t->setColumnWidth(AT_LAPCOUNT, 70);  // lapCount
    t->setColumnWidth(AT_BESTLAPTIME, 100);  // bestLapTime;
    t->setColumnWidth(AT_AVERAGELAPTIME, 100);  // averageLapTime;
    t->setSortingEnabled(activeRidersSortingEnabled);
    t->setEnabled(false);


    // Create a list of hostnames of track readers.  Do not include readers used to read/write tag information.
    // Leave list empty for test mode.

    QList<QString> readerHostnameList;
    //readerHostnameList.append("192.168.1.98");


    // readerHostnameList must have at least one entry, even if empty string

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


    // Configure antenna power comboBoxes (enabled when reader connects)

    ui->antenna1ComboBox->setEnabled(false);
    ui->antenna2ComboBox->setEnabled(false);
    ui->antenna3ComboBox->setEnabled(false);
    ui->antenna4ComboBox->setEnabled(false);


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
    ui->activeRidersTableWidget->setSortingEnabled(false);

    unsigned long long currentSinceEpochMSec = currentDateTime.toMSecsSinceEpoch();

    // Loop through all active riders and see which are geting old

    for (int i=activeRidersList.size()-1; i>=0; i--) {
        unsigned long long timeStampUSec = activeRidersList[i].previousTimeStampUSec;
        long long timeStampSinceInitialMSec = (timeStampUSec - initialTimeStampUSec) / 1000;
        long long currentSinceInitialMSec = currentSinceEpochMSec - initialSinceEpochMSec;
        long long inactiveSec = (currentSinceInitialMSec - timeStampSinceInitialMSec) / 1000;
        if (inactiveSec >= activeRidersTableTimeStampMaxAgeSec) {
            bool riderRemoved = false;
            QString nameToRemove = activeRidersList[i].name;
            for (int j=ui->activeRidersTableWidget->rowCount()-1; j>=0; j--) {
                if (ui->activeRidersTableWidget->item(j, AT_NAME)->text() == nameToRemove) {
                    activeRidersList.removeAt(i);
                    ui->activeRidersTableWidget->removeRow(j);
                    riderRemoved = true;
                    break;
                }
            }
            if (!riderRemoved) {
                printf("Rider not found in activeRidersTable in onPurgeActiveRidersList\n");
                fflush(stdout);
            }
        }
    }

    activeRidersTableMutex.unlock();
    ui->activeRidersTableWidget->setSortingEnabled(activeRidersSortingEnabled);

    // Loop through lapsTable and remove any entries where the timeStamp is older than threshold

    lapsTableMutex.lock();
    ui->lapsTableWidget->setSortingEnabled(false);
    bool scrollToBottomRequired = false;
    if (ui->lapsTableWidget->verticalScrollBar()->sliderPosition() == ui->lapsTableWidget->verticalScrollBar()->maximum()) {
        scrollToBottomRequired = true;
    }
    for (int i=ui->lapsTableWidget->rowCount()-1; i>=0; i--) {  // ignore last entry
        unsigned long long timeStampUSec = ui->lapsTableWidget->item(i, LT_TIMESTAMP)->text().toULongLong();
        long long timeStampSinceInitialMSec = (timeStampUSec - initialTimeStampUSec) / 1000;
        long long currentSinceInitialMSec = currentSinceEpochMSec - initialSinceEpochMSec;
        long long timeStampAgeSec = (currentSinceInitialMSec - timeStampSinceInitialMSec) / 1000;
        if (timeStampAgeSec < 0) timeStampAgeSec = 0;   // required because???
        if (timeStampAgeSec >= lapsTableTimeStampMaxAgeSec) {
            ui->lapsTableWidget->removeRow(i);
        }
    }
    if (scrollToBottomRequired) {
        ui->lapsTableWidget->scrollToBottom();
    }
    lapsTableMutex.unlock();
    ui->lapsTableWidget->setSortingEnabled(lapsTableSortingEnabled);
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
    QTableWidget *t = NULL;
    static int tagCount = 0;

    tagCount++;

    // Keep track of time and timeStamp of first tag to be used to assess age of tag

    if (tagCount == 1) {
        initialTimeStampUSec = tagInfo.timeStampUSec;
        initialSinceEpochMSec = QDateTime::currentMSecsSinceEpoch();
    }

    // Current time (nearest second, used for display, not timing)

    QDateTime currentDateTime(QDateTime::currentDateTime());
    QString time = currentDateTime.toString("hh:mm:ss");

    // Add string to messages window

    onNewLogMessage(s.sprintf("readerId=%d antennaId=%d timeStampUSec=%llu data=%s", tagInfo.readerId, tagInfo.antennaId, tagInfo.timeStampUSec, tagInfo.tagId.toLatin1().data()));

    // Turn off table sorting and lock mutex while we update tables

    lapsTableMutex.lock();
    activeRidersTableMutex.lock();
    ui->lapsTableWidget->setSortingEnabled(false);
    ui->activeRidersTableWidget->setSortingEnabled(false);


    // activeRidersList is the main list containing information from all active riders.  Use it for all calcualtions
    // and then put information to be displayed into activeRidersTable and/or lapsTable.

    // Check to see if tag is in the activeRidersList and get index if it is

    int activeRidersListIndex = -1;
    for (int i=0; i<activeRidersList.size(); i++) {
        if (tagInfo.tagId == activeRidersList[i].tagId) {
            activeRidersListIndex = i;
            break;
        }
    }

    // If not in list, set newActiveRider

    bool newActiveRider = false;
    if (activeRidersListIndex < 0) newActiveRider = true;


    // Get name corresponding to tagId.  If in ridersList, use that.  Otherwise ask dBase.
    // Default to tagId if name not found.

    QString name;
    if (activeRidersListIndex >= 0) {

        // Not a new rider, so get name from activeRidersList

        name = activeRidersList[activeRidersListIndex].name;
    }
    else {

        // New rider, so get from dBase

        bool ok;
        unsigned long long id = tagInfo.tagId.toULongLong(&ok, 16);
        name.clear();
        switch (id & 0x0000ffff) {
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
        if (name.isEmpty()) name = tagInfo.tagId;
    }


    // Set rider to point to entry in activeRidersList

    rider_t *rider = NULL;

    // If new rider, add new entry to activeRidersList

    if (newActiveRider) {
        rider = new rider_t;
        rider->name = name;
        rider->tagId = tagInfo.tagId;
        rider->lapCount = 0;
        rider->lapTimeSec = 0.;
        rider->bestLapTimeSec = 1.e10;
        rider->lapTimeSumSec = 0.;
        rider->previousTimeStampUSec = 0;
        rider->mostRecentDateTime = currentDateTime;
        activeRidersList.append(*rider);
        activeRidersListIndex = activeRidersList.size() - 1;
        rider = &activeRidersList[activeRidersListIndex];
    }

    // else we have completed at least one full lap so calculate lap count and lap time

    else {
        rider = &activeRidersList[activeRidersListIndex];
        double lapSec = (double)(tagInfo.timeStampUSec - rider->previousTimeStampUSec) / 1.e6;

        // If lap time is greater than 120 sec, rider must have taken a break so do not
        // calculate lap time

        if (lapSec > maxLapSec) {
            rider->lapTimeSec = 0.;
            rider->previousTimeStampUSec = tagInfo.timeStampUSec;
            rider->mostRecentDateTime = currentDateTime;
        }
        else {
            rider->lapCount++;
            rider->lapTimeSec = lapSec;
            rider->previousTimeStampUSec = tagInfo.timeStampUSec;
            rider->mostRecentDateTime = currentDateTime;
            if (rider->lapTimeSec < rider->bestLapTimeSec) rider->bestLapTimeSec = rider->lapTimeSec;
            rider->lapTimeSumSec += rider->lapTimeSec;
        }
    }


    // Append new entry to lapsTable

    t = ui->lapsTableWidget;
    int r = t->rowCount();
    bool scrollToBottomRequired = false;
    if (t->verticalScrollBar()->sliderPosition() == t->verticalScrollBar()->maximum()) {
        scrollToBottomRequired = true;
    }
    t->insertRow(r);
    t->setRowHeight(r, 24);
    if (scrollToBottomRequired) {
        t->scrollToBottom();
    }

    // Create new lapsTable entries

    t->setItem(r, LT_NAME, new QTableWidgetItem());
    t->item(r, LT_NAME)->setText(name);

    t->setItem(r, LT_DATETIME, new QTableWidgetItem());
    t->item(r, LT_DATETIME)->setText(time);
    t->item(r, LT_DATETIME)->setTextAlignment(Qt::AlignLeft);

    QTableWidgetItem *item = new QTableWidgetItem;
    item->setData(Qt::DisplayRole, tagInfo.timeStampUSec);
    t->setItem(r, LT_TIMESTAMP, item);
    t->item(r, LT_TIMESTAMP)->setTextAlignment(Qt::AlignHCenter);

    if (!newActiveRider) {
        item = new QTableWidgetItem;
        item->setData(Qt::DisplayRole, rider->lapTimeSec);
        t->setItem(r, LT_LAPTIME, item);
        t->item(r, LT_LAPTIME)->setTextAlignment(Qt::AlignHCenter);

        if (rider->lapTimeSec > 0.) {
            double lapSpeed = blackLineDistancem / (rider->lapTimeSec / 3600.) / 1000.;
            item = new QTableWidgetItem;
            item->setData(Qt::DisplayRole, lapSpeed);
            t->setItem(r, LT_LAPSPEED, item);
            t->item(r, LT_LAPSPEED)->setTextAlignment(Qt::AlignHCenter);
        }
    }


    // Populate activeRidersTable entries
    // If new active rider, append new blank line to activeRidersTable and set activeRidersTableIndex

    int activeRidersTableIndex = -1;
    if (newActiveRider) {
        t = ui->activeRidersTableWidget;
        int r = t->rowCount();
        bool scrollToBottomRequired = false;
        if (t->verticalScrollBar()->sliderPosition() == t->verticalScrollBar()->maximum()) {
            scrollToBottomRequired = true;
        }
        t->insertRow(r);
        activeRidersTableIndex = r;
        t->setRowHeight(activeRidersTableIndex, 24);
        if (scrollToBottomRequired) {
            t->scrollToBottom();
        }

        t->setItem(r, AT_NAME, new QTableWidgetItem());
        t->item(r, AT_NAME)->setText(rider->name);
        t->item(r, AT_NAME)->setTextAlignment(Qt::AlignLeft);

        QTableWidgetItem *item = new QTableWidgetItem;
        item->setData(Qt::DisplayRole, 0);
        t->setItem(r, AT_LAPCOUNT, item);
        t->item(r, AT_LAPCOUNT)->setTextAlignment(Qt::AlignHCenter);

        item = new QTableWidgetItem;
        item->setData(Qt::DisplayRole, 0.);
        t->setItem(r, AT_BESTLAPTIME, item);
        t->item(r, AT_BESTLAPTIME)->setTextAlignment(Qt::AlignHCenter);

        item = new QTableWidgetItem;
        item->setData(Qt::DisplayRole, 0.);
        t->setItem(r, AT_AVERAGELAPTIME, item);
        t->item(r, AT_AVERAGELAPTIME)->setTextAlignment(Qt::AlignHCenter);
    }

    // Otherwise get activeRidersTableIndex corresponding to this rider already in table

    else {
        for (int i=0; i<ui->activeRidersTableWidget->rowCount(); i++) {
            if (ui->activeRidersTableWidget->item(i, AT_NAME)->text() == rider->name) {
                activeRidersTableIndex = i;
                break;
            }
        }
    }

    // If activeRidersTableIndex is still < 0 (should never happen), append blank entry with name ???

    if (activeRidersTableIndex < 0) {
        printf("ActiveRidersTableIndex=%d\n", activeRidersTableIndex);
        fflush(stdout);
        t = ui->activeRidersTableWidget;
        int r = t->rowCount();
        bool scrollToBottomRequired = false;
        if (t->verticalScrollBar()->sliderPosition() == t->verticalScrollBar()->maximum()) {
            scrollToBottomRequired = true;
        }
        t->insertRow(r);
        t->setRowHeight(r, 24);
        if (scrollToBottomRequired) {
            t->scrollToBottom();
        }
        t->setItem(r, AT_NAME, new QTableWidgetItem());
        t->item(r, AT_NAME)->setText("????");

        t->setItem(r, AT_LAPCOUNT, new QTableWidgetItem());
        t->setItem(r, AT_BESTLAPTIME, new QTableWidgetItem());
        t->setItem(r, AT_AVERAGELAPTIME, new QTableWidgetItem());
    }

    // activeRidersListIndex now points to new entry in activeRidersList and
    // activeRidersTableIndex points to entry in activeRidersTable


    // Update activeRidersTable entries

    if (activeRidersTableIndex >= 0) {
        t = ui->activeRidersTableWidget;
        int r = activeRidersTableIndex;

        // Second column is current time

        t->setItem(r, AT_DATETIME, new QTableWidgetItem);
        t->item(r, AT_DATETIME)->setText(rider->mostRecentDateTime.toString("hh:mm:ss"));

        // Third column is lap count

        if (!newActiveRider) {
            t->item(r, AT_LAPCOUNT)->setData(Qt::DisplayRole, rider->lapCount);
        }

        // Fourth column is best lap time

        if (!newActiveRider) {
            t->item(r, AT_BESTLAPTIME)->setData(Qt::DisplayRole, rider->bestLapTimeSec);
        }

        // Fifth column is average lap time

        if (!newActiveRider) {
            t->item(r, AT_AVERAGELAPTIME)->setData(Qt::DisplayRole, rider->lapTimeSumSec / (float)rider->lapCount);
        }

    }


    // Re-enable sorting only if lapsTable is not really large

    if (ui->lapsTableWidget->rowCount() < 40000) {
        ui->lapsTableWidget->setSortingEnabled(lapsTableSortingEnabled);
    }
    ui->activeRidersTableWidget->setSortingEnabled(activeRidersSortingEnabled);

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
    QString s2 = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh:mm:ss") + " " + s;
    m->appendPlainText(s2);
    if (scrollToBottomRequired) {
        m->verticalScrollBar()->setValue(m->verticalScrollBar()->maximum());
    }
}

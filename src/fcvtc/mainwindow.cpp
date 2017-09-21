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
#include <QComboBox>

#include <stdio.h>
#include <unistd.h>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ltkcpp.h"
#include "creader.h"


#define LT_NAME             0
#define LT_LAPCOUNT         1
#define LT_DATETIME         2
#define LT_TIMESTAMP        3
#define LT_LAPTIME          4
#define LT_LAPSPEED         5

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
    float nominalSpeedkmph = 26.0;
    float nominalLapSec = (blackLineDistancem / 1000.) / nominalSpeedkmph * 3600.;
    maxAcceptableLapSec = nominalLapSec * 5.;           // max acceptable time for lap.  If greater, rider must have left and returned to track
    lapsTableTimeStampMaxAgeSec = 12 * 60 * 60;         // Start removing entries from lapsTable that are this age
    activeRidersTableTimeStampMaxAgeSec = 12 * 60 * 60; // Start removing riders from activeRidersTable that are this age
    activeRidersTablePurgeIntervalSec = 1 * 60 * 60;    // interval between purge of tables for stale entries
    lapsTableMaxSizeWithSort = 10000;


    // Create list of tag readers.  Leave IP empty for test mode.

//    readerList.append(new CReader("", CReader::track));
//    readerList.append(new CReader("", CReader::desk));
    readerList.append(new CReader("192.168.1.98", CReader::track));


    // Initialize member variables

    activeRidersTableSortingEnabled = true;
    lapsTableSortingEnabled = true;



    // Initialize 1-sec timer for panel dateTime and possibly other things

    ui->dateTimeLabel->clear();
    connect(&clockTimer, SIGNAL(timeout()), this, SLOT(onClockTimerTimeout()));
    clockTimer.setInterval(1000);
    clockTimer.start();


    // Configure gui

    ui->trackNameLabel->setText("LLRPLaps: " + trackName);
    setWindowTitle("LLRPLaps: " + trackName);
    ui->lapsTableSortedCheckBox->setCheckable(false);
    ui->activeRidersTableSortedCheckBox->setCheckable(false);

    // Configure messages console

    QPlainTextEdit *m = ui->messagesPlainTextEdit;
    m->setReadOnly(true);


    // Configure laps table

    QTableWidget *t = ui->lapsTableWidget;
    t->setColumnWidth(LT_NAME, 200);
    t->setColumnWidth(LT_LAPCOUNT, 60);
    t->setColumnWidth(LT_DATETIME, 80);
    t->setColumnWidth(LT_TIMESTAMP, 190);
    t->setColumnWidth(LT_LAPTIME, 80);
    t->setSortingEnabled(lapsTableSortingEnabled);
    t->setEnabled(false);
    connect(t->horizontalHeader(), SIGNAL(sectionClicked(int)), this, SLOT(onLapsTableHorizontalHeaderSectionClicked(int)));
    connect(ui->lapsTableSortedCheckBox, SIGNAL(clicked(bool)), this, SLOT(onLapsTableSortedCheckBoxClicked(bool)));


    // Configure riders table

    t = ui->activeRidersTableWidget;
    t->setColumnWidth(AT_NAME, 200);  // name
    t->setColumnWidth(AT_DATETIME, 80);  // time
    t->setColumnWidth(AT_LAPCOUNT, 70);  // lapCount
    t->setColumnWidth(AT_BESTLAPTIME, 100);  // bestLapTime;
    t->setColumnWidth(AT_AVERAGELAPTIME, 100);  // averageLapTime;
    t->setSortingEnabled(activeRidersTableSortingEnabled);
    t->setEnabled(false);
    connect(t->horizontalHeader(), SIGNAL(sectionClicked(int)), this, SLOT(onActiveRidersTableHorizontalHeaderSectionClicked(int)));
    connect(ui->activeRidersTableSortedCheckBox, SIGNAL(clicked(bool)), this, SLOT(onActiveRidersTableSortedCheckBoxClicked(bool)));


    // Default to showing messages during connection to reader

    ui->tabWidget->setCurrentIndex(2);


    // Create new CReader class for each reader and move each to new thread.  Some work still required to
    // ensure threads are stopped cleanly.

    for (int i=0; i<readerList.size(); i++) {
        readerList[i]->readerId = i + 1;
        connect(readerList[i], SIGNAL(newLogMessage(QString)), this, SLOT(onNewLogMessage(QString)));
        connect(readerList[i], SIGNAL(connected(int)), this, SLOT(onReaderConnected(int)));
        if (readerList[i]->antennaPosition == CReader::track) connect(readerList[i], SIGNAL(newTag(CTagInfo)), this, SLOT(onNewTag(CTagInfo)));
        else if (readerList[i]->antennaPosition == CReader::desk) connect(readerList[i], SIGNAL(newTag(CTagInfo)), this, SLOT(onNewDeskTag(CTagInfo)));

        QThread *readerThread = new QThread(this);
        readerList[i]->moveToThread(readerThread);
        connect(readerThread, SIGNAL(started(void)), readerList[i], SLOT(onStarted(void)));
        //connect(reader, SIGNAL(finished()), reader, SLOT(deleteLater()));
        //connect(readerThread, SIGNAL(finished(void)), readerThread, SLOT(deleteLater(void)));
        readerThread->start();
    }


    // Configure antenna power comboBoxes (enabled when reader connects)

    ui->antenna1ComboBox->setEnabled(false);
    ui->antenna2ComboBox->setEnabled(false);
   // ui->antenna3ComboBox->setEnabled(false);
   // ui->antenna4ComboBox->setEnabled(false);


    connect(ui->antenna1ComboBox, SIGNAL(activated(int)), this, SLOT(onAntenna1ComboBoxActivated(int)));

    // Start timer that will purge old riders from activeRidersTable

    connect(&purgeActiveRidersListTimer, SIGNAL(timeout(void)), this, SLOT(onPurgeActiveRidersList(void)));
    purgeActiveRidersListTimer.setInterval(activeRidersTablePurgeIntervalSec * 1000);
    purgeActiveRidersListTimer.start();

}






MainWindow::~MainWindow() {
    delete ui;
    for (int i=0; i<readerList.size(); i++) {
        delete readerList[i];
    }
    readerList.clear();
}



void MainWindow::onAntenna1ComboBoxActivated(int index) {
    printf("%d\n", index);
    fflush(stdout);
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
    unsigned long long currentTimeUSec = QDateTime::currentMSecsSinceEpoch() * 1000;

    // Loop through all active riders and see which are geting old

    activeRidersTableMutex.lock();
    ui->activeRidersTableWidget->setSortingEnabled(false);

    for (int i=activeRidersList.size()-1; i>=0; i--) {
        long long inactiveSec = (currentTimeUSec - activeRidersList[i].previousTimeStampUSec) / 1000000;
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
    ui->activeRidersTableWidget->setSortingEnabled(activeRidersTableSortingEnabled);

    // Loop through lapsTable and remove any entries where the timeStamp is older than threshold

    lapsTableMutex.lock();
    ui->lapsTableWidget->setSortingEnabled(false);

    bool scrollToBottomRequired = false;
    if (ui->lapsTableWidget->verticalScrollBar()->sliderPosition() == ui->lapsTableWidget->verticalScrollBar()->maximum()) {
        scrollToBottomRequired = true;
    }
    for (int i=ui->lapsTableWidget->rowCount()-1; i>=0; i--) {  // ignore last entry
        unsigned long long timeStampUSec = ui->lapsTableWidget->item(i, LT_TIMESTAMP)->text().toULongLong();
        long long timeStampAgeSec = (currentTimeUSec - timeStampUSec) / 1000000;
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
    ui->tabWidget->setCurrentIndex(0);
    onNewLogMessage(s.sprintf("Connected to reader %d", readerId));

    // Populate comboBox with available power settings for each reader (WIP).
    // Reader has been configured to select lowest power setting when connected.

    QList<int> *transmitPowerList = readerList[readerId-1]->getTransmitPowerList();
    ui->antenna1ComboBox->clear();
    if (transmitPowerList) for (int i=0; i<transmitPowerList->size(); i++) {
        ui->antenna1ComboBox->addItem(s.setNum(transmitPowerList->at(i)));
    }


    ui->antenna1ComboBox->setEnabled(true);


    ui->lapsTableWidget->setEnabled(true);
    ui->activeRidersTableWidget->setEnabled(true);
}



void MainWindow::onLapsTableHorizontalHeaderSectionClicked(int /*section*/) {
    if (ui->lapsTableWidget->isSortingEnabled()) {
        ui->lapsTableSortedCheckBox->setCheckable(true);
        ui->lapsTableSortedCheckBox->setChecked(true);
    }
}



void MainWindow::onActiveRidersTableHorizontalHeaderSectionClicked(int /*section*/) {
    if (ui->activeRidersTableWidget->isSortingEnabled()) {
        ui->activeRidersTableSortedCheckBox->setCheckable(true);
        ui->activeRidersTableSortedCheckBox->setChecked(true);
    }
}


void MainWindow::onLapsTableSortedCheckBoxClicked(bool state) {
    if (!state) {
        ui->lapsTableSortedCheckBox->setChecked(false);
        ui->lapsTableSortedCheckBox->setCheckable(false);
        ui->lapsTableWidget->setSortingEnabled(true);
        ui->lapsTableWidget->sortByColumn(LT_TIMESTAMP);
        ui->lapsTableWidget->setSortingEnabled(false);
    }
}


void MainWindow::onActiveRidersTableSortedCheckBoxClicked(bool state) {
    if (!state) {
        ui->activeRidersTableSortedCheckBox->setChecked(false);
        ui->activeRidersTableSortedCheckBox->setCheckable(false);
        ui->activeRidersTableWidget->setSortingEnabled(true);
        ui->activeRidersTableWidget->sortByColumn(LT_TIMESTAMP);
        ui->activeRidersTableWidget->setSortingEnabled(false);
    }
}


void MainWindow::onNewDeskTag(CTagInfo tagInfo) {
    ui-> deskTagIdLineEdit->setText(tagInfo.tagId);
}



// Process new tag event
//
void MainWindow::onNewTag(CTagInfo tagInfo) {
    QString s;
    QTableWidget *t = NULL;
    static int tagCount = 0;

    tagCount++;

    // Add string to messages window

    onNewLogMessage(s.sprintf("readerId=%d antennaId=%d timeStampUSec=%llu tagData=%s", tagInfo.readerId, tagInfo.antennaId, tagInfo.timeStampUSec, tagInfo.tagId.toLatin1().data()));

    // Turn off table sorting and lock mutex while we update tables

    lapsTableMutex.lock();
    activeRidersTableMutex.lock();
    ui->lapsTableWidget->setSortingEnabled(false);
    ui->activeRidersTableWidget->setSortingEnabled(false);


    // activeRidersList is the main list containing information from all active riders.  Use it for all calcualtions
    // and then put information to be displayed into activeRidersTable and/or lapsTable.

    // Check to see if tag is in the activeRidersList and get index if it is.  If not in list, set newActiveRider.

    int activeRidersListIndex = -1;
    for (int i=0; i<activeRidersList.size(); i++) {
        if (tagInfo.tagId == activeRidersList[i].tagId) {
            activeRidersListIndex = i;
            break;
        }
    }
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
        rider->previousTimeStampUSec = tagInfo.timeStampUSec;
        activeRidersList.append(*rider);
        activeRidersListIndex = activeRidersList.size() - 1;
        rider = &activeRidersList[activeRidersListIndex];
    }

    // else we have completed at least one full lap so calculate lap count and lap time

    else {
        rider = &activeRidersList[activeRidersListIndex];
        float lapTimeSec = (double)(tagInfo.timeStampUSec - rider->previousTimeStampUSec) / 1.e6;

        // If lap time is greater than 120 sec, rider must have taken a break so do not
        // calculate lap time

        if (lapTimeSec > maxAcceptableLapSec) {
            rider->lapTimeSec = 0.;
            rider->previousTimeStampUSec = tagInfo.timeStampUSec;
        }
        else {
            rider->lapCount++;
            rider->lapTimeSec = lapTimeSec;
            rider->previousTimeStampUSec = tagInfo.timeStampUSec;
            if (rider->lapTimeSec < rider->bestLapTimeSec) rider->bestLapTimeSec = rider->lapTimeSec;
            rider->lapTimeSumSec += rider->lapTimeSec;
        }
    }

    // Update currentTime string

    QString currentTimeString(QDateTime::fromMSecsSinceEpoch(tagInfo.timeStampUSec / 1000).toString("hh:mm:ss"));


    // Append new entry to lapsTable

    t = ui->lapsTableWidget;
    int r = t->rowCount();
    bool scrollToBottomRequired = false;
    if (t->verticalScrollBar()->sliderPosition() == t->verticalScrollBar()->maximum()) {
        scrollToBottomRequired = true;
    }
    t->insertRow(r);
    t->setRowHeight(r, 20);
    if (scrollToBottomRequired) {
        t->scrollToBottom();
    }

    // Create new lapsTable entries

    t->setItem(r, LT_NAME, new QTableWidgetItem());
    t->item(r, LT_NAME)->setText(name);

    QTableWidgetItem *item = new QTableWidgetItem;
    item->setData(Qt::DisplayRole, rider->lapCount);
    t->setItem(r, LT_LAPCOUNT, item);
    t->item(r, LT_LAPCOUNT)->setTextAlignment(Qt::AlignHCenter);

    t->setItem(r, LT_DATETIME, new QTableWidgetItem());
    t->item(r, LT_DATETIME)->setText(currentTimeString);
    t->item(r, LT_DATETIME)->setTextAlignment(Qt::AlignLeft);

    item = new QTableWidgetItem;
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
        t->setRowHeight(activeRidersTableIndex, 20);
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
        t = ui->activeRidersTableWidget;
        int r = t->rowCount();
        bool scrollToBottomRequired = false;
        if (t->verticalScrollBar()->sliderPosition() == t->verticalScrollBar()->maximum()) {
            scrollToBottomRequired = true;
        }
        t->insertRow(r);
        t->setRowHeight(r, 20);
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

        // Current time

        t->setItem(r, AT_DATETIME, new QTableWidgetItem);
        t->item(r, AT_DATETIME)->setText(currentTimeString);

        // Lap count

        if (!newActiveRider) {
            t->item(r, AT_LAPCOUNT)->setData(Qt::DisplayRole, rider->lapCount);
        }

        // Best lap time

        if (!newActiveRider) {
            t->item(r, AT_BESTLAPTIME)->setData(Qt::DisplayRole, rider->bestLapTimeSec);
        }

        // Average lap time

        if (!newActiveRider) {
            t->item(r, AT_AVERAGELAPTIME)->setData(Qt::DisplayRole, rider->lapTimeSumSec / (float)rider->lapCount);
        }

    }

    // Re-enable lapsTable sorting only if enabled and lapsTable is not really large

    if (ui->lapsTableWidget->rowCount() < lapsTableMaxSizeWithSort) {
        ui->lapsTableWidget->setSortingEnabled(lapsTableSortingEnabled);
    }
    else if (ui->lapsTableWidget->rowCount() == lapsTableMaxSizeWithSort) {
        ui->lapsTableWidget->sortByColumn(LT_TIMESTAMP, Qt::AscendingOrder);
        ui->lapsTableWidget->setSortingEnabled(false);
        ui->lapsTableSortedCheckBox->setChecked(false);
        ui->lapsTableSortedCheckBox->setEnabled(false);
    }

    // Re-enable activeRidersTable if enabled

    ui->activeRidersTableWidget->setSortingEnabled(activeRidersTableSortingEnabled);


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

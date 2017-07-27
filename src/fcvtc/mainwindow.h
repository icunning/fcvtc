// mainwindow.h
//


#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QMutex>
#include <QDateTime>

#include <QTableView>
#include <QLabel>
#include <QPushButton>


#include "creader.h"


namespace Ui {
class MainWindow;
}


// rider_t is a structure used to keep all information available for each rider.
//
struct rider_t {
    QString tagId;          // from reader
    QString name;           // from dBase if available
    unsigned long long previousTimeStampUSec;   // timestamp from reader, updated with each lap
    QDateTime mostRecentDateTime;
    int lapCount;
    float lapTimeSec;
    float bestLapTimeSec;
    float lapTimeSumSec;
};





class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
private:
    Ui::MainWindow *ui;
    QTimer clockTimer;
    QTimer purgeActiveRidersListTimer;
    QList<CReader *> readerList;
    QList<rider_t> activeRidersList;
    QMutex lapsTableMutex;
    QMutex activeRidersTableMutex;
    long long lapsTableTimeStampMaxAgeSec;
    long long activeRidersTableTimeStampMaxAgeSec;
    int activeRidersTablePurgeIntervalSec;
    unsigned long long initialTimeStampUSec;
    unsigned long long initialSinceEpochMSec;
    float maxLapSec;        // max time allowable for lap.  If greater, rider must have left and return to track
    float blackLineDistancem;
    bool lapsTableSortingEnabled;
    bool activeRidersSortingEnabled;
private slots:
    void onReaderConnected(int readerId);
    void onClockTimerTimeout(void);
    void onPurgeActiveRidersList(void);
    void onNewTag(CTagInfo);
    void onNewLogMessage(QString);
};

#endif // MAINWINDOW_H

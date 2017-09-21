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
#include <QTableWidgetItem>


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
    float maxAcceptableLapSec;        // max time allowable for lap.  If greater, rider must have left and return to track
    float blackLineDistancem;
    bool lapsTableSortingEnabled;
    bool activeRidersTableSortingEnabled;
    int lapsTableMaxSizeWithSort;
private slots:
    void onReaderConnected(int readerId);
    void onClockTimerTimeout(void);
    void onPurgeActiveRidersList(void);
    void onNewTag(CTagInfo);
    void onNewDeskTag(CTagInfo);
    void onNewLogMessage(QString);
    void onLapsTableHorizontalHeaderSectionClicked(int);
    void onActiveRidersTableHorizontalHeaderSectionClicked(int);
    void onLapsTableSortedCheckBoxClicked(bool);
    void onActiveRidersTableSortedCheckBoxClicked(bool);
    void onAntenna1ComboBoxActivated(int);
};

#endif // MAINWINDOW_H

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QMutex>
#include <QDateTime>


#include "creader.h"


namespace Ui {
class MainWindow;
}


struct rider_t {
    QString tagId;
    QString name;
    unsigned long long previousTimeStampUSec;
    QDateTime mostRecentDateTime;
    int lapCount;
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
    int lapsTableTimeStampMaxAgeMin;
    int activeRidersTableTimeStampMaxAgeMin;
    int activeRidersTablePurgeIntervalSec;
    unsigned long long initialTimeStampUSec;
    unsigned long long initialLocalMSecFromEpoch;
    float blackLineDistancem;
private slots:
    void onReaderConnected(int readerId);
    void onClockTimerTimeout(void);
    //void onAttemptConnectionTimerTimeout(void);
    void onPurgeActiveRidersList(void);
    void onNewTag(CTagInfo);
    void onNewLogMessage(QString);
};

#endif // MAINWINDOW_H

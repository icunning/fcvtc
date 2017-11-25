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
#include <QMessageBox>


#include "creader.h"
#include "cdbase.h"




namespace Ui {
class MainWindow;
}




class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
private:
    Ui::MainWindow *ui;
    CDbase dbase;
    QTimer clockTimer;
    QTimer purgeActiveRidersListTimer;
    CReader *trackReader;
    CReader *deskReader;
    QList<QThread *> readerThreadList;
    QList<CRider> activeRidersList;
    QMutex lapsTableMutex;
    QMutex activeRidersTableMutex;
    long long tablePurgeIntervalSec;
    float maxAcceptableLapSec;        // max time allowable for lap.  If greater, rider must have left and return to track
    float blackLineDistancem;
    float blueLineDistancem;
    bool lapsTableSortingEnabled;
    bool activeRidersTableSortingEnabled;
    int lapsTableMaxSizeWithSort;
    void loadNamesTable(void);
    void guiCritical(QString);
    void guiInformation(QString);
    QMessageBox::StandardButtons guiQuestion(QString s, QMessageBox::StandardButtons b=QMessageBox::Ok);
    float lapSpeed(float lapSec, float lapM);
    QList<float> trackLengthM;      // length of track (1 lap) at height of each antenna
    QSettings settings;
public slots:
    void onDbaseSearchPushButtonClicked(void);
    void onDbaseAddPushButtonClicked(void);
    void onDbaseClearPushButtonClicked(void);
    void onDbaseRemovePushButtonClicked(void);
    void onDbaseUpdatePushButtonClicked(void);
    void onDbaseReadPushButtonClicked(void);
private slots:
    void onReaderConnected(void);
    void onClockTimerTimeout(void);
    void onPurgeActiveRidersList(void);
    void onNewTrackTag(CTagInfo);
    void onNewDeskTag(CTagInfo);
    void onNewLogMessage(QString);
    void onLapsTableHorizontalHeaderSectionClicked(int);
    void onActiveRidersTableHorizontalHeaderSectionClicked(int);
    void onLapsTableSortedCheckBoxClicked(bool);
    void onActiveRidersTableSortedCheckBoxClicked(bool);
    void onAntenna1ComboBoxActivated(int);
    void onApplySettingsPushButtonClicked(void);
    void onSaveSettingsPushButtonClicked(void);
};

#endif // MAINWINDOW_H

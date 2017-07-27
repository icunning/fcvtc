// creader.h
//

#ifndef CREADER_H
#define CREADER_H


#include <QObject>
#include <QString>
#include <QList>

#include "main.h"
#include "ltkcpp.h"


class CTagInfo {
public:
    CTagInfo(void);
    void clear(void);
    int readerId;
    int antennaId;
    QString tagId;
    unsigned long long timeStampUSec;
    unsigned long long firstSeenInApplicationUSec;
};
Q_DECLARE_METATYPE(CTagInfo)



class CReader : public QObject {
Q_OBJECT
public:
    CReader(QString hostName, int readerId);
    ~CReader(void);
    int connectToReader(void);
    QList<int> *getTransmitPowerList(void);
    int setTransmitPower(int index);
    int setReaderConfiguration(void);
    int processReports(void);
private:
    QString hostName;
    int readerId;
    unsigned messageId;
    int checkConnectionStatus(void);
    int scrubConfiguration(void);
    int resetConfigurationToFactoryDefaults(void);
    int deleteAllROSpecs(void);
    int addROSpec(void);
    int enableROSpec(void);
    int startROSpec(void);
    LLRP::CConnection *pConnectionToReader;
    LLRP::CTypeRegistry *pTypeRegistry;
    LLRP::CMessage *recvMessage(int nMaxMS);
    void printXMLMessage(LLRP::CMessage *pMessage);
    void handleReaderEventNotification(LLRP::CReaderEventNotificationData *pNtfData);
    void handleAntennaEvent(LLRP::CAntennaEvent *pAntennaEvent);
    void handleReaderExceptionEvent(LLRP::CReaderExceptionEvent *pReaderExceptionEvent);
    int checkLLRPStatus(LLRP::CLLRPStatus *pLLRPStatus, char *pWhatStr);
    LLRP::CMessage *transact (LLRP::CMessage *pSendMsg);
    int sendMessage(LLRP::CMessage *pSendMsg);
    void processTagList(LLRP::CRO_ACCESS_REPORT *pRO_ACCESS_REPORT);
    int getTransmitPowerCapabilities(void);
    QList<int> transmitPowerList;
    bool simulateReaderMode;
    unsigned long long maxAllowableTimeInListUSec;
signals:
    void connected(int readerId);
    void newTag(CTagInfo);
    void newLogMessage(QString);
    void error(QString);
private slots:
    void onStarted(void);
};

#endif // CREADER_H

#ifndef CREADER_H
#define CREADER_H


#include <QObject>
#include <QString>

#include "ltkcpp.h"


class CTagInfo {
public:
    CTagInfo(void);
    void clear(void);
    int readerId;
    int antennaId;
    //unsigned long long getTimeStampUSec(void);
    //double getTimeStampSec(void);
    double timeStampSec;
    QList<unsigned char> data;
    unsigned long long timeStampUSec;
};



class CReader : public QObject {
Q_OBJECT
public:
    CReader(QString readerHostName, int readerId, int verbose=0);
    ~CReader(void);
    int processRecentChipsSeen(void);
private:
    int readerId;
    int verbose;
    LLRP::CConnection *pConnectionToReader;
    LLRP::CTypeRegistry *pTypeRegistry;
    int checkConnectionStatus(void);
    LLRP::CMessage *recvMessage(int nMaxMS);
    void printXMLMessage(LLRP::CMessage *pMessage);
    int scrubConfiguration(void);
    int resetConfigurationToFactoryDefaults(void);
    int deleteAllROSpecs(void);
    int addROSpec(void);
    int enableROSpec(void);
    int startROSpec(void);
    void handleReaderEventNotification(LLRP::CReaderEventNotificationData *pNtfData);
    void handleAntennaEvent(LLRP::CAntennaEvent *pAntennaEvent);
    void handleReaderExceptionEvent(LLRP::CReaderExceptionEvent *pReaderExceptionEvent);
    int checkLLRPStatus(LLRP::CLLRPStatus *pLLRPStatus, char *pWhatStr);
    LLRP::CMessage *transact (LLRP::CMessage *pSendMsg);
    int sendMessage(LLRP::CMessage *pSendMsg);
    int awaitReports(void);
    void processTagList(LLRP::CRO_ACCESS_REPORT *pRO_ACCESS_REPORT);
signals:
    void newTag(CTagInfo);
    void newLogMessage(QString);
};

#endif // CREADER_H

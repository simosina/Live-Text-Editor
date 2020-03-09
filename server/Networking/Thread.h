#ifndef THREAD_H
#define THREAD_H

#include <QThread>
#include <QTcpSocket>
#include <queue>
#include <QtCore/QTimer>
#include "../../common/Constants.h"
#include "../../client/utils/Identifier.h"
#include "../../client/utils/Character.h"
#include "../../common/commonFunctions.h"
#include "../../common/Message.h"
#include "../../client/utils/Pos.h"
#include "../CRDT/CRDT.h"
#include "Server.h"
#include <shared_mutex>

class Identifier;

class Character;

class Server;

class Thread : public QThread {
Q_OBJECT
private:
	std::map<qintptr, QTcpSocket *> sockets;                        // sync
	std::map<qintptr, QString> usernames;                           // sync
	std::map<qintptr, QTcpSocket *> pendingSocket;                  // sync
	CRDT *crdt;
	QString filename;                                               // sync
	QString usernameOwner;
	QTimer *saveTimer;
	Server *server;
	int saveInterval = 2 * /*60 **/ 1000; // 2 min (in ms) // TODO decidere intervallo
	bool needToSaveFile = false;
	bool timerStarted = false;
	bool fileDeleted = false;

public:
    std::shared_mutex mutexSockets;
    std::shared_mutex mutexUsernames;
    std::shared_mutex mutexPendingSockets;
    std::shared_mutex mutexFilename;
    std::shared_mutex mutexNeedToSave;
    std::shared_mutex mutexFileDeleted;


public:
	explicit Thread(QObject *parent = nullptr, CRDT *crdt = nullptr, QString filename = "", QString usernameOwner = "",
					Server *server = nullptr);

	void run();
	void addSocket(QTcpSocket *soc, QString username);                              // sync ok
	void sendListOfUsers(QTcpSocket *soc);                                          // sync ok
    std::map<qintptr, QTcpSocket *> getSockets();                                   // sync
    void changeFileName(QString filename);                                          // sync ok
    void sendRemoveUser(qintptr socketDescriptor, QString username);               // sync ok
    void addPendingSocket(qintptr socketDescriptor);                                // sync ok
    const std::map<qintptr, QString> &getUsernames() const;                         // sync ok
    void deleteFile();                                                              // sync ok

private:
	bool readInsert(QTcpSocket *soc);
	bool readStyleChanged(QTcpSocket *soc);
	bool readDelete(QTcpSocket *soc);
	void writeInsert(QTcpSocket *soc, Character character);
	void writeStyleChanged(QTcpSocket *soc, Character character);
	void writeDelete(QTcpSocket *soc, Character character);
	void sendNewUser(QTcpSocket *soc);                                              // sync ok
	void sendFile(QTcpSocket *soc);
	bool readShareCode(QTcpSocket *soc);                                            // sync ok
	bool sendAddFile(QTcpSocket *soc, QString filename);
	bool readEditAccount(QTcpSocket *soc);                                          // sync ok
	bool sendUser(QTcpSocket *soc);                                                 // sync ok
    bool readRequestUsernameList(QTcpSocket *soc);
    bool readFileInformationChanges(QTcpSocket *soc);                               // sync ok
    bool readDeleteFile(QTcpSocket *soc);                                           // sync ok
    bool readFileName(QTcpSocket *soc, QMetaObject::Connection *connectReadyRead, QMetaObject::Connection *connectDisconnected);

signals:
    void error(QTcpSocket::SocketError socketerror);
	void newMessage();

public slots:
	void readyRead(QTcpSocket *soc, QMetaObject::Connection *c, QMetaObject::Connection *d);                                            // sync
	void
	disconnected(QTcpSocket *socket, qintptr socketDescriptor, QMetaObject::Connection *c, QMetaObject::Connection *d);                 // sync
	void saveCRDTToFile();                                                                                                              // sync ok
};

#endif // THREAD_H
#include "SpoofBridge.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QTimer>
#include <QNetworkInterface>

SpoofBridge::SpoofBridge(const QString &walletName,
                         quint16 listenPort,
                         const QString &remoteHost,
                         quint16 remotePort,
                         QObject *parent)
    : QObject(parent)
    , m_walletName(walletName)
    , m_remoteHost(remoteHost)
    , m_listenPort(listenPort)
    , m_remotePort(remotePort)
    , m_clientSocket(nullptr)
    , m_remoteSocket(nullptr)
    , m_connected(false)
    , m_reconnectTimer(new QTimer(this))
{
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &SpoofBridge::onNewConnection);

    if (!m_server->listen(QHostAddress::LocalHost, m_listenPort)) {
        emit logMessage(QString("SpoofBridge [%1]: Failed to listen on port %2: %3")
                            .arg(m_walletName).arg(m_listenPort).arg(m_server->errorString()));
    } else {
        emit logMessage(QString("SpoofBridge [%1]: Listening on port %2")
                            .arg(m_walletName).arg(m_listenPort));
    }

    m_reconnectTimer->setInterval(5000);
    connect(m_reconnectTimer, &QTimer::timeout, this, &SpoofBridge::attemptReconnect);

    attemptReconnect();
}

void SpoofBridge::setLocalAddresses(const QStringList &addresses)
{
    m_localAddresses = addresses;
    emit logMessage(QString("SpoofBridge [%1]: Registered %2 local addresses")
                        .arg(m_walletName).arg(addresses.size()));
    registerWithRemote();
}

QStringList SpoofBridge::localAddresses() const
{
    return m_localAddresses;
}

void SpoofBridge::setRemoteAddresses(const QStringList &addresses)
{
    m_remoteAddresses = addresses;
    m_remoteAddressSet = QSet<QString>(addresses.begin(), addresses.end());
    emit logMessage(QString("SpoofBridge [%1]: Registered %2 remote addresses")
                        .arg(m_walletName).arg(addresses.size()));
    emit remoteAddressesReceived(addresses);
}

QStringList SpoofBridge::remoteAddresses() const
{
    return m_remoteAddresses;
}

bool SpoofBridge::isRemoteAddress(const QString &address) const
{
    return m_remoteAddressSet.contains(address);
}

bool SpoofBridge::isConnected() const
{
    return m_clientSocket && m_clientSocket->state() == QAbstractSocket::ConnectedState;
}

bool SpoofBridge::isRemoteConnected() const
{
    return m_remoteSocket && m_remoteSocket->state() == QAbstractSocket::ConnectedState;
}

bool SpoofBridge::sendSpoofedTx(const QString &txid,
                                 quint64 amount,
                                 const QString &toAddress,
                                 const QString &description,
                                 qint64 timestamp)
{
    QJsonObject msg;
    msg["cmd"] = "spoofed_tx";
    msg["txid"] = txid;
    msg["amount"] = static_cast<qint64>(amount);
    msg["to_address"] = toAddress;
    msg["description"] = description;
    msg["timestamp"] = timestamp;
    msg["from_wallet"] = m_walletName;

    QTcpSocket *target = m_remoteSocket ? m_remoteSocket : m_clientSocket;
    if (!target || target->state() != QAbstractSocket::ConnectedState) {
        emit logMessage(QString("SpoofBridge [%1]: Cannot send spoofed tx - no connection")
                            .arg(m_walletName));
        return false;
    }

    sendJson(msg, target);
    emit logMessage(QString("SpoofBridge [%1]: Sent spoofed tx %2 to %3")
                        .arg(m_walletName).arg(txid).arg(toAddress));
    return true;
}

void SpoofBridge::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();

        if (m_remoteSocket) {
            emit logMessage(QString("SpoofBridge [%1]: Rejecting extra connection - already have one")
                                .arg(m_walletName));
            socket->disconnectFromHost();
            socket->deleteLater();
            continue;
        }

        m_remoteSocket = socket;
        connect(m_remoteSocket, &QTcpSocket::readyRead, this, &SpoofBridge::onReadyRead);
        connect(m_remoteSocket, &QTcpSocket::disconnected, this, &SpoofBridge::onRemoteDisconnected);

        emit logMessage(QString("SpoofBridge [%1]: Remote connected from %2:%3")
                            .arg(m_walletName)
                            .arg(socket->peerAddress().toString())
                            .arg(socket->peerPort()));
        emit remoteConnected();

        registerWithRemote();
    }
}

void SpoofBridge::onClientConnected()
{
    m_connected = true;
    m_reconnectTimer->stop();
    emit logMessage(QString("SpoofBridge [%1]: Connected to remote on %2:%3")
                        .arg(m_walletName).arg(m_remoteHost).arg(m_remotePort));
    emit connected();
    registerWithRemote();
}

void SpoofBridge::onClientDisconnected()
{
    m_connected = false;
    emit logMessage(QString("SpoofBridge [%1]: Client disconnected from remote")
                        .arg(m_walletName));
    emit disconnected();
    m_reconnectTimer->start();
}

void SpoofBridge::onRemoteDisconnected()
{
    if (m_remoteSocket) {
        m_remoteSocket->deleteLater();
        m_remoteSocket = nullptr;
    }
    emit logMessage(QString("SpoofBridge [%1]: Remote disconnected").arg(m_walletName));
    emit remoteDisconnected();
}

static void processBuffer(QByteArray &buffer, QTcpSocket *socket, SpoofBridge *bridge)
{
    buffer.append(socket->readAll());

    int idx;
    while ((idx = buffer.indexOf('\n')) != -1) {
        QByteArray line = buffer.left(idx).trimmed();
        buffer.remove(0, idx + 1);

        if (line.isEmpty()) continue;

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            emit bridge->logMessage(QString("SpoofBridge: Invalid JSON: %1").arg(err.errorString()));
            continue;
        }

        bridge->handleMessage(doc.object(), socket);
    }
}

void SpoofBridge::onReadyRead()
{
    if (!m_remoteSocket) return;
    processBuffer(m_remoteBuffer, m_remoteSocket, this);
}

void SpoofBridge::onClientReadyRead()
{
    if (!m_clientSocket) return;
    processBuffer(m_clientBuffer, m_clientSocket, this);
}

void SpoofBridge::attemptReconnect()
{
    if (m_connected) return;

    // If a previous client socket is still around but not connected, tear it
    // down before creating a new one. Without this the very first failed
    // connect would leave a dangling socket and we'd never retry.
    if (m_clientSocket) {
        if (m_clientSocket->state() == QAbstractSocket::ConnectedState ||
            m_clientSocket->state() == QAbstractSocket::ConnectingState ||
            m_clientSocket->state() == QAbstractSocket::HostLookupState) {
            return;
        }
        m_clientSocket->deleteLater();
        m_clientSocket = nullptr;
        m_clientBuffer.clear();
    }

    m_clientSocket = new QTcpSocket(this);
    connect(m_clientSocket, &QTcpSocket::connected, this, &SpoofBridge::onClientConnected);
    connect(m_clientSocket, &QTcpSocket::disconnected, this, &SpoofBridge::onClientDisconnected);
    connect(m_clientSocket, &QTcpSocket::readyRead, this, &SpoofBridge::onClientReadyRead);
    connect(m_clientSocket, &QAbstractSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        // Connection failed; make sure the retry timer is running so
        // attemptReconnect() gets another chance once the socket is closed.
        if (!m_reconnectTimer->isActive())
            m_reconnectTimer->start();
    });

    if (!m_reconnectTimer->isActive())
        m_reconnectTimer->start();

    emit logMessage(QString("SpoofBridge [%1]: Attempting connection to %2:%3")
                        .arg(m_walletName).arg(m_remoteHost).arg(m_remotePort));
    m_clientSocket->connectToHost(m_remoteHost, m_remotePort);
}

void SpoofBridge::sendJson(const QJsonObject &obj, QTcpSocket *socket)
{
    QByteArray data = QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n";
    socket->write(data);
    socket->flush();
}

void SpoofBridge::handleMessage(const QJsonObject &msg, QTcpSocket *socket)
{
    QString cmd = msg["cmd"].toString();

    if (cmd == "register") {
        QJsonArray addrArray = msg["addresses"].toArray();
        QStringList addresses;
        for (const auto &v : addrArray) {
            addresses.append(v.toString());
        }
        QString remoteName = msg["wallet"].toString();
        emit logMessage(QString("SpoofBridge [%1]: Received registration from '%2' with %3 addresses")
                            .arg(m_walletName).arg(remoteName).arg(addresses.size()));
        setRemoteAddresses(addresses);

        QJsonObject ack;
        ack["cmd"] = "ack";
        ack["status"] = "ok";
        sendJson(ack, socket);

    } else if (cmd == "spoofed_tx") {
        QString txid = msg["txid"].toString();
        quint64 amount = static_cast<quint64>(msg["amount"].toDouble());
        QString toAddress = msg["to_address"].toString();
        QString description = msg["description"].toString();
        qint64 timestamp = static_cast<qint64>(msg["timestamp"].toDouble());

        emit logMessage(QString("SpoofBridge [%1]: Received spoofed tx %2 amount=%3 to=%4")
                            .arg(m_walletName).arg(txid).arg(amount).arg(toAddress));
        emit spoofedTxReceived(txid, amount, toAddress, description, timestamp);

        QJsonObject ack;
        ack["cmd"] = "ack";
        ack["status"] = "ok";
        sendJson(ack, socket);

    } else if (cmd == "ack") {
        // acknowledged
    }
}

void SpoofBridge::registerWithRemote()
{
    if (m_localAddresses.isEmpty()) return;

    QJsonObject msg;
    msg["cmd"] = "register";
    msg["wallet"] = m_walletName;

    QJsonArray addrArray;
    for (const auto &addr : m_localAddresses) {
        addrArray.append(addr);
    }
    msg["addresses"] = addrArray;

    QTcpSocket *target = m_remoteSocket ? m_remoteSocket : m_clientSocket;
    if (target && target->state() == QAbstractSocket::ConnectedState) {
        sendJson(msg, target);
    }
}

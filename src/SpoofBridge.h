#ifndef SPOOFBRIDGE_H
#define SPOOFBRIDGE_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHash>
#include <QStringList>
#include <QJsonObject>
#include <QSet>
#include <QByteArray>
#include <QTimer>

class SpoofBridge : public QObject
{
    Q_OBJECT

public:
    SpoofBridge(const QString &walletName,
                quint16 listenPort,
                const QString &remoteHost,
                quint16 remotePort,
                QObject *parent = nullptr);

    void setLocalAddresses(const QStringList &addresses);
    QStringList localAddresses() const;

    void setRemoteAddresses(const QStringList &addresses);
    QStringList remoteAddresses() const;
    bool isRemoteAddress(const QString &address) const;

    bool sendSpoofedTx(const QString &txid,
                       quint64 amount,
                       const QString &toAddress,
                       const QString &description,
                       qint64 timestamp);

    bool isConnected() const;
    bool isRemoteConnected() const;

signals:
    void connected();
    void disconnected();
    void remoteConnected();
    void remoteDisconnected();

    void spoofedTxReceived(const QString &txid,
                           quint64 amount,
                           const QString &toAddress,
                           const QString &description,
                           qint64 timestamp);

    void remoteAddressesReceived(const QStringList &addresses);

    void logMessage(const QString &msg);

private slots:
    void onNewConnection();
    void onClientConnected();
    void onClientDisconnected();
    void onRemoteDisconnected();
    void onReadyRead();
    void onClientReadyRead();
    void attemptReconnect();

private:
    void sendJson(const QJsonObject &obj, QTcpSocket *socket);
    void registerWithRemote();

public:
    void handleMessage(const QJsonObject &msg, QTcpSocket *socket);

    QString m_walletName;
    QStringList m_localAddresses;
    QStringList m_remoteAddresses;
    QSet<QString> m_remoteAddressSet;

    QTcpServer *m_server;
    QTcpSocket *m_clientSocket;
    QTcpSocket *m_remoteSocket;

    QString m_remoteHost;
    quint16 m_listenPort;
    quint16 m_remotePort;
    bool m_connected;
    QTimer *m_reconnectTimer;
    QByteArray m_remoteBuffer;
    QByteArray m_clientBuffer;
};

#endif // SPOOFBRIDGE_H

/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 22/11/2026 by @author Tsukini

File Name:
##  @file Mesh.hpp

File Description:
##  Links to every member of the network: server, dials, LAN discovery, one link per member
\**************************************************************/

#ifndef MESH_H
    #define MESH_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>  // _hot, _cold, _nodiscard
    #include "Server.hpp"       // xmouse::net::Server
    #include "Peer.hpp"         // xmouse::net::Peer, xmouse::net::PeerContext
    #include <QHostAddress>     // QHostAddress
    #include <QStringList>      // QStringList
    #include <QJsonObject>      // QJsonObject
    #include <QUdpSocket>       // QUdpSocket
    #include <QObject>          // QObject
    #include <QString>          // QString
    #include <QTimer>           // QTimer
    #include <QHash>            // QHash
    #include <QList>            // QList
    #include <QPair>            // QPair

namespace xmouse::net { // namespace start

//----------------------------------------------------------------//
/* STRUCT */

// A device announced on the LAN (members and the others)
struct Announce {
    QString id = "";
    QString name = "";
    QString host = ""; // address it was heard from
    quint16 port = 0;
    QString group = ""; // tag of its network ("" = in none)
    QString network = ""; // name of its network
    qint64 seen = 0; // ms
};

//----------------------------------------------------------------//
/* CLASS */

class Mesh: public QObject {
    Q_OBJECT

    private:
        PeerContext _ctx;
        Server _server;
        QHash<QString, Peer*> _peers; // ready links by device id
        QList<Peer*> _pending; // links being authenticated
        QHash<QString, QList<QPair<QString, quint16>>> _candidates; // id -> addresses to dial
        QHash<QString, qint64> _missingSince; // id -> ms since no link (the bigger id waits before dialing too)
        QHash<QString, qint64> _lastDial; // "<id>|<host>" -> ms
        QHash<QString, Announce> _announces; // heard on the LAN
        QHash<QString, int> _codeFailures; // address -> wrong codes
        QHash<QString, qint64> _banned; // address -> ms of the end of the ban
        QUdpSocket _discovery;
        QTimer _announce; // XMOUSE_ANNOUNCE_INTERVAL
        QTimer _dial; // dial loop
        QString _networkName = ""; // announced
        int _maxFailures = 5;
        bool _discover = true;

        // ---------- Pre-Function -------- //
        void onSocket_(WebSocket* ws); // new incoming websocket
        void watch_(Peer* peer); // signals of a link
        void onReady_(Peer* peer);
        void onClosed_(Peer* peer, const QString& reason);
        void dialAll_(void);
        void announce_(void);
        void onDatagram_(void);
        _cold _nodiscard bool keepNew_(const Peer* current, const Peer* candidate) const; // two links to the same member: which one stays

    public:
        // ---------- Pre-Function -------- //
        void start(const QString& bind, const quint16 port, const QSslConfiguration& tls, const bool discovery); // throws Network
        void stop(void);
        void setCandidates(const QString& id, const QList<QPair<QString, quint16>>& addresses); // empty = forgotten
        void dialNow(void); // a member appeared / the network came back: no wait
        Peer* join(const QString& host, const quint16 port, const QString& code, const QByteArray& expected = QByteArray()); // dial with a code (the result: joined / joinFailed), expected = identity of the link
        _hot void send(const QString& id, const QJsonObject& object);
        _hot void sendBinary(const QString& id, const QByteArray& data);
        _hot void broadcast(const QJsonObject& object, const QString& except = "");
        _hot void broadcastBinary(const QByteArray& data, const QString& except = "");
        void drop(const QString& id, const QString& reason); // close the link (removed member)
        void dropAll(const QString& reason); // left the network
        _cold _nodiscard QList<Announce> announces(void) const; // heard in the last 10 s
        _cold _nodiscard QStringList addressesOf(const QString& id) const; // addresses it was reached / heard on
        _cold _nodiscard bool isBanned(const QHostAddress& address) const;

        // ------------ Function ---------- //
        /* setter */
        _cold inline void setMaxFailures(const int max)       {this->_maxFailures = max;};
        _cold inline void setNetworkName(const QString& name) {this->_networkName = name;};

        /* getter */
        _cold _nodiscard inline PeerContext& context(void)             {return this->_ctx;};
        _cold _nodiscard inline Peer* peer(const QString& id) const    {return this->_peers.value(id, nullptr);};
        _cold _nodiscard inline QStringList ids(void) const            {return this->_peers.keys();};
        _cold _nodiscard inline bool isOnline(const QString& id) const {return this->_peers.contains(id);};
        _cold _nodiscard inline Server& server(void)                   {return this->_server;};
        _cold _nodiscard inline quint16 port(void) const               {return this->_server.port();};

        // ---------- Constructor --------- //
        explicit Mesh(QObject* parent = nullptr);

        // ----------- Destructor --------- //
        ~Mesh() override;

    signals:
        void peerReady(const QString& id); // a member is linked (authenticated)
        void peerLost(const QString& id, const QString& reason);
        void message(const QString& id, const QJsonObject& object);
        void binary(const QString& id, const QByteArray& data);
        void joinRequest(xmouse::net::Peer* peer); // a device gave the right code: approve() it (or not)
        void joined(const QJsonObject& welcome, const QString& id); // this device joined a network through id
        void joinFailed(const QString& reason);
        void announced(void); // the LAN list changed
        void linkFailed(const QString& id, const QString& reason); // a dial to a member failed (shown in the logs)
};

} // namespace end
#endif /* MESH_H */

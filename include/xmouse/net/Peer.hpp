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
##  @file Peer.hpp

File Description:
##  One authenticated link to another device (member proof or pairing code, pings)
\**************************************************************/

#ifndef PEER_H
    #define PEER_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>        // _hot, _cold, _nodiscard
    #include "../security/Crypto.hpp" // xmouse::security::auth_transcript
    #include "WebSocket.hpp"          // xmouse::net::WebSocket
    #include <QSslConfiguration>      // QSslConfiguration
    #include <QJsonDocument>          // QJsonDocument
    #include <QHostAddress>           // QHostAddress
    #include <QJsonObject>            // QJsonObject
    #include <QByteArray>             // QByteArray
    #include <functional>             // std::function
    #include <QPointer>               // QPointer
    #include <QObject>                // QObject
    #include <QString>                // QString
    #include <QTimer>                 // QTimer

namespace xmouse::security { // namespace start
class Group;
class PairingCode;
} // namespace end

namespace xmouse::net { // namespace start

//----------------------------------------------------------------//
/* ENUM */

enum class PeerState {
    Connecting, // tcp / tls / websocket handshake
    Hello, // server: hello sent, waiting the auth | client: waiting the hello
    Auth, // client: auth sent, waiting the welcome
    Approval, // server: a valid code, waiting the local person to accept the new device
    Ready, // authenticated member
    Closed,
};

//----------------------------------------------------------------//
/* STRUCT */

// What a link needs from the device (given by the mesh)
struct PeerContext {
    security::Group* group = nullptr;
    security::PairingCode* code = nullptr; // server side joins
    QByteArray fingerprint; // sha256 of the certificate of this device
    QString name = ""; // name of this device
    QString os = ""; // "linux" | "windows" | "macos" | "android"
    quint16 port = 0; // listening port of this device (the others dial it)
    QSslConfiguration tls; // client side configuration (post-quantum groups)
    QSslConfiguration tlsCompat; // client side for a device without ML-KEM (OpenSSL < 3.5) and for the joins (hybrid first, classic accepted)
    std::function<bool(const QString&)> postQuantum; // id -> the device has ML-KEM (true when unknown)
    std::function<QString(const QString&)> memberCheck; // id -> "" = a member that may connect, else why not
    std::function<QByteArray(const QString&)> pinned; // id -> its known certificate fingerprint ("" = unknown)
    std::function<QJsonObject(void)> state; // full replicated state (sent to a new member)
    std::function<bool(const QHostAddress&)> codeAllowed; // false = this address made too many wrong codes
    std::function<void(const QHostAddress&)> codeFailed; // a wrong code from this address
};

//----------------------------------------------------------------//
/* CLASS */

class Peer: public QObject {
    Q_OBJECT

    private:
        QPointer<WebSocket> _ws; // owned
        const PeerContext& _ctx;
        bool _outgoing = false; // this device dialed
        PeerState _state = PeerState::Connecting;
        QString _mode = "member"; // member | join
        QString _code = ""; // client join: the code typed
        QString _device = ""; // device id of the other side (from its hello / auth)
        QString _name = "";
        QString _os = "";
        quint16 _remotePort = 0; // its listening port
        QString _host = ""; // client side: address dialed
        QByteArray _serverNonce;
        QByteArray _clientNonce;
        QByteArray _serverFingerprint; // certificate of the server side of this tls session
        QByteArray _proofKey; // join: argon2id of the code | member with an old epoch: the previous key
        QByteArray _expected; // join: identity given by the link (fp), "" = not checked
        bool _stale = false; // server: the member proved itself with the previous key (it gets the new one)
        QTimer _ping; // every XMOUSE_PING_INTERVAL
        qint64 _lastSeen = 0; // ms, anything received
        int _rtt = -1; // ms, last ping round trip
        QString _closeReason = "";

        // ---------- Pre-Function -------- //
        void onText_(const QString& text);
        void onBinary_(const QByteArray& data);
        void onOpened_(void); // client: websocket ready
        void onHello_(const QJsonObject& o); // client
        void onAuth_(const QJsonObject& o); // server
        void onWelcome_(const QJsonObject& o); // client
        void sendHello_(void); // server
        void sendWelcome_(void); // server: accepted
        void fail_(const QString& reason);

        // ------------ Function ---------- //
        /* tools */
        _cold _nodiscard inline QByteArray transcript_(const QString& role, const QString& id) const {return security::auth_transcript(this->_serverNonce, this->_clientNonce, this->_serverFingerprint, this->_mode + "|" + role + "|" + id);};

    public:
        // ---------- Pre-Function -------- //
        _cold static Peer* accept(WebSocket* ws, const PeerContext& ctx, QObject* parent = nullptr); // server side (hello sent at once)
        _cold static Peer* dial(const QString& host, const quint16 port, const PeerContext& ctx, const QString& code = "", QObject* parent = nullptr, const bool compat = false, const QByteArray& expected = QByteArray()); // client side, code = join, compat = tlsCompat, expected = certificate fingerprint of the link
        _cold void approve(const bool accepted); // server: answer of the local person to a join
        _cold void close(const QString& reason);

        // ------------ Function ---------- //
        /* send */
        _hot inline void sendJson(const QJsonObject& object) {if (this->_ws && this->_ws->isOpen()) this->_ws->sendText(QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact)));};
        _hot inline void sendBinary(const QByteArray& data)  {if (this->_state == PeerState::Ready && this->_ws && this->_ws->isOpen()) this->_ws->sendBinary(data);};

        /* getter */
        _cold _nodiscard inline QHostAddress address(void) const                {return this->_ws ? this->_ws->peerAddress() : QHostAddress();};
        _hot _nodiscard inline qint64 pending(void) const                       {return this->_ws ? this->_ws->pendingBytes() : 0;}; // bytes not written yet (back pressure)
        _cold _nodiscard inline PeerState state(void) const                     {return this->_state;};
        _cold _nodiscard inline bool isReady(void) const                        {return this->_state == PeerState::Ready;};
        _cold _nodiscard inline bool isOutgoing(void) const                     {return this->_outgoing;};
        _cold _nodiscard inline const QString& id(void) const                   {return this->_device;};
        _cold _nodiscard inline const QString& name(void) const                 {return this->_name;};
        _cold _nodiscard inline const QString& os(void) const                   {return this->_os;};
        _cold _nodiscard inline const QString& mode(void) const                 {return this->_mode;};
        _cold _nodiscard inline const QString& host(void) const                 {return this->_host;};
        _cold _nodiscard inline quint16 remotePort(void) const                  {return this->_remotePort;};
        _cold _nodiscard inline int rtt(void) const                             {return this->_rtt;};
        _cold _nodiscard inline bool stale(void) const                          {return this->_stale;};
        _cold _nodiscard inline const QByteArray& serverFingerprint(void) const {return this->_serverFingerprint;};

        // ---------- Constructor --------- //
        Peer(WebSocket* ws, const PeerContext& ctx, const bool outgoing, QObject* parent = nullptr);

        // ----------- Destructor --------- //
        ~Peer() override;

    signals:
        void ready(void); // authenticated (member or new member)
        void joinRequest(void); // server: valid code, approve() expected
        void joined(const QJsonObject& welcome); // client of a join: the network given ({"group", "state"})
        void message(const QJsonObject& object); // control message once ready
        void binary(const QByteArray& data); // hot path message once ready
        void closed(const QString& reason); // once, whatever the reason
};

} // namespace end
#endif /* PEER_H */

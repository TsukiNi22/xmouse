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
##  @file Datagrams.hpp

File Description:
##  Encrypted udp of the hot path (moves, presence): AES-256-GCM with a key derived from the network key,
##  late / replayed packets dropped (the newest position only matters)
\**************************************************************/

#ifndef DATAGRAMS_H
    #define DATAGRAMS_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp> // _hot, _cold, _nodiscard
    #include <QHostAddress>    // QHostAddress
    #include <QByteArray>      // QByteArray
    #include <QUdpSocket>      // QUdpSocket
    #include <QObject>         // QObject
    #include <QString>         // QString
    #include <QHash>           // QHash
    #include <QPair>           // QPair

    //----------------------------------------------------------------//
    /* DEFINE */

    /* packet */
    #define XMOUSE_UDP_MAGIC 0x58 // 'X'
    #define XMOUSE_UDP_VERSION 1
    #define XMOUSE_UDP_SALT 8 // random bytes of the nonce, new at every start (and when the counter wraps)
    #define XMOUSE_UDP_MAX 1200 // biggest packet (no ip fragmentation)

namespace xmouse::net { // namespace start

//----------------------------------------------------------------//
/* STRUCT */

// What a packet carries once opened
struct Datagram {
    QString sender = ""; // device id
    int epoch = 0; // key epoch (low 8 bits)
    QByteArray salt; // nonce prefix of the sender run
    quint32 counter = 0; // nonce suffix (strictly growing in a run)
    QByteArray inner; // binary message (Binary::Pointer, Presence, UdpProbe)
};

//----------------------------------------------------------------//
/* CLASS */

/*
 * Only the newest packet of a sender run is accepted: an older one (reordered) or the same one again (replayed) is
 * dropped. A new salt (restart of the sender) starts a new run.
*/
class ReplayGuard {
    private:
        QHash<QString, QPair<QByteArray, quint32>> _last; // sender -> (salt, highest counter seen)

    public:
        // ------------ Function ---------- //
        /* check */
        _hot _nodiscard inline bool accept(const QString& sender, const QByteArray& salt, const quint32 counter)
        {
            auto it = this->_last.find(sender);

            if (it != this->_last.end() && it->first == salt && counter <= it->second) return false;
            this->_last[sender] = {salt, counter};
            return true;
        };

        /* setter */
        _cold inline void forget(const QString& sender) {this->_last.remove(sender);};
        _cold inline void clear(void)                   {this->_last.clear();};
};

/*
 * Packet: [u8 magic][u8 version][u8 epoch][u8 length][sender id][8 salt][u32 counter][AES-256-GCM(inner) + 16 tag]
 * The header is the additional data of the AEAD, the nonce is salt + counter, the key HMAC(network key, "xmouse-udp-v1")
*/
class Datagrams: public QObject {
    Q_OBJECT

    private:
        QUdpSocket _socket;
        QString _self = ""; // id written in the packets sent
        QByteArray _key; // udp key of the current epoch
        QByteArray _previous; // udp key of the epoch before (a member late on the rotation)
        int _epoch = 0;
        QByteArray _salt; // XMOUSE_UDP_SALT bytes
        quint32 _counter = 0;
        ReplayGuard _guard;
        bool _bound = false;

        // ---------- Pre-Function -------- //
        void onReadyRead_(void);

    public:
        // ---------- Pre-Function -------- //
        bool start(const QString& bind, const quint16 port); // false = the port is taken (tcp only)
        void stop(void);
        void setKeys(const QByteArray& networkKey, const int epoch, const QByteArray& previousNetworkKey); // "" = not in a network (nothing sent / accepted)
        _hot bool send(const QHostAddress& to, const quint16 port, const QByteArray& inner); // false = not sent (no key, too big)

        /* packet */
        _cold _nodiscard static QByteArray deriveKey(const QByteArray& networkKey); // "" for an empty network key
        _hot _nodiscard static QByteArray seal(const QByteArray& key, const int epoch, const QString& sender, const QByteArray& salt, const quint32 counter, const QByteArray& inner);
        _hot _nodiscard static bool open(const QByteArray& packet, const QByteArray& key, const QByteArray& previous, const int epoch, Datagram& out); // false = not ours / forged / other key

        // ------------ Function ---------- //
        /* setter */
        _cold inline void setSelf(const QString& self) {this->_self = self;};
        _cold inline void forget(const QString& id)    {this->_guard.forget(id);};

        /* getter */
        _cold _nodiscard inline bool isBound(void) const {return this->_bound;};
        _cold _nodiscard inline bool hasKey(void) const  {return this->_key.size() == 32;};

        // ---------- Constructor --------- //
        explicit Datagrams(QObject* parent = nullptr);

        // ----------- Destructor --------- //
        ~Datagrams() override = default;

    signals:
        void received(const QString& sender, const QHostAddress& from, const quint16 port, const QByteArray& inner); // authenticated, newest of its run
};

} // namespace end
#endif /* DATAGRAMS_H */

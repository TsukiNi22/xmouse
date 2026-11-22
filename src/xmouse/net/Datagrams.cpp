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
##  @file Datagrams.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#define _Exception
#include <utils/utils.hpp>
#include "xmouse/security/Crypto.hpp"
#include "xmouse/net/Datagrams.hpp"
#include "xmouse/net/Protocol.hpp"
#include <QNetworkDatagram>

//----------------------------------------------------------------//
/* TOOLS */

_hot static QByteArray nonce_of(const QByteArray& salt, const quint32 counter)
{
    QByteArray nonce = salt;

    nonce.append(static_cast<char>(counter >> 24));
    nonce.append(static_cast<char>(counter >> 16));
    nonce.append(static_cast<char>(counter >> 8));
    nonce.append(static_cast<char>(counter));
    return nonce;
}

//----------------------------------------------------------------//
/* CLASS */

xmouse::net::Datagrams::Datagrams(QObject* parent)
: QObject(parent)
{
    QObject::connect(&this->_socket, &QUdpSocket::readyRead, this, &Datagrams::onReadyRead_);
}

bool xmouse::net::Datagrams::start(const QString& bind, const quint16 port)
{
    const QHostAddress address = bind == "::" ? QHostAddress::Any : (bind == "0.0.0.0" || bind.isEmpty() ? QHostAddress::AnyIPv4 : QHostAddress(bind));

    this->stop();
    this->_salt = security::random_bytes(XMOUSE_UDP_SALT);
    this->_counter = 0;
    this->_bound = this->_socket.bind(address, port);
    return this->_bound;
}

void xmouse::net::Datagrams::stop(void)
{
    if (this->_bound) this->_socket.close();
    this->_bound = false;
    this->_guard.clear();
}

QByteArray xmouse::net::Datagrams::deriveKey(const QByteArray& networkKey)
{
    if (networkKey.isEmpty()) return {};
    return security::hmac_sha256(networkKey, "xmouse-udp-v1");
}

void xmouse::net::Datagrams::setKeys(const QByteArray& networkKey, const int epoch, const QByteArray& previousNetworkKey)
{
    this->_key = deriveKey(networkKey);
    this->_previous = deriveKey(previousNetworkKey);
    this->_epoch = epoch;
    this->_guard.clear();
}

QByteArray xmouse::net::Datagrams::seal(const QByteArray& key, const int epoch, const QString& sender, const QByteArray& salt, const quint32 counter, const QByteArray& inner)
{
    const QByteArray id = sender.toUtf8().left(255);
    QByteArray header;

    header.append(static_cast<char>(XMOUSE_UDP_MAGIC));
    header.append(static_cast<char>(XMOUSE_UDP_VERSION));
    header.append(static_cast<char>(epoch & 0xff));
    header.append(static_cast<char>(id.size()));
    header.append(id);
    header.append(salt);
    header.append(nonce_of(QByteArray(), counter)); // u32 counter big endian
    return header + security::aead_seal(key, nonce_of(salt, counter), header, inner);
}

bool xmouse::net::Datagrams::open(const QByteArray& packet, const QByteArray& key, const QByteArray& previous, const int epoch, Datagram& out)
{
    qsizetype pos = 4;
    int length = 0;
    bool ok = false;
    QByteArray header;

    if (packet.size() < 4 + XMOUSE_UDP_SALT + 4 + 16) return false;
    if (static_cast<std::uint8_t>(packet[0]) != XMOUSE_UDP_MAGIC || static_cast<std::uint8_t>(packet[1]) != XMOUSE_UDP_VERSION) return false;
    out.epoch = static_cast<std::uint8_t>(packet[2]);
    length = static_cast<std::uint8_t>(packet[3]);
    if (packet.size() < 4 + length + XMOUSE_UDP_SALT + 4 + 16) return false;
    out.sender = QString::fromUtf8(packet.mid(pos, length));
    pos += length;
    out.salt = packet.mid(pos, XMOUSE_UDP_SALT);
    pos += XMOUSE_UDP_SALT;
    out.counter = (static_cast<quint32>(static_cast<std::uint8_t>(packet[pos])) << 24) | (static_cast<quint32>(static_cast<std::uint8_t>(packet[pos + 1])) << 16)
        | (static_cast<quint32>(static_cast<std::uint8_t>(packet[pos + 2])) << 8) | static_cast<quint32>(static_cast<std::uint8_t>(packet[pos + 3]));
    pos += 4;
    header = packet.left(pos);

    // The key of the epoch written in the packet: the current one, or the previous one (a member not rotated yet)
    if (out.epoch == (epoch & 0xff)) out.inner = security::aead_open(key, nonce_of(out.salt, out.counter), header, packet.mid(pos), ok);
    else if (out.epoch == ((epoch - 1) & 0xff) && previous.size() == 32) out.inner = security::aead_open(previous, nonce_of(out.salt, out.counter), header, packet.mid(pos), ok);
    return ok && !out.inner.isEmpty() && !out.sender.isEmpty();
}

bool xmouse::net::Datagrams::send(const QHostAddress& to, const quint16 port, const QByteArray& inner)
{
    QByteArray packet;

    if (!this->_bound || !this->hasKey() || this->_self.isEmpty() || to.isNull() || port == 0) return false;
    if (this->_counter == 0xffffffffu) {this->_salt = security::random_bytes(XMOUSE_UDP_SALT); this->_counter = 0;} // never the same nonce twice
    packet = seal(this->_key, this->_epoch, this->_self, this->_salt, ++this->_counter, inner);
    if (packet.size() > XMOUSE_UDP_MAX) return false;
    return this->_socket.writeDatagram(packet, to, port) == packet.size();
}

void xmouse::net::Datagrams::onReadyRead_(void)
{
    while (this->_socket.hasPendingDatagrams()) {
        const QNetworkDatagram datagram = this->_socket.receiveDatagram(XMOUSE_UDP_MAX);
        Datagram d;

        if (!this->hasKey() || !open(datagram.data(), this->_key, this->_previous, this->_epoch, d)) continue;
        if (d.sender == this->_self) continue; // our own broadcast / loop
        if (!this->_guard.accept(d.sender, d.salt, d.counter)) continue; // late (reordered) or replayed
        emit this->received(d.sender, datagram.senderAddress(), static_cast<quint16>(datagram.senderPort()), d.inner);
    }
}

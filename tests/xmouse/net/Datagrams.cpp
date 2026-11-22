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
#include <QCoreApplication>
#include <gtest/gtest.h>
#include <QEventLoop>
#include <QTimer>

//----------------------------------------------------------------//
/* TOOLS */

static void ensure_app(void)
{
    static int argc = 1;
    static char name[] = "unit_tests";
    static char* argv[] = {name, nullptr};
    if (!QCoreApplication::instance()) (void)new QCoreApplication(argc, argv); // never destroyed (see Mesh.cpp)
}

static QByteArray key_of(const char c)
{
    return xmouse::net::Datagrams::deriveKey(QByteArray(32, c));
}

//----------------------------------------------------------------//
/* SEAL / OPEN */

TEST(Datagrams, SealThenOpenGivesTheMessageBack)
{
    const QByteArray salt(8, 's');
    const QByteArray inner = xmouse::net::ByteWriter(xmouse::net::Binary::Pointer).u16(7).i32(100).i32(-20).data();
    const QByteArray packet = xmouse::net::Datagrams::seal(key_of('k'), 3, "deviceA", salt, 42, inner);
    xmouse::net::Datagram d;

    ASSERT_TRUE(xmouse::net::Datagrams::open(packet, key_of('k'), QByteArray(), 3, d));
    EXPECT_EQ(d.sender, "deviceA");
    EXPECT_EQ(d.counter, 42u);
    EXPECT_EQ(d.salt, salt);
    EXPECT_EQ(d.inner, inner);
}

TEST(Datagrams, AnotherKeyOrAChangedByteIsRefused)
{
    const QByteArray packet = xmouse::net::Datagrams::seal(key_of('k'), 1, "deviceA", QByteArray(8, 's'), 1, "hello");
    QByteArray tampered = packet;
    xmouse::net::Datagram d;

    EXPECT_FALSE(xmouse::net::Datagrams::open(packet, key_of('x'), QByteArray(), 1, d));
    tampered[tampered.size() - 20] = static_cast<char>(tampered[tampered.size() - 20] ^ 0x01);
    EXPECT_FALSE(xmouse::net::Datagrams::open(tampered, key_of('k'), QByteArray(), 1, d));
    tampered = packet;
    tampered[5] = static_cast<char>(tampered[5] ^ 0x01); // the sender id (additional data)
    EXPECT_FALSE(xmouse::net::Datagrams::open(tampered, key_of('k'), QByteArray(), 1, d));
    EXPECT_FALSE(xmouse::net::Datagrams::open("short", key_of('k'), QByteArray(), 1, d));
}

TEST(Datagrams, ThePreviousEpochStillOpensDuringARotation)
{
    const QByteArray packet = xmouse::net::Datagrams::seal(key_of('o'), 4, "late", QByteArray(8, 's'), 9, "move");
    xmouse::net::Datagram d;

    EXPECT_TRUE(xmouse::net::Datagrams::open(packet, key_of('n'), key_of('o'), 5, d)); // current epoch 5, the packet of epoch 4
    EXPECT_FALSE(xmouse::net::Datagrams::open(packet, key_of('n'), QByteArray(), 5, d)); // no previous key: refused
}

//----------------------------------------------------------------//
/* REPLAY / REORDER */

TEST(Datagrams, OnlyTheNewestPacketOfARunIsAccepted)
{
    xmouse::net::ReplayGuard guard;
    const QByteArray run1(8, 'a');
    const QByteArray run2(8, 'b');

    EXPECT_TRUE(guard.accept("A", run1, 5));
    EXPECT_FALSE(guard.accept("A", run1, 5)); // replayed
    EXPECT_FALSE(guard.accept("A", run1, 3)); // late (reordered)
    EXPECT_TRUE(guard.accept("A", run1, 6));
    EXPECT_TRUE(guard.accept("B", run1, 1)); // another sender
    EXPECT_TRUE(guard.accept("A", run2, 1)); // the sender restarted (new salt)
    guard.forget("A");
    EXPECT_TRUE(guard.accept("A", run2, 1));
}

//----------------------------------------------------------------//
/* LOOPBACK */

TEST(Datagrams, TwoSocketsExchangeAnEncryptedMove)
{
    ensure_app();
    xmouse::net::Datagrams a;
    xmouse::net::Datagrams b;
    QString sender;
    QByteArray received;
    QEventLoop loop;
    QTimer timeout;

    // Fixed ports on the loopback (two random free ones are not known before binding)
    ASSERT_TRUE(a.start("127.0.0.1", 47981));
    ASSERT_TRUE(b.start("127.0.0.1", 47982));
    a.setSelf("A");
    b.setSelf("B");
    a.setKeys(QByteArray(32, 'g'), 1, QByteArray());
    b.setKeys(QByteArray(32, 'g'), 1, QByteArray());
    QObject::connect(&b, &xmouse::net::Datagrams::received, &loop, [&](const QString& from, const QHostAddress&, const quint16, const QByteArray& inner) {
        sender = from;
        received = inner;
        loop.quit();
    });
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(3000);
    ASSERT_TRUE(a.send(QHostAddress::LocalHost, 47982, "move"));
    loop.exec();
    EXPECT_EQ(sender, "A");
    EXPECT_EQ(received, QByteArray("move"));
}

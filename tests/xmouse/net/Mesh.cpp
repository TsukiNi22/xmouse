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
##  @file Mesh.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#define _Exception
#include <utils/utils.hpp>
#include "xmouse/security/Certificate.hpp"
#include "xmouse/security/PairingCode.hpp"
#include "xmouse/security/Crypto.hpp"
#include "xmouse/security/Group.hpp"
#include "xmouse/net/Mesh.hpp"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <gtest/gtest.h>
#include <QEventLoop>
#include <QSslKey>
#include <memory>

//----------------------------------------------------------------//
/* TOOLS */

// One device: its folder, identity, network and mesh (loopback, random port)
struct Device {
    QTemporaryDir dir;
    xmouse::security::Certificate cert{dir.path()};
    xmouse::security::Group group{dir.path()};
    xmouse::security::PairingCode code;
    xmouse::net::Mesh mesh;
    QString allowed = ""; // the member id accepted by memberCheck

    explicit Device(const QString& name)
    {
        QSslConfiguration server = QSslConfiguration::defaultConfiguration();
        QSslConfiguration client = QSslConfiguration::defaultConfiguration();
        xmouse::net::PeerContext& ctx = this->mesh.context();

        this->cert.load();
        this->group.load();
        ctx.group = &this->group;
        ctx.code = &this->code;
        ctx.fingerprint = this->cert.fingerprint();
        ctx.name = name;
        ctx.os = "linux";
        xmouse::security::configure_tls(client, true);
        ctx.tls = client;
        ctx.tlsCompat = QSslConfiguration::defaultConfiguration();
        xmouse::security::configure_tls(ctx.tlsCompat, false);
        ctx.memberCheck = [this](const QString& id) {return id == this->allowed ? QString() : QString("unknown member");};
        ctx.state = []() {return QJsonObject{{"net/name", QJsonObject{{"v", "desk"}, {"c", "1"}, {"a", "x"}}}};};
        server.setLocalCertificate(this->cert.certificate());
        server.setPrivateKey(this->cert.key());
        xmouse::security::configure_tls(server, false);
        this->mesh.start("127.0.0.1", 0, server, false);
    };
};

static void ensure_app(void)
{
    static int argc = 1;
    static char name[] = "unit_tests";
    static char* argv[] = {name, nullptr};
    // Never destroyed: a static QCoreApplication deleted by exit() runs after the statics of Qt (crash at the end of a lone test)
    if (!QCoreApplication::instance()) (void)new QCoreApplication(argc, argv);
}

// Run the event loop until done() or the timeout (false = timeout)
template<typename Fn>
static bool wait_for(Fn done, const int milliseconds = 20000)
{
    QEventLoop loop;
    QTimer poll;
    QTimer timeout;

    if (done()) return true;
    QObject::connect(&poll, &QTimer::timeout, &loop, [&]() {if (done()) loop.quit();});
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    poll.start(20);
    timeout.start(milliseconds);
    loop.exec();
    return done();
}

//----------------------------------------------------------------//
/* TESTS */

/* Join */
TEST(Mesh, JoinWithTheRightCode)
{
    ensure_app();
    Device a("alpha");
    Device b("beta");
    QJsonObject welcome;
    bool asked = false;

    a.group.create("desk");
    QObject::connect(&a.mesh, &xmouse::net::Mesh::joinRequest, &a.mesh, [&](xmouse::net::Peer* peer) {asked = true; peer->approve(true);});
    QObject::connect(&b.mesh, &xmouse::net::Mesh::joined, &b.mesh, [&](const QJsonObject& w, const QString&) {welcome = w;});
    (void)b.mesh.join("127.0.0.1", a.mesh.port(), a.code.display());
    ASSERT_TRUE(wait_for([&]() {return !welcome.isEmpty() && a.mesh.isOnline(b.group.self());}));
    EXPECT_TRUE(asked);
    EXPECT_EQ(welcome.value("group").toObject().value("id").toString(), a.group.id());
    EXPECT_EQ(QByteArray::fromBase64(welcome.value("group").toObject().value("key").toString().toLatin1()), a.group.key());
    EXPECT_TRUE(welcome.value("state").toObject().contains("net/name"));
    EXPECT_TRUE(b.mesh.isOnline(a.group.self()));
}

TEST(Mesh, JoinWithAWrongCodeFails)
{
    ensure_app();
    Device a("alpha");
    Device b("beta");
    QString failure = "";

    a.group.create("desk");
    QObject::connect(&a.mesh, &xmouse::net::Mesh::joinRequest, &a.mesh, [](xmouse::net::Peer* peer) {peer->approve(true);});
    QObject::connect(&b.mesh, &xmouse::net::Mesh::joinFailed, &b.mesh, [&](const QString& reason) {failure = reason;});
    (void)b.mesh.join("127.0.0.1", a.mesh.port(), "AAA-AAA-AAA");
    ASSERT_TRUE(wait_for([&]() {return !failure.isEmpty();}));
    EXPECT_FALSE(b.mesh.isOnline(a.group.self()));
}

TEST(Mesh, JoinRefusedLocally)
{
    ensure_app();
    Device a("alpha");
    Device b("beta");
    QString failure = "";

    a.group.create("desk");
    QObject::connect(&a.mesh, &xmouse::net::Mesh::joinRequest, &a.mesh, [](xmouse::net::Peer* peer) {peer->approve(false);});
    QObject::connect(&b.mesh, &xmouse::net::Mesh::joinFailed, &b.mesh, [&](const QString& reason) {failure = reason;});
    (void)b.mesh.join("127.0.0.1", a.mesh.port(), a.code.display());
    ASSERT_TRUE(wait_for([&]() {return !failure.isEmpty();}));
    EXPECT_FALSE(b.group.joined());
}

/* Members */
TEST(Mesh, MembersLinkWithTheGroupKey)
{
    ensure_app();
    Device a("alpha");
    Device b("beta");

    a.group.create("desk");
    b.group.adopt(a.group.id(), a.group.name(), a.group.key(), a.group.epoch());
    a.allowed = b.group.self();
    b.allowed = a.group.self();
    b.mesh.setCandidates(a.group.self(), {{"127.0.0.1", a.mesh.port()}});
    b.mesh.dialNow();
    ASSERT_TRUE(wait_for([&]() {return a.mesh.isOnline(b.group.self()) && b.mesh.isOnline(a.group.self());}));
}

TEST(Mesh, AnotherKeyIsRefused)
{
    ensure_app();
    Device a("alpha");
    Device b("beta");
    QString failure = "";

    a.group.create("desk");
    b.group.adopt(a.group.id(), a.group.name(), xmouse::security::random_bytes(XMOUSE_GROUP_KEY_SIZE), a.group.epoch());
    a.allowed = b.group.self();
    QObject::connect(&b.mesh, &xmouse::net::Mesh::linkFailed, &b.mesh, [&](const QString&, const QString& reason) {failure = reason;});
    b.mesh.setCandidates(a.group.self(), {{"127.0.0.1", a.mesh.port()}});
    b.mesh.dialNow();
    ASSERT_TRUE(wait_for([&]() {return !failure.isEmpty();}));
    EXPECT_FALSE(a.mesh.isOnline(b.group.self()));
}

TEST(Mesh, AnUnknownMemberIsRefused)
{
    ensure_app();
    Device a("alpha");
    Device b("beta");
    QString failure = "";

    a.group.create("desk");
    b.group.adopt(a.group.id(), a.group.name(), a.group.key(), a.group.epoch());
    a.allowed = "someone else"; // b removed from the network
    QObject::connect(&b.mesh, &xmouse::net::Mesh::linkFailed, &b.mesh, [&](const QString&, const QString& reason) {failure = reason;});
    b.mesh.setCandidates(a.group.self(), {{"127.0.0.1", a.mesh.port()}});
    b.mesh.dialNow();
    ASSERT_TRUE(wait_for([&]() {return !failure.isEmpty();}));
    EXPECT_FALSE(a.mesh.isOnline(b.group.self()));
}

/* Messages */
TEST(Mesh, MessagesAndBinaryReachTheMember)
{
    ensure_app();
    Device a("alpha");
    Device b("beta");
    QJsonObject received;
    QByteArray data;

    a.group.create("desk");
    b.group.adopt(a.group.id(), a.group.name(), a.group.key(), a.group.epoch());
    a.allowed = b.group.self();
    b.allowed = a.group.self();
    QObject::connect(&a.mesh, &xmouse::net::Mesh::message, &a.mesh, [&](const QString&, const QJsonObject& o) {received = o;});
    QObject::connect(&a.mesh, &xmouse::net::Mesh::binary, &a.mesh, [&](const QString&, const QByteArray& d) {data = d;});
    b.mesh.setCandidates(a.group.self(), {{"127.0.0.1", a.mesh.port()}});
    b.mesh.dialNow();
    ASSERT_TRUE(wait_for([&]() {return b.mesh.isOnline(a.group.self());}));
    b.mesh.send(a.group.self(), QJsonObject{{"t", "hello-test"}, {"x", 42}});
    b.mesh.sendBinary(a.group.self(), QByteArray("\x20\x00\x01", 3));
    ASSERT_TRUE(wait_for([&]() {return !received.isEmpty() && !data.isEmpty();}));
    EXPECT_EQ(received.value("x").toInt(), 42);
    EXPECT_EQ(data, QByteArray("\x20\x00\x01", 3));
}

/* Join with a link (its identity) */
TEST(Mesh, JoinWithTheIdentityOfTheLink)
{
    ensure_app();
    Device a("alpha");
    Device b("beta");
    QJsonObject welcome;

    a.group.create("desk");
    QObject::connect(&a.mesh, &xmouse::net::Mesh::joinRequest, &a.mesh, [](xmouse::net::Peer* peer) {peer->approve(true);});
    QObject::connect(&b.mesh, &xmouse::net::Mesh::joined, &b.mesh, [&](const QJsonObject& w, const QString&) {welcome = w;});
    (void)b.mesh.join("127.0.0.1", a.mesh.port(), a.code.display(), a.cert.fingerprint());
    ASSERT_TRUE(wait_for([&]() {return !welcome.isEmpty();}));
}

TEST(Mesh, JoinRefusesAnotherIdentityThanTheLink)
{
    ensure_app();
    Device a("alpha");
    Device b("beta");
    QString failure = "";
    bool asked = false;

    a.group.create("desk");
    QObject::connect(&a.mesh, &xmouse::net::Mesh::joinRequest, &a.mesh, [&](xmouse::net::Peer* peer) {asked = true; peer->approve(true);});
    QObject::connect(&b.mesh, &xmouse::net::Mesh::joinFailed, &b.mesh, [&](const QString& reason) {failure = reason;});
    (void)b.mesh.join("127.0.0.1", a.mesh.port(), a.code.display(), QByteArray(32, 'x')); // the link of another device
    ASSERT_TRUE(wait_for([&]() {return !failure.isEmpty();}));
    EXPECT_FALSE(asked); // nothing sent: the code never reached the device
    EXPECT_TRUE(failure.contains("not the device of the link"));
}

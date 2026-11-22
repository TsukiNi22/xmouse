/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 16/07/2026 by @author Tsukini

File Name:
##  @file Node.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#define _Exception
#include <utils/utils.hpp>
#include "xmouse/security/Certificate.hpp"
#include "xmouse/security/Crypto.hpp"
#include "xmouse/net/Exposure.hpp"
#include "xmouse/net/Protocol.hpp"
#include "xmouse/ui/Ui.hpp"
#include "xmouse/Node.hpp"
#include <QNetworkDatagram>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QUdpSocket>
#include <QLockFile>
#include <iostream>

void xmouse::Node::run(void)
{
    if (this->_command == "version") {std::cout << "xmouse " XMOUSE_VERSION " (" << security::tls_summary().toStdString() << ")" << std::endl; return;}
    if (this->_command == "paths") {this->runPaths_(); return;}
    if (this->_command == "scan") {this->runScan_(); return;}
    if (this->_port > 0) this->_config.settings().port = static_cast<std::uint16_t>(this->_port);
    if (!this->_name.empty()) this->_config.settings().name = QString::fromStdString(this->_name);
    if (this->_regenCert) {
        security::Certificate cert(core::Config::dataDir());
        cert.load(true);
        std::cout << "New identity of this device: " << security::fingerprint_text(cert.fingerprint()).toStdString() << " (the members pinned the old one: remove it from the network and join again)" << std::endl;
    }

    // One panel per user (two would fight for the port and the edges)
    QLockFile lock(core::Config::dataDir() + "/xmouse.lock");
    lock.setStaleLockTime(0);
    if (!lock.tryLock(100)) throw utils::exception::ErrorException(utils::exception::ExternalCode::Usage, "xmouse is already running for this user");
    const QString join = QString::fromStdString(this->_target);
    const QString code = QString::fromStdString(this->_code);
    if (this->_tty || (!this->_gui && !ui::has_display())) this->_exit = ui::run_tty(this->_argc, this->_argv, this->_config, join, code);
    else this->_exit = ui::run_gui(this->_argc, this->_argv, this->_config, join, code);
}

void xmouse::Node::runPaths_(void)
{
    std::cout << "config  " << this->_config.path().toStdString() << std::endl;
    std::cout << "data    " << core::Config::dataDir().toStdString() << std::endl;
    std::cout << "logs    " << core::Config::logDir().toStdString() << std::endl;
    std::cout << "binary  " << core::Config::executablePath().toStdString() << std::endl;
}

void xmouse::Node::runScan_(void)
{
    QCoreApplication app(this->_argc, this->_argv);
    QUdpSocket socket;
    QElapsedTimer timer;
    QHash<QString, QJsonObject> found;

    // Listen to the announces of the devices for 3 s (another xmouse may hold the port: shared)
    if (!socket.bind(QHostAddress::AnyIPv4, XMOUSE_DISCOVERY_PORT, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Network, "Cannot listen on the discovery port " + std::to_string(XMOUSE_DISCOVERY_PORT));
    timer.start();
    while (timer.elapsed() < 3500) {
        if (!socket.waitForReadyRead(200)) continue;
        while (socket.hasPendingDatagrams()) {
            const QNetworkDatagram d = socket.receiveDatagram(4096);
            QJsonObject o = QJsonDocument::fromJson(d.data()).object();
            if (o.value("xmouse").toInt() != XMOUSE_PROTOCOL_VERSION) continue;
            o["host"] = QHostAddress(d.senderAddress().toIPv4Address()).toString();
            found.insert(o.value("id").toString(), o);
        }
    }
    if (found.isEmpty()) {std::cout << "No xmouse device announced on this LAN (3 s)." << std::endl; return;}
    for (const QJsonObject& o: found)
        std::cout << o.value("name").toString().toStdString() << "\t" << o.value("host").toString().toStdString() << ":" << o.value("port").toInt()
            << "\t" << (o.value("network").toString().isEmpty() ? std::string("(no network)") : o.value("network").toString().toStdString())
            << "\txmouse join " << o.value("host").toString().toStdString() << ":" << o.value("port").toInt() << " <code>" << std::endl;
}

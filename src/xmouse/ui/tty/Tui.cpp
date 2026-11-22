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
##  @file Tui.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xmouse/security/Crypto.hpp"
#include "xmouse/ui/tty/Tui.hpp"
#include <QCoreApplication>
#include <QMetaObject>
#include <QStringList>
#include <functional>
#include <iostream>
#include <string>

xmouse::ui::tty::Tui::Tui(core::Hub& hub, QObject* parent)
: QObject(parent), _hub{hub}
{
    QObject::connect(&hub, &core::Hub::notice, this, [this](const QString& text) {this->print_("! " + text);});
    QObject::connect(&hub, &core::Hub::networkChanged, this, [this]() {this->status_();});
    QObject::connect(&hub, &core::Hub::joinResult, this, [this](const bool ok, const QString& message) {this->print_((ok ? "joined: " : "join failed: ") + message);});
    QObject::connect(&hub, &core::Hub::joinApproval, this, [this](net::Peer* peer) {
        this->_pendingJoin = peer;
        this->print_("? " + peer->name() + " (" + peer->os() + ", " + peer->address().toString() + ") gave the right code: type 'accept' or 'refuse' (refused in 30 s)");
    });
    QObject::connect(&hub, &core::Hub::accessRequested, this, [this](const QString& id, const QString& name, const int seconds) {
        this->_pendingAccess = id;
        this->print_("? " + name + " asks the access to this panel: granted in " + QString::number(seconds) + " s, type 'deny' to refuse");
    });
    QObject::connect(&hub, &core::Hub::accessEnded, this, [this](const QString&, const bool granted) {this->print_(granted ? "access granted" : "access ended");});
    QObject::connect(&hub, &core::Hub::padApproval, this, [this](const QString& pending, const QString& name, const QString& kind) {
        this->_pendingPad = pending;
        this->print_("? the " + kind + " " + name + " gave the right code to drive this device: type 'pad accept' or 'pad refuse' (refused in 30 s)");
    });
    QObject::connect(&hub, &core::Hub::linkReceived, this, [this](const QString&, const QString& name, const QString& url) {this->print_("! " + name + " sent a link: " + url);});
    QObject::connect(&hub, &core::Hub::filesReceived, this, [this](const QString& from, const QStringList& paths, const QString&) {this->print_("! files of " + from + ":\n    " + paths.join("\n    "));});
    QObject::connect(&hub.log(), &core::AuditLog::entry, this, [this](const QJsonObject& e) {
        if (e.value("cat").toString() == "network" || e.value("level").toString() != "info") this->print_("  [" + e.value("cat").toString() + "] " + e.value("msg").toString());
    });
}

xmouse::ui::tty::Tui::~Tui()
{
    this->_reader.request_stop();
    if (this->_reader.joinable()) this->_reader.detach(); // blocked in getline: the process ends anyway
}

void xmouse::ui::tty::Tui::start(void)
{
    this->print_("xmouse " XMOUSE_VERSION " - " + security::tls_summary());
    this->status_();
    this->print_("type 'help' for the commands");
    this->_reader = std::jthread([this](std::stop_token stop) {
        std::string line;
        while (!stop.stop_requested() && std::getline(std::cin, line)) {
            const QString text = QString::fromStdString(line).trimmed();
            QMetaObject::invokeMethod(this, [this, text]() {this->command_(text);}, Qt::QueuedConnection);
        }
    });
}

void xmouse::ui::tty::Tui::help_(void) const
{
    this->print_(
        "  status                     network, code, link, cursors\n"
        "  members                    the members, online or not\n"
        "  layout                     where the screens are\n"
        "  create [name]              new network with this device\n"
        "  join <address|link> [code] join a network (code shown on a member)\n"
        "  accept | refuse            answer a device that gave the right code\n"
        "  deny                       refuse the access to this panel asked by a foreign cursor\n"
        "  code                       new pairing code\n"
        "  place <member> <x> <y>     position of a device in the layout\n"
        "  private on|off             refuse every foreign cursor\n"
        "  remove <member>            remove a member (the key of the network changes)\n"
        "  leave                      leave the network\n"
        "  back                       bring the cursor of this device back\n"
        "  ping | laser on|off        \"look here\" wave / laser trail of my cursor\n"
        "  follow <member>|off        point at the cursor of a member (panel overlays, pads)\n"
        "  profile [save|apply|delete <name>]  saved layouts of the network\n"
        "  link <member> <url>        open a web link on a member\n"
        "  send <member> <file>...    send files to a member (its Downloads/xmouse)\n"
        "  transfers                  files being sent / received\n"
        "  pads | padlink             phones / web pads of this device, the link that pairs a new one\n"
        "  pad accept|refuse|remove <name>  answer a new pad, forget one\n"
        "  quit");
}

void xmouse::ui::tty::Tui::status_(void) const
{
    if (!this->_hub.group().joined()) {
        this->print_("not in a network: 'create [name]' or 'join <address> <code>'");
        return;
    }
    this->print_("network  " + this->_hub.state().value("net/name").toString(this->_hub.group().name()));
    this->print_("code     " + this->_hub.code().display() + (this->_hub.code().secondsLeft() >= 0 ? " (" + QString::number(this->_hub.code().secondsLeft()) + " s)" : ""));
    this->print_("link     " + this->_hub.joinLink());
    this->print_("edges    " + this->_hub.captureName() + (this->_hub.captureError().isEmpty() ? "" : " (" + this->_hub.captureError() + ")") + ", injection " + this->_hub.injectorName());
    this->print_("cursor   " + (this->_hub.isAway() ? "on " + this->_hub.layout().device(this->_hub.target()).name : QString("here")));
}

void xmouse::ui::tty::Tui::members_(void) const
{
    for (const core::Member& m: this->_hub.members())
        this->print_("  " + m.id.left(8) + "  " + m.name.leftJustified(20) + " " + m.os.leftJustified(8) + (m.self ? " this device" : (m.online ? (m.relayed ? " relayed" : " online ") + (m.rtt >= 0 ? QString::number(m.rtt) + " ms" : QString()) + (m.udp ? " udp" : "") : " offline")) + (m.via.isEmpty() ? QString() : " (" + m.kind + " of " + this->_hub.layout().device(m.via).name + ")"));
}

void xmouse::ui::tty::Tui::layout_(void) const
{
    for (const core::Member& m: this->_hub.members()) {
        const QRect g = this->_hub.layout().globalBounds(m.id);
        this->print_("  " + m.name.leftJustified(20) + " x " + QString::number(g.x()).rightJustified(6) + "  y " + QString::number(g.y()).rightJustified(6) + "  " + QString::number(g.width()) + " x " + QString::number(g.height()) + " (" + QString::number(m.screens.size()) + " screen(s))");
    }
}

void xmouse::ui::tty::Tui::command_(const QString& line)
{
    const QStringList args = line.split(' ', Qt::SkipEmptyParts);
    const QString cmd = args.isEmpty() ? QString() : args.front().toLower();
    const std::function<QString(const QString&)> member = [this](const QString& prefix) -> QString {
        for (const core::Member& m: this->_hub.members())
            if (m.id.startsWith(prefix) || m.name.compare(prefix, Qt::CaseInsensitive) == 0) return m.id;
        return "";
    };

    if (cmd.isEmpty()) return;
    if (cmd == "help") this->help_();
    else if (cmd == "status") this->status_();
    else if (cmd == "members") this->members_();
    else if (cmd == "layout") this->layout_();
    else if (cmd == "create") this->_hub.createNetwork(args.mid(1).join(' '));
    else if (cmd == "join" && args.size() >= 2) this->_hub.join(args[1], args.value(2));
    else if ((cmd == "accept" || cmd == "refuse") && this->_pendingJoin) this->_pendingJoin->approve(cmd == "accept");
    else if (cmd == "deny" && !this->_pendingAccess.isEmpty()) this->_hub.refuseAccess(this->_pendingAccess);
    else if (cmd == "code") {this->_hub.code().rotate(); this->print_("code " + this->_hub.code().display());}
    else if (cmd == "private" && args.size() == 2) this->_hub.setPrivateMode(args[1] == "on");
    else if (cmd == "place" && args.size() == 4 && !member(args[1]).isEmpty()) this->_hub.placeDevice(member(args[1]), QPoint(args[2].toInt(), args[3].toInt()));
    else if (cmd == "remove" && args.size() == 2 && !member(args[1]).isEmpty()) this->_hub.removeMember(member(args[1]));
    else if (cmd == "leave") this->_hub.leave();
    else if (cmd == "back") this->_hub.comeBack();
    else if (cmd == "ping") this->_hub.ping();
    else if (cmd == "laser" && args.size() == 2) this->_hub.setLaser(args[1] == "on");
    else if (cmd == "follow" && args.size() == 2) this->_hub.follow(args[1] == "off" ? QString() : member(args[1]));
    else if (cmd == "profile" && args.size() == 1) this->print_("profiles: " + (this->_hub.profiles().isEmpty() ? QString("none") : this->_hub.profiles().join(", ")));
    else if (cmd == "profile" && args.size() >= 3 && args[1] == "save") this->_hub.saveProfile(args.mid(2).join(' '));
    else if (cmd == "profile" && args.size() >= 3 && args[1] == "apply") this->_hub.applyProfile(args.mid(2).join(' '));
    else if (cmd == "profile" && args.size() >= 3 && args[1] == "delete") this->_hub.deleteProfile(args.mid(2).join(' '));
    else if (cmd == "link" && args.size() == 3 && !member(args[1]).isEmpty()) this->_hub.sendLink(member(args[1]), args[2]);
    else if (cmd == "send" && args.size() >= 3 && !member(args[1]).isEmpty()) {
        if (this->_hub.sendFiles(member(args[1]), args.mid(2)).isEmpty()) this->print_("nothing sent (offline, or no readable file)");
    } else if (cmd == "transfers") {
        for (const core::Transfer& t: this->_hub.transfers())
            this->print_("  " + QString(t.incoming ? "<- " : "-> ") + t.peerName + "  " + QString::number(t.files.size()) + " file(s)  " + QString::number(t.total > 0 ? t.done * 100 / t.total : 100) + "%");
        if (this->_hub.transfers().isEmpty()) this->print_("no transfer");
    } else if (cmd == "pads") {
        for (const core::Pad& p: this->_hub.pads())
            this->print_("  " + p.name.leftJustified(20) + " " + p.kind.leftJustified(6) + (p.control || p.display ? " connected" : " offline") + (p.screenOn ? " (screen " + QString::number(p.screen.width()) + " x " + QString::number(p.screen.height()) + ")" : ""));
        if (this->_hub.pads().isEmpty()) this->print_("no pad: 'padlink' gives the link to open on a phone");
    } else if (cmd == "padlink") this->print_("open on the phone / browser: " + this->_hub.padLink());
    else if (cmd == "pad" && args.size() == 2 && (args[1] == "accept" || args[1] == "refuse") && !this->_pendingPad.isEmpty()) {this->_hub.approvePad(this->_pendingPad, args[1] == "accept"); this->_pendingPad.clear();}
    else if (cmd == "pad" && args.size() >= 3 && args[1] == "remove") {
        for (const core::Pad& p: this->_hub.pads())
            if (p.name.compare(args.mid(2).join(' '), Qt::CaseInsensitive) == 0) this->_hub.removePad(p.id);
    } else if (cmd == "quit" || cmd == "exit") QCoreApplication::quit();
    else this->print_("unknown or incomplete command (help)");
}

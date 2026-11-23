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
##  @file Hub-windows.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

/*
 * A window goes to another device (dragged on an edge, or chosen in the panel) when the network switch
 * net/windowMove is on: the source pictures it (jpeg frames), the viewer shows it in a window and its mouse / keys
 * drive it back on the source. The document it shows (when known) can be opened on the viewer instead (handoff).
 *  source -> viewer {"t": "wopen", "s", "title", "w", "h", "doc", "docsize", "offer"}, Binary::WindowFrame
 *  viewer -> source {"t": "winput", "s", "k": "move" | "button" | "wheel" | "key" | "text", ...}
 *  both            {"t": "wclose", "s", "reason"}
*/

#define _Attribute
#include <utils/utils.hpp>
#include "xmouse/security/Crypto.hpp"
#include "xmouse/net/Protocol.hpp"
#include "xmouse/core/Tools.hpp"
#include "xmouse/core/Hub.hpp"
#include <QCoreApplication>
#include <QFileInfo>
#include <algorithm>
#include <QBuffer>

//----------------------------------------------------------------//
/* DEFINE */

#define FRAME_MAX_WIDTH 1920 // frames bigger than this are scaled down (the input keeps the window coordinates)
#define FRAME_MAX_HEIGHT 1200
#define FRAME_QUALITY 70 // jpeg
#define FRAME_BACKLOG (2 * 1024 * 1024) // bytes waiting on the link before a frame is skipped

//----------------------------------------------------------------//
/* SOURCE */

QList<xmouse::platform::WindowInfo> xmouse::core::Hub::windows(void) const
{
    QList<platform::WindowInfo> out;

    if (!this->_windows) return out;
    for (const platform::WindowInfo& w: this->_windows->list())
        if (w.pid != QCoreApplication::applicationPid() && !w.title.isEmpty()) out.append(w); // never the windows of xmouse
    return out;
}

QString xmouse::core::Hub::streamWindow(const QString& device, const quint64 window)
{
    std::optional<platform::WindowInfo> info;
    Stream s;
    QJsonObject open;

    if (!this->windowMoveEnabled()) {emit this->notice("Windows between devices are off for this network"); return "";}
    if (!this->_windows || device == this->_group.self() || !this->reachable(device)) return "";
    if (!this->_state.value("device/" + device).toObject().value("windows").toBool(false)) {emit this->notice(this->_layout.device(device).name + " cannot show a window"); return "";}
    info = this->_windows->info(window);
    if (!info) return "";
    s.id = security::random_id(8);
    s.peer = device;
    s.outgoing = true;
    s.window = window;
    s.title = info->title.left(200);
    s.size = info->rect.size();
    s.document = info->document.isEmpty() ? platform::document_of_process(info->pid) : info->document;
    if (!s.document.isEmpty() && QFileInfo(s.document).isFile()) {
        s.offer = this->offerFiles_({s.document}, device);
        s.documentSize = QFileInfo(s.document).size();
    } else s.document.clear();
    open = QJsonObject{{"t", "wopen"}, {"s", s.id}, {"title", s.title}, {"w", s.size.width()}, {"h", s.size.height()}};
    if (!s.offer.isEmpty()) {
        open["doc"] = QFileInfo(s.document).fileName();
        open["docsize"] = QString::number(s.documentSize);
        open["offer"] = s.offer;
    }
    this->_streams[s.id] = s;
    this->sendTo_(device, open);
    if (!this->_streamTimer.isActive()) this->_streamTimer.start();
    this->_log.info("windows", device, "window \"" + s.title + "\" streamed to " + this->_layout.device(device).name);
    emit this->notice("\"" + s.title + "\" shown on " + this->_layout.device(device).name);
    return s.id;
}

void xmouse::core::Hub::pumpStreams_(void)
{
    bool any = false;

    for (const QString& id: this->_streams.keys()) {
        auto it = this->_streams.find(id);
        std::optional<platform::WindowInfo> info;
        QImage frame;
        QByteArray jpeg;
        QBuffer buffer(&jpeg);

        if (it == this->_streams.end() || !it->outgoing) continue;
        any = true;
        if (!this->_windows || this->pendingTo_(it->peer) > FRAME_BACKLOG) continue; // a slow link: this frame is skipped
        info = this->_windows->info(it->window);
        if (!info) {this->stopStream(id); continue;} // closed on the source
        frame = this->_windows->capture(it->window);
        if (frame.isNull()) continue;
        it->size = info->rect.size();
        if (frame.width() > FRAME_MAX_WIDTH || frame.height() > FRAME_MAX_HEIGHT) frame = frame.scaled(FRAME_MAX_WIDTH, FRAME_MAX_HEIGHT, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        buffer.open(QIODevice::WriteOnly);
        if (!frame.convertToFormat(QImage::Format_RGB32).save(&buffer, "JPG", FRAME_QUALITY) || jpeg.size() > XMOUSE_MAX_FRAME) continue;
        it->lastFrame = now_ms();
        this->sendBinaryTo_(it->peer, net::ByteWriter(net::Binary::WindowFrame, static_cast<std::size_t>(jpeg.size()) + 32).text8(id).u16(static_cast<std::uint16_t>(std::clamp(it->size.width(), 1, 65535))).u16(static_cast<std::uint16_t>(std::clamp(it->size.height(), 1, 65535))).bytes(jpeg).data());
    }
    if (!any) this->_streamTimer.stop();
}

void xmouse::core::Hub::onWindowCrossed_(const quint64 window, const QPoint& local, const QPoint& push)
{
    const QString self = this->_group.self();
    const std::optional<Hit> hit = this->_layout.hit(this->_layout.toGlobal(self, local) + push);

    // A window dragged on an edge: it goes to the device on the other side (the switch of the network is on)
    if (!this->windowMoveEnabled() || !hit || hit->device == self || !this->reachable(hit->device)) return;
    for (const Stream& s: this->_streams)
        if (s.outgoing && s.window == window && s.peer == hit->device) return; // already there
    (void)this->streamWindow(hit->device, window);
}

void xmouse::core::Hub::stopStream(const QString& id)
{
    const Stream s = this->_streams.take(id);

    if (s.id.isEmpty()) return;
    this->sendTo_(s.peer, QJsonObject{{"t", "wclose"}, {"s", id}});
    if (s.outgoing) this->_log.info("windows", s.peer, "window \"" + s.title + "\" no longer streamed");
    emit this->streamEnded(id);
}

//----------------------------------------------------------------//
/* VIEWER */

void xmouse::core::Hub::onStreamMessage_(const QString& id, const QJsonObject& o)
{
    const QString type = o.value("t").toString();
    const QString sid = o.value("s").toString().left(32);
    auto it = this->_streams.find(sid);

    if (type == "wopen") {
        Stream s;
        if (sid.isEmpty() || this->_streams.contains(sid)) return;
        if (!this->windowMoveEnabled() || !this->_config.rightsOf(id).windows || !is_gui()) {
            this->sendTo_(id, QJsonObject{{"t", "wclose"}, {"s", sid}, {"reason", !is_gui() ? "no display there" : "refused there"}});
            return;
        }
        s.id = sid;
        s.peer = id;
        s.outgoing = false;
        s.title = o.value("title").toString().left(200);
        s.size = QSize(o.value("w").toInt(), o.value("h").toInt());
        s.document = safe_file_name(o.value("doc").toString());
        s.documentSize = o.value("docsize").toString().toLongLong();
        s.offer = o.value("offer").toString().left(32);
        if (o.value("doc").toString().isEmpty()) {s.document.clear(); s.offer.clear();}
        s.lastFrame = now_ms();
        this->_streams[sid] = s;
        this->_log.info("windows", id, "window \"" + s.title + "\" of " + this->_layout.device(id).name + " shown here");
        emit this->streamStarted(sid, this->_layout.device(id).name, s.title, s.document);
        return;
    }
    if (it == this->_streams.end() || it->peer != id) return;
    if (type == "wclose") {
        const QString reason = o.value("reason").toString();
        this->_streams.erase(it);
        if (!reason.isEmpty()) emit this->notice("Window: " + reason);
        emit this->streamEnded(sid);
        return;
    }

    // Input of the viewer on the window of this device (source): its rights here decide
    if (type == "winput" && it->outgoing && this->_injector && this->_windows && this->_config.rightsOf(id).windows && !this->_config.settings().privateMode) {
        const std::optional<platform::WindowInfo> info = this->_windows->info(it->window);
        const QString kind = o.value("k").toString();
        const bool release = (kind == "button" || kind == "key") && !o.value("down").toBool();
        QPoint at;

        if (!release && this->injectionHeld_(id)) return; // my cursor away under a capture: it would take it there

        if (!info) {this->stopStream(sid); return;}
        if (!it->activated || kind == "button") {this->_windows->activate(it->window); it->activated = true;}
        at = info->rect.topLeft() + QPoint(std::clamp(o.value("x").toInt(), 0, std::max(0, info->rect.width() - 1)), std::clamp(o.value("y").toInt(), 0, std::max(0, info->rect.height() - 1)));
        this->_lastInject = now_ms();
        if (kind == "move" || kind == "button" || kind == "wheel") this->_injector->mouseMove(this->injectPoint_(at).x(), this->injectPoint_(at).y());
        if (kind == "button") {
            const int b = o.value("b").toInt(-1);
            if (b >= 0 && b <= static_cast<int>(net::MouseButton::Forward)) this->_injector->mouseButton(static_cast<net::MouseButton>(b), o.value("down").toBool());
        } else if (kind == "wheel") this->_injector->mouseWheel(std::clamp(o.value("dx").toInt(), -32000, 32000), std::clamp(o.value("dy").toInt(), -32000, 32000));
        else if (kind == "key") this->_injector->key(o.value("code").toString().left(32), o.value("down").toBool());
        else if (kind == "text") this->_injector->text(o.value("text").toString().left(4096));
    }
}

void xmouse::core::Hub::onWindowFrame_(const QString& id, const QByteArray& data)
{
    net::ByteReader r(data);
    const QString sid = r.text8();
    const int w = r.u16();
    const int h = r.u16();
    const QByteArray jpeg = r.rest();
    auto it = this->_streams.find(sid);
    QImage frame;

    if (!r.ok() || it == this->_streams.end() || it->outgoing || it->peer != id) return;
    if (!frame.loadFromData(jpeg, "JPG")) return;
    it->lastFrame = now_ms();
    it->size = QSize(w, h);
    emit this->streamFrame(sid, frame, it->size);
}

void xmouse::core::Hub::streamInput(const QString& id, const QJsonObject& input)
{
    const auto it = this->_streams.constFind(id);
    QJsonObject message = input;

    if (it == this->_streams.constEnd() || it->outgoing) return;
    message["t"] = "winput";
    message["s"] = id;
    this->sendTo_(it->peer, message);
}

void xmouse::core::Hub::openStreamDocument(const QString& id)
{
    const auto it = this->_streams.constFind(id);

    if (it == this->_streams.constEnd() || it->outgoing || it->offer.isEmpty()) return;
    this->fetch_(it->peer, it->offer, {FileEntry{it->document, it->documentSize, ""}}, "open");
}

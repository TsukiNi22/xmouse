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
##  @file Hub-files.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xmouse/security/Crypto.hpp"
#include "xmouse/net/Protocol.hpp"
#include "xmouse/core/Tools.hpp"
#include "xmouse/core/Hub.hpp"
#include <QDesktopServices>
#include <QStandardPaths>
#include <QClipboard>
#include <QJsonArray>
#include <QMimeData>
#include <QFileInfo>
#include <QBuffer>
#include <QImage>
#include <QDir>
#include <QUrl>

//----------------------------------------------------------------//
/* DEFINE */

#define CHUNKS_PER_PUMP 16 // chunks written per turn of the event loop and transfer (the ui stays alive)
#define MIB (1024LL * 1024LL)

//----------------------------------------------------------------//
/* TOOLS */

_cold static QString content_hash(const QString& mime, const QByteArray& data)
{
    return QString::fromLatin1(xmouse::security::sha256(mime.toUtf8() + '|' + data).toHex());
}

_cold static QString size_text(const qint64 bytes)
{
    if (bytes < 1024) return QString::number(bytes) + " B";
    if (bytes < MIB) return QString::number(bytes / 1024.0, 'f', 1) + " KiB";
    if (bytes < 1024 * MIB) return QString::number(static_cast<double>(bytes) / MIB, 'f', 1) + " MiB";
    return QString::number(static_cast<double>(bytes) / (1024 * MIB), 'f', 2) + " GiB";
}

_cold static QJsonArray files_to_json(const QList<xmouse::core::FileEntry>& files)
{
    QJsonArray out;

    for (const xmouse::core::FileEntry& f: files)
        out.append(QJsonObject{{"name", f.name}, {"size", QString::number(f.size)}});
    return out;
}

//----------------------------------------------------------------//
/* CLIPBOARD */

void xmouse::core::Hub::onLocalClipboard_(void)
{
    QJsonObject message{{"t", "clip"}};
    QByteArray data;
    QString mime;
    QString hash;

    this->clipToPads_();
    if (!this->_config.settings().clipboard || !this->_group.joined() || this->_mesh.ids().isEmpty()) return;
    if (this->_clipboard.backend() == "qt") {
        const QMimeData* m = QGuiApplication::clipboard()->mimeData();
        if (!m) return;

        // Files copied in a file manager: offered, the members fetch them (up to their limit)
        if (m->hasUrls() && !m->urls().isEmpty()) {
            QStringList paths;
            QString offer;
            for (const QUrl& url: m->urls())
                if (url.isLocalFile() && QFileInfo(url.toLocalFile()).isFile()) paths.append(url.toLocalFile());
            if (!paths.isEmpty()) {
                hash = content_hash("files", paths.join('\n').toUtf8());
                if (hash == this->_lastClipboardHash) return;
                this->_lastClipboardHash = hash;
                offer = this->offerFiles_(paths, "");
                if (offer.isEmpty()) return;
                for (const QString& id: this->_state.ids("device/")) {
                    if (id == this->_group.self() || !this->_state.value("device/" + id).toObject().value("via").toString().isEmpty() || !this->reachable(id)) continue;
                    this->sendTo_(id, QJsonObject{{"t", "files"}, {"offer", offer}, {"reason", "clip"}, {"files", files_to_json(this->_offers.value(offer).files)}, {"name", this->_config.deviceName()}});
                }
                return;
            }
        }
        if (m->hasImage()) {
            const QImage image = qvariant_cast<QImage>(m->imageData());
            QBuffer buffer(&data);
            buffer.open(QIODevice::WriteOnly);
            if (image.isNull() || !image.save(&buffer, "PNG")) return;
            mime = "image/png";
        } else if (m->hasText()) {
            data = m->text().toUtf8();
            mime = "text/plain";
        } else return;
    } else {
        data = this->_clipboard.text().toUtf8();
        mime = "text/plain";
    }
    if (data.isEmpty() || data.size() > XMOUSE_MAX_CLIPBOARD) return;
    hash = content_hash(mime, data);
    if (hash == this->_lastClipboardHash) return; // set here by a member: not sent back
    this->_lastClipboardHash = hash;
    message["mime"] = mime;
    message["data"] = QString::fromLatin1(data.toBase64());
    for (const QString& id: this->_state.ids("device/")) {
        if (id == this->_group.self() || !this->_state.value("device/" + id).toObject().value("via").toString().isEmpty()) continue;
        if (this->reachable(id)) this->sendTo_(id, message); // the relayed members too
    }
}

void xmouse::core::Hub::onRemoteClipboard_(const QString& id, const QJsonObject& o)
{
    const QString mime = o.value("mime").toString();
    const QByteArray data = QByteArray::fromBase64(o.value("data").toString().toLatin1());

    if (!this->_config.rightsOf(id).clipboard || data.isEmpty() || data.size() > XMOUSE_MAX_CLIPBOARD) return;
    this->_lastClipboardHash = content_hash(mime, data);
    if (mime == "image/png") {
        QImage image;
        if (!image.loadFromData(data, "PNG") || this->_clipboard.backend() != "qt") return;
        QGuiApplication::clipboard()->setImage(image);
    } else if (mime == "text/plain") {
        if (this->_clipboard.backend() == "qt") QGuiApplication::clipboard()->setText(QString::fromUtf8(data));
        else this->_clipboard.setText(QString::fromUtf8(data));
    }
}

//----------------------------------------------------------------//
/* OFFERS */

QString xmouse::core::Hub::offerFiles_(const QStringList& paths, const QString& to)
{
    Offer offer;

    for (const QString& path: paths) {
        const QFileInfo info(path);
        if (!info.isFile() || !info.isReadable() || offer.files.size() >= XMOUSE_MAX_FILES) continue;
        offer.files.append(FileEntry{info.fileName(), info.size(), info.absoluteFilePath()});
    }
    if (offer.files.isEmpty()) return "";
    offer.id = security::random_id(8);
    offer.to = to;
    offer.created = now_ms();
    this->_offers[offer.id] = offer;
    return offer.id;
}

QString xmouse::core::Hub::sendFiles(const QString& device, const QStringList& paths, const QString& reason)
{
    QString offer;
    qint64 total = 0;

    if (!this->_group.joined() || device == this->_group.self() || !this->reachable(device)) return "";
    offer = this->offerFiles_(paths, device);
    if (offer.isEmpty()) {emit this->notice("Nothing to send (folders are not sent, only files)"); return "";}
    for (const FileEntry& f: this->_offers.value(offer).files)
        total += f.size;
    this->sendTo_(device, QJsonObject{{"t", "files"}, {"offer", offer}, {"reason", reason}, {"files", files_to_json(this->_offers.value(offer).files)}, {"name", this->_config.deviceName()}});
    this->_log.info("files", device, "offered " + QString::number(this->_offers.value(offer).files.size()) + " file(s), " + size_text(total));
    emit this->notice("Sending " + QString::number(this->_offers.value(offer).files.size()) + " file(s) to " + this->_layout.device(device).name);
    return offer;
}

QString xmouse::core::Hub::downloadDir(void) const
{
    QString dir = this->_config.settings().downloadDir;

    if (dir.isEmpty()) {
        const QString downloads = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        dir = (downloads.isEmpty() ? QDir::homePath() : downloads) + "/xmouse";
    }
    (void)QDir().mkpath(dir);
    return dir;
}

QList<xmouse::core::Transfer> xmouse::core::Hub::transfers(void) const
{
    return this->_transfers.values();
}

//----------------------------------------------------------------//
/* RECEIVER */

void xmouse::core::Hub::onFilesOffer_(const QString& id, const QJsonObject& o)
{
    const QString offer = o.value("offer").toString().left(32);
    const QString reason = o.value("reason").toString();
    const QString name = this->_layout.device(id).name.isEmpty() ? o.value("name").toString().left(64) : this->_layout.device(id).name;
    QList<FileEntry> files;
    qint64 total = 0;

    if (!this->_config.rightsOf(id).files || offer.isEmpty() || (reason == "clip" && !is_gui())) return; // no clipboard without a display (a drop still lands)
    for (const QJsonValue& v: o.value("files").toArray()) {
        const QJsonObject f = v.toObject();
        const qint64 size = f.value("size").toString().toLongLong();
        if (size < 0 || files.size() >= XMOUSE_MAX_FILES) continue;
        files.append(FileEntry{safe_file_name(f.value("name").toString()), size, ""});
        total += size;
    }
    if (files.isEmpty()) return;
    if (total > static_cast<qint64>(this->_config.settings().filesLimit) * MIB) {
        emit this->notice(name + " offers " + size_text(total) + ": over the limit of this device");
        this->sendTo_(id, QJsonObject{{"t", "fend"}, {"offer", offer}, {"error", "over the limit of the receiver"}});
        return;
    }
    if (reason == "clip") {
        // The latest copy only: a newer one replaces the download of the previous one
        if (total > static_cast<qint64>(this->_config.settings().clipboardFilesLimit) * MIB) {
            emit this->notice(name + " copied " + QString::number(files.size()) + " file(s), " + size_text(total) + ": too big for the clipboard, drag them on this device instead");
            return;
        }
        if (!this->_clipboardTransfer.isEmpty()) this->failTransfer_(this->_clipboardTransfer, "replaced by a newer copy", true);
        this->_clipboardTransfer = offer;
    }
    this->fetch_(id, offer, files, reason == "clip" || reason == "open" ? reason : "drop");
}

void xmouse::core::Hub::fetch_(const QString& peer, const QString& offer, const QList<FileEntry>& files, const QString& reason)
{
    Transfer t;

    if (this->_transfers.contains(offer)) return;
    t.id = offer;
    t.peer = peer;
    t.peerName = this->_layout.device(peer).name;
    t.reason = reason;
    t.incoming = true;
    t.files = files;
    t.started = now_ms();
    t.touched = t.started;
    t.dir = reason == "clip" ? Config::dataDir() + "/clipboard/" + offer : this->downloadDir();
    (void)QDir().mkpath(t.dir);
    for (const FileEntry& f: files)
        t.total += f.size;

    // Empty files are written at once (no chunk will come for them)
    while (t.index < t.files.size() && t.files[t.index].size == 0) {
        QFile empty(unique_path(t.dir, t.files[t.index].name));
        t.files[t.index].path = empty.fileName();
        (void)empty.open(QIODevice::WriteOnly);
        ++t.index;
    }
    this->_transfers[offer] = t;
    this->_log.info("files", peer, "receiving " + QString::number(files.size()) + " file(s), " + size_text(t.total) + " (" + reason + ")");
    if (t.index >= t.files.size()) {this->finishTransfer_(offer); return;}
    this->sendTo_(peer, QJsonObject{{"t", "fget"}, {"offer", offer}, {"index", t.index}});
    emit this->transfersChanged();
}

void xmouse::core::Hub::onFileChunk_(const QString& id, const QByteArray& data)
{
    net::ByteReader r(data);
    const QString offer = r.text8();
    const int index = r.u16();
    const quint32 high = r.u32();
    const quint32 low = r.u32();
    const qint64 offset = (static_cast<qint64>(high) << 32) | low;
    const QByteArray chunk = r.rest();
    auto it = this->_transfers.find(offer);

    if (!r.ok() || it == this->_transfers.end() || !it->incoming || it->peer != id) return;
    if (index != it->index || offset != it->offset || index >= it->files.size()) {this->failTransfer_(offer, "chunk out of order", true); return;}
    if (offset + chunk.size() > it->files[index].size) {this->failTransfer_(offer, "more data than announced", true); return;}
    if (!it->file) {
        it->files[index].path = unique_path(it->dir, it->files[index].name);
        it->file = std::make_shared<QFile>(it->files[index].path);
        if (!it->file->open(QIODevice::WriteOnly)) {this->failTransfer_(offer, "cannot write " + it->files[index].path, true); return;}
    }
    if (it->file->write(chunk) != chunk.size()) {this->failTransfer_(offer, "disk full", true); return;}
    it->offset += chunk.size();
    it->done += chunk.size();
    it->touched = now_ms();

    // The file is complete: the next ones (the empty ones written at once)
    if (it->offset >= it->files[index].size) {
        it->file->close();
        it->file.reset();
        it->offset = 0;
        ++it->index;
        while (it->index < it->files.size() && it->files[it->index].size == 0) {
            QFile empty(unique_path(it->dir, it->files[it->index].name));
            it->files[it->index].path = empty.fileName();
            (void)empty.open(QIODevice::WriteOnly);
            ++it->index;
        }
        if (it->index >= it->files.size()) {this->finishTransfer_(offer); return;}
    }
    if (it->done % (4 * MIB) < chunk.size()) emit this->transfersChanged(); // progress every 4 MiB
}

void xmouse::core::Hub::finishTransfer_(const QString& id)
{
    const Transfer t = this->_transfers.take(id);
    QStringList paths;
    QList<QUrl> urls;

    if (t.file) t.file->close();
    for (const FileEntry& f: t.files) {
        paths.append(f.path);
        urls.append(QUrl::fromLocalFile(f.path));
    }
    this->_log.info("files", t.peer, "received " + QString::number(t.files.size()) + " file(s), " + size_text(t.total) + " in " + t.dir);
    if (t.reason == "clip") {
        // In the clipboard as copied files: a paste in a file manager drops them there
        QMimeData* mime = new QMimeData();
        QByteArray gnome = "copy";
        for (const QUrl& url: urls)
            gnome += "\n" + url.toEncoded();
        mime->setUrls(urls);
        mime->setData("x-special/gnome-copied-files", gnome);
        this->_lastClipboardHash = content_hash("files", paths.join('\n').toUtf8());
        if (this->_clipboardTransfer == id) this->_clipboardTransfer.clear();
        QGuiApplication::clipboard()->setMimeData(mime);
        emit this->notice(QString::number(paths.size()) + " file(s) of " + t.peerName + " in the clipboard");
    } else {
        if (t.reason == "open" && !paths.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(paths.front()));
        emit this->filesReceived(t.peerName, paths, t.dir);
        emit this->notice(QString::number(paths.size()) + " file(s) received from " + t.peerName + " (" + t.dir + ")");
    }
    emit this->transfersChanged();
}

void xmouse::core::Hub::failTransfer_(const QString& id, const QString& reason, const bool tell)
{
    auto it = this->_transfers.find(id);

    if (it == this->_transfers.end()) return;
    if (it->file) {
        it->file->close();
        if (it->incoming) (void)it->file->remove(); // a partial file is not left behind
    }
    if (tell) this->sendTo_(it->peer, QJsonObject{{"t", "fend"}, {"offer", it->incoming ? it->id : id.section(':', 0, 0)}, {"error", reason}});
    this->_log.warn("files", it->peer, "transfer stopped: " + reason);
    if (this->_clipboardTransfer == id) this->_clipboardTransfer.clear();
    this->_transfers.erase(it);
    emit this->transfersChanged();
}

void xmouse::core::Hub::cancelTransfer(const QString& id)
{
    this->failTransfer_(id, "cancelled", true);
}

//----------------------------------------------------------------//
/* SENDER */

void xmouse::core::Hub::onFileGet_(const QString& id, const QJsonObject& o)
{
    const QString offer = o.value("offer").toString();
    const int index = o.value("index").toInt();
    const QString key = offer + ":" + id;
    auto it = this->_offers.find(offer);
    Transfer t;

    if (it == this->_offers.end() || (!it->to.isEmpty() && it->to != id) || index < 0 || index >= it->files.size()) {
        this->sendTo_(id, QJsonObject{{"t", "fend"}, {"offer", offer}, {"error", "offer expired"}});
        return;
    }
    t.id = key;
    t.peer = id;
    t.peerName = this->_layout.device(id).name;
    t.incoming = false;
    t.files = it->files;
    t.index = index;
    t.started = now_ms();
    t.touched = t.started;
    for (const FileEntry& f: t.files)
        t.total += f.size;
    this->_transfers[key] = t;
    if (!this->_transferPump.isActive()) this->_transferPump.start();
    emit this->transfersChanged();
}

void xmouse::core::Hub::pumpTransfers_(void)
{
    bool active = false;

    for (const QString& key: this->_transfers.keys()) {
        auto it = this->_transfers.find(key);
        if (it == this->_transfers.end() || it->incoming) continue;
        active = true;
        for (int n = 0; n < CHUNKS_PER_PUMP && this->pendingTo_(it->peer) < XMOUSE_FILE_WINDOW; ++n) {
            QByteArray chunk;
            net::ByteWriter w(net::Binary::FileChunk, XMOUSE_FILE_CHUNK + 64);

            while (it->index < it->files.size() && it->files[it->index].size == 0) // empty files: written by the receiver at once
                ++it->index;
            if (it->index >= it->files.size()) {
                const QString peer = it->peer;
                this->sendTo_(peer, QJsonObject{{"t", "fend"}, {"offer", key.section(':', 0, 0)}});
                this->_log.info("files", peer, "sent " + QString::number(it->files.size()) + " file(s), " + size_text(it->total));
                this->_transfers.erase(it);
                emit this->transfersChanged();
                break;
            }
            if (!it->file) {
                it->file = std::make_shared<QFile>(it->files[it->index].path);
                if (!it->file->open(QIODevice::ReadOnly) || !it->file->seek(it->offset)) {this->failTransfer_(key, "cannot read " + it->files[it->index].name, true); break;}
            }
            chunk = it->file->read(std::min<qint64>(XMOUSE_FILE_CHUNK, it->files[it->index].size - it->offset));
            if (chunk.isEmpty()) {this->failTransfer_(key, it->files[it->index].name + " changed while sent", true); break;}
            w.text8(key.section(':', 0, 0)).u16(static_cast<std::uint16_t>(it->index)).u32(static_cast<std::uint32_t>(static_cast<quint64>(it->offset) >> 32)).u32(static_cast<std::uint32_t>(it->offset & 0xffffffff));
            w.bytes(chunk);
            this->sendBinaryTo_(it->peer, w.data());
            it->offset += chunk.size();
            it->done += chunk.size();
            it->touched = now_ms();
            if (it->offset >= it->files[it->index].size) {
                it->file->close();
                it->file.reset();
                it->offset = 0;
                ++it->index;
            }
        }
    }
    if (!active) this->_transferPump.stop();
}

void xmouse::core::Hub::onFileEnd_(const QString& id, const QJsonObject& o)
{
    const QString offer = o.value("offer").toString();
    const QString error = o.value("error").toString();

    // The other side stopped: the receiver of an offer, or the sender of one
    if (this->_transfers.contains(offer) && this->_transfers.value(offer).peer == id) {
        if (!error.isEmpty()) {this->failTransfer_(offer, error, false); emit this->notice("Files of " + this->_layout.device(id).name + ": " + error);}
        return;
    }
    if (this->_transfers.contains(offer + ":" + id) && !error.isEmpty()) this->failTransfer_(offer + ":" + id, error, false);
}

//----------------------------------------------------------------//
/* LINKS */

void xmouse::core::Hub::sendLink(const QString& device, const QString& url)
{
    const QUrl parsed = QUrl::fromUserInput(url.trimmed());

    if (!parsed.isValid() || (parsed.scheme() != "https" && parsed.scheme() != "http") || !this->reachable(device)) return;
    this->sendTo_(device, QJsonObject{{"t", "link"}, {"url", parsed.toString()}});
    this->_log.info("links", device, "link sent: " + parsed.toString());
    emit this->notice("Link sent to " + this->_layout.device(device).name);
}

void xmouse::core::Hub::onLink_(const QString& id, const QJsonObject& o)
{
    const QUrl url(o.value("url").toString().left(4096));
    const QString name = this->_layout.device(id).name;
    const QString policy = this->_config.settings().links;

    if (!url.isValid() || (url.scheme() != "https" && url.scheme() != "http")) return; // web links only: never a file / an app scheme
    if (!this->_config.rightsOf(id).links || policy == "never") {this->_log.info("links", id, "link refused: " + url.toString()); return;}
    this->_log.info("links", id, "link received: " + url.toString());
    if (policy == "open" && is_gui()) {
        QDesktopServices::openUrl(url);
        emit this->notice(name + " opened a link here");
        return;
    }
    emit this->linkReceived(id, name, url.toString());
}

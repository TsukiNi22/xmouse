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
##  @file Group.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#define _Exception
#include <utils/utils.hpp>
#include "xmouse/security/Crypto.hpp"
#include "xmouse/security/Group.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QFile>

// Read a json object file ({} when missing, throws Group when unreadable / broken)
_cold static QJsonObject read_json(const QString& path)
{
    QFile file(path);
    QJsonParseError error;
    QJsonObject o;

    if (!file.exists()) return {};
    if (!file.open(QIODevice::ReadOnly)) _unlikely
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Group, "Cannot read " + path.toStdString());
    o = QJsonDocument::fromJson(file.readAll(), &error).object();
    if (error.error != QJsonParseError::NoError) _unlikely
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Group, path.toStdString() + ": " + error.errorString().toStdString());
    return o;
}

_cold static void write_json(const QString& path, const QJsonObject& o)
{
    QSaveFile file(path);

    if (!file.open(QIODevice::WriteOnly)) _unlikely
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Group, "Cannot write " + path.toStdString());
    file.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (!file.commit()) _unlikely
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Group, "Cannot write " + path.toStdString());
}

void xmouse::security::Group::load(void)
{
    const QJsonObject device = read_json(this->_dir + "/device.json");
    const QJsonObject group = read_json(this->_dir + "/group.json");

    // Device id: created once, never changed (the others know this device by it)
    this->_self = device.value("id").toString();
    if (this->_self.size() != 24) {
        this->_self = random_id(12);
        write_json(this->_dir + "/device.json", QJsonObject{{"id", this->_self}});
    }

    // Network
    this->_id = group.value("id").toString();
    this->_name = group.value("name").toString();
    this->_key = QByteArray::fromBase64(group.value("key").toString().toLatin1());
    this->_previous = QByteArray::fromBase64(group.value("previous").toString().toLatin1());
    this->_epoch = group.value("epoch").toInt(0);
    if (!this->_id.isEmpty() && this->_key.size() != XMOUSE_GROUP_KEY_SIZE) _unlikely
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Group, "Invalid key in " + (this->_dir + "/group.json").toStdString());
}

void xmouse::security::Group::save(void) const
{
    if (this->_id.isEmpty()) {(void)QFile::remove(this->_dir + "/group.json"); return;}
    write_json(this->_dir + "/group.json", QJsonObject{
        {"id", this->_id},
        {"name", this->_name},
        {"key", QString::fromLatin1(this->_key.toBase64())},
        {"previous", QString::fromLatin1(this->_previous.toBase64())},
        {"epoch", this->_epoch},
    });
}

void xmouse::security::Group::create(const QString& name)
{
    this->_id = random_id(12);
    this->_name = name.trimmed().left(64);
    this->_key = random_bytes(XMOUSE_GROUP_KEY_SIZE);
    this->_previous.clear();
    this->_epoch = 1;
    this->save();
}

void xmouse::security::Group::adopt(const QString& id, const QString& name, const QByteArray& key, const int epoch)
{
    if (id.isEmpty() || key.size() != XMOUSE_GROUP_KEY_SIZE) _unlikely
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Group, "Invalid network given by the member");
    this->_id = id.left(64);
    this->_name = name.left(64);
    this->_key = key;
    this->_previous.clear();
    this->_epoch = epoch;
    this->save();
}

void xmouse::security::Group::rekey(const QByteArray& key, const int epoch)
{
    if (key.size() != XMOUSE_GROUP_KEY_SIZE || epoch <= this->_epoch) return; // an old or broken key is never taken
    this->_previous = this->_key;
    this->_key = key;
    this->_epoch = epoch;
    this->save();
}

void xmouse::security::Group::leave(void)
{
    this->_id.clear();
    this->_name.clear();
    this->_key.fill('\0');
    this->_key.clear();
    this->_previous.fill('\0');
    this->_previous.clear();
    this->_epoch = 0;
    this->save();
}

QByteArray xmouse::security::Group::previousProof(const QByteArray& transcript) const
{
    if (!this->hasPrevious()) return {};
    return hmac_sha256(this->_previous, transcript);
}

QString xmouse::security::Group::tag(void) const
{
    if (this->_id.isEmpty()) return "";
    return QString::fromLatin1(sha256(("xmouse-group|" + this->_id).toUtf8()).left(8).toHex());
}

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
##  @file Config.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#ifdef _WIN32
    #include <windows.h>
#endif
#define _Attribute
#define _Exception
#include <utils/utils.hpp>
#include "xmouse/core/Config.hpp"
#include <QCoreApplication>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHostInfo>
#include <QSaveFile>
#include <QFileInfo>
#include <algorithm>
#include <QString>
#include <QFile>
#include <QDir>

QString xmouse::core::exposure_name(const Exposure exposure)
{
    switch (exposure) {
        case Exposure::Lan:         return "lan";
        case Exposure::Tailscale:   return "tailscale";
        case Exposure::Upnp:        return "upnp";
        case Exposure::Bore:        return "bore";
        case Exposure::Cloudflared: return "cloudflared";
        case Exposure::Ngrok:       return "ngrok";
        case Exposure::Manual:      return "manual";
    }
    return "lan";
}

xmouse::core::Exposure xmouse::core::exposure_from(const QString& name)
{
    const QString n = name.toLower();

    if (n == "tailscale")   return Exposure::Tailscale;
    if (n == "upnp")        return Exposure::Upnp;
    if (n == "bore")        return Exposure::Bore;
    if (n == "cloudflared") return Exposure::Cloudflared;
    if (n == "ngrok")       return Exposure::Ngrok;
    if (n == "manual")      return Exposure::Manual;
    return Exposure::Lan;
}

QJsonObject xmouse::core::rights_to_json(const Rights& rights)
{
    return QJsonObject{{"mouse", rights.mouse}, {"keyboard", rights.keyboard}, {"clipboard", rights.clipboard}, {"files", rights.files}};
}

xmouse::core::Rights xmouse::core::rights_from_json(const QJsonObject& object)
{
    Rights rights;

    rights.mouse = object.value("mouse").toBool(rights.mouse);
    rights.keyboard = object.value("keyboard").toBool(rights.keyboard);
    rights.clipboard = object.value("clipboard").toBool(rights.clipboard);
    rights.files = object.value("files").toBool(rights.files);
    return rights;
}

void xmouse::core::Config::ensureDir(const QString& path)
{
    QDir dir;

    if (!dir.mkpath(path)) _unlikely
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Io, "Cannot create " + path.toStdString());
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
}

QString xmouse::core::Config::configDir(void)
{
    const QString path = qEnvironmentVariableIsSet("XMOUSE_HOME") ? qEnvironmentVariable("XMOUSE_HOME") + "/config" : QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/xmouse";

    ensureDir(path);
    return path;
}

QString xmouse::core::Config::dataDir(void)
{
    const QString path = qEnvironmentVariableIsSet("XMOUSE_HOME") ? qEnvironmentVariable("XMOUSE_HOME") + "/data" : QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/xmouse";

    ensureDir(path);
    return path;
}

QString xmouse::core::Config::logDir(void)
{
    const QString path = dataDir() + "/logs";

    ensureDir(path);
    return path;
}

QString xmouse::core::Config::executablePath(void)
{
    if (QCoreApplication::instance()) return QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath();
    #ifdef _WIN32
        wchar_t buffer[MAX_PATH] = {0};
        const DWORD size = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        return QString::fromWCharArray(buffer, static_cast<qsizetype>(size));
    #else
        return QFileInfo("/proc/self/exe").canonicalFilePath();
    #endif
}

xmouse::core::Config::Config(void)
: _path{configDir() + "/config.json"}
{
    /* Nothing */
}

QString xmouse::core::Config::deviceName(void) const
{
    const QString name = this->_settings.name.trimmed();

    if (!name.isEmpty()) return name.left(64);
    return QHostInfo::localHostName().section('.', 0, 0).left(64);
}

void xmouse::core::Config::load(void)
{
    QFile file(this->_path);
    QJsonParseError error;
    QJsonObject o;
    QJsonObject rights;
    Settings& s = this->_settings;

    if (!file.exists()) {this->save(); return;}
    if (!file.open(QIODevice::ReadOnly)) _unlikely
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Config, "Cannot read " + this->_path.toStdString());
    o = QJsonDocument::fromJson(file.readAll(), &error).object();
    if (error.error != QJsonParseError::NoError) _unlikely
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Config, this->_path.toStdString() + ": " + error.errorString().toStdString());

    // Identity
    s.name = o.value("name").toString(s.name).left(64);

    // Network
    s.port = static_cast<std::uint16_t>(std::clamp(o.value("port").toInt(s.port), 1, 65535));
    s.bind = o.value("bind").toString(s.bind);
    s.exposure = exposure_from(o.value("exposure").toString(exposure_name(s.exposure)));
    s.publicHost = o.value("publicHost").toString(s.publicHost);
    s.discovery = o.value("discovery").toBool(s.discovery);

    // Access
    s.codeLength = std::clamp(o.value("codeLength").toInt(s.codeLength), 6, 24);
    s.codeLifetime = std::max(0, o.value("codeLifetime").toInt(s.codeLifetime));
    s.joinApproval = o.value("joinApproval").toBool(s.joinApproval);
    s.maxAuthFailures = std::clamp(o.value("maxAuthFailures").toInt(s.maxAuthFailures), 1, 100);
    s.focusRequest = o.value("focusRequest").toBool(s.focusRequest);
    s.focusRequestDelay = std::clamp(o.value("focusRequestDelay").toInt(s.focusRequestDelay), 5, 3600);
    s.privateMode = o.value("privateMode").toBool(s.privateMode);
    s.rights.clear();
    rights = o.value("rights").toObject();
    for (auto it = rights.begin(); it != rights.end(); ++it)
        s.rights.insert(it.key(), rights_from_json(it.value().toObject()));

    // Input
    s.releaseShortcut = o.value("releaseShortcut").toString(s.releaseShortcut);
    s.restorePointer = o.value("restorePointer").toBool(s.restorePointer);
    s.edgeDelay = std::clamp(o.value("edgeDelay").toInt(s.edgeDelay), 0, 2000);
    s.clipboard = o.value("clipboard").toBool(s.clipboard);

    // Ui
    s.theme = o.value("theme").toString(s.theme);
    s.language = o.value("language").toString(s.language);
    s.startMinimized = o.value("startMinimized").toBool(s.startMinimized);
}

void xmouse::core::Config::save(void) const
{
    const Settings& s = this->_settings;
    QJsonObject o;
    QJsonObject rights;
    QSaveFile file(this->_path);

    for (auto it = s.rights.begin(); it != s.rights.end(); ++it)
        rights[it.key()] = rights_to_json(it.value());
    o["name"] = s.name;
    o["port"] = s.port;
    o["bind"] = s.bind;
    o["exposure"] = exposure_name(s.exposure);
    o["publicHost"] = s.publicHost;
    o["discovery"] = s.discovery;
    o["codeLength"] = s.codeLength;
    o["codeLifetime"] = s.codeLifetime;
    o["joinApproval"] = s.joinApproval;
    o["maxAuthFailures"] = s.maxAuthFailures;
    o["focusRequest"] = s.focusRequest;
    o["focusRequestDelay"] = s.focusRequestDelay;
    o["privateMode"] = s.privateMode;
    o["rights"] = rights;
    o["releaseShortcut"] = s.releaseShortcut;
    o["restorePointer"] = s.restorePointer;
    o["edgeDelay"] = s.edgeDelay;
    o["clipboard"] = s.clipboard;
    o["theme"] = s.theme;
    o["language"] = s.language;
    o["startMinimized"] = s.startMinimized;
    if (!file.open(QIODevice::WriteOnly)) _unlikely
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Config, "Cannot write " + this->_path.toStdString());
    file.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (!file.commit()) _unlikely
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Config, "Cannot write " + this->_path.toStdString());
}

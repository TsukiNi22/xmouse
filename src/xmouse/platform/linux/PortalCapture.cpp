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
##  @file PortalCapture.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xmouse/platform/linux/PortalCapture.hpp"
#include "xmouse/security/Crypto.hpp"
#include "xmouse/platform/Keymap.hpp"
#include "xmouse/core/Config.hpp"
#include <linux/input-event-codes.h>
#include <QDBusUnixFileDescriptor>
#include <QDBusConnection>
#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QDBusReply>
#include <QSaveFile>
#include <unistd.h>
#include <libei.h>
#include <dlfcn.h>
#include <QDateTime>
#include <QTimer>
#include <QFile>
#include <cmath>

//----------------------------------------------------------------//
/* DEFINE */

#define PORTAL_SERVICE "org.freedesktop.portal.Desktop"
#define PORTAL_PATH "/org/freedesktop/portal/desktop"
#define PORTAL_INPUT_CAPTURE "org.freedesktop.portal.InputCapture"
#define PORTAL_REQUEST "org.freedesktop.portal.Request"
#define PORTAL_SESSION "org.freedesktop.portal.Session"
#define CAPABILITY_KEYBOARD 1u
#define CAPABILITY_POINTER 2u
#define EI_LIBRARY "libei.so.1"
#define RESTORE_TOKEN_FILE "portal-input-capture.token" // in the data folder (0600)
#define SMOOTH_SCROLL_FACTOR 8.0 // libei smooth scroll (logical pixels) -> 1/120 of a notch
#define RECOVER_INTERVAL 300 // ms between two recoveries of a capture kept by the compositor
#define RECOVER_REARM 150 // ms between the Disable and the Enable of a recovery
#define RECOVER_SERIES 1500 // ms: a recovery within this delay of the last one is the next step of the same series

//----------------------------------------------------------------//
/* STRUCT */

// libei loaded at run time: no build dependency (vendored header), xmouse still starts where libei is missing
struct xmouse::platform::EiApi {
    void* handle = nullptr;
    decltype(&::ei_new_receiver) newReceiver = nullptr;
    decltype(&::ei_configure_name) configureName = nullptr;
    decltype(&::ei_setup_backend_fd) setupBackendFd = nullptr;
    decltype(&::ei_get_fd) getFd = nullptr;
    decltype(&::ei_dispatch) dispatch = nullptr;
    decltype(&::ei_get_event) getEvent = nullptr;
    decltype(&::ei_event_get_type) eventGetType = nullptr;
    decltype(&::ei_event_unref) eventUnref = nullptr;
    decltype(&::ei_event_get_seat) eventGetSeat = nullptr;
    void (*seatBindCapabilities)(struct ei_seat*, ...) = nullptr;
    decltype(&::ei_event_pointer_get_dx) pointerDx = nullptr;
    decltype(&::ei_event_pointer_get_dy) pointerDy = nullptr;
    decltype(&::ei_event_button_get_button) buttonButton = nullptr;
    decltype(&::ei_event_button_get_is_press) buttonIsPress = nullptr;
    decltype(&::ei_event_scroll_get_dx) scrollDx = nullptr;
    decltype(&::ei_event_scroll_get_dy) scrollDy = nullptr;
    decltype(&::ei_event_scroll_get_discrete_dx) scrollDiscreteDx = nullptr;
    decltype(&::ei_event_scroll_get_discrete_dy) scrollDiscreteDy = nullptr;
    decltype(&::ei_event_keyboard_get_key) keyboardKey = nullptr;
    decltype(&::ei_event_keyboard_get_key_is_press) keyboardIsPress = nullptr;
    decltype(&::ei_unref) unref = nullptr;
};

//----------------------------------------------------------------//
/* TOOLS */

// One symbol of libei (ok false + error at the first missing one)
template<typename T>
static void load_symbol(void* handle, T& target, const char* name, bool& ok, QString& error)
{
    target = reinterpret_cast<T>(::dlsym(handle, name));
    if (!target && ok) {ok = false; error = QString("libei: missing ") + name;}
}

// Every symbol or nothing (error says the first missing one)
_cold static xmouse::platform::EiApi* load_ei(QString& error)
{
    xmouse::platform::EiApi* api = new xmouse::platform::EiApi();
    bool ok = true;

    api->handle = ::dlopen(EI_LIBRARY, RTLD_NOW | RTLD_LOCAL);
    if (!api->handle) {error = QString(EI_LIBRARY " not found (install libei)"); delete api; return nullptr;}
    load_symbol(api->handle, api->newReceiver, "ei_new_receiver", ok, error);
    load_symbol(api->handle, api->configureName, "ei_configure_name", ok, error);
    load_symbol(api->handle, api->setupBackendFd, "ei_setup_backend_fd", ok, error);
    load_symbol(api->handle, api->getFd, "ei_get_fd", ok, error);
    load_symbol(api->handle, api->dispatch, "ei_dispatch", ok, error);
    load_symbol(api->handle, api->getEvent, "ei_get_event", ok, error);
    load_symbol(api->handle, api->eventGetType, "ei_event_get_type", ok, error);
    load_symbol(api->handle, api->eventUnref, "ei_event_unref", ok, error);
    load_symbol(api->handle, api->eventGetSeat, "ei_event_get_seat", ok, error);
    load_symbol(api->handle, api->seatBindCapabilities, "ei_seat_bind_capabilities", ok, error);
    load_symbol(api->handle, api->pointerDx, "ei_event_pointer_get_dx", ok, error);
    load_symbol(api->handle, api->pointerDy, "ei_event_pointer_get_dy", ok, error);
    load_symbol(api->handle, api->buttonButton, "ei_event_button_get_button", ok, error);
    load_symbol(api->handle, api->buttonIsPress, "ei_event_button_get_is_press", ok, error);
    load_symbol(api->handle, api->scrollDx, "ei_event_scroll_get_dx", ok, error);
    load_symbol(api->handle, api->scrollDy, "ei_event_scroll_get_dy", ok, error);
    load_symbol(api->handle, api->scrollDiscreteDx, "ei_event_scroll_get_discrete_dx", ok, error);
    load_symbol(api->handle, api->scrollDiscreteDy, "ei_event_scroll_get_discrete_dy", ok, error);
    load_symbol(api->handle, api->keyboardKey, "ei_event_keyboard_get_key", ok, error);
    load_symbol(api->handle, api->keyboardIsPress, "ei_event_keyboard_get_key_is_press", ok, error);
    load_symbol(api->handle, api->unref, "ei_unref", ok, error);
    if (!ok) {::dlclose(api->handle); delete api; return nullptr;}
    return api;
}

_hot static std::optional<xmouse::net::MouseButton> button_from_evdev(const unsigned int code)
{
    switch (code) {
        case BTN_LEFT:   return xmouse::net::MouseButton::Left;
        case BTN_RIGHT:  return xmouse::net::MouseButton::Right;
        case BTN_MIDDLE: return xmouse::net::MouseButton::Middle;
        case BTN_SIDE:   return xmouse::net::MouseButton::Back;
        case BTN_EXTRA:  return xmouse::net::MouseButton::Forward;
        default:         return std::nullopt;
    }
}

// (iiii) / (dd) structures of the portal
_cold static QVariant int4(const int a, const int b, const int c, const int d)
{
    QDBusArgument arg;

    arg.beginStructure();
    arg << a << b << c << d;
    arg.endStructure();
    return QVariant::fromValue(arg);
}

_cold static QVariant double2(const double a, const double b)
{
    QDBusArgument arg;

    arg.beginStructure();
    arg << a << b;
    arg.endStructure();
    return QVariant::fromValue(arg);
}

_cold static QPointF read_double2(const QVariant& v)
{
    const QDBusArgument arg = v.value<QDBusArgument>();
    double x = 0;
    double y = 0;

    arg.beginStructure();
    arg >> x >> y;
    arg.endStructure();
    return QPointF(x, y);
}

// Version of the InputCapture interface (2: CreateSession2 / Start with a restore token)
_cold static unsigned int portal_version(void)
{
    QDBusMessage call = QDBusMessage::createMethodCall(PORTAL_SERVICE, PORTAL_PATH, "org.freedesktop.DBus.Properties", "Get");
    QDBusMessage reply;

    call.setArguments({QString(PORTAL_INPUT_CAPTURE), QString("version")});
    reply = QDBusConnection::sessionBus().call(call, QDBus::Block, 2000);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) return 1;
    return reply.arguments().front().value<QDBusVariant>().variant().toUInt();
}

// A session handle comes as an object path (o), some portals send a string
_cold static QString object_path(const QVariant& value)
{
    if (value.canConvert<QDBusObjectPath>() && value.metaType() == QMetaType::fromType<QDBusObjectPath>()) return value.value<QDBusObjectPath>().path();
    return value.toString();
}

// Restore token of the last session: the permission of the desktop is asked once, not at every start
_cold static QString read_restore_token(void)
{
    QFile file(xmouse::core::Config::dataDir() + "/" RESTORE_TOKEN_FILE);

    if (!file.open(QIODevice::ReadOnly)) return "";
    return QString::fromUtf8(file.readAll()).trimmed().left(512);
}

_cold static void write_restore_token(const QString& token)
{
    QSaveFile file(xmouse::core::Config::dataDir() + "/" RESTORE_TOKEN_FILE);

    if (token.isEmpty() || !file.open(QIODevice::WriteOnly)) return;
    file.write(token.toUtf8());
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    (void)file.commit();
}

//----------------------------------------------------------------//
/* CLASS */

xmouse::platform::PortalCapture::PortalCapture(QObject* parent)
: IInputCapture(parent)
{
    /* Nothing */
}

xmouse::platform::PortalCapture::~PortalCapture()
{
    this->stop();
    if (this->_api) {
        ::dlclose(this->_api->handle);
        delete this->_api;
    }
}

bool xmouse::platform::PortalCapture::available(void)
{
    QDBusMessage call = QDBusMessage::createMethodCall(PORTAL_SERVICE, PORTAL_PATH, "org.freedesktop.DBus.Properties", "Get");
    void* handle = nullptr;

    call.setArguments({QString(PORTAL_INPUT_CAPTURE), QString("version")});
    if (QDBusConnection::sessionBus().call(call, QDBus::Block, 2000).type() != QDBusMessage::ReplyMessage) return false;
    handle = ::dlopen(EI_LIBRARY, RTLD_NOW | RTLD_LOCAL);
    if (!handle) return false;
    ::dlclose(handle);
    return true;
}

void xmouse::platform::PortalCapture::fail_(const QString& error)
{
    this->_error = error;
    emit this->failed(error);
}

bool xmouse::platform::PortalCapture::start(void)
{
    QDBusConnection bus = QDBusConnection::sessionBus();

    if (!this->_api) this->_api = load_ei(this->_error);
    if (!this->_api) return false;
    if (!bus.isConnected()) {this->_error = "no session bus"; return false;}
    for (const char* signal: {"Activated", "Deactivated", "Disabled", "ZonesChanged"}) {
        const QString slot = QString("1on") + signal + "_(QDBusObjectPath,QVariantMap)";
        (void)bus.connect(PORTAL_SERVICE, PORTAL_PATH, PORTAL_INPUT_CAPTURE, signal, this, slot.toLatin1().constData());
    }

    // No session yet: it is a dialog of the desktop, asked only once an edge opens on a device online (setEdges)
    QTimer::singleShot(0, this, [this]() {emit this->ready();});
    return true;
}

void xmouse::platform::PortalCapture::stop(void)
{
    if (this->_capturing) this->release(this->_position);
    if (this->_notifier) {delete this->_notifier; this->_notifier = nullptr;}
    if (this->_ei && this->_api) {this->_api->unref(static_cast<struct ei*>(this->_ei)); this->_ei = nullptr;}
    if (!this->_session.isEmpty()) {
        QDBusMessage close = QDBusMessage::createMethodCall(PORTAL_SERVICE, this->_session, PORTAL_SESSION, "Close");
        (void)QDBusConnection::sessionBus().call(close, QDBus::NoBlock);
        this->_session.clear();
    }
    this->_started = false;
    this->_requested = false;
    this->_enabled = false;
}

void xmouse::platform::PortalCapture::request_(const QString& method, const QList<QVariant>& args, const QVariantMap& options, const std::function<void(const unsigned int, const QVariantMap&)>& done)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    const QString token = "xmouse" + security::random_id(6);
    QString sender = bus.baseService().mid(1);
    QDBusMessage call = QDBusMessage::createMethodCall(PORTAL_SERVICE, PORTAL_PATH, PORTAL_INPUT_CAPTURE, method);
    QVariantMap withToken = options;
    QList<QVariant> all = args;
    QString path;

    // The request path is known before the call: subscribed first, no response can be missed
    sender.replace('.', '_');
    path = QString(PORTAL_PATH) + "/request/" + sender + "/" + token;
    this->_requests.insert(path, done);
    (void)bus.connect(PORTAL_SERVICE, path, PORTAL_REQUEST, "Response", this, SLOT(onResponse_(uint,QVariantMap)));
    withToken.insert("handle_token", token);
    if (method == "SetPointerBarriers") all.insert(1, withToken); // (session, options, barriers, zone_set)
    else all.append(withToken);
    call.setArguments(all);
    const QDBusMessage reply = bus.call(call, QDBus::Block, 10000);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        this->_requests.remove(path);
        (void)bus.disconnect(PORTAL_SERVICE, path, PORTAL_REQUEST, "Response", this, SLOT(onResponse_(uint,QVariantMap)));
        this->fail_(QString("InputCapture.") + method + ": " + reply.errorMessage());
    }
}

void xmouse::platform::PortalCapture::onResponse_(const uint code, const QVariantMap& results)
{
    const QString path = this->message().path();
    auto it = this->_requests.find(path);

    (void)QDBusConnection::sessionBus().disconnect(PORTAL_SERVICE, path, PORTAL_REQUEST, "Response", this, SLOT(onResponse_(uint,QVariantMap)));
    if (it == this->_requests.end()) return;
    const std::function<void(const unsigned int, const QVariantMap&)> done = it.value();
    this->_requests.erase(it);
    done(code, results);
}

void xmouse::platform::PortalCapture::createSession_(void)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusMessage call = QDBusMessage::createMethodCall(PORTAL_SERVICE, PORTAL_PATH, PORTAL_INPUT_CAPTURE, "CreateSession2");
    QDBusMessage reply;
    QVariantMap options;
    QVariantMap start;
    const QString token = read_restore_token();

    options.insert("session_handle_token", "xmouse" + security::random_id(6));

    // Version 1: one dialog at every start (no restore token)
    if (portal_version() < 2) {
        options.insert("capabilities", CAPABILITY_KEYBOARD | CAPABILITY_POINTER);
        this->request_("CreateSession", {QString()}, options, [this](const unsigned int code, const QVariantMap& results) {
            if (code != 0) {this->fail_(code == 1 ? "input capture refused in the dialog of the desktop" : "input capture session refused by the portal"); return;}
            this->_session = object_path(results.value("session_handle"));
            if (this->_session.isEmpty()) {this->fail_("no input capture session"); return;}
            this->connectEis_();
            this->getZones_();
        });
        return;
    }

    // Version 2: the session is created at once, then started with the token of the last run (no dialog when it is still valid)
    call.setArguments({options});
    reply = bus.call(call, QDBus::Block, 10000);
    if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {this->fail_("CreateSession2: " + reply.errorMessage()); return;}
    this->_session = object_path(qdbus_cast<QVariantMap>(reply.arguments().front()).value("session_handle"));
    if (this->_session.isEmpty()) {this->fail_("no input capture session"); return;}
    start.insert("capabilities", CAPABILITY_KEYBOARD | CAPABILITY_POINTER);
    start.insert("persist_mode", 2u); // until revoked in the settings of the desktop
    if (!token.isEmpty()) start.insert("restore_token", token);
    this->request_("Start", {QVariant::fromValue(QDBusObjectPath(this->_session)), QString()}, start, [this](const unsigned int code, const QVariantMap& results) {
        if (code != 0) {this->fail_(code == 1 ? "input capture refused in the dialog of the desktop" : "input capture session refused by the portal"); return;}
        write_restore_token(results.value("restore_token").toString()); // single use: the new one for the next run
        this->connectEis_();
        this->getZones_();
    });
}

void xmouse::platform::PortalCapture::connectEis_(void)
{
    QDBusMessage call = QDBusMessage::createMethodCall(PORTAL_SERVICE, PORTAL_PATH, PORTAL_INPUT_CAPTURE, "ConnectToEIS");
    QDBusReply<QDBusUnixFileDescriptor> reply;
    struct ei* ei = nullptr;
    int fd = -1;

    call.setArguments({QVariant::fromValue(QDBusObjectPath(this->_session)), QVariantMap()});
    reply = QDBusConnection::sessionBus().call(call, QDBus::Block, 10000);
    if (!reply.isValid()) {this->fail_("ConnectToEIS: " + reply.error().message()); return;}
    fd = ::dup(reply.value().fileDescriptor());
    if (fd < 0) {this->fail_("ConnectToEIS: no file descriptor"); return;}
    ei = this->_api->newReceiver(this);
    this->_api->configureName(ei, "xmouse");
    if (this->_api->setupBackendFd(ei, fd) != 0) {this->_api->unref(ei); this->fail_("libei: cannot use the portal connection"); return;}
    this->_ei = ei;
    this->_notifier = new QSocketNotifier(this->_api->getFd(ei), QSocketNotifier::Read, this);
    QObject::connect(this->_notifier, &QSocketNotifier::activated, this, &PortalCapture::dispatch_);
}

void xmouse::platform::PortalCapture::getZones_(void)
{
    this->request_("GetZones", {QVariant::fromValue(QDBusObjectPath(this->_session))}, {}, [this](const unsigned int code, const QVariantMap& results) {
        if (code != 0) {this->fail_("GetZones refused"); return;}
        const QDBusArgument arg = results.value("zones").value<QDBusArgument>();
        this->_zones.clear();
        arg.beginArray();
        while (!arg.atEnd()) {
            unsigned int w = 0;
            unsigned int h = 0;
            int x = 0;
            int y = 0;
            arg.beginStructure();
            arg >> w >> h >> x >> y;
            arg.endStructure();
            this->_zones.append(QRect(x, y, static_cast<int>(w), static_cast<int>(h)));
        }
        arg.endArray();
        this->_zoneSet = results.value("zone_set").toUInt();
        this->_started = true;
        this->applyBarriers_();
    });
}

void xmouse::platform::PortalCapture::setEdges(const QList<core::Edge>& edges)
{
    this->_edges = edges;
    if (!edges.isEmpty() && !this->_requested) {this->_requested = true; this->createSession_(); return;} // the first neighbour online
    if (this->_started) this->applyBarriers_();
}

void xmouse::platform::PortalCapture::applyBarriers_(void)
{
    QDBusArgument barriers;

    // Only the edges that are on a border of a zone of the compositor (a barrier inside the desktop is refused)
    this->_barriers.clear();
    barriers.beginArray(qMetaTypeId<QVariantMap>());
    for (const core::Edge& e: this->_edges) {
        QVariantMap barrier;
        const bool vertical = e.side == core::Side::Left || e.side == core::Side::Right;
        this->_barriers.append(e);
        barrier.insert("barrier_id", static_cast<unsigned int>(this->_barriers.size()));
        barrier.insert("position", vertical ? int4(e.line, e.from, e.line, e.to) : int4(e.from, e.line, e.to, e.line));
        barriers << barrier;
    }
    barriers.endArray();
    if (this->_barriers.isEmpty()) {this->setEnabled_(false); return;}
    this->request_("SetPointerBarriers", {QVariant::fromValue(QDBusObjectPath(this->_session)), QVariant::fromValue(barriers), this->_zoneSet}, {}, [this](const unsigned int code, const QVariantMap& results) {
        if (code != 0) {this->fail_("SetPointerBarriers refused"); return;}
        const QList<unsigned int> failed = qdbus_cast<QList<unsigned int>>(results.value("failed_barriers"));
        if (failed.size() == this->_barriers.size()) this->_error = "no edge of the layout is on a border of the desktop";
        else if (!failed.isEmpty()) this->_error = QString::number(failed.size()) + " edge(s) refused by the compositor";
        else this->_error.clear();
        this->setEnabled_(failed.size() < this->_barriers.size());
    });
}

void xmouse::platform::PortalCapture::setEnabled_(const bool enabled)
{
    QDBusMessage call = QDBusMessage::createMethodCall(PORTAL_SERVICE, PORTAL_PATH, PORTAL_INPUT_CAPTURE, enabled ? "Enable" : "Disable");

    if (this->_session.isEmpty() || (enabled == this->_enabled && enabled)) return;
    call.setArguments({QVariant::fromValue(QDBusObjectPath(this->_session)), QVariantMap()});
    (void)QDBusConnection::sessionBus().call(call, QDBus::NoBlock);
    this->_enabled = enabled;
}

void xmouse::platform::PortalCapture::release(const QPoint& local)
{
    QDBusMessage call = QDBusMessage::createMethodCall(PORTAL_SERVICE, PORTAL_PATH, PORTAL_INPUT_CAPTURE, "Release");
    QVariantMap options;

    if (this->_session.isEmpty() || !this->_capturing) return;
    options.insert("activation_id", this->_activation);
    options.insert("cursor_position", double2(local.x(), local.y()));
    call.setArguments({QVariant::fromValue(QDBusObjectPath(this->_session)), options});
    (void)QDBusConnection::sessionBus().call(call, QDBus::NoBlock);
    this->_capturing = false;
    this->_position = local;

    // The barriers are disabled by a release on some compositors: enabled again
    this->_enabled = false;
    if (!this->_barriers.isEmpty()) this->setEnabled_(true);
}

void xmouse::platform::PortalCapture::onActivated_(const QDBusObjectPath& session, const QVariantMap& options)
{
    const unsigned int barrier = options.value("barrier_id").toUInt();
    QPoint push;

    if (session.path() != this->_session) return;
    this->_activation = options.value("activation_id").toUInt();
    this->_position = read_double2(options.value("cursor_position")).toPoint();
    this->_capturing = true;
    if (barrier >= 1 && barrier <= static_cast<unsigned int>(this->_barriers.size())) {
        switch (this->_barriers[static_cast<int>(barrier) - 1].side) {
            case core::Side::Left:   push = QPoint(-1, 0); break;
            case core::Side::Right:  push = QPoint(1, 0);  break;
            case core::Side::Top:    push = QPoint(0, -1); break;
            case core::Side::Bottom: push = QPoint(0, 1);  break;
        }
    }
    emit this->crossed(this->_position, push);
}

void xmouse::platform::PortalCapture::onDeactivated_(const QDBusObjectPath& session, const QVariantMap& options)
{
    if (session.path() != this->_session || options.value("activation_id").toUInt() != this->_activation) return;
    if (!this->_capturing) return;
    this->_capturing = false;
    emit this->ended();
}

void xmouse::platform::PortalCapture::onDisabled_(const QDBusObjectPath& session, const QVariantMap&)
{
    if (session.path() != this->_session) return;
    this->_enabled = false;
    if (this->_capturing) {this->_capturing = false; emit this->ended();}
    if (!this->_barriers.isEmpty()) this->setEnabled_(true); // a lock screen / vt switch: armed again
}

void xmouse::platform::PortalCapture::onZonesChanged_(const QDBusObjectPath& session, const QVariantMap&)
{
    if (session.path() != this->_session) return;
    this->getZones_(); // a screen plugged / moved: the barriers set again on the new zones
}

void xmouse::platform::PortalCapture::recover_(void)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    // The compositor still sends the input although it was released here (a Release lost, or refused for an old
    // activation): the person would have a dead mouse and keyboard. Disable ends every capture of the session (the
    // pointer comes back to the desktop), the barriers are armed again just after
    if (now - this->_recoveredAt < RECOVER_INTERVAL || this->_session.isEmpty()) return;
    this->_recoveries = now - this->_recoveredAt < RECOVER_SERIES ? this->_recoveries + 1 : 1;
    this->_recoveredAt = now;

    // First a Release of the last activation (the way the compositor expects to end it: the pointer shown again where
    // it was); only when the input still comes, the capture disabled and armed again
    if (this->_recoveries == 1) {
        QDBusMessage call = QDBusMessage::createMethodCall(PORTAL_SERVICE, PORTAL_PATH, PORTAL_INPUT_CAPTURE, "Release");
        QVariantMap options;
        options.insert("activation_id", this->_activation);
        options.insert("cursor_position", double2(this->_position.x(), this->_position.y()));
        call.setArguments({QVariant::fromValue(QDBusObjectPath(this->_session)), options});
        (void)QDBusConnection::sessionBus().call(call, QDBus::NoBlock);
        return;
    }
    this->_enabled = true; // the Disable is sent whatever the state known here
    this->setEnabled_(false);
    QTimer::singleShot(RECOVER_REARM, this, [this]() {if (!this->_barriers.isEmpty() && !this->_enabled) this->setEnabled_(true);});
}

void xmouse::platform::PortalCapture::dispatch_(void)
{
    struct ei* ei = static_cast<struct ei*>(this->_ei);
    struct ei_event* event = nullptr;

    if (!ei) return;
    this->_api->dispatch(ei);
    while ((event = this->_api->getEvent(ei)) != nullptr) {
        switch (this->_api->eventGetType(event)) {
            case EI_EVENT_SEAT_ADDED:
                this->_api->seatBindCapabilities(this->_api->eventGetSeat(event), EI_DEVICE_CAP_POINTER, EI_DEVICE_CAP_KEYBOARD, EI_DEVICE_CAP_BUTTON, EI_DEVICE_CAP_SCROLL, nullptr);
                break;
            case EI_EVENT_DISCONNECT:
                if (this->_capturing) {this->_capturing = false; emit this->ended();}
                this->_api->eventUnref(event);
                if (this->_notifier) this->_notifier->setEnabled(false);
                this->fail_("the compositor closed the input capture");
                return;
            case EI_EVENT_POINTER_MOTION:
                if (this->_capturing) emit this->motion(this->_api->pointerDx(event), this->_api->pointerDy(event));
                else this->recover_();
                break;
            case EI_EVENT_BUTTON_BUTTON:
                if (!this->_capturing) this->recover_();
                if (this->_capturing) {
                    const std::optional<net::MouseButton> button = button_from_evdev(this->_api->buttonButton(event));
                    if (button) emit this->button(*button, this->_api->buttonIsPress(event));
                }
                break;
            case EI_EVENT_SCROLL_DISCRETE:
                this->_frameDiscrete += QPoint(-this->_api->scrollDiscreteDx(event), -this->_api->scrollDiscreteDy(event));
                break;
            case EI_EVENT_SCROLL_DELTA:
                this->_frameSmooth += QPointF(-this->_api->scrollDx(event), -this->_api->scrollDy(event));
                break;
            case EI_EVENT_FRAME:
                // A wheel sends its notches and the same move as smooth scroll: the notches win, never both
                if (this->_capturing && !this->_frameDiscrete.isNull()) emit this->wheel(this->_frameDiscrete.x(), this->_frameDiscrete.y());
                else if (this->_capturing && !this->_frameSmooth.isNull()) emit this->wheel(static_cast<int>(std::lround(this->_frameSmooth.x() * SMOOTH_SCROLL_FACTOR)), static_cast<int>(std::lround(this->_frameSmooth.y() * SMOOTH_SCROLL_FACTOR)));
                this->_frameDiscrete = QPoint();
                this->_frameSmooth = QPointF();
                break;
            case EI_EVENT_KEYBOARD_KEY:
                if (!this->_capturing) this->recover_();
                if (this->_capturing) {
                    const QString code = code_from_evdev(static_cast<int>(this->_api->keyboardKey(event)));
                    if (!code.isEmpty()) emit this->key(code, this->_api->keyboardIsPress(event));
                }
                break;
            default:
                break;
        }
        this->_api->eventUnref(event);
    }
}

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
##  @file Hub.hpp

File Description:
##  The device in the network: members, layout, its cursor on the others, the foreign cursors on it, the relay,
##  the files and links, the phones / web pads, the windows streamed between devices
##  (Hub.cpp, Hub-cursor.cpp, Hub-foreign.cpp, Hub-route.cpp, Hub-files.cpp, Hub-pads.cpp, Hub-windows.cpp)
\**************************************************************/

#ifndef HUB_H
    #define HUB_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>                      // _hot, _cold, _nodiscard
    #include "../platform/IInputInjector.hpp"       // xmouse::platform::IInputInjector
    #include "../platform/IInputCapture.hpp"        // xmouse::platform::IInputCapture
    #include "../security/PairingCode.hpp"          // xmouse::security::PairingCode
    #include "../security/Certificate.hpp"          // xmouse::security::Certificate
    #include "../platform/Clipboard.hpp"            // xmouse::platform::Clipboard
    #include "../platform/IWindows.hpp"             // xmouse::platform::IWindows, xmouse::platform::WindowInfo
    #include "../security/Group.hpp"                // xmouse::security::Group
    #include "../net/Datagrams.hpp"                 // xmouse::net::Datagrams
    #include "../net/Exposure.hpp"                  // xmouse::net::Exposure
    #include "../net/Mesh.hpp"                      // xmouse::net::Mesh
    #include "AuditLog.hpp"                         // xmouse::core::AuditLog
    #include "Routing.hpp"                          // xmouse::core::Route
    #include "Config.hpp"                           // xmouse::core::Config
    #include "Layout.hpp"                           // xmouse::core::Layout
    #include "State.hpp"                            // xmouse::core::State
    #include <QSslConfiguration>                    // QSslConfiguration
    #include <QElapsedTimer>                        // QElapsedTimer
    #include <QHostAddress>                         // QHostAddress
    #include <QStringList>                          // QStringList
    #include <QDateTime>                            // QDateTime
    #include <QPointer>                             // QPointer
    #include <QObject>                              // QObject
    #include <QString>                              // QString
    #include <QPointF>                              // QPointF
    #include <QColor>                               // QColor
    #include <QImage>                               // QImage
    #include <QTimer>                               // QTimer
    #include <memory>                               // std::unique_ptr, std::shared_ptr
    #include <QFile>                                // QFile
    #include <QHash>                                // QHash
    #include <QList>                                // QList
    #include <QPair>                                // QPair
    #include <QSize>                                // QSize
    #include <QSet>                                 // QSet

namespace xmouse::core { // namespace start

//----------------------------------------------------------------//
/* ENUM */

// A foreign cursor on this device
enum class ForeignState {
    Free, // moves, clicks and types here
    Paused, // the panel of xmouse has the focus (or the cursor is over it): its input is held
    Asking, // it asked the access to the panel: granted at the end of the delay unless refused
    Granted, // access to the panel granted (until the local person revokes it)
};

//----------------------------------------------------------------//
/* STRUCT */

struct Member {
    QString id = "";
    QString name = "";
    QString os = "";
    QString kind = "desktop"; // desktop | phone | web
    QString version = "";
    QString via = ""; // a phone / pad: the device it is connected to
    bool self = false;
    bool online = false;
    bool relayed = false; // reached through another member
    bool udp = false; // its moves go by udp
    int rtt = -1; // ms
    QStringList addresses;
    quint16 port = 0;
    QList<Screen> screens;
    QColor color; // its cursor everywhere
};

struct Foreign {
    QString id = ""; // owner of the cursor
    QString name = "";
    QColor color;
    QPoint local; // position on this device (desktop pixels), last one received
    ForeignState state = ForeignState::Free;
    int buttons = 0; // bits of the buttons held (MouseButton)
    qint64 askedAt = 0; // ms, start of the access request
    qint64 seen = 0; // ms, last move
    quint16 seq = 0; // last move accepted (an older one is dropped: reordered udp)
    bool hasSeq = false;
    QPointF velocity; // px / ms of the last moves (prediction while packets are missing)
    bool passing = false; // private mode / no mouse right here: it crosses this device to reach the others, acts on nothing
    bool told = false; // its owner was told why its clicks do nothing here

    // Where it is drawn now: ahead of the last packet along its speed while the next one is late (80 ms at most)
    _hot _nodiscard inline QPoint drawn(const qint64 now) const
    {
        const qint64 age = now - this->seen;

        if (age < 12 || age > 80 || this->buttons != 0) return this->local; // on time, too old, or a drag (never ahead of the real pointer)
        return this->local + (this->velocity * static_cast<double>(age)).toPoint();
    };
};

// Where the cursor of a member is, for the live map (every member tells it)
struct Presence {
    QString id = ""; // owner
    QString on = ""; // device its cursor is on
    QPoint global; // position in the global space when it was told
    QPoint local; // the same on the device it is on (desktop pixels): drawn there, so it follows a device moved on the layout
    int flags = 0; // XMOUSE_PRESENCE_*
    qint64 seen = 0; // ms
    quint16 seq = 0;
};

// The own cursor of a pad gone to another device (phantom / private mode): it goes there under its own name, the
// cursor of this device stays where the person left it
struct PadAway {
    QString target = ""; // device it is on
    QPointF global; // position in the global space
    QPoint local; // the same on the target
    int buttons = 0; // bits of the buttons held there (MouseButton)
    quint16 seq = 0; // moves sent
};

// A phone (android app) or a browser (web pad) paired with this device: it drives its cursor, a phone also offers its screen
struct Pad {
    QString id = "";
    QString name = "";
    QString kind = "web"; // phone | web
    QByteArray hash; // sha256 of its token (the token stays on the pad)
    qint64 created = 0; // ms
    qint64 seen = 0; // ms, last connection
    bool screenOn = false; // its screen is in the layout (the android app in screen mode)
    bool touch = false; // its screen is touched by the cursors (Accessibility service of the app); else they are only shown there
    QSize screen; // logical size of its screen
    double scale = 1.0;
    QPointer<net::WebSocket> control; // the touchpad page
    QPointer<net::WebSocket> display; // the screen service of the android app
};

struct FileEntry {
    QString name = ""; // file name only (no folder)
    qint64 size = 0;
    QString path = ""; // sender: the local file, receiver: where it is written
};

// Files offered by this device (clipboard, drop, handoff of a window), fetched by the members
struct Offer {
    QString id = "";
    QList<FileEntry> files;
    QString to = ""; // "" = every member (clipboard)
    qint64 created = 0; // ms
};

struct Transfer {
    QString id = ""; // offer id
    QString peer = ""; // the other device
    QString peerName = "";
    QString reason = "drop"; // clip | drop | open
    bool incoming = true;
    QList<FileEntry> files;
    int index = 0; // file being sent / received
    qint64 offset = 0; // in that file
    qint64 total = 0; // bytes of every file
    qint64 done = 0;
    QString dir = ""; // receiver: folder of the files
    std::shared_ptr<QFile> file; // open file of the current index
    qint64 started = 0; // ms
    qint64 touched = 0; // ms, last chunk (stalled transfers are dropped)
};

// A window streamed between two devices: the source pictures it, the viewer shows it and drives it
struct Stream {
    QString id = "";
    QString peer = ""; // source: the viewer, viewer: the source
    bool outgoing = true; // this device is the source
    quint64 window = 0; // source: native id
    QString title = "";
    QSize size; // size of the window on the source (input coordinates)
    QString document = ""; // name of the file it shows (handoff offered)
    qint64 documentSize = 0;
    QString offer = ""; // offer of that file
    qint64 lastFrame = 0; // ms
    bool activated = false; // source: raised before the first input of the viewer
};

//----------------------------------------------------------------//
/* CLASS */

class Hub: public QObject {
    Q_OBJECT

    private:
        Config& _config;
        AuditLog _log;
        security::Certificate _certificate;
        security::Group _group;
        security::PairingCode _code;
        State _state;
        Layout _layout;
        net::Mesh _mesh;
        net::Datagrams _udp;
        net::Exposure _exposure;
        platform::Clipboard _clipboard;
        std::unique_ptr<platform::IInputInjector> _injector;
        std::unique_ptr<platform::IInputCapture> _capture;
        std::unique_ptr<platform::IWindows> _windows;
        QSslConfiguration _serverTls; // pinned self-signed certificate (the tailscale one is added for its name)
        QString _injectorError = "";
        QString _captureError = "";
        QList<Edge> _edgesSent; // the edges the capture holds: set again only when they change (a portal ends its capture)
        bool _edgesKnown = false;
        QTimer _saveTimer; // state written 1 s after the last change
        QTimer _flush; // cursor moves sent at most every 4 ms (the last position only)
        QTimer _presenceTimer; // presence of the cursor told to everybody (20 Hz while it moves)
        QTimer _housekeeping; // 1 s: access requests, stale foreign cursors, addresses, stalled transfers
        QTimer _restoreTimer; // the real pointer put back after a foreign click
        QTimer _predict; // 8 ms while a foreign cursor is drawn ahead of its packets
        QTimer _udpProbe; // XMOUSE_UDP_PROBE_INTERVAL
        QTimer _publishSoon; // the links changed: the device entry is written a little later (one write for a burst)
        QTimer _transferPump; // next file chunks
        QTimer _streamTimer; // frames of the windows streamed from here
        QTimer _padTimer; // map and cursors told to the pads

        /* my cursor */
        bool _away = false; // on another device
        bool _awayByPad = false; // left through a phone / web pad (the local capture holds nothing)
        QString _target = ""; // device it is on
        QPointF _global; // position in the global space (sub pixel)
        QPoint _targetLocal; // position on the target
        bool _moveDirty = false;
        bool _presenceDirty = false;
        quint16 _seq = 0;
        quint16 _presenceSeq = 0;
        int _presenceTick = 0; // ticks of the presence timer (the home pointer is read every other one)
        QPoint _lastHomePointer; // last local pointer told at home
        QPoint _awayPointer; // the real pointer of this device when the cursor left (it moves while away: the cursor comes back)
        bool _awayPointerSet = false; // captured: the held point read at the first check (the capture puts the pointer there)
        bool _awayWatch = false; // _awayPointer known (a capture that tells the pointer)
        int _awayTick = 0; // ticks of the presence timer while away (the real pointer is read every 4th one)
        int _awayLost = 0; // reads in a row where a held pointer was found elsewhere
        bool _slowTarget = false; // the cursor is on a phone (pad or member): its moves at the rate of a screen, its keys as text
        QSet<QString> _textKeys; // keys held whose character went to the phone (their release is not sent)
        bool _capsLock = false; // Caps Lock of this device while my cursor is on a phone (its lights at the departure, then the presses)
        bool _numLock = true; // Num Lock the same way: the numpad types its digits there, else its arrows, Home, End...
        QTimer _repeat; // a key held while my cursor is on a phone: sent again (a phone repeats nothing by itself)
        QString _repeatCode = ""; // the key held
        QByteArray _repeatData; // what it sends (its key, or its character)
        QHash<QString, QString> _padClip; // pad -> hash of the clipboard text it was given
        qint64 _lastSlowMove = 0; // ms
        qint64 _padPresenceAt = 0; // ms, last map update for the own cursor of a pad
        bool _padPresencePending = false;
        QSet<QString> _keysDown; // keys held while away (released on the way back)
        int _buttonsDown = 0;
        qint64 _pingUntil = 0; // ms: "look here" wave told until then
        bool _laser = false;
        QString _following = ""; // member followed on the map and the overlays
        QPoint _padPointer; // local pointer moved by a pad when the platform does not tell it
        QString _homePad; // the pad that drove last (its own cursor comes back with the cursor of this device)
        QHash<QString, PadAway> _padAway; // pad -> its own cursor on another device
        QSet<QString> _toldShowOnly; // pads whose screen only shows the cursors, told once a run when a cursor goes there

        /* foreign cursors */
        QHash<QString, Foreign> _foreign;
        QSet<QString> _granted; // cursors given the panel: kept when they come back, until the local person revokes it
        QString _keyboardOwner = ""; // last foreign cursor that clicked here (its keyboard types here)
        QPoint _savedPointer; // local pointer before a foreign click
        bool _hasSavedPointer = false;
        bool _panelFocused = false; // the window of xmouse has the focus here
        QList<QRect> _panelRects; // windows of xmouse on screen (local desktop pixels)
        qint64 _lastInject = 0; // ms of the last event injected for a foreign cursor
        bool _injectHeldTold = false; // told once while my cursor is away: the input of the foreign cursors is not played here
        QList<qint64> _clicks; // ms of the last presses sent on or played here (a loop of clicks is cut)
        qint64 _clickLoopTold = 0; // ms the last cut was logged

        /* udp */
        QHash<QString, qint64> _udpUp; // member -> ms of its last pong (its moves go by udp while it is fresh)
        QHash<QString, QPair<QHostAddress, quint16>> _udpPath; // member -> where its udp comes from (nat friendly)

        /* pads */
        QHash<QString, Pad> _pads; // paired phones / web pads (pads.json) and their live sockets
        QHash<QString, QPointer<net::WebSocket>> _padPending; // key -> socket of a pad waiting for the local approval

        /* files */
        QHash<QString, Offer> _offers;
        QHash<QString, Transfer> _transfers;
        QString _clipboardTransfer = ""; // files of the clipboard being downloaded (the latest copy only)

        /* windows */
        QHash<QString, Stream> _streams;

        /* everybody */
        QHash<QString, Presence> _presence;
        QString _lastClipboardHash = ""; // content set here by a member (no echo back)
        QString _status = "";

        // ---------- Pre-Function -------- //
        void publishSelf_(void); // device/<self> entry: name, screens, addresses, links
        void rebuildLayout_(void); // from the state
        void refreshCandidates_(void); // addresses to dial
        void applyDelta_(const QJsonObject& delta, const QString& from); // merge + gossip + rebuild
        void broadcastDelta_(const QJsonObject& delta);
        void startInjector_(void);
        void startCapture_(void);
        void updateEdges_(void);
        void setStatus_(const QString& status);
        void housekeeping_(void);

        /* links */
        void onPeerReady_(const QString& id);
        void onPeerLost_(const QString& id, const QString& reason);
        void onMessage_(const QString& id, const QJsonObject& o);
        void onBinary_(const QString& id, const QByteArray& data);
        void onJoined_(const QJsonObject& welcome, const QString& through);

        /* routing, relay, udp (Hub-route.cpp) */
        _hot _nodiscard Route routeTo_(const QString& id) const;
        _hot void sendTo_(const QString& id, const QJsonObject& o, const QString& from = "");
        _hot void sendBinaryTo_(const QString& id, const QByteArray& data, const QString& from = "");
        _hot void sendHot_(const QString& id, const QByteArray& data); // moves, presence: udp when it passes
        _hot _nodiscard qint64 pendingTo_(const QString& id) const; // bytes waiting on the next hop (back pressure)
        void onRelay_(const QString& id, const QJsonObject& o);
        void onRelayBinary_(const QString& id, const QByteArray& data);
        void onDatagram_(const QString& sender, const QHostAddress& from, const quint16 port, const QByteArray& inner);
        void probeUdp_(void);
        _cold _nodiscard QStringList links_(void) const; // members and pads linked here (published)

        /* my cursor (Hub-cursor.cpp) */
        void onCrossed_(const QPoint& local, const QPoint& push);
        void onMotion_(const double dx, const double dy);
        void onButton_(const net::MouseButton button, const bool down);
        void onWheel_(const int dx, const int dy);
        void onKey_(const QString& code, const bool down);
        void goHome_(const QPoint& global); // back on this device at this global point
        void switchTarget_(const QString& device);
        void leaveTo_(const QPoint& global, const QString& device, const QPoint& local); // the cursor goes to device (capture or pad)
        void flushMove_(void);
        void sendPresence_(void);
        void checkAway_(void); // away: the real pointer moves although nothing holds it / the capture was lost -> the cursor comes back
        _hot _nodiscard QString keyText_(const QString& code) const; // the character a key types here ("" = a special key / a shortcut)
        _hot _nodiscard QByteArray keyPacket_(const QString& code, const bool down, const QString& target) const; // the key for target, with the letter it types here (a layout of the other side)
        _hot void repeatLater_(const QString& code, const QByteArray& data); // a key pressed while my cursor is on a phone: repeated while it is held
        void clipToPads_(const QString& only = ""); // the clipboard text to the phone screens (only: this one, when a cursor enters it)
        void keepAway_(void); // the layout changed while away: the cursor stays on its device (closest point) or comes back
        void padMotion_(const QString& pad, const double dx, const double dy); // a pad moves the cursor of this device (or its own one, phantom / private mode)
        void padButton_(const QString& pad, const net::MouseButton button, const bool down);
        void padWheel_(const QString& pad, const int dx, const int dy);
        void padKey_(const QString& pad, const QString& code, const bool down);
        void padText_(const QString& pad, const QString& text);
        _hot bool padOwnCursor_(const QString& pad); // its own cursor made / dropped as the modes say, true when it has one
        _hot void padPhantomAt_(const QString& pad, const QPoint& local);
        _hot void padPresence_(void); // the maps told of the own cursors of the pads (30 Hz at most, the last position always)
        void padLeave_(const QString& pad, const QPoint& global, const Hit& hit); // its own cursor crosses to another device
        _hot void padAwayMotion_(const QString& pad, const double dx, const double dy);
        void padAwayEnd_(const QString& pad); // its own cursor leaves the device it was on (back here, gone, mode changed)
        void tellShowOnly_(const QString& device); // a cursor goes on a phone screen that only shows it: told once (its clicks do nothing there)
        _hot void padAwayInput_(const QString& pad, const QByteArray& data); // a click / wheel / key / text of a pad sent where its cursor is
        _cold _nodiscard QPoint localPointer_(void) const; // pointer of this device (capture, else the pad one)

        /* foreign cursors (Hub-foreign.cpp) */
        void foreignEnter_(const QString& id, const QJsonObject& o);
        void foreignLeave_(const QString& id);
        void foreignPointer_(const QString& id, const QPoint& local, const quint16 seq, const bool hasSeq);
        void foreignButton_(const QString& id, const net::MouseButton button, const bool down);
        void foreignWheel_(const QString& id, const int dx, const int dy);
        void foreignKey_(const QString& id, const QString& code, const bool down);
        void foreignText_(const QString& id, const QString& text);
        _hot _nodiscard bool blocked_(Foreign& f, const QPoint& at); // its input held here (panel), told to the owner
        _nodiscard bool injectionHeld_(const QString& id); // my cursor away under a capture that takes it: not played here
        _nodiscard bool clickLoop_(void); // a press past CLICK_LOOP_MAX in a second (two devices clicking each other)
        void restorePointer_(void);
        void predict_(void);

        /* clipboard, files, links (Hub-files.cpp) */
        void onLocalClipboard_(void);
        void onRemoteClipboard_(const QString& id, const QJsonObject& o);
        void onFilesOffer_(const QString& id, const QJsonObject& o);
        void onFileGet_(const QString& id, const QJsonObject& o);
        void onFileChunk_(const QString& id, const QByteArray& data);
        void onFileEnd_(const QString& id, const QJsonObject& o);
        void onLink_(const QString& id, const QJsonObject& o);
        void pumpTransfers_(void);
        void finishTransfer_(const QString& id);
        void failTransfer_(const QString& id, const QString& reason, const bool tell);
        _cold QString offerFiles_(const QStringList& paths, const QString& to); // offer id, "" = nothing readable
        _cold void fetch_(const QString& peer, const QString& offer, const QList<FileEntry>& files, const QString& reason); // download an offer

        /* pads (Hub-pads.cpp) */
        void loadPads_(void);
        void savePads_(void) const;
        void onPadSocket_(net::WebSocket* ws, const QString& agent);
        void onPadText_(net::WebSocket* ws, const QString& text);
        void onPadClosed_(net::WebSocket* ws);
        void padCommand_(const QString& pad, const QJsonObject& o);
        void deliverToPad_(const QString& pad, const QString& from, const QJsonObject& o);
        void deliverBinaryToPad_(const QString& pad, const QString& from, const QByteArray& data);
        void publishPad_(const QString& pad);
        void sendPadMaps_(void);
        _cold _nodiscard QString padOf_(const net::WebSocket* ws) const; // "" = not authenticated

        /* windows (Hub-windows.cpp) */
        void onWindowCrossed_(const quint64 window, const QPoint& local, const QPoint& push);
        void onStreamMessage_(const QString& id, const QJsonObject& o);
        void onWindowFrame_(const QString& id, const QByteArray& data);
        void pumpStreams_(void);

        // ------------ Function ---------- //
        /* tools */
        _cold inline void scheduleSave_(void)                                 {this->_saveTimer.start();}; // state written 1 s after the last change
        _hot _nodiscard inline QPoint injectPoint_(const QPoint& local) const {return local - this->_layout.bounds(this->_group.self()).topLeft();}; // desktop pixels -> injector pixels
        _hot _nodiscard inline bool padPhantom_(void) const                   {return this->_config.settings().privateMode || this->_config.settings().foreignMode == "phantom";}; // private / phantom mode: a pad has its own cursor here

    public:
        // ---------- Pre-Function -------- //
        void start(void); // identity, group, server, capture, injection (throws)
        void stop(void);

        /* network */
        void createNetwork(const QString& name);
        void join(const QString& link, const QString& code = ""); // "host[:port]" or the https:// link of a member (its code and identity inside)
        void leave(void);
        void removeMember(const QString& id);
        void renameNetwork(const QString& name);
        void renameDevice(const QString& name);
        _cold _nodiscard QString joinLink(const QString& host = "", const quint16 port = 0) const; // https://host:port/?code=..&fp=..&net=.. (QR code), "" = the first address the others can use
        _cold _nodiscard QString padLink(const QString& host = "", const quint16 port = 0) const; // https://host:port/pad?code=..&fp=.. (a phone / browser pairs with it)
        void setExposure(const Exposure exposure); // how this device is reached (saved, applied at once)
        _cold _nodiscard QList<Member> members(void) const;
        _cold _nodiscard QList<net::Announce> nearby(void) const; // devices announced on the LAN, not members
        _cold _nodiscard bool reachable(const QString& id) const; // linked, or through a member / a pad of this device

        /* layout */
        void placeDevice(const QString& id, const QPoint& position); // global top left of its desktop
        void resetLayout(void); // every device side by side again
        _cold _nodiscard QStringList profiles(void) const; // saved layouts (profile/<name>)
        void saveProfile(const QString& name); // the current layout under this name (for every member)
        void applyProfile(const QString& name); // the devices of the profile go where it put them
        void deleteProfile(const QString& name);

        /* presence */
        void ping(void); // "look here" wave where my cursor is (1.5 s)
        void setLaser(const bool enabled); // my cursor leaves a trail on the screens it crosses
        void follow(const QString& id); // the map and the overlays point at this member ("" = nobody)
        _cold _nodiscard QPoint pointerGlobal(void) const; // my cursor in the global space
        _hot _nodiscard QPoint presenceGlobal(const Presence& p) const; // where a cursor is now (its device may have moved since)

        /* access */
        void setPrivateMode(const bool enabled);
        void setRights(const QString& id, const Rights& rights);
        void setNetworkSetting(const QString& name, const QJsonValue& value); // net/<name> for every member
        void setPanelFocused(const bool focused); // the window of xmouse got / lost the focus
        void refuseAccess(const QString& id); // the local person refuses an access request
        void grantAccess(const QString& id); // the local person accepts an access request at once (no waiting)
        void revokeAccess(const QString& id); // the local person takes a granted access back
        void kickCursor(const QString& id); // the foreign cursor is sent back to its device
        void comeBack(void); // my cursor back on this device (tray, shortcut)

        /* files and links */
        QString sendFiles(const QString& device, const QStringList& paths, const QString& reason = "drop"); // transfer id, "" = refused (offline, nothing readable)
        void sendLink(const QString& device, const QString& url);
        void cancelTransfer(const QString& id);
        _cold _nodiscard QList<Transfer> transfers(void) const;
        _cold _nodiscard QString downloadDir(void) const; // received files (created)

        /* pads */
        void approvePad(const QString& pending, const bool accepted); // answer of the local person to a new phone / pad
        void removePad(const QString& id); // its token forgotten, its screen out of the layout
        _cold _nodiscard QList<Pad> pads(void) const;

        /* windows */
        _cold _nodiscard QList<platform::WindowInfo> windows(void) const; // the windows of this device that can be streamed
        QString streamWindow(const QString& device, const quint64 window); // stream id, "" = refused (switch off, offline, no picture)
        void stopStream(const QString& id);
        void streamInput(const QString& id, const QJsonObject& input); // viewer side: {"k": "move" | "button" | "wheel" | "key" | "text", ...} in window pixels
        void openStreamDocument(const QString& id); // viewer side: the file the window shows, downloaded and opened here
        _cold _nodiscard bool windowMoveEnabled(void) const; // network wide switch net/windowMove

        /* tools */
        _cold _nodiscard static QColor colorOf(const QString& id); // stable color of a member
        _cold _nodiscard static QString osName(void); // linux | windows | macos | android

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard inline Config& config(void)                                 {return this->_config;};
        _cold _nodiscard inline AuditLog& log(void)                                  {return this->_log;};
        _cold _nodiscard inline const security::Group& group(void) const             {return this->_group;};
        _cold _nodiscard inline security::PairingCode& code(void)                    {return this->_code;};
        _cold _nodiscard inline const State& state(void) const                       {return this->_state;};
        _cold _nodiscard inline const Layout& layout(void) const                     {return this->_layout;};
        _cold _nodiscard inline net::Exposure& exposure(void)                        {return this->_exposure;};
        _cold _nodiscard inline net::Mesh& mesh(void)                                {return this->_mesh;};
        _cold _nodiscard inline const QHash<QString, Foreign>& foreign(void) const   {return this->_foreign;};
        _cold _nodiscard inline const QHash<QString, Presence>& presence(void) const {return this->_presence;};
        _cold _nodiscard inline const QHash<QString, Stream>& streams(void) const    {return this->_streams;};
        _cold _nodiscard inline bool isAway(void) const                              {return this->_away;};
        _cold _nodiscard inline const QString& target(void) const                    {return this->_target;};
        _cold _nodiscard inline const QString& status(void) const                    {return this->_status;};
        _cold _nodiscard inline const QString& injectorError(void) const             {return this->_injectorError;};
        _cold _nodiscard inline const QString& captureError(void) const              {return this->_captureError;};
        _cold _nodiscard inline QString captureName(void) const                      {return this->_capture ? this->_capture->name() : QString("none");};
        _cold _nodiscard inline QString injectorName(void) const                     {return this->_injector ? this->_injector->name() : QString("none");};
        _cold _nodiscard inline QString windowsName(void) const                      {return this->_windows ? this->_windows->name() : QString("none");};
        _cold _nodiscard inline QByteArray fingerprint(void) const                   {return this->_certificate.fingerprint();};
        _cold _nodiscard inline bool laser(void) const                               {return this->_laser;};
        _cold _nodiscard inline const QString& following(void) const                 {return this->_following;};
        _cold _nodiscard inline bool udpUp(const QString& id) const                  {return QDateTime::currentMSecsSinceEpoch() - this->_udpUp.value(id, 0) < XMOUSE_UDP_TIMEOUT;};
        _cold _nodiscard inline int heldButtons(void) const                          {return this->_capture ? this->_capture->heldButtons() : -1;};
        _cold _nodiscard inline QPoint ownPointer(void) const                        {return this->localPointer_();}; // local pointer (laser trail at home)

        /* setter */
        _cold inline void setPanelRects(const QList<QRect>& rects) {this->_panelRects = rects;}; // windows of xmouse on screen (local)

        /* access */
        _cold _nodiscard inline bool injectingNow(void) const {return QDateTime::currentMSecsSinceEpoch() - this->_lastInject < 150;}; // an input of a foreign cursor was injected in the last 150 ms (not the local person)
        _cold _nodiscard inline bool echoNow(void) const {return this->injectingNow() && this->_capture && this->_capture->takesInjected();}; // what the capture gives now may be the echo of an injection
        _cold _nodiscard inline bool injectionEchoes(void) const {return this->_away && !this->_awayByPad && this->_capture && this->_capture->capturing() && this->_capture->takesInjected();}; // an injection here would go where my cursor is

        // ---------- Constructor --------- //
        explicit Hub(Config& config, QObject* parent = nullptr);

        // ----------- Destructor --------- //
        ~Hub() override;

    signals:
        void membersChanged(void); // list, online, names
        void layoutChanged(void);
        void presenceChanged(void); // cursors on the live map
        void foreignChanged(void); // cursors drawn on this device (overlays)
        void awayChanged(void); // my cursor left / came back
        void networkChanged(void); // created, joined, left, renamed
        void statusChanged(const QString& status);
        void joinApproval(xmouse::net::Peer* peer); // accept / refuse a new device (30 s, refused by default)
        void joinResult(const bool ok, const QString& message); // this device joining a network
        void accessRequested(const QString& id, const QString& name, const int seconds); // a foreign cursor asks the panel
        void accessEnded(const QString& id, const bool granted);
        void notice(const QString& text); // short message for the tray / the status bar
        void followChanged(void);
        void linkReceived(const QString& id, const QString& name, const QString& url); // links = "ask": the panel asks what to do
        void transfersChanged(void); // progress of the files
        void filesReceived(const QString& from, const QStringList& paths, const QString& folder); // a drop / handoff landed here
        void padApproval(const QString& pending, const QString& name, const QString& kind); // accept / refuse a new phone / pad (30 s, refused by default)
        void padsChanged(void);
        void streamStarted(const QString& id, const QString& from, const QString& title, const QString& document); // viewer side
        void streamFrame(const QString& id, const QImage& frame, const QSize& size); // viewer side: picture + size of the window on its device
        void streamEnded(const QString& id);
};

} // namespace end
#endif /* HUB_H */

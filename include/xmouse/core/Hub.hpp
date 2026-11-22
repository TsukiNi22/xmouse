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
##  The device in the network: members, layout, its cursor on the others, the foreign cursors on it
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
    #include "../security/Group.hpp"                // xmouse::security::Group
    #include "../net/Exposure.hpp"                  // xmouse::net::Exposure
    #include "../net/Mesh.hpp"                      // xmouse::net::Mesh
    #include "AuditLog.hpp"                         // xmouse::core::AuditLog
    #include "Config.hpp"                           // xmouse::core::Config
    #include "Layout.hpp"                           // xmouse::core::Layout
    #include "State.hpp"                            // xmouse::core::State
    #include <QElapsedTimer>                        // QElapsedTimer
    #include <QStringList>                          // QStringList
    #include <QDateTime>                            // QDateTime
    #include <QPointer>                             // QPointer
    #include <QObject>                              // QObject
    #include <QString>                              // QString
    #include <QPointF>                              // QPointF
    #include <QColor>                               // QColor
    #include <QTimer>                               // QTimer
    #include <memory>                               // std::unique_ptr
    #include <QHash>                                // QHash
    #include <QList>                                // QList
    #include <QSet>                                 // QSet

namespace xmouse::core { // namespace start

//----------------------------------------------------------------//
/* ENUM */

// A foreign cursor on this device
enum class ForeignState {
    Free, // moves, clicks and types here
    Paused, // the panel of xmouse has the focus (or the cursor is over it): its input is held
    Asking, // it asked the access to the panel: granted at the end of the delay unless refused
    Granted, // access to the panel granted (until the local person uses the panel again)
};

//----------------------------------------------------------------//
/* STRUCT */

struct Member {
    QString id = "";
    QString name = "";
    QString os = "";
    QString version = "";
    bool self = false;
    bool online = false;
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
    QPoint local; // position on this device (desktop pixels)
    ForeignState state = ForeignState::Free;
    int buttons = 0; // bits of the buttons held (MouseButton)
    qint64 askedAt = 0; // ms, start of the access request
    qint64 seen = 0; // ms, last move
};

// Where the cursor of a member is, for the live map (every member tells it)
struct Presence {
    QString id = ""; // owner
    QString on = ""; // device its cursor is on
    QPoint global; // position in the global space
    int flags = 0; // XMOUSE_PRESENCE_*
    qint64 seen = 0; // ms
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
        net::Exposure _exposure;
        platform::Clipboard _clipboard;
        std::unique_ptr<platform::IInputInjector> _injector;
        std::unique_ptr<platform::IInputCapture> _capture;
        QString _injectorError = "";
        QString _captureError = "";
        QTimer _saveTimer; // state written 1 s after the last change
        QTimer _flush; // cursor moves sent at most every 4 ms (the last position only)
        QTimer _presenceTimer; // presence of the cursor told to everybody (20 Hz while it moves)
        QTimer _housekeeping; // 1 s: access requests, stale foreign cursors, addresses
        QTimer _restoreTimer; // the real pointer put back after a foreign click

        /* my cursor */
        bool _away = false; // on another device
        QString _target = ""; // device it is on
        QPointF _global; // position in the global space (sub pixel)
        QPoint _targetLocal; // position on the target
        bool _moveDirty = false;
        bool _presenceDirty = false;
        quint16 _seq = 0;
        QSet<QString> _keysDown; // keys held while away (released on the way back)
        int _buttonsDown = 0;

        /* foreign cursors */
        QHash<QString, Foreign> _foreign;
        QString _keyboardOwner = ""; // last foreign cursor that clicked here (its keyboard types here)
        QPoint _savedPointer; // local pointer before a foreign click
        bool _hasSavedPointer = false;
        bool _panelFocused = false; // the window of xmouse has the focus here
        QList<QRect> _panelRects; // windows of xmouse on screen (local desktop pixels)
        qint64 _lastInject = 0; // ms of the last event injected for a foreign cursor

        /* everybody */
        QHash<QString, Presence> _presence;
        QString _lastClipboardHash = ""; // content set here by a member (no echo back)
        QString _status = "";

        // ---------- Pre-Function -------- //
        void publishSelf_(void); // device/<self> entry: name, screens, addresses
        void rebuildLayout_(void); // from the state
        void refreshCandidates_(void); // addresses to dial
        void applyDelta_(const QJsonObject& delta, const QString& from); // merge + gossip + rebuild
        void broadcastDelta_(const QJsonObject& delta);
        void startInjector_(void);
        void startCapture_(void);
        void updateEdges_(void);
        void setStatus_(const QString& status);

        /* links */
        void onPeerReady_(const QString& id);
        void onPeerLost_(const QString& id, const QString& reason);
        void onMessage_(const QString& id, const QJsonObject& o);
        void onBinary_(const QString& id, const QByteArray& data);
        void onJoined_(const QJsonObject& welcome, const QString& through);

        /* my cursor */
        void onCrossed_(const QPoint& local, const QPoint& push);
        void onMotion_(const double dx, const double dy);
        void onButton_(const net::MouseButton button, const bool down);
        void onWheel_(const int dx, const int dy);
        void onKey_(const QString& code, const bool down);
        void goHome_(const QPoint& global); // back on this device at this global point
        void switchTarget_(const QString& device);
        void flushMove_(void);
        void sendPresence_(void);

        /* foreign cursors */
        void foreignEnter_(const QString& id, const QJsonObject& o);
        void foreignLeave_(const QString& id);
        void foreignPointer_(const QString& id, const QPoint& local);
        void foreignButton_(const QString& id, const net::MouseButton button, const bool down);
        void foreignWheel_(const QString& id, const int dx, const int dy);
        void foreignKey_(const QString& id, const QString& code, const bool down);
        void foreignText_(const QString& id, const QString& text);
        _hot _nodiscard bool blocked_(Foreign& f, const QPoint& at); // its input held here (panel), told to the owner
        void restorePointer_(void);
        void housekeeping_(void);

        /* clipboard */
        void onLocalClipboard_(void);
        void onRemoteClipboard_(const QString& id, const QJsonObject& o);

        // ------------ Function ---------- //
        /* tools */
        _cold inline void scheduleSave_(void)                                 {this->_saveTimer.start();}; // state written 1 s after the last change
        _hot _nodiscard inline QPoint injectPoint_(const QPoint& local) const {return local - this->_layout.bounds(this->_group.self()).topLeft();}; // desktop pixels -> injector pixels

    public:
        // ---------- Pre-Function -------- //
        void start(void); // identity, group, server, capture, injection (throws)
        void stop(void);

        /* network */
        void createNetwork(const QString& name);
        void join(const QString& link, const QString& code = ""); // "host[:port]" or an xmouse:// link (its code inside)
        void leave(void);
        void removeMember(const QString& id);
        void renameNetwork(const QString& name);
        void renameDevice(const QString& name);
        _cold _nodiscard QString joinLink(const QString& host = "", const quint16 port = 0) const; // xmouse://host:port?fp=..&code=..&net=.. (QR code), "" = the first address the others can use
        void setExposure(const Exposure exposure); // how this device is reached (saved, applied at once)
        _cold _nodiscard QList<Member> members(void) const;
        _cold _nodiscard QList<net::Announce> nearby(void) const; // devices announced on the LAN, not members

        /* layout */
        void placeDevice(const QString& id, const QPoint& position); // global top left of its desktop
        void resetLayout(void); // every device side by side again

        /* access */
        void setPrivateMode(const bool enabled);
        void setRights(const QString& id, const Rights& rights);
        void setNetworkSetting(const QString& name, const QJsonValue& value); // net/<name> for every member
        void setPanelFocused(const bool focused); // the window of xmouse got / lost the focus
        void panelUsedLocally(void); // a key / click of the local person in the panel: the granted accesses end
        void refuseAccess(const QString& id); // the local person refuses an access request
        void kickCursor(const QString& id); // the foreign cursor is sent back to its device
        void comeBack(void); // my cursor back on this device (tray, shortcut)

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
        _cold _nodiscard inline bool isAway(void) const                              {return this->_away;};
        _cold _nodiscard inline const QString& target(void) const                    {return this->_target;};
        _cold _nodiscard inline const QString& status(void) const                    {return this->_status;};
        _cold _nodiscard inline const QString& injectorError(void) const             {return this->_injectorError;};
        _cold _nodiscard inline const QString& captureError(void) const              {return this->_captureError;};
        _cold _nodiscard inline QString captureName(void) const                      {return this->_capture ? this->_capture->name() : QString("none");};
        _cold _nodiscard inline QString injectorName(void) const                     {return this->_injector ? this->_injector->name() : QString("none");};
        _cold _nodiscard inline QByteArray fingerprint(void) const                   {return this->_certificate.fingerprint();};

        /* setter */
        _cold inline void setPanelRects(const QList<QRect>& rects) {this->_panelRects = rects;}; // windows of xmouse on screen (local)

        /* access */
        _cold _nodiscard inline bool injectingNow(void) const {return QDateTime::currentMSecsSinceEpoch() - this->_lastInject < 150;}; // an input of a foreign cursor was injected in the last 150 ms (not the local person)

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
};

} // namespace end
#endif /* HUB_H */

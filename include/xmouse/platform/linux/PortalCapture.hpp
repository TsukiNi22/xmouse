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
##  @file PortalCapture.hpp

File Description:
##  Wayland edges: xdg-desktop-portal InputCapture (pointer barriers) + libei receiver (loaded at run time)
\**************************************************************/

#ifndef PORTALCAPTURE_H
    #define PORTALCAPTURE_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>          // _cold, _nodiscard
    #include "../IInputCapture.hpp"     // xmouse::platform::IInputCapture
    #include <QSocketNotifier>          // QSocketNotifier
    #include <QDBusObjectPath>          // QDBusObjectPath
    #include <QDBusContext>             // QDBusContext
    #include <QVariantMap>              // QVariantMap
    #include <functional>               // std::function
    #include <QString>                  // QString
    #include <QPointF>                  // QPointF
    #include <QHash>                    // QHash
    #include <QRect>                    // QRect
    #include <QList>                    // QList

namespace xmouse::platform { // namespace start

//----------------------------------------------------------------//
/* STRUCT */

struct EiApi; // libei symbols (only in the .cpp)

//----------------------------------------------------------------//
/* CLASS */

/*
 * Session: CreateSession (keyboard + pointer) -> GetZones -> SetPointerBarriers -> ConnectToEIS -> Enable
 * Activated: the pointer pushed a barrier, the compositor sends the events through libei until Release
 * GNOME asks once per session "allow remote interaction" (the portal dialog)
*/
class PortalCapture final: public IInputCapture, protected QDBusContext {
    Q_OBJECT

    private:
        EiApi* _api = nullptr; // libei.so.1
        void* _ei = nullptr; // struct ei*
        QSocketNotifier* _notifier = nullptr; // libei fd
        QString _session = ""; // session handle
        QString _error = "";
        QList<QRect> _zones; // logical layout of the compositor
        unsigned int _zoneSet = 0;
        QList<core::Edge> _edges; // asked
        QList<core::Edge> _barriers; // set (index + 1 = barrier id)
        unsigned int _activation = 0; // id of the current capture (0 = none)
        QPoint _position; // pointer when the capture started (local)
        bool _enabled = false;
        bool _capturing = false;
        bool _started = false; // session ready
        bool _requested = false; // session asked (once per run: GNOME has no restore token, every session is a dialog)
        qint64 _recoveredAt = 0; // ms, last time a capture kept by the compositor was taken back
        int _recoveries = 0; // in a row: a Release first, then the capture disabled and armed again
        QPoint _frameDiscrete; // wheel notches of the current libei frame (1/120)
        QPointF _frameSmooth; // smooth scroll of the current frame (used only without notches)
        QHash<QString, std::function<void(const unsigned int, const QVariantMap&)>> _requests; // request path -> callback

        // ---------- Pre-Function -------- //
        void request_(const QString& method, const QList<QVariant>& args, const QVariantMap& options, const std::function<void(const unsigned int, const QVariantMap&)>& done);
        void createSession_(void);
        void getZones_(void);
        void applyBarriers_(void);
        void connectEis_(void);
        void setEnabled_(const bool enabled);
        void dispatch_(void); // libei events
        void recover_(void); // input still comes while nothing is captured here: the compositor kept the capture, it is disabled and armed again
        void fail_(const QString& error);

    private slots:
        void onResponse_(const uint code, const QVariantMap& results); // org.freedesktop.portal.Request.Response
        void onActivated_(const QDBusObjectPath& session, const QVariantMap& options);
        void onDeactivated_(const QDBusObjectPath& session, const QVariantMap& options);
        void onDisabled_(const QDBusObjectPath& session, const QVariantMap& options);
        void onZonesChanged_(const QDBusObjectPath& session, const QVariantMap& options);

    public:
        // ---------- Pre-Function -------- //
        bool start(void) override;
        void stop(void) override;
        void setEdges(const QList<core::Edge>& edges) override;
        void release(const QPoint& local) override;
        _cold _nodiscard static bool available(void); // portal InputCapture present and libei loadable

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard bool capturing(void) const override    {return this->_capturing;};
        _cold _nodiscard QString name(void) const override      {return "portal";};
        _cold _nodiscard QString error(void) const override     {return this->_error;};
        _cold _nodiscard bool knowsPointer(void) const override {return false;};
        _cold _nodiscard QPoint pointer(void) const override    {return this->_position;};

        // ---------- Constructor --------- //
        explicit PortalCapture(QObject* parent = nullptr);

        // ----------- Destructor --------- //
        ~PortalCapture() override;
};

} // namespace end
#endif /* PORTALCAPTURE_H */

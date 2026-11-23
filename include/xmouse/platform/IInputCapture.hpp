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
##  @file IInputCapture.hpp

File Description:
##  Capture of the local mouse / keyboard when the cursor leaves through an edge of the screens
\**************************************************************/

#ifndef IINPUTCAPTURE_H
    #define IINPUTCAPTURE_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _hot, _cold, _nodiscard
    #include "../net/Protocol.hpp"  // xmouse::net::MouseButton
    #include "../core/Layout.hpp"   // xmouse::core::Edge
    #include <QObject>              // QObject
    #include <QString>              // QString
    #include <QPoint>               // QPoint
    #include <memory>               // std::unique_ptr
    #include <QList>                // QList

namespace xmouse::platform { // namespace start

//----------------------------------------------------------------//
/* CLASS */

/*
 * Idle: the pointer moves on this device, the backend watches the edges given by setEdges (barriers)
 * Captured: the pointer crossed one, the local pointer is held (hidden / frozen) and every event goes to the signals
 * release(point) ends a capture and puts the local pointer there (local desktop coordinates)
 * Coordinates: logical pixels of the desktop of this device (QScreen::geometry)
*/
class IInputCapture: public QObject {
    Q_OBJECT

    public:
        // ---------- Pre-Function -------- //
        _cold virtual bool start(void) = 0; // false = unavailable (error() says why); may be asynchronous (ready / failed)
        _cold virtual void stop(void) = 0;
        _cold virtual void setEdges(const QList<core::Edge>& edges) = 0; // the openings on the other devices
        _cold virtual void release(const QPoint& local) = 0; // capture ended, local pointer put there
        _cold _nodiscard virtual bool capturing(void) const = 0;
        _cold _nodiscard virtual QString name(void) const = 0; // "portal", "hooks", "grab"
        _cold _nodiscard virtual QString error(void) const = 0;
        _cold _nodiscard virtual bool knowsPointer(void) const = 0; // pointer() is reliable (restore after a foreign click)
        _cold _nodiscard virtual QPoint pointer(void) const = 0; // local pointer position when knowsPointer()

        // ------------ Function ---------- //
        _cold _nodiscard virtual int heldButtons(void) const    {return -1;}; // bits of the buttons held now (MouseButton), -1 = unknown (the drop shelf needs it)
        _cold _nodiscard virtual bool takesInjected(void) const {return true;}; // it takes the input xmouse injects here too (Linux: no mark tells it apart)
        _cold virtual void setWindowMove(const bool enabled)    {(void)enabled;}; // a window dragged on an edge: windowCrossed (true) or the usual border of the OS (false)

        // ---------- Constructor --------- //
        explicit IInputCapture(QObject* parent = nullptr): QObject(parent) { /* Nothing */ };

        // ----------- Destructor --------- //
        ~IInputCapture() override = default;

    signals:
        void ready(void); // asynchronous start done
        void failed(const QString& error); // asynchronous start failed / backend lost
        void crossed(const QPoint& local, const QPoint& push); // capture started: pointer at local on an edge, push = direction it went
        void motion(const double dx, const double dy); // relative move while captured
        void button(const xmouse::net::MouseButton button, const bool down);
        void wheel(const int dx, const int dy); // 1/120 of a notch
        void key(const QString& code, const bool down); // KeyboardEvent.code
        void ended(void); // the capture was taken away (compositor, lock screen, Ctrl+Alt+Del)
        void windowCrossed(const quint64 window, const QPoint& local, const QPoint& push); // a window being dragged pushed an edge (setWindowMove(true) only), nothing captured
};

//----------------------------------------------------------------//
/* PROTOTYPE */

_cold _nodiscard std::unique_ptr<IInputCapture> create_input_capture(QObject* parent = nullptr); // best backend of the platform (not started)

} // namespace end
#endif /* IINPUTCAPTURE_H */

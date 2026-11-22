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
##  @file ShellCapture.hpp

File Description:
##  GNOME Wayland edges through the xmouse GNOME Shell extension (no portal dialog, pointer position known)
\**************************************************************/

#ifndef SHELLCAPTURE_H
    #define SHELLCAPTURE_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>          // _cold, _nodiscard
    #include "../IInputCapture.hpp"     // xmouse::platform::IInputCapture
    #include <QString>                  // QString
    #include <QPoint>                   // QPoint

    //----------------------------------------------------------------//
    /* DEFINE */

    #define XMOUSE_SHELL_SERVICE "org.gnome.Shell"
    #define XMOUSE_SHELL_PATH "/io/github/tsukini22/xmouse"
    #define XMOUSE_SHELL_INTERFACE "io.github.tsukini22.xmouse.Shell1"
    #define XMOUSE_SHELL_VERSION 1 // version of the D-Bus interface of the extension this build speaks
    #define XMOUSE_SHELL_EXTENSION "xmouse@tsukini22.github.io"

namespace xmouse::platform { // namespace start

//----------------------------------------------------------------//
/* CLASS */

/*
 * The extension (gnome-extension/xmouse@tsukini22.github.io) puts the barriers, grabs the input once one is pushed and
 * sends the events to this process only; it gives the pointer back when xmouse releases it, stops or crashes
 * Missing / disabled extension: create_input_capture falls back to the portal (PortalCapture)
*/
class ShellCapture final: public IInputCapture {
    Q_OBJECT

    private:
        bool _capturing = false;
        QPoint _last; // last pointer known (answer of GetPointer, or the point of a release)
        QString _error = "";

        // ---------- Pre-Function -------- //
        void call_(const QString& method, const QList<QVariant>& args); // asynchronous, errors logged in error()

    private slots:
        // ---------- Pre-Function -------- //
        void onCrossed_(const int x, const int y, const int dx, const int dy);
        void onKey_(const uint evdev, const bool down);
        void onEnded_(void);

        // ------------ Function ---------- //
        /* events */
        void onMotion_(const double dx, const double dy)   {if (this->_capturing) emit this->motion(dx, dy);};
        void onButton_(const uint button, const bool down) {if (this->_capturing && button <= static_cast<uint>(net::MouseButton::Forward)) emit this->button(static_cast<net::MouseButton>(button), down);};
        void onWheel_(const int dx, const int dy)          {if (this->_capturing) emit this->wheel(dx, dy);};

    public:
        // ---------- Pre-Function -------- //
        bool start(void) override;
        void stop(void) override;
        void setEdges(const QList<core::Edge>& edges) override;
        void release(const QPoint& local) override;
        _cold _nodiscard QPoint pointer(void) const override; // GetPointer (a short synchronous call)
        _cold _nodiscard static bool available(void); // the extension answers with the expected version

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard bool capturing(void) const override    {return this->_capturing;};
        _cold _nodiscard QString name(void) const override      {return "gnome-shell";};
        _cold _nodiscard QString error(void) const override     {return this->_error;};
        _cold _nodiscard bool knowsPointer(void) const override {return true;};

        // ---------- Constructor --------- //
        explicit ShellCapture(QObject* parent = nullptr): IInputCapture(parent) { /* Nothing */ };

        // ----------- Destructor --------- //
        ~ShellCapture() override;
};

} // namespace end
#endif /* SHELLCAPTURE_H */

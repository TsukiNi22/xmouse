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
##  @file IWindows.hpp

File Description:
##  The windows of this device: list, picture of one (streamed to another device), focus, the document it shows
\**************************************************************/

#ifndef IWINDOWS_H
    #define IWINDOWS_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp> // _cold, _hot, _nodiscard
    #include <optional>        // std::optional
    #include <QString>         // QString
    #include <QImage>          // QImage
    #include <memory>          // std::unique_ptr
    #include <QRect>           // QRect
    #include <QList>           // QList

namespace xmouse::platform { // namespace start

//----------------------------------------------------------------//
/* STRUCT */

struct WindowInfo {
    quint64 id = 0; // native id (HWND, X11 window, mutter window id)
    QString title = "";
    QString app = ""; // program / window class
    qint64 pid = 0;
    QRect rect; // local desktop pixels (the frame)
    QString document = ""; // file it shows when it can be found (handoff), "" = unknown
};

//----------------------------------------------------------------//
/* CLASS */

class IWindows {
    public:
        // ---------- Pre-Function -------- //
        _cold _nodiscard virtual QList<WindowInfo> list(void) = 0; // the normal visible windows (no panel, no dock)
        _cold _nodiscard virtual std::optional<WindowInfo> info(const quint64 id) = 0; // nullopt = closed
        _hot _nodiscard virtual QImage capture(const quint64 id) = 0; // null = cannot be pictured now
        _cold virtual void activate(const quint64 id) = 0; // raise + focus (before the input of a viewer)
        _cold _nodiscard virtual QString name(void) const = 0; // "win32", "gnome-shell", "x11"

        // ----------- Destructor --------- //
        virtual ~IWindows() = default;
};

//----------------------------------------------------------------//
/* PROTOTYPE */

_cold _nodiscard std::unique_ptr<IWindows> create_windows(void); // nullptr = this platform cannot share its windows
_cold _nodiscard QString document_of_process(const qint64 pid); // linux: the last argument of the process that is a file ("" = none)

} // namespace end
#endif /* IWINDOWS_H */

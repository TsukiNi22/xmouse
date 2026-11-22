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
##  @file ShellWindows.hpp

File Description:
##  GNOME Wayland windows through the xmouse extension (list, picture of a window actor, focus)
\**************************************************************/

#ifndef SHELLWINDOWS_H
    #define SHELLWINDOWS_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp> // _cold, _hot, _nodiscard
    #include "../IWindows.hpp" // xmouse::platform::IWindows

namespace xmouse::platform { // namespace start

//----------------------------------------------------------------//
/* CLASS */

class ShellWindows final: public IWindows {
    public:
        // ---------- Pre-Function -------- //
        _cold _nodiscard QList<WindowInfo> list(void) override;
        _cold _nodiscard std::optional<WindowInfo> info(const quint64 id) override;
        _hot _nodiscard QImage capture(const quint64 id) override;
        _cold void activate(const quint64 id) override;

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard QString name(void) const override {return "gnome-shell";};

        // ---------- Constructor --------- //
        ShellWindows(void);

        // ----------- Destructor --------- //
        ~ShellWindows() override;
};

} // namespace end
#endif /* SHELLWINDOWS_H */

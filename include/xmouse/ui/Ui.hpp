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
##  @file Ui.hpp

File Description:
##  Start of the interfaces: graphical panel (Qt) or terminal panel
\**************************************************************/

#ifndef UI_H
    #define UI_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "../core/Config.hpp"   // xmouse::core::Config

namespace xmouse::ui { // namespace start

//----------------------------------------------------------------//
/* PROTOTYPE */

_cold _nodiscard bool has_display(void); // a graphical session (Windows / macOS: always)
_cold void choose_platform(void); // GNOME / Wayland: Xwayland (the overlays stay on top), XMOUSE_QPA overrides
_cold _nodiscard int run_gui(int& argc, char** argv, core::Config& config, const QString& join, const QString& code); // panel + overlays + tray
_cold _nodiscard int run_tty(int& argc, char** argv, core::Config& config, const QString& join, const QString& code); // terminal panel (overlays too when a display exists)

} // namespace end
#endif /* UI_H */

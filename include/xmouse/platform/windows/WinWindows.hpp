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
##  @file WinWindows.hpp

File Description:
##  Windows of Windows: EnumWindows, PrintWindow (the content even when covered), SetForegroundWindow
\**************************************************************/

#ifndef WINWINDOWS_H
    #define WINWINDOWS_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp> // _cold, _hot, _nodiscard
    #include "../IWindows.hpp" // xmouse::platform::IWindows

namespace xmouse::platform { // namespace start

//----------------------------------------------------------------//
/* CLASS */

class WinWindows final: public IWindows {
    public:
        // ---------- Pre-Function -------- //
        _cold _nodiscard QList<WindowInfo> list(void) override;
        _cold _nodiscard std::optional<WindowInfo> info(const quint64 id) override;
        _hot _nodiscard QImage capture(const quint64 id) override;
        _cold void activate(const quint64 id) override;

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard QString name(void) const override {return "win32";};

        // ---------- Constructor --------- //
        WinWindows(void);

        // ----------- Destructor --------- //
        ~WinWindows() override;
};

} // namespace end
#endif /* WINWINDOWS_H */

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
##  @file X11Windows.hpp

File Description:
##  X11 windows (Xorg, or the Xwayland ones): _NET_CLIENT_LIST, XGetImage, _NET_ACTIVE_WINDOW
\**************************************************************/

#ifndef X11WINDOWS_H
    #define X11WINDOWS_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp> // _cold, _hot, _nodiscard
    #include "../IWindows.hpp" // xmouse::platform::IWindows

namespace xmouse::platform { // namespace start

//----------------------------------------------------------------//
/* CLASS */

class X11Windows final: public IWindows {
    private:
        void* _native = nullptr; // backend handle (the X11 Display*), nullptr elsewhere

    public:
        // ---------- Pre-Function -------- //
        _cold _nodiscard QList<WindowInfo> list(void) override;
        _cold _nodiscard std::optional<WindowInfo> info(const quint64 id) override;
        _hot _nodiscard QImage capture(const quint64 id) override;
        _cold void activate(const quint64 id) override;

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard QString name(void) const override {return "x11";};

        // ---------- Constructor --------- //
        X11Windows(void);

        // ----------- Destructor --------- //
        ~X11Windows() override;
};

} // namespace end
#endif /* X11WINDOWS_H */

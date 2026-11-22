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
##  @file MacInjector.hpp

File Description:
##  macOS input injection: CGEventPost of mouse, wheel, keys and unicode text (Accessibility permission)
\**************************************************************/

#ifndef MACINJECTOR_H
    #define MACINJECTOR_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>        // _cold, _hot, _nodiscard
    #include "../IInputInjector.hpp"  // xmouse::platform::IInputInjector
    #include <QString>                // QString
    #include <QPoint>                 // QPoint
    #include <QSet>                   // QSet

namespace xmouse::platform { // namespace start

//----------------------------------------------------------------//
/* CLASS */

class MacInjector final: public IInputInjector {
    private:
        QPoint _origin; // top left of the desktop (the injector gets coordinates from there)
        QPoint _at; // last position posted
        int _buttons = 0; // bits of the buttons held (drags)
        QSet<QString> _keys; // keys held (modifier flags, releaseAll)
        int _lastButton = -1; // double clicks: the click state counted here
        qint64 _lastClick = 0;
        int _clicks = 0;
        double _wheelX = 0; // part of a wheel step not posted yet
        double _wheelY = 0;
        QString _error = "";

        // ---------- Pre-Function -------- //
        _hot void postButton_(const net::MouseButton button, const bool down);
        _hot _nodiscard quint64 flags_(void) const; // CGEventFlags of the modifiers held

    public:
        // ---------- Pre-Function -------- //
        bool start(const QSize& screen) override;
        void mouseMove(const int x, const int y) override;
        void mouseButton(const net::MouseButton button, const bool down) override;
        void mouseWheel(const int dx, const int dy) override;
        void key(const QString& code, const bool down) override;
        void text(const QString& text) override;
        void releaseAll(void) override;
        _hot _nodiscard QString characterOf(const QString& code, const bool shift, const bool altgr) const override; // on the layout of the session (Option: the third level)
        _hot _nodiscard QString codeOf(const QString& character) const override; // the same
        _cold _nodiscard LockKeys lockKeys(void) const override; // a Mac has no Num Lock: its numpad types digits

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard QString name(void) const override  {return "cgevent";};
        _cold _nodiscard QString error(void) const override {return this->_error;};

        // ----------- Destructor --------- //
        ~MacInjector() override = default;
};

/* keymap */
_hot _nodiscard int mac_keycode(const QString& code); // KeyboardEvent.code -> kVK_*, -1 = unknown
_hot _nodiscard QString code_from_mac(const int keycode); // kVK_* -> KeyboardEvent.code ("" = unknown)

} // namespace end
#endif /* MACINJECTOR_H */

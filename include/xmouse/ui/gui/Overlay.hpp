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
##  @file Overlay.hpp

File Description:
##  Foreign cursors drawn over every app: one transparent window per screen, click-through
\**************************************************************/

#ifndef OVERLAY_H
    #define OVERLAY_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>          // _cold, _hot
    #include "../../core/Hub.hpp"       // xmouse::core::Hub, xmouse::core::Foreign
    #include <QPointer>                 // QPointer
    #include <QWidget>                  // QWidget
    #include <QScreen>                  // QScreen
    #include <QList>                    // QList

namespace xmouse::ui::gui { // namespace start

//----------------------------------------------------------------//
/* CLASS */

// One screen: shown only while a foreign cursor is on it
class OverlayWindow final: public QWidget {
    Q_OBJECT

    private:
        core::Hub& _hub;
        QPointer<QScreen> _screen;

    protected:
        // ---------- Pre-Function -------- //
        void paintEvent(QPaintEvent* event) override;

    public:
        // ---------- Pre-Function -------- //
        void refresh(void); // geometry, shown / hidden, repaint

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard inline QScreen* screen(void) const {return this->_screen;};

        // ---------- Constructor --------- //
        OverlayWindow(core::Hub& hub, QScreen* screen);

        // ----------- Destructor --------- //
        ~OverlayWindow() override = default;
};

// Every screen of this device
class Overlay final: public QObject {
    Q_OBJECT

    private:
        core::Hub& _hub;
        QList<OverlayWindow*> _windows;

        // ---------- Pre-Function -------- //
        void rebuild_(void); // screens plugged / removed

    public:
        // ---------- Pre-Function -------- //
        void refresh(void);
        _cold _nodiscard QList<QWidget*> windows(void) const; // never counted as the panel (click-through)

        // ---------- Constructor --------- //
        explicit Overlay(core::Hub& hub, QObject* parent = nullptr);

        // ----------- Destructor --------- //
        ~Overlay() override;
};

} // namespace end
#endif /* OVERLAY_H */

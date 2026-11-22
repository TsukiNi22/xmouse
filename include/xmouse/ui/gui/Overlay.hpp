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
##  Foreign cursors drawn over every app (ahead of a late packet), the "look here" waves, the laser trails and the
##  arrow to a followed member: one transparent window per screen, click-through
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
    #include <QTimer>                   // QTimer
    #include <QHash>                    // QHash
    #include <QList>                    // QList
    #include <QPair>                    // QPair

namespace xmouse::ui::gui { // namespace start

class Overlay;

//----------------------------------------------------------------//
/* CLASS */

// One screen of this device
class OverlayWindow final: public QWidget {
    Q_OBJECT

    private:
        core::Hub& _hub;
        Overlay& _owner;
        QPointer<QScreen> _screen;

        // ---------- Pre-Function -------- //
        void paintFollow_(QPainter& p, const double s); // arrow to the followed member when it is not on this screen

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
        OverlayWindow(core::Hub& hub, Overlay& owner, QScreen* screen);

        // ----------- Destructor --------- //
        ~OverlayWindow() override = default;
};

// Every screen of this device, and what is drawn on them
class Overlay final: public QObject {
    Q_OBJECT

    private:
        core::Hub& _hub;
        QList<OverlayWindow*> _windows;
        QTimer _anim; // 30 ms while a wave / a trail / a follow arrow is shown
        QHash<QString, QList<QPair<qint64, QPoint>>> _trails; // member -> laser points (ms, local desktop pixels)

        // ---------- Pre-Function -------- //
        void rebuild_(void); // screens plugged / removed
        void tick_(void); // trails sampled, animations

    public:
        // ---------- Pre-Function -------- //
        void refresh(void);
        _cold _nodiscard QList<QWidget*> windows(void) const; // never counted as the panel (click-through)
        _cold _nodiscard QPoint toLocal(const QPoint& global) const; // global space -> desktop pixels of this device
        _cold _nodiscard bool animating(void) const; // a wave, a trail or a follow arrow to draw

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard inline const QHash<QString, QList<QPair<qint64, QPoint>>>& trails(void) const {return this->_trails;};

        // ---------- Constructor --------- //
        explicit Overlay(core::Hub& hub, QObject* parent = nullptr);

        // ----------- Destructor --------- //
        ~Overlay() override;
};

} // namespace end
#endif /* OVERLAY_H */

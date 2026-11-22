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
##  @file DropShelf.hpp

File Description:
##  Drag and drop of files between devices: a drop shelf shows at an edge that opens on another device while a file is dragged there
\**************************************************************/

#ifndef DROPSHELF_H
    #define DROPSHELF_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _hot, _nodiscard
    #include "../../core/Hub.hpp"   // xmouse::core::Hub
    #include <QPointer>             // QPointer
    #include <QWidget>              // QWidget
    #include <QString>              // QString
    #include <QLabel>               // QLabel
    #include <QTimer>               // QTimer

namespace xmouse::ui::gui { // namespace start

//----------------------------------------------------------------//
/* CLASS */

// The shelf itself: a small window that takes the files dropped on it
class ShelfWindow final: public QWidget {
    Q_OBJECT

    private:
        QLabel* _text = nullptr;
        bool _hover = false;

    protected:
        // ---------- Pre-Function -------- //
        void paintEvent(QPaintEvent* event) override;
        void dragEnterEvent(QDragEnterEvent* event) override;
        void dragLeaveEvent(QDragLeaveEvent* event) override;
        void dropEvent(QDropEvent* event) override;

    public:
        // ------------ Function ---------- //
        /* setter */
        _cold inline void setText(const QString& text) {this->_text->setText(text);};

        /* getter */
        _cold _nodiscard inline bool hovered(void) const {return this->_hover;};

        // ---------- Constructor --------- //
        ShelfWindow(void);

        // ----------- Destructor --------- //
        ~ShelfWindow() override = default;

    signals:
        void dropped(const QStringList& files);
};

/*
 * Every 120 ms while the left button is held (the platform tells it: GNOME extension 2, Windows, macOS): the pointer
 * on an edge that opens on a reachable device shows the shelf there; the files dropped on it go to that device
*/
class DropShelf final: public QObject {
    Q_OBJECT

    private:
        core::Hub& _hub;
        QTimer _poll;
        QPointer<ShelfWindow> _shelf;
        QString _target = ""; // device the shelf sends to
        qint64 _released = 0; // ms the button went up (the shelf stays a little)

        // ---------- Pre-Function -------- //
        void poll_(void);

    public:
        // ---------- Constructor --------- //
        explicit DropShelf(core::Hub& hub, QObject* parent = nullptr);

        // ----------- Destructor --------- //
        ~DropShelf() override;
};

} // namespace end
#endif /* DROPSHELF_H */

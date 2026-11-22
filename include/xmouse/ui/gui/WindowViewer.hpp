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
##  @file WindowViewer.hpp

File Description:
##  A window of another device streamed here: its frames, and the mouse / keys of this window sent back to drive it
\**************************************************************/

#ifndef WINDOWVIEWER_H
    #define WINDOWVIEWER_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _hot, _nodiscard
    #include "../../core/Hub.hpp"   // xmouse::core::Hub
    #include <QPushButton>          // QPushButton
    #include <QWidget>              // QWidget
    #include <QString>              // QString
    #include <QImage>               // QImage
    #include <QLabel>               // QLabel
    #include <QRect>                // QRect

namespace xmouse::ui::gui { // namespace start

//----------------------------------------------------------------//
/* CLASS */

class WindowViewer final: public QWidget {
    Q_OBJECT

    private:
        core::Hub& _hub;
        QString _stream = ""; // id of the stream
        QImage _frame;
        QSize _source; // size of the window on its device (input coordinates)
        QRect _shown; // where the frame is drawn (widget pixels)
        QWidget* _bar = nullptr;
        QLabel* _info = nullptr;
        QPushButton* _open = nullptr; // the document it shows, opened here (handoff)
        qint64 _lastMove = 0; // ms (moves sent at most every 16 ms)
        bool _closing = false; // the stream ended: no stop sent back

        // ---------- Pre-Function -------- //
        _hot _nodiscard QPoint toSource_(const QPointF& widget) const; // widget pixels -> window pixels on its device
        _hot void send_(const QJsonObject& input);

    protected:
        // ---------- Pre-Function -------- //
        void paintEvent(QPaintEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void wheelEvent(QWheelEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;
        void keyReleaseEvent(QKeyEvent* event) override;
        void closeEvent(QCloseEvent* event) override;

    public:
        // ---------- Pre-Function -------- //
        void setFrame(const QImage& frame, const QSize& source);
        void ended(void); // closed by the source / the network

        // ---------- Constructor --------- //
        WindowViewer(core::Hub& hub, const QString& id, const QString& from, const QString& title, const QString& document);

        // ----------- Destructor --------- //
        ~WindowViewer() override = default;
};

} // namespace end
#endif /* WINDOWVIEWER_H */

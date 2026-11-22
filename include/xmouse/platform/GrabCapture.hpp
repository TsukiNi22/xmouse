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
##  @file GrabCapture.hpp

File Description:
##  Edges watched with the cursor position, capture by a grab of an invisible window (X11, macOS)
\**************************************************************/

#ifndef GRABCAPTURE_H
    #define GRABCAPTURE_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "IInputCapture.hpp"    // xmouse::platform::IInputCapture
    #include <QPointer>             // QPointer
    #include <QCursor>              // QCursor
    #include <QWidget>              // QWidget
    #include <QTimer>               // QTimer
    #include <QPoint>               // QPoint

namespace xmouse::platform { // namespace start

//----------------------------------------------------------------//
/* CLASS */

class GrabCapture final: public IInputCapture {
    Q_OBJECT

    private:
        QTimer _poll; // cursor position, 125 Hz
        QList<core::Edge> _edges;
        QPoint _last; // previous position (direction of the move)
        QPoint _center; // where the pointer is held while captured
        QPointer<QWidget> _grab; // invisible window holding the mouse and the keyboard
        bool _capturing = false;
        QString _error = "";

        // ---------- Pre-Function -------- //
        void onPoll_(void);
        void capture_(const QPoint& at, const QPoint& push);

    public:
        // ---------- Pre-Function -------- //
        bool start(void) override;
        void stop(void) override;
        void release(const QPoint& local) override;
        void moved_(const QPoint& position); // the grab window saw the pointer there (internal)

        // ------------ Function ---------- //
        /* setter */
        void setEdges(const QList<core::Edge>& edges) override {this->_edges = edges;};

        /* getter */
        _cold _nodiscard bool capturing(void) const override    {return this->_capturing;};
        _cold _nodiscard QString name(void) const override      {return "grab";};
        _cold _nodiscard QString error(void) const override     {return this->_error;};
        _cold _nodiscard bool knowsPointer(void) const override {return true;};
        _cold _nodiscard QPoint pointer(void) const override    {return QCursor::pos();};

        // ---------- Constructor --------- //
        explicit GrabCapture(QObject* parent = nullptr);

        // ----------- Destructor --------- //
        ~GrabCapture() override;
};

} // namespace end
#endif /* GRABCAPTURE_H */

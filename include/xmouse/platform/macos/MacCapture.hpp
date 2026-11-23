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
##  @file MacCapture.hpp

File Description:
##  macOS edges: a CGEventTap watches the pointer, a push on an edge dissociates and hides it, the events go to the signals
\**************************************************************/

#ifndef MACCAPTURE_H
    #define MACCAPTURE_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _hot, _nodiscard
    #include "../IInputCapture.hpp" // xmouse::platform::IInputCapture
    #include <QString>              // QString
    #include <QPoint>               // QPoint

namespace xmouse::platform { // namespace start

//----------------------------------------------------------------//
/* CLASS */

/*
 * Needs the Accessibility permission (System Settings > Privacy & Security > Accessibility): the tap is refused without
 * it and error() says so. While captured, the pointer is dissociated from the mouse (CGAssociateMouseAndMouseCursorPosition)
 * and hidden: the moves arrive as deltas, nothing reaches the apps of the Mac
*/
class MacCapture final: public IInputCapture {
    Q_OBJECT

    private:
        void* _tap = nullptr; // CFMachPortRef
        void* _source = nullptr; // CFRunLoopSourceRef
        QList<core::Edge> _edges;
        QPoint _last; // previous pointer (direction of the move)
        bool _capturing = false;
        quint64 _flags = 0; // modifier flags of the last flagsChanged (which key went down / up)
        QString _error = "";

    public:
        // ---------- Pre-Function -------- //
        bool start(void) override;
        void stop(void) override;
        void release(const QPoint& local) override;
        _cold _nodiscard QPoint pointer(void) const override;
        _cold _nodiscard int heldButtons(void) const override;
        _hot bool onEvent_(const unsigned int type, void* event); // true = swallowed (internal: called by the tap)
        _cold void reenable_(void); // the tap was disabled by a timeout (internal)

        // ------------ Function ---------- //
        /* setter */
        void setEdges(const QList<core::Edge>& edges) override {this->_edges = edges;};

        /* getter */
        _cold _nodiscard bool capturing(void) const override    {return this->_capturing;};
        _cold _nodiscard QString name(void) const override      {return "event-tap";};
        _cold _nodiscard QString error(void) const override     {return this->_error;};
        _cold _nodiscard bool knowsPointer(void) const override {return true;};
        _cold _nodiscard bool takesInjected(void) const override {return false;}; // the events posted by xmouse are skipped (their pid)

        // ---------- Constructor --------- //
        explicit MacCapture(QObject* parent = nullptr): IInputCapture(parent) { /* Nothing */ };

        // ----------- Destructor --------- //
        ~MacCapture() override;
};

} // namespace end
#endif /* MACCAPTURE_H */

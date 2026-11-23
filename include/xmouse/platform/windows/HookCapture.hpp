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
##  @file HookCapture.hpp

File Description:
##  Windows edges and capture: low level mouse / keyboard hooks (WH_MOUSE_LL, WH_KEYBOARD_LL)
\**************************************************************/

#ifndef HOOKCAPTURE_H
    #define HOOKCAPTURE_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>          // _cold, _nodiscard
    #include "../IInputCapture.hpp"     // xmouse::platform::IInputCapture
    #include <windows.h>                // HHOOK, POINT
    #include <QPoint>                   // QPoint

namespace xmouse::platform { // namespace start

//----------------------------------------------------------------//
/* CLASS */

/*
 * Idle: every move is checked against the openings, the events pass
 * Captured: the cursor stays frozen where it crossed, the moves (the point it would go to) become deltas, nothing passes
 * The events injected (SendInput: the foreign cursors on this PC) always pass
*/
class HookCapture final: public IInputCapture {
    Q_OBJECT

    private:
        HHOOK _mouse = nullptr;
        HHOOK _keyboard = nullptr;
        HWINEVENTHOOK _moveHook = nullptr; // start / end of a window being moved or resized
        HWND _moving = nullptr; // window being dragged now
        HWND _sent = nullptr; // window already given to windowCrossed during this drag
        bool _windowMove = false; // a dragged window on an edge goes to the other device (else the usual border)
        QList<core::Edge> _edges;
        POINT _frozen = {0, 0}; // where the cursor is held
        POINT _last = {0, 0};
        bool _capturing = false;
        QString _error = "";

        // ---------- Pre-Function -------- //
        static LRESULT CALLBACK mouseProc_(int code, WPARAM wParam, LPARAM lParam);
        static LRESULT CALLBACK keyboardProc_(int code, WPARAM wParam, LPARAM lParam);
        static void CALLBACK windowProc_(HWINEVENTHOOK hook, DWORD event, HWND window, LONG object, LONG child, DWORD thread, DWORD time);
        _hot bool onMouse_(const WPARAM message, const MSLLHOOKSTRUCT* data); // true = swallowed
        _hot bool onKey_(const WPARAM message, const KBDLLHOOKSTRUCT* data);

    public:
        // ---------- Pre-Function -------- //
        bool start(void) override;
        void stop(void) override;
        void release(const QPoint& local) override;
        _cold _nodiscard QPoint pointer(void) const override;
        _cold _nodiscard int heldButtons(void) const override; // GetAsyncKeyState of the buttons
        _cold void setWindowMove(const bool enabled) override {this->_windowMove = enabled;};

        // ------------ Function ---------- //
        /* setter */
        void setEdges(const QList<core::Edge>& edges) override {this->_edges = edges;};

        /* getter */
        _cold _nodiscard bool capturing(void) const override    {return this->_capturing;};
        _cold _nodiscard QString name(void) const override      {return "hooks";};
        _cold _nodiscard QString error(void) const override     {return this->_error;};
        _cold _nodiscard bool knowsPointer(void) const override {return true;};
        _cold _nodiscard bool takesInjected(void) const override {return false;}; // the injected events are marked: skipped

        // ---------- Constructor --------- //
        explicit HookCapture(QObject* parent = nullptr): IInputCapture(parent) { /* Nothing */ };

        // ----------- Destructor --------- //
        ~HookCapture() override;
};

} // namespace end
#endif /* HOOKCAPTURE_H */

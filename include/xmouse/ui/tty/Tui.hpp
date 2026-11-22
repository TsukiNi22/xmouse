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
##  @file Tui.hpp

File Description:
##  Terminal panel: status, code, members, commands typed (join, accept, private, layout...)
\**************************************************************/

#ifndef TUI_H
    #define TUI_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>          // _cold, _nodiscard
    #include "../../core/Hub.hpp"       // xmouse::core::Hub
    #include <QPointer>                 // QPointer
    #include <iostream>                 // std::cout
    #include <QObject>                  // QObject
    #include <QString>                  // QString
    #include <thread>                   // std::jthread

namespace xmouse::ui::tty { // namespace start

//----------------------------------------------------------------//
/* CLASS */

class Tui final: public QObject {
    Q_OBJECT

    private:
        core::Hub& _hub;
        std::jthread _reader; // stdin lines (portable: no socket notifier on a Windows console)
        QPointer<net::Peer> _pendingJoin; // device waiting the accept / refuse
        QString _pendingAccess = ""; // foreign cursor asking the panel

        // ---------- Pre-Function -------- //
        void command_(const QString& line);
        void status_(void) const;
        void members_(void) const;
        void layout_(void) const;
        void help_(void) const;

        // ------------ Function ---------- //
        /* tools */
        _cold inline void print_(const QString& text) const {std::cout << text.toStdString() << std::endl;};

    public:
        // ---------- Pre-Function -------- //
        void start(void); // banner + reader thread

        // ---------- Constructor --------- //
        explicit Tui(core::Hub& hub, QObject* parent = nullptr);

        // ----------- Destructor --------- //
        ~Tui() override;
};

} // namespace end
#endif /* TUI_H */

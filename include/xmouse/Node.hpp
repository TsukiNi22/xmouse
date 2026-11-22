/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 06/09/2026 by @author Tsukini

File Name:
##  @file Node.hpp

File Description:
##  Command line of xmouse: the panel (graphical or terminal), join, scan, paths
\**************************************************************/

#ifndef NODE_H
    #define NODE_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "core/Config.hpp"      // xmouse::core::Config
    #include <string>               // std::string

namespace xmouse { // namespace start

//----------------------------------------------------------------//
/* CLASS */

class Node {
    private:
        core::Config _config;
        int _argc = 0;
        char** _argv = nullptr;
        std::string _command = "run"; // run | join | scan | paths | version
        std::string _target = ""; // join: address or xmouse:// link
        std::string _code = ""; // join: code shown on the member
        bool _tty = false;
        bool _gui = false;
        bool _regenCert = false;
        int _port = 0; // 0 = the one of the settings
        std::string _name = ""; // name of this device for this run
        int _exit = 0;

        // ---------- Pre-Function -------- //
        void runScan_(void);
        void runPaths_(void);

    public:
        // ---------- Pre-Function -------- //
        void init(const int argc, const char* const argv[]);
        void run(void);

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard inline int exit(void) const {return this->_exit;};

        // ------------ Operator ---------- //
        Node& operator=(const Node& other) = delete;
        Node& operator=(Node&& other) = delete;

        // ---------- Constructor --------- //
        Node() = default;
        Node(const Node& other) = delete;
        Node(Node&& other) = delete;

        // ----------- Destructor --------- //
        ~Node() = default;
};

} // namespace end
#endif /* NODE_H */

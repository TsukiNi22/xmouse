/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 14/09/2026 by @author Tsukini

File Name:
##  @file Node-init.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#define _Exception
#define _Verbose
#define _Arguments
#include <utils/utils.hpp>
#include "xmouse/Node.hpp"
#include <algorithm>
#include <optional>
#include <string>
#include <vector>

_nodiscard static std::optional<std::string> VerboseParsingHook(const std::string& option)
{
    if (option == "none" || option == "basic" || option == "advanced" || option == "debug") return std::nullopt;
    return "Invalid verbose level, should be (none|basic|advanced|debug), but got: " + option;
}

void xmouse::Node::init(const int argc, const char* const argv[])
{
    static const std::vector<std::string> COMMANDS = {"run", "join", "scan", "paths"};
    static const std::vector<std::string> VALUE_FLAGS = {"-p", "--port", "-n", "--name", "-v", "--verbose"};
    bool commandSeen = false;
    utils::arguments::ArgParser parser = utils::arguments::ArgParser("xmouse",
        "Network of screens: the cursor, the keyboard and the clipboard of every device go from one screen to the other,\n"
        "every link secured end to end (TLS 1.3 + ML-KEM post-quantum key exchange, Argon2id codes).\n"
        "Commands:\n"
        "  xmouse                          the panel (graphical, or in the terminal with --tty / without display)\n"
        "  xmouse join <address> [code]    join the network of a member (address[:port] or its xmouse:// link)\n"
        "  xmouse scan                     list the devices announced on the LAN\n"
        "  xmouse paths                    print the configuration, data and log folders");
    std::vector<std::string> args;
    std::vector<const char*> cargs;

    this->_argc = argc;
    this->_argv = const_cast<char**>(argv);

    // Command and its targets (the parser only sees the flags): anywhere, but never the value of a flag
    args.push_back(argv[0]);
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool afterValueFlag = std::find(VALUE_FLAGS.begin(), VALUE_FLAGS.end(), args.back()) != VALUE_FLAGS.end();
        if (!commandSeen && !afterValueFlag && std::find(COMMANDS.begin(), COMMANDS.end(), arg) != COMMANDS.end()) {this->_command = arg; commandSeen = true; continue;}
        if (this->_command == "join" && !arg.starts_with('-') && (this->_target.empty() || this->_code.empty()) && !afterValueFlag) {
            if (this->_target.empty()) this->_target = arg;
            else this->_code = arg;
            continue;
        }
        args.push_back(arg);
    }

    // Flags
    parser.setDefaultUsage();
    parser.setFlag("verbose", {"v", "", "verbose", "XMOUSE_VERBOSE"}, {{"level", true, VerboseParsingHook}}, "Verbose level none|basic|advanced|debug (default: none)");
    parser.setFlag("tty", {"t", "", "tty", ""}, {}, "Terminal panel (the cursors are still drawn when a display exists)");
    parser.setFlag("gui", {"g", "", "gui", ""}, {}, "Graphical panel (Qt)");
    parser.setFlag("port", {"p", "", "port", "XMOUSE_PORT"}, {{"port", true, utils::arguments::defaultInt32ParsingHook}}, "Port of this run (default 47900)");
    parser.setFlag("name", {"n", "", "name", "XMOUSE_NAME"}, {{"name", true, utils::arguments::defaultTrueParsingHook}}, "Name of this device for this run");
    parser.setFlag("regen-cert", {"", "", "regen-cert", ""}, {}, "New identity of this device (the members must see it join again)");
    parser.setFlag("version", {"V", "", "version", ""}, {}, "Print the version");
    for (const std::string& a: args)
        cargs.push_back(a.c_str());
    utils::arguments::ParsedUsages usages = parser.parse(static_cast<int>(cargs.size()), cargs.data());
    set_verbose(None)
    for (const auto& [id, _, options]: usages.front().arguments) {
        const std::string& value = (options.empty() ? "" : options.front());
        if (id == "verbose") {
            if      (value == "none")     set_verbose(None)
            else if (value == "basic")    set_verbose(Basic)
            else if (value == "advanced") set_verbose(Advanced)
            else if (value == "debug")    set_verbose(Debug)
        } else if (id == "tty")        this->_tty = true;
        else if (id == "gui")        this->_gui = true;
        else if (id == "port")       this->_port = std::stoi(value);
        else if (id == "name")       this->_name = value;
        else if (id == "regen-cert") this->_regenCert = true;
        else if (id == "version")    this->_command = "version";
    }
    if (this->_port < 0 || this->_port > 65535) throw utils::exception::ErrorException(utils::exception::ExternalCode::Usage, "Invalid port " + std::to_string(this->_port));
    if (this->_command == "join" && this->_target.empty()) throw utils::exception::ErrorException(utils::exception::ExternalCode::Usage, "xmouse join <address|xmouse:// link> [code]");
    this->_config.load();
    onBasicVerbose("xmouse " XMOUSE_VERSION " - command " + this->_command);
}

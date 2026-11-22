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
##  @file Config.hpp

File Description:
##  Local settings of the device (config.json) and the folders of xmouse
\**************************************************************/

#ifndef CONFIG_H
    #define CONFIG_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>  // _cold, _nodiscard
    #include <QJsonObject>      // QJsonObject
    #include <QStringList>      // QStringList
    #include <QString>          // QString
    #include <cstdint>          // std::uint16_t
    #include <QHash>            // QHash

namespace xmouse::core { // namespace start

//----------------------------------------------------------------//
/* ENUM */

// How the device is reachable from the other members
enum class Exposure {
    Lan, // every local interface, LAN discovery
    Tailscale, // the tailnet address of the device (end to end, WireGuard + TLS)
    Upnp, // port opened on the router with UPnP (upnpc), public address
    Bore, // tcp tunnel bore.pub (TLS kept end to end)
    Cloudflared, // https tunnel trycloudflare.com (the relay terminates the outer TLS)
    Ngrok, // tcp tunnel ngrok (TLS kept end to end, account needed)
    Manual, // port forwarded by hand, public address given in the settings
};

//----------------------------------------------------------------//
/* STRUCT */

// What a member may do on this device (decided here only, never by the others)
struct Rights {
    bool mouse = true; // its cursor moves and clicks here
    bool keyboard = true; // its keyboard types here (where its cursor clicked)
    bool clipboard = true; // its copies land in the clipboard of this device
    bool files = true; // its files can be dropped / pasted here
    bool links = true; // the links it sends are shown here (opened after a click, or at once with links = "open")
    bool windows = true; // it may stream a window here / drive the window this device streams to it
};

struct Settings {
    /* identity */
    QString name = ""; // name shown to the others ("" = host name)

    /* network */
    std::uint16_t port = 47900; // https + wss port
    QString bind = "0.0.0.0"; // listening address ("::" for IPv6)
    Exposure exposure = Exposure::Lan; // how the device is reachable
    QString publicHost = ""; // Manual: public address / domain given to the others
    bool discovery = true; // udp broadcast on the LAN (members found without an address)
    bool udp = true; // the moves and the presence go through encrypted udp when it passes (tcp otherwise)
    bool relay = true; // this device carries the messages of two members that cannot reach each other

    /* access */
    int codeLength = 9; // characters of the pairing code (Crockford base32, 5 bits each)
    int codeLifetime = 300; // seconds before the code rotates (0 = only after use)
    bool joinApproval = true; // a device joining with the code must also be accepted here
    int maxAuthFailures = 5; // failed codes of one address before its ban (1 h)
    bool focusRequest = true; // a member paused by the focus of the panel may ask its control back
    int focusRequestDelay = 60; // seconds before that request is granted (time to refuse / unplug)
    bool privateMode = false; // every foreign cursor is refused (they stay at the edge)
    QHash<QString, Rights> rights; // <member id, rights> (missing = the defaults)

    /* input */
    QString releaseShortcut = "Ctrl+Alt+Shift"; // brings the cursor back on this device at once
    QString foreignMode = "pointer"; // a foreign cursor here: "pointer" moves the real pointer (default), "phantom" is drawn over the apps (the real pointer only jumps for a click)
    bool restorePointer = true; // phantom mode: the real pointer goes back where it was after a foreign click (when the platform tells it)
    int edgeDelay = 0; // ms the cursor must push on an edge before it crosses (0 = at once)
    bool clipboard = true; // copies of this device go to the members (their rights decide)
    /* files and links */
    int clipboardFilesLimit = 64; // MiB: copied files downloaded at once by the members (bigger: drag them)
    int filesLimit = 4096; // MiB: biggest transfer accepted here
    QString downloadDir = ""; // received files ("" = Downloads/xmouse)
    QString links = "ask"; // a link sent by a member: ask | open | never
    /* phones and web pads */
    bool pads = true; // a phone / browser may pair with the code and drive this device
    /* windows */
    int windowFps = 12; // frames per second of a window streamed to another device

    /* ui */
    QString theme = "system"; // system | dark | light
    QString language = "en"; // en | fr
    bool startMinimized = false; // tray only at start (gui)
};

//----------------------------------------------------------------//
/* PROTOTYPE */

/* names */
_cold _nodiscard QString exposure_name(const Exposure exposure); // "lan" | "tailscale" ...
_cold _nodiscard Exposure exposure_from(const QString& name); // unknown -> Lan
_cold _nodiscard QJsonObject rights_to_json(const Rights& rights);
_cold _nodiscard Rights rights_from_json(const QJsonObject& object); // missing keys -> defaults

//----------------------------------------------------------------//
/* CLASS */

class Config {
    private:
        Settings _settings; // current values
        QString _path; // config.json

    public:
        // ---------- Pre-Function -------- //
        void load(void); // read config.json (written with the defaults when missing)
        void save(void) const; // write config.json (0600)
        _cold _nodiscard QString deviceName(void) const; // settings name, else the host name

        /* folders */
        _cold _nodiscard static QString configDir(void); // ~/.config/xmouse | %APPDATA%/xmouse
        _cold _nodiscard static QString dataDir(void); // ~/.local/share/xmouse | %LOCALAPPDATA%/xmouse (certificate, group)
        _cold _nodiscard static QString logDir(void); // <data>/logs (audit jsonl)
        _cold _nodiscard static QString executablePath(void); // absolute path of the running binary
        _cold static void ensureDir(const QString& path); // created 0700, throws Io

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard inline Settings& settings(void)                     {return this->_settings;};
        _cold _nodiscard inline const Settings& settings(void) const         {return this->_settings;};
        _cold _nodiscard inline const QString& path(void) const              {return this->_path;};
        _cold _nodiscard inline Rights rightsOf(const QString& member) const {return this->_settings.rights.value(member, Rights());}; // defaults when not set

        // ------------ Operator ---------- //
        Config& operator=(const Config& other) = delete;
        Config& operator=(Config&& other) = delete;

        // ---------- Constructor --------- //
        Config(void); // path from configDir(), nothing read yet
        explicit Config(const QString& path): _path{path} { /* Nothing */ }; // another file (tests)
        Config(const Config& other) = delete;
        Config(Config&& other) = delete;

        // ----------- Destructor --------- //
        ~Config() = default;
};

} // namespace end
#endif /* CONFIG_H */

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
##  @file Protocol.hpp

File Description:
##  Wire protocol of xmouse: message types, constants and the binary reader / writer
\**************************************************************/

#ifndef PROTOCOL_H
    #define PROTOCOL_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>  // _hot, _cold, _nodiscard
    #include <QByteArray>       // QByteArray
    #include <QString>          // QString
    #include <cstdint>          // std::uint8_t, std::uint16_t, std::uint32_t, std::int16_t, std::int32_t

    //----------------------------------------------------------------//
    /* DEFINE */

    /* version */
    #define XMOUSE_PROTOCOL_VERSION 1 // bumped on every incompatible change of the wire format
    #define XMOUSE_DEFAULT_PORT 47900 // https + wss port of every device
    #define XMOUSE_DISCOVERY_PORT 47901 // udp broadcast of the members on the LAN

    /* limits */
    #define XMOUSE_MAX_MESSAGE (24 * 1024 * 1024) // biggest websocket message accepted (an image of the clipboard)
    #define XMOUSE_MAX_CONTROL (256 * 1024) // biggest json control message accepted (the replicated state)
    #define XMOUSE_MAX_CLIPBOARD (16 * 1024 * 1024) // biggest clipboard content shared (text or image)

    /* timings */
    #define XMOUSE_PING_INTERVAL 2000 // ms between two pings of a link (latency, dead link detection)
    #define XMOUSE_LINK_TIMEOUT 8000 // ms without anything received before a link is dropped
    #define XMOUSE_PRESENCE_INTERVAL 50 // ms between two presence updates of a moving cursor (20 Hz)
    #define XMOUSE_UDP_PROBE_INTERVAL 1000 // ms between two udp probes of a member
    #define XMOUSE_UDP_TIMEOUT 3500 // ms without an udp answer before the moves go back to tcp

    /* relay, files, windows */
    #define XMOUSE_RELAY_HOPS 3 // members a relayed message may cross
    #define XMOUSE_FILE_CHUNK (128 * 1024) // bytes of a file chunk
    #define XMOUSE_FILE_WINDOW (2 * 1024 * 1024) // bytes waiting on a link before the next chunk waits (back pressure)
    #define XMOUSE_MAX_FILES 1000 // files of one transfer
    #define XMOUSE_MAX_FRAME (4 * 1024 * 1024) // biggest frame of a streamed window (jpeg)

namespace xmouse::net { // namespace start

//----------------------------------------------------------------//
/* ENUM */

/*
 * Every websocket message is either:
 *  - a text frame: one JSON object {"t": "<type>", ...} (control, see docs/protocol)
 *  - a binary frame: [u8 type][payload] big endian, the types below (the hot path of the cursors)
*/
enum class Binary: std::uint8_t {
    Pointer = 0x20, // owner -> target u16 seq, i32 x, i32 y (absolute, logical pixels of the desktop of the target)
    Button = 0x21, // owner -> target u8 button (MouseButton), u8 down [, i32 x, i32 y: where (the moves may come by udp)]
    Wheel = 0x22, // owner -> target i16 dx, i16 dy (1/120 of a notch, like Qt angleDelta)
    Key = 0x23, // owner -> target u8 down, u8 length, code (KeyboardEvent.code: "KeyA", "ShiftLeft"...)
    Text = 0x24, // owner -> target utf8 text typed (soft keyboards, IME)
    Presence = 0x30, // owner -> every member u16 seq, u8 length, device id of the screen, i32 x, i32 y (global layout space), u8 flags (Presence*)
    UdpProbe = 0x31, // udp only: u8 0 = ping | 1 = pong (the path works both ways)
    FileChunk = 0x40, // sender -> receiver text8 transfer, u16 file, u32 offset high, u32 offset low, data
    Relay = 0x50, // through a member: text8 to, text8 from, u8 hops, the inner binary message
    WindowFrame = 0x60, // source -> viewer text8 stream, u16 width, u16 height, jpeg
};

enum class MouseButton: std::uint8_t {
    Left = 0,
    Middle = 1,
    Right = 2,
    Back = 3,
    Forward = 4,
};

// How a device takes part in the network
enum class DeviceKind: std::uint8_t {
    Desktop = 0, // native app (Qt panel or terminal panel)
    Phone = 1, // android application
    Web = 2, // browser (a controller only: it cannot draw over the other apps)
};

//----------------------------------------------------------------//
/* DEFINE */

    /* presence flags */
    #define XMOUSE_PRESENCE_DOWN 0x01 // a button is held (drag)
    #define XMOUSE_PRESENCE_HIDDEN 0x02 // the cursor is back on its own device (not drawn by the others)
    #define XMOUSE_PRESENCE_PING 0x04 // "look here" wave asked
    #define XMOUSE_PRESENCE_LASER 0x08 // laser pointer: its trail is drawn where it goes

//----------------------------------------------------------------//
/* PROTOTYPE */

/* kind */
_cold _nodiscard QString device_kind_name(const DeviceKind kind); // "desktop" | "phone" | "web"
_cold _nodiscard DeviceKind device_kind_from(const QString& name); // unknown -> Web (the most restricted)
_cold _nodiscard QString mouse_button_name(const MouseButton button); // "left" | "middle" ...

//----------------------------------------------------------------//
/* CLASS */

// Append big endian values to a binary message
class ByteWriter {
    private:
        QByteArray _data; // message being built

    public:
        // ------------ Function ---------- //
        /* setter */
        _hot inline ByteWriter& u8(const std::uint8_t v)   {this->_data.append(static_cast<char>(v)); return *this;};
        _hot inline ByteWriter& u16(const std::uint16_t v) {this->u8(static_cast<std::uint8_t>(v >> 8)); return this->u8(static_cast<std::uint8_t>(v));};
        _hot inline ByteWriter& u32(const std::uint32_t v) {this->u16(static_cast<std::uint16_t>(v >> 16)); return this->u16(static_cast<std::uint16_t>(v));};
        _hot inline ByteWriter& i16(const std::int16_t v)  {return this->u16(static_cast<std::uint16_t>(v));};
        _hot inline ByteWriter& i32(const std::int32_t v)  {return this->u32(static_cast<std::uint32_t>(v));};
        _hot inline ByteWriter& bytes(const QByteArray& v) {this->_data.append(v); return *this;};
        _hot inline ByteWriter& text8(const QString& v)    {const QByteArray s = v.toUtf8().left(255); this->u8(static_cast<std::uint8_t>(s.size())); return this->bytes(s);}; // u8 length + utf8

        /* getter */
        _hot _nodiscard inline QByteArray& data(void) {return this->_data;};

        // ---------- Constructor --------- //
        explicit ByteWriter(const Binary type, const std::size_t reserve = 32) {this->_data.reserve(static_cast<qsizetype>(reserve)); this->u8(static_cast<std::uint8_t>(type));}; // start a message of this type
};

// Read big endian values of a binary message, every read is bound checked (ok() false after an overflow)
class ByteReader {
    private:
        const QByteArray& _data; // message read (must outlive the reader)
        qsizetype _pos = 1; // after the type byte
        bool _ok = true; // false once a read went past the end

    public:
        // ------------ Function ---------- //
        /* getter */
        _hot _nodiscard inline bool ok(void) const             {return this->_ok;};
        _hot _nodiscard inline qsizetype remaining(void) const {return this->_data.size() - this->_pos;};

        /* parsing */
        _hot _nodiscard inline std::uint8_t u8(void)
        {
            if (this->_pos + 1 > this->_data.size()) {this->_ok = false; return 0;}
            return static_cast<std::uint8_t>(this->_data[this->_pos++]);
        };
        _hot _nodiscard inline std::uint16_t u16(void) {const std::uint16_t hi = this->u8(); return static_cast<std::uint16_t>((hi << 8) | this->u8());};
        _hot _nodiscard inline std::uint32_t u32(void) {const std::uint32_t hi = this->u16(); return (hi << 16) | this->u16();};
        _hot _nodiscard inline std::int16_t i16(void)  {return static_cast<std::int16_t>(this->u16());};
        _hot _nodiscard inline std::int32_t i32(void)  {return static_cast<std::int32_t>(this->u32());};
        _hot _nodiscard inline QByteArray bytes(const qsizetype size)
        {
            if (size < 0 || this->_pos + size > this->_data.size()) {this->_ok = false; return {};}
            const QByteArray out = this->_data.mid(this->_pos, size);
            this->_pos += size;
            return out;
        };
        _hot _nodiscard inline QString text8(void)   {const std::uint8_t size = this->u8(); return QString::fromUtf8(this->bytes(size));};
        _hot _nodiscard inline QByteArray rest(void) {return this->bytes(this->remaining());};

        // ---------- Constructor --------- //
        explicit ByteReader(const QByteArray& data): _data{data} { /* Nothing */ }; // the type byte is skipped
};

} // namespace end
#endif /* PROTOCOL_H */

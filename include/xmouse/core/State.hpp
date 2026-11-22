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
##  @file State.hpp

File Description:
##  State replicated between the members (last writer wins per key, Lamport clocks)
\**************************************************************/

#ifndef STATE_H
    #define STATE_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>  // _hot, _cold, _nodiscard
    #include <QJsonObject>      // QJsonObject
    #include <QStringList>      // QStringList
    #include <QJsonValue>       // QJsonValue
    #include <QString>          // QString
    #include <cstdint>          // std::uint64_t
    #include <QHash>            // QHash

namespace xmouse::core { // namespace start

//----------------------------------------------------------------//
/* STRUCT */

struct StateEntry {
    QJsonValue value = QJsonValue::Null; // Null + deleted = removed key (kept as a tombstone)
    std::uint64_t clock = 0; // Lamport clock of the write
    QString author = ""; // device id that wrote it (tie break of two writes with the same clock)
    bool deleted = false;
};

//----------------------------------------------------------------//
/* CLASS */

/*
 * Keys used by xmouse:
 *  device/<id>   -> {"name", "os", "kind", "fp", "port", "addresses": [], "screens": [{"name", "x", "y", "w", "h", "scale"}], "joined"}
 *  layout/<id>   -> {"x", "y"} position of the desktop of the device in the global space
 *  removed/<id>  -> true (a removed member, refused everywhere even with the old key)
 *  net/<name>    -> network wide settings ("net/windowMove": bool, "net/name": string)
 * A delta = {"<key>": {"v": value, "c": clock, "a": author, "d": deleted}, ...}
*/
class State {
    private:
        QHash<QString, StateEntry> _entries;
        std::uint64_t _clock = 0; // highest clock seen
        QString _self = ""; // device id of the writes made here

        // ---------- Pre-Function -------- //
        _hot _nodiscard static bool newer_(const StateEntry& a, const StateEntry& b); // a wins over b

    public:
        // ---------- Pre-Function -------- //
        _hot QJsonObject set(const QString& key, const QJsonValue& value); // local write, returns the delta to broadcast ({} = same value: nothing to send)
        _hot QJsonObject remove(const QString& key); // tombstone, returns the delta
        _hot QStringList merge(const QJsonObject& delta); // remote writes, returns the keys whose value changed
        _hot _nodiscard QJsonValue value(const QString& key) const; // Null when missing / removed
        _cold _nodiscard QStringList keys(const QString& prefix = "") const; // live keys starting with prefix
        _cold _nodiscard QStringList ids(const QString& prefix) const; // "device/" -> the ids after the prefix
        _cold _nodiscard QJsonObject toJson(void) const; // full state as a delta (tombstones included)
        _cold void load(const QString& path); // missing file: empty state
        _cold void save(const QString& path) const; // 0600

        // ------------ Function ---------- //
        /* setter */
        _cold inline void setSelf(const QString& self) {this->_self = self;};
        _cold inline void clear(void)                  {this->_entries.clear(); this->_clock = 0;};

        /* getter */
        _cold _nodiscard inline std::uint64_t clock(void) const         {return this->_clock;};
        _cold _nodiscard inline bool contains(const QString& key) const {return this->_entries.contains(key) && !this->_entries.value(key).deleted;};
};

} // namespace end
#endif /* STATE_H */

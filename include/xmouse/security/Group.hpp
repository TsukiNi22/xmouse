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
##  @file Group.hpp

File Description:
##  Identity of this device and of the network it belongs to (group key, proofs of the members)
\**************************************************************/

#ifndef GROUP_H
    #define GROUP_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>  // _cold, _nodiscard
    #include "Crypto.hpp"       // xmouse::security::hmac_sha256
    #include <QByteArray>       // QByteArray
    #include <QString>          // QString

    //----------------------------------------------------------------//
    /* DEFINE */

    #define XMOUSE_GROUP_KEY_SIZE 32 // bytes of the shared secret of a network

namespace xmouse::security { // namespace start

//----------------------------------------------------------------//
/* CLASS */

/*
 * Every member holds the same 256 bits group key: it proves to the others that it is a member
 * (HMAC of the transcript of the TLS session: nonces + certificate fingerprint of the server)
 * Removing a member rotates the key (epoch + 1), the new key only goes to the remaining members
 * Files: <data>/device.json (id of this device), <data>/group.json (0600)
*/
class Group {
    private:
        QString _dir; // data folder
        QString _self = ""; // id of this device (random, stable)
        QString _id = ""; // id of the network ("" = not in a network)
        QString _name = ""; // name of the network (shown)
        QByteArray _key; // group key
        QByteArray _previous; // key of the epoch before (a member offline during the rotation still proves itself once)
        int _epoch = 0; // version of the key (bumped at each removal)

    public:
        // ---------- Pre-Function -------- //
        void load(void); // device id created when missing, group read when present
        void save(void) const; // group.json (0600), removed when not in a network
        void create(const QString& name); // new network with this device alone
        void adopt(const QString& id, const QString& name, const QByteArray& key, const int epoch); // joined: the network given by a member
        void rekey(const QByteArray& key, const int epoch); // new key of the network (a member was removed)
        void leave(void); // out of the network (key forgotten)
        _cold _nodiscard QByteArray previousProof(const QByteArray& transcript) const; // HMAC(previous key, transcript), "" = no previous key
        _cold _nodiscard QString tag(void) const; // short public tag of the network (LAN discovery): sha256(id) first 8 bytes in hex

        // ------------ Function ---------- //
        /* proof */
        _cold _nodiscard inline QByteArray memberProof(const QByteArray& transcript) const {return hmac_sha256(this->_key, transcript);}; // HMAC(key, transcript)

        /* getter */
        _cold _nodiscard inline bool joined(void) const                {return !this->_id.isEmpty();};
        _cold _nodiscard inline const QString& self(void) const        {return this->_self;};
        _cold _nodiscard inline const QString& id(void) const          {return this->_id;};
        _cold _nodiscard inline const QString& name(void) const        {return this->_name;};
        _cold _nodiscard inline const QByteArray& key(void) const      {return this->_key;};
        _cold _nodiscard inline const QByteArray& previous(void) const {return this->_previous;}; // key of the epoch before ("" = none)
        _cold _nodiscard inline bool hasPrevious(void) const           {return this->_previous.size() == XMOUSE_GROUP_KEY_SIZE;};
        _cold _nodiscard inline int epoch(void) const                  {return this->_epoch;};

        // ------------ Operator ---------- //
        Group& operator=(const Group& other) = delete;
        Group& operator=(Group&& other) = delete;

        // ---------- Constructor --------- //
        explicit Group(const QString& dir): _dir{dir} { /* Nothing */ }; // load() reads it
        Group(const Group& other) = delete;
        Group(Group&& other) = delete;

        // ----------- Destructor --------- //
        ~Group() = default;
};

} // namespace end
#endif /* GROUP_H */

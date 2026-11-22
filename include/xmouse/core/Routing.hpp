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
##  @file Routing.hpp

File Description:
##  Where a message for a device goes: its own link, a phone / pad of this device, or a member that reaches it (relay)
\**************************************************************/

#ifndef ROUTING_H
    #define ROUTING_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp> // _hot, _cold, _nodiscard
    #include "State.hpp"       // xmouse::core::State
    #include <QString>         // QString
    #include <QPoint>          // QPoint
    #include <QHash>           // QHash
    #include <QSet>            // QSet

namespace xmouse::core { // namespace start

//----------------------------------------------------------------//
/* ENUM */

enum class RouteKind {
    None, // unreachable now (offline, or no member reaches it)
    Local, // this device
    Direct, // its own link
    Pad, // a phone / web pad connected to this device
    Relay, // through the member hop (it reaches the target)
};

//----------------------------------------------------------------//
/* STRUCT */

struct Route {
    RouteKind kind = RouteKind::None;
    QString hop = ""; // Direct: the target, Relay: the member that carries it
};

//----------------------------------------------------------------//
/* PROTOTYPE */

/*
 * device/<id> may hold:
 *  "links": [ids]  the members (and the pads) it is linked to now
 *  "via": "<id>"   a phone / pad: the device it is connected to
 *  "relay": false  the member refuses to carry the messages of the others
*/
_hot _nodiscard Route route_to(const State& state, const QSet<QString>& online, const QSet<QString>& pads, const QString& self, const QString& target);

/* profiles */
_cold _nodiscard QHash<QString, QPoint> profile_positions(const State& state, const QString& name); // profile/<name> -> {device: top left}

} // namespace end
#endif /* ROUTING_H */

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
##  @file Routing.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xmouse/core/Routing.hpp"
#include <QJsonObject>
#include <QStringList>
#include <QJsonArray>
#include <algorithm>

//----------------------------------------------------------------//
/* TOOLS */

_hot static bool links_to(const QJsonObject& device, const QString& target)
{
    for (const QJsonValue& v: device.value("links").toArray())
        if (v.toString() == target) return true;
    return false;
}

//----------------------------------------------------------------//
/* ROUTE */

xmouse::core::Route xmouse::core::route_to(const State& state, const QSet<QString>& online, const QSet<QString>& pads, const QString& self, const QString& target)
{
    const QJsonObject device = state.value("device/" + target).toObject();
    const QString via = device.value("via").toString();
    QStringList members(online.begin(), online.end());

    if (target.isEmpty() || state.contains("removed/" + target)) return {};
    if (target == self) return {RouteKind::Local, self};
    if (online.contains(target)) return {RouteKind::Direct, target};
    if (pads.contains(target)) return {RouteKind::Pad, target};

    // A phone / pad of another device: through that device, when it says the pad is there
    if (!via.isEmpty()) {
        if (via == self || !online.contains(via)) return {};
        if (!links_to(state.value("device/" + via).toObject(), target)) return {};
        return {RouteKind::Relay, via};
    }

    // A member linked to the target and willing to carry (the same choice on every call: sorted ids)
    std::sort(members.begin(), members.end());
    for (const QString& m: members) {
        const QJsonObject hop = state.value("device/" + m).toObject();
        if (!hop.value("relay").toBool(true)) continue;
        if (links_to(hop, target)) return {RouteKind::Relay, m};
    }
    return {};
}

//----------------------------------------------------------------//
/* PROFILES */

QHash<QString, QPoint> xmouse::core::profile_positions(const State& state, const QString& name)
{
    const QJsonObject layout = state.value("profile/" + name).toObject().value("layout").toObject();
    QHash<QString, QPoint> out;

    for (auto it = layout.begin(); it != layout.end(); ++it) {
        const QJsonObject p = it.value().toObject();
        if (p.contains("x") && p.contains("y")) out.insert(it.key(), QPoint(p.value("x").toInt(), p.value("y").toInt()));
    }
    return out;
}

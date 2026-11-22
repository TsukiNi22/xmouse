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
#include "xmouse/core/State.hpp"
#include <gtest/gtest.h>
#include <QJsonArray>

//----------------------------------------------------------------//
/* TOOLS */

// A network seen from "self": every device entry with its links
static xmouse::core::State network(void)
{
    xmouse::core::State s;

    s.setSelf("self");
    (void)s.set("device/self", QJsonObject{{"name", "me"}, {"links", QJsonArray{"hub"}}});
    (void)s.set("device/hub", QJsonObject{{"name", "hub"}, {"links", QJsonArray{"self", "far", "phone"}}});
    (void)s.set("device/far", QJsonObject{{"name", "far"}, {"links", QJsonArray{"hub"}}});
    (void)s.set("device/phone", QJsonObject{{"name", "phone"}, {"via", "hub"}});
    (void)s.set("device/mine", QJsonObject{{"name", "pad"}, {"via", "self"}});
    return s;
}

//----------------------------------------------------------------//
/* ROUTES */

TEST(Routing, SelfDirectAndUnknown)
{
    const xmouse::core::State s = network();

    EXPECT_EQ(xmouse::core::route_to(s, {"hub"}, {}, "self", "self").kind, xmouse::core::RouteKind::Local);
    EXPECT_EQ(xmouse::core::route_to(s, {"hub"}, {}, "self", "hub").kind, xmouse::core::RouteKind::Direct);
    EXPECT_EQ(xmouse::core::route_to(s, {"hub"}, {}, "self", "nobody").kind, xmouse::core::RouteKind::None);
    EXPECT_EQ(xmouse::core::route_to(s, {"hub"}, {}, "self", "").kind, xmouse::core::RouteKind::None);
}

TEST(Routing, AMemberNotLinkedIsReachedThroughOneThatIs)
{
    const xmouse::core::State s = network();
    const xmouse::core::Route route = xmouse::core::route_to(s, {"hub"}, {}, "self", "far");

    EXPECT_EQ(route.kind, xmouse::core::RouteKind::Relay);
    EXPECT_EQ(route.hop, "hub");
    EXPECT_EQ(xmouse::core::route_to(s, {}, {}, "self", "far").kind, xmouse::core::RouteKind::None); // nobody online
}

TEST(Routing, AMemberThatRefusesToRelayIsSkipped)
{
    xmouse::core::State s = network();

    (void)s.set("device/hub", QJsonObject{{"name", "hub"}, {"links", QJsonArray{"far"}}, {"relay", false}});
    EXPECT_EQ(xmouse::core::route_to(s, {"hub"}, {}, "self", "far").kind, xmouse::core::RouteKind::None);
}

TEST(Routing, APadGoesThroughItsDevice)
{
    const xmouse::core::State s = network();

    EXPECT_EQ(xmouse::core::route_to(s, {"hub"}, {}, "self", "phone").hop, "hub"); // a pad of another device, there now
    EXPECT_EQ(xmouse::core::route_to(s, {}, {}, "self", "phone").kind, xmouse::core::RouteKind::None); // its device offline
    EXPECT_EQ(xmouse::core::route_to(s, {"hub"}, {"mine"}, "self", "mine").kind, xmouse::core::RouteKind::Pad); // a pad of this device, connected
    EXPECT_EQ(xmouse::core::route_to(s, {"hub"}, {}, "self", "mine").kind, xmouse::core::RouteKind::None); // not connected
}

TEST(Routing, APadOfAnotherDeviceThatIsNotThereIsUnreachable)
{
    xmouse::core::State s = network();

    (void)s.set("device/hub", QJsonObject{{"name", "hub"}, {"links", QJsonArray{"self"}}});
    EXPECT_EQ(xmouse::core::route_to(s, {"hub"}, {}, "self", "phone").kind, xmouse::core::RouteKind::None);
}

TEST(Routing, ARemovedDeviceIsNeverReached)
{
    xmouse::core::State s = network();

    (void)s.set("removed/hub", true);
    EXPECT_EQ(xmouse::core::route_to(s, {"hub"}, {}, "self", "hub").kind, xmouse::core::RouteKind::None);
}

//----------------------------------------------------------------//
/* PROFILES */

TEST(Routing, ProfilePositionsAreRead)
{
    xmouse::core::State s;
    QHash<QString, QPoint> positions;

    s.setSelf("self");
    (void)s.set("profile/desk", QJsonObject{{"layout", QJsonObject{{"a", QJsonObject{{"x", 0}, {"y", 0}}}, {"b", QJsonObject{{"x", 1920}, {"y", -100}}}, {"bad", QJsonObject{{"x", 3}}}}}});
    positions = xmouse::core::profile_positions(s, "desk");
    EXPECT_EQ(positions.size(), 2);
    EXPECT_EQ(positions.value("b"), QPoint(1920, -100));
    EXPECT_TRUE(xmouse::core::profile_positions(s, "none").isEmpty());
}

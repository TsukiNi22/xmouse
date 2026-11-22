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
##  @file Layout.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xmouse/core/Layout.hpp"
#include <gtest/gtest.h>
#include <QJsonObject>

// Two devices side by side: a (1920x1080) then b (1280x1024) on its right
static xmouse::core::Layout side_by_side(void)
{
    xmouse::core::Layout layout;

    layout.setDevice("a", "pc", {xmouse::core::Screen{"A1", QRect(0, 0, 1920, 1080), 1.0}});
    layout.setDevice("b", "laptop", {xmouse::core::Screen{"B1", QRect(0, 0, 1280, 1024), 1.0}});
    layout.setPosition("a", QPoint(0, 0));
    layout.setPosition("b", QPoint(1920, 0));
    return layout;
}

/* Hits */
TEST(Layout, HitFindsTheDeviceAndItsLocalPoint)
{
    const xmouse::core::Layout layout = side_by_side();
    const std::optional<xmouse::core::Hit> hit = layout.hit(QPoint(2000, 10));

    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->device, "b");
    EXPECT_EQ(hit->local, QPoint(80, 10));
}

TEST(Layout, HitBetweenTheScreensIsNothing)
{
    const xmouse::core::Layout layout = side_by_side();

    EXPECT_FALSE(layout.hit(QPoint(2000, 1050)).has_value()); // under b (1024 high)
    EXPECT_FALSE(layout.hit(QPoint(-1, 0)).has_value());
}

TEST(Layout, LocalOriginOfTheDesktopIsKept)
{
    xmouse::core::Layout layout;

    // A desktop whose left screen is at a negative x (as Windows places a screen left of the primary)
    layout.setDevice("w", "win", {xmouse::core::Screen{"L", QRect(-1280, 0, 1280, 1024), 1.0}, xmouse::core::Screen{"P", QRect(0, 0, 1920, 1080), 1.0}});
    layout.setPosition("w", QPoint(100, 100));
    EXPECT_EQ(layout.toGlobal("w", QPoint(-1280, 0)), QPoint(100, 100));
    EXPECT_EQ(layout.hit(QPoint(100 + 1280, 100))->local, QPoint(0, 0));
}

/* Moves */
TEST(Layout, MoveCrossesToTheNeighbour)
{
    const xmouse::core::Layout layout = side_by_side();

    EXPECT_EQ(layout.move(QPoint(1919, 500), QPoint(5, 0)), QPoint(1924, 500));
}

TEST(Layout, MoveStopsOnABorderWithoutNeighbour)
{
    const xmouse::core::Layout layout = side_by_side();

    EXPECT_EQ(layout.move(QPoint(10, 10), QPoint(-50, -50)), QPoint(0, 0));
    EXPECT_EQ(layout.move(QPoint(2000, 1000), QPoint(0, 200)), QPoint(2000, 1023));
}

TEST(Layout, MoveSlidesAlongABorder)
{
    const xmouse::core::Layout layout = side_by_side();

    // Diagonal into the gap under b: the x part still lands on a
    EXPECT_EQ(layout.move(QPoint(1900, 1070), QPoint(30, 5)), QPoint(1900, 1075));
}

/* Edges */
TEST(Layout, ExitsAreTheSharedPartOfTheBorder)
{
    const xmouse::core::Layout layout = side_by_side();
    const QList<xmouse::core::Edge> a = layout.exits("a");
    const QList<xmouse::core::Edge> b = layout.exits("b");

    ASSERT_EQ(a.size(), 1);
    EXPECT_EQ(a[0].side, xmouse::core::Side::Right);
    EXPECT_EQ(a[0].line, 1920);
    EXPECT_EQ(a[0].from, 0);
    EXPECT_EQ(a[0].to, 1023);
    EXPECT_EQ(a[0].target, "b");
    ASSERT_EQ(b.size(), 1);
    EXPECT_EQ(b[0].side, xmouse::core::Side::Left);
    EXPECT_EQ(b[0].line, 0);
    EXPECT_EQ(b[0].to, 1023);
}

// The hub sets the barriers of the capture again only when the edges change (the portal of GNOME ended the capture
// under the cursor at each refresh): the same layout gives equal edges, a moved device other ones
TEST(Layout, ExitsCompareEqualUntilTheLayoutChanges)
{
    xmouse::core::Layout layout = side_by_side();
    const QList<xmouse::core::Edge> before = layout.exits("a");

    EXPECT_EQ(side_by_side().exits("a"), before);
    layout.setPosition("b", QPoint(1920, 300));
    EXPECT_NE(layout.exits("a"), before);
}

TEST(Layout, ExitsAboveAndBelow)
{
    xmouse::core::Layout layout;

    layout.setDevice("top", "tv", {xmouse::core::Screen{"T", QRect(0, 0, 3840, 2160), 1.0}});
    layout.setDevice("low", "pc", {xmouse::core::Screen{"P", QRect(0, 0, 1920, 1080), 1.0}});
    layout.setPosition("top", QPoint(0, 0));
    layout.setPosition("low", QPoint(1000, 2160));
    ASSERT_EQ(layout.exits("top").size(), 1);
    EXPECT_EQ(layout.exits("top")[0].side, xmouse::core::Side::Bottom);
    EXPECT_EQ(layout.exits("top")[0].from, 1000);
    EXPECT_EQ(layout.exits("top")[0].to, 2919);
    ASSERT_EQ(layout.exits("low").size(), 1);
    EXPECT_EQ(layout.exits("low")[0].side, xmouse::core::Side::Top);
    EXPECT_EQ(layout.exits("low")[0].line, 0);
}

TEST(Layout, NoExitWhenTheScreensDoNotTouch)
{
    xmouse::core::Layout layout = side_by_side();

    layout.setPosition("b", QPoint(1930, 0));
    EXPECT_TRUE(layout.exits("a").isEmpty());
}

/* Placement */
TEST(Layout, UnplacedDevicesGoToTheRight)
{
    xmouse::core::Layout layout;

    layout.setDevice("a", "pc", {xmouse::core::Screen{"A1", QRect(0, 0, 1920, 1080), 1.0}});
    layout.setDevice("b", "laptop", {xmouse::core::Screen{"B1", QRect(0, 0, 1280, 800), 1.0}});
    layout.setPosition("a", QPoint(0, 0));
    layout.placeUnplaced();
    EXPECT_EQ(layout.device("b").position, QPoint(1920, 0));
    EXPECT_FALSE(layout.overlaps("b"));
}

TEST(Layout, SnapSticksToTheClosestBorder)
{
    xmouse::core::Layout layout = side_by_side();

    EXPECT_EQ(layout.snap("b", QPoint(1931, 7), 20), QPoint(1920, 0));
    EXPECT_EQ(layout.snap("b", QPoint(2500, 300), 20), QPoint(2500, 300));
}

TEST(Layout, OverlapIsSeen)
{
    xmouse::core::Layout layout = side_by_side();

    layout.setPosition("b", QPoint(1000, 0));
    EXPECT_TRUE(layout.overlaps("b"));
}

/* Json */
TEST(Layout, ScreensJsonRoundTripAndInvalidDropped)
{
    QJsonArray array = xmouse::core::screens_to_json({xmouse::core::Screen{"A", QRect(10, 20, 1920, 1080), 1.5}});

    array.append(QJsonObject{{"name", "tiny"}, {"x", 0}, {"y", 0}, {"w", 2}, {"h", 2}});
    const QList<xmouse::core::Screen> screens = xmouse::core::screens_from_json(array);
    ASSERT_EQ(screens.size(), 1);
    EXPECT_EQ(screens[0].rect, QRect(10, 20, 1920, 1080));
    EXPECT_DOUBLE_EQ(screens[0].scale, 1.5);
}

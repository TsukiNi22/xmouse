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
##  @file State.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xmouse/core/State.hpp"
#include <QTemporaryDir>
#include <gtest/gtest.h>

/* Local writes */
TEST(State, SetReturnsADeltaAndStoresTheValue)
{
    xmouse::core::State state;
    QJsonObject delta;

    state.setSelf("a");
    delta = state.set("net/name", "home");
    EXPECT_TRUE(delta.contains("net/name"));
    EXPECT_EQ(state.value("net/name").toString(), "home");
    EXPECT_EQ(state.clock(), 1u);
}

TEST(State, SameValueSendsNothing)
{
    xmouse::core::State state;

    (void)state.set("net/name", "home");
    EXPECT_TRUE(state.set("net/name", "home").isEmpty());
    EXPECT_EQ(state.clock(), 1u);
}

TEST(State, RemoveLeavesATombstone)
{
    xmouse::core::State state;

    (void)state.set("device/x", QJsonObject{{"name", "pc"}});
    EXPECT_FALSE(state.remove("device/x").isEmpty());
    EXPECT_FALSE(state.contains("device/x"));
    EXPECT_TRUE(state.value("device/x").isNull());
    EXPECT_TRUE(state.toJson().contains("device/x"));
    EXPECT_TRUE(state.remove("device/x").isEmpty());
}

/* Merge */
TEST(State, MergeTakesTheNewerWrite)
{
    xmouse::core::State a;
    xmouse::core::State b;

    a.setSelf("a");
    b.setSelf("b");
    (void)a.set("layout/x", QJsonObject{{"x", 1}});
    (void)b.merge(a.toJson());
    (void)b.set("layout/x", QJsonObject{{"x", 2}});
    EXPECT_EQ(a.merge(b.toJson()), QStringList{"layout/x"});
    EXPECT_EQ(a.value("layout/x").toObject().value("x").toInt(), 2);
}

TEST(State, MergeIgnoresAnOlderWrite)
{
    xmouse::core::State a;
    xmouse::core::State b;

    a.setSelf("a");
    b.setSelf("b");
    (void)b.set("k", 1);
    (void)a.set("k", 10);
    (void)a.set("k", 11);
    EXPECT_TRUE(a.merge(b.toJson()).isEmpty());
    EXPECT_EQ(a.value("k").toInt(), 11);
}

TEST(State, ConcurrentWritesConvergeOnTheAuthor)
{
    xmouse::core::State a;
    xmouse::core::State b;

    a.setSelf("a");
    b.setSelf("b");
    (void)a.set("k", "from a");
    (void)b.set("k", "from b");
    (void)a.merge(b.toJson());
    (void)b.merge(a.toJson());
    EXPECT_EQ(a.value("k"), b.value("k"));
    EXPECT_EQ(a.value("k").toString(), "from b");
}

TEST(State, MergeTwiceChangesNothing)
{
    xmouse::core::State a;
    xmouse::core::State b;

    (void)a.set("k", 1);
    EXPECT_EQ(b.merge(a.toJson()).size(), 1);
    EXPECT_TRUE(b.merge(a.toJson()).isEmpty());
}

TEST(State, MergeRaisesTheClock)
{
    xmouse::core::State a;
    xmouse::core::State b;

    for (int i = 0; i < 5; ++i)
        (void)a.set("k", i);
    (void)b.merge(a.toJson());
    EXPECT_EQ(b.clock(), 5u);
    (void)b.set("k", 99);
    EXPECT_FALSE(a.merge(b.toJson()).isEmpty());
    EXPECT_EQ(a.value("k").toInt(), 99);
}

TEST(State, RemovalPropagates)
{
    xmouse::core::State a;
    xmouse::core::State b;

    (void)a.set("device/x", true);
    (void)b.merge(a.toJson());
    (void)a.remove("device/x");
    EXPECT_EQ(b.merge(a.toJson()), QStringList{"device/x"});
    EXPECT_FALSE(b.contains("device/x"));
}

/* Keys */
TEST(State, KeysAndIdsByPrefix)
{
    xmouse::core::State state;

    (void)state.set("device/b", true);
    (void)state.set("device/a", true);
    (void)state.set("layout/a", true);
    (void)state.remove("device/b");
    EXPECT_EQ(state.keys("device/"), QStringList{"device/a"});
    EXPECT_EQ(state.ids("device/"), QStringList{"a"});
}

/* Files */
TEST(State, SaveAndLoadKeepEverything)
{
    QTemporaryDir dir;
    xmouse::core::State a;
    xmouse::core::State b;

    ASSERT_TRUE(dir.isValid());
    (void)a.set("k", "v");
    (void)a.set("gone", 1);
    (void)a.remove("gone");
    a.save(dir.filePath("state.json"));
    b.load(dir.filePath("state.json"));
    EXPECT_EQ(b.value("k").toString(), "v");
    EXPECT_FALSE(b.contains("gone"));
    EXPECT_EQ(b.clock(), a.clock());
}

TEST(State, LoadOfAMissingFileIsEmpty)
{
    QTemporaryDir dir;
    xmouse::core::State state;

    (void)state.set("k", 1);
    state.load(dir.filePath("missing.json"));
    EXPECT_TRUE(state.keys().isEmpty());
}

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
##  @file Group.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#define _Exception
#include <utils/utils.hpp>
#include "xmouse/security/Crypto.hpp"
#include "xmouse/security/Group.hpp"
#include <QTemporaryDir>
#include <gtest/gtest.h>
#include <QFile>

/* Identity */
TEST(Group, DeviceIdIsCreatedOnceAndKept)
{
    QTemporaryDir dir;
    xmouse::security::Group a(dir.path());
    xmouse::security::Group b(dir.path());

    a.load();
    b.load();
    EXPECT_EQ(a.self().size(), 24);
    EXPECT_EQ(a.self(), b.self());
    EXPECT_FALSE(a.joined());
}

/* Network */
TEST(Group, CreateSaveLoad)
{
    QTemporaryDir dir;
    xmouse::security::Group a(dir.path());
    xmouse::security::Group b(dir.path());

    a.load();
    a.create("desk");
    b.load();
    EXPECT_TRUE(b.joined());
    EXPECT_EQ(b.name(), "desk");
    EXPECT_EQ(b.key(), a.key());
    EXPECT_EQ(b.key().size(), XMOUSE_GROUP_KEY_SIZE);
    EXPECT_EQ(b.epoch(), 1);
    #ifndef _WIN32
        EXPECT_EQ(QFile(dir.filePath("group.json")).permissions() & (QFileDevice::ReadGroup | QFileDevice::ReadOther), QFileDevice::Permissions()); // no unix rights on Windows
    #endif
}

TEST(Group, AdoptRefusesABrokenKey)
{
    QTemporaryDir dir;
    xmouse::security::Group g(dir.path());

    g.load();
    EXPECT_THROW(g.adopt("id", "desk", QByteArray(5, 'x'), 1), utils::exception::IException);
    EXPECT_FALSE(g.joined());
}

TEST(Group, RekeyKeepsThePreviousKeyForOneEpoch)
{
    QTemporaryDir dir;
    xmouse::security::Group g(dir.path());
    const QByteArray transcript = "transcript";
    QByteArray old;

    g.load();
    g.create("desk");
    old = g.memberProof(transcript);
    EXPECT_FALSE(g.hasPrevious());
    g.rekey(xmouse::security::random_bytes(XMOUSE_GROUP_KEY_SIZE), 2);
    EXPECT_EQ(g.epoch(), 2);
    EXPECT_NE(g.memberProof(transcript), old);
    EXPECT_EQ(g.previousProof(transcript), old);
}

TEST(Group, RekeyIgnoresAnOldEpoch)
{
    QTemporaryDir dir;
    xmouse::security::Group g(dir.path());
    QByteArray key;

    g.load();
    g.create("desk");
    key = g.key();
    g.rekey(xmouse::security::random_bytes(XMOUSE_GROUP_KEY_SIZE), 1);
    EXPECT_EQ(g.key(), key);
}

TEST(Group, LeaveForgetsTheKey)
{
    QTemporaryDir dir;
    xmouse::security::Group g(dir.path());

    g.load();
    g.create("desk");
    g.leave();
    EXPECT_FALSE(g.joined());
    EXPECT_TRUE(g.key().isEmpty());
    EXPECT_FALSE(QFile::exists(dir.filePath("group.json")));
    EXPECT_EQ(g.tag(), "");
}

TEST(Group, TagIsPublicAndStable)
{
    QTemporaryDir dir;
    xmouse::security::Group g(dir.path());

    g.load();
    g.create("desk");
    EXPECT_EQ(g.tag().size(), 16);
    EXPECT_FALSE(g.tag().contains(g.id()));
}

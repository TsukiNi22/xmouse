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
##  @file Node.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#define _Exception
#define _Verbose
#include <utils/utils.hpp>
#include "xmouse/Node.hpp"
#include <QTemporaryDir>
#include <gtest/gtest.h>

// Every test in its own XMOUSE_HOME: the folders of the user are never touched
class NodeTest: public ::testing::Test {
    protected:
        QTemporaryDir _home;

        void SetUp(void) override    {qputenv("XMOUSE_HOME", this->_home.path().toUtf8());};
        void TearDown(void) override {qunsetenv("XMOUSE_HOME");};
};

/* Init */
TEST_F(NodeTest, InitWithoutArgument)
{
    xmouse::Node core;
    const char* const argv[] = {"xmouse", nullptr};

    EXPECT_NO_THROW(core.init(1, argv));
}

TEST_F(NodeTest, InitSetsTheVerboseLevel)
{
    xmouse::Node core;
    const char* const argv[] = {"xmouse", "--verbose", "none", nullptr};

    EXPECT_NO_THROW(core.init(3, argv));
    EXPECT_EQ(utils::verbose::verbose.load(), utils::verbose::Verbose::None);
}

TEST_F(NodeTest, InitRefusesAnInvalidVerboseLevel)
{
    xmouse::Node core;
    const char* const argv[] = {"xmouse", "--verbose", "loud", nullptr};

    EXPECT_THROW(core.init(3, argv), utils::exception::IException);
}

TEST_F(NodeTest, JoinNeedsAnAddress)
{
    xmouse::Node core;
    const char* const argv[] = {"xmouse", "join", nullptr};

    EXPECT_THROW(core.init(2, argv), utils::exception::IException);
}

TEST_F(NodeTest, JoinAfterTheFlags)
{
    xmouse::Node core;
    const char* const argv[] = {"xmouse", "--tty", "-p", "47911", "-n", "beta", "join", "127.0.0.1:47910", "K7F-29Q-XM4", nullptr};

    EXPECT_NO_THROW(core.init(9, argv));
}

TEST_F(NodeTest, AFlagValueIsNeverACommand)
{
    xmouse::Node core;
    const char* const argv[] = {"xmouse", "--name", "join", nullptr};

    EXPECT_NO_THROW(core.init(3, argv)); // a device named "join", no join command
}

TEST_F(NodeTest, InvalidPortRefused)
{
    xmouse::Node core;
    const char* const argv[] = {"xmouse", "--port", "70000", nullptr};

    EXPECT_THROW(core.init(3, argv), utils::exception::IException);
}

/* Run */
TEST_F(NodeTest, VersionAndPathsDoNotStartThePanel)
{
    xmouse::Node version;
    xmouse::Node paths;
    const char* const argvVersion[] = {"xmouse", "--version", nullptr};
    const char* const argvPaths[] = {"xmouse", "paths", nullptr};

    version.init(2, argvVersion);
    EXPECT_NO_THROW(version.run());
    paths.init(2, argvPaths);
    EXPECT_NO_THROW(paths.run());
    EXPECT_EQ(paths.exit(), 0);
}

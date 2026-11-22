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
##  @file Tools.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xmouse/core/Tools.hpp"
#include "xmouse/core/Hub.hpp"
#include <QTemporaryDir>
#include <gtest/gtest.h>
#include <QFile>

//----------------------------------------------------------------//
/* FILE NAMES */

TEST(Tools, AReceivedFileNameNeverLeavesItsFolder)
{
    EXPECT_EQ(xmouse::core::safe_file_name("../../etc/passwd"), "passwd");
    EXPECT_EQ(xmouse::core::safe_file_name("C:\\Users\\a\\report.pdf"), "report.pdf");
    EXPECT_EQ(xmouse::core::safe_file_name(".bashrc"), "bashrc");
    EXPECT_EQ(xmouse::core::safe_file_name(""), "file");
    EXPECT_EQ(xmouse::core::safe_file_name("a:b?c.txt"), "a_b_c.txt");
    EXPECT_EQ(xmouse::core::safe_file_name(QString(300, 'x')).size(), 200);
}

TEST(Tools, AnExistingFileIsNeverOverwritten)
{
    QTemporaryDir dir;
    QFile first(dir.filePath("photo.jpg"));

    ASSERT_TRUE(first.open(QIODevice::WriteOnly));
    first.close();
    EXPECT_EQ(xmouse::core::unique_path(dir.path(), "photo.jpg"), dir.filePath("photo (2).jpg"));
    EXPECT_EQ(xmouse::core::unique_path(dir.path(), "new.txt"), dir.filePath("new.txt"));
}

//----------------------------------------------------------------//
/* PREDICTION */

TEST(Tools, AForeignCursorIsDrawnAheadOnlyWhileItsPacketIsLate)
{
    xmouse::core::Foreign f;

    f.local = QPoint(100, 100);
    f.seen = 1000;
    f.velocity = QPointF(0.5, -0.25); // px / ms
    EXPECT_EQ(f.drawn(1005), QPoint(100, 100)); // on time
    EXPECT_EQ(f.drawn(1040), QPoint(120, 90)); // 40 ms late: along its speed
    EXPECT_EQ(f.drawn(1200), QPoint(100, 100)); // too old: where it was
    f.buttons = 1;
    EXPECT_EQ(f.drawn(1040), QPoint(100, 100)); // a drag: never ahead of the real pointer
}

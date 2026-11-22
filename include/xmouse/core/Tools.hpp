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
##  @file Tools.hpp

File Description:
##  Small tools shared by the parts of the hub (clock, graphical session, file names)
\**************************************************************/

#ifndef TOOLS_H
    #define TOOLS_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>  // _hot, _cold, _nodiscard
    #include <QCoreApplication> // QCoreApplication
    #include <QGuiApplication>  // QGuiApplication
    #include <QDateTime>        // QDateTime
    #include <QFileInfo>        // QFileInfo
    #include <QString>          // QString
    #include <QDir>             // QDir

namespace xmouse::core { // namespace start

//----------------------------------------------------------------//
/* PROTOTYPE */

_hot _nodiscard inline qint64 now_ms(void) {return QDateTime::currentMSecsSinceEpoch();};

// A graphical application (screens, clipboard, overlays): not the terminal panel of a machine without display
_cold _nodiscard inline bool is_gui(void) {return qobject_cast<QGuiApplication*>(QCoreApplication::instance()) != nullptr;};

// A file name received from a member: no folder, no hidden / special name, 200 characters at most
_cold _nodiscard inline QString safe_file_name(const QString& name)
{
    QString clean = QString(name).replace('\\', '/');
    QString out;

    // The forbidden characters first: "a:" would be read as a drive letter on Windows
    clean.remove(QChar('\0'));
    for (const QChar c: QString("<>:\"|?*"))
        clean.replace(c, '_');
    out = QFileInfo(clean).fileName().trimmed();
    while (out.startsWith('.'))
        out.remove(0, 1);
    if (out.isEmpty()) out = "file";
    return out.left(200);
}

// dir/name, or dir/name (2).ext ... when it exists already
_cold _nodiscard inline QString unique_path(const QString& dir, const QString& name)
{
    const QFileInfo info(name);
    const QString base = info.completeBaseName();
    const QString suffix = info.suffix().isEmpty() ? QString() : "." + info.suffix();
    QString path = QDir(dir).filePath(name);

    for (int i = 2; QFileInfo::exists(path) && i < 10000; ++i)
        path = QDir(dir).filePath(base + " (" + QString::number(i) + ")" + suffix);
    return path;
}

} // namespace end
#endif /* TOOLS_H */

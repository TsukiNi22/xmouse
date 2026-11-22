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
##  @file Layout.hpp

File Description:
##  Screens of every member placed in one global space: moves, crossings and shared edges
\**************************************************************/

#ifndef LAYOUT_H
    #define LAYOUT_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>  // _hot, _cold, _nodiscard
    #include <QJsonArray>       // QJsonArray
    #include <optional>         // std::optional
    #include <QString>          // QString
    #include <QPoint>           // QPoint
    #include <QRect>            // QRect
    #include <QHash>            // QHash
    #include <QList>            // QList

namespace xmouse::core { // namespace start

//----------------------------------------------------------------//
/* ENUM */

enum class Side {
    Left,
    Right,
    Top,
    Bottom,
};

//----------------------------------------------------------------//
/* STRUCT */

struct Screen {
    QString name = ""; // "DP-1", "eDP-1", "\\.\DISPLAY1"...
    QRect rect; // logical pixels in the desktop of its device (as its OS places it)
    double scale = 1.0; // device pixel ratio (informative)
};

struct Device {
    QString id = "";
    QString name = "";
    QList<Screen> screens; // empty = a device without screen (a controller only)
    QPoint position; // top left of the desktop of the device in the global space
    bool placed = false; // false = not placed yet (put next to the others)
};

// Where a global point lands
struct Hit {
    QString device = "";
    int screen = -1; // index in Device::screens
    QPoint local; // logical pixels of the desktop of the device
};

// Part of the border of a local screen that opens on the screen of another device (pointer barrier)
struct Edge {
    Side side = Side::Right;
    int line = 0; // local coordinate of the barrier: x of a left / right edge (first pixel outside), y of a top / bottom one
    int from = 0; // first pixel covered along the edge (y for left / right, x for top / bottom)
    int to = 0; // last pixel covered (inclusive)
    QString target = ""; // device on the other side
    int screen = -1; // local screen of the edge
};

//----------------------------------------------------------------//
/* PROTOTYPE */

/* json */
_cold _nodiscard QJsonArray screens_to_json(const QList<Screen>& screens);
_cold _nodiscard QList<Screen> screens_from_json(const QJsonArray& array); // invalid entries dropped (at most 16 screens)
_cold _nodiscard QString side_name(const Side side); // "left" | "right" | "top" | "bottom"

//----------------------------------------------------------------//
/* CLASS */

class Layout {
    private:
        QHash<QString, Device> _devices;

        // ---------- Pre-Function -------- //
        _hot _nodiscard std::optional<Hit> hitOn_(const QString& id, const QPoint& global) const;

    public:
        // ---------- Pre-Function -------- //
        _cold void setDevice(const QString& id, const QString& name, const QList<Screen>& screens); // keeps the position
        _cold void setPosition(const QString& id, const QPoint& position); // global top left of its desktop
        _cold void placeUnplaced(void); // devices without a position go to the right of the others
        _hot _nodiscard std::optional<Hit> hit(const QPoint& global) const; // nullopt = between the screens
        _hot _nodiscard QPoint toGlobal(const QString& id, const QPoint& local) const;
        _hot _nodiscard QPoint toLocal(const QString& id, const QPoint& global) const; // inverse of toGlobal
        _hot _nodiscard QPoint move(const QPoint& from, const QPoint& delta) const; // slides along the borders, never leaves the screens
        _hot _nodiscard QPoint clampTo(const QString& id, const QPoint& global) const; // closest point on a screen of the device
        _cold _nodiscard QRect bounds(const QString& id) const; // local bounding box of its screens
        _cold _nodiscard QRect globalRect(const QString& id, const int screen) const;
        _cold _nodiscard QRect globalBounds(const QString& id) const;
        _cold _nodiscard QRect totalBounds(void) const; // every screen
        _cold _nodiscard QList<Edge> exits(const QString& id) const; // edges of its screens opening on another device
        _cold _nodiscard QPoint snap(const QString& id, const QPoint& position, const int tolerance) const; // proposed position stuck to the closest border of the others
        _cold _nodiscard bool overlaps(const QString& id) const; // a screen of the device covers a screen of another one

        // ------------ Function ---------- //
        /* setter */
        _cold inline void removeDevice(const QString& id) {this->_devices.remove(id);};

        /* getter */
        _cold _nodiscard inline const QHash<QString, Device>& devices(void) const {return this->_devices;};
        _cold _nodiscard inline bool contains(const QString& id) const            {return this->_devices.contains(id);};
        _cold _nodiscard inline const Device device(const QString& id) const      {return this->_devices.value(id);};
};

} // namespace end
#endif /* LAYOUT_H */

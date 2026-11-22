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
#include <QJsonObject>
#include <algorithm>
#include <climits>
#include <cstdlib>

//----------------------------------------------------------------//
/* TOOLS */

// One past the last pixel (QRect::right() is inclusive)
_hot static inline int end_x(const QRect& r) {return r.x() + r.width();}
_hot static inline int end_y(const QRect& r) {return r.y() + r.height();}

//----------------------------------------------------------------//
/* JSON */

QJsonArray xmouse::core::screens_to_json(const QList<Screen>& screens)
{
    QJsonArray array;

    for (const Screen& s: screens)
        array.append(QJsonObject{{"name", s.name}, {"x", s.rect.x()}, {"y", s.rect.y()}, {"w", s.rect.width()}, {"h", s.rect.height()}, {"scale", s.scale}});
    return array;
}

QList<xmouse::core::Screen> xmouse::core::screens_from_json(const QJsonArray& array)
{
    QList<Screen> screens;

    for (const QJsonValue& v: array) {
        const QJsonObject o = v.toObject();
        Screen s;

        s.name = o.value("name").toString().left(64);
        s.rect = QRect(o.value("x").toInt(), o.value("y").toInt(), o.value("w").toInt(), o.value("h").toInt());
        s.scale = std::clamp(o.value("scale").toDouble(1.0), 0.25, 8.0);
        if (s.rect.width() < 16 || s.rect.height() < 16 || s.rect.width() > 32768 || s.rect.height() > 32768) continue;
        if (std::abs(s.rect.x()) > 1000000 || std::abs(s.rect.y()) > 1000000) continue;
        screens.append(s);
        if (screens.size() == 16) break;
    }
    return screens;
}

QString xmouse::core::side_name(const Side side)
{
    switch (side) {
        case Side::Left:   return "left";
        case Side::Right:  return "right";
        case Side::Top:    return "top";
        default:           return "bottom";
    }
}

//----------------------------------------------------------------//
/* CLASS */

void xmouse::core::Layout::setDevice(const QString& id, const QString& name, const QList<Screen>& screens)
{
    Device& device = this->_devices[id];

    device.id = id;
    device.name = name;
    device.screens = screens;
}

void xmouse::core::Layout::setPosition(const QString& id, const QPoint& position)
{
    Device& device = this->_devices[id];

    device.id = id;
    device.position = position;
    device.placed = true;
}

void xmouse::core::Layout::placeUnplaced(void)
{
    QStringList ids = this->_devices.keys();

    // Stable order: the same placement on every member
    ids.sort();
    for (const QString& id: ids) {
        Device& device = this->_devices[id];
        int right = INT_MIN;
        int top = 0;

        if (device.placed || device.screens.isEmpty()) continue;
        for (const QString& other: ids) {
            const Device& o = this->_devices[other];
            if (other == id || !o.placed || o.screens.isEmpty()) continue;
            const QRect r = this->globalBounds(other);
            if (end_x(r) > right) {right = end_x(r); top = r.y();}
        }
        device.position = right == INT_MIN ? QPoint(0, 0) : QPoint(right, top);
        device.placed = true;
    }
}

QRect xmouse::core::Layout::bounds(const QString& id) const
{
    auto it = this->_devices.constFind(id);
    QRect box;

    if (it == this->_devices.constEnd()) return {};
    for (const Screen& s: it->screens)
        box = box.united(s.rect);
    return box;
}

QRect xmouse::core::Layout::globalRect(const QString& id, const int screen) const
{
    auto it = this->_devices.constFind(id);

    if (it == this->_devices.constEnd() || screen < 0 || screen >= it->screens.size()) return {};
    return it->screens[screen].rect.translated(it->position - this->bounds(id).topLeft());
}

QRect xmouse::core::Layout::globalBounds(const QString& id) const
{
    const QRect local = this->bounds(id);
    auto it = this->_devices.constFind(id);

    if (it == this->_devices.constEnd()) return {};
    return QRect(it->position, local.size());
}

QRect xmouse::core::Layout::totalBounds(void) const
{
    QRect box;

    for (auto it = this->_devices.constBegin(); it != this->_devices.constEnd(); ++it)
        if (it->placed) box = box.united(this->globalBounds(it.key()));
    return box;
}

QPoint xmouse::core::Layout::toGlobal(const QString& id, const QPoint& local) const
{
    auto it = this->_devices.constFind(id);

    if (it == this->_devices.constEnd()) return local;
    return local - this->bounds(id).topLeft() + it->position;
}

std::optional<xmouse::core::Hit> xmouse::core::Layout::hitOn_(const QString& id, const QPoint& global) const
{
    const Device& device = this->_devices[id];

    if (!device.placed) return std::nullopt;
    for (int i = 0; i < device.screens.size(); ++i) {
        const QRect g = this->globalRect(id, i);
        if (!g.contains(global)) continue;
        return Hit{id, i, device.screens[i].rect.topLeft() + (global - g.topLeft())};
    }
    return std::nullopt;
}

std::optional<xmouse::core::Hit> xmouse::core::Layout::hit(const QPoint& global) const
{
    QStringList ids = this->_devices.keys();

    // Overlapping screens (a layout being edited): the smallest id wins, the same answer everywhere
    ids.sort();
    for (const QString& id: ids) {
        const std::optional<Hit> h = this->hitOn_(id, global);
        if (h) return h;
    }
    return std::nullopt;
}

QPoint xmouse::core::Layout::clampTo(const QString& id, const QPoint& global) const
{
    auto it = this->_devices.constFind(id);
    QPoint best = global;
    long long bestDistance = LLONG_MAX;

    if (it == this->_devices.constEnd()) return global;
    for (int i = 0; i < it->screens.size(); ++i) {
        const QRect g = this->globalRect(id, i);
        const QPoint p(std::clamp(global.x(), g.left(), g.right()), std::clamp(global.y(), g.top(), g.bottom()));
        const long long dx = p.x() - global.x();
        const long long dy = p.y() - global.y();
        if (dx * dx + dy * dy < bestDistance) {bestDistance = dx * dx + dy * dy; best = p;}
    }
    return best;
}

QPoint xmouse::core::Layout::move(const QPoint& from, const QPoint& delta) const
{
    const QPoint target = from + delta;
    const std::optional<Hit> start = this->hit(from);
    QRect current;

    if (this->hit(target)) return target;

    // Slide along the border: keep the axis that still lands on a screen
    if (delta.x() != 0 && delta.y() != 0) {
        if (this->hit(QPoint(target.x(), from.y()))) return QPoint(target.x(), from.y());
        if (this->hit(QPoint(from.x(), target.y()))) return QPoint(from.x(), target.y());
    }

    // Between the screens: stopped on the border of the screen it was on
    if (!start) return this->_devices.isEmpty() ? from : this->clampTo(this->_devices.constBegin().key(), from);
    current = this->globalRect(start->device, start->screen);
    return QPoint(std::clamp(target.x(), current.left(), current.right()), std::clamp(target.y(), current.top(), current.bottom()));
}

QList<xmouse::core::Edge> xmouse::core::Layout::exits(const QString& id) const
{
    QList<Edge> edges;
    QHash<QString, Device>::const_iterator self = this->_devices.constFind(id);

    if (self == this->_devices.constEnd() || !self->placed) return edges;
    for (int i = 0; i < self->screens.size(); ++i) {
        const QRect g = this->globalRect(id, i);
        const QRect local = self->screens[i].rect;

        for (auto it = this->_devices.constBegin(); it != this->_devices.constEnd(); ++it) {
            if (it.key() == id || !it->placed) continue;
            for (int j = 0; j < it->screens.size(); ++j) {
                const QRect f = this->globalRect(it.key(), j);
                Edge edge;
                edge.target = it.key();
                edge.screen = i;

                // Left / right: the other screen starts where this one ends, the overlap along y is the opening
                if (f.x() == end_x(g) || end_x(f) == g.x()) {
                    const int a = std::max(g.y(), f.y());
                    const int b = std::min(end_y(g), end_y(f)) - 1;
                    if (a > b) continue;
                    edge.side = f.x() == end_x(g) ? Side::Right : Side::Left;
                    edge.line = edge.side == Side::Right ? end_x(local) : local.x();
                    edge.from = a - g.y() + local.y();
                    edge.to = b - g.y() + local.y();
                    edges.append(edge);
                } else if (f.y() == end_y(g) || end_y(f) == g.y()) {
                    const int a = std::max(g.x(), f.x());
                    const int b = std::min(end_x(g), end_x(f)) - 1;
                    if (a > b) continue;
                    edge.side = f.y() == end_y(g) ? Side::Bottom : Side::Top;
                    edge.line = edge.side == Side::Bottom ? end_y(local) : local.y();
                    edge.from = a - g.x() + local.x();
                    edge.to = b - g.x() + local.x();
                    edges.append(edge);
                }
            }
        }
    }
    return edges;
}

QPoint xmouse::core::Layout::snap(const QString& id, const QPoint& position, const int tolerance) const
{
    const QRect box(position, this->bounds(id).size());
    int bestX = tolerance + 1;
    int bestY = tolerance + 1;
    QPoint out = position;

    for (auto it = this->_devices.constBegin(); it != this->_devices.constEnd(); ++it) {
        if (it.key() == id || !it->placed) continue;
        for (int j = 0; j < it->screens.size(); ++j) {
            const QRect f = this->globalRect(it.key(), j);

            // Each edge of the moved box against each edge of the other screen, on both axes
            for (const int candidate: {f.x() - box.width(), end_x(f), f.x(), end_x(f) - box.width()}) {
                const int d = std::abs(candidate - box.x());
                if (d < bestX) {bestX = d; out.setX(candidate);}
            }
            for (const int candidate: {f.y() - box.height(), end_y(f), f.y(), end_y(f) - box.height()}) {
                const int d = std::abs(candidate - box.y());
                if (d < bestY) {bestY = d; out.setY(candidate);}
            }
        }
    }
    return out;
}

bool xmouse::core::Layout::overlaps(const QString& id) const
{
    QHash<QString, Device>::const_iterator self = this->_devices.constFind(id);

    if (self == this->_devices.constEnd() || !self->placed) return false;
    for (int i = 0; i < self->screens.size(); ++i) {
        const QRect g = this->globalRect(id, i);
        for (auto it = this->_devices.constBegin(); it != this->_devices.constEnd(); ++it) {
            if (it.key() == id || !it->placed) continue;
            for (int j = 0; j < it->screens.size(); ++j)
                if (g.intersects(this->globalRect(it.key(), j))) return true;
        }
    }
    return false;
}

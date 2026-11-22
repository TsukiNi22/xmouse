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
##  @file LayoutView.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xmouse/ui/gui/LayoutView.hpp"
#include "xmouse/ui/gui/Theme.hpp"
#include "xmouse/net/Protocol.hpp"
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <cmath>

//----------------------------------------------------------------//
/* DEFINE */

#define MARGIN 24 // widget pixels around the layout
#define SNAP_PIXELS 14 // widget pixels within which a dragged device sticks to a border

//----------------------------------------------------------------//
/* CLASS */

xmouse::ui::gui::LayoutView::LayoutView(core::Hub& hub, QWidget* parent)
: QWidget(parent), _hub{hub}
{
    this->setMouseTracking(true);
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QObject::connect(&hub, &core::Hub::layoutChanged, this, qOverload<>(&QWidget::update));
    QObject::connect(&hub, &core::Hub::presenceChanged, this, qOverload<>(&QWidget::update));
    QObject::connect(&hub, &core::Hub::foreignChanged, this, qOverload<>(&QWidget::update));
    QObject::connect(&hub, &core::Hub::membersChanged, this, qOverload<>(&QWidget::update));
}

void xmouse::ui::gui::LayoutView::fit_(void)
{
    QRect box = this->_hub.layout().totalBounds();
    const double w = std::max(1, this->width() - 2 * MARGIN);
    const double h = std::max(1, this->height() - 2 * MARGIN);

    if (!this->_dragged.isEmpty() && this->_view.isValid()) return; // stable while dragging
    if (box.isEmpty()) box = QRect(0, 0, 1920, 1080);
    box.adjust(-box.width() / 6, -box.height() / 6, box.width() / 6, box.height() / 6); // room to move a device around
    this->_view = box;
    this->_scale = std::min(w / box.width(), h / box.height());
    this->_offset = QPointF(MARGIN + (w - box.width() * this->_scale) / 2 - box.x() * this->_scale, MARGIN + (h - box.height() * this->_scale) / 2 - box.y() * this->_scale);
}

QString xmouse::ui::gui::LayoutView::deviceAt_(const QPoint& widget) const
{
    const QPoint global(static_cast<int>(std::floor((widget.x() - this->_offset.x()) / this->_scale)), static_cast<int>(std::floor((widget.y() - this->_offset.y()) / this->_scale)));
    const std::optional<core::Hit> hit = this->_hub.layout().hit(global);

    return hit ? hit->device : QString();
}

void xmouse::ui::gui::LayoutView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    const Palette& palette = Theme::palette();
    const core::Layout& layout = this->_hub.layout();
    QHash<QString, core::Member> members;
    QFont font = this->font();
    QFont small = this->font();

    this->fit_();
    for (const core::Member& m: this->_hub.members())
        members.insert(m.id, m);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(this->rect(), palette.surfaceAlt);
    small.setPointSizeF(font.pointSizeF() * 0.85);

    // Screens: each device in its color, offline ones dimmed, the dragged one where it would land
    for (auto it = layout.devices().constBegin(); it != layout.devices().constEnd(); ++it) {
        const core::Member m = members.value(it.key());
        const bool dragged = it.key() == this->_dragged;
        const QPoint shift = dragged ? this->_dragPosition - it->position : QPoint();
        QColor fill = m.color.isValid() ? m.color : palette.accent;
        bool first = true;

        if (!m.online) fill = palette.border;
        for (int i = 0; i < it->screens.size(); ++i) {
            const QRect g = layout.globalRect(it.key(), i).translated(shift);
            const QRectF r(this->toWidget_(g.topLeft()), QSizeF(g.width() * this->_scale, g.height() * this->_scale));
            QColor inside = fill;

            inside.setAlpha(dragged ? 110 : 70);
            p.setPen(QPen(fill, m.self ? 2.4 : 1.4));
            p.setBrush(inside);
            p.drawRoundedRect(r.adjusted(1, 1, -1, -1), 4, 4);
            p.setPen(palette.text);
            font.setBold(true);
            p.setFont(font);
            if (first) p.drawText(r.adjusted(8, 6, -8, -6), Qt::AlignTop | Qt::AlignLeft, m.name + (m.self ? "  (this device)" : "") + (m.online ? "" : "  - offline"));
            p.setFont(small);
            p.setPen(palette.muted);
            p.drawText(r.adjusted(8, 6, -8, -6), Qt::AlignBottom | Qt::AlignLeft, it->screens[i].name + "  " + QString::number(it->screens[i].rect.width()) + " x " + QString::number(it->screens[i].rect.height()));
            first = false;
        }
    }

    // Cursors of every member (the live map): a dot in its color, its name, where it is
    for (const core::Presence& pr: this->_hub.presence()) {
        const core::Member m = members.value(pr.id);
        const QPointF at = this->toWidget_(pr.global);
        const bool hidden = pr.flags & XMOUSE_PRESENCE_HIDDEN;
        QColor color = m.color.isValid() ? m.color : palette.accent;

        if (hidden && !m.self) continue; // on its own device: its OS draws it, the map shows it only for this device
        if (!this->rect().contains(at.toPoint())) continue;
        p.setPen(QPen(Qt::white, 1.5));
        p.setBrush(color);
        p.drawEllipse(at, (pr.flags & XMOUSE_PRESENCE_DOWN) ? 7.0 : 5.0, (pr.flags & XMOUSE_PRESENCE_DOWN) ? 7.0 : 5.0);
        p.setFont(small);
        p.setPen(color.darker(palette.dark ? 60 : 140));
        p.drawText(at + QPointF(9, 4), m.name.isEmpty() ? pr.id.left(6) : m.name);
    }

    if (layout.devices().isEmpty()) {
        p.setPen(palette.muted);
        p.drawText(this->rect(), Qt::AlignCenter, "Create or join a network: the screens of every device show here");
    }
}

void xmouse::ui::gui::LayoutView::mousePressEvent(QMouseEvent* event)
{
    const QString id = this->deviceAt_(event->position().toPoint());

    if (!this->_editable || event->button() != Qt::LeftButton || id.isEmpty()) return;
    this->_hub.panelUsedLocally();
    this->_dragged = id;
    this->_dragStart = event->position().toPoint();
    this->_dragOrigin = this->_hub.layout().device(id).position;
    this->_dragPosition = this->_dragOrigin;
    this->setCursor(Qt::ClosedHandCursor);
}

void xmouse::ui::gui::LayoutView::mouseMoveEvent(QMouseEvent* event)
{
    if (this->_dragged.isEmpty()) {
        this->setCursor(this->_editable && !this->deviceAt_(event->position().toPoint()).isEmpty() ? Qt::OpenHandCursor : Qt::ArrowCursor);
        return;
    }
    const QPoint delta = event->position().toPoint() - this->_dragStart;
    const QPoint proposed = this->_dragOrigin + QPoint(static_cast<int>(delta.x() / this->_scale), static_cast<int>(delta.y() / this->_scale));
    this->_dragPosition = this->_hub.layout().snap(this->_dragged, proposed, static_cast<int>(SNAP_PIXELS / this->_scale));
    this->update();
}

void xmouse::ui::gui::LayoutView::mouseReleaseEvent(QMouseEvent* event)
{
    const QString id = this->_dragged;

    if (id.isEmpty() || event->button() != Qt::LeftButton) return;
    core::Layout check = this->_hub.layout();

    this->_dragged.clear();
    this->setCursor(Qt::OpenHandCursor);
    check.setPosition(id, this->_dragPosition);
    if (check.overlaps(id)) {this->update(); return;} // dropped over another screen: back where it was
    if (this->_dragPosition != this->_dragOrigin) this->_hub.placeDevice(id, this->_dragPosition);
    this->update();
}

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
#include <QContextMenuEvent>
#include <QFontMetrics>
#include <QInputDialog>
#include <QResizeEvent>
#include <QToolButton>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QFileDialog>
#include <QMimeData>
#include <QDateTime>
#include <QPainter>
#include <QMenu>
#include <cmath>
#include <QUrl>

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
    QObject::connect(&hub, &core::Hub::followChanged, this, qOverload<>(&QWidget::update));
    this->setAcceptDrops(true);
    {
        QToolButton* recenter = new QToolButton(this);
        recenter->setText("Recenter");
        recenter->setToolTip("The whole layout in view again (drag the background to move the view, the wheel zooms)");
        recenter->setCursor(Qt::PointingHandCursor);
        recenter->adjustSize();
        QObject::connect(recenter, &QToolButton::clicked, this, [this]() {this->_fitted = false; this->update();});
        this->_recenter = recenter;
    }
    this->_anim.setInterval(30);
    QObject::connect(&this->_anim, &QTimer::timeout, this, [this]() {
        bool waving = false;
        for (const core::Presence& pr: this->_hub.presence())
            if (pr.flags & XMOUSE_PRESENCE_PING) waving = true;
        if (!waving) this->_anim.stop();
        this->update();
    });
    QObject::connect(&hub, &core::Hub::presenceChanged, this, [this]() {
        for (const core::Presence& pr: this->_hub.presence())
            if ((pr.flags & XMOUSE_PRESENCE_PING) && !this->_anim.isActive()) this->_anim.start();
    });
}

void xmouse::ui::gui::LayoutView::fit_(void)
{
    QRect box = this->_hub.layout().totalBounds();
    const double w = std::max(1, this->width() - 2 * MARGIN);
    const double h = std::max(1, this->height() - 2 * MARGIN);

    if (this->_fitted) return; // the view stays where the person put it (never recentered by itself)
    if (box.isEmpty()) box = QRect(0, 0, 1920, 1080);
    else this->_fitted = true; // fitted on a real layout: kept from now on
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
            if (it.key() == this->_dropTarget) { // files dragged over it: they go there
                p.setPen(QPen(palette.accent, 3, Qt::DashLine));
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(r.adjusted(3, 3, -3, -3), 4, 4);
            }
            p.setPen(palette.text);
            font.setBold(true);
            p.setFont(font);
            if (first) p.drawText(r.adjusted(8, 6, -8, -6), Qt::AlignTop | Qt::AlignLeft, m.name + (m.self ? "  (this device)" : "") + (m.kind == "phone" ? "  - phone" : "") + (m.online ? (m.relayed ? "  - relayed" : "") : "  - offline"));
            p.setFont(small);
            p.setPen(palette.muted);
            p.drawText(r.adjusted(8, 6, -8, -6), Qt::AlignBottom | Qt::AlignLeft, it->screens[i].name + "  " + QString::number(it->screens[i].rect.width()) + " x " + QString::number(it->screens[i].rect.height()));
            first = false;
        }
    }

    // Cursors of every member (the live map): a dot in its color, its name, where it is
    for (const core::Presence& pr: this->_hub.presence()) {
        const core::Member m = members.value(pr.id);
        const QPointF at = this->toWidget_(this->_hub.presenceGlobal(pr)); // on its device as placed now
        const bool hidden = pr.flags & XMOUSE_PRESENCE_HIDDEN;
        QColor color = m.color.isValid() ? m.color : palette.accent;

        if (hidden && !m.self) continue; // on its own device: its OS draws it, the map shows it only for this device
        if (pr.flags & XMOUSE_PRESENCE_UNKNOWN) continue; // its platform does not tell where it is: never a stale dot
        if (!this->rect().contains(at.toPoint())) continue;
        if (pr.flags & XMOUSE_PRESENCE_PING) { // "look here": waves around it
            const double t = static_cast<double>(QDateTime::currentMSecsSinceEpoch() % 900) / 900.0;
            QColor ring = color;
            ring.setAlphaF(1.0 - t);
            p.setPen(QPen(ring, 2));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(at, 6 + 22 * t, 6 + 22 * t);
        }
        if (pr.id == this->_hub.following()) { // followed: a ring
            p.setPen(QPen(color, 2, Qt::DotLine));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(at, 12, 12);
        }
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

    // The background (or the middle button anywhere): the view moves
    if (event->button() == Qt::MiddleButton || (event->button() == Qt::LeftButton && id.isEmpty())) {
        this->_panning = true;
        this->_panStart = event->position().toPoint();
        this->_panOrigin = this->_offset;
        this->setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (!this->_editable || event->button() != Qt::LeftButton || id.isEmpty()) return;
    if (!this->_hub.foreignInputNow()) this->_hub.panelUsedLocally(); // the click of a cursor given the panel keeps it
    this->_dragged = id;
    this->_dragStart = event->position().toPoint();
    this->_dragOrigin = this->_hub.layout().device(id).position;
    this->_dragPosition = this->_dragOrigin;
    this->setCursor(Qt::ClosedHandCursor);
}

void xmouse::ui::gui::LayoutView::mouseMoveEvent(QMouseEvent* event)
{
    if (this->_panning) {
        this->_fitted = true;
        this->_offset = this->_panOrigin + QPointF(event->position().toPoint() - this->_panStart);
        this->update();
        return;
    }
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

    if (this->_panning) {this->_panning = false; this->setCursor(Qt::ArrowCursor); return;}
    if (id.isEmpty() || event->button() != Qt::LeftButton) return;
    core::Layout check = this->_hub.layout();

    this->_dragged.clear();
    this->setCursor(Qt::OpenHandCursor);
    check.setPosition(id, this->_dragPosition);
    if (check.overlaps(id)) {this->update(); return;} // dropped over another screen: back where it was
    if (this->_dragPosition != this->_dragOrigin) this->_hub.placeDevice(id, this->_dragPosition);
    this->update();
}

//----------------------------------------------------------------//
/* MENU AND DROPS */

void xmouse::ui::gui::LayoutView::contextMenuEvent(QContextMenuEvent* event)
{
    const QString id = this->deviceAt_(event->pos());
    const QString name = this->_hub.layout().device(id).name;
    QMenu menu(this);

    if (id.isEmpty() || id == this->_hub.group().self() || !this->_hub.reachable(id)) return;
    menu.addAction(this->_hub.following() == id ? "Stop following " + name : "Follow the cursor of " + name, this, [this, id]() {
        this->_hub.follow(this->_hub.following() == id ? QString() : id);
    });
    menu.addSeparator();
    menu.addAction("Send files to " + name + "...", this, [this, id]() {
        const QStringList files = QFileDialog::getOpenFileNames(this, "Send files to " + this->_hub.layout().device(id).name);
        if (!files.isEmpty()) (void)this->_hub.sendFiles(id, files);
    });
    menu.addAction("Send a link to " + name + "...", this, [this, id]() {
        bool ok = false;
        const QString url = QInputDialog::getText(this, "Send a link", "Link opened on " + this->_hub.layout().device(id).name + " (https://...)", QLineEdit::Normal, "https://", &ok);
        if (ok && !url.trimmed().isEmpty()) this->_hub.sendLink(id, url);
    });
    if (this->_hub.windowMoveEnabled() && !this->_hub.windows().isEmpty()) {
        menu.addAction("Show a window of this device on " + name + "...", this, [this, id]() {
            const QList<platform::WindowInfo> windows = this->_hub.windows();
            QStringList titles;
            bool ok = false;
            for (const platform::WindowInfo& w: windows)
                titles.append(w.title.left(80) + (w.app.isEmpty() ? QString() : "  (" + w.app + ")"));
            const QString chosen = QInputDialog::getItem(this, "Show a window", "Window streamed to " + this->_hub.layout().device(id).name + " (driven from there)", titles, 0, false, &ok);
            if (ok && titles.contains(chosen)) (void)this->_hub.streamWindow(id, windows[titles.indexOf(chosen)].id);
        });
    }
    menu.exec(event->globalPos());
}

void xmouse::ui::gui::LayoutView::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

void xmouse::ui::gui::LayoutView::dragMoveEvent(QDragMoveEvent* event)
{
    const QString id = this->deviceAt_(event->position().toPoint());
    const bool ok = !id.isEmpty() && id != this->_hub.group().self() && this->_hub.reachable(id);

    if (ok != (this->_dropTarget == id) || (!ok && !this->_dropTarget.isEmpty())) {this->_dropTarget = ok ? id : QString(); this->update();}
    if (ok) event->acceptProposedAction();
    else event->ignore();
}

void xmouse::ui::gui::LayoutView::dragLeaveEvent(QDragLeaveEvent*)
{
    this->_dropTarget.clear();
    this->update();
}

void xmouse::ui::gui::LayoutView::dropEvent(QDropEvent* event)
{
    const QString id = this->_dropTarget;
    QStringList files;

    this->_dropTarget.clear();
    this->update();
    for (const QUrl& url: event->mimeData()->urls())
        if (url.isLocalFile()) files.append(url.toLocalFile());
    if (id.isEmpty() || files.isEmpty()) return;
    if (!this->_hub.sendFiles(id, files).isEmpty()) event->acceptProposedAction();
}

//----------------------------------------------------------------//
/* VIEW */

void xmouse::ui::gui::LayoutView::wheelEvent(QWheelEvent* event)
{
    const QPointF at = event->position();
    const QPointF global = (at - this->_offset) / this->_scale; // the point under the pointer stays under it
    const double factor = std::pow(1.15, event->angleDelta().y() / 120.0);

    this->fit_();
    this->_fitted = true;
    this->_scale = std::clamp(this->_scale * factor, 0.005, 2.0);
    this->_offset = at - global * this->_scale;
    this->update();
    event->accept();
}

void xmouse::ui::gui::LayoutView::resizeEvent(QResizeEvent* event)
{
    if (this->_fitted && event->oldSize().isValid()) this->_offset += QPointF(event->size().width() - event->oldSize().width(), event->size().height() - event->oldSize().height()) / 2.0;
    if (this->_recenter) this->_recenter->move(this->width() - this->_recenter->width() - 8, 8);
    QWidget::resizeEvent(event);
}

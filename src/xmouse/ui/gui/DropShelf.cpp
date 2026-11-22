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
##  @file DropShelf.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xmouse/ui/gui/DropShelf.hpp"
#include "xmouse/ui/gui/Theme.hpp"
#include <QGuiApplication>
#include <QDragEnterEvent>
#include <QVBoxLayout>
#include <QDropEvent>
#include <QMimeData>
#include <QDateTime>
#include <QPainter>
#include <QScreen>
#include <QUrl>

//----------------------------------------------------------------//
/* DEFINE */

#define POLL_INTERVAL 120 // ms
#define EDGE_DISTANCE 4 // pixels from an edge where the shelf opens
#define SHELF_WIDTH 200
#define SHELF_HEIGHT 110
#define LINGER 1500 // ms the shelf stays once the button is up (the drop arrives after it)

//----------------------------------------------------------------//
/* SHELF */

xmouse::ui::gui::ShelfWindow::ShelfWindow(void)
: QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool | Qt::WindowDoesNotAcceptFocus)
{
    QVBoxLayout* layout = new QVBoxLayout(this);

    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setAttribute(Qt::WA_ShowWithoutActivating);
    this->setProperty("xmouse_not_panel", true);
    this->setAcceptDrops(true);
    this->setFixedSize(SHELF_WIDTH, SHELF_HEIGHT);
    this->setWindowTitle("xmouse drop");
    this->_text = new QLabel(this);
    this->_text->setAlignment(Qt::AlignCenter);
    this->_text->setWordWrap(true);
    this->_text->setStyleSheet("color: white; font-weight: bold;");
    layout->addWidget(this->_text);
}

void xmouse::ui::gui::ShelfWindow::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    QColor fill = Theme::palette().accent;

    p.setRenderHint(QPainter::Antialiasing);
    fill.setAlpha(this->_hover ? 245 : 200);
    p.setPen(QPen(Qt::white, 2, this->_hover ? Qt::SolidLine : Qt::DashLine));
    p.setBrush(fill);
    p.drawRoundedRect(this->rect().adjusted(2, 2, -2, -2), 12, 12);
}

void xmouse::ui::gui::ShelfWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (!event->mimeData()->hasUrls()) return;
    this->_hover = true;
    this->update();
    event->acceptProposedAction();
}

void xmouse::ui::gui::ShelfWindow::dragLeaveEvent(QDragLeaveEvent*)
{
    this->_hover = false;
    this->update();
}

void xmouse::ui::gui::ShelfWindow::dropEvent(QDropEvent* event)
{
    QStringList files;

    this->_hover = false;
    for (const QUrl& url: event->mimeData()->urls())
        if (url.isLocalFile()) files.append(url.toLocalFile());
    if (files.isEmpty()) return;
    event->acceptProposedAction();
    emit this->dropped(files);
}

//----------------------------------------------------------------//
/* WATCHER */

xmouse::ui::gui::DropShelf::DropShelf(core::Hub& hub, QObject* parent)
: QObject(parent), _hub{hub}
{
    this->_poll.setInterval(POLL_INTERVAL);
    QObject::connect(&this->_poll, &QTimer::timeout, this, &DropShelf::poll_);
    this->_poll.start();
}

xmouse::ui::gui::DropShelf::~DropShelf()
{
    delete this->_shelf;
}

void xmouse::ui::gui::DropShelf::poll_(void)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const int held = this->_hub.heldButtons();
    const QString self = this->_hub.group().self();
    QPoint p;

    if (held < 0) {this->_poll.stop(); return;} // the platform does not tell the buttons: the drop on the map stays
    if (!this->_hub.group().joined() || this->_hub.isAway()) return;
    if (!(held & 1)) {
        // Button up: the shelf stays a moment (the drop event comes after), unless files hover it
        if (this->_shelf && this->_shelf->isVisible() && !this->_shelf->hovered()) {
            if (this->_released == 0) this->_released = now;
            if (now - this->_released > LINGER) this->_shelf->hide();
        }
        return;
    }
    this->_released = 0;
    p = this->_hub.ownPointer();
    for (const core::Edge& e: this->_hub.layout().exits(self)) {
        QPoint at;
        bool near = false;

        if (!this->_hub.reachable(e.target)) continue;
        switch (e.side) {
            case core::Side::Right:  near = p.x() >= e.line - 1 - EDGE_DISTANCE && p.y() >= e.from && p.y() <= e.to; at = QPoint(e.line - SHELF_WIDTH - 8, p.y() - SHELF_HEIGHT / 2); break;
            case core::Side::Left:   near = p.x() <= e.line + EDGE_DISTANCE && p.y() >= e.from && p.y() <= e.to; at = QPoint(e.line + 8, p.y() - SHELF_HEIGHT / 2); break;
            case core::Side::Bottom: near = p.y() >= e.line - 1 - EDGE_DISTANCE && p.x() >= e.from && p.x() <= e.to; at = QPoint(p.x() - SHELF_WIDTH / 2, e.line - SHELF_HEIGHT - 8); break;
            case core::Side::Top:    near = p.y() <= e.line + EDGE_DISTANCE && p.x() >= e.from && p.x() <= e.to; at = QPoint(p.x() - SHELF_WIDTH / 2, e.line + 8); break;
        }
        if (!near) continue;
        if (!this->_shelf) {
            this->_shelf = new ShelfWindow();
            QObject::connect(this->_shelf, &ShelfWindow::dropped, this, [this](const QStringList& files) {
                if (!this->_target.isEmpty()) (void)this->_hub.sendFiles(this->_target, files);
                this->_shelf->hide();
            });
        }
        this->_target = e.target;
        this->_shelf->setText("Drop here:\nsent to " + this->_hub.layout().device(e.target).name);
        this->_shelf->move(at);
        if (!this->_shelf->isVisible()) this->_shelf->show();
        this->_shelf->raise();
        return;
    }
}

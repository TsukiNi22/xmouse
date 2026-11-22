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
##  @file WindowViewer.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xmouse/ui/gui/WindowViewer.hpp"
#include "xmouse/platform/Keymap.hpp"
#include "xmouse/ui/gui/Theme.hpp"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QCloseEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QDateTime>
#include <QPainter>
#include <QIcon>

//----------------------------------------------------------------//
/* TOOLS */

_hot static int button_of(const Qt::MouseButton button)
{
    switch (button) {
        case Qt::LeftButton:    return 0;
        case Qt::MiddleButton:  return 1;
        case Qt::RightButton:   return 2;
        case Qt::BackButton:    return 3;
        case Qt::ForwardButton: return 4;
        default:                return -1;
    }
}

//----------------------------------------------------------------//
/* CLASS */

xmouse::ui::gui::WindowViewer::WindowViewer(core::Hub& hub, const QString& id, const QString& from, const QString& title, const QString& document)
: QWidget(nullptr), _hub{hub}, _stream{id}
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    QHBoxLayout* bar = nullptr;
    QPushButton* close = new QPushButton("Give it back");

    this->setWindowTitle(title + "  -  " + from + " (xmouse)");
    this->setWindowIcon(QIcon(Theme::logo(64)));
    this->setProperty("xmouse_not_panel", true); // the foreign cursors use it like any app (not the protected panel)
    this->setAttribute(Qt::WA_DeleteOnClose);
    this->setMouseTracking(true);
    this->setFocusPolicy(Qt::StrongFocus);
    this->resize(1024, 680);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    this->_bar = new QWidget(this);
    this->_bar->setObjectName("viewerBar");
    this->_bar->setAttribute(Qt::WA_StyledBackground, true);
    bar = new QHBoxLayout(this->_bar);
    bar->setContentsMargins(10, 6, 10, 6);
    this->_info = new QLabel("Window of <b>" + from.toHtmlEscaped() + "</b>: " + title.toHtmlEscaped() + "  (your mouse and keys drive it there)", this->_bar);
    bar->addWidget(this->_info, 1);
    if (!document.isEmpty()) {
        this->_open = new QPushButton("Open " + document + " here", this->_bar);
        this->_open->setToolTip("Handoff: the document of this window is copied here and opened with the app of this device");
        bar->addWidget(this->_open);
        QObject::connect(this->_open, &QPushButton::clicked, this, [this]() {this->_hub.openStreamDocument(this->_stream); this->_open->setEnabled(false);});
    }
    bar->addWidget(close);
    layout->addWidget(this->_bar);
    layout->addStretch(1);
    QObject::connect(close, &QPushButton::clicked, this, &QWidget::close);
}

void xmouse::ui::gui::WindowViewer::setFrame(const QImage& frame, const QSize& source)
{
    this->_frame = frame;
    this->_source = source;
    this->update();
}

void xmouse::ui::gui::WindowViewer::ended(void)
{
    this->_closing = true;
    this->close();
}

QPoint xmouse::ui::gui::WindowViewer::toSource_(const QPointF& widget) const
{
    if (this->_shown.isEmpty() || this->_source.isEmpty()) return QPoint(-1, -1);
    return QPoint(static_cast<int>((widget.x() - this->_shown.x()) * this->_source.width() / this->_shown.width()),
        static_cast<int>((widget.y() - this->_shown.y()) * this->_source.height() / this->_shown.height()));
}

void xmouse::ui::gui::WindowViewer::send_(const QJsonObject& input)
{
    this->_hub.streamInput(this->_stream, input);
}

void xmouse::ui::gui::WindowViewer::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    const QRect area = this->rect().adjusted(0, this->_bar->height(), 0, 0);

    p.fillRect(area, Qt::black);
    if (this->_frame.isNull()) {
        p.setPen(Qt::white);
        p.drawText(area, Qt::AlignCenter, "Waiting for the first picture...");
        return;
    }
    // The frame fitted in the window, its aspect kept (the input is mapped back with the same box)
    const QSize size = this->_frame.size().scaled(area.size(), Qt::KeepAspectRatio);
    this->_shown = QRect(area.x() + (area.width() - size.width()) / 2, area.y() + (area.height() - size.height()) / 2, size.width(), size.height());
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.drawImage(this->_shown, this->_frame);
}

void xmouse::ui::gui::WindowViewer::mouseMoveEvent(QMouseEvent* event)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const QPoint at = this->toSource_(event->position());

    if (at.x() < 0 || now - this->_lastMove < 16) return;
    this->_lastMove = now;
    this->send_(QJsonObject{{"k", "move"}, {"x", at.x()}, {"y", at.y()}});
}

void xmouse::ui::gui::WindowViewer::mousePressEvent(QMouseEvent* event)
{
    const QPoint at = this->toSource_(event->position());
    const int b = button_of(event->button());

    if (at.x() < 0 || b < 0) return;
    this->setFocus();
    this->send_(QJsonObject{{"k", "button"}, {"b", b}, {"down", true}, {"x", at.x()}, {"y", at.y()}});
}

void xmouse::ui::gui::WindowViewer::mouseReleaseEvent(QMouseEvent* event)
{
    const QPoint at = this->toSource_(event->position());
    const int b = button_of(event->button());

    if (b < 0) return;
    this->send_(QJsonObject{{"k", "button"}, {"b", b}, {"down", false}, {"x", std::max(0, at.x())}, {"y", std::max(0, at.y())}});
}

void xmouse::ui::gui::WindowViewer::wheelEvent(QWheelEvent* event)
{
    const QPoint at = this->toSource_(event->position());

    if (at.x() < 0) return;
    this->send_(QJsonObject{{"k", "wheel"}, {"dx", event->angleDelta().x()}, {"dy", event->angleDelta().y()}, {"x", at.x()}, {"y", at.y()}});
}

void xmouse::ui::gui::WindowViewer::keyPressEvent(QKeyEvent* event)
{
    const QString code = platform::code_from_key_event(event);

    if (event->isAutoRepeat() && !code.isEmpty()) {this->send_(QJsonObject{{"k", "key"}, {"code", code}, {"down", true}}); return;}
    if (!code.isEmpty()) this->send_(QJsonObject{{"k", "key"}, {"code", code}, {"down", true}});
    else if (!event->text().isEmpty()) this->send_(QJsonObject{{"k", "text"}, {"text", event->text()}}); // a key this platform does not name: its text
}

void xmouse::ui::gui::WindowViewer::keyReleaseEvent(QKeyEvent* event)
{
    const QString code = platform::code_from_key_event(event);

    if (!code.isEmpty() && !event->isAutoRepeat()) this->send_(QJsonObject{{"k", "key"}, {"code", code}, {"down", false}});
}

void xmouse::ui::gui::WindowViewer::closeEvent(QCloseEvent* event)
{
    if (!this->_closing) this->_hub.stopStream(this->_stream); // given back: the source stops streaming
    event->accept();
}

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
##  @file MainWindow.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xmouse/ui/gui/MainWindow.hpp"
#include "xmouse/security/Crypto.hpp"
#include "xmouse/ui/gui/Theme.hpp"
#include "xmouse/net/Protocol.hpp"
#include <qrcodegen/qrcodegen.hpp>
#include <QGuiApplication>
#include <QApplication>
#include <QHeaderView>
#include <QMessageBox>
#include <QScrollArea>
#include <QCloseEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QClipboard>
#include <QJsonArray>
#include <functional>
#include <QDateTime>
#include <QPainter>
#include <QFrame>
#include <QStyle>
#include <QMenu>

//----------------------------------------------------------------//
/* TOOLS */

static QFrame* make_card(QWidget* parent = nullptr)
{
    QFrame* card = new QFrame(parent);

    card->setObjectName("card");
    return card;
}

static QLabel* make_label(const QString& text, const QString& object = "", QWidget* parent = nullptr)
{
    QLabel* label = new QLabel(object == "subtitle" ? text.toUpper() : text, parent);

    if (!object.isEmpty()) label->setObjectName(object);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    if (object == "muted") label->setWordWrap(true); // never wider than the page
    return label;
}

static QPushButton* make_button(const QString& text, const QString& kind = "", const QString& icon = "")
{
    QPushButton* button = new QPushButton(text);

    if (!kind.isEmpty()) button->setProperty("kind", kind);
    if (!icon.isEmpty()) button->setIcon(xmouse::ui::gui::Theme::icon(icon, kind == "primary" ? xmouse::ui::gui::Theme::palette().accentText : QColor()));
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

static void set_tone(QLabel* label, const QString& tone)
{
    if (label->property("tone").toString() == tone) return;
    label->setProperty("tone", tone);
    label->style()->unpolish(label);
    label->style()->polish(label);
}

// Card with a title in small capitals, the caller fills the body layout
static QFrame* make_panel(const QString& title, QVBoxLayout*& body)
{
    QFrame* card = make_card();

    body = new QVBoxLayout(card);
    body->setContentsMargins(18, 14, 18, 16);
    body->setSpacing(10);
    body->addWidget(make_label(title, "subtitle"));
    return card;
}

static QWidget* make_page_head(const QString& title, const QString& text)
{
    QWidget* head = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(head);
    QLabel* sub = make_label(text, "muted");

    layout->setContentsMargins(0, 0, 0, 4);
    layout->setSpacing(2);
    layout->addWidget(make_label(title, "title"));
    sub->setWordWrap(true);
    layout->addWidget(sub);
    return head;
}

static QWidget* scrolled(QWidget* page)
{
    QScrollArea* area = new QScrollArea();

    area->setWidget(page);
    area->setWidgetResizable(true);
    area->setFrameShape(QFrame::NoFrame);
    return area;
}

static QPixmap qr_pixmap(const QString& text, const int size)
{
    const qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(text.toUtf8().constData(), qrcodegen::QrCode::Ecc::MEDIUM);
    const int modules = qr.getSize() + 8;
    const int scale = std::max(1, size / modules);
    QImage image(modules * scale, modules * scale, QImage::Format_RGB32);
    QPainter p(&image);

    image.fill(Qt::white);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::black);
    for (int y = 0; y < qr.getSize(); ++y)
        for (int x = 0; x < qr.getSize(); ++x)
            if (qr.getModule(x, y)) p.drawRect((x + 4) * scale, (y + 4) * scale, scale, scale);
    p.end();
    return QPixmap::fromImage(image);
}

static QPixmap color_dot(const QColor& color)
{
    QPixmap pix(12, 12);
    QPainter p(&pix);

    pix.fill(Qt::transparent);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawEllipse(1, 1, 10, 10);
    return pix;
}

//----------------------------------------------------------------//
/* CLASS */

xmouse::ui::gui::MainWindow::MainWindow(core::Hub& hub, QWidget* parent)
: QMainWindow(parent), _hub{hub}, _overlay{hub}
{
    QWidget* root = new QWidget(this);
    QHBoxLayout* rootLayout = new QHBoxLayout(root);
    QWidget* sidebar = new QWidget(root);
    QVBoxLayout* sideLayout = new QVBoxLayout(sidebar);
    QHBoxLayout* brand = new QHBoxLayout();
    QLabel* logo = new QLabel(sidebar);
    QWidget* main = new QWidget(root);
    QVBoxLayout* mainLayout = new QVBoxLayout(main);
    QFrame* rule = new QFrame(sidebar);
    QPushButton* comeBack = make_button("Bring my cursor back", "ghost", "control");

    this->setWindowTitle("xmouse - network of screens");
    this->setWindowIcon(QIcon(Theme::logo(64)));
    this->resize(1120, 760);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // Sidebar: brand, pages, theme, private mode
    sidebar->setObjectName("sidebar");
    sidebar->setAttribute(Qt::WA_StyledBackground, true);
    sidebar->setFixedWidth(220);
    sideLayout->setContentsMargins(14, 18, 14, 16);
    sideLayout->setSpacing(6);
    logo->setPixmap(Theme::logo(30));
    brand->addWidget(logo);
    brand->addWidget(make_label("xmouse", "brand"));
    brand->addStretch();
    brand->addWidget(make_label(QString("v") + XMOUSE_VERSION, "version"));
    sideLayout->addLayout(brand);
    sideLayout->addSpacing(16);
    this->_nav = new QListWidget(sidebar);
    this->_nav->setObjectName("nav");
    this->_nav->setFrameShape(QFrame::NoFrame);
    this->_nav->setIconSize(QSize(18, 18));
    for (const std::pair<QString, QString>& entry: {std::pair<QString, QString>{"Network", "devices"}, {"Layout", "screen"}, {"Activity", "logs"}, {"Settings", "settings"}})
        this->_nav->addItem(new QListWidgetItem(Theme::icon(entry.second), entry.first));
    sideLayout->addWidget(this->_nav, 1);
    sideLayout->addWidget(make_label("Theme", "subtitle"));
    sideLayout->addWidget(Theme::switcher([this](const QString& theme) {this->_hub.config().settings().theme = theme; this->_hub.config().save();}, sidebar));
    sideLayout->addSpacing(12);
    rule->setObjectName("rule");
    sideLayout->addWidget(rule);
    sideLayout->addSpacing(8);
    this->_private = new QCheckBox("Private mode", sidebar);
    this->_private->setToolTip("Every foreign cursor is refused: they stay at the edge of this device");
    this->_private->setChecked(hub.config().settings().privateMode);
    sideLayout->addWidget(this->_private);
    sideLayout->addWidget(comeBack);
    this->_status = make_label("", "muted");
    this->_status->setWordWrap(true);
    sideLayout->addWidget(this->_status);
    QObject::connect(this->_private, &QCheckBox::toggled, this, [this](const bool on) {this->_hub.setPrivateMode(on);});
    QObject::connect(comeBack, &QPushButton::clicked, &hub, &core::Hub::comeBack);

    // Banners: access asked by a foreign cursor, notices
    mainLayout->setContentsMargins(26, 22, 26, 22);
    mainLayout->setSpacing(14);
    this->_accessBanner = new QWidget(main);
    this->_accessBanner->setObjectName("focusBanner");
    this->_accessBanner->setAttribute(Qt::WA_StyledBackground, true);
    QHBoxLayout* accessLayout = new QHBoxLayout(this->_accessBanner);
    accessLayout->setContentsMargins(14, 10, 10, 10);
    this->_accessText = new QLabel(this->_accessBanner);
    this->_accessText->setWordWrap(true);
    QPushButton* refuse = make_button("Refuse", "danger");
    accessLayout->addWidget(this->_accessText, 1);
    accessLayout->addWidget(refuse);
    this->_accessBanner->hide();
    QObject::connect(refuse, &QPushButton::clicked, this, [this]() {this->_hub.refuseAccess(this->_accessId);});
    mainLayout->addWidget(this->_accessBanner);
    this->_noticeBanner = new QWidget(main);
    this->_noticeBanner->setObjectName("banner");
    this->_noticeBanner->setAttribute(Qt::WA_StyledBackground, true);
    QHBoxLayout* noticeLayout = new QHBoxLayout(this->_noticeBanner);
    noticeLayout->setContentsMargins(14, 10, 10, 10);
    this->_noticeText = new QLabel(this->_noticeBanner);
    this->_noticeText->setWordWrap(true);
    QPushButton* dismiss = make_button("Dismiss", "ghost");
    dismiss->setStyleSheet("color: white;");
    noticeLayout->addWidget(this->_noticeText, 1);
    noticeLayout->addWidget(dismiss);
    this->_noticeBanner->hide();
    QObject::connect(dismiss, &QPushButton::clicked, this->_noticeBanner, &QWidget::hide);
    mainLayout->addWidget(this->_noticeBanner);

    // Pages
    this->_pages = new QStackedWidget(main);
    this->_pages->addWidget(scrolled(this->buildNetwork_()));
    this->_pages->addWidget(this->buildLayout_());
    this->_pages->addWidget(this->buildActivity_());
    this->_pages->addWidget(scrolled(this->buildSettings_()));
    mainLayout->addWidget(this->_pages, 1);
    QObject::connect(this->_nav, &QListWidget::currentRowChanged, this->_pages, &QStackedWidget::setCurrentIndex);
    this->_nav->setCurrentRow(0);
    rootLayout->addWidget(sidebar);
    rootLayout->addWidget(main, 1);
    this->setCentralWidget(root);

    // Hub
    QObject::connect(&hub, &core::Hub::membersChanged, this, &MainWindow::refreshMembers_);
    QObject::connect(&hub, &core::Hub::networkChanged, this, &MainWindow::refreshNetwork_);
    QObject::connect(&hub, &core::Hub::presenceChanged, this, &MainWindow::refreshWhere_);
    QObject::connect(&hub, &core::Hub::awayChanged, this, &MainWindow::refreshWhere_);
    QObject::connect(&hub, &core::Hub::statusChanged, this, [this](const QString&) {this->refreshBackends_();});
    QObject::connect(&hub, &core::Hub::joinApproval, this, &MainWindow::onJoinApproval_);
    QObject::connect(&hub, &core::Hub::notice, this, [this](const QString& text) {
        this->showNotice_(text);
        if (this->_tray && !this->isActiveWindow()) this->_tray->showMessage("xmouse", text, QSystemTrayIcon::Information, 5000);
    });
    QObject::connect(&hub, &core::Hub::joinResult, this, [this](const bool ok, const QString& message) {
        this->_joinStatus->setText(message);
        set_tone(this->_joinStatus, ok ? "success" : "danger");
        if (ok) {this->_joinCode->clear(); this->refreshNetwork_();}
    });
    QObject::connect(&hub, &core::Hub::accessRequested, this, [this](const QString& id, const QString& name, const int) {
        this->_accessId = id;
        this->_accessBanner->setProperty("name", name);
        this->_accessBanner->show();
        this->raise();
        if (this->_tray) this->_tray->showMessage("Access to the panel asked", name + " asks to use the xmouse panel of this device", QSystemTrayIcon::Warning, 8000);
    });
    QObject::connect(&hub, &core::Hub::accessEnded, this, [this](const QString& id, const bool granted) {
        if (id != this->_accessId) return;
        this->_accessBanner->hide();
        if (granted) this->showNotice_(this->_accessBanner->property("name").toString() + " can now use the panel (until you use it again)", "warning");
    });
    QObject::connect(&hub.log(), &core::AuditLog::entry, this, [this](const QJsonObject& e) {
        const QString line = QDateTime::fromMSecsSinceEpoch(e.value("ts").toVariant().toLongLong()).toString("HH:mm:ss") + "  " + e.value("cat").toString().leftJustified(8) + "  " + e.value("msg").toString();
        this->_log->addItem(line);
        if (this->_log->count() > 2000) delete this->_log->takeItem(0);
        this->_log->scrollToBottom();
    });

    // 1 s: code countdown, access countdown, latencies
    this->_tick.setInterval(1000);
    QObject::connect(&this->_tick, &QTimer::timeout, this, [this]() {
        this->refreshCode_();
        if (this->_accessBanner->isVisible()) {
            const core::Foreign f = this->_hub.foreign().value(this->_accessId);
            const qint64 left = this->_hub.config().settings().focusRequestDelay - (QDateTime::currentMSecsSinceEpoch() - f.askedAt) / 1000;
            this->_accessText->setText("<b>" + f.name.toHtmlEscaped() + "</b> asks to use this xmouse panel with its cursor: granted in <b>" + QString::number(std::max<qint64>(0, left)) + " s</b> unless you refuse (or use the panel yourself).");
        }
    });
    this->_tick.start();
    qApp->installEventFilter(this);
    this->buildTray_();
    this->refreshNetwork_();
    this->refreshBackends_();
    for (const QJsonObject& e: hub.log().recent(300))
        emit hub.log().entry(e);
}

xmouse::ui::gui::MainWindow::~MainWindow()
{
    qApp->removeEventFilter(this);
}

//----------------------------------------------------------------//
/* PAGES */

QWidget* xmouse::ui::gui::MainWindow::buildNetwork_(void)
{
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);
    QWidget* alone = new QWidget();
    QVBoxLayout* aloneLayout = new QVBoxLayout(alone);
    QWidget* joined = new QWidget();
    QVBoxLayout* joinedLayout = new QVBoxLayout(joined);
    QVBoxLayout* body = nullptr;
    QFrame* card = nullptr;

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);
    layout->addWidget(make_page_head("Network", "The devices whose screens and cursors are linked. Every link is TLS 1.3 with a post-quantum key exchange, a device joins with the one time code (or the QR code) of any member."));
    this->_networkStack = new QStackedWidget(page);
    aloneLayout->setContentsMargins(0, 0, 0, 0);
    aloneLayout->setSpacing(14);
    joinedLayout->setContentsMargins(0, 0, 0, 0);
    joinedLayout->setSpacing(14);

    // Not in a network: create one, or join one
    card = make_panel("Create a network", body);
    this->_createName = new QLineEdit();
    this->_createName->setPlaceholderText(this->_hub.config().deviceName() + " network");
    QPushButton* create = make_button("Create", "primary", "devices");
    QHBoxLayout* createRow = new QHBoxLayout();
    createRow->addWidget(this->_createName, 1);
    createRow->addWidget(create);
    body->addWidget(make_label("This device becomes the first member: the others join with its code.", "muted"));
    body->addLayout(createRow);
    QObject::connect(create, &QPushButton::clicked, this, [this]() {this->_hub.createNetwork(this->_createName->text());});
    aloneLayout->addWidget(card);

    // Join (shown in both states: joining another network leaves the current one)
    const std::function<QFrame*(QListWidget*&)> joinCard = [this](QListWidget*& nearby) {
        QVBoxLayout* b = nullptr;
        QFrame* c = make_panel("Join a network", b);
        QHBoxLayout* row = new QHBoxLayout();
        QPushButton* join = make_button("Join", "primary", "control");
        QLineEdit* address = new QLineEdit();
        QLineEdit* code = new QLineEdit();
        if (!this->_joinAddress) {this->_joinAddress = address; this->_joinCode = code;}
        address->setPlaceholderText("address[:port] or xmouse:// link");
        code->setPlaceholderText("K7F-29Q-XM4");
        code->setObjectName("codeInput");
        code->setMaximumWidth(170);
        row->addWidget(address, 1);
        row->addWidget(code);
        row->addWidget(join);
        b->addWidget(make_label("The code (or the QR code) is shown on the Network page of a member.", "muted"));
        b->addLayout(row);
        nearby = new QListWidget();
        nearby->setMaximumHeight(130);
        b->addWidget(make_label("Devices on this LAN (click to fill the address)", "muted"));
        b->addWidget(nearby);
        QObject::connect(join, &QPushButton::clicked, this, [this, address, code]() {
            this->_joinAddress = address;
            this->_joinCode = code;
            this->joinFromFields_();
        });
        QObject::connect(code, &QLineEdit::returnPressed, join, &QPushButton::click);
        QObject::connect(nearby, &QListWidget::itemClicked, this, [address](QListWidgetItem* item) {address->setText(item->data(Qt::UserRole).toString());});
        return c;
    };
    aloneLayout->addWidget(joinCard(this->_nearbyAlone));
    this->_joinStatus = make_label("", "muted");
    aloneLayout->addWidget(this->_joinStatus);
    aloneLayout->addStretch();

    // In a network: add a device (code + QR), the members
    this->_networkTitle = make_label("", "title");
    joinedLayout->addWidget(this->_networkTitle);
    card = make_panel("Add a device", body);
    QHBoxLayout* pair = new QHBoxLayout();
    QVBoxLayout* pairText = new QVBoxLayout();
    QHBoxLayout* pairButtons = new QHBoxLayout();
    QPushButton* copy = make_button("Copy the link", "ghost", "copy");
    QPushButton* renew = make_button("New code", "ghost", "refresh");
    this->_code = make_label("", "code");
    this->_codeLeft = make_label("", "muted");
    this->_link = make_label("", "mono");
    this->_qr = new QLabel();
    this->_qr->setObjectName("qr");
    this->_qr->setFixedSize(168, 168);
    this->_qr->setScaledContents(true);
    pairText->addWidget(make_label("On the new device: xmouse, Join a network, this address and this code (or scan the QR code with the phone app). Used once, then a new code.", "muted"));
    pairText->addWidget(this->_code);
    pairText->addWidget(this->_codeLeft);
    pairText->addWidget(this->_link);
    pairButtons->addWidget(copy);
    pairButtons->addWidget(renew);
    pairButtons->addStretch();
    pairText->addLayout(pairButtons);
    pairText->addStretch();
    pair->addLayout(pairText, 1);
    pair->addWidget(this->_qr, 0, Qt::AlignTop);
    body->addLayout(pair);
    QObject::connect(copy, &QPushButton::clicked, this, [this]() {QGuiApplication::clipboard()->setText(this->_hub.joinLink());});
    QObject::connect(renew, &QPushButton::clicked, this, [this]() {this->_hub.code().rotate(); this->refreshCode_();});
    QObject::connect(&this->_hub.code(), &security::PairingCode::changed, this, &MainWindow::refreshCode_);
    joinedLayout->addWidget(card);

    card = make_panel("Members", body);
    this->_members = new QTableWidget(0, 7);
    this->_members->setHorizontalHeaderLabels({"Device", "System", "Link", "Latency", "Address", "Allowed here", ""});
    this->_members->verticalHeader()->hide();
    this->_members->setSelectionMode(QAbstractItemView::NoSelection);
    this->_members->setEditTriggers(QAbstractItemView::NoEditTriggers);
    this->_members->horizontalHeader()->setStretchLastSection(false);
    this->_members->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    this->_members->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    this->_members->setMinimumHeight(220);
    Theme::decorate(this->_members);
    body->addWidget(this->_members);
    joinedLayout->addWidget(card);
    QListWidget* nearbyJoined = nullptr;
    joinedLayout->addWidget(joinCard(nearbyJoined));
    this->_nearby = nearbyJoined;
    QPushButton* leave = make_button("Leave this network", "danger", "power");
    QHBoxLayout* leaveRow = new QHBoxLayout();
    leaveRow->addStretch();
    leaveRow->addWidget(leave);
    joinedLayout->addLayout(leaveRow);
    joinedLayout->addStretch();
    QObject::connect(leave, &QPushButton::clicked, this, [this]() {
        if (QMessageBox::question(this, "Leave the network", "This device leaves the network: the others forget it, a new code of a member is needed to come back.") == QMessageBox::Yes) this->_hub.leave();
    });

    this->_networkStack->addWidget(alone);
    this->_networkStack->addWidget(joined);
    layout->addWidget(this->_networkStack);
    QObject::connect(&this->_hub.mesh(), &net::Mesh::announced, this, &MainWindow::refreshNearby_);
    return page;
}

QWidget* xmouse::ui::gui::MainWindow::buildLayout_(void)
{
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);
    QVBoxLayout* body = nullptr;
    QFrame* card = nullptr;
    QHBoxLayout* actions = new QHBoxLayout();
    QPushButton* reset = make_button("Side by side", "ghost", "refresh");

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);
    layout->addWidget(make_page_head("Layout", "Drag the screens of the devices where they are on your desk: the cursor goes from one to the other through the borders that touch (the same layout on every member). The dots are the cursors of the members, live."));
    card = make_panel("Screens", body);
    this->_layoutView = new LayoutView(this->_hub, card);
    body->addWidget(this->_layoutView, 1);
    actions->addWidget(reset);
    actions->addStretch();
    this->_windowMove = new QCheckBox("Windows can move between devices (whole network)");
    this->_windowMove->setToolTip("Off: dragging a window to an edge behaves as usual. The move itself (handoff / streamed window) comes in v0.4");
    actions->addWidget(this->_windowMove);
    body->addLayout(actions);
    layout->addWidget(card, 3);
    card = make_panel("Where the cursors are", body);
    this->_where = make_label("", "");
    this->_where->setWordWrap(true);
    this->_where->setTextFormat(Qt::RichText);
    body->addWidget(this->_where);
    layout->addWidget(card, 1);
    QObject::connect(reset, &QPushButton::clicked, &this->_hub, &core::Hub::resetLayout);
    QObject::connect(this->_windowMove, &QCheckBox::toggled, this, [this](const bool on) {this->_hub.setNetworkSetting("windowMove", on);});
    return page;
}

QWidget* xmouse::ui::gui::MainWindow::buildActivity_(void)
{
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);
    QVBoxLayout* body = nullptr;
    QFrame* card = nullptr;

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);
    layout->addWidget(make_page_head("Activity", "Links, joins, cursors entering this device, access requests to the panel (also in " + core::Config::logDir() + ")."));
    card = make_panel("Journal", body);
    this->_log = new QListWidget();
    this->_log->setObjectName("mono");
    this->_log->setFont(QFont(Theme::monospaceFamily()));
    body->addWidget(this->_log, 1);
    layout->addWidget(card, 1);
    return page;
}

QWidget* xmouse::ui::gui::MainWindow::buildSettings_(void)
{
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);
    QVBoxLayout* body = nullptr;
    QFrame* card = nullptr;
    core::Settings& s = this->_hub.config().settings();
    QFormLayout* form = nullptr;
    const std::function<void(void)> save = [this]() {this->_hub.config().save();};
    const std::function<QCheckBox*(const QString&, bool&, const QString&)> check = [save](const QString& text, bool& value, const QString& tip) {
        QCheckBox* box = new QCheckBox(text);
        box->setChecked(value);
        box->setToolTip(tip);
        QObject::connect(box, &QCheckBox::toggled, box, [&value, save](const bool on) {value = on; save();});
        return box;
    };

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);
    layout->addWidget(make_page_head("Settings", "Settings of this device (the network wide ones are on the Layout page)."));

    card = make_panel("This device", body);
    form = new QFormLayout();
    this->_deviceName = new QLineEdit(s.name);
    this->_deviceName->setPlaceholderText(this->_hub.config().deviceName());
    form->addRow("Name", this->_deviceName);
    QSpinBox* port = new QSpinBox();
    port->setRange(1, 65535);
    port->setValue(s.port);
    form->addRow("Port (restart)", port);
    QComboBox* exposure = new QComboBox();
    for (const char* mode: {"lan", "tailscale", "upnp", "bore", "ngrok", "manual"})
        exposure->addItem(mode);
    exposure->setCurrentText(core::exposure_name(s.exposure));
    form->addRow("Reachable through (restart)", exposure);
    body->addLayout(form);
    QObject::connect(this->_deviceName, &QLineEdit::editingFinished, this, [this]() {this->_hub.renameDevice(this->_deviceName->text());});
    QObject::connect(port, &QSpinBox::valueChanged, this, [this, save](const int v) {this->_hub.config().settings().port = static_cast<std::uint16_t>(v); save();});
    QObject::connect(exposure, &QComboBox::currentTextChanged, this, [this, save](const QString& v) {this->_hub.config().settings().exposure = core::exposure_from(v); save();});
    layout->addWidget(card);

    card = make_panel("Access", body);
    body->addWidget(check("A new device must also be accepted here (after its code)", s.joinApproval, ""));
    body->addWidget(check("A foreign cursor may ask the access to this panel", s.focusRequest, "Like xdesk: its clicks on the panel are held, a second click asks, granted after the delay unless refused"));
    form = new QFormLayout();
    QSpinBox* delay = new QSpinBox();
    delay->setRange(5, 3600);
    delay->setSuffix(" s");
    delay->setValue(s.focusRequestDelay);
    form->addRow("Delay before the access is granted", delay);
    body->addLayout(form);
    QObject::connect(delay, &QSpinBox::valueChanged, this, [this, save](const int v) {this->_hub.config().settings().focusRequestDelay = v; save();});
    layout->addWidget(card);

    card = make_panel("Input and clipboard", body);
    body->addWidget(check("Copies of this device go to the members (text and images)", s.clipboard, ""));
    body->addWidget(check("The real pointer goes back after a foreign click (X11, Windows)", s.restorePointer, ""));
    body->addWidget(make_label("Ctrl + Alt + Shift brings the cursor of this device back at once.", "muted"));
    layout->addWidget(card);

    card = make_panel("Backends", body);
    this->_backends = make_label("", "mono");
    this->_backends->setWordWrap(true);
    body->addWidget(this->_backends);
    layout->addWidget(card);
    layout->addStretch();
    return page;
}

void xmouse::ui::gui::MainWindow::buildTray_(void)
{
    QMenu* menu = new QMenu(this);

    if (!QSystemTrayIcon::isSystemTrayAvailable()) return;
    this->_tray = new QSystemTrayIcon(QIcon(Theme::logo(64)), this);
    menu->addAction("Show the panel", this, [this]() {this->showNormal(); this->raise(); this->activateWindow();});
    menu->addAction("Bring my cursor back", &this->_hub, &core::Hub::comeBack);
    QAction* priv = menu->addAction("Private mode");
    priv->setCheckable(true);
    priv->setChecked(this->_hub.config().settings().privateMode);
    QObject::connect(priv, &QAction::toggled, this->_private, &QCheckBox::setChecked);
    QObject::connect(this->_private, &QCheckBox::toggled, priv, &QAction::setChecked);
    menu->addSeparator();
    menu->addAction("Quit", this, [this]() {this->_quitting = true; this->close();});
    this->_tray->setContextMenu(menu);
    this->_tray->setToolTip("xmouse");
    QObject::connect(this->_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {this->showNormal(); this->raise(); this->activateWindow();}
    });
    this->_tray->show();
}

//----------------------------------------------------------------//
/* REFRESH */

void xmouse::ui::gui::MainWindow::refreshNetwork_(void)
{
    const bool joined = this->_hub.group().joined();
    const QString name = this->_hub.state().value("net/name").toString(this->_hub.group().name());

    this->_networkStack->setCurrentIndex(joined ? 1 : 0);
    this->_networkTitle->setText(name);
    this->_windowMove->blockSignals(true);
    this->_windowMove->setChecked(this->_hub.state().value("net/windowMove").toBool(true));
    this->_windowMove->setEnabled(joined);
    this->_windowMove->blockSignals(false);
    this->refreshCode_();
    this->refreshMembers_();
    this->refreshNearby_();
}

void xmouse::ui::gui::MainWindow::refreshCode_(void)
{
    const QString link = this->_hub.joinLink();
    const int left = this->_hub.code().secondsLeft();

    if (!this->_hub.group().joined()) return;
    this->_code->setText(this->_hub.code().display());
    this->_codeLeft->setText(left < 0 ? "valid until used" : "new code in " + QString::number(left / 60) + " min " + QString::number(left % 60).rightJustified(2, '0') + " s");
    if (this->_link->toolTip() != link) {
        this->_link->setText(link.section('?', 0, 0) + "?fp=" + this->_hub.state().value("device/" + this->_hub.group().self()).toObject().value("fp").toString().left(16) + "...");
        this->_link->setToolTip(link);
        this->_qr->setPixmap(qr_pixmap(link, 336));
    }
}

void xmouse::ui::gui::MainWindow::refreshMembers_(void)
{
    const QList<core::Member> members = this->_hub.members();

    if (!this->_members) return;
    this->_members->setRowCount(static_cast<int>(members.size()));
    for (int row = 0; row < members.size(); ++row) {
        const core::Member& m = members[row];
        QTableWidgetItem* name = new QTableWidgetItem(QIcon(color_dot(m.color)), m.name + (m.self ? "  (this device)" : ""));
        QWidget* rights = new QWidget();
        QHBoxLayout* rightsLayout = new QHBoxLayout(rights);
        const core::Rights r = this->_hub.config().rightsOf(m.id);

        this->_members->setItem(row, 0, name);
        this->_members->setItem(row, 1, new QTableWidgetItem(m.os + "  " + m.version));
        this->_members->setItem(row, 2, new QTableWidgetItem(m.self ? "-" : (m.online ? "online" : "offline")));
        this->_members->setItem(row, 3, new QTableWidgetItem(m.self || m.rtt < 0 ? "-" : QString::number(m.rtt) + " ms"));
        this->_members->setItem(row, 4, new QTableWidgetItem(m.self ? "" : this->_hub.mesh().addressesOf(m.id).join(", ")));
        rightsLayout->setContentsMargins(4, 0, 4, 0);
        rightsLayout->setSpacing(8);
        if (!m.self) {
            const std::array<std::pair<QString, int>, 4> boxes = {std::pair<QString, int>{"Mouse", 0}, {"Keys", 1}, {"Clipboard", 2}, {"Files", 3}};
            for (const auto& [label, index]: boxes) {
                QCheckBox* box = new QCheckBox(label);
                const bool values[] = {r.mouse, r.keyboard, r.clipboard, r.files};
                box->setChecked(values[index]);
                rightsLayout->addWidget(box);
                QObject::connect(box, &QCheckBox::toggled, this, [this, id = m.id, index](const bool on) {
                    core::Rights rr = this->_hub.config().rightsOf(id);
                    if (index == 0) rr.mouse = on;
                    else if (index == 1) rr.keyboard = on;
                    else if (index == 2) rr.clipboard = on;
                    else rr.files = on;
                    this->_hub.setRights(id, rr);
                });
            }
        }
        this->_members->setCellWidget(row, 5, rights);
        if (!m.self) {
            QPushButton* remove = make_button("Remove", "ghost");
            QObject::connect(remove, &QPushButton::clicked, this, [this, id = m.id, name = m.name]() {
                if (QMessageBox::question(this, "Remove " + name, name + " leaves the network: the key of the network changes, it needs a new code to come back.") == QMessageBox::Yes) this->_hub.removeMember(id);
            });
            this->_members->setCellWidget(row, 6, remove);
        } else this->_members->setCellWidget(row, 6, nullptr);
        set_tone(this->_status, "muted");
    }
    this->refreshWhere_();
    this->refreshBackends_();
}

void xmouse::ui::gui::MainWindow::refreshNearby_(void)
{
    for (QListWidget* list: {this->_nearby, this->_nearbyAlone}) {
        if (!list) continue;
        list->clear();
        for (const net::Announce& a: this->_hub.nearby()) {
            QListWidgetItem* item = new QListWidgetItem(a.name + "  -  " + a.host + ":" + QString::number(a.port) + (a.network.isEmpty() ? "" : "  (" + a.network + ")"));
            item->setData(Qt::UserRole, a.host + ":" + QString::number(a.port));
            list->addItem(item);
        }
    }
}

void xmouse::ui::gui::MainWindow::refreshWhere_(void)
{
    QStringList lines;
    QHash<QString, QString> names;

    if (!this->_where) return;
    for (const core::Member& m: this->_hub.members())
        names.insert(m.id, m.name);
    for (const core::Member& m: this->_hub.members()) {
        const core::Presence p = this->_hub.presence().value(m.id);
        QString where = "on itself";
        if (!p.on.isEmpty() && p.on != m.id) where = "on <b>" + names.value(p.on, p.on).toHtmlEscaped() + "</b>";
        if (m.self && this->_hub.isAway()) where = "on <b>" + names.value(this->_hub.target()).toHtmlEscaped() + "</b>";
        if (!m.online) where = "offline";
        lines.append("<span style='color:" + m.color.name() + "'>&#9679;</span> <b>" + m.name.toHtmlEscaped() + "</b>: cursor and keyboard " + where);
    }
    for (const core::Foreign& f: this->_hub.foreign())
        if (f.state != core::ForeignState::Free) lines.append("<i>" + f.name.toHtmlEscaped() + " is held on this panel</i>");
    this->_where->setText(lines.isEmpty() ? "No member yet" : lines.join("<br>"));
    this->_status->setText(this->_hub.isAway() ? "Your cursor is on " + names.value(this->_hub.target()) + " (Ctrl + Alt + Shift to come back)" : this->_hub.status());
}

void xmouse::ui::gui::MainWindow::refreshBackends_(void)
{
    QStringList lines;

    if (!this->_backends) return;
    lines.append("edges      " + this->_hub.captureName() + (this->_hub.captureError().isEmpty() ? "" : "  (" + this->_hub.captureError() + ")"));
    lines.append("injection  " + this->_hub.injectorName() + (this->_hub.injectorError().isEmpty() ? "" : "  (" + this->_hub.injectorError() + ")"));
    lines.append("display    " + QGuiApplication::platformName() + " (" + qEnvironmentVariable("XDG_SESSION_TYPE", "native") + " session)");
    lines.append("security   " + security::tls_summary());
    lines.append("identity   " + security::fingerprint_short(this->_hub.fingerprint()));
    lines.append("port       " + QString::number(this->_hub.mesh().port()));
    this->_backends->setText(lines.join("\n"));
    if (!this->_hub.isAway()) this->_status->setText(this->_hub.status());
}

void xmouse::ui::gui::MainWindow::showNotice_(const QString& text, const QString& tone)
{
    (void)tone;
    this->_noticeText->setText(text);
    this->_noticeBanner->show();
    QTimer::singleShot(10000, this->_noticeBanner, &QWidget::hide);
}

void xmouse::ui::gui::MainWindow::joinFromFields_(void)
{
    if (this->_hub.group().joined() && QMessageBox::question(this, "Join another network", "This device leaves its current network to join the other one.") != QMessageBox::Yes) return;
    this->_joinStatus->setText("Joining...");
    set_tone(this->_joinStatus, "muted");
    this->_hub.join(this->_joinAddress->text(), this->_joinCode->text());
}

void xmouse::ui::gui::MainWindow::onJoinApproval_(net::Peer* peer)
{
    QMessageBox* box = new QMessageBox(QMessageBox::Question, "A device wants to join", "", QMessageBox::NoButton, this);
    QPointer<net::Peer> guard = peer;

    box->setText("<b>" + peer->name().toHtmlEscaped() + "</b> (" + peer->os().toHtmlEscaped() + ", " + peer->address().toString() + ") gave the right code and wants to join the network.<br><br>Refused by default in 30 s.");
    box->addButton("Accept", QMessageBox::AcceptRole);
    box->addButton("Refuse", QMessageBox::RejectRole);
    box->setAttribute(Qt::WA_DeleteOnClose);
    QObject::connect(box, &QMessageBox::buttonClicked, this, [box, guard](QAbstractButton* button) {
        if (guard) guard->approve(box->buttonRole(button) == QMessageBox::AcceptRole);
    });
    QObject::connect(peer, &net::Peer::closed, box, &QMessageBox::close);
    QTimer::singleShot(30000, box, &QMessageBox::close);
    box->show();
    box->raise();
    if (this->_tray) this->_tray->showMessage("A device wants to join", peer->name() + " gave the right code", QSystemTrayIcon::Warning, 8000);
}

//----------------------------------------------------------------//
/* EVENTS */

void xmouse::ui::gui::MainWindow::reportPanel_(void)
{
    QList<QRect> rects;

    // Every window of xmouse that a cursor could click (not the click-through overlays)
    for (QWidget* top: QApplication::topLevelWidgets()) {
        if (!top->isVisible() || top->testAttribute(Qt::WA_TransparentForMouseEvents)) continue;
        if (top->windowFlags() & Qt::WindowTransparentForInput) continue;
        rects.append(top->frameGeometry());
    }
    this->_hub.setPanelRects(rects);
    this->_hub.setPanelFocused(QApplication::activeWindow() != nullptr);
}

void xmouse::ui::gui::MainWindow::closeEvent(QCloseEvent* event)
{
    if (!this->_quitting && this->_tray && QSystemTrayIcon::isSystemTrayAvailable()) {
        event->ignore();
        this->hide();
        this->_tray->showMessage("xmouse keeps running", "The screens stay linked. Quit from the tray menu.", QSystemTrayIcon::Information, 4000);
        return;
    }
    event->accept();
    QApplication::quit();
}

void xmouse::ui::gui::MainWindow::changeEvent(QEvent* event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::ActivationChange) this->reportPanel_();
}

void xmouse::ui::gui::MainWindow::moveEvent(QMoveEvent* event)
{
    QMainWindow::moveEvent(event);
    this->reportPanel_();
}

void xmouse::ui::gui::MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    this->reportPanel_();
}

void xmouse::ui::gui::MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    this->reportPanel_();
}

void xmouse::ui::gui::MainWindow::hideEvent(QHideEvent* event)
{
    QMainWindow::hideEvent(event);
    QTimer::singleShot(0, this, &MainWindow::reportPanel_);
}

bool xmouse::ui::gui::MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    // A click / key of the local person in a window of xmouse ends the accesses granted to the foreign cursors
    if ((event->type() == QEvent::MouseButtonPress || event->type() == QEvent::KeyPress) && watched->isWidgetType() && !this->_hub.injectingNow())
        this->_hub.panelUsedLocally();
    if (event->type() == QEvent::WindowActivate || event->type() == QEvent::WindowDeactivate) QTimer::singleShot(0, this, &MainWindow::reportPanel_);
    return QMainWindow::eventFilter(watched, event);
}

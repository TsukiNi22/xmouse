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
#include <QDesktopServices>
#include <QGuiApplication>
#include <QApplication>
#include <QInputDialog>
#include <QFileDialog>
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
#include <QUrlQuery>
#include <QDateTime>
#include <QPainter>
#include <QDialog>
#include <QFrame>
#include <QStyle>
#include <QMenu>
#include <QUrl>
#include <QDir>

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

// Style property: the style sheet is applied again only when the value changes
static void set_prop(QWidget* widget, const char* name, const QVariant& value)
{
    if (widget->property(name) == value) return;
    widget->setProperty(name, value);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
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

    image.fill(Qt::white); // before the painter
    QPainter p(&image);
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

    pix.fill(Qt::transparent); // before the painter: a pixmap being painted on cannot be filled
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawEllipse(1, 1, 10, 10);
    return pix;
}

//----------------------------------------------------------------//
/* CLASS */

xmouse::ui::gui::MainWindow::MainWindow(core::Hub& hub, QWidget* parent)
: QMainWindow(parent), _hub{hub}, _overlay{hub}, _shelf{hub}
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

    // Network switch at the bottom (always in reach, like the sharing switch of xdesk): red = leave, green = create / join
    this->_leave = make_button("", "", "power");
    this->_leave->setObjectName("share");
    sideLayout->addSpacing(6);
    sideLayout->addWidget(this->_leave);
    QObject::connect(this->_leave, &QPushButton::clicked, this, [this]() {
        if (!this->_hub.group().joined()) {this->showPage(0); this->_createName->setFocus(); return;}
        if (QMessageBox::question(this, "Leave the network",
            "This device leaves the network <b>" + this->_hub.state().value("net/name").toString(this->_hub.group().name()).toHtmlEscaped() + "</b>.<br><br>"
            "The other members stay linked together: there is no server, the network lives as long as one of its members runs. "
            "To come back, this device needs a new code of a member.") == QMessageBox::Yes) this->_hub.leave();
    });
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
    this->_accessAccept = make_button("Accept", "primary");
    this->_accessRefuse = make_button("Refuse", "danger");
    this->_accessRevoke = make_button("Revoke", "danger");
    accessLayout->addWidget(this->_accessText, 1);
    accessLayout->addWidget(this->_accessAccept);
    accessLayout->addWidget(this->_accessRefuse);
    accessLayout->addWidget(this->_accessRevoke);
    this->_accessBanner->hide();
    QObject::connect(this->_accessAccept, &QPushButton::clicked, this, [this]() {this->_hub.grantAccess(this->_accessId);});
    QObject::connect(this->_accessRefuse, &QPushButton::clicked, this, [this]() {this->_hub.refuseAccess(this->_accessId);});
    QObject::connect(this->_accessRevoke, &QPushButton::clicked, this, [this]() {
        this->_hub.revokeAccess(this->_accessId);
        this->_accessBanner->hide();
    });
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
        this->_accessAccept->show();
        this->_accessRefuse->show();
        this->_accessRevoke->hide();
        this->_accessBanner->show();
        this->raise();
        if (this->_tray) this->_tray->showMessage("Access to the panel asked", name + " asks to use the xmouse panel of this device", QSystemTrayIcon::Warning, 8000);
    });
    QObject::connect(&hub, &core::Hub::accessEnded, this, [this](const QString& id, const bool granted) {
        if (id != this->_accessId) return;
        if (!granted) {this->_accessBanner->hide(); return;}

        // Granted: the banner stays, the access is taken back only with Revoke (the clicks here never end it)
        this->_accessText->setText("<b>" + this->_accessBanner->property("name").toString().toHtmlEscaped() + "</b> can use this xmouse panel with its cursor, until you revoke it.");
        this->_accessAccept->hide();
        this->_accessRefuse->hide();
        this->_accessRevoke->show();
        this->_accessBanner->show();
    });
    QObject::connect(&hub, &core::Hub::padsChanged, this, &MainWindow::refreshPads_);
    QObject::connect(&hub, &core::Hub::padApproval, this, &MainWindow::onPadApproval_);
    QObject::connect(&hub, &core::Hub::linkReceived, this, &MainWindow::onLink_);
    QObject::connect(&hub, &core::Hub::layoutChanged, this, &MainWindow::refreshProfiles_);
    QObject::connect(&hub, &core::Hub::followChanged, this, &MainWindow::refreshFollow_);
    QObject::connect(&hub, &core::Hub::transfersChanged, this, &MainWindow::refreshTransfers_);
    QObject::connect(&hub, &core::Hub::filesReceived, this, [this](const QString& from, const QStringList& paths, const QString& folder) {
        if (this->_tray) this->_tray->showMessage("Files from " + from, QString::number(paths.size()) + " file(s) in " + folder, QSystemTrayIcon::Information, 6000);
        this->refreshTransfers_();
    });

    // Windows of the other devices shown here (one viewer per stream)
    QObject::connect(&hub, &core::Hub::streamStarted, this, [this](const QString& id, const QString& from, const QString& title, const QString& document) {
        WindowViewer* viewer = new WindowViewer(this->_hub, id, from, title, document);
        this->_viewers.insert(id, viewer);
        viewer->show();
        viewer->raise();
    });
    QObject::connect(&hub, &core::Hub::streamFrame, this, [this](const QString& id, const QImage& frame, const QSize& size) {
        if (this->_viewers.value(id)) this->_viewers.value(id)->setFrame(frame, size);
    });
    QObject::connect(&hub, &core::Hub::streamEnded, this, [this](const QString& id) {
        QPointer<WindowViewer> viewer = this->_viewers.take(id);
        if (viewer) viewer->ended();
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
        this->refreshStatus_();
        this->refreshEndpoints_();
        if (this->_accessBanner->isVisible() && this->_accessAccept->isVisible()) { // asking: its countdown
            const core::Foreign f = this->_hub.foreign().value(this->_accessId);
            const qint64 left = this->_hub.config().settings().focusRequestDelay - (QDateTime::currentMSecsSinceEpoch() - f.askedAt) / 1000;
            this->_accessText->setText("<b>" + f.name.toHtmlEscaped() + "</b> asks to use this xmouse panel with its cursor: granted in <b>" + QString::number(std::max<qint64>(0, left)) + " s</b> unless you refuse, or at once if you accept.");
        }
    });
    this->_tick.start();
    qApp->installEventFilter(this);
    this->buildTray_();
    this->refreshNetwork_();
    this->refreshBackends_();
    this->refreshPads_();
    this->refreshProfiles_();
    this->refreshFollow_();
    this->refreshTransfers_();
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
    QHBoxLayout* joinedLayout = new QHBoxLayout(joined);
    QWidget* head = make_page_head("Network", "");
    QVBoxLayout* body = nullptr;
    QFrame* card = nullptr;

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);
    this->_identity = head->findChildren<QLabel*>().last();
    this->_identity->setTextFormat(Qt::RichText);
    layout->addWidget(head);
    aloneLayout->setContentsMargins(0, 0, 0, 0);
    joinedLayout->setContentsMargins(0, 0, 0, 0);
    joinedLayout->setSpacing(16);

    // No network: this device creates it (the others join with its code)
    card = make_panel("Create a network", body);
    this->_createName = new QLineEdit();
    this->_createName->setPlaceholderText(this->_hub.config().deviceName() + " network");
    QPushButton* create = make_button("Create", "primary", "devices");
    QHBoxLayout* createRow = new QHBoxLayout();
    createRow->addWidget(this->_createName, 1);
    createRow->addWidget(create);
    body->addWidget(make_label("This device becomes the first member: the others join with its code. There is no server: the network lives as long as one of its members runs.", "muted"));
    body->addLayout(createRow);
    QObject::connect(create, &QPushButton::clicked, this, [this]() {this->_hub.createNetwork(this->_createName->text());});
    QObject::connect(this->_createName, &QLineEdit::returnPressed, create, &QPushButton::click);
    aloneLayout->addWidget(card);

    // In a network: the connection code (left) and the state of this device (right)
    {
        QVBoxLayout* code = nullptr;
        QFrame* codeCard = make_panel("Connection code", code);
        QHBoxLayout* buttons = new QHBoxLayout();
        QPushButton* copy = make_button("Copy", "", "copy");
        QPushButton* qr = make_button("QR code", "", "qr");
        QPushButton* renew = make_button("New code", "ghost", "refresh");
        QVBoxLayout* status = nullptr;
        QFrame* statusCard = make_panel("Status", status);
        QFormLayout* form = new QFormLayout();

        codeCard->setProperty("accent", true);
        code->addWidget(make_label("Give it to the device that joins (the address and this code, or the QR code): it works once, then changes", "muted"));
        this->_code = make_label("---", "code");
        code->addWidget(this->_code);
        this->_codeBar = new QProgressBar(codeCard);
        this->_codeBar->setTextVisible(false);
        this->_codeBar->setFixedHeight(3);
        code->addWidget(this->_codeBar);
        this->_codeTimer = make_label("", "muted");
        code->addWidget(this->_codeTimer);
        this->_fingerprint = make_label("", "muted");
        this->_fingerprint->setToolTip(security::fingerprint_text(this->_hub.fingerprint()));
        code->addWidget(this->_fingerprint);
        code->addStretch();
        buttons->addWidget(copy);
        buttons->addWidget(qr);
        buttons->addWidget(renew);
        buttons->addStretch();
        code->addLayout(buttons);
        copy->setToolTip("Copy the code");
        qr->setToolTip("A QR code with an address of this device and the code: scanned by the phone app, or its link pasted in Connect to a network");
        QObject::connect(copy, &QPushButton::clicked, this, [this, copy]() {
            QGuiApplication::clipboard()->setText(this->_hub.code().display());
            copy->setText("Copied");
            QTimer::singleShot(1500, copy, [copy]() {copy->setText("Copy");});
        });
        QObject::connect(qr, &QPushButton::clicked, this, [this]() {this->showQr_(-1);});
        QObject::connect(renew, &QPushButton::clicked, this, [this]() {this->_hub.code().rotate(); this->refreshCode_();});
        QObject::connect(&this->_hub.code(), &security::PairingCode::changed, this, &MainWindow::refreshCode_);

        form->setVerticalSpacing(12);
        form->setHorizontalSpacing(18);
        form->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
        this->_statusEdges = make_label("-");
        this->_statusEdges->setWordWrap(true);
        this->_statusInput = make_label("-");
        this->_statusInput->setWordWrap(true);
        this->_statusTls = make_label(security::tls_summary());
        this->_statusTls->setWordWrap(true);
        this->_statusMembers = make_label("-");
        form->addRow(make_label("Edges", "muted"), this->_statusEdges);
        form->addRow(make_label("Input", "muted"), this->_statusInput);
        form->addRow(make_label("Encryption", "muted"), this->_statusTls);
        form->addRow(make_label("Members", "muted"), this->_statusMembers);
        status->addLayout(form);
        status->addStretch();
        joinedLayout->addWidget(codeCard, 3);
        joinedLayout->addWidget(statusCard, 2);
    }
    this->_aloneTop = alone;
    this->_joinedTop = joined;
    layout->addWidget(alone);
    layout->addWidget(joined); // only one of the two shown (refreshNetwork_): a hidden one takes no room

    // Who can reach this device: the paths of the exposure, chosen like in xdesk (the LAN is always open)
    {
        QFrame* reach = make_card();
        QVBoxLayout* reachLayout = new QVBoxLayout(reach);
        QHBoxLayout* reachHead = new QHBoxLayout();

        reachLayout->setContentsMargins(18, 14, 18, 8);
        reachLayout->setSpacing(6);
        this->_exposure = new QComboBox();
        this->_exposure->addItem("LAN only", "lan");
        this->_exposure->addItem("LAN + Tailscale", "tailscale");
        this->_exposure->addItem("LAN + internet (UPnP router port)", "upnp");
        this->_exposure->addItem("LAN + internet (bore.pub tunnel)", "bore");
        this->_exposure->addItem("LAN + internet (ngrok tunnel)", "ngrok");
        this->_exposure->addItem("LAN + internet (port forwarded by hand)", "manual");
        this->_exposure->setItemData(0, "Only the devices on the same network", Qt::ToolTipRole);
        this->_exposure->setItemData(1, "Also your devices signed in to the same tailnet (WireGuard)", Qt::ToolTipRole);
        this->_exposure->setItemData(2, "Also the devices on the internet: xmouse asks the router to open the port", Qt::ToolTipRole);
        this->_exposure->setItemData(3, "Also the devices on the internet, through bore.pub (TLS stays end to end)", Qt::ToolTipRole);
        this->_exposure->setItemData(4, "Also the devices on the internet, through ngrok (TLS stays end to end, account needed)", Qt::ToolTipRole);
        this->_exposure->setItemData(5, "Also the devices on the internet, through the port you forwarded on the router (public address in the settings)", Qt::ToolTipRole);
        this->_exposure->setMinimumWidth(250);
        reachHead->addWidget(make_label("Who can reach this device", "subtitle"));
        reachHead->addStretch();
        reachHead->addWidget(make_label("Open to", "muted"));
        reachHead->addWidget(this->_exposure);
        reachLayout->addLayout(reachHead);
        this->_exposureStatus = make_label("", "muted");
        reachLayout->addWidget(this->_exposureStatus);
        this->_endpointList = new QWidget(reach);
        new QVBoxLayout(this->_endpointList);
        this->_endpointList->layout()->setContentsMargins(0, 0, 0, 0);
        this->_endpointList->layout()->setSpacing(0);
        reachLayout->addWidget(this->_endpointList);
        QObject::connect(this->_exposure, &QComboBox::activated, this, [this](const int) {
            this->_hub.setExposure(core::exposure_from(this->_exposure->currentData().toString()));
            this->refreshEndpoints_();
        });
        QObject::connect(&this->_hub.exposure(), &net::Exposure::changed, this, &MainWindow::refreshEndpoints_);
        layout->addWidget(reach);
    }

    // Members of the network
    this->_membersCard = make_panel("Members", body);
    this->_members = new QTableWidget(0, 7);
    this->_members->setHorizontalHeaderLabels({"Device", "System", "Link", "Latency", "Address", "Allowed here", ""});
    this->_members->verticalHeader()->hide();
    this->_members->setSelectionMode(QAbstractItemView::NoSelection);
    this->_members->setEditTriggers(QAbstractItemView::NoEditTriggers);
    this->_members->horizontalHeader()->setStretchLastSection(false);
    this->_members->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    this->_members->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    this->_members->setMinimumHeight(200);
    Theme::decorate(this->_members);
    body->addWidget(this->_members);
    layout->addWidget(this->_membersCard);

    // Phones and web pads: they drive the cursor of this device (touchpad, keyboard), a phone also lends its screen
    {
        QHBoxLayout* row = new QHBoxLayout();
        QPushButton* add = make_button("Pair a phone or a browser", "primary", "qr");

        this->_padsCard = make_panel("Phones and web pads", body);
        body->addWidget(make_label("A phone (the xmouse app, or its browser) or any browser becomes a touchpad and a keyboard for this device; the Android app also puts its screen in the layout, the cursors of the network go on it. Scan the QR code with it, then accept it here.", "muted"));
        this->_pads = new QTableWidget(0, 4);
        this->_pads->setHorizontalHeaderLabels({"Pad", "Kind", "Now", ""});
        this->_pads->verticalHeader()->hide();
        this->_pads->setSelectionMode(QAbstractItemView::NoSelection);
        this->_pads->setEditTriggers(QAbstractItemView::NoEditTriggers);
        this->_pads->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        this->_pads->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        this->_pads->setMinimumHeight(110);
        Theme::decorate(this->_pads);
        body->addWidget(this->_pads);
        row->addWidget(add);
        row->addStretch();
        body->addLayout(row);
        QObject::connect(add, &QPushButton::clicked, this, &MainWindow::showPadQr_);
        layout->addWidget(this->_padsCard);
    }

    // Connect to a network (the part of xmouse): the address of a member and its code, or its link / QR code
    {
        QFrame* connect = make_panel("Connect to a network", body);
        QHBoxLayout* row = new QHBoxLayout();
        QPushButton* join = make_button("Join", "primary", "control");

        this->_connectTitle = connect->findChildren<QLabel*>().first();
        this->_joinAddress = new QLineEdit();
        this->_joinCode = new QLineEdit();
        this->_joinAddress->setPlaceholderText("address[:port] of a member, or its https:// link");
        this->_joinCode->setPlaceholderText("K7F-29Q-XM4");
        this->_joinCode->setObjectName("codeInput");
        this->_joinCode->setMaximumWidth(170);
        row->addWidget(this->_joinAddress, 1);
        row->addWidget(this->_joinCode);
        row->addWidget(join);
        body->addWidget(make_label("The address and the code are on the Network page of any member (Connection code, Who can reach this device). The link of its QR code fills both.", "muted"));
        body->addLayout(row);
        this->_joinStatus = make_label("", "muted");
        body->addWidget(this->_joinStatus);
        this->_nearby = new QListWidget();
        this->_nearby->setMaximumHeight(120);
        body->addWidget(make_label("Devices running xmouse on this LAN (click to fill the address)", "muted"));
        body->addWidget(this->_nearby);
        QObject::connect(join, &QPushButton::clicked, this, &MainWindow::joinFromFields_);
        QObject::connect(this->_joinCode, &QLineEdit::returnPressed, join, &QPushButton::click);
        QObject::connect(this->_joinAddress, &QLineEdit::textChanged, this, [this](const QString& text) {
            const QString code = QUrlQuery(QUrl(text.trimmed())).queryItemValue("code");
            if ((text.trimmed().startsWith("https://") || text.trimmed().startsWith("xmouse://")) && !code.isEmpty()) this->_joinCode->setText(code); // a pasted link brings its code
        });
        QObject::connect(this->_nearby, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {this->_joinAddress->setText(item->data(Qt::UserRole).toString());});
        layout->addWidget(connect);
    }
    layout->addStretch();
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
    actions->addSpacing(8);
    actions->addWidget(make_label("Profile", "muted"));
    this->_profiles = new QComboBox();
    this->_profiles->setMinimumWidth(150);
    this->_profiles->setToolTip("Saved layouts of the network (desk, travel...): applied for every member");
    actions->addWidget(this->_profiles);
    {
        QPushButton* apply = make_button("Apply", "ghost");
        QPushButton* save = make_button("Save as...", "ghost");
        QPushButton* drop = make_button("Delete", "ghost");
        actions->addWidget(apply);
        actions->addWidget(save);
        actions->addWidget(drop);
        QObject::connect(apply, &QPushButton::clicked, this, [this]() {if (!this->_profiles->currentText().isEmpty()) this->_hub.applyProfile(this->_profiles->currentText());});
        QObject::connect(drop, &QPushButton::clicked, this, [this]() {if (!this->_profiles->currentText().isEmpty()) this->_hub.deleteProfile(this->_profiles->currentText());});
        QObject::connect(save, &QPushButton::clicked, this, [this]() {
            bool ok = false;
            const QString name = QInputDialog::getText(this, "Save the layout", "Name of this layout (the same name replaces it)", QLineEdit::Normal, this->_profiles->currentText(), &ok);
            if (ok && !name.trimmed().isEmpty()) this->_hub.saveProfile(name);
        });
    }
    actions->addStretch();
    body->addLayout(actions);
    {
        QHBoxLayout* presence = new QHBoxLayout();
        QPushButton* ping = make_button("Look here", "ghost", "control");
        ping->setToolTip("A wave where your cursor is, on its screen and on every map");
        this->_laser = make_button("Laser", "ghost");
        this->_laser->setCheckable(true);
        this->_laser->setToolTip("Your cursor leaves a red trail on the screens it crosses");
        this->_follow = new QComboBox();
        this->_follow->setToolTip("An arrow at the border of your screen points at the cursor of this member");
        presence->addWidget(ping);
        presence->addWidget(this->_laser);
        presence->addSpacing(8);
        presence->addWidget(make_label("Follow", "muted"));
        presence->addWidget(this->_follow);
        presence->addStretch();
        this->_windowMove = new QCheckBox("Windows can move between devices (whole network)");
        this->_windowMove->setToolTip("On: a window dragged on an edge goes to the device on the other side (streamed there, driven from there; its document can be opened there). Off: the edge behaves as usual. Right click a device on the map to show a window there.");
        presence->addWidget(this->_windowMove);
        body->addLayout(presence);
        body->addWidget(make_label("Right click a device: follow its cursor, send it files or a link, show one of your windows there. Drop files on a device to send them.", "muted"));
        QObject::connect(ping, &QPushButton::clicked, &this->_hub, &core::Hub::ping);
        QObject::connect(this->_laser, &QPushButton::toggled, &this->_hub, &core::Hub::setLaser);
        QObject::connect(this->_follow, &QComboBox::activated, this, [this](const int index) {this->_hub.follow(this->_follow->itemData(index).toString());});
    }
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
    layout->addWidget(make_page_head("Activity", "Links, joins, cursors entering this device, access requests to the panel, files (also in " + core::Config::logDir() + ")."));
    {
        QHBoxLayout* row = new QHBoxLayout();
        QPushButton* cancel = make_button("Cancel the selected transfer", "ghost");
        QPushButton* folder = make_button("Open the received files", "ghost");

        card = make_panel("Files", body);
        this->_transfers = new QListWidget();
        this->_transfers->setMaximumHeight(140);
        body->addWidget(this->_transfers);
        row->addWidget(cancel);
        row->addWidget(folder);
        row->addStretch();
        body->addLayout(row);
        layout->addWidget(card);
        QObject::connect(cancel, &QPushButton::clicked, this, [this]() {
            if (this->_transfers->currentItem()) this->_hub.cancelTransfer(this->_transfers->currentItem()->data(Qt::UserRole).toString());
        });
        QObject::connect(folder, &QPushButton::clicked, this, [this]() {QDesktopServices::openUrl(QUrl::fromLocalFile(this->_hub.downloadDir()));});
    }
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
    QLineEdit* publicHost = new QLineEdit(s.publicHost);
    publicHost->setPlaceholderText("public address (port forwarded by hand)");
    form->addRow("Public address", publicHost);
    body->addLayout(form);
    QObject::connect(this->_deviceName, &QLineEdit::editingFinished, this, [this]() {this->_hub.renameDevice(this->_deviceName->text());});
    QObject::connect(port, &QSpinBox::valueChanged, this, [this, save](const int v) {this->_hub.config().settings().port = static_cast<std::uint16_t>(v); save();});
    QObject::connect(publicHost, &QLineEdit::editingFinished, this, [this, publicHost, save]() {this->_hub.config().settings().publicHost = publicHost->text().trimmed(); save();});
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
    form = new QFormLayout();
    {
        QComboBox* mode = new QComboBox();
        mode->addItem("Move the real pointer (like a second mouse)", "pointer");
        mode->addItem("Phantom cursors drawn over the apps (Canva-like)", "phantom");
        mode->setCurrentIndex(std::max(0, mode->findData(s.foreignMode)));
        mode->setToolTip("Pointer: the cursor of a member moves the pointer of this device. Phantom: it is drawn with its name over the apps, your pointer stays yours (it only jumps for one of its clicks, then comes back)");
        form->addRow("The cursors of the others here", mode);
        QObject::connect(mode, &QComboBox::currentIndexChanged, this, [this, mode, save](const int) {this->_hub.config().settings().foreignMode = mode->currentData().toString(); save(); this->_overlay.refresh();});
    }
    body->addLayout(form);
    body->addWidget(check("Phantom mode: the real pointer goes back after a foreign click (X11, Windows, GNOME extension)", s.restorePointer, ""));
    body->addWidget(make_label("Ctrl + Alt + Shift brings the cursor of this device back at once.", "muted"));
    layout->addWidget(card);

    card = make_panel("Network", body);
    body->addWidget(check("Moves and presence by encrypted udp when it passes (tcp otherwise)", s.udp, "AES-256-GCM with a key of the network; the late packets are dropped and the cursor is drawn ahead while one is missing (restart)"));
    body->addWidget(check("Carry the messages of two members that cannot reach each other (relay)", s.relay, "The members not linked directly reach each other through this device (still end to end in the network)"));
    layout->addWidget(card);

    card = make_panel("Files and links", body);
    form = new QFormLayout();
    {
        QSpinBox* clipLimit = new QSpinBox();
        QSpinBox* filesLimit = new QSpinBox();
        QLineEdit* folder = new QLineEdit(s.downloadDir);
        QComboBox* links = new QComboBox();
        clipLimit->setRange(0, 4096);
        clipLimit->setSuffix(" MiB");
        clipLimit->setValue(s.clipboardFilesLimit);
        filesLimit->setRange(1, 1024 * 1024);
        filesLimit->setSuffix(" MiB");
        filesLimit->setValue(s.filesLimit);
        folder->setPlaceholderText(this->_hub.downloadDir());
        links->addItem("Ask what to do", "ask");
        links->addItem("Open them at once", "open");
        links->addItem("Refuse them", "never");
        links->setCurrentIndex(std::max(0, links->findData(s.links)));
        form->addRow("Copied files fetched at once up to", clipLimit);
        form->addRow("Biggest transfer accepted", filesLimit);
        form->addRow("Received files go to", folder);
        form->addRow("Links sent by the members", links);
        QObject::connect(clipLimit, &QSpinBox::valueChanged, this, [this, save](const int v) {this->_hub.config().settings().clipboardFilesLimit = v; save();});
        QObject::connect(filesLimit, &QSpinBox::valueChanged, this, [this, save](const int v) {this->_hub.config().settings().filesLimit = v; save();});
        QObject::connect(folder, &QLineEdit::editingFinished, this, [this, folder, save]() {this->_hub.config().settings().downloadDir = folder->text().trimmed(); save();});
        QObject::connect(links, &QComboBox::currentIndexChanged, this, [this, links, save](const int) {this->_hub.config().settings().links = links->currentData().toString(); save();});
    }
    body->addLayout(form);
    layout->addWidget(card);

    card = make_panel("Phones, pads and windows", body);
    body->addWidget(check("A phone or a browser may pair with the code and drive this device", s.pads, "The page /pad of this device and the Android app; each new one is accepted here"));
    form = new QFormLayout();
    {
        QSpinBox* fps = new QSpinBox();
        fps->setRange(1, 30);
        fps->setSuffix(" frames / s");
        fps->setValue(s.windowFps);
        form->addRow("A window streamed from here", fps);
        QObject::connect(fps, &QSpinBox::valueChanged, this, [this, save](const int v) {this->_hub.config().settings().windowFps = v; save();});
    }
    body->addLayout(form);
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
    menu->addAction("Look here (wave)", &this->_hub, &core::Hub::ping);
    {
        QAction* laser = menu->addAction("Laser pointer");
        laser->setCheckable(true);
        QObject::connect(laser, &QAction::toggled, this->_laser, &QPushButton::setChecked);
        QObject::connect(this->_laser, &QPushButton::toggled, laser, &QAction::setChecked);
    }
    menu->addAction("Send files to...", this, &MainWindow::sendFilesTo_);
    menu->addAction("Send a link to...", this, &MainWindow::sendLinkTo_);
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

    this->_aloneTop->setVisible(!joined);
    this->_joinedTop->setVisible(joined);
    this->_membersCard->setVisible(joined);
    this->_connectTitle->setText(joined ? "CONNECT TO ANOTHER NETWORK" : "CONNECT TO A NETWORK");
    this->_identity->setText("This device is <b>" + this->_hub.config().deviceName().toHtmlEscaped() + "</b>" + (joined ? ", in the network <b>" + name.toHtmlEscaped() + "</b>" : QString(", in no network yet: create one, or connect to the network of another device")));
    this->_leave->setText(joined ? "Leave the network" : "Create or join");
    this->_leave->setIcon(Theme::icon("power", joined ? QColor(Qt::white) : Theme::palette().accentText));
    set_prop(this->_leave, "on", joined); // the #share style: true = red, false = green
    this->_windowMove->blockSignals(true);
    this->_windowMove->setChecked(this->_hub.state().value("net/windowMove").toBool(true));
    this->_windowMove->setEnabled(joined);
    this->_windowMove->blockSignals(false);
    this->refreshCode_();
    this->refreshStatus_();
    this->refreshEndpoints_();
    this->refreshMembers_();
    this->refreshNearby_();
}

void xmouse::ui::gui::MainWindow::refreshCode_(void)
{
    const int left = this->_hub.code().secondsLeft();
    const int lifetime = std::max(1, this->_hub.config().settings().codeLifetime);

    this->_code->setText(this->_hub.code().display());
    this->_codeBar->setVisible(left >= 0);
    this->_codeBar->setRange(0, lifetime);
    this->_codeBar->setValue(std::max(0, left));
    this->_codeTimer->setText(left >= 0 ? "Changes in " + QString::number(left / 60) + ":" + QString::number(left % 60).rightJustified(2, '0') + ", and after each use" : "Changes after each use");
    this->_fingerprint->setText("Device identity " + security::fingerprint_short(this->_hub.fingerprint()) + "  (the members pin it, the QR code carries it)");
}

void xmouse::ui::gui::MainWindow::refreshStatus_(void)
{
    const QList<core::Member> members = this->_hub.members();
    int online = 0;

    for (const core::Member& m: members)
        online += !m.self && m.online;
    this->_statusEdges->setText(this->_hub.captureName() + (this->_hub.captureError().isEmpty() ? "" : " - " + this->_hub.captureError()));
    set_tone(this->_statusEdges, this->_hub.captureName() == "none" || !this->_hub.captureError().isEmpty() ? "warning" : "success");
    this->_statusInput->setText(this->_hub.injectorError().isEmpty() ? this->_hub.injectorName() : "unavailable - " + this->_hub.injectorError());
    set_tone(this->_statusInput, this->_hub.injectorError().isEmpty() ? "success" : "warning");
    this->_statusMembers->setText(members.size() <= 1 ? QString("only this device: add one with the code") : QString::number(online) + " online of " + QString::number(members.size() - 1));
    set_tone(this->_statusMembers, online > 0 ? "success" : "muted");
}

void xmouse::ui::gui::MainWindow::refreshEndpoints_(void)
{
    const QList<net::Endpoint>& endpoints = this->_hub.exposure().endpoints();
    const bool joined = this->_hub.group().joined();
    QLayout* layout = this->_endpointList->layout();
    QString key = joined ? "on" : "off";

    this->_exposure->setCurrentIndex(std::max(0, this->_exposure->findData(core::exposure_name(this->_hub.config().settings().exposure))));
    // No network: every path is closed (like xdesk with the sharing off), the choice above applies once in one
    this->_exposureStatus->setText(joined ? this->_hub.exposure().status() + "." : QString("No network: every path is closed. The choice above applies once this device creates or joins a network."));

    // Rebuilt only when an address changes (refreshed every second)
    if (joined)
        for (const net::Endpoint& e: endpoints)
            key += "\n" + e.label + "|" + e.host + "|" + QString::number(e.port) + "|" + e.reach + "|" + e.ip;
    if (key == this->_endpointKey) return;
    this->_endpointKey = key;
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    if (!joined) return; // nothing to reach yet
    if (endpoints.isEmpty()) {
        layout->addWidget(make_label("Looking for the addresses of this device...", "muted"));
        return;
    }
    for (int i = 0; i < endpoints.size(); ++i) {
        const net::Endpoint& e = endpoints[i];
        const QString urlHost = QUrl(e.url).host().isEmpty() ? e.host : QUrl(e.url).host(); // the tailnet name only with its certificate (HSTS), else the address
        const QString address = "https://" + urlHost + ":" + QString::number(e.port);
        const QString kind = e.label == "Tailscale" ? "tailscale" : (e.label == "LAN" ? "lan" : (e.label == "This PC" ? "local" : "internet"));
        QFrame* row = new QFrame(this->_endpointList);
        QHBoxLayout* rowLayout = new QHBoxLayout(row);
        QVBoxLayout* texts = new QVBoxLayout();
        QLabel* tag = make_label(e.label == "This PC" ? QString("THIS DEVICE") : e.label.toUpper(), "tag");
        QLabel* reach = make_label(e.reach + (e.ip.isEmpty() ? QString() : "  ·  IP " + e.ip), "muted");
        QPushButton* copy = make_button("", "ghost", "copy");
        QPushButton* qr = make_button("", "ghost", "qr");

        // <tag> <address / who can use it> <copy> <qr>
        row->setObjectName("endpoint");
        row->setProperty("last", i == endpoints.size() - 1);
        row->setAttribute(Qt::WA_StyledBackground, true);
        rowLayout->setContentsMargins(0, 10, 0, 10);
        rowLayout->setSpacing(14);
        tag->setProperty("kind", kind);
        tag->setAlignment(Qt::AlignCenter);
        tag->setFixedWidth(104);
        texts->setSpacing(3);
        texts->addWidget(make_label(address, "address"));
        texts->addWidget(reach);
        copy->setToolTip("Copy " + address + " (Connect to a network on the other device, or a browser)");
        qr->setToolTip("QR code of this address (with the code)");
        qr->setVisible(kind != "local"); // "This device" only: no QR code for another device
        rowLayout->addWidget(tag, 0, Qt::AlignVCenter);
        rowLayout->addLayout(texts, 1);
        rowLayout->addWidget(copy);
        rowLayout->addWidget(qr);
        QObject::connect(copy, &QPushButton::clicked, this, [copy, text = address]() {
            QGuiApplication::clipboard()->setText(text);
            copy->setText("Copied");
            QTimer::singleShot(1500, copy, [copy]() {copy->setText("");});
        });
        QObject::connect(qr, &QPushButton::clicked, this, [this, i]() {this->showQr_(i);});
        layout->addWidget(row);
    }
}

void xmouse::ui::gui::MainWindow::showQr_(const int endpoint)
{
    const QList<net::Endpoint> endpoints = this->_hub.exposure().endpoints();
    QDialog* dialog = nullptr;
    QVBoxLayout* layout = nullptr;
    QComboBox* address = nullptr;
    QLabel* image = nullptr;
    QLabel* link = nullptr;
    QPushButton* copy = nullptr;
    QPushButton* close = nullptr;
    QHBoxLayout* buttons = nullptr;
    std::function<void(void)> draw;
    int first = -1;

    if (!this->_hub.group().joined()) {
        QMessageBox::information(this, "QR code", "Create a network first: the QR code holds an address of this device and the code of the network.");
        return;
    }
    dialog = new QDialog(this);
    layout = new QVBoxLayout(dialog);
    address = new QComboBox(dialog);
    image = new QLabel(dialog);
    link = make_label("", "mono");
    copy = make_button("Copy the link", "ghost", "copy");
    close = make_button("Close", "ghost");
    buttons = new QHBoxLayout();
    dialog->setWindowTitle("QR code - xmouse");
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setFixedWidth(440);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(10);
    layout->addWidget(make_label("Join this network", "title"));
    layout->addWidget(make_label("Scan it with the phone app, or paste its link in Connect to a network on the other device: the address, the code and the identity of this device are inside.", "muted"));

    // One entry per address another device can use (the chosen one, else the first that is not this device only)
    for (int i = 0; i < endpoints.size(); ++i) {
        if (endpoints[i].label == "This PC") continue;
        if (i == endpoint || (endpoint < 0 && first < 0)) first = address->count();
        address->addItem(endpoints[i].label + "  ·  " + (QUrl(endpoints[i].url).host().isEmpty() ? endpoints[i].host : QUrl(endpoints[i].url).host()) + ":" + QString::number(endpoints[i].port), i);
    }
    address->setCurrentIndex(std::max(0, first));
    layout->addWidget(address);
    image->setObjectName("qr");
    image->setAlignment(Qt::AlignCenter);
    layout->addWidget(image, 0, Qt::AlignHCenter);
    link->setWordWrap(true);
    link->setAlignment(Qt::AlignCenter);
    layout->addWidget(link);
    buttons->addWidget(copy);
    buttons->addStretch();
    buttons->addWidget(close);
    layout->addLayout(buttons);
    draw = [this, address, image, link, endpoints]() {
        const int index = address->currentData().toInt();
        const QString host = index >= 0 && index < endpoints.size() ? (QUrl(endpoints[index].url).host().isEmpty() ? endpoints[index].host : QUrl(endpoints[index].url).host()) : QString();
        const QString text = host.isEmpty() ? this->_hub.joinLink() : this->_hub.joinLink(host, endpoints[index].port);
        image->setPixmap(qr_pixmap(text, 300));
        link->setText(text.section('?', 0, 0) + "  ·  code " + this->_hub.code().display()); // the identity is too long to show: in the tooltip and the copy
        link->setToolTip(text);
        link->setProperty("full", text);
    };
    draw();
    QObject::connect(address, &QComboBox::currentIndexChanged, dialog, [draw](const int) {draw();});
    QObject::connect(&this->_hub.code(), &security::PairingCode::changed, dialog, [draw](const QString&) {draw();}); // a used / expired code: a new QR
    QObject::connect(copy, &QPushButton::clicked, dialog, [link, copy]() {
        QGuiApplication::clipboard()->setText(link->property("full").toString());
        copy->setText("Copied");
        QTimer::singleShot(1500, copy, [copy]() {copy->setText("Copy the link");});
    });
    QObject::connect(close, &QPushButton::clicked, dialog, &QDialog::close);
    dialog->show();
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
        this->_members->setItem(row, 2, new QTableWidgetItem(m.self ? "-" : (!m.online ? "offline" : (!m.via.isEmpty() ? m.kind + " of " + this->_hub.layout().device(m.via).name : (m.relayed ? "relayed" : (m.udp ? "online, udp" : "online"))))));
        this->_members->setItem(row, 3, new QTableWidgetItem(m.self || m.rtt < 0 ? "-" : QString::number(m.rtt) + " ms"));
        this->_members->setItem(row, 4, new QTableWidgetItem(m.self ? "" : this->_hub.mesh().addressesOf(m.id).join(", ")));
        rightsLayout->setContentsMargins(4, 0, 4, 0);
        rightsLayout->setSpacing(8);
        if (!m.self && m.via.isEmpty()) { // a phone / pad drives its own device: no rights to give it here
            const std::array<std::pair<QString, int>, 6> boxes = {std::pair<QString, int>{"Mouse", 0}, {"Keys", 1}, {"Clipboard", 2}, {"Files", 3}, {"Links", 4}, {"Windows", 5}};
            for (const auto& [label, index]: boxes) {
                QCheckBox* box = new QCheckBox(label);
                const bool values[] = {r.mouse, r.keyboard, r.clipboard, r.files, r.links, r.windows};
                box->setChecked(values[index]);
                rightsLayout->addWidget(box);
                QObject::connect(box, &QCheckBox::toggled, this, [this, id = m.id, index](const bool on) {
                    core::Rights rr = this->_hub.config().rightsOf(id);
                    if (index == 0) rr.mouse = on;
                    else if (index == 1) rr.keyboard = on;
                    else if (index == 2) rr.clipboard = on;
                    else if (index == 3) rr.files = on;
                    else if (index == 4) rr.links = on;
                    else rr.windows = on;
                    this->_hub.setRights(id, rr);
                });
            }
        }
        this->_members->setCellWidget(row, 5, rights);
        if (!m.self) {
            QPushButton* remove = make_button("Remove", "ghost");
            const bool padOfThis = m.via == this->_hub.group().self();
            if (!m.via.isEmpty() && !padOfThis) remove->setEnabled(false); // the pad of another device: removed there
            QObject::connect(remove, &QPushButton::clicked, this, [this, id = m.id, name = m.name, padOfThis]() {
                const QString text = padOfThis ? name + " can no longer drive this device (it pairs again with a new code)." : name + " leaves the network: the key of the network changes, it needs a new code to come back.";
                if (QMessageBox::question(this, "Remove " + name, text) == QMessageBox::Yes) this->_hub.removeMember(id);
            });
            this->_members->setCellWidget(row, 6, remove);
        } else this->_members->setCellWidget(row, 6, nullptr);
        set_tone(this->_status, "muted");
    }
    this->refreshWhere_();
    this->refreshBackends_();
    this->refreshFollow_();
}

void xmouse::ui::gui::MainWindow::refreshNearby_(void)
{
    this->_nearby->clear();
    for (const net::Announce& a: this->_hub.nearby()) {
        QListWidgetItem* item = new QListWidgetItem(a.name + "  -  " + a.host + ":" + QString::number(a.port) + (a.network.isEmpty() ? "  (no network)" : "  (network " + a.network + ")"));
        item->setData(Qt::UserRole, a.host + ":" + QString::number(a.port));
        this->_nearby->addItem(item);
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

void xmouse::ui::gui::MainWindow::refreshPads_(void)
{
    const QList<core::Pad> pads = this->_hub.pads();

    if (!this->_pads) return;
    this->_pads->setRowCount(static_cast<int>(pads.size()));
    for (int row = 0; row < pads.size(); ++row) {
        const core::Pad& pad = pads[row];
        QPushButton* remove = make_button("Forget", "ghost");
        QString now = pad.control || pad.display ? "connected" : "offline";

        if (pad.display && pad.screenOn) now += ", screen " + QString::number(pad.screen.width()) + " x " + QString::number(pad.screen.height()) + " in the layout";
        this->_pads->setItem(row, 0, new QTableWidgetItem(pad.name));
        this->_pads->setItem(row, 1, new QTableWidgetItem(pad.kind == "phone" ? "phone (app)" : "browser"));
        this->_pads->setItem(row, 2, new QTableWidgetItem(now));
        QObject::connect(remove, &QPushButton::clicked, this, [this, id = pad.id, name = pad.name]() {
            if (QMessageBox::question(this, "Forget " + name, name + " can no longer drive this device (it pairs again with a new code).") == QMessageBox::Yes) this->_hub.removePad(id);
        });
        this->_pads->setCellWidget(row, 3, remove);
    }
}

void xmouse::ui::gui::MainWindow::refreshProfiles_(void)
{
    const QString current = this->_profiles ? this->_profiles->currentText() : QString();
    const QStringList names = this->_hub.profiles();

    if (!this->_profiles) return;
    this->_profiles->blockSignals(true);
    this->_profiles->clear();
    this->_profiles->addItems(names);
    if (names.contains(current)) this->_profiles->setCurrentText(current);
    this->_profiles->setEnabled(!names.isEmpty());
    this->_profiles->blockSignals(false);
}

void xmouse::ui::gui::MainWindow::refreshFollow_(void)
{
    if (!this->_follow) return;
    this->_follow->blockSignals(true);
    this->_follow->clear();
    this->_follow->addItem("Nobody", QString());
    for (const core::Member& m: this->_hub.members())
        if (!m.self && m.online) this->_follow->addItem(QIcon(color_dot(m.color)), m.name, m.id);
    this->_follow->setCurrentIndex(std::max(0, this->_follow->findData(this->_hub.following())));
    this->_follow->blockSignals(false);
}

void xmouse::ui::gui::MainWindow::refreshTransfers_(void)
{
    if (!this->_transfers) return;
    this->_transfers->clear();
    for (const core::Transfer& t: this->_hub.transfers()) {
        const qint64 percent = t.total > 0 ? t.done * 100 / t.total : 100;
        QListWidgetItem* item = new QListWidgetItem(QString(t.incoming ? "From " : "To ") + t.peerName + ":  " + QString::number(t.files.size()) + " file(s), " + QString::number(percent) + "%" + (t.files.size() == 1 ? "  (" + t.files.front().name + ")" : QString()));
        item->setData(Qt::UserRole, t.id);
        this->_transfers->addItem(item);
    }
    if (this->_transfers->count() == 0) this->_transfers->addItem("No file on the way. Drop files on a device of the Layout page, copy files (they follow the clipboard), or drag one to an edge.");
}

void xmouse::ui::gui::MainWindow::showPadQr_(void)
{
    QDialog* dialog = new QDialog(this);
    QVBoxLayout* layout = new QVBoxLayout(dialog);
    QLabel* image = new QLabel(dialog);
    QLabel* link = make_label("", "mono");
    QPushButton* copy = make_button("Copy the link", "ghost", "copy");
    QPushButton* close = make_button("Close", "ghost");
    QHBoxLayout* buttons = new QHBoxLayout();
    const std::function<void(void)> draw = [this, image, link]() {
        const QString text = this->_hub.padLink();
        image->setPixmap(qr_pixmap(text, 300));
        link->setText(text.section('?', 0, 0) + "  ·  code " + this->_hub.code().display());
        link->setToolTip(text);
        link->setProperty("full", text);
    };

    if (!this->_hub.config().settings().pads) {QMessageBox::information(this, "Phones and pads", "The pads are refused by the settings of this device (Settings > Phones, pads and windows)."); return;}
    dialog->setWindowTitle("Pair a phone or a browser - xmouse");
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setFixedWidth(440);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(10);
    layout->addWidget(make_label("Drive this device from a phone", "title"));
    layout->addWidget(make_label("Scan it with the camera of the phone (or the xmouse app): the page of this device opens as a touchpad and a keyboard. Accept it here once.", "muted"));
    image->setObjectName("qr");
    image->setAlignment(Qt::AlignCenter);
    layout->addWidget(image, 0, Qt::AlignHCenter);
    link->setWordWrap(true);
    link->setAlignment(Qt::AlignCenter);
    layout->addWidget(link);
    buttons->addWidget(copy);
    buttons->addStretch();
    buttons->addWidget(close);
    layout->addLayout(buttons);
    draw();
    QObject::connect(&this->_hub.code(), &security::PairingCode::changed, dialog, [draw](const QString&) {draw();});
    QObject::connect(copy, &QPushButton::clicked, dialog, [link, copy]() {
        QGuiApplication::clipboard()->setText(link->property("full").toString());
        copy->setText("Copied");
        QTimer::singleShot(1500, copy, [copy]() {copy->setText("Copy the link");});
    });
    QObject::connect(close, &QPushButton::clicked, dialog, &QDialog::close);
    dialog->show();
}

void xmouse::ui::gui::MainWindow::onPadApproval_(const QString& pending, const QString& name, const QString& kind)
{
    QMessageBox* box = new QMessageBox(QMessageBox::Question, "A pad wants to drive this device", "", QMessageBox::NoButton, this);

    box->setText("The " + kind.toHtmlEscaped() + " <b>" + name.toHtmlEscaped() + "</b> gave the right code and wants to drive the cursor and the keyboard of this device.<br><br>Refused by default in 30 s.");
    box->addButton("Accept", QMessageBox::AcceptRole);
    box->addButton("Refuse", QMessageBox::RejectRole);
    box->setAttribute(Qt::WA_DeleteOnClose);
    QObject::connect(box, &QMessageBox::buttonClicked, this, [this, box, pending](QAbstractButton* button) {
        this->_hub.approvePad(pending, box->buttonRole(button) == QMessageBox::AcceptRole);
    });
    QTimer::singleShot(30000, box, &QMessageBox::close);
    box->show();
    box->raise();
    if (this->_tray) this->_tray->showMessage("A pad wants to drive this device", name + " gave the right code", QSystemTrayIcon::Warning, 8000);
}

void xmouse::ui::gui::MainWindow::onLink_(const QString& id, const QString& name, const QString& url)
{
    QMessageBox* box = new QMessageBox(QMessageBox::Question, "Link from " + name, "", QMessageBox::NoButton, this);
    QPushButton* open = box->addButton("Open", QMessageBox::AcceptRole);
    QPushButton* copy = box->addButton("Copy", QMessageBox::ActionRole);

    (void)id;
    box->addButton("Ignore", QMessageBox::RejectRole);
    box->setText("<b>" + name.toHtmlEscaped() + "</b> sends you a link:<br><br><span style='font-family: monospace'>" + url.toHtmlEscaped() + "</span>");
    box->setAttribute(Qt::WA_DeleteOnClose);
    QObject::connect(box, &QMessageBox::buttonClicked, this, [open, copy, url](QAbstractButton* button) {
        if (button == open) QDesktopServices::openUrl(QUrl(url));
        else if (button == copy) QGuiApplication::clipboard()->setText(url);
    });
    box->show();
    box->raise();
    if (this->_tray) this->_tray->showMessage("Link from " + name, url, QSystemTrayIcon::Information, 6000);
}

void xmouse::ui::gui::MainWindow::sendFilesTo_(void)
{
    QStringList names;
    QStringList ids;
    QString chosen;
    QStringList files;
    bool ok = false;

    for (const core::Member& m: this->_hub.members())
        if (!m.self && m.online) {names.append(m.name); ids.append(m.id);}
    if (names.isEmpty()) {QMessageBox::information(this, "Send files", "No member online."); return;}
    chosen = QInputDialog::getItem(this, "Send files", "To", names, 0, false, &ok);
    if (!ok) return;
    files = QFileDialog::getOpenFileNames(this, "Send files to " + chosen);
    if (!files.isEmpty()) (void)this->_hub.sendFiles(ids[names.indexOf(chosen)], files);
}

void xmouse::ui::gui::MainWindow::sendLinkTo_(void)
{
    QStringList names;
    QStringList ids;
    QString chosen;
    QString url;
    bool ok = false;

    for (const core::Member& m: this->_hub.members())
        if (!m.self && m.online) {names.append(m.name); ids.append(m.id);}
    if (names.isEmpty()) {QMessageBox::information(this, "Send a link", "No member online."); return;}
    chosen = QInputDialog::getItem(this, "Send a link", "To", names, 0, false, &ok);
    if (!ok) return;
    url = QInputDialog::getText(this, "Send a link", "Link opened on " + chosen, QLineEdit::Normal, QGuiApplication::clipboard()->text().startsWith("http") ? QGuiApplication::clipboard()->text() : QString("https://"), &ok);
    if (ok && !url.trimmed().isEmpty()) this->_hub.sendLink(ids[names.indexOf(chosen)], url);
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
        if (top->property("xmouse_not_panel").toBool()) continue; // streamed windows, drop shelf: used like any app
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
    if (event->type() == QEvent::WindowActivate || event->type() == QEvent::WindowDeactivate) QTimer::singleShot(0, this, &MainWindow::reportPanel_);
    return QMainWindow::eventFilter(watched, event);
}

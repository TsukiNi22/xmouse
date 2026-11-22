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
##  @file MainWindow.hpp

File Description:
##  Panel of the device: network and pairing (code / QR), layout, activity, settings, tray
\**************************************************************/

#ifndef MAINWINDOW_H
    #define MAINWINDOW_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>          // _cold, _nodiscard
    #include "../../core/Hub.hpp"       // xmouse::core::Hub
    #include "WindowViewer.hpp"         // xmouse::ui::gui::WindowViewer
    #include "LayoutView.hpp"           // xmouse::ui::gui::LayoutView
    #include "DropShelf.hpp"            // xmouse::ui::gui::DropShelf
    #include "Overlay.hpp"              // xmouse::ui::gui::Overlay
    #include <QSystemTrayIcon>          // QSystemTrayIcon
    #include <QStackedWidget>           // QStackedWidget
    #include <QProgressBar>             // QProgressBar
    #include <QTableWidget>             // QTableWidget
    #include <QListWidget>              // QListWidget
    #include <QPushButton>              // QPushButton
    #include <QMainWindow>              // QMainWindow
    #include <QLineEdit>                // QLineEdit
    #include <QCheckBox>                // QCheckBox
    #include <QComboBox>                // QComboBox
    #include <algorithm>                // std::clamp
    #include <QSpinBox>                 // QSpinBox
    #include <QPointer>                 // QPointer
    #include <QLabel>                   // QLabel
    #include <QTimer>                   // QTimer
    #include <QHash>                    // QHash

namespace xmouse::ui::gui { // namespace start

//----------------------------------------------------------------//
/* CLASS */

class MainWindow final: public QMainWindow {
    Q_OBJECT

    private:
        core::Hub& _hub;
        Overlay _overlay;
        DropShelf _shelf;
        QHash<QString, QPointer<WindowViewer>> _viewers; // windows of the other devices streamed here
        QListWidget* _nav = nullptr;
        QStackedWidget* _pages = nullptr;
        QSystemTrayIcon* _tray = nullptr;
        QTimer _tick; // 1 s: code countdown, access countdown, latencies
        bool _quitting = false;

        /* banners */
        QWidget* _accessBanner = nullptr;
        QLabel* _accessText = nullptr;
        QPushButton* _accessAccept = nullptr; // asking: Accept / Refuse; granted: Revoke
        QPushButton* _accessRefuse = nullptr;
        QPushButton* _accessRevoke = nullptr;
        QString _accessId = ""; // member asking the panel (or given it)
        QWidget* _noticeBanner = nullptr;
        QLabel* _noticeText = nullptr;

        /* network page */
        QWidget* _aloneTop = nullptr; // no network: create one
        QWidget* _joinedTop = nullptr; // in a network: code + status
        QLabel* _identity = nullptr; // "This device is ..." under the title
        QLabel* _code = nullptr;
        QProgressBar* _codeBar = nullptr; // time left of the code
        QLabel* _codeTimer = nullptr;
        QLabel* _fingerprint = nullptr;
        QLabel* _statusEdges = nullptr;
        QLabel* _statusInput = nullptr;
        QLabel* _statusTls = nullptr;
        QLabel* _statusMembers = nullptr;
        QComboBox* _exposure = nullptr; // "Open to"
        QLabel* _exposureStatus = nullptr;
        QWidget* _endpointList = nullptr; // one row per address
        QString _endpointKey = ""; // the rows are rebuilt only when an address changes
        QWidget* _membersCard = nullptr;
        QTableWidget* _members = nullptr;
        QLabel* _connectTitle = nullptr;
        QListWidget* _nearby = nullptr;
        QLineEdit* _createName = nullptr;
        QLineEdit* _joinAddress = nullptr;
        QLineEdit* _joinCode = nullptr;
        QWidget* _padsCard = nullptr; // phones and web pads of this device
        QTableWidget* _pads = nullptr;
        QLabel* _joinStatus = nullptr;

        /* layout page */
        LayoutView* _layoutView = nullptr;
        QLabel* _where = nullptr; // where each cursor is
        QCheckBox* _windowMove = nullptr;
        QComboBox* _profiles = nullptr; // saved layouts
        QComboBox* _follow = nullptr; // member followed
        QPushButton* _laser = nullptr;

        /* activity page */
        QListWidget* _log = nullptr;
        QListWidget* _transfers = nullptr; // files being sent / received

        /* settings page */
        QLineEdit* _deviceName = nullptr;
        QCheckBox* _private = nullptr;
        QPushButton* _leave = nullptr; // leave the network (red) | create / join one (green)
        QLabel* _backends = nullptr;
        QLabel* _status = nullptr;

        // ---------- Pre-Function -------- //
        QWidget* buildNetwork_(void);
        QWidget* buildLayout_(void);
        QWidget* buildActivity_(void);
        QWidget* buildSettings_(void);
        void buildTray_(void);
        void refreshNetwork_(void);
        void refreshMembers_(void);
        void refreshNearby_(void);
        void refreshCode_(void);
        void refreshStatus_(void);
        void refreshEndpoints_(void);
        Q_INVOKABLE void showQr_(const int endpoint); // QR code of an address + the code (-1 = the first one another device can use)
        void refreshWhere_(void);
        void refreshBackends_(void);
        void refreshPads_(void);
        void refreshProfiles_(void);
        void refreshFollow_(void);
        void refreshTransfers_(void);
        void showPadQr_(void); // QR code of the link that pairs a phone / browser
        void onPadApproval_(const QString& pending, const QString& name, const QString& kind);
        void onLink_(const QString& id, const QString& name, const QString& url);
        void sendFilesTo_(void); // tray: pick a member, then files
        void sendLinkTo_(void); // tray: pick a member, then a link
        void onJoinApproval_(net::Peer* peer);
        void showNotice_(const QString& text, const QString& tone = "accent");
        void reportPanel_(void); // geometry + focus of the panel to the hub
        void joinFromFields_(void);

    protected:
        // ---------- Pre-Function -------- //
        void closeEvent(QCloseEvent* event) override;
        void changeEvent(QEvent* event) override;
        void moveEvent(QMoveEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;
        void showEvent(QShowEvent* event) override;
        void hideEvent(QHideEvent* event) override;
        bool eventFilter(QObject* watched, QEvent* event) override;

    public:
        // ------------ Function ---------- //
        /* setter */
        _cold inline void showPage(const int page) {this->_nav->setCurrentRow(std::clamp(page, 0, 3));}; // 0 network, 1 layout, 2 activity, 3 settings

        // ---------- Constructor --------- //
        explicit MainWindow(core::Hub& hub, QWidget* parent = nullptr);

        // ----------- Destructor --------- //
        ~MainWindow() override;
};

} // namespace end
#endif /* MAINWINDOW_H */

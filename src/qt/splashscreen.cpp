// Copyright (c) 2011-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bitcoin-build-config.h> // IWYU pragma: keep

#include <qt/splashscreen.h>

#include <clientversion.h>
#include <common/system.h>
#include <interfaces/handler.h>
#include <interfaces/node.h>
#include <interfaces/wallet.h>
#include <qt/guiutil.h>
#include <qt/networkstyle.h>
#include <qt/walletmodel.h>
#include <util/translation.h>

#include <functional>

#include <QApplication>
#include <QCloseEvent>
#include <QPainter>
#include <QRadialGradient>
#include <QScreen>


SplashScreen::SplashScreen(const NetworkStyle* networkStyle)
    : QWidget()
{
    // set reference point, paddings
    int paddingRight            = 42;
    int paddingTop              = 82;
    int titleVersionVSpace      = 27;
    int titleCopyrightVSpace    = 84;

    float fontFactor            = 1.0;
    float devicePixelRatio      = 1.0;
    devicePixelRatio = static_cast<QGuiApplication*>(QCoreApplication::instance())->devicePixelRatio();

    // define text to place
    QString titleText       = CLIENT_NAME;
    QString versionText     = QString("Version %1").arg(QString::fromStdString(FormatFullVersion()));
    QString copyrightText   = QString::fromUtf8(CopyrightHolders(strprintf("\xc2\xA9 %u-%u ", 2009, COPYRIGHT_YEAR)).c_str());
    const QString& titleAddText    = networkStyle->getTitleAddText();

    QString font            = QApplication::font().toString();

    // create a bitmap according to device pixelratio
    QSize splashSize(620*devicePixelRatio,360*devicePixelRatio);
    pixmap = QPixmap(splashSize);

    // change to HiDPI if it makes sense
    pixmap.setDevicePixelRatio(devicePixelRatio);

    QPainter pixPaint(&pixmap);
    pixPaint.setPen(QColor(33,27,20));

    // draw a slightly radial gradient
    QRadialGradient gradient(QPoint(90,60), splashSize.width()/devicePixelRatio);
    gradient.setColorAt(0, QColor(255,253,248));
    gradient.setColorAt(1, QColor(238,230,218));
    QRect rGradient(QPoint(0,0), splashSize);
    pixPaint.fillRect(rGradient, gradient);

    // draw the bitcoin icon, expected size of PNG: 1024x1024
    QRect rectIcon(QPoint(34,72), QSize(210,210));

    const QSize requiredSize(1024,1024);
    QPixmap icon(networkStyle->getAppIcon().pixmap(requiredSize));

    pixPaint.setOpacity(0.96);
    pixPaint.drawPixmap(rectIcon, icon);
    pixPaint.setOpacity(1.0);

    // A restrained gold rule separates the brand from the wallet details.
    pixPaint.fillRect(QRect(270, 54, 3, 244), QColor(184,135,46));

    // check font size and drawing with
    pixPaint.setFont(QFont(font, 31*fontFactor, QFont::DemiBold));
    QFontMetrics fm = pixPaint.fontMetrics();
    int titleTextWidth = GUIUtil::TextWidth(fm, titleText);
    if (titleTextWidth > 285) {
        fontFactor = fontFactor * 285 / titleTextWidth;
    }

    pixPaint.setFont(QFont(font, 31*fontFactor, QFont::DemiBold));
    fm = pixPaint.fontMetrics();
    titleTextWidth  = GUIUtil::TextWidth(fm, titleText);
    pixPaint.setPen(QColor(33,27,20));
    pixPaint.drawText(pixmap.width()/devicePixelRatio-titleTextWidth-paddingRight,paddingTop,titleText);

    pixPaint.setFont(QFont(font, 12*fontFactor));
    pixPaint.setPen(QColor(139,99,31));

    // if the version string is too long, reduce size
    fm = pixPaint.fontMetrics();
    int versionTextWidth  = GUIUtil::TextWidth(fm, versionText);
    if(versionTextWidth > titleTextWidth+paddingRight-10) {
        pixPaint.setFont(QFont(font, 10*fontFactor));
        titleVersionVSpace -= 5;
    }
    pixPaint.drawText(pixmap.width()/devicePixelRatio-titleTextWidth-paddingRight+2,paddingTop+titleVersionVSpace,versionText);

    pixPaint.setFont(QFont(font, 9*fontFactor, QFont::DemiBold));
    pixPaint.setPen(QColor(98,86,71));
    pixPaint.drawText(pixmap.width()/devicePixelRatio-titleTextWidth-paddingRight+2,
                      paddingTop+titleVersionVSpace+29,
                      QStringLiteral("SECURE DESKTOP WALLET  ·  LIVE MARKET DATA"));

    // draw copyright stuff
    {
        pixPaint.setFont(QFont(font, 9*fontFactor));
        pixPaint.setPen(QColor(92,82,69));
        const int x = pixmap.width()/devicePixelRatio-titleTextWidth-paddingRight;
        const int y = paddingTop+titleCopyrightVSpace;
        QRect copyrightRect(x, y, pixmap.width() - x - paddingRight, pixmap.height() - y);
        pixPaint.drawText(copyrightRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                          copyrightText + QStringLiteral("\nDesktop mining is disabled. Network sync remains active."));
    }

    // draw additional text if special network
    if(!titleAddText.isEmpty()) {
        QFont boldFont = QFont(font, 10*fontFactor);
        boldFont.setWeight(QFont::Bold);
        pixPaint.setFont(boldFont);
        fm = pixPaint.fontMetrics();
        int titleAddTextWidth  = GUIUtil::TextWidth(fm, titleAddText);
        pixPaint.setPen(QColor(184,135,46));
        pixPaint.drawText(pixmap.width()/devicePixelRatio-titleAddTextWidth-16,22,titleAddText);
    }

    pixPaint.end();

    // Set window title
    setWindowTitle(titleText + " " + titleAddText);

    // Resize window and move to center of desktop, disallow resizing
    QRect r(QPoint(), QSize(pixmap.size().width()/devicePixelRatio,pixmap.size().height()/devicePixelRatio));
    resize(r.size());
    setFixedSize(r.size());
    move(QGuiApplication::primaryScreen()->geometry().center() - r.center());

    installEventFilter(this);

    GUIUtil::handleCloseWindowShortcut(this);
}

SplashScreen::~SplashScreen()
{
    if (m_node) unsubscribeFromCoreSignals();
}

void SplashScreen::setNode(interfaces::Node& node)
{
    assert(!m_node);
    m_node = &node;
    subscribeToCoreSignals();
    if (m_shutdown) m_node->startShutdown();
}

void SplashScreen::shutdown()
{
    m_shutdown = true;
    if (m_node) m_node->startShutdown();
}

bool SplashScreen::eventFilter(QObject * obj, QEvent * ev) {
    if (ev->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(ev);
        if (keyEvent->key() == Qt::Key_Q) {
            shutdown();
        }
    }
    return QObject::eventFilter(obj, ev);
}

static void InitMessage(SplashScreen *splash, const std::string &message)
{
    bool invoked = QMetaObject::invokeMethod(splash, "showMessage",
        Qt::QueuedConnection,
        Q_ARG(QString, QString::fromStdString(message)),
        Q_ARG(int, Qt::AlignBottom|Qt::AlignHCenter),
        Q_ARG(QColor, QColor(72,60,45)));
    assert(invoked);
}

static void ShowProgress(SplashScreen *splash, const std::string &title, int nProgress, bool resume_possible)
{
    InitMessage(splash, title + std::string("\n") +
            (resume_possible ? SplashScreen::tr("(press q to shutdown and continue later)").toStdString()
                                : SplashScreen::tr("press q to shutdown").toStdString()) +
            strprintf("\n%d", nProgress) + "%");
}

void SplashScreen::subscribeToCoreSignals()
{
    // Connect signals to client
    m_handler_init_message = m_node->handleInitMessage([this](const std::string& message) {
        InitMessage(this, message);
    });
    m_handler_show_progress = m_node->handleShowProgress([this](const std::string& title, int nProgress, bool resume_possible) {
        ShowProgress(this, title, nProgress, resume_possible);
    });
    m_handler_init_wallet = m_node->handleInitWallet([this]() { handleLoadWallet(); });
}

void SplashScreen::handleLoadWallet()
{
#ifdef ENABLE_WALLET
    if (!WalletModel::isWalletEnabled()) return;
    m_handler_load_wallet = m_node->walletLoader().handleLoadWallet([this](std::unique_ptr<interfaces::Wallet> wallet) {
        m_connected_wallet_handlers.emplace_back(wallet->handleShowProgress([this](const std::string& title, int nProgress) {
            ShowProgress(this, title, nProgress, /*resume_possible=*/false);
        }));
        m_connected_wallets.emplace_back(std::move(wallet));
    });
#endif
}

void SplashScreen::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client
    m_handler_init_message->disconnect();
    m_handler_show_progress->disconnect();
    for (const auto& handler : m_connected_wallet_handlers) {
        handler->disconnect();
    }
    m_connected_wallet_handlers.clear();
    m_connected_wallets.clear();
}

void SplashScreen::showMessage(const QString &message, int alignment, const QColor &color)
{
    curMessage = message;
    curAlignment = alignment;
    curColor = color;
    update();
}

void SplashScreen::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.drawPixmap(0, 0, pixmap);
    QRect r = rect().adjusted(5, 5, -5, -5);
    painter.setPen(curColor);
    painter.drawText(r, curAlignment, curMessage);
}

void SplashScreen::closeEvent(QCloseEvent *event)
{
    shutdown(); // allows an "emergency" shutdown during startup
    event->ignore();
}

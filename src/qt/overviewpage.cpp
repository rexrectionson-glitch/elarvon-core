// Copyright (c) 2011-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/overviewpage.h>
#include <qt/forms/ui_overviewpage.h>

#include <qt/bitcoinunits.h>
#include <qt/clientmodel.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/transactionfilterproxy.h>
#include <qt/transactionoverviewwidget.h>
#include <qt/transactiontablemodel.h>
#include <qt/walletmodel.h>

#include <QAbstractItemDelegate>
#include <QApplication>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QSettings>
#include <QStatusTipEvent>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <map>

#define DECORATION_SIZE 54
#define NUM_ITEMS 5

Q_DECLARE_METATYPE(interfaces::WalletBalances)

class TxViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    explicit TxViewDelegate(const PlatformStyle* _platformStyle, QObject* parent = nullptr)
        : QAbstractItemDelegate(parent), platformStyle(_platformStyle)
    {
        connect(this, &TxViewDelegate::width_changed, this, &TxViewDelegate::sizeHintChanged);
    }

    inline void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const override
    {
        painter->save();

        QIcon icon = qvariant_cast<QIcon>(index.data(TransactionTableModel::RawDecorationRole));
        QRect mainRect = option.rect;
        QRect decorationRect(mainRect.topLeft(), QSize(DECORATION_SIZE, DECORATION_SIZE));
        int xspace = DECORATION_SIZE + 8;
        int ypad = 6;
        int halfheight = (mainRect.height() - 2*ypad)/2;
        QRect amountRect(mainRect.left() + xspace, mainRect.top()+ypad, mainRect.width() - xspace, halfheight);
        QRect addressRect(mainRect.left() + xspace, mainRect.top()+ypad+halfheight, mainRect.width() - xspace, halfheight);
        icon = platformStyle->SingleColorIcon(icon);
        icon.paint(painter, decorationRect);

        QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
        QString address = index.data(Qt::DisplayRole).toString();
        qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
        bool confirmed = index.data(TransactionTableModel::ConfirmedRole).toBool();
        QVariant value = index.data(Qt::ForegroundRole);
        QColor foreground = option.palette.color(QPalette::Text);
        if(value.canConvert<QBrush>())
        {
            QBrush brush = qvariant_cast<QBrush>(value);
            foreground = brush.color();
        }

        painter->setPen(foreground);
        QRect boundingRect;
        painter->drawText(addressRect, Qt::AlignLeft | Qt::AlignVCenter, address, &boundingRect);

        if(amount < 0)
        {
            foreground = COLOR_NEGATIVE;
        }
        else if(!confirmed)
        {
            foreground = COLOR_UNCONFIRMED;
        }
        else
        {
            foreground = option.palette.color(QPalette::Text);
        }
        painter->setPen(foreground);
        QString amountText = BitcoinUnits::formatWithUnit(unit, amount, true, BitcoinUnits::SeparatorStyle::ALWAYS);
        if(!confirmed)
        {
            amountText = QString("[") + amountText + QString("]");
        }

        QRect amount_bounding_rect;
        painter->drawText(amountRect, Qt::AlignRight | Qt::AlignVCenter, amountText, &amount_bounding_rect);

        painter->setPen(option.palette.color(QPalette::Text));
        QRect date_bounding_rect;
        painter->drawText(amountRect, Qt::AlignLeft | Qt::AlignVCenter, GUIUtil::dateTimeStr(date), &date_bounding_rect);

        // 0.4*date_bounding_rect.width() is used to visually distinguish a date from an amount.
        const int minimum_width = 1.4 * date_bounding_rect.width() + amount_bounding_rect.width();
        const auto search = m_minimum_width.find(index.row());
        if (search == m_minimum_width.end() || search->second != minimum_width) {
            m_minimum_width[index.row()] = minimum_width;
            Q_EMIT width_changed(index);
        }

        painter->restore();
    }

    inline QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const auto search = m_minimum_width.find(index.row());
        const int minimum_text_width = search == m_minimum_width.end() ? 0 : search->second;
        return {DECORATION_SIZE + 8 + minimum_text_width, DECORATION_SIZE};
    }

    BitcoinUnit unit{BitcoinUnit::BTC};

Q_SIGNALS:
    //! An intermediate signal for emitting from the `paint() const` member function.
    void width_changed(const QModelIndex& index) const;

private:
    const PlatformStyle* platformStyle;
    mutable std::map<int, int> m_minimum_width;
};

#include <qt/overviewpage.moc>

OverviewPage::OverviewPage(const PlatformStyle *platformStyle, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::OverviewPage),
    m_platform_style{platformStyle},
    txdelegate(new TxViewDelegate(platformStyle, this))
{
    ui->setupUi(this);

    m_price_status = new QLabel(tr("Loading live ELVN reference price…"), this);
    m_price_status->setObjectName(QStringLiteral("priceStatus"));
    m_price_status->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
    ui->topLayout->insertWidget(1, m_price_status);

    QSettings settings;
    m_price_usd = settings.value(QStringLiteral("elarvonPriceUsd"), 0.0).toDouble();
    m_price_eur = settings.value(QStringLiteral("elarvonPriceEur"), 0.0).toDouble();
    m_price_updated_at = settings.value(QStringLiteral("elarvonPriceUpdatedAt")).toString();
    m_price_available = m_price_usd > 0.0 && m_price_eur > 0.0;
    m_price_cached = m_price_available;
    updatePriceStatus();

    m_price_manager = new QNetworkAccessManager(this);
    connect(m_price_manager, &QNetworkAccessManager::finished, this, &OverviewPage::handlePriceReply);
    auto* price_timer = new QTimer(this);
    price_timer->setInterval(60 * 1000);
    connect(price_timer, &QTimer::timeout, this, &OverviewPage::requestPrice);
    price_timer->start();
    QTimer::singleShot(0, this, &OverviewPage::requestPrice);

    // use a SingleColorIcon for the "out of sync warning" icon
    QIcon icon = m_platform_style->SingleColorIcon(QStringLiteral(":/icons/warning"));
    ui->labelTransactionsStatus->setIcon(icon);
    ui->labelWalletStatus->setIcon(icon);

    // Recent transactions
    ui->listTransactions->setItemDelegate(txdelegate);
    ui->listTransactions->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listTransactions->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listTransactions->setAttribute(Qt::WA_MacShowFocusRect, false);

    connect(ui->listTransactions, &TransactionOverviewWidget::clicked, this, &OverviewPage::handleTransactionClicked);

    // start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);
    connect(ui->labelWalletStatus, &QPushButton::clicked, this, &OverviewPage::outOfSyncWarningClicked);
    connect(ui->labelTransactionsStatus, &QPushButton::clicked, this, &OverviewPage::outOfSyncWarningClicked);
}

void OverviewPage::handleTransactionClicked(const QModelIndex &index)
{
    if(filter)
        Q_EMIT transactionClicked(filter->mapToSource(index));
}

void OverviewPage::setPrivacy(bool privacy)
{
    m_privacy = privacy;
    clientModel->getOptionsModel()->setOption(OptionsModel::OptionID::MaskValues, privacy);
    const auto& balances = walletModel->getCachedBalance();
    if (balances.balance != -1) {
        setBalance(balances);
    }

    ui->listTransactions->setVisible(!m_privacy);

    const QString status_tip = m_privacy ? tr("Privacy mode activated for the Overview tab. To unmask the values, uncheck Settings->Mask values.") : "";
    setStatusTip(status_tip);
    QStatusTipEvent event(status_tip);
    QApplication::sendEvent(this, &event);
}

OverviewPage::~OverviewPage()
{
    delete ui;
}

void OverviewPage::setBalance(const interfaces::WalletBalances& balances)
{
    BitcoinUnit unit = walletModel->getOptionsModel()->getDisplayUnit();

    // ELARVON fiat display.
    const auto format_elvn_fiat = [this, unit](const CAmount amount) -> QString {
        const QString elvn = BitcoinUnits::formatWithPrivacy(
            unit,
            amount,
            BitcoinUnits::SeparatorStyle::ALWAYS,
            m_privacy).toHtmlEscaped();

        if (m_privacy) {
            return QStringLiteral(
                "<b>%1</b>"
                "&nbsp;&nbsp;=&nbsp;&nbsp;$•••• USD"
                "&nbsp;&nbsp;/&nbsp;&nbsp;"
                "<span style=\"color:#C9972D;font-weight:600;\">€•••• EUR</span>"
            ).arg(elvn);
        }

        const double coins =
            static_cast<double>(amount) /
            static_cast<double>(BitcoinUnits::factor(BitcoinUnit::BTC));

        const QLocale locale(QLocale::English, QLocale::UnitedStates);

        const QString usd =
            locale.toString(coins * m_price_usd, 'f', 2);

        const QString eur =
            locale.toString(coins * m_price_eur, 'f', 2);

        if (!m_price_available) {
            return QStringLiteral(
                "<b>%1</b>"
                "&nbsp;&nbsp;=&nbsp;&nbsp;"
                "<span style=\"color:#71685F;\">USD unavailable</span>"
                "&nbsp;&nbsp;/&nbsp;&nbsp;"
                "<span style=\"color:#8F6B35;font-weight:600;\">EUR unavailable</span>"
            ).arg(elvn);
        }

        return QStringLiteral(
            "<b>%1</b>"
            "&nbsp;&nbsp;=&nbsp;&nbsp;"
            "<span style=\"color:#666666;\">$%2 USD</span>"
            "&nbsp;&nbsp;/&nbsp;&nbsp;"
            "<span style=\"color:#C9972D;font-weight:600;\">€%3 EUR</span>"
        ).arg(elvn, usd, eur);
    };

    ui->labelBalance->setTextFormat(Qt::RichText);
    ui->labelUnconfirmed->setTextFormat(Qt::RichText);
    ui->labelImmature->setTextFormat(Qt::RichText);
    ui->labelTotal->setTextFormat(Qt::RichText);

    ui->labelBalance->setText(format_elvn_fiat(balances.balance));
    ui->labelUnconfirmed->setText(format_elvn_fiat(balances.unconfirmed_balance));
    ui->labelImmature->setText(format_elvn_fiat(balances.immature_balance));
    ui->labelTotal->setText(format_elvn_fiat(
        balances.balance +
        balances.unconfirmed_balance +
        balances.immature_balance));
    // only show immature (newly mined) balance if it's non-zero, so as not to complicate things
    // for the non-mining users
    bool showImmature = balances.immature_balance != 0;

    ui->labelImmature->setVisible(showImmature);
    ui->labelImmatureText->setVisible(showImmature);
}

void OverviewPage::requestPrice()
{
    QNetworkRequest request(QUrl(QStringLiteral("https://elarvon.io/api/price")));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("ELARVON-Core/31.2"));
    request.setRawHeader("Accept", "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_price_manager->get(request);
    QTimer::singleShot(5000, reply, [reply] {
        if (reply->isRunning()) reply->abort();
    });
}

void OverviewPage::handlePriceReply(QNetworkReply* reply)
{
    reply->deleteLater();
    const QByteArray payload = reply->readAll();
    if (reply->error() != QNetworkReply::NoError || payload.size() > 64 * 1024) {
        m_price_cached = m_price_available;
        updatePriceStatus();
        return;
    }

    QJsonParseError parse_error;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parse_error);
    const QJsonObject root = document.object();
    const QJsonObject asset = root.value(QStringLiteral("asset")).toObject();
    const QJsonObject price = root.value(QStringLiteral("price")).toObject();
    const QJsonObject fx = root.value(QStringLiteral("fx")).toObject();
    const double usd = price.value(QStringLiteral("USD")).toDouble(-1.0);
    double eur = price.value(QStringLiteral("EUR")).toDouble(-1.0);
    if (eur <= 0.0) {
        const double usd_eur = fx.value(QStringLiteral("USD_EUR")).toDouble(-1.0);
        if (usd > 0.0 && usd_eur > 0.0) eur = usd * usd_eur;
    }
    const QString updated_at = root.value(QStringLiteral("updatedAt")).toString();
    QDateTime timestamp = QDateTime::fromString(updated_at, Qt::ISODateWithMs);
    if (!timestamp.isValid()) timestamp = QDateTime::fromString(updated_at, Qt::ISODate);

    if (parse_error.error != QJsonParseError::NoError || !root.value(QStringLiteral("ok")).toBool() ||
        asset.value(QStringLiteral("symbol")).toString() != QStringLiteral("ELVN") ||
        usd <= 0.0 || eur <= 0.0 || !timestamp.isValid()) {
        m_price_cached = m_price_available;
        updatePriceStatus();
        return;
    }

    m_price_usd = usd;
    m_price_eur = eur;
    m_price_updated_at = updated_at;
    m_price_available = true;
    const qint64 price_age = timestamp.toUTC().secsTo(QDateTime::currentDateTimeUtc());
    m_price_cached = price_age < 0 || price_age > 15 * 60;

    QSettings settings;
    settings.setValue(QStringLiteral("elarvonPriceUsd"), m_price_usd);
    settings.setValue(QStringLiteral("elarvonPriceEur"), m_price_eur);
    settings.setValue(QStringLiteral("elarvonPriceUpdatedAt"), m_price_updated_at);

    updatePriceStatus();
    if (walletModel && walletModel->getCachedBalance().balance != -1) {
        setBalance(walletModel->getCachedBalance());
    }
}

void OverviewPage::updatePriceStatus()
{
    if (!m_price_status) return;
    if (!m_price_available) {
        m_price_status->setText(tr("Live ELVN reference price is temporarily unavailable · elarvon.io"));
        m_price_status->setToolTip(tr("The wallet will retry https://elarvon.io/api/price automatically every minute."));
        return;
    }

    const QLocale locale;
    const QString state = m_price_cached ? tr("Cached reference price") : tr("Live reference price");
    m_price_status->setText(tr("%1 · 1 ELVN = $%2 USD · €%3 EUR · elarvon.io")
        .arg(state, locale.toString(m_price_usd, 'f', 2), locale.toString(m_price_eur, 'f', 2)));
    m_price_status->setToolTip(tr("Reference market price from https://elarvon.io/api/price. Updated automatically every minute."));
}

void OverviewPage::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if (model) {
        // Show warning, for example if this is a prerelease version
        connect(model, &ClientModel::alertsChanged, this, &OverviewPage::updateAlerts);
        updateAlerts(model->getStatusBarWarnings());

        connect(model->getOptionsModel(), &OptionsModel::fontForMoneyChanged, this, &OverviewPage::setMonospacedFont);
        setMonospacedFont(clientModel->getOptionsModel()->getFontForMoney());
    }
}

void OverviewPage::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
    if(model && model->getOptionsModel())
    {
        // Set up transaction list
        filter.reset(new TransactionFilterProxy());
        filter->setSourceModel(model->getTransactionTableModel());
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->setShowInactive(false);
        filter->sort(TransactionTableModel::Date, Qt::DescendingOrder);

        ui->listTransactions->setModel(filter.get());
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);

        connect(filter.get(), &TransactionFilterProxy::rowsInserted, this, &OverviewPage::LimitTransactionRows);
        connect(filter.get(), &TransactionFilterProxy::rowsRemoved, this, &OverviewPage::LimitTransactionRows);
        connect(filter.get(), &TransactionFilterProxy::rowsMoved, this, &OverviewPage::LimitTransactionRows);
        LimitTransactionRows();
        // Keep up to date with wallet
        setBalance(model->getCachedBalance());
        connect(model, &WalletModel::balanceChanged, this, &OverviewPage::setBalance);

        connect(model->getOptionsModel(), &OptionsModel::displayUnitChanged, this, &OverviewPage::updateDisplayUnit);
    }

    // update the display unit, to not use the default ("BTC")
    updateDisplayUnit();
}

void OverviewPage::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::PaletteChange) {
        QIcon icon = m_platform_style->SingleColorIcon(QStringLiteral(":/icons/warning"));
        ui->labelTransactionsStatus->setIcon(icon);
        ui->labelWalletStatus->setIcon(icon);
    }

    QWidget::changeEvent(e);
}

// Only show most recent NUM_ITEMS rows
void OverviewPage::LimitTransactionRows()
{
    if (filter && ui->listTransactions && ui->listTransactions->model() && filter.get() == ui->listTransactions->model()) {
        for (int i = 0; i < filter->rowCount(); ++i) {
            ui->listTransactions->setRowHidden(i, i >= NUM_ITEMS);
        }
    }
}

void OverviewPage::updateDisplayUnit()
{
    if (walletModel && walletModel->getOptionsModel()) {
        const auto& balances = walletModel->getCachedBalance();
        if (balances.balance != -1) {
            setBalance(balances);
        }

        // Update txdelegate->unit with the current unit
        txdelegate->unit = walletModel->getOptionsModel()->getDisplayUnit();

        ui->listTransactions->update();
    }
}

void OverviewPage::updateAlerts(const QString &warnings)
{
    this->ui->labelAlerts->setVisible(!warnings.isEmpty());
    this->ui->labelAlerts->setText(warnings);
}

void OverviewPage::showOutOfSyncWarning(bool fShow)
{
    ui->labelWalletStatus->setVisible(fShow);
    ui->labelTransactionsStatus->setVisible(fShow);
}

void OverviewPage::setMonospacedFont(const QFont& f)
{
    ui->labelBalance->setFont(f);
    ui->labelUnconfirmed->setFont(f);
    ui->labelImmature->setFont(f);
    ui->labelTotal->setFont(f);
}

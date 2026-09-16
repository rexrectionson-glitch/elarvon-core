// Copyright (c) 2026 The ELARVON Core developers
// Distributed under the MIT software license.

#ifndef BITCOIN_QT_ELARVONTHEME_H
#define BITCOIN_QT_ELARVONTHEME_H

#include <QString>

/** ELARVON's restrained ivory, obsidian and gold desktop theme. */
inline QString ElarvonStyleSheet()
{
    return QStringLiteral(R"QSS(
QWidget {
    color: #1F1A13;
    background-color: #F4EFE6;
    font-family: "Segoe UI", "Inter", sans-serif;
    font-size: 10pt;
}
QMainWindow, QDialog { background-color: #F4EFE6; }
QMenuBar {
    background: #17140F;
    color: #F7F3EC;
    padding: 5px 8px;
    border-bottom: 1px solid #3B3020;
}
QMenuBar::item { padding: 7px 12px; border-radius: 5px; }
QMenuBar::item:selected { background: #B58A45; color: #FFFFFF; }
QMenu {
    background: #FFFDF8;
    color: #1F1A13;
    border: 1px solid #D9D0C2;
    padding: 6px;
}
QMenu::item { padding: 8px 28px 8px 12px; border-radius: 4px; }
QMenu::item:selected { background: #EAD9B8; color: #17140F; }
QToolBar {
    background: #1F1A13;
    border: 0;
    border-bottom: 2px solid #B58A45;
    spacing: 7px;
    padding: 7px 10px;
}
QToolBar QToolButton {
    color: #F7F3EC;
    background: transparent;
    border: 0;
    border-radius: 6px;
    padding: 9px 14px;
    font-weight: 600;
}
QToolBar QToolButton:hover { background: #332B21; color: #E9C77E; }
QToolBar QToolButton:checked { background: #B58A45; color: #FFFFFF; }
QToolBar QLabel { color: #E8DFD1; background: transparent; }
QToolBar QComboBox { min-width: 140px; }
QFrame {
    background: #FFFDF8;
    border: 1px solid #E2D9CB;
    border-radius: 9px;
}
QLabel { background: transparent; }
QLabel#priceStatus {
    color: #684A17;
    background: #F5E8CE;
    border: 1px solid #D9BB7A;
    border-radius: 7px;
    padding: 9px 12px;
    font-weight: 600;
}
QPushButton {
    color: #FFFFFF;
    background: #1F1A13;
    border: 1px solid #1F1A13;
    border-radius: 6px;
    padding: 8px 15px;
    font-weight: 600;
}
QPushButton:hover { background: #B58A45; border-color: #B58A45; }
QPushButton:pressed { background: #8D651F; }
QPushButton:disabled { color: #A59C90; background: #E5DED4; border-color: #D6CDC0; }
QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox, QDoubleSpinBox, QComboBox {
    color: #1F1A13;
    background: #FFFFFF;
    border: 1px solid #CBC1B3;
    border-radius: 6px;
    padding: 7px 9px;
    selection-background-color: #D8B46F;
}
QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus, QSpinBox:focus,
QDoubleSpinBox:focus, QComboBox:focus { border: 2px solid #B58A45; }
QComboBox::drop-down { border: 0; width: 24px; }
QTableView, QListView, QTreeView {
    alternate-background-color: #F5F0E8;
    background: #FFFDF8;
    border: 1px solid #DED5C8;
    border-radius: 7px;
    gridline-color: #E9E1D6;
    selection-background-color: #E5C98F;
    selection-color: #17140F;
}
QHeaderView::section {
    color: #5E5140;
    background: #EEE7DC;
    border: 0;
    border-bottom: 1px solid #D6CCBD;
    padding: 8px;
    font-weight: 600;
}
QTabWidget::pane { border: 1px solid #D8CEC0; background: #FFFDF8; }
QTabBar::tab {
    background: #EAE3D8;
    color: #554A3B;
    padding: 8px 14px;
    border: 1px solid #D8CEC0;
}
QTabBar::tab:selected { background: #FFFDF8; color: #9B6E21; border-bottom-color: #FFFDF8; }
QStatusBar { color: #665C4F; background: #EFE9DF; border-top: 1px solid #DCD2C4; }
QStatusBar::item { border: 0; }
QProgressBar {
    color: #FFFFFF;
    background: #DED6CA;
    border: 0;
    border-radius: 7px;
    text-align: center;
    min-height: 14px;
}
QProgressBar::chunk { background: #B58A45; border-radius: 7px; }
QToolTip { color: #FFFFFF; background: #1F1A13; border: 1px solid #B58A45; padding: 5px; }
QScrollBar:vertical { background: #EFE9DF; width: 12px; margin: 0; }
QScrollBar::handle:vertical { background: #B9AA96; min-height: 28px; border-radius: 6px; }
QScrollBar:horizontal { background: #EFE9DF; height: 12px; margin: 0; }
QScrollBar::handle:horizontal { background: #B9AA96; min-width: 28px; border-radius: 6px; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
)QSS");
}

#endif // BITCOIN_QT_ELARVONTHEME_H

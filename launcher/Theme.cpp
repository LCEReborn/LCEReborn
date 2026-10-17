#include "Theme.hpp"

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>

#include "Widgets.hpp"

void ApplyTheme(QApplication& application)
{
    application.setStyle(QStyleFactory::create("Fusion"));

    const QColor window(25, 23, 21);
    const QColor base(19, 17, 16);
    const QColor text(236, 230, 221);
    const QColor muted(154, 145, 136);
    const QColor accent(232, 165, 75);

    QPalette palette;
    palette.setColor(QPalette::Window, window);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Base, base);
    palette.setColor(QPalette::AlternateBase, QColor(28, 26, 24));
    palette.setColor(QPalette::ToolTipBase, QColor(44, 40, 36));
    palette.setColor(QPalette::ToolTipText, text);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::PlaceholderText, muted);
    palette.setColor(QPalette::Button, QColor(44, 40, 36));
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::BrightText, Qt::white);
    palette.setColor(QPalette::Highlight, QColor(150, 98, 36));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Link, accent);
    palette.setColor(QPalette::LinkVisited, QColor(214, 150, 70));
    palette.setColor(QPalette::Disabled, QPalette::Text, muted);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, muted);
    palette.setColor(QPalette::Disabled, QPalette::WindowText, muted);
    application.setPalette(palette);

    QString style = R"(
        QWidget { font-size: 10pt; }
        QFrame#TopBar { background: rgba(14, 13, 12, 170); border-bottom: 1px solid rgba(255, 255, 255, 18); }
        QFrame#NavSeparator { background: rgba(255, 255, 255, 34); border: none; }
        QFrame#BottomBar { background: rgba(14, 13, 12, 170); border-top: 1px solid rgba(255, 255, 255, 18); }
        QLabel#Wordmark { font-family: "@DISPLAY@"; font-size: 17pt; font-weight: 800; color: #ece6dd;
                          letter-spacing: 3px; }
        QLabel#Tagline { font-family: "@DISPLAY@"; font-size: 8pt; font-weight: 600; color: #8a8178;
                         letter-spacing: 1px; }
        QLabel#PageTitle { font-family: "@DISPLAY@"; font-size: 30pt; font-weight: 700; color: #ece6dd; }
        QLabel#PageSubtitle { color: #9a9188; font-size: 10.5pt; }
        QLabel#Status { color: #9a9188; font-size: 9pt; }
        QLabel#StatusReady { color: #8fcf6c; font-size: 9pt; font-weight: 600; }
        QLabel#StatusWarning { color: #e8b25f; font-size: 9pt; }
        QLabel#StatusError { color: #ef6a5e; font-size: 9pt; }
        QPushButton#RailLink { background: transparent; border: none; padding: 2px 4px; color: #8a8178;
                               font-size: 9pt; text-decoration: underline; }
        QPushButton#RailLink:hover { color: #e8a54b; }
        QLabel#SetupWordmark { font-family: "@DISPLAY@"; font-size: 30pt; font-weight: 800; color: #ece6dd;
                               letter-spacing: 4px; }
        QLabel#SetupStatement { font-family: "@DISPLAY@"; font-size: 36pt; font-weight: 700; color: #e8a54b; }
        QLabel#SetupExplanation { color: #b3aaa0; font-size: 10.5pt; line-height: 140%; }
        QLabel#StepNumber { font-family: "@DISPLAY@"; font-size: 30pt; font-weight: 700; color: #e8a54b; }
        QLabel#StepTitle { font-family: "@DISPLAY@"; font-size: 18pt; font-weight: 700; color: #ece6dd; }
        QLabel#StepHint { color: #9a9188; font-size: 10pt; }
        QLabel#StepGood { color: #8fcf6c; font-size: 10pt; font-weight: 600; }
        QLabel#StepBad { color: #e8b25f; font-size: 10pt; }
        QPushButton { background: #2c2824; border: 1px solid #3d3832; border-radius: 5px; padding: 6px 14px;
                      color: #ece6dd; icon-size: 16px; }
        QPushButton:hover { background: #36312c; border-color: #4a443d; }
        QPushButton:pressed { background: #26221f; }
        QPushButton:disabled { color: #6f675f; background: #24211e; }
        QPushButton#SetupButton { background: #e8a54b; border-color: #b07028; color: #241608; font-weight: 700; }
        QPushButton#SetupButton:hover { background: #f0b25e; }
        QPushButton#SetupButton:disabled { background: #5a4630; color: #a8957e; }
        QLineEdit, QComboBox { background: #151311; border: 1px solid #3d3832; border-radius: 5px; padding: 6px 9px;
                               selection-background-color: #96622a; }
        QLineEdit:focus, QComboBox:focus { border-color: #e8a54b; }
        QComboBox::drop-down { border: none; width: 22px; }
        QComboBox QAbstractItemView { background: #1f1c19; border: 1px solid #3d3832;
                                      selection-background-color: #5a3d1c; }
        QFrame#ProfilePopup { background: #1f1c19; border: 1px solid #3d3832; border-radius: 6px; }
        QFrame#PopupSeparator { background: #3d3832; border: none; }
        QToolButton#PopupAction { background: transparent; border: none; border-radius: 5px; padding: 4px 0;
                                  color: #9a9188; font-size: 8pt; }
        QToolButton#PopupAction:hover { background: rgba(255, 255, 255, 14); color: #e8a54b; }
        QMenu { background: #1f1c19; border: 1px solid #3d3832; padding: 6px; }
        QMenu::item { padding: 7px 26px 7px 22px; border-radius: 4px; }
        QMenu::item:selected { background: #3a2a18; color: #f6c27c; }
        QMenu::separator { height: 1px; background: #3d3832; margin: 5px 8px; }
        QFrame#Card { background: rgba(255, 255, 255, 6); border: 1px solid rgba(255, 255, 255, 16); border-radius: 8px; }
        QLabel#Heading { font-family: "@DISPLAY@"; font-size: 15pt; font-weight: 700; }
        QLabel#Muted { color: #9a9188; }
        QTextBrowser#NewsBody { background: transparent; border: none; font-size: 10.5pt; }
        QTableView#LogView { background: rgba(8, 7, 6, 150); border: none; border-radius: 4px; }
        QTableView#LogView::item { padding-left: 10px; }
        QTableWidget#ProfileList { background: transparent; border: none; font-size: 11pt;
                                   selection-background-color: rgba(232, 165, 75, 34); selection-color: #f6c27c; }
        QTableWidget#ProfileList::item { border-bottom: 1px solid rgba(255, 255, 255, 14); padding-left: 10px;
                                         padding-right: 18px; }
        QTableWidget#ProfileList::item:selected { background: rgba(232, 165, 75, 30); color: #f6c27c; }
        QTableView, QTableWidget, QTextBrowser { background: rgba(10, 9, 8, 120); border: none; gridline-color: #2a2622; }
        QHeaderView::section { background: transparent; color: #8a8178; border: none;
                               border-bottom: 1px solid rgba(255, 255, 255, 18); padding: 6px 10px; font-weight: 700;
                               font-family: "@DISPLAY@"; letter-spacing: 1px; }
        QProgressBar { background: #151311; border: 1px solid #3d3832; border-radius: 4px; height: 14px; }
        QProgressBar::chunk { background: #e8a54b; border-radius: 3px; }
        QCheckBox::indicator { width: 14px; height: 14px; }
        QToolTip { border: 1px solid #3d3832; }
        QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }
        QScrollBar::handle:vertical { background: #3d3832; border-radius: 5px; min-height: 30px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar:horizontal { background: transparent; height: 10px; margin: 0; }
        QScrollBar::handle:horizontal { background: #3d3832; border-radius: 5px; min-width: 30px; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
    )";
    style.replace("@DISPLAY@", DisplayFamily());
    application.setStyleSheet(style);
}

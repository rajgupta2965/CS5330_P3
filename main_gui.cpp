/*
Name: Sangeeth Deleep Menon | NUID: 002524579
Course: CS5330; Pattern Recognition and Computer Vision | Section: 03 | CRN: 40669 | Online

Name: Raj Gupta | NUID: 002068701
Course: CS5330; Pattern Recognition and Computer Vision | Section: 01 | CRN: 38745 | Online
*/

/*
main_gui.cpp
  CS 5330 - Project 3
  Qt application entry point.
*/
#include <QApplication>
#include "gui.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Dark theme
    app.setStyle("Fusion");
    QPalette dark;
    dark.setColor(QPalette::Window,          QColor(45, 45, 48));
    dark.setColor(QPalette::WindowText,      Qt::white);
    dark.setColor(QPalette::Base,            QColor(30, 30, 30));
    dark.setColor(QPalette::AlternateBase,   QColor(45, 45, 48));
    dark.setColor(QPalette::ToolTipBase,     Qt::white);
    dark.setColor(QPalette::ToolTipText,     Qt::white);
    dark.setColor(QPalette::Text,            Qt::white);
    dark.setColor(QPalette::Button,          QColor(55, 55, 58));
    dark.setColor(QPalette::ButtonText,      Qt::white);
    dark.setColor(QPalette::BrightText,      Qt::red);
    dark.setColor(QPalette::Highlight,       QColor(42, 130, 218));
    dark.setColor(QPalette::HighlightedText, Qt::black);
    app.setPalette(dark);

    MainWindow win;
    win.show();

    return app.exec();
}
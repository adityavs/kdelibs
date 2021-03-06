/*
    Copyright 2002 Simon Hausmann <hausmann@kde.org>
    Copyright 2005-2006 David Faure <faure@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include <QtCore/QTimer>
#include <QtGui/QLabel>

#include <kapplication.h>
#include <kcmdlineargs.h>
#include <kstatusbar.h>
#include <kmenubar.h>

#include "kmainwindowtest.h"

MainWindow::MainWindow()
{
    QTimer::singleShot( 2*1000, this, SLOT(showMessage()) );

    setCentralWidget( new QLabel( "foo", this ) );

    menuBar()->addAction( "hi" );
}

void MainWindow::showMessage()
{
    statusBar()->show();
    statusBar()->showMessage( "test" );
}

int main( int argc, char **argv )
{
    KCmdLineArgs::init( argc, argv, "kmainwindowtest", 0, ki18n("KMainWindowTest"), "1.0", ki18n("kmainwindow test app"));
    KApplication app;

    MainWindow* mw = new MainWindow; // deletes itself when closed
    mw->show();

    return app.exec();
}

#include "kmainwindowtest.moc"

/* vim: et sw=4 ts=4
 */

/*
 * This file was generated by dbusxml2cpp version 0.6
 * Command line was: dbusxml2cpp -p klauncher_iface -m ../kinit/org.kde.KLauncher.xml
 *
 * dbusxml2cpp is Copyright (C) 2006 Trolltech AS. All rights reserved.
 *
 * This is an auto-generated file.
 * This file may have been hand-edited. Look for HAND-EDIT comments
 * before re-generating it.
 */

#include "klauncher_iface.h"

/*
 * Implementation of interface class OrgKdeKLauncherInterface
 */

OrgKdeKLauncherInterface::OrgKdeKLauncherInterface(const QString &service, const QString &path, const QDBusConnection &connection, QObject *parent)
    : QDBusAbstractInterface(service, path, staticInterfaceName(), connection, parent)
{
}

OrgKdeKLauncherInterface::~OrgKdeKLauncherInterface()
{
}


#include "klauncher_iface.moc"
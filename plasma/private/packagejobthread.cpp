/******************************************************************************
*   Copyright 2007-2009 by Aaron Seigo <aseigo@kde.org>                       *
*   Copyright 2012 Sebastian Kügler <sebas@kde.org>                           *
*                                                                             *
*   This library is free software; you can redistribute it and/or             *
*   modify it under the terms of the GNU Library General Public               *
*   License as published by the Free Software Foundation; either              *
*   version 2 of the License, or (at your option) any later version.          *
*                                                                             *
*   This library is distributed in the hope that it will be useful,           *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of            *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU          *
*   Library General Public License for more details.                          *
*                                                                             *
*   You should have received a copy of the GNU Library General Public License *
*   along with this library; see the file COPYING.LIB.  If not, write to      *
*   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,      *
*   Boston, MA 02110-1301, USA.                                               *
*******************************************************************************/

#include "private/packagejobthread_p.h"


#include "package.h"
#include "config-plasma.h"

#include <karchive.h>
#include <kdesktopfile.h>
#include <ktar.h>
#include <kzip.h>

#include <QDir>
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QFile>
#include <QIODevice>
#include <QMimeType>
#include <QMimeDatabase>
#include <QRegExp>
#include <QtNetwork/QHostInfo>
#include <qtemporarydir.h>

#include <kdebug.h>

namespace Plasma
{

bool copyFolder(QString sourcePath, QString targetPath)
{
    QDir source(sourcePath);
    if (!source.exists()) {
        return false;
    }

    QDir target(targetPath);
    if (!target.exists()) {
        QString targetName = target.dirName();
        target.cdUp();
        target.mkdir(targetName);
        target = QDir(targetPath);
    }

    foreach (const QString &fileName, source.entryList(QDir::Files)) {
        QString sourceFilePath = sourcePath + QDir::separator() + fileName;
        QString targetFilePath = targetPath + QDir::separator() + fileName;

        if (!QFile::copy(sourceFilePath, targetFilePath)) {
            return false;
        }
    }

    foreach (const QString &subFolderName, source.entryList(QDir::AllDirs | QDir::NoDotAndDotDot)) {
        QString sourceSubFolderPath = sourcePath + QDir::separator() + subFolderName;
        QString targetSubFolderPath = targetPath + QDir::separator() + subFolderName;

        if (!copyFolder(sourceSubFolderPath, targetSubFolderPath)) {
            return false;
        }
    }

    return true;
}

// Qt5 TODO: use QDir::removeRecursively() instead
bool removeFolder(QString folderPath)
{
    QDir folder(folderPath);
    if(!folder.exists())
        return false;

    foreach (const QString &fileName, folder.entryList(QDir::Files)) {
        if (!QFile::remove(folderPath + QDir::separator() + fileName)) {
            return false;
        }
    }

    foreach (const QString &subFolderName, folder.entryList(QDir::AllDirs | QDir::NoDotAndDotDot)) {
        if (!removeFolder(folderPath + QDir::separator() + subFolderName)) {
            return false;
        }
    }

    QString folderName = folder.dirName();
    folder.cdUp();
    return folder.rmdir(folderName);
}


class PackageJobThreadPrivate {
public:
    QString installPath;
//     QString packageRoot;
    QString servicePrefix;
};



PackageJobThread::PackageJobThread(const QString &servicePrefix, QObject* parent) :
QThread(parent)
{
    d = new PackageJobThreadPrivate;
    d->servicePrefix = servicePrefix;
}

PackageJobThread::~PackageJobThread()
{
    delete d;
}

bool PackageJobThread::install(const QString& src, const QString &dest)
{
    bool ok = installPackage(src, dest);
    kDebug() << "emit installPathChanged " << d->installPath;
    emit installPathChanged(d->installPath);
    kDebug() << "Thread: installFinished" << ok;
    emit finished(ok);
    return ok;
}

bool PackageJobThread::installPackage(const QString& src, const QString &dest)
{
    kDebug() << "::install: " << src << dest;
    //TODO: report *what* failed if something does fail
    QString packageRoot = dest;

    QDir root(dest);

    // FIXME: make sure package root is there.
    if (!root.exists()) {
        QDir().mkpath(dest);
        if (!root.exists()) {
            kWarning() << "Could not create package root directory:" << dest;
            return false;
        }
    }

    QFileInfo fileInfo(src);
    if (!fileInfo.exists()) {
        kWarning() << "No such file:" << src;
        return false;
    }

    QString path;
    QTemporaryDir tempdir;
    bool archivedPackage = false;

    if (fileInfo.isDir()) {
        // we have a directory, so let's just install what is in there
        path = src;

        // make sure we end in a slash!
        if (path[path.size() - 1] != '/') {
            path.append('/');
        }
    } else {
        KArchive *archive = 0;
        QMimeDatabase db;
        QMimeType mimetype = db.mimeTypeForFile(src);

        if (mimetype.inherits("application/zip")) {
            archive = new KZip(src);
        } else if (mimetype.inherits("application/x-compressed-tar") ||
                   mimetype.inherits("application/x-tar")|| mimetype.inherits("application/x-bzip-compressed-tar") ||
                   mimetype.inherits("application/x-xz") || mimetype.inherits("application/x-lzma")) {
            archive = new KTar(src);
        } else {
            kWarning() << "Could not open package file, unsupported archive format:" << src << mimetype.name();
            return false;
        }

        if (!archive->open(QIODevice::ReadOnly)) {
            kWarning() << "Could not open package file:" << src;
        delete archive;
            return false;
        }

        archivedPackage = true;
        path = tempdir.path() + '/';

        kDebug() << "path: " << path;
        d->installPath = path;

        const KArchiveDirectory *source = archive->directory();
        source->copyTo(path);

        QStringList entries = source->entries();
        if (entries.count() == 1) {
            const KArchiveEntry *entry = source->entry(entries[0]);
            if (entry->isDirectory()) {
                path.append(entry->name()).append("/");
            }
        }

        delete archive;
    }

    QString metadataPath = path + "metadata.desktop";
    if (!QFile::exists(metadataPath)) {
        kWarning() << "No metadata file in package" << src << metadataPath;
        return false;
    }

    KPluginInfo meta(metadataPath);
    QString pluginName = meta.pluginName();
    kDebug() << "pluginname: " << meta.pluginName();
    if (pluginName.isEmpty()) {
        kWarning() << "Package plugin name not specified";
        return false;
    }

    // Ensure that package names are safe so package uninstall can't inject
    // bad characters into the paths used for removal.
    QRegExp validatePluginName("^[\\w-\\.]+$"); // Only allow letters, numbers, underscore and period.
    if (!validatePluginName.exactMatch(pluginName)) {
        kWarning() << "Package plugin name " << pluginName << "contains invalid characters";
        return false;
    }

    const QString targetName = dest + '/' + pluginName;
    kDebug() << " Target installation path: " << targetName;
    if (QFile::exists(targetName)) {
        kWarning() << targetName << "already exists";
        return false;
    }

    if (archivedPackage) {
        // it's in a temp dir, so just move it over.
        const bool ok = copyFolder(path, targetName);
        removeFolder(path);
        if (!ok) {
            kWarning() << "Could not move package to destination:" << targetName;
            return false;
        }
    } else {
        // it's a directory containing the stuff, so copy the contents rather
        // than move them
        const bool ok = copyFolder(path, targetName);
        if (!ok) {
            kWarning() << "Could not copy package to destination:" << targetName;
            return false;
        }
    }

    if (archivedPackage) {
        // no need to remove the temp dir (which has been successfully moved if it's an archive)
        tempdir.setAutoRemove(false);
    }

    if (!d->servicePrefix.isEmpty()) {
        // and now we register it as a service =)
        QString metaPath = targetName + "/metadata.desktop";
        KDesktopFile df(metaPath);
        KConfigGroup cg = df.desktopGroup();

        // Q: should not installing it as a service disqualify it?
        // Q: i don't think so since KServiceTypeTrader may not be
        // used by the installing app in any case, and the
        // package is properly installed - aseigo

        //TODO: reduce code duplication with registerPackage below

        const QString serviceName = d->servicePrefix + meta.pluginName() + ".desktop";

        QString service = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1String("/kde5/services/") + serviceName;
        const bool ok = QFile::copy(metaPath, service);
        if (ok) {
            // the icon in the installed file needs to point to the icon in the
            // installation dir!
            QString iconPath = targetName + '/' + cg.readEntry("Icon");
            QFile icon(iconPath);
            if (icon.exists()) {
                KDesktopFile df(service);
                KConfigGroup cg = df.desktopGroup();
                cg.writeEntry("Icon", iconPath);
            }
        } else {
            kWarning() << "Could not register package as service (this is not necessarily fatal):" << serviceName;
        }
    }
    /*
    QDBusInterface sycoca("org.kde.kded5", "/kbuildsycoca");
    sycoca.asyncCall("recreate");
    */
    kWarning() << "Not updating kbuildsycoca4, since that will go away. Do it yourself for now if needed.";
    return true;

}

bool PackageJobThread::uninstall(const QString& packagePath)
{
    // We need to remove the package directory and its metadata file.
    const QString targetName = packagePath; // FIXME : remove

    if (!QFile::exists(targetName)) {
        kWarning() << targetName << "does not exist";
        return false; // FIXME: KJob!
    }
    const QString &packageName = "FIXME";

    const QString serviceName = d->servicePrefix + packageName + ".desktop";

    QString service = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1String("/kde5/services/") + serviceName;
#ifndef NDEBUG
    kDebug() << "Removing service file " << service;
#endif
    bool ok = QFile::remove(service);

    if (!ok) {
        kWarning() << "Unable to remove " << service;
    }

    ok = removeFolder(targetName);
    if (!ok) {
        kWarning() << "Could not delete package from:" << targetName;
        return false; // FIXME: KJob!
    }

//     QDBusInterface sycoca("org.kde.kded5", "/kbuildsycoca");
//     sycoca.asyncCall("recreate");
    return true; // FIXME: KJob!
}



} // namespace Plasma

#include "moc_packagejobthread_p.cpp"

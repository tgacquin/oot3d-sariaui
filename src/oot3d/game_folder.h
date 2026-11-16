// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStringList>

namespace OoT3D::GameFolder {

inline QString Root() {
#ifdef Q_OS_MACOS
    QDir bundle(QCoreApplication::applicationDirPath());
    if (bundle.dirName() == QStringLiteral("MacOS") && bundle.cdUp() &&
        bundle.dirName() == QStringLiteral("Contents") && bundle.cdUp() &&
        bundle.dirName().endsWith(QStringLiteral(".app")) && bundle.cdUp()) {
        return bundle.absolutePath();
    }
#endif
    const auto appimage = qEnvironmentVariable("APPIMAGE");
    return appimage.isEmpty() ? QCoreApplication::applicationDirPath()
                              : QFileInfo(appimage).absolutePath();
}

inline QString User() {
    // Portable releases keep saves, settings and textures beside the app.
    // The developer launcher supplies its existing isolated user directory.
    const auto override_path = qEnvironmentVariable("OOT3D_USER_PATH");
    return override_path.isEmpty() ? QDir(Root()).filePath(QStringLiteral("user"))
                                   : override_path;
}

inline bool IsGameFile(const QFileInfo& file) {
    const auto suffix = file.suffix().toLower();
    return file.isFile() && file.isReadable() &&
           (suffix == QStringLiteral("cci") || suffix == QStringLiteral("3ds") ||
            suffix == QStringLiteral("cxi") || suffix == QStringLiteral("zcci"));
}

inline QString Resolve(const QString& root, const QStringList& args) {
    // Preserve explicit developer launches, including paths containing spaces.
    // Skip option values so movie/dump paths are never mistaken for the game.
    const QStringList value_options{QStringLiteral("--dump-video"), QStringLiteral("-d"),
        QStringLiteral("--gdbport"), QStringLiteral("-g"), QStringLiteral("--install"),
        QStringLiteral("-i"), QStringLiteral("--movie-play"), QStringLiteral("-p"),
        QStringLiteral("--movie-record"), QStringLiteral("-r"),
        QStringLiteral("--movie-record-author"), QStringLiteral("-a"),
        QStringLiteral("--multiplayer"), QStringLiteral("-m")};
    for (int i = 1; i < args.size(); ++i) {
        if (value_options.contains(args[i])) {
            if (i + 1 < args.size() && !args[i + 1].startsWith(QLatin1Char('-'))) ++i;
        } else if (i == args.size() - 1 && !args[i].startsWith(QLatin1Char('-'))) {
            const QFileInfo file(args[i]);
            return IsGameFile(file) ? file.absoluteFilePath() : QString{};
        }
    }

    QString game;
    for (const auto& file : QDir(root).entryInfoList(QDir::Files, QDir::Name)) {
        if (!IsGameFile(file)) continue;
        if (!game.isEmpty()) return {}; // Do not guess between multiple dumps.
        game = file.absoluteFilePath();
    }
    return game;
}

inline const QString& Rom() {
    static const QString game = Resolve(Root(), QCoreApplication::arguments());
    return game;
}

} // namespace OoT3D::GameFolder

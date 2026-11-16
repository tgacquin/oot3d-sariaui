// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/game_folder.h"

#include <QFile>
#include <QTemporaryDir>
#include <cstdlib>
#include <iostream>

static void Check(bool condition) {
    if (!condition) std::abort();
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    Check(temporary.isValid());
    const auto root = temporary.path();
    // Resolve paths without creating/moving saves or falling back to app data.
    qunsetenv("OOT3D_USER_PATH");
    const auto portable = QDir(OoT3D::GameFolder::Root()).filePath("user");
    Check(OoT3D::GameFolder::User() == portable);
    const auto developer_user = root + "/developer user";
    qputenv("OOT3D_USER_PATH", developer_user.toUtf8());
    Check(OoT3D::GameFolder::User() == developer_user);
    Check(!QDir(developer_user).exists());
    qunsetenv("OOT3D_USER_PATH");
    // AppImage data belongs beside the distributed image, not its mount point.
    qputenv("APPIMAGE", (root + "/release folder/SariaUI.AppImage").toUtf8());
    Check(OoT3D::GameFolder::User() == root + "/release folder/user");
    qunsetenv("APPIMAGE");
    Check(OoT3D::GameFolder::User() == portable);
    const QStringList launch{QStringLiteral("SariaUI")};
    using OoT3D::GameFolder::Resolve;
    Check(Resolve(root, launch).isEmpty());
    const auto game = root + QStringLiteral("/my game.CCI");
    QFile file(game);
    Check(file.open(QIODevice::WriteOnly));
    file.close();
    Check(Resolve(root, launch) == game);
    Check(Resolve(root, {launch.front(), "--fullscreen"}) == game);
    Check(Resolve(root, {launch.front(), "--movie-play", "movie.cci"}) == game);
    Check(Resolve(root, {launch.front(), "--windowed", game}) == game);
    Check(Resolve(root, {launch.front(), root + "/missing.cci"}).isEmpty());
    const auto other = root + QStringLiteral("/another.3ds");
    QFile second(other);
    Check(second.open(QIODevice::WriteOnly));
    second.close();
    Check(Resolve(root, launch).isEmpty());
    Check(Resolve(root, {launch.front(), game}) == game);
    Check(second.remove());
    Check(QDir(root).mkdir("folder.3ds"));
    Check(Resolve(root, launch) == game);
    Check(file.remove());
    Check(Resolve(root, launch).isEmpty());
    std::cout << "Game-folder checks passed\n";
}

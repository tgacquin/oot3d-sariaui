// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QFileInfo>
#include <QString>

namespace OOT3D::ReleaseDefaults {
// Authored geometry is a product asset, even though the editor saves it outside
// its INI. Read the embedded baseline when no local edit exists; never write to
// the user's configuration just to supply the default artwork boundaries.
inline QString PresentationAsset(const QString& local_path) {
    const QFileInfo local(local_path);
    if (local.exists()) return local_path;
    return QStringLiteral(":/oot3d/defaults/") + local.fileName();
}
}

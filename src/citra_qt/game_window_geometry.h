// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <algorithm>
#include <QGuiApplication>
#include <QScreen>
#include <QWidget>

namespace GameWindowGeometry {
inline QRect Centered(const QRect& available) {
    const int unit = std::max(1, std::min(available.width() * 4 / 5 / 16,
                                          available.height() * 4 / 5 / 9));
    QRect bounds(QPoint{}, QSize(16 * unit, 9 * unit));
    bounds.moveCenter(available.center());
    return bounds;
}

inline void RestoreWindowed(QWidget& window, const QByteArray& saved) {
    if (!saved.isEmpty() && window.restoreGeometry(saved) && !window.isFullScreen() &&
        !window.isMinimized()) {
        const QRect bounds = window.isMaximized() ? window.normalGeometry() : window.frameGeometry();
        for (const auto* screen : QGuiApplication::screens()) {
            if (bounds.isValid() && screen->availableGeometry().contains(bounds))
                return;
        }
    }
    window.setWindowState(Qt::WindowNoState);
    window.setGeometry(Centered(window.screen()->availableGeometry()));
}
} // namespace GameWindowGeometry

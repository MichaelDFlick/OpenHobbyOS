// Copyright (C) 2025 The OpenHobbyOS Project
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QOPENHOBBYOSWINDOW_H
#define QOPENHOBBYOSWINDOW_H

#include <qpa/qplatformwindow.h>
#include <QtCore/QRect>
#include <QtCore/QString>
#include <cstdint>

QT_BEGIN_NAMESPACE

class QOpenHobbyOSWindow : public QPlatformWindow
{
public:
    explicit QOpenHobbyOSWindow(QWindow *window);
    ~QOpenHobbyOSWindow();

    void setVisible(bool visible) override;
    void setGeometry(const QRect &rect) override;
    void setWindowTitle(const QString &title) override;
    void setWindowState(Qt::WindowStates state) override;
    void setWindowFlags(Qt::WindowFlags flags) override;
    void propagateSizeHints() override;
    void setOpacity(qreal level) override;
    void requestActivateWindow() override;

    uint32_t xnxSurfaceId() const { return m_surfaceId; }
    bool hasXnxSurface() const { return m_surfaceId != 0; }

private:
    bool createXnxSurface();
    void destroyXnxSurface();

    uint32_t m_surfaceId;
    QString m_windowTitle;
    bool m_visible;
    bool m_surfaceCreated;
};

QT_END_NAMESPACE

#endif // QOPENHOBBYOSWINDOW_H

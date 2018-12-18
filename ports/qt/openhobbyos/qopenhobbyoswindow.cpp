// Copyright (C) 2025 The OpenHobbyOS Project
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qopenhobbyoswindow.h"
#include "qopenhobbyosintegration.h"
#include "qopenhobbyosscreen.h"

#include <QtCore/QDebug>
#include <qpa/qwindowsysteminterface.h>

QT_BEGIN_NAMESPACE

QOpenHobbyOSWindow::QOpenHobbyOSWindow(QWindow *window)
    : QPlatformWindow(window)
    , m_surfaceId(0)
    , m_visible(false)
    , m_surfaceCreated(false)
{
}

QOpenHobbyOSWindow::~QOpenHobbyOSWindow()
{
    if (m_surfaceCreated)
        destroyXnxSurface();
}

void QOpenHobbyOSWindow::setVisible(bool visible)
{
    QOpenHobbyOSIntegration *integration = QOpenHobbyOSIntegration::instance();
    if (!integration || !integration->xnxConnection())
        return;

    m_visible = visible;

    if (visible && !m_surfaceCreated) {
        createXnxSurface();
    }

    if (!visible && m_surfaceCreated) {
        destroyXnxSurface();
    }

    QPlatformWindow::setVisible(visible);
}

void QOpenHobbyOSWindow::setGeometry(const QRect &rect)
{
    QPlatformWindow::setGeometry(rect);

    QOpenHobbyOSIntegration *integration = QOpenHobbyOSIntegration::instance();
    if (m_surfaceCreated && integration && integration->xnxConnection()) {
        xnx_set_geometry(
            integration->xnxConnection(),
            m_surfaceId,
            rect.x(), rect.y(),
            static_cast<uint32_t>(rect.width()),
            static_cast<uint32_t>(rect.height())
        );
    }
}

void QOpenHobbyOSWindow::setWindowTitle(const QString &title)
{
    m_windowTitle = title;

    if (!m_surfaceCreated)
        return;

    QOpenHobbyOSIntegration *integration = QOpenHobbyOSIntegration::instance();
    if (integration && integration->xnxConnection()) {
        QByteArray titleUtf8 = title.toUtf8();
        xnx_set_title(integration->xnxConnection(), m_surfaceId, titleUtf8.constData());
    }
}

void QOpenHobbyOSWindow::setWindowState(Qt::WindowStates state)
{
    Q_UNUSED(state);
    QPlatformWindow::setWindowState(state);
}

void QOpenHobbyOSWindow::setWindowFlags(Qt::WindowFlags flags)
{
    Q_UNUSED(flags);
    QPlatformWindow::setWindowFlags(flags);
}

void QOpenHobbyOSWindow::propagateSizeHints()
{
    QPlatformWindow::propagateSizeHints();
}

void QOpenHobbyOSWindow::setOpacity(qreal level)
{
    Q_UNUSED(level);
}

void QOpenHobbyOSWindow::requestActivateWindow()
{
    QPlatformWindow::requestActivateWindow();
}

bool QOpenHobbyOSWindow::createXnxSurface()
{
    QOpenHobbyOSIntegration *integration = QOpenHobbyOSIntegration::instance();
    if (!integration || !integration->xnxConnection()) {
        qWarning("QOpenHobbyOSWindow: No XNX connection available");
        return false;
    }

    QRect geom = geometry();
    uint32_t w = static_cast<uint32_t>(qMax(geom.width(), 1));
    uint32_t h = static_cast<uint32_t>(qMax(geom.height(), 1));

    if (xnx_create_surface(integration->xnxConnection(), w, h, &m_surfaceId) < 0) {
        qWarning("QOpenHobbyOSWindow: Failed to create XNX surface");
        return false;
    }

    m_surfaceCreated = true;

    if (!m_windowTitle.isEmpty()) {
        QByteArray titleUtf8 = m_windowTitle.toUtf8();
        xnx_set_title(integration->xnxConnection(), m_surfaceId, titleUtf8.constData());
    }

    xnx_set_geometry(integration->xnxConnection(), m_surfaceId,
                     geom.x(), geom.y(), w, h);

    return true;
}

void QOpenHobbyOSWindow::destroyXnxSurface()
{
    QOpenHobbyOSIntegration *integration = QOpenHobbyOSIntegration::instance();
    if (!integration || !integration->xnxConnection())
        return;

    xnx_destroy_surface(integration->xnxConnection(), m_surfaceId);
    m_surfaceId = 0;
    m_surfaceCreated = false;
}

QT_END_NAMESPACE

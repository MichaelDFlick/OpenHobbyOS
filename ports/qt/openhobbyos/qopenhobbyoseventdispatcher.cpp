// Copyright (C) 2025 The OpenHobbyOS Project
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qopenhobbyoseventdispatcher.h"
#include "qopenhobbyosintegration.h"
#include "qopenhobbyoswindow.h"
#include "qopenhobbyoscursor.h"

#include <QtCore/QDebug>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <qpa/qwindowsysteminterface.h>

#include <poll.h>
#include <errno.h>

QT_BEGIN_NAMESPACE

static QWindow *windowForSurfaceId(uint32_t surfaceId)
{
    const auto topLevels = QGuiApplication::topLevelWindows();
    for (QWindow *w : topLevels) {
        QOpenHobbyOSWindow *ohosWin = static_cast<QOpenHobbyOSWindow *>(w->handle());
        if (ohosWin && ohosWin->xnxSurfaceId() == surfaceId)
            return w;
    }
    return nullptr;
}

QOpenHobbyOSEventDispatcher::QOpenHobbyOSEventDispatcher(QObject *parent)
    : QUnixEventDispatcherQPA(parent)
{
}

QOpenHobbyOSEventDispatcher::~QOpenHobbyOSEventDispatcher()
{
}

bool QOpenHobbyOSEventDispatcher::processEvents(QEventLoop::ProcessEventsFlags flags)
{
    bool processed = QUnixEventDispatcherQPA::processEvents(flags);

    if (flags & QEventLoop::ExcludeUserInputEvents)
        return processed;

    QOpenHobbyOSIntegration *integration = QOpenHobbyOSIntegration::instance();
    if (!integration || !integration->xnxConnection())
        return processed;

    xnx_conn_t *conn = integration->xnxConnection();
    int xnxFd = xnx_get_fd(conn);
    if (xnxFd < 0)
        return processed;

    struct pollfd pfd;
    pfd.fd = xnxFd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    int pollResult = poll(&pfd, 1, 0);
    if (pollResult < 0) {
        if (errno == EINTR)
            return processed;
        return processed;
    }

    if (pollResult == 0 || !(pfd.revents & POLLIN))
        return processed;

    struct xnx_event event;
    int eventResult = xnx_poll_event(conn, &event);
    while (eventResult > 0) {
        if (event.type == XNX_EVENT_KEY) {
            Qt::Key qtKey = static_cast<Qt::Key>(event.data.key.keycode);
            bool pressed = event.data.key.pressed != 0;
            uint32_t surfaceId = event.data.key.surface_id;

            QWindow *targetWindow = windowForSurfaceId(surfaceId);
            if (!targetWindow)
                targetWindow = QGuiApplication::focusWindow();
            if (!targetWindow && !QGuiApplication::topLevelWindows().isEmpty())
                targetWindow = QGuiApplication::topLevelWindows().first();

            if (targetWindow && pressed) {
                QWindowSystemInterface::handleKeyEvent(targetWindow,
                    QEvent::KeyPress, qtKey,
                    Qt::NoModifier);
            } else if (targetWindow && !pressed) {
                QWindowSystemInterface::handleKeyEvent(targetWindow,
                    QEvent::KeyRelease, qtKey,
                    Qt::NoModifier);
            }

            processed = true;
        } else if (event.type == XNX_EVENT_POINTER) {
            uint32_t surfaceId = event.data.pointer.surface_id;
            int localX = event.data.pointer.x;
            int localY = event.data.pointer.y;
            uint8_t buttons = event.data.pointer.buttons;

            QOpenHobbyOSCursor::pointerEvent(QPoint(localX, localY));

            QWindow *targetWindow = windowForSurfaceId(surfaceId);
            if (!targetWindow && !QGuiApplication::topLevelWindows().isEmpty())
                targetWindow = QGuiApplication::topLevelWindows().first();

            if (targetWindow) {
                Qt::MouseButtons qtButtons = Qt::NoButton;
                if (buttons & 0x01) qtButtons |= Qt::LeftButton;
                if (buttons & 0x02) qtButtons |= Qt::RightButton;
                if (buttons & 0x04) qtButtons |= Qt::MiddleButton;

                QWindowSystemInterface::handleMouseEvent(targetWindow, localX, localY, localX, localY, qtButtons, Qt::NoButton, QEvent::MouseMove);
            }

            processed = true;
        }

        eventResult = xnx_poll_event(conn, &event);
    }

    return processed;
}

QT_END_NAMESPACE

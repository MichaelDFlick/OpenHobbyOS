// Copyright (C) 2025 The OpenHobbyOS Project
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qopenhobbyoscursor.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QWindow>
#include <QtGui/private/qhighdpiscaling_p.h>

QT_BEGIN_NAMESPACE

static int g_cursorX = 512;
static int g_cursorY = 384;

QOpenHobbyOSCursor::QOpenHobbyOSCursor()
    : QPlatformCursor()
{
}

void QOpenHobbyOSCursor::changeCursor(QCursor *cursor, QWindow *window)
{
    Q_UNUSED(cursor);
    Q_UNUSED(window);
}

QPoint QOpenHobbyOSCursor::pos() const
{
    return QPoint(g_cursorX, g_cursorY);
}

void QOpenHobbyOSCursor::setPos(const QPoint &pos)
{
    g_cursorX = pos.x();
    g_cursorY = pos.y();
}

void QOpenHobbyOSCursor::pointerEvent(const QPoint &pos)
{
    g_cursorX = pos.x();
    g_cursorY = pos.y();
}

QT_END_NAMESPACE

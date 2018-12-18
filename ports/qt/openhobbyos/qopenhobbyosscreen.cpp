// Copyright (C) 2025 The OpenHobbyOS Project
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qopenhobbyosscreen.h"
#include "qopenhobbyoscursor.h"
#include "qopenhobbyoswindow.h"

#include <QtCore/QDebug>
#include <qpa/qwindowsysteminterface.h>

QT_BEGIN_NAMESPACE

QOpenHobbyOSScreen::QOpenHobbyOSScreen(uint32_t width, uint32_t height)
    : QObject()
    , QPlatformScreen()
    , mDisplayWidth(width)
    , mDisplayHeight(height)
    , mDepth(32)
    , mFormat(QImage::Format_ARGB32_Premultiplied)
    , mCursor(nullptr)
{
}

QOpenHobbyOSScreen::~QOpenHobbyOSScreen()
{
}

bool QOpenHobbyOSScreen::initialize()
{
    mGeometry = QRect(0, 0, static_cast<int>(mDisplayWidth), static_cast<int>(mDisplayHeight));

    const int dpi = 100;
    mPhysicalSize = QSizeF(
        mDisplayWidth * 25.4 / dpi,
        mDisplayHeight * 25.4 / dpi
    );

    mCursor = new QOpenHobbyOSCursor();

    return true;
}

QWindow *QOpenHobbyOSScreen::topLevelAt(const QPoint &p) const
{
    const QList<QWindow *> windows = QGuiApplication::topLevelWindows();
    for (int i = windows.size() - 1; i >= 0; --i) {
        QWindow *w = windows.at(i);
        if (w->isVisible() && w->geometry().contains(p))
            return w;
    }
    return nullptr;
}

QPixmap QOpenHobbyOSScreen::grabWindow(WId wid, int x, int y, int width, int height) const
{
    Q_UNUSED(wid);
    Q_UNUSED(x);
    Q_UNUSED(y);
    Q_UNUSED(width);
    Q_UNUSED(height);
    return QPixmap();
}

QT_END_NAMESPACE

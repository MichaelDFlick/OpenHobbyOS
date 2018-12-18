// Copyright (C) 2025 The OpenHobbyOS Project
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QOPENHOBBYOSSCREEN_H
#define QOPENHOBBYOSSCREEN_H

#include <qpa/qplatformscreen.h>
#include <QtGui/QImage>
#include "qopenhobbyoscursor.h"

QT_BEGIN_NAMESPACE

class QOpenHobbyOSScreen : public QObject, public QPlatformScreen
{
    Q_OBJECT

public:
    explicit QOpenHobbyOSScreen(uint32_t width, uint32_t height);
    ~QOpenHobbyOSScreen();

    bool initialize();

    QRect geometry() const override { return mGeometry; }
    int depth() const override { return mDepth; }
    QImage::Format format() const override { return mFormat; }
    QSizeF physicalSize() const override { return mPhysicalSize; }
    QString name() const override { return QStringLiteral("OpenHobbyOS Display"); }

    QPlatformCursor *cursor() const override { return mCursor; }
    QWindow *topLevelAt(const QPoint &p) const override;

    QPixmap grabWindow(WId wid, int x, int y, int width, int height) const override;

    uint32_t displayWidth() const { return mDisplayWidth; }
    uint32_t displayHeight() const { return mDisplayHeight; }

private:
    uint32_t mDisplayWidth;
    uint32_t mDisplayHeight;
    QRect mGeometry;
    int mDepth;
    QImage::Format mFormat;
    QSizeF mPhysicalSize;
    QOpenHobbyOSCursor *mCursor;
};

QT_END_NAMESPACE

#endif // QOPENHOBBYOSSCREEN_H

// Copyright (C) 2025 The OpenHobbyOS Project
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QOPENHOBBYOSCURSOR_H
#define QOPENHOBBYOSCURSOR_H

#include <qpa/qplatformcursor.h>

QT_BEGIN_NAMESPACE

class QOpenHobbyOSCursor : public QPlatformCursor
{
public:
    QOpenHobbyOSCursor();

    void changeCursor(QCursor *cursor, QWindow *window) override;
    QPoint pos() const override;
    void setPos(const QPoint &pos) override;

    static void pointerEvent(const QPoint &pos);
};

QT_END_NAMESPACE

#endif // QOPENHOBBYOSCURSOR_H

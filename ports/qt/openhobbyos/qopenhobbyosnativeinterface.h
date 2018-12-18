// Copyright (C) 2025 The OpenHobbyOS Project
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QOPENHOBBYOSNATIVEINTERFACE_H
#define QOPENHOBBYOSNATIVEINTERFACE_H

#include <QtCore/QByteArray>
#include <qpa/qplatformnativeinterface.h>

QT_BEGIN_NAMESPACE

class QOpenHobbyOSNativeInterface : public QPlatformNativeInterface
{
public:
    QOpenHobbyOSNativeInterface();

    void *nativeResourceForWindow(const QByteArray &resource, QWindow *window) override;
    QFunctionPointer platformFunction(const QByteArray &function) const override;
};

QT_END_NAMESPACE

#endif // QOPENHOBBYOSNATIVEINTERFACE_H

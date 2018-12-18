// Copyright (C) 2025 The OpenHobbyOS Project
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qopenhobbyosnativeinterface.h"

#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

QOpenHobbyOSNativeInterface::QOpenHobbyOSNativeInterface()
    : QPlatformNativeInterface()
{
}

void *QOpenHobbyOSNativeInterface::nativeResourceForWindow(const QByteArray &resource, QWindow *window)
{
    Q_UNUSED(resource);
    Q_UNUSED(window);
    return nullptr;
}

QFunctionPointer QOpenHobbyOSNativeInterface::platformFunction(const QByteArray &function) const
{
    Q_UNUSED(function);
    return nullptr;
}

QT_END_NAMESPACE

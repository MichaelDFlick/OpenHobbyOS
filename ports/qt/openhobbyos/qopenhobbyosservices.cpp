// Copyright (C) 2025 The OpenHobbyOS Project
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qopenhobbyosservices.h"

#include <QtCore/QUrl>
#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

QOpenHobbyOSServices::QOpenHobbyOSServices()
    : QPlatformServices()
{
}

bool QOpenHobbyOSServices::hasCapability(Capability capability) const
{
    Q_UNUSED(capability);
    return false;
}

bool QOpenHobbyOSServices::openUrl(const QUrl &url)
{
    Q_UNUSED(url);
    return false;
}

bool QOpenHobbyOSServices::openDocument(const QUrl &url)
{
    Q_UNUSED(url);
    return false;
}

QByteArray QOpenHobbyOSServices::desktopEnvironment() const
{
    return QByteArray("OpenHobbyOS");
}

QT_END_NAMESPACE

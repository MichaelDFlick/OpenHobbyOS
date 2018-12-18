// Copyright (C) 2025 The OpenHobbyOS Project
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QOPENHOBBYOSSERVICES_H
#define QOPENHOBBYOSSERVICES_H

#include <qpa/qplatformservices.h>

QT_BEGIN_NAMESPACE

class QOpenHobbyOSServices : public QPlatformServices
{
public:
    QOpenHobbyOSServices();

    bool hasCapability(Capability capability) const override;
    bool openUrl(const QUrl &url) override;
    bool openDocument(const QUrl &url) override;
    QByteArray desktopEnvironment() const override;
};

QT_END_NAMESPACE

#endif // QOPENHOBBYOSSERVICES_H

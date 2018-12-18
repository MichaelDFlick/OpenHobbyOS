// Copyright (C) 2025 The OpenHobbyOS Project
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <qpa/qplatformintegrationplugin.h>
#include "qopenhobbyosintegration.h"

QT_BEGIN_NAMESPACE

using namespace Qt::StringLiterals;

class QOpenHobbyOSIntegrationPlugin : public QPlatformIntegrationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QPlatformIntegrationFactoryInterface_iid FILE "openhobbyos.json")
public:
    QPlatformIntegration *create(const QString&, const QStringList&) override;
};

QPlatformIntegration* QOpenHobbyOSIntegrationPlugin::create(const QString& system, const QStringList& paramList)
{
    if (!system.compare("openhobbyos"_L1, Qt::CaseInsensitive))
        return new QOpenHobbyOSIntegration(paramList);

    return nullptr;
}

QT_END_NAMESPACE

#include "main.moc"

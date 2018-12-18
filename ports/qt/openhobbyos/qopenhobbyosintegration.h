// Copyright (C) 2025 The OpenHobbyOS Project
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QOPENHOBBYOSINTEGRATION_H
#define QOPENHOBBYOSINTEGRATION_H

#include <qpa/qplatformintegration.h>
#include <qpa/qplatformnativeinterface.h>

#include <QtCore/QScopedPointer>
#include <QtCore/QList>

#include "qopenhobbyosscreen.h"
#include "qopenhobbyoswindow.h"
#include "xnx.h"

QT_BEGIN_NAMESPACE

class QAbstractEventDispatcher;
class QOpenHobbyOSEventDispatcher;
class QOpenHobbyOSFontDatabase;
class QOpenHobbyOSServices;
class QOpenHobbyOSNativeInterface;
class QOpenHobbyOSClipboard;
class QOpenHobbyOSInputContext;

class QOpenHobbyOSIntegration : public QPlatformIntegration
{
public:
    QOpenHobbyOSIntegration(const QStringList &paramList);
    ~QOpenHobbyOSIntegration();

    void initialize() override;
    bool hasCapability(QPlatformIntegration::Capability cap) const override;

    QPlatformWindow *createPlatformWindow(QWindow *window) const override;
    QPlatformBackingStore *createPlatformBackingStore(QWindow *window) const override;
    QAbstractEventDispatcher *createEventDispatcher() const override;

    QPlatformFontDatabase *fontDatabase() const override;
    QPlatformServices *services() const override;
    QPlatformInputContext *inputContext() const override;
    QPlatformNativeInterface *nativeInterface() const override;

    QList<QPlatformScreen *> screens() const;

    xnx_conn_t *xnxConnection() const { return m_xnxConn; }
    static QOpenHobbyOSIntegration *instance();

private:
    xnx_conn_t *m_xnxConn;
    bool m_xnxConnected;
    QOpenHobbyOSScreen *m_primaryScreen;
    QOpenHobbyOSInputContext *m_inputContext;
    QScopedPointer<QOpenHobbyOSFontDatabase> m_fontDb;
    mutable QScopedPointer<QOpenHobbyOSServices> m_services;
    QScopedPointer<QOpenHobbyOSNativeInterface> m_nativeInterface;
    QScopedPointer<QOpenHobbyOSClipboard> m_clipboard;
};

QT_END_NAMESPACE

#endif // QOPENHOBBYOSINTEGRATION_H

// Copyright (C) 2025 The OpenHobbyOS Project
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qopenhobbyosintegration.h"
#include "qopenhobbyosbackingstore.h"
#include "qopenhobbyoscursor.h"
#include "qopenhobbyoseventdispatcher.h"
#include "qopenhobbyosfontdatabase.h"
#include "qopenhobbyosinputcontext.h"
#include "qopenhobbyosnativeinterface.h"
#include "qopenhobbyosservices.h"
#include "qopenhobbyosclipboard.h"

#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/private/qgenericunixfontdatabase_p.h>
#include <QtGui/private/qgenericunixeventdispatcher_p.h>
#include <qpa/qplatforminputcontextfactory_p.h>
#include <qpa/qwindowsysteminterface.h>


#include <QDebug>

QT_BEGIN_NAMESPACE

QOpenHobbyOSIntegration *QOpenHobbyOSIntegration::instance()
{
    return static_cast<QOpenHobbyOSIntegration *>(QGuiApplicationPrivate::platformIntegration());
}

QOpenHobbyOSIntegration::QOpenHobbyOSIntegration(const QStringList &paramList)
    : m_xnxConn(nullptr)
    , m_xnxConnected(false)
    , m_primaryScreen(nullptr)
    , m_inputContext(nullptr)
{
    Q_UNUSED(paramList);

    m_nativeInterface.reset(new QOpenHobbyOSNativeInterface());
    m_clipboard.reset(new QOpenHobbyOSClipboard());
    m_fontDb.reset(new QOpenHobbyOSFontDatabase());

    m_xnxConn = xnx_connect("/tmp/xnx.sock");
    if (!m_xnxConn) {
        qWarning("QOpenHobbyOSIntegration: Failed to connect to XNX compositor");
        m_xnxConnected = false;
        return;
    }
    m_xnxConnected = true;

    uint32_t dispWidth = 0, dispHeight = 0;
    if (xnx_get_display_info(m_xnxConn, &dispWidth, &dispHeight) < 0) {
        qWarning("QOpenHobbyOSIntegration: Failed to get display info from XNX");
    }

    if (dispWidth == 0 || dispHeight == 0) {
        dispWidth = 1024;
        dispHeight = 768;
    }

    m_primaryScreen = new QOpenHobbyOSScreen(dispWidth, dispHeight);
}

QOpenHobbyOSIntegration::~QOpenHobbyOSIntegration()
{
    if (m_primaryScreen)
        QWindowSystemInterface::handleScreenRemoved(m_primaryScreen);

    if (m_xnxConn)
        xnx_disconnect(m_xnxConn);
}

void QOpenHobbyOSIntegration::initialize()
{
    if (m_primaryScreen) {
        m_primaryScreen->initialize();
        QWindowSystemInterface::handleScreenAdded(m_primaryScreen);
    }

    const auto requestedInputContext = QPlatformInputContextFactory::requested();
    if (requestedInputContext.isEmpty()) {
        m_inputContext = new QOpenHobbyOSInputContext();
    } else {
        m_inputContext = static_cast<QOpenHobbyOSInputContext *>(
            QPlatformInputContextFactory::create(requestedInputContext));
    }
}

bool QOpenHobbyOSIntegration::hasCapability(QPlatformIntegration::Capability cap) const
{
    switch (cap) {
    case ThreadedPixmaps: return true;
    case MultipleWindows: return true;
    case WindowManagement: return true;
    case RhiBasedRendering: return false;
    default: return QPlatformIntegration::hasCapability(cap);
    }
}

QPlatformWindow *QOpenHobbyOSIntegration::createPlatformWindow(QWindow *window) const
{
    return new QOpenHobbyOSWindow(window);
}

QPlatformBackingStore *QOpenHobbyOSIntegration::createPlatformBackingStore(QWindow *window) const
{
    return new QOpenHobbyOSBackingStore(window);
}

QAbstractEventDispatcher *QOpenHobbyOSIntegration::createEventDispatcher() const
{
    return new QOpenHobbyOSEventDispatcher();
}

QPlatformFontDatabase *QOpenHobbyOSIntegration::fontDatabase() const
{
    return m_fontDb.data();
}

QPlatformServices *QOpenHobbyOSIntegration::services() const
{
    if (m_services.isNull())
        m_services.reset(new QOpenHobbyOSServices());
    return m_services.data();
}

QPlatformInputContext *QOpenHobbyOSIntegration::inputContext() const
{
    return m_inputContext;
}

QPlatformNativeInterface *QOpenHobbyOSIntegration::nativeInterface() const
{
    return m_nativeInterface.data();
}

QList<QPlatformScreen *> QOpenHobbyOSIntegration::screens() const
{
    QList<QPlatformScreen *> list;
    if (m_primaryScreen)
        list.append(m_primaryScreen);
    return list;
}

QT_END_NAMESPACE

// Copyright (C) 2025 The OpenHobbyOS Project
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QOPENHOBBYOSEVENTDISPATCHER_H
#define QOPENHOBBYOSEVENTDISPATCHER_H

#include <QtCore/QObject>
#include <QtGui/private/qunixeventdispatcher_qpa_p.h>

QT_BEGIN_NAMESPACE

class QOpenHobbyOSEventDispatcher : public QUnixEventDispatcherQPA
{
    Q_OBJECT

public:
    explicit QOpenHobbyOSEventDispatcher(QObject *parent = nullptr);
    ~QOpenHobbyOSEventDispatcher();

protected:
    bool processEvents(QEventLoop::ProcessEventsFlags flags) override;
};

QT_END_NAMESPACE

#endif // QOPENHOBBYOSEVENTDISPATCHER_H

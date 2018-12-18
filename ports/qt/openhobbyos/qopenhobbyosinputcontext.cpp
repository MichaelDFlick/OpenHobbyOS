// Copyright (C) 2025 The OpenHobbyOS Project
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qopenhobbyosinputcontext.h"

#include <QtCore/QDebug>
#include <qpa/qwindowsysteminterface.h>

QT_BEGIN_NAMESPACE

QOpenHobbyOSInputContext::QOpenHobbyOSInputContext()
    : QPlatformInputContext()
{
}

QOpenHobbyOSInputContext::~QOpenHobbyOSInputContext()
{
}

void QOpenHobbyOSInputContext::reset()
{
}

void QOpenHobbyOSInputContext::commit()
{
}

void QOpenHobbyOSInputContext::update(Qt::InputMethodQueries queries)
{
    Q_UNUSED(queries);
}

void QOpenHobbyOSInputContext::invokeAction(QInputMethod::Action action, int cursorPosition)
{
    Q_UNUSED(action);
    Q_UNUSED(cursorPosition);
}

QRectF QOpenHobbyOSInputContext::keyboardRect() const
{
    return QRectF();
}

bool QOpenHobbyOSInputContext::isAnimating() const
{
    return false;
}

void QOpenHobbyOSInputContext::showInputPanel()
{
}

void QOpenHobbyOSInputContext::hideInputPanel()
{
}

bool QOpenHobbyOSInputContext::isInputPanelVisible() const
{
    return false;
}

void QOpenHobbyOSInputContext::setFocusObject(QObject *object)
{
    Q_UNUSED(object);
    QPlatformInputContext::setFocusObject(object);
}

QT_END_NAMESPACE

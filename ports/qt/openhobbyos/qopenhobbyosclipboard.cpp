// Copyright (C) 2025 The OpenHobbyOS Project
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qopenhobbyosclipboard.h"

#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

QOpenHobbyOSClipboard::QOpenHobbyOSClipboard()
    : QPlatformClipboard()
    , m_clipboardData(nullptr)
    , m_ownsClipboard(false)
{
}

QOpenHobbyOSClipboard::~QOpenHobbyOSClipboard()
{
    if (m_clipboardData && m_ownsClipboard)
        delete m_clipboardData;
}

QMimeData *QOpenHobbyOSClipboard::mimeData(QClipboard::Mode mode)
{
    if (mode != QClipboard::Clipboard)
        return nullptr;
    return m_clipboardData;
}

void QOpenHobbyOSClipboard::setMimeData(QMimeData *data, QClipboard::Mode mode)
{
    if (mode != QClipboard::Clipboard)
        return;

    if (m_clipboardData && m_ownsClipboard)
        delete m_clipboardData;

    m_clipboardData = data;
    m_ownsClipboard = (data != nullptr);
}

bool QOpenHobbyOSClipboard::supportsMode(QClipboard::Mode mode) const
{
    return mode == QClipboard::Clipboard;
}

bool QOpenHobbyOSClipboard::ownsMode(QClipboard::Mode mode) const
{
    return mode == QClipboard::Clipboard && m_ownsClipboard;
}

QT_END_NAMESPACE

// Copyright (C) 2025 The OpenHobbyOS Project
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QOPENHOBBYOSCLIPBOARD_H
#define QOPENHOBBYOSCLIPBOARD_H

#include <qpa/qplatformclipboard.h>
#include <QtCore/QMimeData>

QT_BEGIN_NAMESPACE

class QOpenHobbyOSClipboard : public QPlatformClipboard
{
public:
    QOpenHobbyOSClipboard();
    ~QOpenHobbyOSClipboard();

    QMimeData *mimeData(QClipboard::Mode mode = QClipboard::Clipboard) override;
    void setMimeData(QMimeData *data, QClipboard::Mode mode = QClipboard::Clipboard) override;
    bool supportsMode(QClipboard::Mode mode) const override;
    bool ownsMode(QClipboard::Mode mode) const override;

private:
    QMimeData *m_clipboardData;
    bool m_ownsClipboard;
};

QT_END_NAMESPACE

#endif // QOPENHOBBYOSCLIPBOARD_H

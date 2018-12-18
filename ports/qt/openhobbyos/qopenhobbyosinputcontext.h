// Copyright (C) 2025 The OpenHobbyOS Project
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QOPENHOBBYOSINPUTCONTEXT_H
#define QOPENHOBBYOSINPUTCONTEXT_H

#include <qpa/qplatforminputcontext.h>
#include <QtCore/QObject>

QT_BEGIN_NAMESPACE

class QOpenHobbyOSInputContext : public QPlatformInputContext
{
    Q_OBJECT

public:
    QOpenHobbyOSInputContext();
    ~QOpenHobbyOSInputContext();

    bool isValid() const override { return true; }
    void reset() override;
    void commit() override;
    void update(Qt::InputMethodQueries queries) override;
    void invokeAction(QInputMethod::Action action, int cursorPosition) override;
    QRectF keyboardRect() const override;
    bool isAnimating() const override;
    void showInputPanel() override;
    void hideInputPanel() override;
    bool isInputPanelVisible() const override;
    void setFocusObject(QObject *object) override;
};

QT_END_NAMESPACE

#endif // QOPENHOBBYOSINPUTCONTEXT_H

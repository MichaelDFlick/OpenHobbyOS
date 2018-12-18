// Copyright (C) 2025 The OpenHobbyOS Project
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QOPENHOBBYOSFONTDATABASE_H
#define QOPENHOBBYOSFONTDATABASE_H

#include <QtGui/private/qgenericunixfontdatabase_p.h>

QT_BEGIN_NAMESPACE

class QOpenHobbyOSFontDatabase : public QGenericUnixFontDatabase
{
public:
    QOpenHobbyOSFontDatabase();
    ~QOpenHobbyOSFontDatabase();

    void populateFontDatabase() override;
    QStringList fallbacksForFamily(const QString &family,
                                   QFont::Style style,
                                   QFont::StyleHint styleHint,
                                   QFontDatabasePrivate::ExtendedScript script) const override;
    QFont defaultFont() const override;
};

QT_END_NAMESPACE

#endif // QOPENHOBBYOSFONTDATABASE_H

// Copyright (C) 2025 The OpenHobbyOS Project
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qopenhobbyosfontdatabase.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

QOpenHobbyOSFontDatabase::QOpenHobbyOSFontDatabase()
    : QGenericUnixFontDatabase()
{
}

QOpenHobbyOSFontDatabase::~QOpenHobbyOSFontDatabase()
{
}

void QOpenHobbyOSFontDatabase::populateFontDatabase()
{
    QGenericUnixFontDatabase::populateFontDatabase();
}

QStringList QOpenHobbyOSFontDatabase::fallbacksForFamily(
    const QString &family,
    QFont::Style style,
    QFont::StyleHint styleHint,
    QFontDatabasePrivate::ExtendedScript script) const
{
    return QGenericUnixFontDatabase::fallbacksForFamily(family, style, styleHint, script);
}

QFont QOpenHobbyOSFontDatabase::defaultFont() const
{
    return QGenericUnixFontDatabase::defaultFont();
}

QT_END_NAMESPACE

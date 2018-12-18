// Copyright (C) 2025 The OpenHobbyOS Project
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QOPENHOBBYOSBACKINGSTORE_H
#define QOPENHOBBYOSBACKINGSTORE_H

#include <QtGui/qimage.h>
#include <qpa/qplatformbackingstore.h>
#include <QtGui/private/qrasterbackingstore_p.h>

#include <pixman.h>

QT_BEGIN_NAMESPACE

class QOpenHobbyOSBackingStore : public QRasterBackingStore
{
public:
    explicit QOpenHobbyOSBackingStore(QWindow *window);
    ~QOpenHobbyOSBackingStore();

    void resize(const QSize &size, const QRegion &staticContents) override;
    void flush(QWindow *window, const QRegion &region, const QPoint &offset) override;
    QImage::Format format() const override;
    bool scroll(const QRegion &area, int dx, int dy) override;

private:
    void flushToXnx(const QImage &image, const QRegion &region, const QPoint &offset, QWindow *window);
    pixman_image_t *createPixmanImage(const QImage &image) const;

    QImage m_sourceImage;
    pixman_image_t *m_pixmanImage;
};

QT_END_NAMESPACE

#endif // QOPENHOBBYOSBACKINGSTORE_H

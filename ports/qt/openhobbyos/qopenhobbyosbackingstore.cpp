// Copyright (C) 2025 The OpenHobbyOS Project
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qopenhobbyosbackingstore.h"
#include "qopenhobbyosintegration.h"
#include "qopenhobbyoswindow.h"
#include "qopenhobbyosscreen.h"

#include <QtGui/QPainter>
#include <QtCore/QDebug>
#include <QtCore/QVarLengthArray>
#include <qpa/qwindowsysteminterface.h>

#include <cstring>

QT_BEGIN_NAMESPACE

QOpenHobbyOSBackingStore::QOpenHobbyOSBackingStore(QWindow *window)
    : QRasterBackingStore(window)
    , m_pixmanImage(nullptr)
{
}

QOpenHobbyOSBackingStore::~QOpenHobbyOSBackingStore()
{
    if (m_pixmanImage)
        pixman_image_unref(m_pixmanImage);
}

void QOpenHobbyOSBackingStore::resize(const QSize &size, const QRegion &staticContents)
{
    Q_UNUSED(staticContents);

    QImage::Format fmt = format();
    if (fmt == QImage::Format_Invalid)
        fmt = QImage::Format_ARGB32_Premultiplied;

    int w = qMax(size.width(), 1);
    int h = qMax(size.height(), 1);

    m_sourceImage = QImage(w, h, fmt);

    if (m_pixmanImage)
        pixman_image_unref(m_pixmanImage);
    m_pixmanImage = createPixmanImage(m_sourceImage);

    QRasterBackingStore::resize(size, staticContents);
}

void QOpenHobbyOSBackingStore::flush(QWindow *window, const QRegion &region, const QPoint &offset)
{
    if (!window || region.isEmpty())
        return;

    Q_UNUSED(offset);

    QOpenHobbyOSWindow *ohosWindow = static_cast<QOpenHobbyOSWindow *>(window->handle());
    if (!ohosWindow || !ohosWindow->hasXnxSurface())
        return;

    QImage image = m_sourceImage;
    if (image.isNull())
        return;

    QOpenHobbyOSIntegration *integration = QOpenHobbyOSIntegration::instance();
    if (!integration || !integration->xnxConnection())
        return;

    QRect windowGeometry = window->geometry();
    const QPoint windowOffset(windowGeometry.x(), windowGeometry.y());

    flushToXnx(image, region, windowOffset, window);

    QWindowSystemInterface::handleExposeEvent(window, region);
}

QImage::Format QOpenHobbyOSBackingStore::format() const
{
    return QImage::Format_ARGB32_Premultiplied;
}

bool QOpenHobbyOSBackingStore::scroll(const QRegion &area, int dx, int dy)
{
    Q_UNUSED(area);
    Q_UNUSED(dx);
    Q_UNUSED(dy);
    return false;
}

void QOpenHobbyOSBackingStore::flushToXnx(const QImage &image, const QRegion &region, const QPoint &offset, QWindow *window)
{
    QOpenHobbyOSWindow *ohosWindow = static_cast<QOpenHobbyOSWindow *>(window->handle());
    if (!ohosWindow)
        return;

    QOpenHobbyOSIntegration *integration = QOpenHobbyOSIntegration::instance();
    if (!integration || !integration->xnxConnection())
        return;

    uint32_t surfaceId = ohosWindow->xnxSurfaceId();
    if (surfaceId == 0)
        return;

    xnx_conn_t *conn = integration->xnxConnection();

    const QRect bounds = region.boundingRect();
    const int regionX = bounds.x();
    const int regionY = bounds.y();
    const int regionW = bounds.width();
    const int regionH = bounds.height();

    if (regionW <= 0 || regionH <= 0)
        return;

    QImage converted;
    const QImage *source = &image;

    if (image.format() != QImage::Format_ARGB32) {
        converted = image.convertToFormat(QImage::Format_ARGB32);
        source = &converted;
    }

    if (source->isNull())
        return;

    const uchar *bits = source->constBits();
    if (!bits)
        return;

    int srcBytesPerLine = source->bytesPerLine();
    int pixelOffset = regionY * srcBytesPerLine + regionX * 4;

    QVarLengthArray<uint32_t> pixelData(static_cast<int>(regionW) * static_cast<int>(regionH));
    if (pixelData.isEmpty())
        return;

    for (int y = 0; y < regionH; ++y) {
        const uint32_t *srcLine = reinterpret_cast<const uint32_t *>(bits + pixelOffset + y * srcBytesPerLine);
        uint32_t *dstLine = pixelData.data() + y * regionW;
        memcpy(dstLine, srcLine, static_cast<size_t>(regionW) * sizeof(uint32_t));
    }

    if (m_pixmanImage) {
        pixman_image_t *srcPixman = pixman_image_create_bits(
            PIXMAN_a8r8g8b8,
            source->width(), source->height(),
            reinterpret_cast<uint32_t *>(const_cast<uchar *>(source->bits())),
            source->bytesPerLine()
        );

        if (srcPixman) {
            pixman_image_t *destPixman = pixman_image_create_bits(
                PIXMAN_a8r8g8b8,
                regionW, regionH, pixelData.data(),
                regionW * 4
            );

            if (destPixman) {
                pixman_image_composite32(
                    PIXMAN_OP_SRC,
                    srcPixman,
                    nullptr,
                    destPixman,
                    regionX, regionY,
                    0, 0,
                    0, 0,
                    regionW, regionH
                );
                pixman_image_unref(destPixman);
            }
            pixman_image_unref(srcPixman);
        }
    }

    xnx_write_buffer(conn, surfaceId,
                     static_cast<uint32_t>(regionX),
                     static_cast<uint32_t>(regionY),
                     static_cast<uint32_t>(regionW),
                     static_cast<uint32_t>(regionH),
                     pixelData.data());

    xnx_commit(conn, surfaceId);
}

pixman_image_t *QOpenHobbyOSBackingStore::createPixmanImage(const QImage &image) const
{
    if (image.isNull())
        return nullptr;

    pixman_format_code_t pixmanFormat = PIXMAN_a8r8g8b8;
    switch (image.format()) {
    case QImage::Format_ARGB32:
    case QImage::Format_ARGB32_Premultiplied:
        pixmanFormat = PIXMAN_a8r8g8b8;
        break;
    case QImage::Format_RGB32:
        pixmanFormat = PIXMAN_x8r8g8b8;
        break;
    case QImage::Format_RGB16:
        pixmanFormat = PIXMAN_r5g6b5;
        break;
    default:
        return nullptr;
    }

    return pixman_image_create_bits(
        pixmanFormat,
        image.width(), image.height(),
        reinterpret_cast<uint32_t *>(const_cast<uchar *>(image.bits())),
        image.bytesPerLine()
    );
}

QT_END_NAMESPACE

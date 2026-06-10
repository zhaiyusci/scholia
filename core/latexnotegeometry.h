/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef OKULAR_LATEXNOTEGEOMETRY_H
#define OKULAR_LATEXNOTEGEOMETRY_H

#include <algorithm>
#include <cmath>

#include <QSizeF>

namespace Okular
{
namespace LatexNoteGeometry
{
constexpr double paddingPoints()
{
    return 6.0;
}

constexpr double contentInsetPoints()
{
    return paddingPoints() / 2.0;
}

inline double layoutWidthForVisibleWidth(double visibleWidthPoints, double scale)
{
    if (!std::isfinite(visibleWidthPoints) || visibleWidthPoints <= 0.0 || !std::isfinite(scale) || scale <= 0.0) {
        return 0.0;
    }
    return std::max(1.0, visibleWidthPoints / scale - paddingPoints());
}

inline QSizeF visualSizeForContent(const QSizeF &contentSizePoints, double layoutWidthPoints)
{
    if (!contentSizePoints.isValid() || contentSizePoints.isEmpty()) {
        return contentSizePoints;
    }

    return QSizeF(std::max(layoutWidthPoints, contentSizePoints.width() + paddingPoints()), contentSizePoints.height() + paddingPoints());
}
}
}

#endif

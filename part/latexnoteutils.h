/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef OKULAR_LATEXNOTEUTILS_H
#define OKULAR_LATEXNOTEUTILS_H

#include <QColor>
#include <QSizeF>
#include <QString>

#include "core/area.h"

namespace Okular
{
class Annotation;
class Page;
class StampAnnotation;
}

namespace LatexNoteUtils
{
struct RenderResult {
    bool ok = false;
    QString pdfFileName;
    QString errorMessage;
};

Okular::StampAnnotation *annotationAsLatexNote(Okular::Annotation *annotation);
const Okular::StampAnnotation *annotationAsLatexNote(const Okular::Annotation *annotation);

QColor colorForLatexNote(const Okular::StampAnnotation *annotation);
int latexFontSize();
int convertedTextFontSize();

double pageWidthInPoints(const Okular::Page *page);
double rectWidthInPoints(const Okular::NormalizedRect &rect, const Okular::Page *page);
double annotationWidthInPoints(const Okular::Annotation *annotation, const Okular::Page *page);
double layoutWidthForLatexNote(const Okular::StampAnnotation *annotation, const Okular::Page *page);
double scaleForLatexNote(const Okular::StampAnnotation *annotation, const Okular::Page *page, const QSizeF &pdfSizePoints);
Okular::NormalizedRect boundingRectForPdf(const Okular::NormalizedRect &sourceRect, const Okular::Page *page, const QSizeF &pdfSizePoints, double scale = 1.0);

RenderResult renderToCache(const QString &latexInput, const QColor &textColor, int fontSize, double layoutWidthPoints);
}

#endif

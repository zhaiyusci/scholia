/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef OKULAR_LATEXNOTEUTILS_H
#define OKULAR_LATEXNOTEUTILS_H

#include <QColor>
#include <QPoint>
#include <QSizeF>
#include <QString>

class QWidget;

#include "core/area.h"
#include "latexrenderer.h"

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
    QString warningMessage;
    GuiUtils::LatexRenderWarning warning;
};

Okular::StampAnnotation *annotationAsLatexNote(Okular::Annotation *annotation);
const Okular::StampAnnotation *annotationAsLatexNote(const Okular::Annotation *annotation);

QColor colorForLatexNote(const Okular::StampAnnotation *annotation);
int latexFontSize();
int convertedTextFontSize();

double pageWidthInPoints(const Okular::Page *page);
double pageHeightInPoints(const Okular::Page *page);
double rectWidthInPoints(const Okular::NormalizedRect &rect, const Okular::Page *page);
double rectHeightInPoints(const Okular::NormalizedRect &rect, const Okular::Page *page);
double annotationWidthInPoints(const Okular::Annotation *annotation, const Okular::Page *page);
double layoutWidthForVisibleWidth(double visibleWidthPoints, double scale, bool boxed = false);
double layoutWidthForLatexNote(const Okular::StampAnnotation *annotation, const Okular::Page *page);
double scaleForLatexNote(const Okular::StampAnnotation *annotation, const Okular::Page *page, const QSizeF &pdfSizePoints);
Okular::NormalizedRect boundingRectForPdf(const Okular::NormalizedRect &sourceRect, const Okular::Page *page, const QSizeF &pdfSizePoints, double scale = 1.0);

RenderResult renderToCache(const QString &latexInput, const QColor &textColor, int fontSize, double layoutWidthPoints, bool boxed = false);
QString warningText(const GuiUtils::LatexRenderWarning &warning);
void showRenderWarning(QWidget *parent, const QString &warningMessage);
void showRenderWarning(QWidget *parent, const GuiUtils::LatexRenderWarning &warning);
void showRenderWarning(QWidget *parent, const QString &warningMessage, const QPoint &globalPosition);
void showRenderWarning(QWidget *parent, const GuiUtils::LatexRenderWarning &warning, const QPoint &globalPosition);
}

#endif

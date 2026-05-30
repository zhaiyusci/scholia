/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "latexnoteutils.h"

#include <cmath>

#include <KLocalizedString>
#include <QCryptographicHash>
#include <QCursor>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QToolTip>

#include "core/annotations.h"
#include "core/page.h"
#include "core/utils.h"
#include "gui/guiutils.h"
#include "latexrenderer.h"
#include "settings.h"

namespace
{
bool isFiniteUsableRect(const Okular::NormalizedRect &rect)
{
    return std::isfinite(rect.left) && std::isfinite(rect.top) && std::isfinite(rect.right) && std::isfinite(rect.bottom) && rect.width() > 0.0 && rect.height() > 0.0;
}

Okular::NormalizedRect fitRectInsidePage(Okular::NormalizedRect rect)
{
    if (rect.right > 1.0) {
        rect.left -= rect.right - 1.0;
        rect.right = 1.0;
    }
    if (rect.bottom > 1.0) {
        rect.top -= rect.bottom - 1.0;
        rect.bottom = 1.0;
    }
    if (rect.left < 0.0) {
        rect.right -= rect.left;
        rect.left = 0.0;
    }
    if (rect.top < 0.0) {
        rect.bottom -= rect.top;
        rect.top = 0.0;
    }
    rect.right = qBound(0.0, rect.right, 1.0);
    rect.bottom = qBound(0.0, rect.bottom, 1.0);
    return rect;
}

QString latexErrorMessage(GuiUtils::LatexRenderer::Error errorCode, const QString &latexOutput)
{
    switch (errorCode) {
    case GuiUtils::LatexRenderer::LatexNotFound:
        return i18n("Cannot find xelatex or lualatex executable.");
    case GuiUtils::LatexRenderer::DvipngNotFound:
        return i18n("Cannot find dvipng executable.");
    case GuiUtils::LatexRenderer::LatexFailed:
        return i18n("LaTeX rendering failed:\n%1", GuiUtils::LatexRenderer::compactErrorMessage(latexOutput));
    case GuiUtils::LatexRenderer::DvipngFailed:
        return i18n("A problem occurred during the execution of the 'dvipng' command.");
    case GuiUtils::LatexRenderer::PdfToImageNotFound:
        return i18n("Cannot find pdftocairo executable.");
    case GuiUtils::LatexRenderer::PdfToImageFailed:
        return i18n("A problem occurred during the execution of the 'pdftocairo' command.");
    case GuiUtils::LatexRenderer::MicrotexFailed:
        return i18n("MicroTeX fallback failed:\n%1", GuiUtils::LatexRenderer::compactErrorMessage(latexOutput));
    case GuiUtils::LatexRenderer::NoError:
        break;
    }
    return QString();
}

constexpr double latexBoxFrameInsetPoints = 3.0;

QString latexNoteBaseName(const QString &latexInput, const QColor &textColor, int fontSize, double layoutWidthPoints, const QString &sourcePreamble, const QString &backendName)
{
    const bool fixedWidth = std::isfinite(layoutWidthPoints) && layoutWidthPoints > 0.0;
    const QString widthText = fixedWidth ? QString::number(layoutWidthPoints, 'f', 3) : QStringLiteral("0");
    const QString renderMode = fixedWidth ? QStringLiteral("fixed-width-content-v6") : QStringLiteral("natural-width-content-v6");
    const QString hashText = latexInput + QStringLiteral("|%1|%2|%3|%4|%5|%6").arg(textColor.name(QColor::HexArgb)).arg(fontSize).arg(widthText, sourcePreamble, renderMode, backendName);
    return QString::fromLatin1(QCryptographicHash::hash(hashText.toUtf8(), QCryptographicHash::Sha256).toHex());
}

QDir latexNoteCacheDir()
{
    QString dataLocation = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataLocation.isEmpty()) {
        dataLocation = QDir::tempPath();
    }
    return QDir(dataLocation);
}

QSizeF pageSizeInPoints(const Okular::Page *page)
{
    if (!page || page->width() <= 0.0 || page->height() <= 0.0) {
        return {};
    }

    const QSizeF dpi = Okular::Utils::realDpi(nullptr);
    const double dpiX = dpi.width() > 0.0 && std::isfinite(dpi.width()) ? dpi.width() : 72.0;
    const double dpiY = dpi.height() > 0.0 && std::isfinite(dpi.height()) ? dpi.height() : 72.0;
    return QSizeF(page->width() * 72.0 / dpiX, page->height() * 72.0 / dpiY);
}
}

namespace LatexNoteUtils
{
const Okular::StampAnnotation *annotationAsLatexNote(const Okular::Annotation *annotation)
{
    if (!annotation || annotation->subType() != Okular::Annotation::AStamp || annotation->contents().trimmed().isEmpty()) {
        return nullptr;
    }

    const auto *stampAnnotation = static_cast<const Okular::StampAnnotation *>(annotation);
    if (GuiUtils::latexNotePdfFileForStamp(stampAnnotation->stampIconName()).isEmpty()) {
        return nullptr;
    }

    if (!isFiniteUsableRect(stampAnnotation->boundingRectangle())) {
        return nullptr;
    }

    return stampAnnotation;
}

Okular::StampAnnotation *annotationAsLatexNote(Okular::Annotation *annotation)
{
    return const_cast<Okular::StampAnnotation *>(annotationAsLatexNote(static_cast<const Okular::Annotation *>(annotation)));
}

QColor colorForLatexNote(const Okular::StampAnnotation *annotation)
{
    if (!annotation) {
        return Qt::black;
    }

    QColor textColor = annotation->style().color();
    if (!textColor.isValid() || textColor.alpha() == 0) {
        textColor = Qt::black;
    }
    return textColor;
}

int latexFontSize()
{
    return qBound(1, Okular::Settings::latexAnnotationFontSize(), 72);
}

int convertedTextFontSize()
{
    return qBound(1, Okular::Settings::latexTextAnnotationFontSize(), 72);
}

double rectWidthInPoints(const Okular::NormalizedRect &rect, const Okular::Page *page)
{
    const double pageWidth = pageWidthInPoints(page);
    if (pageWidth <= 0.0 || !std::isfinite(rect.width()) || rect.width() <= 0.0) {
        return 0.0;
    }
    return qMax(1.0, rect.width() * pageWidth);
}

double pageWidthInPoints(const Okular::Page *page)
{
    return pageSizeInPoints(page).width();
}

double pageHeightInPoints(const Okular::Page *page)
{
    return pageSizeInPoints(page).height();
}

double rectHeightInPoints(const Okular::NormalizedRect &rect, const Okular::Page *page)
{
    const double pageHeight = pageHeightInPoints(page);
    if (pageHeight <= 0.0 || !std::isfinite(rect.height()) || rect.height() <= 0.0) {
        return 0.0;
    }

    return qMax(1.0, rect.height() * pageHeight);
}

double annotationWidthInPoints(const Okular::Annotation *annotation, const Okular::Page *page)
{
    return annotation ? rectWidthInPoints(annotation->boundingRectangle(), page) : 0.0;
}

double layoutWidthForVisibleWidth(double visibleWidthPoints, double scale, bool boxed)
{
    if (!std::isfinite(visibleWidthPoints) || visibleWidthPoints <= 0.0 || !std::isfinite(scale) || scale <= 0.0) {
        return 0.0;
    }

    constexpr double boxedFrameWidthPoints = 2.0 * latexBoxFrameInsetPoints;
    return qMax(1.0, visibleWidthPoints / scale - (boxed ? boxedFrameWidthPoints : 0.0));
}

double layoutWidthForLatexNote(const Okular::StampAnnotation *annotation, const Okular::Page *page)
{
    if (!annotation) {
        return 0.0;
    }

    const double storedWidth = annotation->latexNoteLayoutWidth();
    if (std::isfinite(storedWidth) && storedWidth > 0.0) {
        return storedWidth;
    }

    Q_UNUSED(page);
    return 0.0;
}

double scaleForLatexNote(const Okular::StampAnnotation *annotation, const Okular::Page *page, const QSizeF &pdfSizePoints)
{
    if (!annotation) {
        return 1.0;
    }

    const double storedScale = annotation->latexNoteScale();
    if (std::isfinite(storedScale) && storedScale > 0.0) {
        return storedScale;
    }

    const QSizeF visualSizePoints = visualSizeForLatexNote(pdfSizePoints, layoutWidthForLatexNote(annotation, page), annotation->latexNoteBoxed());
    if (page && visualSizePoints.isValid() && !visualSizePoints.isEmpty()) {
        const double visibleHeight = rectHeightInPoints(annotation->boundingRectangle(), page);
        if (visualSizePoints.height() > 0.0 && std::isfinite(visibleHeight) && visibleHeight > 0.0) {
            return qMax(0.01, visibleHeight / visualSizePoints.height());
        }

        const double visibleWidth = annotationWidthInPoints(annotation, page);
        if (visualSizePoints.width() > 0.0 && std::isfinite(visibleWidth) && visibleWidth > 0.0) {
            return qMax(0.01, visibleWidth / visualSizePoints.width());
        }
    }

    return 1.0;
}

QSizeF visualSizeForLatexNote(const QSizeF &contentPdfSizePoints, double layoutWidthPoints, bool boxed)
{
    if (!contentPdfSizePoints.isValid() || contentPdfSizePoints.isEmpty()) {
        return contentPdfSizePoints;
    }
    if (!boxed) {
        return contentPdfSizePoints;
    }

    const double frameWidth = std::isfinite(layoutWidthPoints) && layoutWidthPoints > 0.0 ? layoutWidthPoints + 2.0 * latexBoxFrameInsetPoints : contentPdfSizePoints.width() + 2.0 * latexBoxFrameInsetPoints;
    const double visualWidth = qMax(frameWidth, contentPdfSizePoints.width() + latexBoxFrameInsetPoints);
    const double visualHeight = contentPdfSizePoints.height() + 2.0 * latexBoxFrameInsetPoints;
    if (!std::isfinite(visualWidth) || !std::isfinite(visualHeight) || visualWidth <= 0.0 || visualHeight <= 0.0) {
        return contentPdfSizePoints;
    }
    return QSizeF(visualWidth, visualHeight);
}

Okular::NormalizedRect boundingRectForPdf(const Okular::NormalizedRect &sourceRect, const Okular::Page *page, const QSizeF &pdfSizePoints, double scale)
{
    const QSizeF pageSizePoints = pageSizeInPoints(page);
    if (!pageSizePoints.isValid() || pageSizePoints.isEmpty() || !pdfSizePoints.isValid() || pdfSizePoints.isEmpty()) {
        return sourceRect;
    }

    if (!std::isfinite(scale) || scale <= 0.0) {
        scale = 1.0;
    }

    double normalizedWidth = pdfSizePoints.width() * scale / pageSizePoints.width();
    double normalizedHeight = pdfSizePoints.height() * scale / pageSizePoints.height();
    if (!std::isfinite(normalizedWidth) || !std::isfinite(normalizedHeight) || normalizedWidth <= 0.0 || normalizedHeight <= 0.0) {
        return sourceRect;
    }

    if (normalizedWidth > 1.0 || normalizedHeight > 1.0) {
        const double shrink = qMin(1.0 / normalizedWidth, 1.0 / normalizedHeight);
        normalizedWidth *= shrink;
        normalizedHeight *= shrink;
    }

    return fitRectInsidePage(Okular::NormalizedRect(sourceRect.left, sourceRect.top, sourceRect.left + normalizedWidth, sourceRect.top + normalizedHeight));
}

Okular::NormalizedRect boundingRectForLatexNote(const Okular::NormalizedRect &sourceRect, const Okular::Page *page, const QSizeF &contentPdfSizePoints, double layoutWidthPoints, bool boxed, double scale)
{
    return boundingRectForPdf(sourceRect, page, visualSizeForLatexNote(contentPdfSizePoints, layoutWidthPoints, boxed), scale);
}

RenderResult renderToCache(const QString &latexInput, const QColor &textColor, int fontSize, double layoutWidthPoints)
{
    RenderResult result;
    if (latexInput.trimmed().isEmpty()) {
        result.errorMessage = i18n("LaTeX source is empty.");
        return result;
    }

    GuiUtils::LatexRenderer renderer;
    QString latexOutput;
    QString temporaryPdfFile;
    const QString sourcePreamble = Okular::Settings::latexPreamble();
    const GuiUtils::LatexRenderer::Error errorCode = renderer.renderLatexToPdf(latexInput, textColor, fontSize, temporaryPdfFile, latexOutput, layoutWidthPoints, sourcePreamble);
    if (errorCode != GuiUtils::LatexRenderer::NoError) {
        result.errorMessage = latexErrorMessage(errorCode, latexOutput);
        return result;
    }

    QDir dataDir = latexNoteCacheDir();
    if (!dataDir.mkpath(QStringLiteral("latex-notes"))) {
        result.errorMessage = i18n("Could not create a directory for LaTeX notes.");
        return result;
    }

    const QString noteBaseName = latexNoteBaseName(latexInput, textColor, fontSize, layoutWidthPoints, sourcePreamble, renderer.lastBackendName());
    const QString cachedPdfFileName = dataDir.filePath(QStringLiteral("latex-notes/%1.pdf").arg(noteBaseName));
    if (!temporaryPdfFile.isEmpty() && !QFile::exists(cachedPdfFileName) && !QFile::copy(temporaryPdfFile, cachedPdfFileName)) {
        result.errorMessage = i18n("Could not save the rendered LaTeX note PDF.");
        return result;
    }

    if (!QFile::exists(cachedPdfFileName)) {
        result.errorMessage = i18n("Could not save the rendered LaTeX note PDF.");
        return result;
    }

    result.ok = true;
    result.pdfFileName = cachedPdfFileName;
    result.warning = renderer.lastWarning();
    result.warningMessage = warningText(result.warning);
    return result;
}

QString warningText(const GuiUtils::LatexRenderWarning &warning)
{
    return warning.isValid() ? warning.message : QString();
}

void showRenderWarning(QWidget *parent, const QString &warningMessage)
{
    showRenderWarning(parent, warningMessage, QCursor::pos());
}

void showRenderWarning(QWidget *parent, const GuiUtils::LatexRenderWarning &warning)
{
    showRenderWarning(parent, warningText(warning));
}

void showRenderWarning(QWidget *parent, const QString &warningMessage, const QPoint &globalPosition)
{
    if (warningMessage.isEmpty()) {
        return;
    }

    QToolTip::showText(globalPosition, warningMessage, parent, QRect(), 7000);
}

void showRenderWarning(QWidget *parent, const GuiUtils::LatexRenderWarning &warning, const QPoint &globalPosition)
{
    showRenderWarning(parent, warningText(warning), globalPosition);
}
}

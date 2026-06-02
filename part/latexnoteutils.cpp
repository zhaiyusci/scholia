/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "latexnoteutils.h"

#include <cmath>

#include <KLocalizedString>
#include <KMessageBox>
#include <QCryptographicHash>
#include <QCursor>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QToolTip>

#include "core/annotations.h"
#include "core/document.h"
#include "core/page.h"
#include "core/utils.h"
#include "gui/debug_ui.h"
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

constexpr double latexFreeTextPaddingPoints = 6.0;

QString latexNoteBaseName(const QString &latexInput, const QColor &textColor, int fontSize, double layoutWidthPoints, const QString &sourcePreamble, const QString &backendName)
{
    const bool fixedWidth = std::isfinite(layoutWidthPoints) && layoutWidthPoints > 0.0;
    const QString widthText = fixedWidth ? QString::number(layoutWidthPoints, 'f', 3) : QStringLiteral("0");
    const QString renderMode = fixedWidth ? QStringLiteral("fixed-width-content-v6") : QStringLiteral("natural-width-content-v6");
    const QString hashText = latexInput + QStringLiteral("|%1|%2|%3|%4|%5|%6").arg(textColor.name(QColor::HexArgb)).arg(fontSize).arg(widthText, sourcePreamble, renderMode, backendName);
    return QString::fromLatin1(QCryptographicHash::hash(hashText.toUtf8(), QCryptographicHash::Sha256).toHex());
}

QTemporaryDir *latexAppearanceSessionRoot()
{
    static QTemporaryDir *sessionRoot = []() {
        QString baseLocation = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
        if (baseLocation.isEmpty()) {
            baseLocation = QDir::tempPath();
        }
        QDir().mkpath(baseLocation);

        auto *runtimeDir = new QTemporaryDir(QDir(baseLocation).filePath(QStringLiteral("okular-latex-appearances-XXXXXX")));
        if (runtimeDir->isValid()) {
            runtimeDir->setAutoRemove(true);
            return runtimeDir;
        }

        delete runtimeDir;
        auto *fallbackDir = new QTemporaryDir(QDir(QDir::tempPath()).filePath(QStringLiteral("okular-latex-appearances-XXXXXX")));
        fallbackDir->setAutoRemove(true);
        return fallbackDir;
    }();
    return sessionRoot;
}

QDir latexAppearanceSessionDir()
{
    QTemporaryDir *sessionRoot = latexAppearanceSessionRoot();
    if (!sessionRoot || !sessionRoot->isValid()) {
        QDir fallbackDir(QDir::tempPath());
        fallbackDir.mkpath(QStringLiteral("okular-latex-appearances/latex-notes"));
        return QDir(fallbackDir.filePath(QStringLiteral("okular-latex-appearances/latex-notes")));
    }

    QDir rootDir(sessionRoot->path());
    rootDir.mkpath(QStringLiteral("latex-notes"));
    return QDir(rootDir.filePath(QStringLiteral("latex-notes")));
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
const Okular::TextAnnotation *annotationAsLatexTextAnnotation(const Okular::Annotation *annotation)
{
    if (!annotation || annotation->subType() != Okular::Annotation::AText || annotation->contents().trimmed().isEmpty() || !annotation->isOkularLatex()) {
        return nullptr;
    }

    const auto *textAnnotation = static_cast<const Okular::TextAnnotation *>(annotation);
    if (textAnnotation->textType() != Okular::TextAnnotation::InPlace) {
        return nullptr;
    }

    if (!isFiniteUsableRect(textAnnotation->boundingRectangle())) {
        return nullptr;
    }

    return textAnnotation;
}

Okular::TextAnnotation *annotationAsLatexTextAnnotation(Okular::Annotation *annotation)
{
    return const_cast<Okular::TextAnnotation *>(annotationAsLatexTextAnnotation(static_cast<const Okular::Annotation *>(annotation)));
}

const Okular::StampAnnotation *annotationAsLatexStampAnnotation(const Okular::Annotation *annotation)
{
    if (!annotation || annotation->subType() != Okular::Annotation::AStamp || annotation->contents().trimmed().isEmpty() || !annotation->isOkularLatex()) {
        return nullptr;
    }

    const auto *stampAnnotation = static_cast<const Okular::StampAnnotation *>(annotation);
    if (!isFiniteUsableRect(stampAnnotation->boundingRectangle())) {
        return nullptr;
    }

    return stampAnnotation;
}

Okular::StampAnnotation *annotationAsLatexStampAnnotation(Okular::Annotation *annotation)
{
    return const_cast<Okular::StampAnnotation *>(annotationAsLatexStampAnnotation(static_cast<const Okular::Annotation *>(annotation)));
}

bool annotationIsLatex(const Okular::Annotation *annotation)
{
    return annotationAsLatexTextAnnotation(annotation) || annotationAsLatexStampAnnotation(annotation);
}

bool annotationIsLatex(Okular::Annotation *annotation)
{
    return annotationAsLatexTextAnnotation(annotation) || annotationAsLatexStampAnnotation(annotation);
}

QColor colorForLatexAnnotation(const Okular::Annotation *annotation)
{
    if (!annotation) {
        return Qt::black;
    }

    if (annotation->subType() == Okular::Annotation::AText) {
        const auto *textAnnotation = static_cast<const Okular::TextAnnotation *>(annotation);
        QColor textColor = textAnnotation->textColor();
        if (!textColor.isValid() || textColor.alpha() == 0) {
            textColor = Qt::black;
        }
        return textColor;
    }

    QColor textColor = annotation->latexTextColor();
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

double latexTextAnnotationPaddingPoints()
{
    return latexFreeTextPaddingPoints;
}

double layoutWidthForLatexTextVisibleWidth(double visibleWidthPoints, double scale)
{
    if (!std::isfinite(visibleWidthPoints) || visibleWidthPoints <= 0.0 || !std::isfinite(scale) || scale <= 0.0) {
        return 0.0;
    }
    return qMax(1.0, visibleWidthPoints / scale - latexFreeTextPaddingPoints);
}

double scaleForLatexTextAnnotation(const Okular::TextAnnotation *annotation)
{
    if (!annotation || !std::isfinite(annotation->latexScale()) || annotation->latexScale() <= 0.0) {
        return 1.0;
    }
    return annotation->latexScale();
}

double layoutWidthForLatexTextAnnotation(const Okular::TextAnnotation *annotation, const Okular::Page *page)
{
    if (!annotation) {
        return 0.0;
    }

    const double storedWidth = annotation->latexLayoutWidth();
    if (std::isfinite(storedWidth) && storedWidth > 0.0) {
        return storedWidth;
    }

    const double visibleWidthPoints = annotationWidthInPoints(annotation, page);
    return layoutWidthForLatexTextVisibleWidth(visibleWidthPoints, scaleForLatexTextAnnotation(annotation));
}

QSizeF visualSizeForLatexTextAnnotation(const QSizeF &contentPdfSizePoints, double layoutWidthPoints)
{
    if (!contentPdfSizePoints.isValid() || contentPdfSizePoints.isEmpty()) {
        return contentPdfSizePoints;
    }

    return QSizeF(qMax(layoutWidthPoints, contentPdfSizePoints.width() + latexFreeTextPaddingPoints), contentPdfSizePoints.height() + latexFreeTextPaddingPoints);
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

RenderResult renderAppearancePdf(const QString &latexInput, const QColor &textColor, int fontSize, double layoutWidthPoints)
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
        qCWarning(OkularUiDebug) << "LaTeX note PDF render failed; backend:" << renderer.lastBackendName() << "layout width:" << layoutWidthPoints << "error:" << errorCode
                                 << "message:" << latexErrorMessage(errorCode, latexOutput);
        result.errorMessage = latexErrorMessage(errorCode, latexOutput);
        return result;
    }

    const QFileInfo temporaryPdfInfo(temporaryPdfFile);
    qCDebug(OkularUiDebug) << "LaTeX note PDF render finished; backend:" << renderer.lastBackendName() << "layout width:" << layoutWidthPoints << "temporary PDF:" << temporaryPdfFile
                           << "exists:" << temporaryPdfInfo.exists() << "bytes:" << temporaryPdfInfo.size();

    result.pdfSizePoints = GuiUtils::pdfPageSizeInPoints(temporaryPdfFile);
    if (!result.pdfSizePoints.isValid() || result.pdfSizePoints.isEmpty()) {
        qCWarning(OkularUiDebug) << "LaTeX note rendered PDF has invalid size; temporary PDF:" << temporaryPdfFile << "size:" << result.pdfSizePoints;
        result.errorMessage = i18n("Could not load the rendered LaTeX note PDF.");
        return result;
    }
    qCDebug(OkularUiDebug) << "LaTeX note rendered PDF page size:" << result.pdfSizePoints;

    QDir dataDir = latexAppearanceSessionDir();
    if (!dataDir.exists()) {
        result.errorMessage = i18n("Could not create a temporary directory for LaTeX note appearances.");
        return result;
    }

    const QString noteBaseName = latexNoteBaseName(latexInput, textColor, fontSize, layoutWidthPoints, sourcePreamble, renderer.lastBackendName());
    const QString appearancePdfFileName = dataDir.filePath(QStringLiteral("%1.pdf").arg(noteBaseName));
    if (QFile::exists(appearancePdfFileName)) {
        QFile::remove(appearancePdfFileName);
    }
    if (temporaryPdfFile.isEmpty() || !QFile::copy(temporaryPdfFile, appearancePdfFileName)) {
        qCWarning(OkularUiDebug) << "Could not copy rendered LaTeX note PDF; from:" << temporaryPdfFile << "to:" << appearancePdfFileName;
        result.errorMessage = i18n("Could not save the rendered LaTeX note PDF.");
        return result;
    }

    if (!QFile::exists(appearancePdfFileName)) {
        qCWarning(OkularUiDebug) << "Copied LaTeX note appearance PDF is missing; target:" << appearancePdfFileName;
        result.errorMessage = i18n("Could not save the rendered LaTeX note PDF.");
        return result;
    }

    result.ok = true;
    result.pdfFileName = appearancePdfFileName;
    result.warning = renderer.lastWarning();
    result.warningMessage = warningText(result.warning);
    const QFileInfo appearancePdfInfo(appearancePdfFileName);
    qCDebug(OkularUiDebug) << "LaTeX note appearance PDF ready; path:" << result.pdfFileName << "exists:" << appearancePdfInfo.exists() << "bytes:" << appearancePdfInfo.size()
                           << "page size:" << result.pdfSizePoints << "warning:" << result.warningMessage;
    return result;
}

bool updateLatexTextAnnotationAppearance(QWidget *parent,
                                         Okular::Document *document,
                                         int pageNumber,
                                         Okular::TextAnnotation *textAnnotation,
                                         const QColor &textColor,
                                         const QColor &fillColor,
                                         const QColor &borderColor,
                                         double layoutWidthPoints,
                                         bool boxed,
                                         double visualScale)
{
    if (!document || pageNumber == -1 || !textAnnotation) {
        return false;
    }

    const Okular::Page *page = document->page(pageNumber);
    if (!page) {
        return false;
    }
    if (!std::isfinite(layoutWidthPoints) || layoutWidthPoints < 0.0) {
        layoutWidthPoints = layoutWidthForLatexTextAnnotation(textAnnotation, page);
    }
    if (!std::isfinite(visualScale) || visualScale <= 0.0) {
        visualScale = scaleForLatexTextAnnotation(textAnnotation);
    }

    const RenderResult rendered = renderAppearancePdf(textAnnotation->contents(), textColor, latexFontSize(), layoutWidthPoints);
    if (!rendered.ok) {
        KMessageBox::error(parent, rendered.errorMessage, i18n("LaTeX rendering failed"));
        return false;
    }

    const QSizeF visualSizePoints = visualSizeForLatexTextAnnotation(rendered.pdfSizePoints, layoutWidthPoints);
    if (!visualSizePoints.isValid() || visualSizePoints.isEmpty()) {
        KMessageBox::error(parent, i18n("Could not load the rendered LaTeX note PDF."), i18n("LaTeX rendering failed"));
        return false;
    }

    const Okular::NormalizedRect updatedRect = boundingRectForPdf(textAnnotation->boundingRectangle(), page, visualSizePoints, visualScale);
    const Okular::TextAnnotation::InplaceIntent targetIntent =
        textAnnotation->inplaceIntent() == Okular::TextAnnotation::Callout ? Okular::TextAnnotation::Callout : (boxed ? Okular::TextAnnotation::Unknown : Okular::TextAnnotation::TypeWriter);
    const double targetBorderWidth = boxed ? qMax(1.0, textAnnotation->style().width()) : 0.0;
    const bool sameLayoutWidth = qAbs(textAnnotation->latexLayoutWidth() - layoutWidthPoints) < 1e-3;
    const bool sameScale = qAbs(textAnnotation->latexScale() - visualScale) < 1e-6;
    showRenderWarning(parent, rendered.warning);
    if (rendered.pdfFileName == textAnnotation->latexAppearancePdfFileName() && updatedRect == textAnnotation->boundingRectangle() && textAnnotation->textColor() == textColor
        && textAnnotation->style().color() == fillColor && textAnnotation->inplaceBorderColor() == borderColor && textAnnotation->inplaceIntent() == targetIntent
        && qAbs(textAnnotation->style().width() - targetBorderWidth) < 1e-6 && sameLayoutWidth && sameScale && textAnnotation->isOkularLatex()) {
        return true;
    }

    document->prepareToModifyAnnotationProperties(textAnnotation);
    textAnnotation->setOkularLatex(true);
    textAnnotation->setLatexAppearancePdfFileName(rendered.pdfFileName);
    textAnnotation->setLatexLayoutWidth(layoutWidthPoints);
    textAnnotation->setLatexScale(visualScale);
    textAnnotation->setTextColor(textColor);
    textAnnotation->setInplaceBorderColor(borderColor);
    textAnnotation->setInplaceIntent(targetIntent);
    textAnnotation->style().setColor(fillColor);
    textAnnotation->style().setWidth(targetBorderWidth);
    textAnnotation->setBoundingRectangle(updatedRect);
    textAnnotation->setModificationDate(QDateTime::currentDateTime());
    qCDebug(OkularUiDebug) << "Updating LaTeX note appearance; source path:" << rendered.pdfFileName << "layout width:" << layoutWidthPoints << "scale:" << visualScale
                           << "pdf size:" << rendered.pdfSizePoints << "visual size:" << visualSizePoints << "rect:" << updatedRect.left << updatedRect.top << updatedRect.right
                           << updatedRect.bottom;
    document->modifyPageAnnotationProperties(pageNumber, textAnnotation);
    return true;
}

bool updateLatexStampAnnotationAppearance(QWidget *parent,
                                          Okular::Document *document,
                                          int pageNumber,
                                          Okular::StampAnnotation *stampAnnotation,
                                          const QColor &textColor,
                                          const QColor &fillColor,
                                          const QColor &borderColor,
                                          double layoutWidthPoints,
                                          bool boxed,
                                          double visualScale,
                                          bool prepareModification)
{
    if (!document || pageNumber == -1 || !stampAnnotation) {
        return false;
    }

    const Okular::Page *page = document->page(pageNumber);
    if (!page) {
        return false;
    }
    if (!std::isfinite(layoutWidthPoints) || layoutWidthPoints < 0.0) {
        const double storedWidth = stampAnnotation->latexLayoutWidth();
        layoutWidthPoints = std::isfinite(storedWidth) && storedWidth > 0.0 ? storedWidth : 0.0;
    }
    if (!std::isfinite(visualScale) || visualScale <= 0.0) {
        visualScale = stampAnnotation->latexScale();
    }
    if (!std::isfinite(visualScale) || visualScale <= 0.0) {
        visualScale = 1.0;
    }

    const RenderResult rendered = renderAppearancePdf(stampAnnotation->contents(), textColor, latexFontSize(), layoutWidthPoints);
    if (!rendered.ok) {
        KMessageBox::error(parent, rendered.errorMessage, i18n("LaTeX rendering failed"));
        return false;
    }

    const QSizeF visualSizePoints = visualSizeForLatexTextAnnotation(rendered.pdfSizePoints, layoutWidthPoints);
    if (!visualSizePoints.isValid() || visualSizePoints.isEmpty()) {
        KMessageBox::error(parent, i18n("Could not load the rendered LaTeX note PDF."), i18n("LaTeX rendering failed"));
        return false;
    }

    const Okular::NormalizedRect updatedRect = boundingRectForPdf(stampAnnotation->boundingRectangle(), page, visualSizePoints, visualScale);
    const double targetBorderWidth = boxed ? qMax(1.0, stampAnnotation->style().width()) : 0.0;
    showRenderWarning(parent, rendered.warning);
    if (rendered.pdfFileName == stampAnnotation->latexAppearancePdfFileName() && updatedRect == stampAnnotation->boundingRectangle() && qAbs(stampAnnotation->latexLayoutWidth() - layoutWidthPoints) < 1e-3
        && qAbs(stampAnnotation->latexScale() - visualScale) < 1e-6 && stampAnnotation->isOkularLatex() && stampAnnotation->latexTextColor() == textColor
        && stampAnnotation->latexFillColor() == fillColor && stampAnnotation->latexBorderColor() == borderColor && qAbs(stampAnnotation->style().width() - targetBorderWidth) < 1e-6) {
        if (prepareModification) {
            return true;
        }
    }

    if (prepareModification) {
        document->prepareToModifyAnnotationProperties(stampAnnotation);
    }
    stampAnnotation->setOkularLatex(true);
    stampAnnotation->setStampIconName(QStringLiteral("latex-notes"));
    stampAnnotation->setStampImagePath(QString());
    stampAnnotation->setLatexAppearancePdfFileName(rendered.pdfFileName);
    stampAnnotation->setLatexLayoutWidth(layoutWidthPoints);
    stampAnnotation->setLatexScale(visualScale);
    stampAnnotation->setLatexTextColor(textColor);
    stampAnnotation->setLatexFillColor(fillColor);
    stampAnnotation->setLatexBorderColor(borderColor);
    stampAnnotation->style().setWidth(targetBorderWidth);
    stampAnnotation->style().setColor(textColor);
    stampAnnotation->setBoundingRectangle(updatedRect);
    stampAnnotation->setModificationDate(QDateTime::currentDateTime());
    qCDebug(OkularUiDebug) << "Updating LaTeX stamp appearance; source path:" << rendered.pdfFileName << "layout width:" << layoutWidthPoints << "scale:" << visualScale
                           << "pdf size:" << rendered.pdfSizePoints << "visual size:" << visualSizePoints << "rect:" << updatedRect.left << updatedRect.top << updatedRect.right
                           << updatedRect.bottom;
    document->modifyPageAnnotationProperties(pageNumber, stampAnnotation);
    return true;
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

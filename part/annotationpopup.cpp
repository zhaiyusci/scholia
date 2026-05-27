/*
    SPDX-FileCopyrightText: 2006 Tobias Koenig <tokoe@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "annotationpopup.h"

#include <algorithm>
#include <cmath>

#include <KLocalizedString>
#include <KMessageBox>
#include <QApplication>
#include <QClipboard>
#include <QColorDialog>
#include <QDateTime>
#include <QDomDocument>
#include <QFile>
#include <QFont>
#include <QFontMetricsF>
#include <QIcon>
#include <QInputDialog>
#include <QMenu>
#include <QMimeData>
#include <QSizeF>
#include <QTransform>

#include "annotationpropertiesdialog.h"

#include "core/annotations.h"
#include "core/bookmarkmanager.h"
#include "core/document.h"
#include "core/page.h"
#include "gui/guiutils.h"
#include "latexnoteutils.h"
#include "okmenutitle.h"
#include "settings.h"

#include <KIO/JobUiDelegateFactory>
#include <KIO/OpenUrlJob>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QUrl>

Q_DECLARE_METATYPE(AnnotationPopup::AnnotPagePair)

namespace
{
bool annotationHasFileAttachment(Okular::Annotation *annotation)
{
    return (annotation->subType() == Okular::Annotation::AFileAttachment || annotation->subType() == Okular::Annotation::ARichMedia);
}

Okular::EmbeddedFile *embeddedFileFromAnnotation(Okular::Annotation *annotation)
{
    if (annotation->subType() == Okular::Annotation::AFileAttachment) {
        const Okular::FileAttachmentAnnotation *fileAttachAnnot = static_cast<Okular::FileAttachmentAnnotation *>(annotation);
        return fileAttachAnnot->embeddedFile();
    } else if (annotation->subType() == Okular::Annotation::ARichMedia) {
        const Okular::RichMediaAnnotation *richMediaAnnot = static_cast<Okular::RichMediaAnnotation *>(annotation);
        return richMediaAnnot->embeddedFile();
    } else {
        return nullptr;
    }
}

bool annotationSupportsCopy(const Okular::Annotation *annotation)
{
    if (!annotation) {
        return false;
    }

    switch (annotation->subType()) {
    case Okular::Annotation::AGeom:
    case Okular::Annotation::AInk:
    case Okular::Annotation::ALine:
    case Okular::Annotation::AText:
        return true;
        break;

    default:
        return false;
        break;
    }
    return false;
}

bool clipboardFormatVersionSupported(const QDomElement &root)
{
    const QString versionAttributeName = QStringLiteral("version");
    if (!root.hasAttribute(versionAttributeName)) {
        return false;
    }

    bool versionIsNumber = false;
    const int clipboardFormatVersion = root.attribute(versionAttributeName).toInt(&versionIsNumber);
    return versionIsNumber && clipboardFormatVersion == AnnotationPopup::annotationClipboardFormatVersion;
}

Okular::TextAnnotation *convertibleTextAnnotation(Okular::Annotation *annotation)
{
    if (!annotation || annotation->subType() != Okular::Annotation::AText) {
        return nullptr;
    }

    auto *textAnnotation = static_cast<Okular::TextAnnotation *>(annotation);
    if (textAnnotation->textType() != Okular::TextAnnotation::InPlace || textAnnotation->contents().trimmed().isEmpty()) {
        return nullptr;
    }

    const Okular::NormalizedRect rect = textAnnotation->boundingRectangle();
    if (!std::isfinite(rect.left) || !std::isfinite(rect.top) || !std::isfinite(rect.right) || !std::isfinite(rect.bottom) || rect.width() <= 0.0) {
        return nullptr;
    }

    return textAnnotation;
}

Okular::StampAnnotation *latexStampAnnotation(Okular::Annotation *annotation)
{
    return LatexNoteUtils::annotationAsLatexNote(annotation);
}

int latexFontSizeForTextAnnotation(const Okular::TextAnnotation *)
{
    return LatexNoteUtils::latexFontSize();
}

QColor latexTextColorForTextAnnotation(const Okular::TextAnnotation *annotation)
{
    if (!annotation) {
        return Qt::black;
    }

    QColor textColor = annotation->textColor();
    if (!textColor.isValid() || textColor.alpha() == 0) {
        textColor = Qt::black;
    }
    return textColor;
}

double latexMaxWidthForTextAnnotation(const Okular::TextAnnotation *annotation, const Okular::Page *page)
{
    if (!annotation || !page) {
        return 0.0;
    }

    const Okular::NormalizedRect rect = annotation->boundingRectangle();
    return LatexNoteUtils::rectWidthInPoints(rect, page);
}

int textFontSizeForLatexStampAnnotation(const Okular::StampAnnotation *)
{
    return LatexNoteUtils::convertedTextFontSize();
}

Okular::NormalizedRect textAnnotationRectForStampSource(const Okular::StampAnnotation *annotation, const Okular::Page *page, const QFont &font)
{
    if (!annotation || !page || page->width() <= 0.0 || page->height() <= 0.0) {
        return annotation ? annotation->boundingRectangle() : Okular::NormalizedRect();
    }

    const Okular::NormalizedRect sourceRect = annotation->boundingRectangle();
    constexpr int padding = 2;
    const QRectF textArea = Okular::NormalizedRect(sourceRect.left, sourceRect.top, sourceRect.right, 1.0).geometryF(page->width(), page->height()).adjusted(padding, padding, -padding, -padding);
    if (textArea.width() <= 0.0 || textArea.height() <= 0.0) {
        return sourceRect;
    }

    const QFontMetricsF metrics(font);
    const QRectF textRect = metrics.boundingRect(textArea, Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, annotation->contents());
    double normalizedHeight = (textRect.height() + padding * 2) / page->height();
    const double minimumHeight = (metrics.height() + padding * 2) / page->height();
    normalizedHeight = qMax(normalizedHeight, minimumHeight);
    if (!std::isfinite(normalizedHeight) || normalizedHeight <= 0.0) {
        return sourceRect;
    }

    return Okular::NormalizedRect(sourceRect.left, sourceRect.top, sourceRect.right, qMin(1.0, sourceRect.top + normalizedHeight));
}

QColor textColorForLatexStampAnnotation(const Okular::StampAnnotation *annotation)
{
    return LatexNoteUtils::colorForLatexNote(annotation);
}

bool renderLatexNoteToCache(QWidget *parent, const QString &latexInput, const QColor &textColor, int fontSize, double maxWidth, QString *pdfFileName, GuiUtils::LatexRenderWarning *warning = nullptr)
{
    const LatexNoteUtils::RenderResult rendered = LatexNoteUtils::renderToCache(latexInput, textColor, fontSize, maxWidth);
    if (!rendered.ok) {
        KMessageBox::error(parent, rendered.errorMessage, i18n("LaTeX rendering failed"));
        return false;
    }

    *pdfFileName = rendered.pdfFileName;
    if (warning) {
        *warning = rendered.warning;
    }
    return true;
}

Okular::NormalizedRect latexStampBoundingRect(const Okular::NormalizedRect &sourceRect, const Okular::Page *page, const QSizeF &stampSizePoints, double scale = 1.0)
{
    return LatexNoteUtils::boundingRectForPdf(sourceRect, page, stampSizePoints, scale);
}

bool updateLatexNoteAppearance(QWidget *parent, Okular::Document *document, int pageNumber, Okular::StampAnnotation *stampAnnotation, const QColor &textColor, double layoutWidthPoints)
{
    if (!document || pageNumber == -1 || !stampAnnotation) {
        return false;
    }

    const Okular::Page *page = document->page(pageNumber);
    const QSizeF currentStampSizePoints = GuiUtils::latexNotePdfSizeInPointsForStamp(stampAnnotation->stampIconName());
    const double visualScale = LatexNoteUtils::scaleForLatexNote(stampAnnotation, page, currentStampSizePoints);
    QString pdfFileName;
    GuiUtils::LatexRenderWarning warning;
    if (!renderLatexNoteToCache(parent, stampAnnotation->contents(), textColor, latexFontSizeForTextAnnotation(nullptr), layoutWidthPoints, &pdfFileName, &warning)) {
        return false;
    }

    const QSizeF stampSizePoints = GuiUtils::latexNotePdfSizeInPointsForStamp(pdfFileName);
    if (!stampSizePoints.isValid() || stampSizePoints.isEmpty()) {
        KMessageBox::error(parent, i18n("Could not load the rendered LaTeX note PDF."), i18n("LaTeX rendering failed"));
        return false;
    }

    const Okular::NormalizedRect updatedRect = latexStampBoundingRect(stampAnnotation->boundingRectangle(), page, stampSizePoints, visualScale);
    const bool sameLayoutWidth = qAbs(stampAnnotation->latexNoteLayoutWidth() - layoutWidthPoints) < 1e-3;
    const bool sameScale = qAbs(stampAnnotation->latexNoteScale() - visualScale) < 1e-6;
    LatexNoteUtils::showRenderWarning(parent, warning);
    if (pdfFileName == stampAnnotation->stampIconName() && updatedRect == stampAnnotation->boundingRectangle() && stampAnnotation->style().color() == textColor && sameLayoutWidth && sameScale) {
        return true;
    }

    document->prepareToModifyAnnotationProperties(stampAnnotation);
    stampAnnotation->setStampIconName(pdfFileName);
    stampAnnotation->setLatexNoteLayoutWidth(layoutWidthPoints);
    stampAnnotation->setLatexNoteScale(visualScale);
    stampAnnotation->setBoundingRectangle(updatedRect);
    stampAnnotation->style().setColor(textColor);
    stampAnnotation->setModificationDate(QDateTime::currentDateTime());
    document->modifyPageAnnotationProperties(pageNumber, stampAnnotation);
    return true;
}

QTransform pageRotationMatrix(Okular::Rotation rotation)
{
    QTransform matrix;
    matrix.rotate(int(rotation) * 90);

    switch (rotation) {
    case Okular::Rotation90:
        matrix.translate(0, -1);
        break;
    case Okular::Rotation180:
        matrix.translate(-1, -1);
        break;
    case Okular::Rotation270:
        matrix.translate(-1, 0);
        break;
    default:
        break;
    }

    return matrix;
}

Okular::NormalizedRect rectForDocumentAdd(const Okular::NormalizedRect &baseRect, const Okular::Page *page)
{
    Okular::NormalizedRect rect = baseRect;
    if (page) {
        rect.transform(pageRotationMatrix(page->rotation()));
    }
    return rect;
}

}

AnnotationPopup::AnnotationPopup(Okular::Document *document, MenuMode mode, QWidget *parent)
    : mParent(parent)
    , mDocument(document)
    , mMenuMode(mode)
{
}

void AnnotationPopup::addAnnotation(Okular::Annotation *annotation, int pageNumber)
{
    AnnotPagePair pair(annotation, pageNumber);
    const auto sameAnnotation = [pair](const AnnotPagePair &existing) {
        if (existing == pair) {
            return true;
        }
        if (existing.pageNumber != pair.pageNumber || !existing.annotation || !pair.annotation) {
            return false;
        }
        const QString existingUniqueName = existing.annotation->uniqueName();
        return !existingUniqueName.isEmpty() && existingUniqueName == pair.annotation->uniqueName();
    };
    if (!std::any_of(mAnnotations.cbegin(), mAnnotations.cend(), sameAnnotation)) {
        mAnnotations.append(pair);
    }
}

void AnnotationPopup::exec(const QPoint point)
{
    if (mAnnotations.isEmpty()) {
        return;
    }

    QMenu menu(mParent);

    addActionsToMenu(&menu);

    menu.exec(point.isNull() ? QCursor::pos() : point);
}

void AnnotationPopup::addActionsToMenu(QMenu *menu)
{
    QAction *action = nullptr;

    if (mMenuMode == SingleAnnotationMode) {
        const bool onlyOne = (mAnnotations.count() == 1);

        const AnnotPagePair &pair = mAnnotations.at(0);

        menu->addAction(new OKMenuTitle(menu, i18ncp("Menu title", "Annotation", "%1 Annotations", mAnnotations.count())));

        action = menu->addAction(QIcon::fromTheme(QStringLiteral("comment")), i18n("&Open Pop-up Note"));
        action->setEnabled(onlyOne);
        connect(action, &QAction::triggered, menu, [this, pair] { doOpenAnnotationWindow(pair); });

        Okular::DocumentViewport vp = calculateAnnotationViewport(pair);
        bool isBookmarked = mDocument->bookmarkManager()->isBookmarked(vp);

        if (isBookmarked) {
            action = menu->addAction(QIcon::fromTheme(QStringLiteral("bookmark-remove")), i18n("Remove Bookmark"));
            action->setEnabled(onlyOne);
            connect(action, &QAction::triggered, menu, [this, pair] { doRemoveAnnotationBookmark(pair); });
        } else {
            action = menu->addAction(QIcon::fromTheme(QStringLiteral("bookmark-new")), i18n("Add Bookmark"));
            action->setEnabled(onlyOne);
            connect(action, &QAction::triggered, menu, [this, pair] { doAddAnnotationBookmark(pair); });
        }

        action = menu->addAction(QIcon::fromTheme(QStringLiteral("edit-copy")), i18n("Copy"));
        action->setEnabled(onlyOne && annotationSupportsCopy(pair.annotation));
        connect(action, &QAction::triggered, menu, [this, pair] { doCopyAnnotation(pair); });

        action = menu->addAction(QIcon::fromTheme(QStringLiteral("edit-paste")), i18n("Paste"));
        action->setEnabled(onlyOne && mDocument->isAllowed(Okular::AllowNotes) && clipboardHasAnnotations());
        connect(action, &QAction::triggered, menu, [this, pair] { doPasteAnnotation(pair); });

        if (!pair.annotation->contents().isEmpty()) {
            action = menu->addAction(QIcon::fromTheme(QStringLiteral("edit-copy")), i18n("Copy Text to Clipboard"));
            const bool copyAllowed = mDocument->isAllowed(Okular::AllowCopy);
            if (!copyAllowed) {
                action->setEnabled(false);
                action->setText(i18n("Copy forbidden by DRM"));
            }
            connect(action, &QAction::triggered, menu, [this, pair] { doCopyAnnotationText(pair); });
        }

        if (onlyOne && convertibleTextAnnotation(pair.annotation)) {
            action = menu->addAction(QIcon::fromTheme(QStringLiteral("text-x-tex")), i18nc("@action:inmenu", "Convert to LaTeX Note"));
            action->setEnabled(mDocument->isAllowed(Okular::AllowNotes) && mDocument->canRemovePageAnnotation(pair.annotation));
            connect(action, &QAction::triggered, menu, [this, pair] { doConvertTextAnnotationToLatex(pair); });
        }

        if (onlyOne && latexStampAnnotation(pair.annotation)) {
            action = menu->addAction(QIcon::fromTheme(QStringLiteral("format-fill-color")), i18nc("@action:inmenu", "Change LaTeX Note Color…"));
            action->setEnabled(mDocument->isAllowed(Okular::AllowNotes) && mDocument->canModifyPageAnnotation(pair.annotation));
            connect(action, &QAction::triggered, menu, [this, pair] { doChangeLatexAnnotationColor(pair); });

            action = menu->addAction(QIcon::fromTheme(QStringLiteral("transform-scale")), i18nc("@action:inmenu", "Set LaTeX Note Width…"));
            action->setEnabled(mDocument->isAllowed(Okular::AllowNotes) && mDocument->canModifyPageAnnotation(pair.annotation));
            connect(action, &QAction::triggered, menu, [this, pair] { doSetLatexAnnotationWidth(pair); });

            action = menu->addAction(QIcon::fromTheme(QStringLiteral("zoom-fit-width")), i18nc("@action:inmenu", "Fit LaTeX Note to Content"));
            action->setEnabled(mDocument->isAllowed(Okular::AllowNotes) && mDocument->canModifyPageAnnotation(pair.annotation));
            connect(action, &QAction::triggered, menu, [this, pair] { doFitLatexAnnotationToContent(pair); });

            action = menu->addAction(QIcon::fromTheme(QStringLiteral("zoom-original")), i18nc("@action:inmenu", "Reset LaTeX Note Scale"));
            action->setEnabled(mDocument->isAllowed(Okular::AllowNotes) && mDocument->canModifyPageAnnotation(pair.annotation));
            connect(action, &QAction::triggered, menu, [this, pair] { doResetLatexAnnotationScale(pair); });

            action = menu->addAction(QIcon::fromTheme(QStringLiteral("tool-text")), i18nc("@action:inmenu", "Convert Comment to Text"));
            action->setEnabled(mDocument->isAllowed(Okular::AllowNotes) && mDocument->canRemovePageAnnotation(pair.annotation));
            connect(action, &QAction::triggered, menu, [this, pair] { doConvertLatexAnnotationToText(pair); });
        }

        action = menu->addAction(QIcon::fromTheme(QStringLiteral("list-remove")), i18n("&Delete"));
        action->setEnabled(mDocument->isAllowed(Okular::AllowNotes));
        connect(action, &QAction::triggered, menu, [this] {
            for (const AnnotPagePair &pair : std::as_const(mAnnotations)) {
                doRemovePageAnnotation(pair);
            }
        });

        for (const AnnotPagePair &annot : std::as_const(mAnnotations)) {
            if (!mDocument->canRemovePageAnnotation(annot.annotation)) {
                action->setEnabled(false);
            }
        }

        action = menu->addAction(QIcon::fromTheme(QStringLiteral("configure")), i18n("&Properties"));
        action->setEnabled(onlyOne);
        connect(action, &QAction::triggered, menu, [this, pair] { doOpenPropertiesDialog(pair); });

        if (onlyOne && annotationHasFileAttachment(pair.annotation)) {
            const Okular::EmbeddedFile *embeddedFile = embeddedFileFromAnnotation(pair.annotation);
            if (embeddedFile) {
                const QString openText = i18nc("%1 is the name of the file to open", "&Open '%1'…", embeddedFile->name());
                action = menu->addAction(QIcon::fromTheme(QStringLiteral("document-open")), openText);
                connect(action, &QAction::triggered, menu, [this, pair] { doOpenEmbeddedFile(pair); });

                const QString saveText = i18nc("%1 is the name of the file to save", "&Save '%1'…", embeddedFile->name());
                menu->addSeparator();
                action = menu->addAction(QIcon::fromTheme(QStringLiteral("document-save")), saveText);
                connect(action, &QAction::triggered, menu, [this, pair] { doSaveEmbeddedFile(pair); });
            }
        }
    } else {
        for (const AnnotPagePair &pair : std::as_const(mAnnotations)) {
            menu->addAction(new OKMenuTitle(menu, GuiUtils::captionForAnnotation(pair.annotation)));

            action = menu->addAction(QIcon::fromTheme(QStringLiteral("comment")), i18n("&Open Pop-up Note"));
            connect(action, &QAction::triggered, menu, [this, pair] { doOpenAnnotationWindow(pair); });

            Okular::DocumentViewport vp = calculateAnnotationViewport(pair);
            bool isBookmarked = mDocument->bookmarkManager()->isBookmarked(vp);

            if (isBookmarked) {
                action = menu->addAction(QIcon::fromTheme(QStringLiteral("bookmark-remove")), i18n("Remove Bookmark"));
                connect(action, &QAction::triggered, menu, [this, pair] { doRemoveAnnotationBookmark(pair); });
            } else {
                action = menu->addAction(QIcon::fromTheme(QStringLiteral("bookmark-new")), i18n("Add Bookmark"));
                connect(action, &QAction::triggered, menu, [this, pair] { doAddAnnotationBookmark(pair); });
            }

            action = menu->addAction(QIcon::fromTheme(QStringLiteral("edit-copy")), i18n("Copy"));
            action->setEnabled(annotationSupportsCopy(pair.annotation));
            connect(action, &QAction::triggered, menu, [this, pair] { doCopyAnnotation(pair); });

            action = menu->addAction(QIcon::fromTheme(QStringLiteral("edit-paste")), i18n("Paste"));
            action->setEnabled(mDocument->isAllowed(Okular::AllowNotes) && clipboardHasAnnotations());
            connect(action, &QAction::triggered, menu, [this, pair] { doPasteAnnotation(pair); });

            if (!pair.annotation->contents().isEmpty()) {
                action = menu->addAction(QIcon::fromTheme(QStringLiteral("edit-copy")), i18n("Copy Text to Clipboard"));
                const bool copyAllowed = mDocument->isAllowed(Okular::AllowCopy);
                if (!copyAllowed) {
                    action->setEnabled(false);
                    action->setText(i18n("Copy forbidden by DRM"));
                }
                connect(action, &QAction::triggered, menu, [this, pair] { doCopyAnnotationText(pair); });
            }

            if (convertibleTextAnnotation(pair.annotation)) {
                action = menu->addAction(QIcon::fromTheme(QStringLiteral("text-x-tex")), i18nc("@action:inmenu", "Convert to LaTeX Note"));
                action->setEnabled(mDocument->isAllowed(Okular::AllowNotes) && mDocument->canRemovePageAnnotation(pair.annotation));
                connect(action, &QAction::triggered, menu, [this, pair] { doConvertTextAnnotationToLatex(pair); });
            }

            if (latexStampAnnotation(pair.annotation)) {
                action = menu->addAction(QIcon::fromTheme(QStringLiteral("format-fill-color")), i18nc("@action:inmenu", "Change LaTeX Note Color…"));
                action->setEnabled(mDocument->isAllowed(Okular::AllowNotes) && mDocument->canModifyPageAnnotation(pair.annotation));
                connect(action, &QAction::triggered, menu, [this, pair] { doChangeLatexAnnotationColor(pair); });

                action = menu->addAction(QIcon::fromTheme(QStringLiteral("transform-scale")), i18nc("@action:inmenu", "Set LaTeX Note Width…"));
                action->setEnabled(mDocument->isAllowed(Okular::AllowNotes) && mDocument->canModifyPageAnnotation(pair.annotation));
                connect(action, &QAction::triggered, menu, [this, pair] { doSetLatexAnnotationWidth(pair); });

                action = menu->addAction(QIcon::fromTheme(QStringLiteral("zoom-fit-width")), i18nc("@action:inmenu", "Fit LaTeX Note to Content"));
                action->setEnabled(mDocument->isAllowed(Okular::AllowNotes) && mDocument->canModifyPageAnnotation(pair.annotation));
                connect(action, &QAction::triggered, menu, [this, pair] { doFitLatexAnnotationToContent(pair); });

                action = menu->addAction(QIcon::fromTheme(QStringLiteral("zoom-original")), i18nc("@action:inmenu", "Reset LaTeX Note Scale"));
                action->setEnabled(mDocument->isAllowed(Okular::AllowNotes) && mDocument->canModifyPageAnnotation(pair.annotation));
                connect(action, &QAction::triggered, menu, [this, pair] { doResetLatexAnnotationScale(pair); });

                action = menu->addAction(QIcon::fromTheme(QStringLiteral("tool-text")), i18nc("@action:inmenu", "Convert Comment to Text"));
                action->setEnabled(mDocument->isAllowed(Okular::AllowNotes) && mDocument->canRemovePageAnnotation(pair.annotation));
                connect(action, &QAction::triggered, menu, [this, pair] { doConvertLatexAnnotationToText(pair); });
            }

            action = menu->addAction(QIcon::fromTheme(QStringLiteral("list-remove")), i18n("&Delete"));
            action->setEnabled(mDocument->isAllowed(Okular::AllowNotes) && mDocument->canRemovePageAnnotation(pair.annotation));
            connect(action, &QAction::triggered, menu, [this, pair] { doRemovePageAnnotation(pair); });

            action = menu->addAction(QIcon::fromTheme(QStringLiteral("configure")), i18n("&Properties"));
            connect(action, &QAction::triggered, menu, [this, pair] { doOpenPropertiesDialog(pair); });

            if (annotationHasFileAttachment(pair.annotation)) {
                const Okular::EmbeddedFile *embeddedFile = embeddedFileFromAnnotation(pair.annotation);
                if (embeddedFile) {
                    const QString openText = i18nc("%1 is the name of the file to open", "&Open '%1'…", embeddedFile->name());
                    action = menu->addAction(QIcon::fromTheme(QStringLiteral("document-open")), openText);
                    connect(action, &QAction::triggered, menu, [this, pair] { doOpenEmbeddedFile(pair); });

                    const QString saveText = i18nc("%1 is the name of the file to save", "&Save '%1'…", embeddedFile->name());
                    menu->addSeparator();
                    action = menu->addAction(QIcon::fromTheme(QStringLiteral("document-save")), saveText);
                    connect(action, &QAction::triggered, menu, [this, pair] { doSaveEmbeddedFile(pair); });
                }
            }
        }
    }
}

void AnnotationPopup::doCopyAnnotation(AnnotPagePair pair)
{
    if (mAnnotations.isEmpty() || !annotationSupportsCopy(pair.annotation)) {
        return;
    }

    QDomDocument document(QStringLiteral("okular-annotations"));
    QDomElement root = document.createElement(QStringLiteral("annotations"));
    root.setAttribute(QStringLiteral("version"), AnnotationPopup::annotationClipboardFormatVersion);
    document.appendChild(root);

    QDomElement annotationElement = document.createElement(QStringLiteral("annotation"));
    Okular::AnnotationUtils::storeAnnotation(pair.annotation, annotationElement, document);
    root.appendChild(annotationElement);

    auto *mimeData = new QMimeData();
    mimeData->setData(QLatin1String(annotationClipboardMimeType), document.toByteArray());
    QApplication::clipboard()->setMimeData(mimeData, QClipboard::Clipboard);
}

void AnnotationPopup::doPasteAnnotation(AnnotPagePair pair)
{
    pasteAnnotationToPage(pair.pageNumber);
}

void AnnotationPopup::doCopyAnnotationText(AnnotPagePair pair)
{
    const QString text = pair.annotation->contents();
    if (!text.isEmpty()) {
        QClipboard *cb = QApplication::clipboard();
        cb->setText(text, QClipboard::Clipboard);
    }
}

void AnnotationPopup::doConvertTextAnnotationToLatex(AnnotPagePair pair)
{
    if (pair.pageNumber == -1 || !mDocument->isAllowed(Okular::AllowNotes) || !mDocument->canRemovePageAnnotation(pair.annotation)) {
        return;
    }

    Okular::TextAnnotation *textAnnotation = convertibleTextAnnotation(pair.annotation);
    if (!textAnnotation) {
        return;
    }

    const QString latexInput = textAnnotation->contents();
    const int fontSize = latexFontSizeForTextAnnotation(textAnnotation);
    const QColor textColor = latexTextColorForTextAnnotation(textAnnotation);
    const Okular::Page *page = mDocument->page(pair.pageNumber);
    const double maxWidth = latexMaxWidthForTextAnnotation(textAnnotation, page);

    QString pdfFileName;
    GuiUtils::LatexRenderWarning warning;
    if (!renderLatexNoteToCache(mParent, latexInput, textColor, fontSize, maxWidth, &pdfFileName, &warning)) {
        return;
    }

    const QSizeF stampSizePoints = GuiUtils::latexNotePdfSizeInPointsForStamp(pdfFileName);
    if (!stampSizePoints.isValid() || stampSizePoints.isEmpty()) {
        KMessageBox::error(mParent, i18n("Could not load the rendered LaTeX note PDF."), i18n("LaTeX rendering failed"));
        return;
    }

    auto *stampAnnotation = new Okular::StampAnnotation();
    stampAnnotation->setStampIconName(pdfFileName);
    stampAnnotation->setLatexNoteLayoutWidth(maxWidth);
    stampAnnotation->setLatexNoteScale(1.0);
    stampAnnotation->setContents(latexInput);
    stampAnnotation->setBoundingRectangle(rectForDocumentAdd(latexStampBoundingRect(textAnnotation->boundingRectangle(), page, stampSizePoints), page));
    stampAnnotation->style() = textAnnotation->style();
    stampAnnotation->style().setColor(textColor);
    stampAnnotation->window().setSummary(i18n("LaTeX Note"));

    const QDateTime now = QDateTime::currentDateTime();
    stampAnnotation->setCreationDate(textAnnotation->creationDate().isValid() ? textAnnotation->creationDate() : now);
    stampAnnotation->setModificationDate(now);
    stampAnnotation->setAuthor(textAnnotation->author().isEmpty() ? Okular::Settings::identityAuthor() : textAnnotation->author());

    mDocument->addPageAnnotation(pair.pageNumber, stampAnnotation);
    mDocument->removePageAnnotation(pair.pageNumber, pair.annotation);
    LatexNoteUtils::showRenderWarning(mParent, warning);
}

void AnnotationPopup::doChangeLatexAnnotationColor(AnnotPagePair pair)
{
    if (pair.pageNumber == -1 || !mDocument->isAllowed(Okular::AllowNotes) || !mDocument->canModifyPageAnnotation(pair.annotation)) {
        return;
    }

    Okular::StampAnnotation *stampAnnotation = latexStampAnnotation(pair.annotation);
    if (!stampAnnotation) {
        return;
    }

    const QColor currentColor = textColorForLatexStampAnnotation(stampAnnotation);
    const QColor selectedColor = QColorDialog::getColor(currentColor, mParent, i18nc("@title:window", "Select LaTeX Note Color"));
    if (!selectedColor.isValid()) {
        return;
    }

    const Okular::Page *page = mDocument->page(pair.pageNumber);
    updateLatexNoteAppearance(mParent, mDocument, pair.pageNumber, stampAnnotation, selectedColor, LatexNoteUtils::layoutWidthForLatexNote(stampAnnotation, page));
}

void AnnotationPopup::doSetLatexAnnotationWidth(AnnotPagePair pair)
{
    if (pair.pageNumber == -1 || !mDocument->isAllowed(Okular::AllowNotes) || !mDocument->canModifyPageAnnotation(pair.annotation)) {
        return;
    }

    Okular::StampAnnotation *stampAnnotation = latexStampAnnotation(pair.annotation);
    const Okular::Page *page = mDocument->page(pair.pageNumber);
    if (!stampAnnotation || !page || page->width() <= 0.0) {
        return;
    }

    const double currentPercent = qBound(1.0, stampAnnotation->boundingRectangle().width() * 100.0, 100.0);
    bool ok = false;
    const double selectedPercent = QInputDialog::getDouble(mParent,
                                                           i18nc("@title:window", "Set LaTeX Note Width"),
                                                           i18nc("@label:spinbox", "Width (% of page):"),
                                                           currentPercent,
                                                           1.0,
                                                           100.0,
                                                           1,
                                                           &ok);
    if (!ok) {
        return;
    }

    const QSizeF currentStampSizePoints = GuiUtils::latexNotePdfSizeInPointsForStamp(stampAnnotation->stampIconName());
    const double visualScale = LatexNoteUtils::scaleForLatexNote(stampAnnotation, page, currentStampSizePoints);
    const double visibleWidthPoints = LatexNoteUtils::pageWidthInPoints(page) * selectedPercent / 100.0;
    const double layoutWidthPoints = LatexNoteUtils::layoutWidthForVisibleWidth(visibleWidthPoints, visualScale);
    if (!std::isfinite(layoutWidthPoints) || layoutWidthPoints <= 0.0) {
        return;
    }
    updateLatexNoteAppearance(mParent,
                              mDocument,
                              pair.pageNumber,
                              stampAnnotation,
                              textColorForLatexStampAnnotation(stampAnnotation),
                              layoutWidthPoints);
}

void AnnotationPopup::doFitLatexAnnotationToContent(AnnotPagePair pair)
{
    if (pair.pageNumber == -1 || !mDocument->isAllowed(Okular::AllowNotes) || !mDocument->canModifyPageAnnotation(pair.annotation)) {
        return;
    }

    Okular::StampAnnotation *stampAnnotation = latexStampAnnotation(pair.annotation);
    if (!stampAnnotation) {
        return;
    }

    updateLatexNoteAppearance(mParent, mDocument, pair.pageNumber, stampAnnotation, textColorForLatexStampAnnotation(stampAnnotation), 0.0);
}

void AnnotationPopup::doResetLatexAnnotationScale(AnnotPagePair pair)
{
    if (pair.pageNumber == -1 || !mDocument->isAllowed(Okular::AllowNotes) || !mDocument->canModifyPageAnnotation(pair.annotation)) {
        return;
    }

    Okular::StampAnnotation *stampAnnotation = latexStampAnnotation(pair.annotation);
    if (!stampAnnotation) {
        return;
    }

    const Okular::Page *page = mDocument->page(pair.pageNumber);
    const QSizeF stampSizePoints = GuiUtils::latexNotePdfSizeInPointsForStamp(stampAnnotation->stampIconName());
    if (!stampSizePoints.isValid() || stampSizePoints.isEmpty()) {
        KMessageBox::error(mParent, i18n("Could not load the rendered LaTeX note PDF."), i18n("LaTeX rendering failed"));
        return;
    }

    const Okular::NormalizedRect updatedRect = latexStampBoundingRect(stampAnnotation->boundingRectangle(), page, stampSizePoints, 1.0);
    if (updatedRect == stampAnnotation->boundingRectangle() && qAbs(stampAnnotation->latexNoteScale() - 1.0) < 1e-6) {
        return;
    }

    mDocument->prepareToModifyAnnotationProperties(stampAnnotation);
    stampAnnotation->setLatexNoteScale(1.0);
    stampAnnotation->setBoundingRectangle(updatedRect);
    stampAnnotation->setModificationDate(QDateTime::currentDateTime());
    mDocument->modifyPageAnnotationProperties(pair.pageNumber, stampAnnotation);
}

void AnnotationPopup::doConvertLatexAnnotationToText(AnnotPagePair pair)
{
    if (pair.pageNumber == -1 || !mDocument->isAllowed(Okular::AllowNotes) || !mDocument->canRemovePageAnnotation(pair.annotation)) {
        return;
    }

    Okular::StampAnnotation *stampAnnotation = latexStampAnnotation(pair.annotation);
    if (!stampAnnotation) {
        return;
    }

    const Okular::Page *page = mDocument->page(pair.pageNumber);
    auto *textAnnotation = new Okular::TextAnnotation();
    textAnnotation->setFlags(textAnnotation->flags() | Okular::Annotation::FixedRotation);
    textAnnotation->setContents(stampAnnotation->contents());
    textAnnotation->setTextType(Okular::TextAnnotation::InPlace);
    textAnnotation->setInplaceIntent(Okular::TextAnnotation::TypeWriter);
    textAnnotation->setTextColor(textColorForLatexStampAnnotation(stampAnnotation));

    QFont font(QStringLiteral("Noto Sans"));
    font.setPointSize(textFontSizeForLatexStampAnnotation(stampAnnotation));
    textAnnotation->setTextFont(font);
    textAnnotation->setBoundingRectangle(rectForDocumentAdd(textAnnotationRectForStampSource(stampAnnotation, page, font), page));
    textAnnotation->style().setColor(QColor(255, 255, 255, 0));
    textAnnotation->style().setWidth(0);
    textAnnotation->window().setSummary(i18n("Typewriter"));

    const QDateTime now = QDateTime::currentDateTime();
    textAnnotation->setCreationDate(stampAnnotation->creationDate().isValid() ? stampAnnotation->creationDate() : now);
    textAnnotation->setModificationDate(now);
    textAnnotation->setAuthor(stampAnnotation->author().isEmpty() ? Okular::Settings::identityAuthor() : stampAnnotation->author());

    mDocument->addPageAnnotation(pair.pageNumber, textAnnotation);
    mDocument->removePageAnnotation(pair.pageNumber, pair.annotation);
}

void AnnotationPopup::pasteAnnotationToPage(int pageNumber, const Okular::NormalizedPoint *targetPoint)
{
    if (pageNumber == -1 || !mDocument->isAllowed(Okular::AllowNotes)) {
        return;
    }

    const QMimeData *mimeData = QApplication::clipboard()->mimeData();
    if (!mimeData || !mimeData->hasFormat(QLatin1String(annotationClipboardMimeType))) {
        return;
    }

    QDomDocument document;
    if (!document.setContent(mimeData->data(QLatin1String(annotationClipboardMimeType)))) {
        return;
    }

    QDomElement root = document.documentElement();
    if (root.tagName() != QLatin1String("annotations")) {
        return;
    }
    if (!clipboardFormatVersionSupported(root)) {
        return;
    }

    QList<Okular::Annotation *> annotations;
    Okular::NormalizedRect unionRect;
    bool hasUnionRect = false;
    for (QDomElement element = root.firstChildElement(QStringLiteral("annotation")); !element.isNull(); element = element.nextSiblingElement(QStringLiteral("annotation"))) {
        Okular::Annotation *annotation = Okular::AnnotationUtils::createAnnotation(element);
        if (!annotation) {
            continue;
        }

        // Avoid duplicate uniqueName collisions when pasting annotations.
        annotation->setUniqueName(QString());

        const Okular::NormalizedRect rect = annotation->boundingRectangle();
        if (!rect.isNull()) {
            unionRect = hasUnionRect ? (unionRect | rect) : rect;
            hasUnionRect = true;
        }
        annotations.append(annotation);
    }

    if (annotations.isEmpty()) {
        return;
    }

    Okular::NormalizedPoint offset(0.02, 0.02);
    if (targetPoint && hasUnionRect) {
        const double centerX = (unionRect.left + unionRect.right) / 2.0;
        const double centerY = (unionRect.top + unionRect.bottom) / 2.0;
        offset = Okular::NormalizedPoint(targetPoint->x - centerX, targetPoint->y - centerY);
    }

    for (Okular::Annotation *annotation : std::as_const(annotations)) {
        annotation->translate(offset);
        mDocument->addPageAnnotation(pageNumber, annotation);
    }
}

bool AnnotationPopup::clipboardHasAnnotations()
{
    const QMimeData *mimeData = QApplication::clipboard()->mimeData();
    return mimeData && mimeData->hasFormat(QLatin1String(annotationClipboardMimeType));
}

void AnnotationPopup::doRemovePageAnnotation(AnnotPagePair pair)
{
    if (pair.pageNumber != -1) {
        mDocument->removePageAnnotation(pair.pageNumber, pair.annotation);
    }
}

void AnnotationPopup::doOpenAnnotationWindow(AnnotPagePair pair)
{
    Q_EMIT openAnnotationWindow(pair.annotation, pair.pageNumber);
}

void AnnotationPopup::doOpenPropertiesDialog(AnnotPagePair pair)
{
    if (pair.pageNumber != -1) {
        AnnotsPropertiesDialog propdialog(mParent, mDocument, pair.pageNumber, pair.annotation);
        propdialog.exec();
    }
}

void AnnotationPopup::doSaveEmbeddedFile(AnnotPagePair pair)
{
    Okular::EmbeddedFile *embeddedFile = embeddedFileFromAnnotation(pair.annotation);
    GuiUtils::saveEmbeddedFile(embeddedFile, mParent);
}

void AnnotationPopup::doOpenEmbeddedFile(AnnotPagePair pair)
{
    Okular::EmbeddedFile *embeddedFile = embeddedFileFromAnnotation(pair.annotation);
    if (!embeddedFile) {
        return;
    }
    // preserve the file extension so the OS knows which app to use
    const QString name = embeddedFile->name();
    const QString extension = QFileInfo(name).suffix();
    const QString templateName = QDir::tempPath() + QLatin1Char('/') + QFileInfo(name).baseName() + QStringLiteral(".XXXXXX.") + extension;
    QTemporaryFile tempFile(templateName);
    tempFile.setAutoRemove(false);

    if (tempFile.open()) {
        tempFile.write(embeddedFile->data());
        tempFile.setPermissions(QFile::ReadOwner);
        const QString fileName = tempFile.fileName();
        tempFile.close();
        auto *job = new KIO::OpenUrlJob(QUrl::fromLocalFile(fileName));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, mParent));
        job->start();
    }
}

// This code comes from Reviews::activated in side_reviews.cpp
Okular::DocumentViewport AnnotationPopup::calculateAnnotationViewport(AnnotPagePair pair) const
{
    Okular::DocumentViewport vp;

    const Okular::Page *page = mDocument->page(pair.pageNumber);
    if (!page || !pair.annotation) {
        return vp; // Return empty viewport on error
    }

    QRect rect = Okular::AnnotationUtils::annotationGeometry(pair.annotation, page->width(), page->height());
    Okular::NormalizedRect nr(rect, (int)page->width(), (int)page->height());

    vp.pageNumber = pair.pageNumber;
    vp.rePos.enabled = true;
    vp.rePos.pos = Okular::DocumentViewport::Center;
    vp.rePos.normalizedX = (nr.right + nr.left) / 2.0;
    vp.rePos.normalizedY = (nr.bottom + nr.top) / 2.0;

    return vp;
}

void AnnotationPopup::doAddAnnotationBookmark(AnnotPagePair pair)
{
    if (pair.pageNumber != -1) {
        Okular::DocumentViewport vp = calculateAnnotationViewport(pair);
        QString title = pair.annotation->contents();
        if (title.isEmpty()) {
            mDocument->bookmarkManager()->addBookmark(mDocument->currentDocument(), vp);
        } else {
            mDocument->bookmarkManager()->addBookmark(mDocument->currentDocument(), vp, title);
        }
    }
}

void AnnotationPopup::doRemoveAnnotationBookmark(AnnotPagePair pair)
{
    if (pair.pageNumber != -1) {
        Okular::DocumentViewport vp = calculateAnnotationViewport(pair);
        mDocument->bookmarkManager()->removeBookmark(vp);
    }
}

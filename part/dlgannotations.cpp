/*
    SPDX-FileCopyrightText: 2006 Pino Toscano <toscano.pino@tiscali.it>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "dlgannotations.h"

#include "latexrenderer.h"
#include "widgetannottools.h"

#include <KLocalizedString>

#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

namespace
{
QString stemTeXLightHtml(bool ok)
{
    return QStringLiteral("<span style=\"color:%1;font-size:14px;\">&#9679;</span>").arg(ok ? QStringLiteral("#179c48") : QStringLiteral("#c62828"));
}

QString stemTeXStageText(const GuiUtils::StemTeXStatus &status)
{
    // Mirrors StemTeXRenderStage in stemtex_renderer.h.
    switch (status.renderStage) {
    case 1:
        return i18nc("@info Config dialog, annotations page, StemTeX engine status", "latest request queued");
    case 2:
        return i18nc("@info Config dialog, annotations page, StemTeX engine status", "XeTeX typesetting");
    case 3:
        return i18nc("@info Config dialog, annotations page, StemTeX engine status", "xdvipdfmx converting PDF");
    case 4:
        return i18nc("@info Config dialog, annotations page, StemTeX engine status", "worker rebuilding");
    case 5:
        return i18nc("@info Config dialog, annotations page, StemTeX engine status", "renderer stopping");
    case 0:
    default:
        return i18nc("@info Config dialog, annotations page, StemTeX engine status", "idle");
    }
}
}

DlgAnnotations::DlgAnnotations(QWidget *parent)
    : QWidget(parent)
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidgetResizable(true);
    mainLayout->addWidget(scrollArea);

    auto *content = new QWidget(scrollArea);
    scrollArea->setWidget(content);

    QFormLayout *layout = new QFormLayout(content);

    // BEGIN Annotation toolbar: Combo box to set the annotation toolbar associated to annotation action in tool menu
    QComboBox *primaryAnnotationToolBar = new QComboBox(this);
    primaryAnnotationToolBar->addItem(i18nc("item:inlistbox Config dialog, general page", "Full Annotation Toolbar"));
    primaryAnnotationToolBar->addItem(i18nc("item:inlistbox Config dialog, general page", "Quick Annotation Toolbar"));
    primaryAnnotationToolBar->setObjectName(QStringLiteral("kcfg_PrimaryAnnotationToolBar"));
    layout->addRow(i18nc("label:listbox Config dialog, general page", "Annotation toolbar:"), primaryAnnotationToolBar);
    // END Annotation toolbar

    // BEGIN Author row: Line edit to set the annotation’s default author value.
    QLineEdit *authorLineEdit = new QLineEdit(this);
    authorLineEdit->setObjectName(QStringLiteral("kcfg_IdentityAuthor"));
    layout->addRow(i18nc("@label:textbox Config dialog, annotations page", "Author:"), authorLineEdit);

    QLabel *authorInfoLabel = new QLabel(this);
    authorInfoLabel->setText(
        i18nc("@info Config dialog, annotations page", "<b>Note:</b> the information here is used only for annotations. The information is saved in annotated documents, and so will be transmitted together with the document."));
    authorInfoLabel->setWordWrap(true);
    layout->addRow(authorInfoLabel);
    // END Author row

    // Silly 1Em spacer:
    layout->addRow(new QLabel(this));

    QLabel *latexLabel = new QLabel(this);
    latexLabel->setText(i18nc("@label Config dialog, annotations page, heading line for LaTeX note settings", "<h3>LaTeX Notes</h3>"));
    layout->addRow(latexLabel);

    QSpinBox *latexAnnotationFontSize = new QSpinBox(this);
    latexAnnotationFontSize->setObjectName(QStringLiteral("kcfg_LatexAnnotationFontSize"));
    latexAnnotationFontSize->setRange(1, 72);
    latexAnnotationFontSize->setSuffix(i18nc("@item:spinbox Config dialog, annotations page, font size unit", " pt"));
    layout->addRow(i18nc("@label:spinbox Config dialog, annotations page", "LaTeX render font size:"), latexAnnotationFontSize);

    QSpinBox *latexTextAnnotationFontSize = new QSpinBox(this);
    latexTextAnnotationFontSize->setObjectName(QStringLiteral("kcfg_LatexTextAnnotationFontSize"));
    latexTextAnnotationFontSize->setRange(1, 72);
    latexTextAnnotationFontSize->setSuffix(i18nc("@item:spinbox Config dialog, annotations page, font size unit", " pt"));
    layout->addRow(i18nc("@label:spinbox Config dialog, annotations page", "Converted text font size:"), latexTextAnnotationFontSize);

    QLineEdit *latexExecutablePath = new QLineEdit(this);
    latexExecutablePath->setObjectName(QStringLiteral("kcfg_LatexExecutablePath"));
    latexExecutablePath->setPlaceholderText(i18nc("@info:placeholder Config dialog, annotations page", "Leave empty to search PATH"));
    layout->addRow(i18nc("@label:textbox Config dialog, annotations page", "XeLaTeX executable:"), latexExecutablePath);

    QComboBox *latexRenderBackend = new QComboBox(this);
    latexRenderBackend->addItem(i18nc("@item:inlistbox Config dialog, annotations page", "Auto (system TeX, then MicroTeX fallback)"));
    latexRenderBackend->addItem(i18nc("@item:inlistbox Config dialog, annotations page", "System TeX only"));
    latexRenderBackend->addItem(i18nc("@item:inlistbox Config dialog, annotations page", "MicroTeX only"));
    latexRenderBackend->addItem(i18nc("@item:inlistbox Config dialog, annotations page", "StemTeX hot XeTeX only"));
    latexRenderBackend->setObjectName(QStringLiteral("kcfg_LatexRenderBackend"));
    layout->addRow(i18nc("@label:listbox Config dialog, annotations page", "LaTeX renderer:"), latexRenderBackend);

    m_stemTeXStatusLabel = new QLabel(this);
    m_stemTeXStatusLabel->setTextFormat(Qt::RichText);
    layout->addRow(i18nc("@label Config dialog, annotations page", "StemTeX status:"), m_stemTeXStatusLabel);
    m_stemTeXStatusTimer = new QTimer(this);
    m_stemTeXStatusTimer->setInterval(500);
    connect(m_stemTeXStatusTimer, &QTimer::timeout, this, &DlgAnnotations::refreshStemTeXStatus);
    m_stemTeXStatusTimer->start();
    refreshStemTeXStatus();

    QPlainTextEdit *latexPreamble = new QPlainTextEdit(this);
    latexPreamble->setObjectName(QStringLiteral("kcfg_LatexPreamble"));
    latexPreamble->setMinimumHeight(latexPreamble->fontMetrics().lineSpacing() * 7);
    layout->addRow(i18nc("@label:textbox Config dialog, annotations page", "LaTeX preamble:"), latexPreamble);

    // Silly 1Em spacer:
    layout->addRow(new QLabel(this));

    // BEGIN Quick annotation tools section: WidgetAnnotTools manages tools.
    QLabel *toolsLabel = new QLabel(this);
    toolsLabel->setText(i18nc("@label Config dialog, annotations page, heading line for Quick Annotations tool manager", "<h3>Quick Annotation Tools</h3>"));
    layout->addRow(toolsLabel);

    WidgetAnnotTools *kcfg_QuickAnnotationTools = new WidgetAnnotTools(this);
    kcfg_QuickAnnotationTools->setObjectName(QStringLiteral("kcfg_QuickAnnotationTools"));
    layout->addRow(kcfg_QuickAnnotationTools);
    // END Quick annotation tools section
}

void DlgAnnotations::refreshStemTeXStatus()
{
    if (!m_stemTeXStatusLabel) {
        return;
    }

    const GuiUtils::StemTeXStatus status = GuiUtils::LatexRenderer::stemTeXStatus();
    QString text;
    if (!status.supported) {
        text = stemTeXLightHtml(false) + QStringLiteral("<span style=\"color:transparent;font-size:14px;\">&#9679;</span>") + status.note.toHtmlEscaped();
        m_stemTeXStatusLabel->setText(text);
        return;
    }

    text += stemTeXLightHtml(status.primaryReady);
    const int spareTarget = qMax(2, status.spareTarget);
    for (int i = 0; i < spareTarget; ++i) {
        text += stemTeXLightHtml(i < status.spareReady);
    }

    QString note = status.note;
    if (note.isEmpty() && status.ready) {
        note = stemTeXStageText(status);
    }
    if (status.asyncRunning) {
        note += i18nc("@info Config dialog, annotations page, StemTeX engine status", ", running job %1", QString::number(status.runningJobId));
    }
    if (status.asyncPending) {
        note += i18nc("@info Config dialog, annotations page, StemTeX engine status", ", pending job %1", QString::number(status.pendingJobId));
    }
    if (status.spareRebuilding) {
        if (note.isEmpty()) {
            note = i18nc("@info Config dialog, annotations page, StemTeX engine status", "rebuilding");
        } else {
            note += i18nc("@info Config dialog, annotations page, StemTeX engine status", ", rebuilding spare");
        }
    } else if (note.isEmpty() && status.initializing) {
        note = i18nc("@info Config dialog, annotations page, StemTeX engine status", "starting");
    } else if (note.isEmpty() && status.ready && status.primaryReady && status.spareReady >= qMin(2, spareTarget)) {
        note = i18nc("@info Config dialog, annotations page, StemTeX engine status", "ready");
    }

    if (!note.isEmpty()) {
        text += QStringLiteral("<span style=\"color:transparent;font-size:14px;\">&#9679;</span>");
        text += note.toHtmlEscaped();
    }
    m_stemTeXStatusLabel->setText(text);
}

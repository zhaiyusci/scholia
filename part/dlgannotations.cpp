/*
    SPDX-FileCopyrightText: 2006 Pino Toscano <toscano.pino@tiscali.it>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "dlgannotations.h"

#include "latexrenderer.h"
#include "settings.h"
#include "widgetannottools.h"

#include <KLocalizedString>

#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
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

    m_stemTeXProfileNameEdit = new QLineEdit(this);
    m_stemTeXProfileNameEdit->setObjectName(QStringLiteral("kcfg_LatexStemtexProfileName"));
    m_stemTeXProfileNameEdit->hide();

    m_stemTeXProfileCombo = new QComboBox(this);
    reloadStemTeXProfiles();
    connect(m_stemTeXProfileCombo, &QComboBox::currentIndexChanged, this, [this]() {
        if (!m_stemTeXProfileNameEdit || !m_stemTeXProfileCombo) {
            return;
        }
        m_stemTeXProfileNameEdit->setText(m_stemTeXProfileCombo->currentData().toString());
    });
    connect(m_stemTeXProfileNameEdit, &QLineEdit::textChanged, this, &DlgAnnotations::syncStemTeXProfileCombo);
    m_stemTeXProfileNameEdit->setText(Okular::Settings::latexStemtexProfileName());
    syncStemTeXProfileCombo(m_stemTeXProfileNameEdit->text());
    layout->addRow(i18nc("@label:listbox Config dialog, annotations page", "StemTeX profile:"), m_stemTeXProfileCombo);

    auto *texmfRow = new QWidget(this);
    auto *texmfLayout = new QHBoxLayout(texmfRow);
    texmfLayout->setContentsMargins(0, 0, 0, 0);
    texmfLayout->setSpacing(6);
    auto *texmfRoot = new QLineEdit(texmfRow);
    texmfRoot->setObjectName(QStringLiteral("kcfg_LatexStemtexTexmfRoot"));
    texmfRoot->setPlaceholderText(i18nc("@info:placeholder Config dialog, annotations page", "Leave empty to use bundled StemTeX TeX tree"));
    texmfRoot->setToolTip(i18nc("@info:tooltip Config dialog, annotations page", "Directory containing texmf-dist. Empty uses %1.", QDir::toNativeSeparators(GuiUtils::LatexRenderer::defaultStemTeXTexmfRoot())));
    auto *browseTexmf = new QPushButton(i18nc("@action:button Config dialog, annotations page", "Browse..."), texmfRow);
    connect(browseTexmf, &QPushButton::clicked, this, [this, texmfRoot]() {
        const QString start = texmfRoot->text().trimmed().isEmpty() ? GuiUtils::LatexRenderer::defaultStemTeXTexmfRoot() : texmfRoot->text().trimmed();
        const QString selected = QFileDialog::getExistingDirectory(this, i18nc("@title:window Config dialog, annotations page", "Select TeXLive package/font tree"), start);
        if (!selected.isEmpty()) {
            texmfRoot->setText(QDir::toNativeSeparators(QDir::cleanPath(selected)));
        }
    });
    texmfLayout->addWidget(texmfRoot, 1);
    texmfLayout->addWidget(browseTexmf);
    layout->addRow(i18nc("@label:textbox Config dialog, annotations page", "StemTeX TeX tree:"), texmfRow);

    m_stemTeXStatusLabel = new QLabel(this);
    m_stemTeXStatusLabel->setTextFormat(Qt::RichText);
    layout->addRow(i18nc("@label Config dialog, annotations page", "StemTeX status:"), m_stemTeXStatusLabel);
    m_stemTeXStatusTimer = new QTimer(this);
    m_stemTeXStatusTimer->setInterval(500);
    connect(m_stemTeXStatusTimer, &QTimer::timeout, this, &DlgAnnotations::refreshStemTeXStatus);
    m_stemTeXStatusTimer->start();
    refreshStemTeXStatus();

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

void DlgAnnotations::reloadStemTeXProfiles()
{
    if (!m_stemTeXProfileCombo) {
        return;
    }

    const QString currentProfile = Okular::Settings::latexStemtexProfileName().trimmed();
    m_stemTeXProfileCombo->clear();
    m_stemTeXProfileCombo->addItem(i18nc("@item:inlistbox Config dialog, annotations page", "Auto (recommended bundled profile)"), QString());

    const QStringList profiles = GuiUtils::LatexRenderer::stemTeXProfileNames();
    for (const QString &profile : profiles) {
        m_stemTeXProfileCombo->addItem(profile, profile);
    }

    if (!currentProfile.isEmpty() && !profiles.contains(currentProfile)) {
        m_stemTeXProfileCombo->addItem(i18nc("@item:inlistbox Config dialog, annotations page", "Missing: %1", currentProfile), currentProfile);
    }

    m_stemTeXProfileCombo->setEnabled(!profiles.isEmpty());
    m_stemTeXProfileCombo->setToolTip(profiles.isEmpty() ? i18nc("@info:tooltip Config dialog, annotations page", "No bundled StemTeX profiles were found.") : QString());
}

void DlgAnnotations::syncStemTeXProfileCombo(const QString &profileName)
{
    if (!m_stemTeXProfileCombo) {
        return;
    }

    const QString target = profileName.trimmed();
    for (int i = 0; i < m_stemTeXProfileCombo->count(); ++i) {
        if (m_stemTeXProfileCombo->itemData(i).toString() == target) {
            if (m_stemTeXProfileCombo->currentIndex() != i) {
                m_stemTeXProfileCombo->setCurrentIndex(i);
            }
            return;
        }
    }

    if (!target.isEmpty()) {
        m_stemTeXProfileCombo->addItem(i18nc("@item:inlistbox Config dialog, annotations page", "Missing: %1", target), target);
        m_stemTeXProfileCombo->setCurrentIndex(m_stemTeXProfileCombo->count() - 1);
    }
}

void DlgAnnotations::refreshStemTeXStatus()
{
    if (!m_stemTeXStatusLabel) {
        return;
    }

    const GuiUtils::StemTeXStatus status = GuiUtils::LatexRenderer::stemTeXStatus();
    QString text;
    if (!status.supported) {
        text = stemTeXLightHtml(false) + QStringLiteral("&nbsp;") + status.note.toHtmlEscaped();
        m_stemTeXStatusLabel->setText(text);
        return;
    }

    text += stemTeXLightHtml(status.ready && status.primaryReady);

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
    if (note.isEmpty() && status.initializing) {
        note = i18nc("@info Config dialog, annotations page, StemTeX engine status", "starting");
    } else if (note.isEmpty() && status.ready && status.primaryReady) {
        note = i18nc("@info Config dialog, annotations page, StemTeX engine status", "ready");
    }

    if (!note.isEmpty()) {
        text += QStringLiteral("&nbsp;");
        text += note.toHtmlEscaped();
    }
    m_stemTeXStatusLabel->setText(text);
}

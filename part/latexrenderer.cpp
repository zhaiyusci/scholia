/*
    SPDX-FileCopyrightText: 2004 Duncan Mac-Vicar Prett <duncan@kde.org>
    SPDX-FileCopyrightText: 2004-2005 Olivier Goffart <ogoffart@kde.org>
    SPDX-FileCopyrightText: 2011 Niels Ole Salscheider
    <niels_ole@salscheider-online.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "latexrenderer.h"

#include <cmath>

#include <QDebug>

#include <KLocalizedString>
#include <KProcess>

#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QPen>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTextStream>

#include "gui/debug_ui.h"

namespace GuiUtils
{
LatexRenderer::LatexRenderer()
{
}

LatexRenderer::~LatexRenderer()
{
    for (const QString &file : std::as_const(m_fileList)) {
        QFile::remove(file);
    }
}

LatexRenderer::Error LatexRenderer::renderLatexInHtml(QString &html, const QColor &textColor, int fontSize, int resolution, QString &latexOutput)
{
    if (!html.contains(QStringLiteral("$$"))) {
        return NoError;
    }

    // this searches for $$formula$$
    static const QRegularExpression rg(QStringLiteral("\\$\\$.+?\\$\\$"));
    QRegularExpressionMatchIterator matchIt = rg.globalMatch(html);

    QMap<QString, QString> replaceMap;
    while (matchIt.hasNext()) {
        QRegularExpressionMatch match = matchIt.next();
        const QString matchedString = match.captured(0);

        QString formul = matchedString;
        // first remove the $$ delimiters on start and end
        formul.remove(QStringLiteral("$$"));
        // then trim the result, so we can skip totally empty/whitespace-only formulas
        formul = formul.trimmed();
        if (formul.isEmpty() || !securityCheck(formul)) {
            continue;
        }

        // unescape formula
        formul.replace(QLatin1String("&gt;"), QLatin1String(">"));
        formul.replace(QLatin1String("&lt;"), QLatin1String("<"));
        formul.replace(QLatin1String("&amp;"), QLatin1String("&"));
        formul.replace(QLatin1String("&quot;"), QLatin1String("\""));
        formul.replace(QLatin1String("&apos;"), QLatin1String("\'"));
        formul.replace(QLatin1String("<br>"), QLatin1String(" "));

        QString fileName;
        Error returnCode = handleLatex(fileName, nullptr, formul, textColor, fontSize, resolution, latexOutput);
        if (returnCode != NoError) {
            return returnCode;
        }

        replaceMap[matchedString] = fileName;
    }

    if (replaceMap.isEmpty()) { // we haven't found any LaTeX strings
        return NoError;
    }

    int imagePxWidth, imagePxHeight;
    for (QMap<QString, QString>::ConstIterator it = replaceMap.constBegin(); it != replaceMap.constEnd(); ++it) {
        QImage theImage(*it);
        if (theImage.isNull()) {
            continue;
        }
        imagePxWidth = theImage.width();
        imagePxHeight = theImage.height();
        QString escapedLATEX = it.key().toHtmlEscaped().replace(QLatin1Char('"'), QLatin1String("&quot;")); // we need  the escape quotes because that string will be in a title="" argument, but not the \n
        html.replace(it.key(),
                     QStringLiteral(" <img width=\"") + QString::number(imagePxWidth) + QStringLiteral("\" height=\"") + QString::number(imagePxHeight) + QStringLiteral("\" align=\"middle\" src=\"") + (*it) + QStringLiteral("\"  alt=\"") +
                         escapedLATEX + QStringLiteral("\" title=\"") + escapedLATEX + QStringLiteral("\"  /> "));
    }
    return NoError;
}

bool LatexRenderer::mightContainLatex(const QString &text)
{
    if (!text.contains(QStringLiteral("$$"))) {
        return false;
    }

    // this searches for $$formula$$
    static const QRegularExpression rg(QStringLiteral("\\$\\$.+?\\$\\$"));
    if (!rg.match(text).hasMatch()) {
        return false;
    }

    return true;
}

QString LatexRenderer::compactErrorMessage(const QString &latexOutput)
{
    const QStringList lines = latexOutput.split(QLatin1Char('\n'));
    QString message;
    QString context;
    bool foundErrorLine = false;

    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        if (!foundErrorLine && trimmed.startsWith(QLatin1Char('!'))) {
            message = trimmed.mid(1).trimmed();
            foundErrorLine = true;
            continue;
        }
        if (foundErrorLine && trimmed.startsWith(QLatin1String("l."))) {
            context = trimmed;
            break;
        }
        if (message.isEmpty()) {
            message = trimmed;
        }
    }

    if (message.isEmpty()) {
        message = i18n("Unknown LaTeX error.");
    }
    if (!context.isEmpty()) {
        message = i18nc("LaTeX error with compiler context", "%1 (%2)", message, context);
    }

    message = message.simplified();
    constexpr int maxLength = 180;
    if (message.size() > maxLength) {
        message = message.left(maxLength - 3) + QStringLiteral("...");
    }
    return message;
}

QImage LatexRenderer::createErrorImage(const QString &message, int resolution)
{
    QImage image(QSize(960, 260), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    image.setDotsPerMeterX(qRound(resolution / 0.0254));
    image.setDotsPerMeterY(qRound(resolution / 0.0254));

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRect frame = image.rect().adjusted(12, 12, -12, -12);
    painter.setPen(QPen(QColor(170, 32, 32), 5));
    painter.setBrush(QColor(255, 250, 245, 245));
    painter.drawRoundedRect(frame, 10, 10);

    QFont titleFont = painter.font();
    titleFont.setBold(true);
    titleFont.setPixelSize(42);
    painter.setFont(titleFont);
    painter.setPen(QColor(120, 16, 16));
    painter.drawText(frame.adjusted(28, 22, -28, -120), Qt::AlignLeft | Qt::AlignTop, i18n("LaTeX error"));

    QFont bodyFont = painter.font();
    bodyFont.setBold(false);
    bodyFont.setPixelSize(32);
    painter.setFont(bodyFont);
    painter.setPen(QColor(55, 45, 40));
    painter.drawText(frame.adjusted(28, 88, -28, -24), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, message);
    painter.end();

    return image;
}

LatexRenderer::Error LatexRenderer::renderLatexToImage(const QString &latexFormula, const QColor &textColor, int fontSize, int resolution, QString &fileName, QString &latexOutput)
{
    QString formula = latexFormula.trimmed();
    if (formula.isEmpty()) {
        fileName.clear();
        return LatexFailed;
    }
    if (!securityCheck(formula)) {
        fileName.clear();
        latexOutput = QStringLiteral("The formula contains unsupported LaTeX commands.");
        return LatexFailed;
    }

    return handleLatex(fileName, nullptr, formula, textColor, fontSize, resolution, latexOutput, BodyMode::Source);
}

LatexRenderer::Error LatexRenderer::renderLatexToPdfAndImage(const QString &latexFormula, const QColor &textColor, int fontSize, int resolution, QString &imageFileName, QString &pdfFileName, QString &latexOutput, double maxWidth)
{
    QString formula = latexFormula.trimmed();
    if (formula.isEmpty()) {
        imageFileName.clear();
        pdfFileName.clear();
        return LatexFailed;
    }
    if (!securityCheck(formula)) {
        imageFileName.clear();
        pdfFileName.clear();
        latexOutput = QStringLiteral("The formula contains unsupported LaTeX commands.");
        return LatexFailed;
    }

    return handleLatex(imageFileName, &pdfFileName, formula, textColor, fontSize, resolution, latexOutput, BodyMode::Source, maxWidth);
}

LatexRenderer::Error LatexRenderer::handleLatex(QString &fileName, QString *pdfFileName, const QString &latexSource, const QColor &textColor, int fontSize, int resolution, QString &latexOutput, BodyMode bodyMode, double maxWidth)
{
    KProcess latexProc;
    KProcess dvipngProc;
    KProcess pdfToImageProc;
    const bool renderSource = bodyMode == BodyMode::Source;
    const bool constrainSourceWidth = renderSource && std::isfinite(maxWidth) && maxWidth > 0.0;
    const QString sourceWidth = constrainSourceWidth ? QString::number(maxWidth, 'f', 3) + QStringLiteral("bp") : QString();

    QTemporaryFile *tempFile = new QTemporaryFile(QDir::tempPath() + QLatin1String("/okular_kdelatex-XXXXXX.tex"));
    if (!tempFile->open()) {
        delete tempFile;
        return LatexNotFound;
    }
    QString tempFileName = tempFile->fileName();
    QFileInfo *tempFileInfo = new QFileInfo(tempFileName);
    QString tempFileNameNS = tempFileInfo->absolutePath() + QLatin1Char('/') + tempFileInfo->baseName();
    QString tempFilePath = tempFileInfo->absolutePath();
    delete tempFileInfo;
    QTextStream tempStream(tempFile);

    if (renderSource) {
        tempStream << "\
\\documentclass["
                   << fontSize << "pt,varwidth,border=0pt]{standalone} \
\\usepackage{xcolor} \
\\usepackage{amsmath,mathtools,latexsym} \
\\usepackage[version=4]{mhchem} \
\\usepackage{physics} \
\\usepackage{unicode-math} \
\\pagestyle{empty} \
\\begin{document} \
{\\color[rgb]{" << textColor.redF()
                   << "," << textColor.greenF() << "," << textColor.blueF() << "} ";
        if (constrainSourceWidth) {
            tempStream << "\\noindent\\begin{minipage}{" << sourceWidth << "} ";
        }
    } else {
        tempStream << "\
\\documentclass["
                   << fontSize << "pt]{article} \
\\usepackage{color} \
\\usepackage{amsmath,latexsym,amsfonts,amssymb,ulem} \
\\pagestyle{empty} \
\\begin{document} \
{\\color[rgb]{" << textColor.redF()
                   << "," << textColor.greenF() << "," << textColor.blueF() << "} ";
    }
    if (!renderSource) {
        tempStream << "\
\\begin{eqnarray*} \
" << latexSource
                   << " \
\\end{eqnarray*}";
    } else {
        tempStream << latexSource;
    }
    if (constrainSourceWidth) {
        tempStream << " \\end{minipage}";
    }
    tempStream << "} \
\\end{document}";

    tempFile->close();
    QString latexExecutable;
    if (renderSource) {
        latexExecutable = QStandardPaths::findExecutable(QStringLiteral("xelatex"));
        if (latexExecutable.isEmpty()) {
            latexExecutable = QStandardPaths::findExecutable(QStringLiteral("lualatex"));
        }
    } else {
        latexExecutable = QStandardPaths::findExecutable(QStringLiteral("latex"));
    }
    if (latexExecutable.isEmpty()) {
        qCDebug(OkularUiDebug) << "Could not find latex!";
        delete tempFile;
        fileName = QString();
        return LatexNotFound;
    }
    latexProc << latexExecutable << QStringLiteral("-interaction=nonstopmode") << QStringLiteral("-halt-on-error") << QStringLiteral("-output-directory=%1").arg(tempFilePath) << tempFile->fileName();
    latexProc.setOutputChannelMode(KProcess::MergedChannels);
    latexProc.execute();
    latexOutput = QString::fromLocal8Bit(latexProc.readAll());
    tempFile->remove();

    QFile::remove(tempFileNameNS + QStringLiteral(".log"));
    QFile::remove(tempFileNameNS + QStringLiteral(".aux"));
    delete tempFile;

    if (renderSource) {
        const QString temporaryPdfFile = tempFileNameNS + QStringLiteral(".pdf");
        if (!QFile::exists(temporaryPdfFile)) {
            fileName = QString();
            if (pdfFileName) {
                pdfFileName->clear();
            }
            return LatexFailed;
        }

        const QString pdfToImageExecutable = QStandardPaths::findExecutable(QStringLiteral("pdftocairo"));
        if (pdfToImageExecutable.isEmpty()) {
            qCDebug(OkularUiDebug) << "Could not find pdftocairo!";
            QFile::remove(temporaryPdfFile);
            fileName = QString();
            if (pdfFileName) {
                pdfFileName->clear();
            }
            return PdfToImageNotFound;
        }

        pdfToImageProc << pdfToImageExecutable << QStringLiteral("-png") << QStringLiteral("-singlefile") << QStringLiteral("-transp") << QStringLiteral("-r") << QString::number(resolution) << QStringLiteral("%1").arg(temporaryPdfFile) << tempFileNameNS;
        pdfToImageProc.setOutputChannelMode(KProcess::MergedChannels);
        pdfToImageProc.execute();

        if (pdfFileName) {
            *pdfFileName = temporaryPdfFile;
            m_fileList << temporaryPdfFile;
        } else {
            QFile::remove(temporaryPdfFile);
        }

        if (!QFile::exists(tempFileNameNS + QStringLiteral(".png"))) {
            fileName = QString();
            if (pdfFileName) {
                pdfFileName->clear();
            }
            return PdfToImageFailed;
        }

        fileName = tempFileNameNS + QStringLiteral(".png");
        m_fileList << fileName;
        return NoError;
    }

    if (!QFile::exists(tempFileNameNS + QStringLiteral(".dvi"))) {
        fileName = QString();
        return LatexFailed;
    }

    QString dvipngExecutable = QStandardPaths::findExecutable(QStringLiteral("dvipng"));
    if (dvipngExecutable.isEmpty()) {
        qCDebug(OkularUiDebug) << "Could not find dvipng!";
        fileName = QString();
        return DvipngNotFound;
    }

    dvipngProc << dvipngExecutable << QStringLiteral("-o%1").arg(tempFileNameNS + QStringLiteral(".png")) << QStringLiteral("-Ttight") << QStringLiteral("-bgTransparent") << QStringLiteral("-D %1").arg(resolution)
               << QStringLiteral("%1").arg(tempFileNameNS + QStringLiteral(".dvi"));
    dvipngProc.setOutputChannelMode(KProcess::MergedChannels);
    dvipngProc.execute();

    QFile::remove(tempFileNameNS + QStringLiteral(".dvi"));

    if (!QFile::exists(tempFileNameNS + QStringLiteral(".png"))) {
        fileName = QString();
        return DvipngFailed;
    }

    fileName = tempFileNameNS + QStringLiteral(".png");
    m_fileList << fileName;
    return NoError;
}

bool LatexRenderer::securityCheck(const QString &latexFormula)
{
    static const auto formulaRegex =
        QRegularExpression(QString::fromLatin1("\\\\(def|let|futurelet|newcommand|renewcommand|else|fi|write|input|include"
                                               "|chardef|catcode|makeatletter|noexpand|toksdef|every|errhelp|errorstopmode|scrollmode|nonstopmode|batchmode"
                                               "|read|csname|newhelp|relax|afterground|afterassignment|expandafter|noexpand|special|command|loop|repeat|toks"
                                               "|output|line|mathcode|name|item|section|mbox|DeclareRobustCommand)[^a-zA-Z]"));
    return !latexFormula.contains(formulaRegex);
}

}

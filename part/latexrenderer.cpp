/*
    SPDX-FileCopyrightText: 2004 Duncan Mac-Vicar Prett <duncan@kde.org>
    SPDX-FileCopyrightText: 2004-2005 Olivier Goffart <ogoffart@kde.org>
    SPDX-FileCopyrightText: 2011 Niels Ole Salscheider
    <niels_ole@salscheider-online.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "latexrenderer.h"

#include <cmath>
#include <memory>
#include <mutex>

#include <QDebug>

#include <KLocalizedString>
#include <KProcess>

#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTextStream>

#include "gui/debug_ui.h"
#include "settings.h"

#ifdef OKULAR_ENABLE_MICROTEX
#include "latex.h"
#include "platform/qt/graphic_qt.h"
#include "render.h"
#endif

namespace GuiUtils
{
namespace
{
QString executableFromUserSetting()
{
    const QString configured = Okular::Settings::latexExecutablePath().trimmed();
    if (configured.isEmpty()) {
        return QString();
    }

    if (configured.contains(QLatin1Char('/')) || QFileInfo(configured).isAbsolute()) {
        const QFileInfo info(configured);
        if (info.exists() && info.isFile() && info.isExecutable()) {
            return info.absoluteFilePath();
        }
        qCDebug(OkularUiDebug) << "Configured XeLaTeX executable is not usable:" << configured;
        return QString();
    }

    const QString executable = QStandardPaths::findExecutable(configured);
    if (executable.isEmpty()) {
        qCDebug(OkularUiDebug) << "Configured XeLaTeX executable was not found in PATH:" << configured;
    }
    return executable;
}

QString sourceLatexExecutable()
{
    QString executable = executableFromUserSetting();
    if (!executable.isEmpty()) {
        return executable;
    }

    executable = QStandardPaths::findExecutable(QStringLiteral("xelatex"));
    if (!executable.isEmpty()) {
        return executable;
    }

    return QStandardPaths::findExecutable(QStringLiteral("lualatex"));
}

QString sourceLatexBackendName(const QString &executable)
{
    const QString baseName = QFileInfo(executable).baseName();
    if (baseName == QLatin1String("lualatex")) {
        return QStringLiteral("lualatex");
    }
    if (baseName == QLatin1String("xelatex")) {
        return QStringLiteral("xelatex");
    }
    return QStringLiteral("custom-xelatex");
}

#ifdef OKULAR_ENABLE_MICROTEX
std::mutex microtexMutex;
bool microtexInitialized = false;

QString microtexResourceRoot()
{
    const QString envPath = QString::fromLocal8Bit(qgetenv("OKULAR_MICROTEX_RES")).trimmed();
    if (!envPath.isEmpty() && QFileInfo(envPath).isDir()) {
        return envPath;
    }

    const QString installedPath = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("okular/microtex/res"), QStandardPaths::LocateDirectory);
    if (!installedPath.isEmpty()) {
        return installedPath;
    }

    const QString buildPath = QStringLiteral(OKULAR_MICROTEX_RES_DIR);
    if (!buildPath.isEmpty() && QFileInfo(buildPath).isDir()) {
        return buildPath;
    }

    return QString();
}

tex::color microtexColor(const QColor &color)
{
    const QColor effectiveColor = color.isValid() ? color : Qt::black;
    return effectiveColor.rgba();
}

LatexRenderWarning microtexOverflowWarning(int renderedWidth, int availableWidth)
{
    LatexRenderWarning warning;
    warning.type = LatexRenderWarningType::ClippingRisk;
    warning.severity = renderedWidth - availableWidth;
    warning.message = i18n("LaTeX output is wider than the layout width. The note is shown fully; the blue width handle marks the requested layout width. Rendered width is %1 pt, layout width is %2 pt.",
                           renderedWidth,
                           availableWidth);
    return warning;
}

LatexRenderer::Error renderMicrotexToPdf(const QString &latexSource, const QColor &textColor, int fontSize, double maxWidth, QString &pdfFileName, QString &latexOutput, QStringList &fileList, LatexRenderWarning *warning)
{
    std::lock_guard<std::mutex> guard(microtexMutex);

    try {
        if (!microtexInitialized) {
            const QString resRoot = microtexResourceRoot();
            if (resRoot.isEmpty()) {
                latexOutput = QStringLiteral("MicroTeX resource directory was not found.");
                return LatexRenderer::MicrotexFailed;
            }
            tex::LaTeX::init(resRoot.toStdString());
            microtexInitialized = true;
        }

        const bool fixedWidth = std::isfinite(maxWidth) && maxWidth > 0.0;
        const int requestedWidth = fixedWidth ? qMax(1, static_cast<int>(std::ceil(maxWidth))) : 0;
        const int padding = qMax(2, static_cast<int>(std::ceil(fontSize * 0.2)));
        const int horizontalPadding = fixedWidth ? 0 : padding;
        const int layoutWidth = fixedWidth ? requestedWidth : 10000;
        std::unique_ptr<tex::TeXRender> render(tex::LaTeX::parse(latexSource.toStdWString(), layoutWidth, fontSize, fontSize / 3.0f, microtexColor(textColor)));
        if (!render) {
            latexOutput = QStringLiteral("MicroTeX did not return a render object.");
            return LatexRenderer::MicrotexFailed;
        }

        const int width = fixedWidth ? qMax(requestedWidth, render->getWidth()) : qMax(1, render->getWidth() + 2 * horizontalPadding);
        const int height = qMax(1, render->getHeight() + 2 * padding);

        QTemporaryFile tempFile(QDir::tempPath() + QLatin1String("/okular_microtex-XXXXXX.pdf"));
        tempFile.setAutoRemove(false);
        if (!tempFile.open()) {
            latexOutput = QStringLiteral("Could not create a temporary PDF file for MicroTeX output.");
            return LatexRenderer::MicrotexFailed;
        }
        const QString tempFileName = tempFile.fileName();
        tempFile.close();

        QPdfWriter writer(tempFileName);
        writer.setCreator(QStringLiteral("Okular MicroTeX"));
        writer.setResolution(72);
        writer.setPageSize(QPageSize(QSizeF(width, height), QPageSize::Point));
        writer.setPageMargins(QMarginsF(0, 0, 0, 0), QPageLayout::Point);

        QPainter painter(&writer);
        if (!painter.isActive()) {
            QFile::remove(tempFileName);
            latexOutput = QStringLiteral("Could not paint MicroTeX output into a PDF file.");
            return LatexRenderer::MicrotexFailed;
        }

        tex::Graphics2D_qt graphics(&painter);
        render->draw(graphics, horizontalPadding, padding);
        painter.end();

        if (!QFileInfo::exists(tempFileName) || QFileInfo(tempFileName).size() <= 0) {
            QFile::remove(tempFileName);
            latexOutput = QStringLiteral("MicroTeX produced an empty PDF file.");
            return LatexRenderer::MicrotexFailed;
        }

        pdfFileName = tempFileName;
        fileList << tempFileName;
        QStringList outputLines;
        outputLines << QStringLiteral("Rendered with MicroTeX fallback. LaTeX preamble and external packages are not supported by MicroTeX.");
        if (fixedWidth) {
            const int availableWidth = requestedWidth;
            if (render->getWidth() > availableWidth) {
                const LatexRenderWarning overflowWarning = microtexOverflowWarning(render->getWidth(), availableWidth);
                if (warning) {
                    *warning = overflowWarning;
                }
                outputLines << overflowWarning.message;
            }
        }
        latexOutput = outputLines.join(QLatin1Char('\n'));
        return LatexRenderer::NoError;
    } catch (const std::exception &e) {
        latexOutput = QStringLiteral("MicroTeX fallback failed: %1").arg(QString::fromLocal8Bit(e.what()));
        return LatexRenderer::MicrotexFailed;
    } catch (...) {
        latexOutput = QStringLiteral("MicroTeX fallback failed with an unknown error.");
        return LatexRenderer::MicrotexFailed;
    }
}
#endif

QString compactWarningLine(const QString &line)
{
    QString message = line.simplified();
    constexpr int maxLength = 180;
    if (message.size() > maxLength) {
        message = message.left(maxLength - 3) + QStringLiteral("...");
    }
    return message;
}

LatexRenderWarning latexWarningMessage(const QString &latexOutput)
{
    static const QRegularExpression overfullRegex(QStringLiteral("^Overfull \\\\([hv])box \\(([0-9]+(?:\\.[0-9]+)?)pt too (?:wide|high)\\)"));
    static const QRegularExpression underfullRegex(QStringLiteral("^Underfull \\\\([hv])box .*badness ([0-9]+)"));
    constexpr double overfullThresholdPt = 0.5;
    constexpr int underfullThresholdBadness = 1000;

    const QStringList lines = latexOutput.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        const QRegularExpressionMatch overfullMatch = overfullRegex.match(trimmed);
        if (overfullMatch.hasMatch()) {
            const double overfullPoints = overfullMatch.captured(2).toDouble();
            if (overfullPoints <= overfullThresholdPt) {
                continue;
            }

            LatexRenderWarning warning;
            warning.type = LatexRenderWarningType::ClippingRisk;
            warning.severity = overfullPoints;
            warning.message = i18n("LaTeX output is wider than the layout width. The note is shown fully; the blue width handle marks the requested layout width:\n%1", compactWarningLine(trimmed));
            return warning;
        }

        const QRegularExpressionMatch underfullMatch = underfullRegex.match(trimmed);
        if (underfullMatch.hasMatch()) {
            const int badness = underfullMatch.captured(2).toInt();
            if (badness < underfullThresholdBadness) {
                continue;
            }

            LatexRenderWarning warning;
            warning.type = LatexRenderWarningType::LooseLayout;
            warning.severity = badness;
            warning.message = i18n("LaTeX produced a loose layout:\n%1", compactWarningLine(trimmed));
            return warning;
        }
    }

    return {};
}

double maxHorizontalOverflowPoints(const QString &latexOutput)
{
    static const QRegularExpression overfullHBoxRegex(QStringLiteral("^Overfull \\\\hbox \\(([0-9]+(?:\\.[0-9]+)?)pt too wide\\)"));
    constexpr double overfullThresholdPt = 0.5;

    double maxOverflow = 0.0;
    const QStringList lines = latexOutput.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QRegularExpressionMatch match = overfullHBoxRegex.match(line.trimmed());
        if (!match.hasMatch()) {
            continue;
        }

        const double overflow = match.captured(1).toDouble();
        if (overflow > maxOverflow) {
            maxOverflow = overflow;
        }
    }

    return maxOverflow > overfullThresholdPt ? maxOverflow : 0.0;
}
}

LatexRenderer::LatexRenderer()
{
}

LatexRenderer::~LatexRenderer()
{
    for (const QString &file : std::as_const(m_fileList)) {
        QFile::remove(file);
    }
}

QString LatexRenderer::lastBackendName() const
{
    return m_lastBackendName;
}

LatexRenderWarning LatexRenderer::lastWarning() const
{
    return m_lastWarning;
}

QString LatexRenderer::lastWarningMessage() const
{
    return m_lastWarning.message;
}

LatexRenderer::Error LatexRenderer::renderLatexInHtml(QString &html, const QColor &textColor, int fontSize, int resolution, QString &latexOutput)
{
    m_lastBackendName.clear();
    m_lastWarning = {};

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

QString LatexRenderer::defaultSourcePreamble()
{
    return QStringLiteral("\\usepackage{xcolor}\n"
                          "\\usepackage{amsmath,mathtools,latexsym}\n"
                          "\\usepackage[version=4]{mhchem}\n"
                          "\\usepackage{physics}\n"
                          "\\usepackage{unicode-math}");
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

LatexRenderer::Error LatexRenderer::renderLatexToImage(const QString &latexFormula, const QColor &textColor, int fontSize, int resolution, QString &fileName, QString &latexOutput)
{
    m_lastBackendName.clear();
    m_lastWarning = {};

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

LatexRenderer::Error LatexRenderer::renderLatexToPdf(const QString &latexFormula, const QColor &textColor, int fontSize, QString &pdfFileName, QString &latexOutput, double maxWidth, const QString &sourcePreamble)
{
    m_lastBackendName.clear();
    m_lastWarning = {};

    QString formula = latexFormula.trimmed();
    if (formula.isEmpty()) {
        pdfFileName.clear();
        return LatexFailed;
    }
    if (!securityCheck(formula)) {
        pdfFileName.clear();
        latexOutput = QStringLiteral("The formula contains unsupported LaTeX commands.");
        return LatexFailed;
    }

    QString imageFileName;
    return handleLatex(imageFileName, &pdfFileName, formula, textColor, fontSize, 0, latexOutput, BodyMode::Source, maxWidth, sourcePreamble);
}

LatexRenderer::Error LatexRenderer::handleLatex(QString &fileName, QString *pdfFileName, const QString &latexSource, const QColor &textColor, int fontSize, int resolution, QString &latexOutput, BodyMode bodyMode, double maxWidth, const QString &sourcePreamble)
{
    KProcess dvipngProc;
    KProcess pdfToImageProc;
    const bool renderSource = bodyMode == BodyMode::Source;
    const bool constrainSourceWidth = renderSource && std::isfinite(maxWidth) && maxWidth > 0.0;
    const QString sourceWidth = constrainSourceWidth ? QString::number(maxWidth, 'f', 3) + QStringLiteral("bp") : QString();
    const QString sourceVarWidth = QStringLiteral("varwidth");
    const QString effectiveSourcePreamble = sourcePreamble.isNull() ? defaultSourcePreamble() : sourcePreamble;

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
    tempFile->close();

    auto writeLatexSource = [&](double extraRightWidthPoints) -> bool {
        QFile sourceFile(tempFileName);
        if (!sourceFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            return false;
        }

        QTextStream tempStream(&sourceFile);
        if (renderSource) {
            tempStream << "\
\\documentclass["
                       << fontSize << "pt," << sourceVarWidth << ",border=0pt]{standalone}\n"
                       << effectiveSourcePreamble << "\n"
                       << "\\pagestyle{empty}\n"
                          "\\begin{document}\n"
                          "{\\color[rgb]{"
                       << textColor.redF()
                       << "," << textColor.greenF() << "," << textColor.blueF() << "} ";
            if (constrainSourceWidth) {
                tempStream << "\\noindent\\begin{minipage}[t]{" << sourceWidth << "}%\n";
            } else {
                tempStream << "\\noindent ";
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
            if (constrainSourceWidth) {
                tempStream << "\n\\end{minipage}%\n";
                if (std::isfinite(extraRightWidthPoints) && extraRightWidthPoints > 0.0) {
                    tempStream << "\\hspace*{" << QString::number(extraRightWidthPoints, 'f', 3) << "bp}%\n";
                }
            }
        }
        tempStream << "} \
\\end{document}";
        return true;
    };

    if (!writeLatexSource(0.0)) {
        delete tempFile;
        return LatexNotFound;
    }

    QString latexExecutable = renderSource ? sourceLatexExecutable() : QStandardPaths::findExecutable(QStringLiteral("latex"));
    if (latexExecutable.isEmpty()) {
        qCDebug(OkularUiDebug) << "Could not find latex!";
        delete tempFile;
        fileName = QString();
#ifdef OKULAR_ENABLE_MICROTEX
        if (renderSource && pdfFileName && resolution <= 0) {
            Error fallbackError = renderMicrotexToPdf(latexSource, textColor, fontSize, maxWidth, *pdfFileName, latexOutput, m_fileList, &m_lastWarning);
            if (fallbackError == NoError) {
                m_lastBackendName = QStringLiteral("microtex");
            }
            return fallbackError;
        }
#endif
        return LatexNotFound;
    }
    m_lastBackendName = renderSource ? sourceLatexBackendName(latexExecutable) : QStringLiteral("latex");
    auto runLatex = [&]() -> QString {
        KProcess latexProc;
        latexProc << latexExecutable << QStringLiteral("-interaction=nonstopmode") << QStringLiteral("-halt-on-error") << QStringLiteral("-output-directory=%1").arg(tempFilePath) << tempFileName;
        latexProc.setOutputChannelMode(KProcess::MergedChannels);
        latexProc.execute();
        return QString::fromLocal8Bit(latexProc.readAll());
    };

    latexOutput = runLatex();
    m_lastWarning = latexWarningMessage(latexOutput);
    if (renderSource && pdfFileName && resolution <= 0 && constrainSourceWidth) {
        const double overflowPoints = maxHorizontalOverflowPoints(latexOutput);
        if (overflowPoints > 0.0 && writeLatexSource(overflowPoints + 1.0)) {
            latexOutput = runLatex();
            m_lastWarning = latexWarningMessage(latexOutput);
        }
    }
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

        if (resolution <= 0) {
            fileName.clear();
            if (pdfFileName) {
                *pdfFileName = temporaryPdfFile;
                m_fileList << temporaryPdfFile;
            } else {
                QFile::remove(temporaryPdfFile);
            }
            return NoError;
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

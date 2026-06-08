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
#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPdfWriter>
#include <QRectF>
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
    if (configured.isEmpty() || configured == QLatin1String("QString()")) {
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

bool canUseMicrotexForRender(bool renderSource, QString *pdfFileName, int resolution)
{
    return renderSource && pdfFileName && resolution <= 0;
}

QString texInvocationLogPath()
{
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (logDir.isEmpty()) {
        logDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }
    if (logDir.isEmpty()) {
        logDir = QDir::tempPath();
    }
    QDir().mkpath(logDir);
    return QDir(logDir).filePath(QStringLiteral("okular-tex-debug.log"));
}

void logTexInvocation(const char *operation, const QString &backend, const QString &reason, const QStringList &details = QStringList())
{
    if (!OkularUiDebug().isDebugEnabled()) {
        return;
    }

    QStringList fields = {QStringLiteral("Invoking TeX; operation: %1").arg(QLatin1String(operation)), QStringLiteral("backend: %1").arg(backend), QStringLiteral("reason: %1").arg(reason)};
    for (const QString &detail : details) {
        fields << detail;
    }
    const QString message = fields.join(QStringLiteral("; "));
    qCDebug(OkularUiDebug).noquote() << message;

    QFile logFile(texInvocationLogPath());
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&logFile);
        stream << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << " " << message << '\n';
    }
}

QString latexTemporaryPath()
{
    QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (tempPath.isEmpty()) {
        tempPath = QDir::tempPath();
    }
#ifdef Q_OS_UNIX
    const QString homePath = QDir::homePath();
    if (!homePath.isEmpty()) {
        const QString absoluteTempPath = QDir(tempPath).absolutePath();
        const QString absoluteHomePath = QDir(homePath).absolutePath();
        if (absoluteTempPath == absoluteHomePath || absoluteTempPath.startsWith(absoluteHomePath + QLatin1Char('/'))) {
            tempPath = QStringLiteral("/tmp");
        }
    }
#endif
    QDir().mkpath(tempPath);
    return tempPath;
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

struct MicrotexInkBounds {
    QRectF rect;
    bool valid = false;
};

void uniteMicrotexBounds(MicrotexInkBounds *bounds, const QRectF &rect)
{
    if (!bounds || !rect.isValid() || rect.isEmpty()) {
        return;
    }
    bounds->rect = bounds->valid ? bounds->rect.united(rect) : rect;
    bounds->valid = true;
}

class MicrotexVectorBoundsGraphics final : public tex::Graphics2D_qt
{
public:
    explicit MicrotexVectorBoundsGraphics(QPainter *painter)
        : tex::Graphics2D_qt(painter)
    {
    }

    MicrotexInkBounds bounds() const
    {
        return m_bounds;
    }

    void setStroke(const tex::Stroke &stroke) override
    {
        m_stroke = stroke;
        tex::Graphics2D_qt::setStroke(stroke);
    }

    void setStrokeWidth(float width) override
    {
        m_stroke.lineWidth = width;
        tex::Graphics2D_qt::setStrokeWidth(width);
    }

    void setFont(const tex::Font *font) override
    {
        m_font = static_cast<const tex::Font_qt *>(font);
        tex::Graphics2D_qt::setFont(font);
    }

    void translate(float dx, float dy) override
    {
        m_transform.translate(dx, dy);
        tex::Graphics2D_qt::translate(dx, dy);
    }

    void scale(float sx, float sy) override
    {
        m_transform.scale(sx, sy);
        tex::Graphics2D_qt::scale(sx, sy);
    }

    void rotate(float angle) override
    {
        m_transform.rotateRadians(angle);
        tex::Graphics2D_qt::rotate(angle);
    }

    void rotate(float angle, float px, float py) override
    {
        m_transform.translate(px, py);
        m_transform.rotateRadians(angle);
        m_transform.translate(-px, -py);
        tex::Graphics2D_qt::rotate(angle, px, py);
    }

    void reset() override
    {
        m_transform.reset();
        tex::Graphics2D_qt::reset();
    }

    void drawText(const std::wstring &text, float x, float y) override
    {
        if (m_font) {
            QPainterPath path;
            path.addText(QPointF(x, y), m_font->getQFont(), tex::wstring_to_QString(text));
            addPath(path);
        }
        tex::Graphics2D_qt::drawText(text, x, y);
    }

    void drawLine(float x1, float y1, float x2, float y2) override
    {
        QPainterPath path(QPointF(x1, y1));
        path.lineTo(QPointF(x2, y2));
        addStrokedPath(path);
        tex::Graphics2D_qt::drawLine(x1, y1, x2, y2);
    }

    void drawRect(float x, float y, float width, float height) override
    {
        QPainterPath path;
        path.addRect(QRectF(x, y, width, height));
        addStrokedPath(path);
        tex::Graphics2D_qt::drawRect(x, y, width, height);
    }

    void fillRect(float x, float y, float width, float height) override
    {
        addRect(QRectF(x, y, width, height));
        tex::Graphics2D_qt::fillRect(x, y, width, height);
    }

    void drawRoundRect(float x, float y, float width, float height, float rx, float ry) override
    {
        QPainterPath path;
        path.addRoundedRect(QRectF(x, y, width, height), rx, ry);
        addStrokedPath(path);
        tex::Graphics2D_qt::drawRoundRect(x, y, width, height, rx, ry);
    }

    void fillRoundRect(float x, float y, float width, float height, float rx, float ry) override
    {
        QPainterPath path;
        path.addRoundedRect(QRectF(x, y, width, height), rx, ry);
        addPath(path);
        tex::Graphics2D_qt::fillRoundRect(x, y, width, height, rx, ry);
    }

private:
    void addRect(const QRectF &rect)
    {
        uniteMicrotexBounds(&m_bounds, m_transform.mapRect(rect));
    }

    void addPath(const QPainterPath &path)
    {
        uniteMicrotexBounds(&m_bounds, m_transform.map(path).boundingRect());
    }

    void addStrokedPath(const QPainterPath &path)
    {
        QPainterPathStroker stroker;
        stroker.setCapStyle(Qt::FlatCap);
        stroker.setJoinStyle(Qt::MiterJoin);
        stroker.setMiterLimit(m_stroke.miterLimit);
        stroker.setWidth(qMax(0.0f, m_stroke.lineWidth));
        addPath(stroker.createStroke(path));
    }

    QTransform m_transform;
    tex::Stroke m_stroke;
    const tex::Font_qt *m_font = nullptr;
    MicrotexInkBounds m_bounds;
};

MicrotexInkBounds measureMicrotexVectorBounds(tex::TeXRender *render)
{
    if (!render) {
        return {};
    }

    QImage dummy(1, 1, QImage::Format_ARGB32_Premultiplied);
    dummy.fill(Qt::transparent);
    QPainter dummyPainter(&dummy);
    MicrotexVectorBoundsGraphics graphics(&dummyPainter);
    render->draw(graphics, 0, 0);
    dummyPainter.end();
    return graphics.bounds();
}

QByteArray pdfReal(double value)
{
    QByteArray number = QByteArray::number(value, 'f', 6);
    while (number.contains('.') && number.endsWith('0')) {
        number.chop(1);
    }
    if (number.endsWith('.')) {
        number.chop(1);
    }
    if (number.isEmpty() || number == "-0") {
        return QByteArrayLiteral("0");
    }
    return number;
}

bool replaceOrInsertPdfDictionaryEntry(QString *dictionary, const QRegularExpression &entryRegex, const QString &entry)
{
    if (!dictionary) {
        return false;
    }

    const QRegularExpressionMatch match = entryRegex.match(*dictionary);
    if (match.hasMatch()) {
        dictionary->replace(match.capturedStart(), match.capturedLength(), entry);
        return true;
    }

    const int dictionaryEnd = dictionary->lastIndexOf(QStringLiteral(">>"));
    if (dictionaryEnd < 0) {
        return false;
    }

    dictionary->insert(dictionaryEnd, QLatin1Char('\n') + entry + QLatin1Char('\n'));
    return true;
}

bool addIncrementalPdfCropBox(const QString &pdfFileName, double left, double bottom, double right, double top)
{
    QFile pdfFile(pdfFileName);
    if (!pdfFile.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray pdfData = pdfFile.readAll();
    pdfFile.close();

    const QString pdfText = QString::fromLatin1(pdfData);
    static const QRegularExpression pageObjectRegex(QStringLiteral(R"((\d+)\s+0\s+obj\s*<<(?:(?!endobj).)*?\/Type\s*\/Page\b(?:(?!endobj).)*?>>\s*endobj)"), QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch pageMatch = pageObjectRegex.match(pdfText);
    if (!pageMatch.hasMatch()) {
        return false;
    }

    const int pageObjectNumber = pageMatch.captured(1).toInt();
    const QString pageObject = pageMatch.captured(0);
    const int dictionaryStart = pageObject.indexOf(QStringLiteral("<<"));
    const int dictionaryEnd = pageObject.lastIndexOf(QStringLiteral(">>"));
    if (pageObjectNumber <= 0 || dictionaryStart < 0 || dictionaryEnd <= dictionaryStart) {
        return false;
    }

    QString pageDictionary = pageObject.mid(dictionaryStart, dictionaryEnd - dictionaryStart + 2);
    const QString cropBoxEntry = QString::fromLatin1("/CropBox [%1 %2 %3 %4]")
                                     .arg(QString::fromLatin1(pdfReal(left)),
                                          QString::fromLatin1(pdfReal(bottom)),
                                          QString::fromLatin1(pdfReal(right)),
                                          QString::fromLatin1(pdfReal(top)));
    static const QRegularExpression cropBoxRegex(QStringLiteral(R"(\/CropBox\s*\[[^\]]*\])"));
    if (!replaceOrInsertPdfDictionaryEntry(&pageDictionary, cropBoxRegex, cropBoxEntry)) {
        return false;
    }

    const int startXrefIndex = pdfText.lastIndexOf(QStringLiteral("startxref"));
    if (startXrefIndex < 0) {
        return false;
    }
    const QRegularExpression startXrefRegex(QStringLiteral(R"(startxref\s+(\d+))"));
    const QRegularExpressionMatch startXrefMatch = startXrefRegex.match(pdfText, startXrefIndex);
    if (!startXrefMatch.hasMatch()) {
        return false;
    }
    const qint64 previousXrefOffset = startXrefMatch.captured(1).toLongLong();

    const int trailerIndex = pdfText.lastIndexOf(QStringLiteral("trailer"), startXrefIndex);
    if (trailerIndex < 0) {
        return false;
    }
    const int trailerDictionaryStart = pdfText.indexOf(QStringLiteral("<<"), trailerIndex);
    const int trailerDictionaryEnd = pdfText.indexOf(QStringLiteral(">>"), trailerDictionaryStart);
    if (trailerDictionaryStart < 0 || trailerDictionaryEnd <= trailerDictionaryStart || trailerDictionaryEnd > startXrefIndex) {
        return false;
    }

    QString trailerDictionary = pdfText.mid(trailerDictionaryStart, trailerDictionaryEnd - trailerDictionaryStart + 2);
    const QString prevEntry = QStringLiteral("/Prev %1").arg(previousXrefOffset);
    static const QRegularExpression prevRegex(QStringLiteral(R"(\/Prev\s+\d+)"));
    if (!replaceOrInsertPdfDictionaryEntry(&trailerDictionary, prevRegex, prevEntry)) {
        return false;
    }

    const qint64 objectOffset = pdfData.size() + 1;
    QByteArray incrementalUpdate;
    incrementalUpdate.append('\n');
    incrementalUpdate.append(QByteArray::number(pageObjectNumber));
    incrementalUpdate.append(" 0 obj\n");
    incrementalUpdate.append(pageDictionary.toLatin1());
    incrementalUpdate.append("\nendobj\n");
    const qint64 xrefOffset = pdfData.size() + incrementalUpdate.size();
    incrementalUpdate.append("xref\n");
    incrementalUpdate.append(QByteArray::number(pageObjectNumber));
    incrementalUpdate.append(" 1\n");
    incrementalUpdate.append(QStringLiteral("%1 00000 n \n").arg(objectOffset, 10, 10, QLatin1Char('0')).toLatin1());
    incrementalUpdate.append("trailer\n");
    incrementalUpdate.append(trailerDictionary.toLatin1());
    incrementalUpdate.append("\nstartxref\n");
    incrementalUpdate.append(QByteArray::number(xrefOffset));
    incrementalUpdate.append("\n%%EOF\n");

    if (!pdfFile.open(QIODevice::Append)) {
        return false;
    }
    const bool ok = pdfFile.write(incrementalUpdate) == incrementalUpdate.size();
    pdfFile.close();
    return ok;
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
                latexOutput = i18n("MicroTeX resource directory was not found.");
                return LatexRenderer::MicrotexFailed;
            }
            tex::LaTeX::init(resRoot.toStdString());
            microtexInitialized = true;
        }

        const bool fixedWidth = std::isfinite(maxWidth) && maxWidth > 0.0;
        const int requestedWidth = fixedWidth ? qMax(1, static_cast<int>(std::ceil(maxWidth))) : 0;
        const int layoutWidth = fixedWidth ? requestedWidth : 10000;
        std::unique_ptr<tex::TeXRender> render(tex::LaTeX::parseText(latexSource.toStdWString(), layoutWidth, fontSize, fontSize / 3.0f, microtexColor(textColor)));
        if (!render) {
            latexOutput = i18n("MicroTeX did not return a render object.");
            return LatexRenderer::MicrotexFailed;
        }

        const int logicalWidth = fixedWidth ? qMax(requestedWidth, render->getWidth()) : qMax(1, render->getWidth());
        const int logicalHeight = qMax(1, render->getHeight());
        const MicrotexInkBounds vectorBounds = measureMicrotexVectorBounds(render.get());
        const QRectF vectorRect = vectorBounds.valid ? vectorBounds.rect : QRectF(0.0, 0.0, logicalWidth, logicalHeight);
        const double pageLeft = qMin(0.0, vectorRect.left());
        const double pageTop = qMin(0.0, vectorRect.top());
        const double pageRight = qMax(static_cast<double>(logicalWidth), vectorRect.right());
        const double pageBottom = qMax(static_cast<double>(logicalHeight), vectorRect.bottom());
        const double contentWidth = qMax(1.0, pageRight - pageLeft);
        const double contentHeight = qMax(1.0, pageBottom - pageTop);
        constexpr double cropBoxPaddingPoints = 0.125;
        const double paddedContentWidth = contentWidth + 2.0 * cropBoxPaddingPoints;
        const double paddedContentHeight = contentHeight + 2.0 * cropBoxPaddingPoints;
        const double width = qMax(1.0, std::ceil(paddedContentWidth));
        const double height = qMax(1.0, std::ceil(paddedContentHeight));

        QTemporaryFile tempFile(QDir(latexTemporaryPath()).filePath(QStringLiteral("okular_microtex-XXXXXX.pdf")));
        tempFile.setAutoRemove(false);
        if (!tempFile.open()) {
            latexOutput = i18n("Could not create a temporary PDF file for MicroTeX output.");
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
            latexOutput = i18n("Could not paint MicroTeX output into a PDF file.");
            return LatexRenderer::MicrotexFailed;
        }

        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        painter.translate(cropBoxPaddingPoints - pageLeft, cropBoxPaddingPoints - pageTop);
        tex::Graphics2D_qt graphics(&painter);
        render->draw(graphics, 0, 0);
        painter.end();
        qCDebug(OkularUiDebug) << "MicroTeX draw finished; output PDF:" << tempFileName;

        addIncrementalPdfCropBox(tempFileName, 0.0, qMax(0.0, height - paddedContentHeight), qMin(width, paddedContentWidth), height);

        if (!QFileInfo::exists(tempFileName) || QFileInfo(tempFileName).size() <= 0) {
            QFile::remove(tempFileName);
            latexOutput = i18n("MicroTeX produced an empty PDF file.");
            return LatexRenderer::MicrotexFailed;
        }

        pdfFileName = tempFileName;
        fileList << tempFileName;
        qCDebug(OkularUiDebug) << "MicroTeX render finished; output PDF:" << pdfFileName << "bytes:" << QFileInfo(pdfFileName).size();
        QStringList outputLines;
        outputLines << i18n("Rendered with MicroTeX fallback. LaTeX preamble and external packages are not supported by MicroTeX.");
        if (fixedWidth) {
            const int availableWidth = requestedWidth;
            const int renderedWidth = qCeil(qMax(static_cast<double>(render->getWidth()), vectorRect.right()));
            if (renderedWidth > availableWidth) {
                const LatexRenderWarning overflowWarning = microtexOverflowWarning(renderedWidth, availableWidth);
                if (warning) {
                    *warning = overflowWarning;
                }
                outputLines << overflowWarning.message;
            }
        }
        latexOutput = outputLines.join(QLatin1Char('\n'));
        return LatexRenderer::NoError;
    } catch (const std::exception &e) {
        latexOutput = i18n("MicroTeX fallback failed: %1", QString::fromLocal8Bit(e.what()));
        return LatexRenderer::MicrotexFailed;
    } catch (...) {
        latexOutput = i18n("MicroTeX fallback failed with an unknown error.");
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
        latexOutput = i18n("The formula contains unsupported LaTeX commands.");
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
        latexOutput = i18n("The formula contains unsupported LaTeX commands.");
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
    const QString effectiveSourcePreamble = sourcePreamble.isNull() ? defaultSourcePreamble() : sourcePreamble;
    const int latexRenderBackend = Okular::Settings::latexRenderBackend();

    auto renderWithMicrotex = [&](const char *reason) -> Error {
        fileName.clear();
#ifdef OKULAR_ENABLE_MICROTEX
        logTexInvocation("microtex-render",
                         QStringLiteral("microtex"),
                         QString::fromLatin1(reason),
                         {QStringLiteral("font size: %1").arg(fontSize),
                          QStringLiteral("resolution: %1").arg(resolution),
                          QStringLiteral("max width: %1").arg(maxWidth),
                          QStringLiteral("source length: %1").arg(latexSource.size())});
        Error microtexError = renderMicrotexToPdf(latexSource, textColor, fontSize, maxWidth, *pdfFileName, latexOutput, m_fileList, &m_lastWarning);
        if (microtexError == NoError) {
            m_lastBackendName = QStringLiteral("microtex");
        }
        return microtexError;
#else
        pdfFileName->clear();
        latexOutput = i18n("MicroTeX rendering is not available in this Okular build.");
        return MicrotexFailed;
#endif
    };

    if (latexRenderBackend == Okular::Settings::EnumLatexRenderBackend::Microtex && canUseMicrotexForRender(renderSource, pdfFileName, resolution)) {
        return renderWithMicrotex("configured-microtex");
    }

    QTemporaryFile *tempFile = new QTemporaryFile(QDir(latexTemporaryPath()).filePath(QStringLiteral("okular_kdelatex-XXXXXX.tex")));
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

    auto writeLatexSource = [&]() -> bool {
        QFile sourceFile(tempFileName);
        if (!sourceFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            return false;
        }

        QTextStream tempStream(&sourceFile);
        if (renderSource) {
            tempStream << "\
\\documentclass["
                       << fontSize << "pt]{article}\n"
                       << "\\usepackage[paper=a0paper,margin=0pt]{geometry}\n"
                       << effectiveSourcePreamble << "\n"
                       << "\\pagestyle{empty}\n"
                          "\\setlength{\\parindent}{0pt}\n"
                          "\\begin{document}\n";
            tempStream << "\\thispagestyle{empty}\n";
            tempStream << "{\\color[rgb]{"
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
            }
        }
        tempStream << "} \
\\end{document}";
        return true;
    };

    if (!writeLatexSource()) {
        delete tempFile;
        return LatexNotFound;
    }

    QString latexExecutable = renderSource ? sourceLatexExecutable() : QStandardPaths::findExecutable(QStringLiteral("latex"));
    if (latexExecutable.isEmpty()) {
        qCDebug(OkularUiDebug) << "Could not find latex!";
        delete tempFile;
        fileName = QString();
        if (latexRenderBackend == Okular::Settings::EnumLatexRenderBackend::Auto && canUseMicrotexForRender(renderSource, pdfFileName, resolution)) {
            return renderWithMicrotex("system-tex-not-found-fallback");
        }
        return LatexNotFound;
    }
    m_lastBackendName = renderSource ? sourceLatexBackendName(latexExecutable) : QStringLiteral("latex");
    auto runLatex = [&](const char *reason) -> QString {
        KProcess latexProc;
        latexProc << latexExecutable << QStringLiteral("-interaction=nonstopmode") << QStringLiteral("-halt-on-error") << QStringLiteral("-output-directory=%1").arg(tempFilePath) << tempFileName;
        latexProc.setOutputChannelMode(KProcess::MergedChannels);
        logTexInvocation("system-latex",
                         m_lastBackendName,
                         QString::fromLatin1(reason),
                         {QStringLiteral("executable: %1").arg(latexExecutable),
                          QStringLiteral("source: %1").arg(tempFileName),
                          QStringLiteral("output directory: %1").arg(tempFilePath),
                          QStringLiteral("max width: %1").arg(maxWidth)});
        latexProc.execute();
        return QString::fromLocal8Bit(latexProc.readAll());
    };

    latexOutput = runLatex("initial-render");
    m_lastWarning = latexWarningMessage(latexOutput);
    tempFile->remove();

    QFile::remove(tempFileNameNS + QStringLiteral(".log"));
    QFile::remove(tempFileNameNS + QStringLiteral(".aux"));
    delete tempFile;

    if (renderSource) {
        QString temporaryPdfFile = tempFileNameNS + QStringLiteral(".pdf");
        if (!QFile::exists(temporaryPdfFile)) {
            fileName = QString();
            if (pdfFileName) {
                pdfFileName->clear();
            }
            return LatexFailed;
        }

        const QString pdfCropExecutable = QStandardPaths::findExecutable(QStringLiteral("pdfcrop"));
        if (pdfCropExecutable.isEmpty()) {
            qCDebug(OkularUiDebug) << "Could not find pdfcrop!";
            QFile::remove(temporaryPdfFile);
            fileName = QString();
            if (pdfFileName) {
                pdfFileName->clear();
            }
            latexOutput += QStringLiteral("\npdfcrop was not found.");
            return LatexFailed;
        }

        const QString croppedPdfFile = tempFileNameNS + QStringLiteral("-crop.pdf");
        QFile::remove(croppedPdfFile);
        KProcess pdfCropProc;
        pdfCropProc << pdfCropExecutable << QStringLiteral("--hires") << QStringLiteral("--margins") << QStringLiteral("0 0 0 0") << temporaryPdfFile << croppedPdfFile;
        pdfCropProc.setWorkingDirectory(tempFilePath);
        pdfCropProc.setOutputChannelMode(KProcess::MergedChannels);
        logTexInvocation("texlive-pdfcrop",
                         QStringLiteral("pdfcrop"),
                         QStringLiteral("source-pdf-crop"),
                         {QStringLiteral("executable: %1").arg(pdfCropExecutable), QStringLiteral("input: %1").arg(temporaryPdfFile), QStringLiteral("output: %1").arg(croppedPdfFile)});
        const int pdfCropExitCode = pdfCropProc.execute();
        const QString pdfCropOutput = QString::fromLocal8Bit(pdfCropProc.readAll());
        if (pdfCropExitCode != 0 || !QFile::exists(croppedPdfFile)) {
            qCWarning(OkularUiDebug) << "Could not crop rendered LaTeX PDF; exit code:" << pdfCropExitCode << "output:" << pdfCropOutput;
            QFile::remove(temporaryPdfFile);
            fileName = QString();
            if (pdfFileName) {
                pdfFileName->clear();
            }
            latexOutput += QLatin1Char('\n') + pdfCropOutput;
            return LatexFailed;
        }
        QFile::remove(temporaryPdfFile);
        temporaryPdfFile = croppedPdfFile;
        m_fileList << temporaryPdfFile;

        if (resolution <= 0) {
            fileName.clear();
            if (pdfFileName) {
                *pdfFileName = temporaryPdfFile;
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
        logTexInvocation("pdf-to-image",
                         QStringLiteral("system-converter"),
                         QStringLiteral("render-image"),
                         {QStringLiteral("executable: %1").arg(pdfToImageExecutable),
                          QStringLiteral("source PDF: %1").arg(temporaryPdfFile),
                          QStringLiteral("output base: %1").arg(tempFileNameNS),
                          QStringLiteral("resolution: %1").arg(resolution)});
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
    logTexInvocation("dvi-to-image",
                     QStringLiteral("system-converter"),
                     QStringLiteral("render-image"),
                     {QStringLiteral("executable: %1").arg(dvipngExecutable),
                      QStringLiteral("source DVI: %1").arg(tempFileNameNS + QStringLiteral(".dvi")),
                      QStringLiteral("output PNG: %1").arg(tempFileNameNS + QStringLiteral(".png")),
                      QStringLiteral("resolution: %1").arg(resolution)});
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

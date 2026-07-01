/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "templatenoteutils.h"

#include "config-okular.h"

#include <core/annotations.h>
#include <core/document.h>
#include <core/page.h>

#include <QFileInfo>
#include <QDateTime>
#include <QColor>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#if HAVE_JS
#include <QJSEngine>
#endif

namespace
{
#if HAVE_JS
void setString(QJSValue object, const QString &name, const QString &value)
{
    object.setProperty(name, value);
}

void setNumber(QJSValue object, const QString &name, int value)
{
    object.setProperty(name, value);
}

QString evaluateExpression(QJSEngine &engine, const QString &expression, QString *errorMessage)
{
    const QJSValue value = engine.evaluate(expression);
    if (value.isError()) {
        if (errorMessage) {
            *errorMessage = value.toString();
        }
        return QString();
    }
    return value.toString();
}
#endif
}

QString TemplateNoteUtils::defaultPageNumberTemplateData()
{
    QJsonObject root;
    root.insert(QStringLiteral("version"), 20260630);
    root.insert(QStringLiteral("kind"), QStringLiteral("scholia-template-note"));
    root.insert(QStringLiteral("template"), QStringLiteral("${frameNumber} / ${totalFrameNumber}"));

    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

QString TemplateNoteUtils::templateDataWithTemplate(const QString &templateData, const QString &templateText)
{
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(templateData.toUtf8(), &parseError);
    QJsonObject root;
    if (parseError.error == QJsonParseError::NoError && document.isObject()) {
        root = document.object();
    } else {
        document = QJsonDocument::fromJson(defaultPageNumberTemplateData().toUtf8());
        root = document.object();
    }

    root.insert(QStringLiteral("version"), 20260630);
    root.insert(QStringLiteral("kind"), QStringLiteral("scholia-template-note"));
    root.insert(QStringLiteral("template"), templateText);
    root.remove(QStringLiteral("style"));
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

bool TemplateNoteUtils::annotationIsTemplateNote(const Okular::Annotation *annotation)
{
    if (!annotation || annotation->subType() != Okular::Annotation::AText || !annotation->isTemplateNote()) {
        return false;
    }

    const auto *textAnnotation = static_cast<const Okular::TextAnnotation *>(annotation);
    return textAnnotation->textType() == Okular::TextAnnotation::InPlace;
}

const Okular::TextAnnotation *TemplateNoteUtils::annotationAsTemplateTextAnnotation(const Okular::Annotation *annotation)
{
    if (!annotationIsTemplateNote(annotation)) {
        return nullptr;
    }
    return static_cast<const Okular::TextAnnotation *>(annotation);
}

Okular::TextAnnotation *TemplateNoteUtils::annotationAsTemplateTextAnnotation(Okular::Annotation *annotation)
{
    return const_cast<Okular::TextAnnotation *>(annotationAsTemplateTextAnnotation(static_cast<const Okular::Annotation *>(annotation)));
}

QString TemplateNoteUtils::expandTemplate(const Okular::Document *document, int pageIndex, const Okular::TextAnnotation *annotation, QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!document || !annotation || !annotationIsTemplateNote(annotation)) {
        return QString();
    }

    const QString source = annotation->templateNoteTemplate();
    if (source.isEmpty()) {
        return QString();
    }

    const int pageCount = static_cast<int>(document->pages());
    const Okular::Page *page = document->page(pageIndex);
    const QString pageLabel = page ? page->label() : QString();
    const Okular::DocumentInfo info = document->documentInfo(QSet<Okular::DocumentInfo::Key>() << Okular::DocumentInfo::Title << Okular::DocumentInfo::Author);
    const QString title = info.get(Okular::DocumentInfo::Title);
    const QString author = info.get(Okular::DocumentInfo::Author);
    const QString fileName = QFileInfo(document->currentDocument().toLocalFile()).fileName();

#if HAVE_JS
    QJSEngine engine;
    engine.evaluate(QStringLiteral(R"(
function formatDate(date, patternOrOptions, locale) {
    if (!(date instanceof Date)) return "";
    if (patternOrOptions === "yyyy-MM-dd" || patternOrOptions === undefined) {
        const y = String(date.getFullYear()).padStart(4, "0");
        const m = String(date.getMonth() + 1).padStart(2, "0");
        const d = String(date.getDate()).padStart(2, "0");
        return `${y}-${m}-${d}`;
    }
    return new Intl.DateTimeFormat(locale || undefined, patternOrOptions).format(date);
}
function formatNumber(value, options, locale) {
    if (options === undefined) return String(Math.trunc(Number(value)));
    return new Intl.NumberFormat(locale || undefined, options).format(value);
}
function pad(value, width) {
    return String(Math.trunc(Number(value))).padStart(width, "0");
}
function roman(value) {
    let n = Math.trunc(Number(value));
    if (n <= 0) return "";
    const map = [["m",1000],["cm",900],["d",500],["cd",400],["c",100],["xc",90],["l",50],["xl",40],["x",10],["ix",9],["v",5],["iv",4],["i",1]];
    let out = "";
    for (const [sym, val] of map) {
        while (n >= val) { out += sym; n -= val; }
    }
    return out;
}
function upperRoman(value) {
    return roman(value).toUpperCase();
}
)"));

    QJSValue global = engine.globalObject();

    setNumber(global, QStringLiteral("pageIndex"), pageIndex);
    setNumber(global, QStringLiteral("pageNumber"), pageIndex + 1);
    setNumber(global, QStringLiteral("pageCount"), pageCount);
    setString(global, QStringLiteral("pageLabel"), pageLabel);
    setNumber(global, QStringLiteral("frameIndex"), pageIndex);
    setNumber(global, QStringLiteral("frameNumber"), pageIndex + 1);
    setNumber(global, QStringLiteral("totalFrameNumber"), pageCount);
    setNumber(global, QStringLiteral("slideNumber"), 1);
    setNumber(global, QStringLiteral("overlayNumber"), 1);

    setString(global, QStringLiteral("title"), title);
    setString(global, QStringLiteral("shortTitle"), title);
    setString(global, QStringLiteral("subtitle"), QString());
    setString(global, QStringLiteral("shortSubtitle"), QString());
    setString(global, QStringLiteral("author"), author);
    setString(global, QStringLiteral("shortAuthor"), author);
    setString(global, QStringLiteral("institute"), QString());
    setString(global, QStringLiteral("shortInstitute"), QString());
    setString(global, QStringLiteral("dateText"), QString());
    setString(global, QStringLiteral("shortDate"), QString());
    setString(global, QStringLiteral("documentTitle"), title);
    setString(global, QStringLiteral("fileName"), fileName);
    global.setProperty(QStringLiteral("now"), engine.evaluate(QStringLiteral("new Date(%1)").arg(QDateTime::currentDateTime().toMSecsSinceEpoch())));

    setString(global, QStringLiteral("part"), QString());
    setString(global, QStringLiteral("shortPart"), QString());
    setNumber(global, QStringLiteral("partNumber"), 0);
    setString(global, QStringLiteral("section"), QString());
    setString(global, QStringLiteral("shortSection"), QString());
    setNumber(global, QStringLiteral("sectionNumber"), 0);
    setString(global, QStringLiteral("subsection"), QString());
    setString(global, QStringLiteral("shortSubsection"), QString());
    setNumber(global, QStringLiteral("subsectionNumber"), 0);

    QString output;
    int cursor = 0;
    static const QRegularExpression placeholder(QStringLiteral(R"(\$\{([^}]*)\})"));
    const auto matches = placeholder.globalMatch(source);
    auto it = matches;
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        output += source.mid(cursor, match.capturedStart() - cursor);
        const QString value = evaluateExpression(engine, match.captured(1), errorMessage);
        if (errorMessage && !errorMessage->isEmpty()) {
            return QString();
        }
        output += value;
        cursor = match.capturedEnd();
    }
    output += source.mid(cursor);
    return output;
#else
    QHash<QString, QString> values;
    values.insert(QStringLiteral("pageIndex"), QString::number(pageIndex));
    values.insert(QStringLiteral("pageNumber"), QString::number(pageIndex + 1));
    values.insert(QStringLiteral("pageCount"), QString::number(pageCount));
    values.insert(QStringLiteral("pageLabel"), pageLabel);
    values.insert(QStringLiteral("frameIndex"), QString::number(pageIndex));
    values.insert(QStringLiteral("frameNumber"), QString::number(pageIndex + 1));
    values.insert(QStringLiteral("totalFrameNumber"), QString::number(pageCount));
    values.insert(QStringLiteral("slideNumber"), QStringLiteral("1"));
    values.insert(QStringLiteral("overlayNumber"), QStringLiteral("1"));
    values.insert(QStringLiteral("title"), title);
    values.insert(QStringLiteral("shortTitle"), title);
    values.insert(QStringLiteral("author"), author);
    values.insert(QStringLiteral("shortAuthor"), author);
    values.insert(QStringLiteral("documentTitle"), title);
    values.insert(QStringLiteral("fileName"), fileName);
    values.insert(QStringLiteral("partNumber"), QStringLiteral("0"));
    values.insert(QStringLiteral("sectionNumber"), QStringLiteral("0"));
    values.insert(QStringLiteral("subsectionNumber"), QStringLiteral("0"));

    QString output;
    int cursor = 0;
    static const QRegularExpression placeholder(QStringLiteral(R"(\$\{([^}]*)\})"));
    auto it = placeholder.globalMatch(source);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        output += source.mid(cursor, match.capturedStart() - cursor);
        output += values.value(match.captured(1).trimmed());
        cursor = match.capturedEnd();
    }
    output += source.mid(cursor);
    return output;
#endif
}

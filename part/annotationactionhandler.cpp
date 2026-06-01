/*
    SPDX-FileCopyrightText: 2019 Simone Gaiarin <simgunz@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "annotationactionhandler.h"

// qt includes
#include <QActionGroup>
#include <QBitmap>
#include <QColorDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDialog>
#include <QHash>
#include <QInputDialog>
#include <QMenu>
#include <QPainter>
#include <QPen>
#include <QWidget>

// kde includes
#include <KActionCollection>
#include <KLocalizedString>
#include <KMessageBox>
#include <KParts/MainWindow>
#include <KSelectAction>
#include <KToolBar>
#include <kwidgetsaddons_version.h>

// local includes
#include "actionbar.h"
#include "annotationwidgets.h"
#include "core/annotations.h"
#include "core/utils.h"
#include "gui/guiutils.h"
#include "latexnoteutils.h"
#include "pageview.h"
#include "pageviewannotator.h"
#include "settings.h"
#include "toggleactionmenu.h"

class AnnotationActionHandlerPrivate
{
public:
    enum class AnnotationColor { Color, InnerColor, TextColor };
    struct BuiltinToolSpec {
        QString type;
        QString name;
    };
    static const QList<QPair<KLocalizedString, QColor>> defaultColors;
    static const QList<double> widthStandardValues;
    static const QList<double> opacityStandardValues;

    explicit AnnotationActionHandlerPrivate(AnnotationActionHandler *qq)
        : q(qq)
        , annotator(nullptr)
        , agTools(nullptr)
        , agLastAction(nullptr)
        , aQuickTools(nullptr)
        , aQuickToolsBar(nullptr)
        , aGeomShapes(nullptr)
        , aStamp(nullptr)
        , aSelectCustomStamp(nullptr)
        , aAddLatexNote(nullptr)
        , aAddLatexInlineNote(nullptr)
        , aAddToQuickTools(nullptr)
        , aContinuousMode(nullptr)
        , aConstrainRatioAndAngle(nullptr)
        , aWidth(nullptr)
        , aColor(nullptr)
        , aInnerColor(nullptr)
        , aTextColor(nullptr)
        , aOpacity(nullptr)
        , aFont(nullptr)
        , aAdvancedSettings(nullptr)
        , aHideToolBar(nullptr)
        , aShowToolBar(nullptr)
        , aToolBarVisibility(nullptr)
        , aCustomStamp(nullptr)
        , aCustomWidth(nullptr)
        , aCustomOpacity(nullptr)
        , currentColor(QColor())
        , currentInnerColor(QColor())
        , currentTextColor(QColor())
        , currentFont(QFont())
        , currentWidth(-1)
        , selectedBuiltinTool(-1)
        , textToolsEnabled(false)
    {
    }

    QAction *selectActionItem(KSelectAction *aList, QAction *aCustomCurrent, double value, const QList<double> &defaultValues, const QIcon &icon, const QString &label);

    /**
     * @short Adds a custom stamp annotation action to the stamp list when the stamp is not a default stamp
     *
     * When @p stampIconName cannot be found among the default stamps, this method creates a new action
     * for the custom stamp annotation and adds it to the stamp action combo box.
     * If a custom action is already present in the list, it is removed before adding the new custom action.
     * If @p stampIconName matches a default stamp, any existing custom stamp annotation action is removed.
     */
    void maybeUpdateCustomStampAction(const QString &stampIconName);
    void parseTool(int toolId);

    void updateConfigActions(const QString &annotType = QLatin1String(""));
    void populateQuickAnnotations();
    KSelectAction *colorPickerAction(AnnotationColor colorType);

    const QIcon widthIcon(double width);
    const QIcon stampIcon(const QString &stampIconName);

    void addBuiltinToolAction(QAction *action, const QString &type, const QString &name = QString());
    void selectTool(int toolId);
    void selectTool(const BuiltinToolSpec &toolSpec);
    void slotStampToolSelected(const QString &stamp, const QString &contents = QString(), bool latexNoteBoxed = false);
    void slotSelectCustomStamp();
    void slotAddLatexNote(bool boxed = false);
    void slotQuickToolSelected(int favToolId);
    void slotSetColor(AnnotationColor colorType, const QColor &color = QColor());
    void slotSelectAnnotationFont();
    bool isQuickToolAction(QAction *aTool);
    bool isQuickToolStamp(int toolId);
    void assertToolBarExists(KParts::MainWindow *mw, const QString &toolBarName);

    AnnotationActionHandler *q;

    PageViewAnnotator *annotator;

    QList<QAction *> quickTools;
    QList<QAction *> textTools;
    QList<QAction *> textQuickTools;
    QActionGroup *agTools;
    QHash<QAction *, BuiltinToolSpec> builtinToolForAction;
    QAction *agLastAction;

    ToggleActionMenu *aQuickTools;
    ActionBar *aQuickToolsBar;
    ToggleActionMenu *aGeomShapes;
    ToggleActionMenu *aStamp;
    QAction *aSelectCustomStamp;
    QAction *aAddLatexNote;
    QAction *aAddLatexInlineNote;
    QAction *aAddToQuickTools;
    KToggleAction *aContinuousMode;
    KToggleAction *aConstrainRatioAndAngle;
    KSelectAction *aWidth;
    KSelectAction *aColor;
    KSelectAction *aInnerColor;
    KSelectAction *aTextColor;
    KSelectAction *aOpacity;
    QAction *aFont;
    QAction *aAdvancedSettings;
    QAction *aHideToolBar;
    QAction *aShowToolBar;
    KToggleAction *aToolBarVisibility;

    QAction *aCustomStamp;
    QAction *aCustomWidth;
    QAction *aCustomOpacity;

    QColor currentColor;
    QColor currentInnerColor;
    QColor currentTextColor;
    QFont currentFont;
    int currentWidth;

    int selectedBuiltinTool;
    bool textToolsEnabled;
};

const QList<QPair<KLocalizedString, QColor>> AnnotationActionHandlerPrivate::defaultColors = {{ki18nc("@item:inlistbox Color name", "Red"), Qt::red},
                                                                                              {ki18nc("@item:inlistbox Color name", "Orange"), QColor(255, 85, 0)},
                                                                                              {ki18nc("@item:inlistbox Color name", "Yellow"), Qt::yellow},
                                                                                              {ki18nc("@item:inlistbox Color name", "Green"), Qt::green},
                                                                                              {ki18nc("@item:inlistbox Color name", "Cyan"), Qt::cyan},
                                                                                              {ki18nc("@item:inlistbox Color name", "Blue"), Qt::blue},
                                                                                              {ki18nc("@item:inlistbox Color name", "Magenta"), Qt::magenta},
                                                                                              {ki18nc("@item:inlistbox Color name", "White"), Qt::white},
                                                                                              {ki18nc("@item:inlistbox Color name", "Gray"), Qt::gray},
                                                                                              {ki18nc("@item:inlistbox Color name", "Black"), Qt::black}

};

const QList<double> AnnotationActionHandlerPrivate::widthStandardValues = {1, 1.5, 2, 2.5, 3, 3.5, 4, 4.5, 5};

const QList<double> AnnotationActionHandlerPrivate::opacityStandardValues = {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0};

QAction *AnnotationActionHandlerPrivate::selectActionItem(KSelectAction *aList, QAction *aCustomCurrent, double value, const QList<double> &defaultValues, const QIcon &icon, const QString &label)
{
    if (aCustomCurrent) {
        aList->removeAction(aCustomCurrent);
        delete aCustomCurrent;
    }
    QAction *aCustom = nullptr;
    const int defaultValueIdx = defaultValues.indexOf(value);
    if (defaultValueIdx >= 0) {
        aList->setCurrentItem(defaultValueIdx);
    } else {
        aCustom = new KToggleAction(icon, label, q);
        const int aBeforeIdx = std::lower_bound(defaultValues.begin(), defaultValues.end(), value) - defaultValues.begin();
        QAction *aBefore = aBeforeIdx < defaultValues.size() ? aList->actions().at(aBeforeIdx) : nullptr;
        aList->insertAction(aBefore, aCustom);
        aList->setCurrentAction(aCustom);
    }
    return aCustom;
}

void AnnotationActionHandlerPrivate::maybeUpdateCustomStampAction(const QString &stampIconName)
{
    const auto defaultStamps = StampAnnotationWidget::defaultStamps();
    auto it = std::find_if(defaultStamps.begin(), defaultStamps.end(), [&stampIconName](const QPair<QString, QString> &element) { return element.second == stampIconName; });
    bool defaultStamp = it != defaultStamps.end();

    if (aCustomStamp && aCustomStamp->data().toString() == stampIconName) {
        agLastAction = aCustomStamp;
        aStamp->setDefaultAction(aCustomStamp);
        aCustomStamp->setChecked(true);
        return;
    }

    if (aCustomStamp) {
        aStamp->removeAction(aCustomStamp);
        agTools->removeAction(aCustomStamp);
        delete aCustomStamp;
        aCustomStamp = nullptr;
    }
    if (!defaultStamp) {
        QFileInfo info(stampIconName);
        QString stampActionName = info.fileName();
        aCustomStamp = new KToggleAction(stampIcon(stampIconName), stampActionName, q);
        aCustomStamp->setData(stampIconName);
        if (aSelectCustomStamp) {
            aStamp->insertAction(aSelectCustomStamp, aCustomStamp);
        } else {
            aStamp->addAction(aCustomStamp);
        }
        aStamp->setDefaultAction(aCustomStamp);
        agTools->addAction(aCustomStamp);
        agLastAction = aCustomStamp;
        aCustomStamp->setChecked(true);
        QObject::connect(aCustomStamp, &QAction::toggled, q, [this, stampIconName](bool checked) {
            if (checked) {
                slotStampToolSelected(stampIconName);
            }
        });
    }
}

void AnnotationActionHandlerPrivate::parseTool(int toolId)
{
    if (toolId == -1) {
        updateConfigActions();
        return;
    }

    QDomElement toolElement = annotator->builtinTool(toolId);
    const QString annotType = toolElement.attribute(QStringLiteral("type"));
    QDomElement engineElement = toolElement.firstChildElement(QStringLiteral("engine"));
    QDomElement annElement = engineElement.firstChildElement(QStringLiteral("annotation"));

    QColor color, innerColor, textColor, borderColor, engineColor;
    if (engineElement.hasAttribute(QStringLiteral("color"))) {
        engineColor = QColor(engineElement.attribute(QStringLiteral("color")));
    }
    if (annElement.hasAttribute(QStringLiteral("color"))) {
        color = QColor(annElement.attribute(QStringLiteral("color")));
    }
    if (annElement.hasAttribute(QStringLiteral("innerColor"))) {
        innerColor = QColor(annElement.attribute(QStringLiteral("innerColor")));
    }
    if (annElement.hasAttribute(QStringLiteral("textColor"))) {
        textColor = QColor(annElement.attribute(QStringLiteral("textColor")));
    }
    if (annElement.hasAttribute(QStringLiteral("borderColor"))) {
        borderColor = QColor(annElement.attribute(QStringLiteral("borderColor")));
    }
    if (annotType == QStringLiteral("note-inline") || annotType == QStringLiteral("note-callout")) {
        currentColor = borderColor.isValid() ? borderColor : (textColor.isValid() ? textColor : engineColor);
        currentInnerColor = color;
        currentTextColor = textColor.isValid() ? textColor : Qt::black;
    } else if (annotType == QStringLiteral("typewriter")) {
        currentColor = textColor;
        currentInnerColor = QColor();
        currentTextColor = textColor;
    } else {
        currentColor = color;
        currentInnerColor = innerColor;
        currentTextColor = QColor();
    }

    if (annElement.hasAttribute(QStringLiteral("font"))) {
        currentFont.fromString(annElement.attribute(QStringLiteral("font")));
    }

    // if the width value is not a default one, insert a new action in the width list
    if (annElement.hasAttribute(QStringLiteral("width"))) {
        double width = annElement.attribute(QStringLiteral("width")).toDouble();
        aCustomWidth = selectActionItem(aWidth, aCustomWidth, width, widthStandardValues, widthIcon(width), i18nc("@item:inlistbox", "Width %1", width));
    }

    // if the opacity value is not a default one, insert a new action in the opacity list
    if (annElement.hasAttribute(QStringLiteral("opacity"))) {
        double opacity = annElement.attribute(QStringLiteral("opacity")).toDouble();
        aCustomOpacity = selectActionItem(aOpacity, aCustomOpacity, opacity, opacityStandardValues, GuiUtils::createOpacityIcon(opacity), i18nc("@item:inlistbox", "%1%", opacity * 100));
    } else {
        aOpacity->setCurrentItem(opacityStandardValues.size() - 1); // 100 %
    }

    // if the tool is a custom stamp, insert a new action in the stamp list
    if (annotType == QStringLiteral("stamp")) {
        QString stampIconName = annElement.attribute(QStringLiteral("icon"));
        maybeUpdateCustomStampAction(stampIconName);
    }

    updateConfigActions(annotType);
}

void AnnotationActionHandlerPrivate::updateConfigActions(const QString &annotType)
{
    const bool isAnnotationSelected = !annotType.isEmpty();
    const bool isTypewriter = annotType == QStringLiteral("typewriter");
    const bool isInlineNote = annotType == QStringLiteral("note-inline");
    const bool isCallout = annotType == QStringLiteral("note-callout");
    const bool isBoxedText = isInlineNote || isCallout;
    const bool isText = isBoxedText || isTypewriter;
    const bool isPolygon = annotType == QStringLiteral("polygon");
    const bool isShape = annotType == QStringLiteral("rectangle") || annotType == QStringLiteral("ellipse") || isPolygon;
    const bool isStraightLine = annotType == QStringLiteral("straight-line");
    const bool isLine = annotType == QStringLiteral("ink") || isStraightLine;
    const bool isStamp = annotType == QStringLiteral("stamp");

    aColor->setIcon(GuiUtils::createColorIcon({currentColor}, QIcon::fromTheme(isTypewriter ? QStringLiteral("format-text-color") : QStringLiteral("format-stroke-color"))));
    aInnerColor->setIcon(GuiUtils::createColorIcon({currentInnerColor}, QIcon::fromTheme(QStringLiteral("format-fill-color"))));
    aTextColor->setIcon(GuiUtils::createColorIcon({currentTextColor}, QIcon::fromTheme(QStringLiteral("format-text-color"))));

    aAddToQuickTools->setEnabled(isAnnotationSelected);
    aWidth->setEnabled(isLine || isShape || isBoxedText);
    aColor->setEnabled(isAnnotationSelected && !isStamp);
    aInnerColor->setEnabled(isShape || isBoxedText);
    aTextColor->setEnabled(isText);
    aOpacity->setEnabled(isAnnotationSelected);
    aFont->setEnabled(isText);
    aConstrainRatioAndAngle->setEnabled(isStraightLine || isShape);
    aAdvancedSettings->setEnabled(isAnnotationSelected);

    // set tooltips
    if (!isAnnotationSelected) {
        aWidth->setToolTip(i18nc("@info:tooltip", "Annotation line width (No annotation selected)"));
        aColor->setToolTip(i18nc("@info:tooltip", "Annotation color (No annotation selected)"));
        aInnerColor->setToolTip(i18nc("@info:tooltip", "Annotation fill color (No annotation selected)"));
        aTextColor->setToolTip(i18nc("@info:tooltip", "Annotation text color (No annotation selected)"));
        aOpacity->setToolTip(i18nc("@info:tooltip", "Annotation opacity (No annotation selected)"));
        aFont->setToolTip(i18nc("@info:tooltip", "Annotation font (No annotation selected)"));
        aAddToQuickTools->setToolTip(i18nc("@info:tooltip", "Add the current annotation to the quick annotations menu (No annotation selected)"));
        aConstrainRatioAndAngle->setToolTip(i18nc("@info:tooltip", "Constrain shape ratio to 1:1 or line angle to 15° steps (No annotation selected)"));
        aAdvancedSettings->setToolTip(i18nc("@info:tooltip", "Advanced settings for the current annotation tool (No annotation selected)"));
        return;
    }

    if (isLine || isShape || isBoxedText) {
        aWidth->setToolTip(i18nc("@info:tooltip", "Annotation line width"));
    } else {
        aWidth->setToolTip(i18nc("@info:tooltip", "Annotation line width (Current annotation has no line width)"));
    }

    if (isTypewriter) {
        aColor->setToolTip(i18nc("@info:tooltip", "Annotation text color"));
    } else if (isShape || isBoxedText) {
        aColor->setToolTip(i18nc("@info:tooltip", "Annotation border color"));
    } else {
        aColor->setToolTip(i18nc("@info:tooltip", "Annotation color"));
    }

    if (isShape || isBoxedText) {
        aInnerColor->setToolTip(i18nc("@info:tooltip", "Annotation fill color"));
    } else {
        aInnerColor->setToolTip(i18nc("@info:tooltip", "Annotation fill color (Current annotation has no fill color)"));
    }

    if (isText) {
        aTextColor->setToolTip(i18nc("@info:tooltip", "Annotation text color"));
    } else {
        aTextColor->setToolTip(i18nc("@info:tooltip", "Annotation text color (Current annotation has no text color)"));
    }

    if (isText) {
        aFont->setToolTip(i18nc("@info:tooltip", "Annotation font"));
    } else {
        aFont->setToolTip(i18nc("@info:tooltip", "Annotation font (Current annotation has no font)"));
    }

    if (isStraightLine || isPolygon) {
        aConstrainRatioAndAngle->setToolTip(i18nc("@info:tooltip", "Constrain line angle to 15° steps"));
    } else if (isShape) {
        aConstrainRatioAndAngle->setToolTip(i18nc("@info:tooltip", "Constrain shape ratio to 1:1"));
    } else {
        aConstrainRatioAndAngle->setToolTip(i18nc("@info:tooltip", "Constrain shape ratio to 1:1 or line angle to 15° steps (Not supported by current annotation)"));
    }

    aOpacity->setToolTip(i18nc("@info:tooltip", "Annotation opacity"));
    aAddToQuickTools->setToolTip(i18nc("@info:tooltip", "Add the current annotation to the quick annotations menu"));
    aAdvancedSettings->setToolTip(i18nc("@info:tooltip", "Advanced settings for the current annotation tool"));
}

void AnnotationActionHandlerPrivate::populateQuickAnnotations()
{
    if (!aQuickTools->isEnabled()) {
        return;
    }

    const QList<int> numberKeys = {Qt::Key_1, Qt::Key_2, Qt::Key_3, Qt::Key_4, Qt::Key_5, Qt::Key_6, Qt::Key_7, Qt::Key_8, Qt::Key_9, Qt::Key_0};
    const bool isFirstTimePopulated = aQuickTools->menu()->actions().isEmpty();

    // to be safe and avoid undefined states of the currently selected quick annotation
    if (isQuickToolAction(agTools->checkedAction())) {
        q->deselectAllAnnotationActions();
    }

    for (QAction *action : std::as_const(quickTools)) {
        aQuickTools->removeAction(action);
        aQuickToolsBar->removeAction(action);
        delete action;
    }
    quickTools.clear();
    textQuickTools.clear();

    int favToolId = 1;
    QList<int>::const_iterator shortcutNumber = numberKeys.begin();
    QDomElement favToolElement = annotator->quickTool(favToolId);
    int actionBarInsertPosition = 0;
    QAction *aSeparator = aQuickTools->menu()->actions().constFirst();
    while (!favToolElement.isNull()) {
        QString itemText = favToolElement.attribute(QStringLiteral("name"));
        if (favToolElement.attribute(QStringLiteral("default"), QStringLiteral("false")) == QLatin1String("true")) {
            itemText = i18n(itemText.toLatin1().constData());
        }
        if (itemText.isEmpty()) {
            itemText = PageViewAnnotator::defaultToolName(favToolElement);
        }
        QIcon toolIcon = QIcon(PageViewAnnotator::makeToolPixmap(favToolElement));
        QAction *annFav = new KToggleAction(toolIcon, itemText, q);
        aQuickTools->insertAction(aSeparator, annFav);
        aQuickToolsBar->insertAction(actionBarInsertPosition++, annFav);
        agTools->addAction(annFav);
        quickTools.append(annFav);
        if (shortcutNumber != numberKeys.end()) {
            annFav->setShortcut(QKeySequence(*(shortcutNumber++)));
            annFav->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        }
        QObject::connect(annFav, &KToggleAction::toggled, q, [this, favToolId](bool checked) {
            if (checked) {
                slotQuickToolSelected(favToolId);
            }
        });
        QDomElement engineElement = favToolElement.firstChildElement(QStringLiteral("engine"));
        if (engineElement.attribute(QStringLiteral("type")) == QStringLiteral("TextSelector")) {
            textQuickTools.append(annFav);
            annFav->setEnabled(textToolsEnabled);
        }
        favToolElement = annotator->quickTool(++favToolId);
    }
    aQuickToolsBar->recreateWidgets();

    // set the default action
    if (quickTools.isEmpty()) {
        aShowToolBar->setVisible(false);
        aQuickTools->addAction(aToolBarVisibility);
        aQuickTools->setDefaultAction(aToolBarVisibility);
        Okular::Settings::setQuickAnnotationDefaultAction(0);
        Okular::Settings::self()->save();
    } else {
        aShowToolBar->setVisible(true);
        aQuickTools->removeAction(aToolBarVisibility);
        aQuickTools->setDefaultAction(aQuickTools);
        int defaultAction = Okular::Settings::quickAnnotationDefaultAction();
        if (isFirstTimePopulated && defaultAction < quickTools.count()) {
            // we can reach here also if no quick tools were defined before, in that case defaultAction is correctly equal to zero
            aQuickTools->setDefaultAction(quickTools.at(defaultAction));
        } else {
            // if the quick tools have been modified we cannot restore the previous default action
            aQuickTools->setDefaultAction(quickTools.at(0));
            Okular::Settings::setQuickAnnotationDefaultAction(0);
            Okular::Settings::self()->save();
        }
    }
}

KSelectAction *AnnotationActionHandlerPrivate::colorPickerAction(AnnotationColor colorType)
{
    auto colorList = defaultColors;
    QString aText(i18nc("@action:intoolbar Current annotation config option", "Color"));
    if (colorType == AnnotationColor::InnerColor) {
        aText = i18nc("@action:intoolbar Current annotation config option", "Fill Color");
        colorList.append(QPair<KLocalizedString, Qt::GlobalColor>(ki18nc("@item:inlistbox Color name", "Transparent"), Qt::transparent));
    } else if (colorType == AnnotationColor::TextColor) {
        aText = i18nc("@action:intoolbar Current annotation config option", "Text Color");
    }
    KSelectAction *aColorPicker = new KSelectAction(QIcon(), aText, q);
    aColorPicker->setToolBarMode(KSelectAction::MenuMode);
    for (const auto &colorNameValue : colorList) {
        QColor color(colorNameValue.second);
        QAction *colorAction = new QAction(GuiUtils::createColorIcon({color}, QIcon(), GuiUtils::VisualizeTransparent), colorNameValue.first.toString(), q);
        aColorPicker->addAction(colorAction);
        QObject::connect(colorAction, &QAction::triggered, q, [this, colorType, color]() { slotSetColor(colorType, color); });
    }
    QAction *aCustomColor = new QAction(QIcon::fromTheme(QStringLiteral("color-picker")), i18nc("@item:inlistbox", "Custom Color…"), q);
    aColorPicker->addAction(aCustomColor);
    QObject::connect(aCustomColor, &QAction::triggered, q, [this, colorType]() { slotSetColor(colorType); });
    return aColorPicker;
}

const QIcon AnnotationActionHandlerPrivate::widthIcon(double width)
{
    QPixmap pm(32, 32);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(Qt::black, 2 * width, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(0, pm.height() / 2, pm.width(), pm.height() / 2);
    p.end();
    return QIcon(pm);
}

const QIcon AnnotationActionHandlerPrivate::stampIcon(const QString &stampIconName)
{
    QPixmap stampPix = GuiUtils::stampPixmap(stampIconName, QSize(32, 32));
    if (stampPix.width() == stampPix.height()) {
        return QIcon(stampPix);
    } else {
        return QIcon::fromTheme(QStringLiteral("tag"));
    }
}

void AnnotationActionHandlerPrivate::addBuiltinToolAction(QAction *action, const QString &type, const QString &name)
{
    agTools->addAction(action);
    builtinToolForAction.insert(action, {type, name});
    QObject::connect(action, &QAction::toggled, q, [this, action](bool checked) {
        if (checked) {
            selectTool(builtinToolForAction.value(action));
        }
    });
}

void AnnotationActionHandlerPrivate::selectTool(int toolId)
{
    selectedBuiltinTool = toolId;
    annotator->selectBuiltinTool(toolId, PageViewAnnotator::ShowTip::Yes);
    parseTool(toolId);
}

void AnnotationActionHandlerPrivate::selectTool(const BuiltinToolSpec &toolSpec)
{
    selectedBuiltinTool = annotator->selectBuiltinToolByType(toolSpec.type, toolSpec.name, PageViewAnnotator::ShowTip::Yes);
    parseTool(selectedBuiltinTool);
}

void AnnotationActionHandlerPrivate::slotStampToolSelected(const QString &stamp, const QString &contents, bool latexNoteBoxed)
{
    selectedBuiltinTool = annotator->selectStampTool(stamp, contents, latexNoteBoxed);
    if (selectedBuiltinTool == -1) {
        return;
    }
    maybeUpdateCustomStampAction(stamp);
    updateConfigActions(QStringLiteral("stamp"));
}

void AnnotationActionHandlerPrivate::slotSelectCustomStamp()
{
    const QString customStampFile = QFileDialog::getOpenFileName(nullptr,
                                                                 i18nc("@title:window file chooser", "Select Image Note"),
                                                                 QString(),
                                                                 i18n("*.ico *.png *.xpm *.svg *.svgz | Icon Files (*.ico *.png *.xpm *.svg *.svgz)"));
    if (customStampFile.isEmpty()) {
        return;
    }

    const QPixmap pixmap = Okular::AnnotationUtils::loadStamp(customStampFile, QSize(64, 64));
    if (pixmap.isNull()) {
        KMessageBox::error(nullptr, xi18nc("@info", "Could not load the file <filename>%1</filename>", customStampFile), i18nc("@title:window", "Invalid file"));
        return;
    }

    slotStampToolSelected(customStampFile);
}

void AnnotationActionHandlerPrivate::slotAddLatexNote(bool boxed)
{
    bool ok = false;
    const QString title = boxed ? i18nc("@title:window", "Add LaTeX Inline Note") : i18nc("@title:window", "Add LaTeX Note");
    const QString latexInput = QInputDialog::getMultiLineText(nullptr, title, i18nc("@label:textbox", "LaTeX source:"), QString(), &ok).trimmed();
    if (!ok || latexInput.isEmpty()) {
        return;
    }

    const LatexNoteUtils::RenderResult rendered = LatexNoteUtils::renderToCache(latexInput, Qt::black, LatexNoteUtils::latexFontSize(), 0.0);
    if (!rendered.ok) {
        KMessageBox::error(nullptr, rendered.errorMessage, i18n("LaTeX rendering failed"));
        return;
    }
    LatexNoteUtils::showRenderWarning(qobject_cast<QWidget *>(annotator ? annotator->parent() : nullptr), rendered.warning);

    if (boxed) {
        selectedBuiltinTool = annotator->selectLatexFreeTextTool(rendered.pdfFileName, latexInput);
        if (selectedBuiltinTool != -1) {
            updateConfigActions(QStringLiteral("note-inline"));
        }
    } else {
        slotStampToolSelected(rendered.pdfFileName, latexInput, false);
    }
}

void AnnotationActionHandlerPrivate::slotQuickToolSelected(int favToolId)
{
    annotator->selectQuickTool(favToolId);
    selectedBuiltinTool = -1;
    updateConfigActions();
    Okular::Settings::setQuickAnnotationDefaultAction(favToolId - 1);
    Okular::Settings::self()->save();
}

void AnnotationActionHandlerPrivate::slotSetColor(AnnotationColor colorType, const QColor &color)
{
    QColor selectedColor(color);
    if (!selectedColor.isValid()) {
        const QColor initialColor = colorType == AnnotationColor::InnerColor ? currentInnerColor : (colorType == AnnotationColor::TextColor ? currentTextColor : currentColor);
        selectedColor = QColorDialog::getColor(initialColor, nullptr, i18nc("@title:window", "Select color"));
        if (!selectedColor.isValid()) {
            return;
        }
    }
    if (colorType == AnnotationColor::Color) {
        currentColor = selectedColor;
        annotator->setAnnotationColor(selectedColor);
    } else if (colorType == AnnotationColor::InnerColor) {
        currentInnerColor = selectedColor;
        annotator->setAnnotationInnerColor(selectedColor);
    } else if (colorType == AnnotationColor::TextColor) {
        currentTextColor = selectedColor;
        annotator->setAnnotationTextColor(selectedColor);
    }
}

void AnnotationActionHandlerPrivate::slotSelectAnnotationFont()
{
    bool ok;
    QFont selectedFont = QFontDialog::getFont(&ok, currentFont);
    if (ok) {
        currentFont = selectedFont;
        annotator->setAnnotationFont(currentFont);
    }
}

bool AnnotationActionHandlerPrivate::isQuickToolAction(QAction *aTool)
{
    return quickTools.contains(aTool);
}

bool AnnotationActionHandlerPrivate::isQuickToolStamp(int toolId)
{
    QDomElement toolElement = annotator->quickTool(toolId);
    const QString annotType = toolElement.attribute(QStringLiteral("type"));
    QDomElement engineElement = toolElement.firstChildElement(QStringLiteral("engine"));
    QDomElement annElement = engineElement.firstChildElement(QStringLiteral("annotation"));
    return annotType == QStringLiteral("stamp");
}

void AnnotationActionHandlerPrivate::assertToolBarExists(KParts::MainWindow *mw, const QString &toolBarName)
{
    QList<KToolBar *> toolbars = mw->toolBars();
    auto itToolBar = std::find_if(toolbars.begin(), toolbars.end(), [&](const KToolBar *toolBar) { return toolBar->objectName() == toolBarName; });
    Q_ASSERT(itToolBar != toolbars.end());
}

// TODO: icon names should match getAnnotationInfo in gui/guiutils.cpp
AnnotationActionHandler::AnnotationActionHandler(PageViewAnnotator *parent, KActionCollection *ac)
    : QObject(parent)
    , d(new AnnotationActionHandlerPrivate(this))
{
    d->annotator = parent;

    // toolbar visibility actions
    d->aToolBarVisibility = new KToggleAction(QIcon::fromTheme(QStringLiteral("draw-freehand")), i18n("&Annotations"), this);
    d->aHideToolBar = new QAction(QIcon::fromTheme(QStringLiteral("dialog-close")), i18nc("@action:intoolbar Hide the toolbar", "Hide"), this);
    d->aShowToolBar = new QAction(QIcon::fromTheme(QStringLiteral("draw-freehand")), i18nc("@action:intoolbar Show the builtin annotation toolbar", "Show more annotation tools"), this);

    // Text markup actions
    KToggleAction *aHighlighter = new KToggleAction(QIcon::fromTheme(QStringLiteral("draw-highlight")), i18nc("@action:intoolbar Annotation tool", "Highlighter"), this);
    KToggleAction *aUnderline = new KToggleAction(QIcon::fromTheme(QStringLiteral("format-text-underline")), i18nc("@action:intoolbar Annotation tool", "Underline"), this);
    KToggleAction *aSquiggle = new KToggleAction(QIcon::fromTheme(QStringLiteral("format-text-underline-squiggle")), i18nc("@action:intoolbar Annotation tool", "Squiggle"), this);
    KToggleAction *aStrikeout = new KToggleAction(QIcon::fromTheme(QStringLiteral("format-text-strikethrough")), i18nc("@action:intoolbar Annotation tool", "Strike Out"), this);
    // Notes actions
    KToggleAction *aTypewriter = new KToggleAction(QIcon::fromTheme(QStringLiteral("tool-text")), i18nc("@action:intoolbar Annotation tool", "Typewriter"), this);
    KToggleAction *aInlineNote = new KToggleAction(QIcon::fromTheme(QStringLiteral("note")), i18nc("@action:intoolbar Annotation tool", "Inline Note"), this);
    KToggleAction *aPopupNote = new KToggleAction(QIcon::fromTheme(QStringLiteral("edit-comment")), i18nc("@action:intoolbar Annotation tool", "Popup Note"), this);
    KToggleAction *aCallout = new KToggleAction(QIcon::fromTheme(QStringLiteral("draw-arrow")), i18nc("@action:intoolbar Annotation tool", "Callout"), this);
    KToggleAction *aFreehandLine = new KToggleAction(QIcon::fromTheme(QStringLiteral("draw-freehand")), i18nc("@action:intoolbar Annotation tool", "Freehand Line"), this);
    // Geometrical shapes actions
    KToggleAction *aStraightLine = new KToggleAction(QIcon::fromTheme(QStringLiteral("draw-line")), i18nc("@action:intoolbar Annotation tool", "Straight line"), this);
    KToggleAction *aArrow = new KToggleAction(QIcon::fromTheme(QStringLiteral("draw-arrow")), i18nc("@action:intoolbar Annotation tool", "Arrow"), this);
    KToggleAction *aRectangle = new KToggleAction(QIcon::fromTheme(QStringLiteral("draw-rectangle")), i18nc("@action:intoolbar Annotation tool", "Rectangle"), this);
    KToggleAction *aEllipse = new KToggleAction(QIcon::fromTheme(QStringLiteral("draw-ellipse")), i18nc("@action:intoolbar Annotation tool", "Ellipse"), this);
    KToggleAction *aPolygon = new KToggleAction(QIcon::fromTheme(QStringLiteral("draw-polyline")), i18nc("@action:intoolbar Annotation tool", "Polygon"), this);
    d->aGeomShapes = new ToggleActionMenu(i18nc("@action", "Geometrical shapes"), this);
    d->aGeomShapes->setEnabled(true); // Need to explicitly set this once, or refreshActions() in part.cpp will disable this action
    d->aGeomShapes->setPopupMode(QToolButton::MenuButtonPopup);
    d->aGeomShapes->addAction(aArrow);
    d->aGeomShapes->addAction(aStraightLine);
    d->aGeomShapes->addAction(aRectangle);
    d->aGeomShapes->addAction(aEllipse);
    d->aGeomShapes->addAction(aPolygon);
    d->aGeomShapes->setDefaultAction(aArrow);
    connect(d->aGeomShapes->menu(), &QMenu::triggered, d->aGeomShapes, &ToggleActionMenu::setDefaultAction);

    d->agTools = new QActionGroup(this);
    d->addBuiltinToolAction(aHighlighter, QStringLiteral("highlight"));
    d->addBuiltinToolAction(aUnderline, QStringLiteral("underline"));
    d->addBuiltinToolAction(aSquiggle, QStringLiteral("squiggly"));
    d->addBuiltinToolAction(aStrikeout, QStringLiteral("strikeout"));
    d->addBuiltinToolAction(aTypewriter, QStringLiteral("typewriter"));
    d->addBuiltinToolAction(aInlineNote, QStringLiteral("note-inline"));
    d->addBuiltinToolAction(aPopupNote, QStringLiteral("note-linked"));
    d->addBuiltinToolAction(aFreehandLine, QStringLiteral("ink"));
    d->addBuiltinToolAction(aArrow, QStringLiteral("straight-line"), QStringLiteral("Arrow"));
    d->addBuiltinToolAction(aStraightLine, QStringLiteral("straight-line"));
    d->addBuiltinToolAction(aRectangle, QStringLiteral("rectangle"));
    d->addBuiltinToolAction(aEllipse, QStringLiteral("ellipse"));
    d->addBuiltinToolAction(aPolygon, QStringLiteral("polygon"));
    d->addBuiltinToolAction(aCallout, QStringLiteral("note-callout"));

    d->textTools.append(aHighlighter);
    d->textTools.append(aUnderline);
    d->textTools.append(aSquiggle);
    d->textTools.append(aStrikeout);

    // Stamp action
    d->aStamp = new ToggleActionMenu(QIcon::fromTheme(QStringLiteral("tag")), i18nc("@action", "Stamp"), this);
    d->aStamp->setPopupMode(QToolButton::MenuButtonPopup);
    for (const auto &stamp : StampAnnotationWidget::defaultStamps()) {
        KToggleAction *ann = new KToggleAction(d->stampIcon(stamp.second), stamp.first, this);
        d->aStamp->addAction(ann);
        d->agTools->addAction(ann);
        // action group workaround: connecting to toggled instead of triggered
        // (because deselectAllAnnotationActions has to call triggered)
        connect(ann, &QAction::toggled, this, [this, stamp](bool checked) {
            if (checked) {
                d->slotStampToolSelected(stamp.second);
            }
        });
    }
    if (!d->aStamp->menu()->actions().isEmpty()) {
        d->aStamp->setDefaultAction(d->aStamp->menu()->actions().constFirst());
    }
    QAction *aStampSeparator = new QAction(this);
    aStampSeparator->setSeparator(true);
    d->aStamp->addAction(aStampSeparator);
    d->aSelectCustomStamp = new QAction(QIcon::fromTheme(QStringLiteral("image-x-generic")), i18nc("@action:intoolbar Annotation tool", "Add Image Note…"), this);
    d->aSelectCustomStamp->setToolTip(i18nc("@info:tooltip", "Add an image as a movable annotation"));
    d->aStamp->addAction(d->aSelectCustomStamp);
    connect(d->aSelectCustomStamp, &QAction::triggered, this, [this]() { d->slotSelectCustomStamp(); });
    d->aAddLatexNote = new QAction(QIcon::fromTheme(QStringLiteral("text-x-tex")), i18nc("@action:intoolbar Annotation tool", "Add LaTeX Note…"), this);
    d->aAddLatexNote->setToolTip(i18nc("@info:tooltip", "Add rendered LaTeX as a movable annotation"));
    connect(d->aAddLatexNote, &QAction::triggered, this, [this]() { d->slotAddLatexNote(); });
    d->aAddLatexInlineNote = new QAction(QIcon::fromTheme(QStringLiteral("note")), i18nc("@action:intoolbar Annotation tool", "Add LaTeX Inline Note…"), this);
    d->aAddLatexInlineNote->setToolTip(i18nc("@info:tooltip", "Add rendered LaTeX with an inline-note background and border"));
    connect(d->aAddLatexInlineNote, &QAction::triggered, this, [this]() { d->slotAddLatexNote(true); });
    connect(d->aStamp->menu(), &QMenu::triggered, this, [this](QAction *action) {
        if (action->isCheckable()) {
            d->aStamp->setDefaultAction(action);
        }
    });

    // Quick annotations action
    d->aQuickTools = new ToggleActionMenu(i18nc("@action:intoolbar Show list of quick annotation tools", "Quick Annotations"), this);
    d->aQuickTools->setPopupMode(QToolButton::MenuButtonPopup);
    d->aQuickTools->setIcon(QIcon::fromTheme(QStringLiteral("draw-freehand")));
    d->aQuickTools->setToolTip(i18nc("@info:tooltip", "Choose an annotation tool from the quick annotations"));
    d->aQuickTools->setEnabled(true); // required to ensure that populateQuickAnnotations is executed the first time
    // set the triggered quick annotation as default action (but avoid setting 'Configure...' as default action)
    connect(d->aQuickTools->menu(), &QMenu::triggered, this, [this](QAction *action) {
        if (action->isCheckable()) {
            d->aQuickTools->setDefaultAction(action);
        }
    });

    d->aQuickToolsBar = new ActionBar(this);
    d->aQuickToolsBar->setText(i18n("Quick Annotation Bar"));

    QAction *aQuickToolsSeparator = new QAction(this);
    aQuickToolsSeparator->setSeparator(true);
    d->aQuickTools->addAction(aQuickToolsSeparator);
    d->aQuickTools->addAction(d->aShowToolBar);
    QAction *aConfigAnnotation = ac->action(QStringLiteral("options_configure_annotations"));
    if (aConfigAnnotation) {
        d->aQuickTools->addAction(aConfigAnnotation);
        d->aQuickToolsBar->addAction(aConfigAnnotation);
    }
    d->populateQuickAnnotations();

    // Add to quick annotation action
    d->aAddToQuickTools = new QAction(QIcon::fromTheme(QStringLiteral("favorite")), i18nc("@action:intoolbar Add current annotation tool to the quick annotations list", "Add to Quick Annotations"), this);

    // Pin action
    d->aContinuousMode = new KToggleAction(QIcon::fromTheme(QStringLiteral("pin")), i18nc("@action:intoolbar When checked keep the current annotation tool active after use", "Keep Active"), this);
    d->aContinuousMode->setToolTip(i18nc("@info:tooltip", "Keep the annotation tool active after use"));
    d->aContinuousMode->setChecked(d->annotator->continuousMode());

    // Constrain angle action
    d->aConstrainRatioAndAngle =
        new KToggleAction(QIcon::fromTheme(QStringLiteral("snap-angle")), i18nc("@action When checked, line annotations are constrained to 15° steps, shape annotations to 1:1 ratio", "Constrain Ratio and Angle of Annotation Tools"), this);
    d->aConstrainRatioAndAngle->setChecked(d->annotator->constrainRatioAndAngleActive());

    // Annotation settings actions
    d->aColor = d->colorPickerAction(AnnotationActionHandlerPrivate::AnnotationColor::Color);
    d->aInnerColor = d->colorPickerAction(AnnotationActionHandlerPrivate::AnnotationColor::InnerColor);
    d->aTextColor = d->colorPickerAction(AnnotationActionHandlerPrivate::AnnotationColor::TextColor);
    d->aFont = new QAction(QIcon::fromTheme(QStringLiteral("font-face")), i18nc("@action:intoolbar Current annotation config option", "Font"), this);
    d->aAdvancedSettings = new QAction(QIcon::fromTheme(QStringLiteral("settings-configure")), i18nc("@action:intoolbar Current annotation advanced settings", "Annotation Settings"), this);

    // Width list
    d->aWidth = new KSelectAction(QIcon::fromTheme(QStringLiteral("edit-line-width")), i18nc("@action:intoolbar Current annotation config option", "Line width"), this);
    d->aWidth->setToolBarMode(KSelectAction::MenuMode);
    for (auto width : d->widthStandardValues) {
        KToggleAction *ann = new KToggleAction(d->widthIcon(width), i18nc("@item:inlistbox", "Width %1", width), this);
        d->aWidth->addAction(ann);
        connect(ann, &QAction::triggered, this, [this, width]() { d->annotator->setAnnotationWidth(width); });
    }

    // Opacity list
    d->aOpacity = new KSelectAction(QIcon::fromTheme(QStringLiteral("edit-opacity")), i18nc("@action:intoolbar Current annotation config option", "Opacity"), this);
    d->aOpacity->setToolBarMode(KSelectAction::MenuMode);
    for (double opacity : d->opacityStandardValues) {
        KToggleAction *ann = new KToggleAction(GuiUtils::createOpacityIcon(opacity), i18nc("@item:inlistbox Annotation opacity percentage level, make sure to include %1 in your translation", "%1%", opacity * 100), this);
        d->aOpacity->addAction(ann);
        connect(ann, &QAction::triggered, this, [this, opacity]() { d->annotator->setAnnotationOpacity(opacity); });
    }

    connect(d->aAddToQuickTools, &QAction::triggered, d->annotator, &PageViewAnnotator::addToQuickAnnotations);
    connect(d->aContinuousMode, &QAction::toggled, d->annotator, &PageViewAnnotator::setContinuousMode);
    connect(d->aConstrainRatioAndAngle, &QAction::toggled, d->annotator, &PageViewAnnotator::setConstrainRatioAndAngle);
    connect(d->aAdvancedSettings, &QAction::triggered, d->annotator, &PageViewAnnotator::slotAdvancedSettings);
    connect(d->aFont, &QAction::triggered, std::bind(&AnnotationActionHandlerPrivate::slotSelectAnnotationFont, d));

    // action group workaround: allows unchecking the currently selected annotation action.
    // Other parts of code dependent to this workaround are marked with "action group workaround".
    connect(d->agTools, &QActionGroup::triggered, this, [this](QAction *action) {
        if (action == d->agLastAction) {
            d->agLastAction = nullptr;
            d->agTools->checkedAction()->setChecked(false);
            d->selectTool(-1);
        } else {
            d->agLastAction = action;
            // Show the annotation toolbar whenever builtin tool actions are triggered (e.g using shortcuts)
            if (!d->isQuickToolAction(action)) {
                d->aToolBarVisibility->setChecked(true);
            }
        }
    });

    ac->addAction(QStringLiteral("mouse_toggle_annotate"), d->aToolBarVisibility);
    ac->addAction(QStringLiteral("hide_annotation_toolbar"), d->aHideToolBar);
    ac->addAction(QStringLiteral("quick_annotation_action_bar"), d->aQuickToolsBar);
    ac->addAction(QStringLiteral("annotation_highlighter"), aHighlighter);
    ac->addAction(QStringLiteral("annotation_underline"), aUnderline);
    ac->addAction(QStringLiteral("annotation_squiggle"), aSquiggle);
    ac->addAction(QStringLiteral("annotation_strike_out"), aStrikeout);
    ac->addAction(QStringLiteral("annotation_typewriter"), aTypewriter);
    ac->addAction(QStringLiteral("annotation_inline_note"), aInlineNote);
    ac->addAction(QStringLiteral("annotation_popup_note"), aPopupNote);
    ac->addAction(QStringLiteral("annotation_callout"), aCallout);
    ac->addAction(QStringLiteral("annotation_freehand_line"), aFreehandLine);
    ac->addAction(QStringLiteral("annotation_arrow"), aArrow);
    ac->addAction(QStringLiteral("annotation_straight_line"), aStraightLine);
    ac->addAction(QStringLiteral("annotation_rectangle"), aRectangle);
    ac->addAction(QStringLiteral("annotation_ellipse"), aEllipse);
    ac->addAction(QStringLiteral("annotation_polygon"), aPolygon);
    ac->addAction(QStringLiteral("annotation_geometrical_shape"), d->aGeomShapes);
    ac->addAction(QStringLiteral("annotation_stamp"), d->aStamp);
    ac->addAction(QStringLiteral("annotation_add_image_note"), d->aSelectCustomStamp);
    ac->addAction(QStringLiteral("annotation_add_latex_note"), d->aAddLatexNote);
    ac->addAction(QStringLiteral("annotation_add_latex_inline_note"), d->aAddLatexInlineNote);
    ac->addAction(QStringLiteral("annotation_favorites"), d->aQuickTools);
    ac->addAction(QStringLiteral("annotation_bookmark"), d->aAddToQuickTools);
    ac->addAction(QStringLiteral("annotation_settings_pin"), d->aContinuousMode);
    ac->addAction(QStringLiteral("annotation_constrain_ratio_angle"), d->aConstrainRatioAndAngle);
    ac->addAction(QStringLiteral("annotation_settings_width"), d->aWidth);
    ac->addAction(QStringLiteral("annotation_settings_color"), d->aColor);
    ac->addAction(QStringLiteral("annotation_settings_inner_color"), d->aInnerColor);
    ac->addAction(QStringLiteral("annotation_settings_text_color"), d->aTextColor);
    ac->addAction(QStringLiteral("annotation_settings_opacity"), d->aOpacity);
    ac->addAction(QStringLiteral("annotation_settings_font"), d->aFont);
    ac->addAction(QStringLiteral("annotation_settings_advanced"), d->aAdvancedSettings);

    ac->setDefaultShortcut(d->aToolBarVisibility, Qt::Key_F6);
    ac->setDefaultShortcut(aHighlighter, Qt::ALT | Qt::Key_1);
    ac->setDefaultShortcut(aUnderline, Qt::ALT | Qt::Key_2);
    ac->setDefaultShortcut(aSquiggle, Qt::ALT | Qt::Key_3);
    ac->setDefaultShortcut(aStrikeout, Qt::ALT | Qt::Key_4);
    ac->setDefaultShortcut(aTypewriter, Qt::ALT | Qt::Key_5);
    ac->setDefaultShortcut(aInlineNote, Qt::ALT | Qt::Key_6);
    ac->setDefaultShortcut(aPopupNote, Qt::ALT | Qt::Key_7);
    ac->setDefaultShortcut(aFreehandLine, Qt::ALT | Qt::Key_8);
    ac->setDefaultShortcut(aArrow, Qt::ALT | Qt::Key_9);
    ac->setDefaultShortcut(aRectangle, Qt::ALT | Qt::Key_0);
    ac->setDefaultShortcut(d->aAddToQuickTools, QKeySequence((Qt::CTRL | Qt::SHIFT) | Qt::Key_B));
    d->updateConfigActions();

    connect(Okular::Settings::self(), &Okular::Settings::primaryAnnotationToolBarChanged, this, &AnnotationActionHandler::setupAnnotationToolBarVisibilityAction);
}

AnnotationActionHandler::~AnnotationActionHandler()
{
    // delete the private data storage structure
    delete d;
}

void AnnotationActionHandler::setupAnnotationToolBarVisibilityAction()
{
    // find the main window associated to the toggle toolbar action
    QList<QObject *> objects = d->aToolBarVisibility->associatedObjects();
    auto itMainWindow = std::find_if(objects.begin(), objects.end(), [](const QObject *object) { return qobject_cast<const KParts::MainWindow *>(object) != nullptr; });
    Q_ASSERT(itMainWindow != objects.end());
    KParts::MainWindow *mw = qobject_cast<KParts::MainWindow *>(*itMainWindow);

    // ensure that the annotation toolbars have been created
    d->assertToolBarExists(mw, QStringLiteral("annotationToolBar"));
    d->assertToolBarExists(mw, QStringLiteral("quickAnnotationToolBar"));

    KToolBar *annotationToolBar = mw->toolBar(QStringLiteral("annotationToolBar"));
    connect(annotationToolBar, &QToolBar::visibilityChanged, this, &AnnotationActionHandler::slotAnnotationToolBarVisibilityChanged, Qt::UniqueConnection);
    // show action
    connect(d->aShowToolBar, &QAction::triggered, annotationToolBar, &KToolBar::show, Qt::UniqueConnection);
    // hide action
    connect(d->aHideToolBar, &QAction::triggered, annotationToolBar, &KToolBar::hide, Qt::UniqueConnection);

    KToolBar *primaryAnnotationToolBar = annotationToolBar;
    if (Okular::Settings::primaryAnnotationToolBar() == Okular::Settings::EnumPrimaryAnnotationToolBar::QuickAnnotationToolBar) {
        primaryAnnotationToolBar = mw->toolBar(QStringLiteral("quickAnnotationToolBar"));
    }
    d->aToolBarVisibility->setChecked(false);
    d->aToolBarVisibility->disconnect(primaryAnnotationToolBar);
    d->aToolBarVisibility->setChecked(primaryAnnotationToolBar->isVisible());
    connect(primaryAnnotationToolBar, &QToolBar::visibilityChanged, d->aToolBarVisibility, &QAction::setChecked, Qt::UniqueConnection);
    connect(d->aToolBarVisibility, &QAction::toggled, primaryAnnotationToolBar, &KToolBar::setVisible, Qt::UniqueConnection);
    d->aShowToolBar->setEnabled(!primaryAnnotationToolBar->isVisible());
}

void AnnotationActionHandler::reparseBuiltinToolsConfig()
{
    d->parseTool(d->selectedBuiltinTool);
}

void AnnotationActionHandler::reparseQuickToolsConfig()
{
    d->populateQuickAnnotations();
}

void AnnotationActionHandler::setToolsEnabled(bool on)
{
    const QList<QAction *> tools = d->agTools->actions();
    for (QAction *ann : tools) {
        ann->setEnabled(on);
    }
    d->aQuickTools->setEnabled(on);
    d->aGeomShapes->setEnabled(on);
    d->aStamp->setEnabled(on);
    d->aSelectCustomStamp->setEnabled(on);
    d->aAddLatexNote->setEnabled(on);
    d->aAddLatexInlineNote->setEnabled(on);
    d->aContinuousMode->setEnabled(on);
}

void AnnotationActionHandler::setTextToolsEnabled(bool on)
{
    d->textToolsEnabled = on;
    for (QAction *ann : std::as_const(d->textTools)) {
        ann->setEnabled(on);
    }
    for (QAction *ann : std::as_const(d->textQuickTools)) {
        ann->setEnabled(on);
    }
}

void AnnotationActionHandler::deselectAllAnnotationActions()
{
    QAction *checkedAction = d->agTools->checkedAction();
    if (checkedAction) {
        d->agLastAction = checkedAction;
        checkedAction->trigger(); // action group workaround: using trigger instead of setChecked
    }
}

void AnnotationActionHandler::slotAnnotationToolBarVisibilityChanged(bool visible)
{
    d->aShowToolBar->setEnabled(!visible);
    if (!visible && !d->isQuickToolAction(d->agTools->checkedAction())) {
        deselectAllAnnotationActions();
    }
}

#include "moc_annotationactionhandler.cpp"

#include "QtRocket/gui/plot/PlotExport.h"

#include <filesystem>
#include <optional>
#include <system_error>

#include <QAction>
#include <QChar>
#include <QCoreApplication>
#include <QList>
#include <QMenu>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <Qt>
#include <QtContainerFwd>
#include <QtTypes>

#include <rocketplot/Legend.h>
#include <rocketplot/PlotWidget.h>

#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/util/FileIo.h"

namespace QtRocket::Gui
{

namespace
{

/// The translation context of rocketplot's own texts (PlotWidget's tr()).
constexpr const char* kRocketplotContext = "rocketplot::PlotWidget";

/// A text of rocketplot's menus as the plot shows it: @p source (rocketplot's English text) or
/// its translation, when one is installed.
[[nodiscard]] QString rocketplotText(const char* source)
{
    return QCoreApplication::translate(kRocketplotContext, source);
}

/// The first action of @p menu whose text is @p text, or null.
[[nodiscard]] QAction* actionWithText(const QMenu& menu, const QString& text)
{
    const QList<QAction*> actions = menu.actions();
    for (QAction* const action : actions)
    {
        if (action->text() == text)
        {
            return action;
        }
    }
    return nullptr;
}

/// The text a name filter lists its patterns in: what is between its last parentheses, or the
/// whole filter without them ("*.png").
[[nodiscard]] QString patternsOf(const QString& nameFilter)
{
    const qsizetype open  = nameFilter.lastIndexOf(QLatin1Char('('));
    const qsizetype close = nameFilter.lastIndexOf(QLatin1Char(')'));
    if (open < 0 || close < open)
    {
        return nameFilter;
    }
    return nameFilter.mid(open + 1, close - open - 1);
}

/// The suffix of the name @p name as FileHelper.forceExtension() reads it: the text after its last
/// dot, none for a name that starts with its only dot (or has none).
[[nodiscard]] QString suffixOfName(const QString& name)
{
    const qsizetype dot = name.lastIndexOf(QLatin1Char('.'));
    return dot > 0 ? name.mid(dot + 1) : QString();
}

/// The first suffix of @p nameFilters' filters, or empty.
[[nodiscard]] QString firstSuffix(const QStringList& nameFilters)
{
    for (const QString& filter : nameFilters)
    {
        const QStringList found = PlotExport::suffixes(filter);
        if (!found.isEmpty())
        {
            return found.front();
        }
    }
    return {};
}

}  // namespace

QStringList PlotExport::nameFilters()
{
    return {
        //: FileHelper.PNG_FILTER
        tr("PNG image (*.png)"),
        //: rocketplot's text (a kind of file of its Export... dialog), not in OpenRocket
        tr("JPEG image (*.jpg)"),
        //: FileHelper.SVG_FILTER
        tr("SVG files (*.svg)"),
        //: FileHelper.PDF_FILTER
        tr("PDF files (*.pdf)"),
        //: FileHelper.CSV_FILTER
        tr("Comma Separated Files (*.csv)"),
    };
}

UserPrompts::SaveFileRequest PlotExport::exportRequest(const Preferences& preferences)
{
    UserPrompts::SaveFileRequest request;
    //: rocketplot's text (the title of its Export... dialog), not in OpenRocket
    request.title       = tr("Export plot");
    request.nameFilters = nameFilters();
    if (const std::optional<std::filesystem::path> directory = preferences.getDefaultDirectory())
    {
        request.directory = *directory;
    }
    request.refuseIllegalNames = true;
    return request;
}

bool PlotExport::exportPlot(rocketplot::PlotWidget& plot, const GuiServices& services,
                            const UserPrompts::SaveFileRequest& request)
{
    UserPrompts& prompts = services.prompts();
    // A modal dialog runs the event loop, in which the plot may go (its window closed): nothing of
    // it is used once it has gone.
    const QPointer<rocketplot::PlotWidget>           alive(&plot);
    const std::optional<UserPrompts::SaveFileAnswer> answer = prompts.askSaveFile(&plot, request);
    if (alive.isNull() || !answer.has_value())
    {
        return false;
    }
    const std::filesystem::path file =
        withSuffix(answer->file, request.nameFilters, answer->selectedNameFilter);
    std::error_code error;
    if (std::filesystem::exists(file, error))
    {
        const bool overwrite = prompts.askOverwrite(&plot, QtStrings::fromPath(file.filename()));
        if (alive.isNull() || !overwrite)
        {
            return false;
        }
    }
    if (plot.exportTo(QtStrings::fromPath(file)))
    {
        return true;
    }
    prompts.showMessage(&plot, {.severity = UserPrompts::Severity::ERROR,
                                //: error.writing.title
                                .title = tr("Error writing file"),
                                //: error.writing.desc
                                .lines = {tr("An error occurred while writing to the file:"),
                                          QtStrings::fromPath(file)}});
    return false;
}

bool PlotExport::exportPlot(rocketplot::PlotWidget& plot, const GuiServices& services)
{
    return exportPlot(plot, services, exportRequest(services.preferences()));
}

void PlotExport::install(rocketplot::PlotWidget& plot, const GuiServices& services)
{
    // The plot is the context of every connection: none runs once it has gone.
    QObject::connect(
        &plot, &rocketplot::PlotWidget::contextMenuAboutToShow, &plot,
        guarded([plotPointer = &plot,
                 services](QMenu* menu) { replaceExportAction(*menu, *plotPointer, services); },
                QStringLiteral("plot context menu")));
    const auto                      removeColors = [](QMenu* menu) { removeColorActions(*menu); };
    const rocketplot::Legend* const legend       = plot.legend();
    QObject::connect(legend, &rocketplot::Legend::entryMenuAboutToShow, &plot,
                     guarded(removeColors, QStringLiteral("plot legend menu")));
    QObject::connect(legend, &rocketplot::Legend::annotationEntryMenuAboutToShow, &plot,
                     guarded(removeColors, QStringLiteral("plot legend menu")));
}

void PlotExport::replaceExportAction(QMenu& menu, rocketplot::PlotWidget& plot,
                                     const GuiServices& services)
{
    QAction* const own = actionWithText(menu, rocketplotText("Export\u2026"));
    //: rocketplot's text (its context menu's Export...), not in OpenRocket
    const QString fallback = tr("Export\u2026");
    auto* const   action   = makeChild<QAction>(&menu, own != nullptr ? own->text() : fallback);
    action->setObjectName(exportActionName());
    QObject::connect(action, &QAction::triggered, &plot,
                     guarded([plotPointer = &plot,
                              services] { static_cast<void>(exportPlot(*plotPointer, services)); },
                             QStringLiteral("Export plot")));
    if (own == nullptr)
    {
        menu.addAction(action);
        return;
    }
    menu.insertAction(own, action);
    // rocketplot's action stays the menu's child, deleted with it.
    menu.removeAction(own);
}

void PlotExport::removeColorActions(QMenu& menu)
{
    const QString         color   = rocketplotText("Color\u2026");
    const QList<QAction*> actions = menu.actions();
    for (QAction* const action : actions)
    {
        if (action->text() == color)
        {
            menu.removeAction(action);
        }
    }
}

QString PlotExport::exportActionName()
{
    return QStringLiteral("exportPlot");
}

QStringList PlotExport::suffixes(const QString& nameFilter)
{
    QStringList       found;
    const QStringList patterns = patternsOf(nameFilter).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString& pattern : patterns)
    {
        const QString suffix =
            pattern.startsWith(QStringLiteral("*.")) ? pattern.mid(2) : QString();
        if (!suffix.isEmpty() && !suffix.contains(QLatin1Char('*')) &&
            !suffix.contains(QLatin1Char('?')))
        {
            found.append(suffix);
        }
    }
    return found;
}

std::filesystem::path PlotExport::withSuffix(const std::filesystem::path& file,
                                             const QStringList&           nameFilters,
                                             const QString&               chosenFilter)
{
    const QString current = suffixOfName(QtStrings::fromPath(file.filename()));
    if (!current.isEmpty())
    {
        for (const QString& filter : nameFilters)
        {
            const QStringList known = suffixes(filter);
            if (known.contains(current, Qt::CaseInsensitive))
            {
                return file;
            }
        }
    }
    const QStringList chosen =
        nameFilters.contains(chosenFilter) ? suffixes(chosenFilter) : QStringList();
    const QString suffix = chosen.isEmpty() ? firstSuffix(nameFilters) : chosen.front();
    if (suffix.isEmpty())
    {
        return file;
    }
    return pathFromUtf8(pathToUtf8(file) + '.' + QtStrings::toUtf8(suffix));
}

}  // namespace QtRocket::Gui

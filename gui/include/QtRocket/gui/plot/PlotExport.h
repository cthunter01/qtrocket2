#pragma once

#include <filesystem>

#include <QCoreApplication>
#include <QString>
#include <QStringList>

#include <rocketplot/PlotWidget.h>

#include "QtRocket/gui/util/UserPrompts.h"

class QMenu;

namespace QtRocket
{
class Preferences;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class GuiServices;

/// QtRocket's export of a plot to a file, and what it changes of rocketplot's menus so that no
/// modal dialog of rocketplot's own opens: every question goes through UserPrompts (an offscreen
/// test would hang in a QFileDialog, a QColorDialog or a QMessageBox).
///
/// OpenRocket exports its charts in two ways: the ChartPanel's popup "Save as > PNG...", and the
/// buttons "Export Image" of its plot dialog (PlotDialog.doPNGExport()) and "Export plot..." of
/// the landing dispersion (LandingDispersionResultsDialog.exportPlot()). Those buttons ask a
/// SaveFileChooser for a PNG file in the default directory, force the extension ".png"
/// (FileHelper.forceExtension()), confirm an overwrite (FileHelper.confirmWrite()) and write the
/// image. rocketplot's context menu has "Export..." for an image, an SVG or PDF drawing or the
/// data as CSV, whose own dialog this replaces (install(), the user's decision U-M3-6b) with the
/// same steps as OpenRocket's buttons over rocketplot's kinds of file (exportPlot()). Milestone
/// 4's "Export Image" asks with its own request (the PNG filter only) through exportPlot().
///
/// rocketplot (v0.6.0) names none of its actions, so install() finds its "Export..." and its
/// legend entries' "Color..." by their text (rocketplot's English text, or its translation in
/// rocketplot's context when one is installed: QtRocket installs none), until rocketplot gives
/// them names or hooks (the upstream request N12).
class PlotExport
{
    Q_DECLARE_TR_FUNCTIONS(PlotExport)

public:
    PlotExport() = delete;

    /// The kinds of file a plot's export offers, as Qt name filters in the dialog's order:
    /// rocketplot's own Export... dialog's (PNG, JPEG, SVG, PDF, CSV), with OpenRocket's texts
    /// where it has one (FileHelper.PNG_FILTER "PNG image (*.png)", SVG_FILTER "SVG files
    /// (*.svg)", PDF_FILTER "PDF files (*.pdf)", CSV_FILTER "Comma Separated Files (*.csv)";
    /// rocketplot's "JPEG image (*.jpg)"). Each names one suffix that PlotWidget::exportTo()
    /// writes.
    [[nodiscard]] static QStringList nameFilters();

    /// The question of the context menu's Export...: titled "Export plot" (rocketplot's title),
    /// nameFilters(), starting in @p preferences' default directory (none: the user's home
    /// directory), no name proposed, refusing illegal names (OpenRocket's SaveFileChooser, as its
    /// plot exports ask), no storage option.
    [[nodiscard]] static UserPrompts::SaveFileRequest exportRequest(const Preferences& preferences);

    /// Exports @p plot as OpenRocket's plot exports do (see the class comment), through the
    /// prompts of @p services with @p plot as the dialogs' parent: asks @p request; gives the file
    /// chosen the suffix of the name filter chosen (withSuffix()); asks before replacing a file
    /// that exists (askOverwrite(), FileHelper.confirmWrite()); writes it with
    /// PlotWidget::exportTo() in the kind its suffix names, at the plot's size and in its theme;
    /// and shows "Error writing file" (error.writing.*, with the file) when that fails. The default
    /// directory is not stored (as OpenRocket's plot exports leave it). Nothing more once the plot
    /// has gone during a question (a modal dialog runs the event loop). Whether the file was
    /// written.
    static bool exportPlot(rocketplot::PlotWidget& plot, const GuiServices& services,
                           const UserPrompts::SaveFileRequest& request);

    /// exportPlot() with exportRequest() of @p services' preferences.
    static bool exportPlot(rocketplot::PlotWidget& plot, const GuiServices& services);

    /// Replaces rocketplot's own dialogs on @p plot for as long as it lives: its context menu's
    /// "Export..." becomes an action that runs exportPlot() (replaceExportAction(), when the
    /// menu is about to show), and the "Color..." of its legend's entries, for a series and for
    /// an annotation, is removed (removeColorActions()); the rest of rocketplot's menus stays
    /// (Back, Forward, Reset view, the crosshair, Copy image; the legend entries' visibility,
    /// line width, marker, Theme color and Remove). @p services must outlive @p plot (the
    /// application's do; a test's are declared before the plot). Call it once per plot.
    static void install(rocketplot::PlotWidget& plot, const GuiServices& services);

    /// In @p menu, a context menu of @p plot as rocketplot builds it: puts an action of the same
    /// text (objectName exportActionName()) that runs exportPlot(@p plot, @p services) in place
    /// of rocketplot's "Export..." (removed from the menu, which still owns it); without one in
    /// the menu, adds it at the end, with the text tr("Export...").
    static void replaceExportAction(QMenu& menu, rocketplot::PlotWidget& plot,
                                    const GuiServices& services);

    /// Removes rocketplot's "Color..." from @p menu (a legend entry's menu), which would open a
    /// QColorDialog.
    static void removeColorActions(QMenu& menu);

    /// The objectName of QtRocket's Export... action ("exportPlot").
    [[nodiscard]] static QString exportActionName();

    /// The suffixes a Qt name filter names, without their "*.": "PNG image (*.png)" is {"png"},
    /// "Images (*.png *.JPG)" {"png", "JPG"}; patterns that are no "*.<suffix>" ("*") are left
    /// out.
    [[nodiscard]] static QStringList suffixes(const QString& nameFilter);

    /// @p file, or @p file with "." and the first suffix of @p chosenFilter appended (the first of
    /// @p nameFilters' for an empty @p chosenFilter) when the name of @p file ends in none of the
    /// suffixes of @p nameFilters, ignoring case (FileHelper.forceExtension() over several kinds:
    /// a name that starts with its only dot has none, "plot.v2" gets ".png"). @p file as it is
    /// when no filter names a suffix.
    [[nodiscard]] static std::filesystem::path withSuffix(const std::filesystem::path& file,
                                                          const QStringList&           nameFilters,
                                                          const QString& chosenFilter);
};

}  // namespace QtRocket::Gui

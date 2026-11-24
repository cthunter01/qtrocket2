#include "QtRocket/gui/util/FileNames.h"

#include <filesystem>
#include <system_error>

#include <QChar>
#include <QString>
#include <QtTypes>

#include "QtRocket/gui/util/QtStrings.h"

namespace QtRocket::Gui
{

std::filesystem::path FileNames::absoluteFile(const std::filesystem::path& file)
{
    std::filesystem::path absolute = file;
    if (!file.is_absolute())
    {
        std::error_code             error;
        const std::filesystem::path current = std::filesystem::current_path(error);
        if (!error)
        {
            absolute = current / file;
        }
    }
    return absolute.make_preferred();
}

std::filesystem::path FileNames::forceExtension(const std::filesystem::path& file,
                                                const QString&               extension)
{
    // FileHelper.forceExtension(): the extension of the name is what follows its last dot,
    // unless that dot is its first character.
    const QString   name  = QtStrings::fromPath(file.filename());
    const qsizetype index = name.lastIndexOf(QChar(u'.'));
    const QString   given = index > 0 ? name.mid(index + 1) : QString();
    if (given.toLower() == extension.toLower())
    {
        return file;
    }
    QString absolute = QtStrings::fromPath(absoluteFile(file));
    if (!extension.startsWith(QChar(u'.')))
    {
        absolute += QChar(u'.');
    }
    return QtStrings::toPath(absolute + extension);
}

QString FileNames::csvFilter()
{
    //: FileHelper.CSV_FILTER
    return tr("Comma Separated Files (*.csv)");
}

QString FileNames::allFilesFilter()
{
    // Swing's FileChooser.acceptAllFileFilterText, with Qt's pattern.
    return tr("All Files") + QStringLiteral(" (*)");
}

}  // namespace QtRocket::Gui

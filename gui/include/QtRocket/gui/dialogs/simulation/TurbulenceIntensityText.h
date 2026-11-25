#pragma once

#include <string_view>

#include <QCoreApplication>
#include <QString>

namespace QtRocket
{
class PinkNoiseWindModel;
}  // namespace QtRocket

namespace QtRocket::Gui
{

/// The words that describe a turbulence intensity (OpenRocket: PinkNoiseWindModel's and
/// LevelWindModel's getIntensityDescription(), trans.get() of a key simedtdlg.IntensityDesc.*):
/// "None", "Very low", "Low", "Medium", "High", "Very high" or "Extreme". The core gives the key
/// (PinkNoiseWindModel::getIntensityDescriptionKey(), whose thresholds are Java's); this gives its
/// text, which the launch conditions show beside the average wind's turbulence intensity
/// (SimulationConditionsPanel) and the wind editor's table in its Intensity column.
class TurbulenceIntensityText final
{
    Q_DECLARE_TR_FUNCTIONS(TurbulenceIntensityText)

public:
    TurbulenceIntensityText() = delete;

    /// The text of @p key, one of the seven keys simedtdlg.IntensityDesc.None, .Verylow, .Low,
    /// .Medium, .High, .Veryhigh and .Extreme.
    /// @throws BugError for any other key
    [[nodiscard]] static QString text(std::string_view key);

    /// The text that describes @p model's turbulence intensity (Java:
    /// model.getIntensityDescription()).
    [[nodiscard]] static QString of(const PinkNoiseWindModel& model);
};

}  // namespace QtRocket::Gui

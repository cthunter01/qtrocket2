#include "QtRocket/gui/dialogs/simulation/TurbulenceIntensityText.h"

#include <string>
#include <string_view>

#include <QString>

#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

QString TurbulenceIntensityText::text(std::string_view key)
{
    if (key == "simedtdlg.IntensityDesc.None")
    {
        //: simedtdlg.IntensityDesc.None
        return tr("None");
    }
    if (key == "simedtdlg.IntensityDesc.Verylow")
    {
        //: simedtdlg.IntensityDesc.Verylow
        return tr("Very low");
    }
    if (key == "simedtdlg.IntensityDesc.Low")
    {
        //: simedtdlg.IntensityDesc.Low
        return tr("Low");
    }
    if (key == "simedtdlg.IntensityDesc.Medium")
    {
        //: simedtdlg.IntensityDesc.Medium
        return tr("Medium");
    }
    if (key == "simedtdlg.IntensityDesc.High")
    {
        //: simedtdlg.IntensityDesc.High
        return tr("High");
    }
    if (key == "simedtdlg.IntensityDesc.Veryhigh")
    {
        //: simedtdlg.IntensityDesc.Veryhigh
        return tr("Very high");
    }
    if (key == "simedtdlg.IntensityDesc.Extreme")
    {
        //: simedtdlg.IntensityDesc.Extreme
        return tr("Extreme");
    }
    bug("No turbulence intensity is described by the key " + std::string(key));
}

QString TurbulenceIntensityText::of(const PinkNoiseWindModel& model)
{
    return text(model.getIntensityDescriptionKey());
}

}  // namespace QtRocket::Gui

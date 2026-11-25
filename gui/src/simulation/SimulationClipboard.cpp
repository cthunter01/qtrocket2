#include "QtRocket/gui/simulation/SimulationClipboard.h"

#include <QObject>

namespace QtRocket::Gui
{

SimulationClipboard::SimulationClipboard(QObject* parent) : QObject(parent) { }

SimulationClipboard::~SimulationClipboard() = default;

}  // namespace QtRocket::Gui

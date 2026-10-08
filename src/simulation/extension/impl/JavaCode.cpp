#include "QtRocket/simulation/extension/impl/JavaCode.h"

#include <memory>
#include <string>
#include <string_view>

#include "QtRocket/simulation/exception/SimulationException.h"
#include "QtRocket/simulation/extension/AbstractSimulationExtension.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

JavaCode::JavaCode() : AbstractSimulationExtension(std::string(kId)) { }

bool JavaCode::isMonteCarloSafe() const
{
    return Strings::isEmpty(getClassName());
}

void JavaCode::initialize(SimulationConditions& /*conditions*/)
{
    const std::string className = getClassName();
    if (!Strings::isEmpty(className))
    {
        // HOOK(scripting): Java loads the class (Class.forName()), makes an instance and adds it
        // to the conditions as a listener. No class can be found here: Java's
        // ClassNotFoundException path.
        throw SimulationException("Could not find class " + className);
    }
}

std::string JavaCode::getName() const
{
    std::string       name      = "Java code: ";
    const std::string className = getClassName();
    if (!Strings::isEmpty(className))
    {
        name += className;
    }
    else
    {
        name += "none";
    }
    return name;
}

std::unique_ptr<SimulationExtension> JavaCode::clone() const
{
    return std::make_unique<JavaCode>(*this);
}

std::string JavaCode::getClassName() const
{
    return m_config.getString("className", "");
}

void JavaCode::setClassName(std::string_view className)
{
    m_config.put("className", className);
    fireChangeEvent();
}

}  // namespace QtRocket

#include "QtRocket/util/Rotation2D.h"

#include <cmath>

namespace QtRocket
{

Rotation2D::Rotation2D(double angle) : Rotation2D(std::sin(angle), std::cos(angle)) { }

}  // namespace QtRocket

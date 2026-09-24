#pragma once

#include "QtRocket/logging/ErrorMessage.h"
#include "QtRocket/logging/MessageSet.h"

namespace QtRocket
{

/// A MessageSet of errors (OpenRocket: logging/ErrorSet). add(std::string_view) builds an
/// ErrorMessage::Other; OpenRocket adds nothing else to the base set.
using ErrorSet = MessageSet<ErrorMessage>;

}  // namespace QtRocket

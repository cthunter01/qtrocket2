#pragma once

// The motors the 16 example designs refer to, and what OpenRocket makes of each reference with
// the bundled motor database: the table the tests of the motor finder, of the motor handler and
// of the example designs share. Test-only.

#include <array>
#include <cstddef>
#include <string_view>

#include "QtRocket/motor/Motor.h"

namespace QtRocket::Test
{

/// How the motor found relates to the digest of the file.
enum class ExampleMotorMatch
{
    EXACT,        ///< the motor's digest is the file's
    COMPATIBLE,   ///< the file's digest is one of an older format of the motor's curve
    DESCRIPTION,  ///< the digest does not fit: the motor is taken by its description, silently
};

/// One <motor> reference of an example design, and what OpenRocket makes of it.
struct ExampleMotor
{
    std::string_view  file;
    Motor::Type       type;
    std::string_view  manufacturer;
    std::string_view  designation;
    std::string_view  digest;  ///< the file's
    std::string_view  found;   ///< the digest of the motor OpenRocket loads
    std::size_t       candidates;
    ExampleMotorMatch match;
};

/// Every distinct motor reference of the 16 example designs, by file (41; the files hold 65
/// <motor> elements): expected-motors.tsv of the tier 9 scouts, with the kind of match of
/// motor-queries.tsv.
// clang-format off
inline constexpr auto kExampleMotors = std::to_array<ExampleMotor>({
    {.file = "3D printable nose cone and fins.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "A8", .digest = "22aec01287ea1e3b8c6f66b26fe5fea6", .found = "c049499ca03fd69115352e5d4be72de7", .candidates = 2, .match = ExampleMotorMatch::COMPATIBLE},
    {.file = "3D printable nose cone and fins.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "B6", .digest = "524787762ca4da7db6dbfb7e43b4cd09", .found = "74472299ac7ffc451f7b434d6c81b89c", .candidates = 1, .match = ExampleMotorMatch::DESCRIPTION},
    {.file = "3D printable nose cone and fins.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "C6", .digest = "c8743ef3fa99e14a89885cbe0ead47b2", .found = "2967cd7a160b396ef96f09695429d8e9", .candidates = 2, .match = ExampleMotorMatch::COMPATIBLE},
    {.file = "A simple model rocket.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "A8", .digest = "22aec01287ea1e3b8c6f66b26fe5fea6", .found = "c049499ca03fd69115352e5d4be72de7", .candidates = 2, .match = ExampleMotorMatch::COMPATIBLE},
    {.file = "A simple model rocket.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "B4", .digest = "c15b9b96bf06e0ab896394787da3c47e", .found = "be11a726b56813c4b1aea0574c8302b2", .candidates = 2, .match = ExampleMotorMatch::COMPATIBLE},
    {.file = "A simple model rocket.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "C6", .digest = "c8743ef3fa99e14a89885cbe0ead47b2", .found = "2967cd7a160b396ef96f09695429d8e9", .candidates = 2, .match = ExampleMotorMatch::COMPATIBLE},
    {.file = "ARC payload rocket.ork", .type = Motor::Type::SINGLE, .manufacturer = "AeroTech", .designation = "F50T", .digest = "019164167fce99d0709c22dc0e410ad7", .found = "88ed07b96422ec99767fb35bf6a51966", .candidates = 2, .match = ExampleMotorMatch::COMPATIBLE},
    {.file = "Airstart timing.ork", .type = Motor::Type::RELOAD, .manufacturer = "AeroTech", .designation = "K550W", .digest = "822e1d4ed7924f9dea7a62b5f5ff2823", .found = "9e8c531f225566c6c75eedaeacfcfef1", .candidates = 1, .match = ExampleMotorMatch::COMPATIBLE},
    {.file = "Airstart timing.ork", .type = Motor::Type::RELOAD, .manufacturer = "AeroTech", .designation = "I211W", .digest = "6f055ac96725164ac5814d96f109b8d5", .found = "8a203416fee4400cdaf09edf369235af", .candidates = 1, .match = ExampleMotorMatch::COMPATIBLE},
    {.file = "Chute release.ork", .type = Motor::Type::SINGLE, .manufacturer = "AeroTech", .designation = "G40W", .digest = "96f84e3d549cb6ae6e52df6732fe1bfc", .found = "7c6080928783078289d9a473efecc134", .candidates = 1, .match = ExampleMotorMatch::COMPATIBLE},
    {.file = "Chute release.ork", .type = Motor::Type::SINGLE, .manufacturer = "AeroTech", .designation = "G80T", .digest = "bc77ef06b0e4f9471985fe24e1d7fbd6", .found = "6438359c302fd7c031af855f17d8d27e", .candidates = 8, .match = ExampleMotorMatch::DESCRIPTION},
    {.file = "Clustered motors.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "A8", .digest = "22aec01287ea1e3b8c6f66b26fe5fea6", .found = "c049499ca03fd69115352e5d4be72de7", .candidates = 2, .match = ExampleMotorMatch::COMPATIBLE},
    {.file = "Clustered motors.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "B4", .digest = "c15b9b96bf06e0ab896394787da3c47e", .found = "be11a726b56813c4b1aea0574c8302b2", .candidates = 2, .match = ExampleMotorMatch::COMPATIBLE},
    {.file = "Clustered motors.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "C6", .digest = "c8743ef3fa99e14a89885cbe0ead47b2", .found = "2967cd7a160b396ef96f09695429d8e9", .candidates = 2, .match = ExampleMotorMatch::COMPATIBLE},
    {.file = "Deployable payload.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "A8", .digest = "22aec01287ea1e3b8c6f66b26fe5fea6", .found = "c049499ca03fd69115352e5d4be72de7", .candidates = 2, .match = ExampleMotorMatch::COMPATIBLE},
    {.file = "Deployable payload.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "B4", .digest = "c15b9b96bf06e0ab896394787da3c47e", .found = "be11a726b56813c4b1aea0574c8302b2", .candidates = 2, .match = ExampleMotorMatch::COMPATIBLE},
    {.file = "Deployable payload.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "C6", .digest = "c8743ef3fa99e14a89885cbe0ead47b2", .found = "2967cd7a160b396ef96f09695429d8e9", .candidates = 2, .match = ExampleMotorMatch::COMPATIBLE},
    {.file = "Dual parachute deployment.ork", .type = Motor::Type::RELOAD, .manufacturer = "AeroTech", .designation = "H669N", .digest = "dfeac53db6e18bc3583856fb9f06272e", .found = "26a5e7834018943090396d419ca64662", .candidates = 1, .match = ExampleMotorMatch::COMPATIBLE},
    {.file = "Dual parachute deployment.ork", .type = Motor::Type::RELOAD, .manufacturer = "AeroTech", .designation = "H242T", .digest = "e49249080ef9c1671ec2a86418848af9", .found = "cff44a78b2c4bc9e2ef19f6a2ded4da0", .candidates = 2, .match = ExampleMotorMatch::DESCRIPTION},
    {.file = "Dual parachute deployment.ork", .type = Motor::Type::RELOAD, .manufacturer = "AeroTech", .designation = "J570W", .digest = "14373c65485521035497b1db20e110d3", .found = "6eb7fe0c078fe0365e2052d8c877f6a4", .candidates = 2, .match = ExampleMotorMatch::COMPATIBLE},
    {.file = "Dual parachute deployment.ork", .type = Motor::Type::RELOAD, .manufacturer = "AeroTech", .designation = "H999N", .digest = "e8c8cdd09c63c4fc3d6790252de6049a", .found = "871f7fe309f61ec7e45e4b29833349d9", .candidates = 2, .match = ExampleMotorMatch::COMPATIBLE},
    {.file = "Dual parachute deployment.ork", .type = Motor::Type::RELOAD, .manufacturer = "AeroTech", .designation = "I1299N", .digest = "6b4aebccc355196a4e289ce84d517b0e", .found = "e26c7159754a0c58db92d8c5b3568902", .candidates = 1, .match = ExampleMotorMatch::COMPATIBLE},
    {.file = "Dual parachute deployment.ork", .type = Motor::Type::RELOAD, .manufacturer = "AeroTech", .designation = "G64W", .digest = "d72e5f01400ce95e40c3f4f4990a67f8", .found = "0fa1ede88d6407eadc5bbc413810100d", .candidates = 1, .match = ExampleMotorMatch::COMPATIBLE},
    {.file = "Parallel booster staging.ork", .type = Motor::Type::RELOAD, .manufacturer = "AeroTech", .designation = "I115W", .digest = "18b10160746afa88aa571c950221cb06", .found = "0039ed088e61360d934d9bd8503fad92", .candidates = 1, .match = ExampleMotorMatch::COMPATIBLE},
    {.file = "Parallel booster staging.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "E12", .digest = "27575268e95ac6983801956efb389764", .found = "7baff5a049a60829dbb8316d1b183e86", .candidates = 1, .match = ExampleMotorMatch::COMPATIBLE},
    {.file = "Pods--airframes and winglets.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "A8", .digest = "f3a785e1523935caf239c7366cf81fee", .found = "bd060845629e4cfceec7e9b19297ab9f", .candidates = 2, .match = ExampleMotorMatch::DESCRIPTION},
    {.file = "Pods--airframes and winglets.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "B6", .digest = "524787762ca4da7db6dbfb7e43b4cd09", .found = "74472299ac7ffc451f7b434d6c81b89c", .candidates = 1, .match = ExampleMotorMatch::DESCRIPTION},
    {.file = "Pods--airframes and winglets.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "C6", .digest = "c8743ef3fa99e14a89885cbe0ead47b2", .found = "2967cd7a160b396ef96f09695429d8e9", .candidates = 2, .match = ExampleMotorMatch::COMPATIBLE},
    {.file = "Pods--airframes and winglets.ork", .type = Motor::Type::SINGLE, .manufacturer = "Quest", .designation = "C12", .digest = "14b080b33be165c3309254a604bcecba", .found = "a47e144089f502e95b8a16130db9b416", .candidates = 1, .match = ExampleMotorMatch::COMPATIBLE},
    {.file = "Pods--airframes and winglets.ork", .type = Motor::Type::SINGLE, .manufacturer = "Quest", .designation = "D16", .digest = "99efdce542a60845c9b82c846fb59d4c", .found = "597863e31e02f245b23d2bfd550f615e", .candidates = 1, .match = ExampleMotorMatch::COMPATIBLE},
    {.file = "Pods--powered with recovery deployment.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "A10", .digest = "e5b53def203dd437ebf0d67846f6cd3b", .found = "e5b53def203dd437ebf0d67846f6cd3b", .candidates = 1, .match = ExampleMotorMatch::EXACT},
    {.file = "Pods--powered with recovery deployment.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "A8", .digest = "bd060845629e4cfceec7e9b19297ab9f", .found = "bd060845629e4cfceec7e9b19297ab9f", .candidates = 1, .match = ExampleMotorMatch::EXACT},
    {.file = "Simulation extensions.ork", .type = Motor::Type::HYBRID, .manufacturer = "HyperTEK", .designation = "2800CC172L-L540", .digest = "08bbc968cab39e437cf3721ae4fac3f1", .found = "5498ead583ab6cd6900a533b1cb69df8", .candidates = 1, .match = ExampleMotorMatch::DESCRIPTION},
    {.file = "Simulation scripting.ork", .type = Motor::Type::HYBRID, .manufacturer = "HyperTEK", .designation = "2800CC172L-L540", .digest = "08bbc968cab39e437cf3721ae4fac3f1", .found = "5498ead583ab6cd6900a533b1cb69df8", .candidates = 1, .match = ExampleMotorMatch::DESCRIPTION},
    {.file = "Three stage low power rocket.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "A8", .digest = "22aec01287ea1e3b8c6f66b26fe5fea6", .found = "c049499ca03fd69115352e5d4be72de7", .candidates = 2, .match = ExampleMotorMatch::COMPATIBLE},
    {.file = "Three stage low power rocket.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "C6", .digest = "c8743ef3fa99e14a89885cbe0ead47b2", .found = "2967cd7a160b396ef96f09695429d8e9", .candidates = 2, .match = ExampleMotorMatch::COMPATIBLE},
    {.file = "Three stage low power rocket.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "B6", .digest = "ebc2af7f8c6f8665d1bec720ddc2b5e1", .found = "74472299ac7ffc451f7b434d6c81b89c", .candidates = 1, .match = ExampleMotorMatch::COMPATIBLE},
    {.file = "Tube fin rocket.ork", .type = Motor::Type::SINGLE, .manufacturer = "Estes", .designation = "D12", .digest = "ec683e131a2b32950561abfe011b0afc", .found = "c26987c1c7e95810bbb6f2e284861494", .candidates = 1, .match = ExampleMotorMatch::COMPATIBLE},
    {.file = "Two stage high power rocket.ork", .type = Motor::Type::RELOAD, .manufacturer = "AeroTech", .designation = "H148R", .digest = "a06234b6049c1079e394cb9ecd4607a9", .found = "a06234b6049c1079e394cb9ecd4607a9", .candidates = 1, .match = ExampleMotorMatch::EXACT},
    {.file = "Two stage high power rocket.ork", .type = Motor::Type::RELOAD, .manufacturer = "AeroTech", .designation = "I59WN", .digest = "a0006978c9a542518b425c0caa67042b", .found = "a0006978c9a542518b425c0caa67042b", .candidates = 1, .match = ExampleMotorMatch::EXACT},
    {.file = "Two stage high power rocket.ork", .type = Motor::Type::RELOAD, .manufacturer = "AeroTech", .designation = "I357T", .digest = "1a3327383625336706131b2a9d198139", .found = "1a3327383625336706131b2a9d198139", .candidates = 1, .match = ExampleMotorMatch::EXACT},
});
// clang-format on

}  // namespace QtRocket::Test

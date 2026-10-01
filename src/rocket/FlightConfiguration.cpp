#include "QtRocket/rocket/FlightConfiguration.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <deque>
#include <format>
#include <iterator>
#include <limits>
#include <map>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BoxBounded.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InstanceContext.h"
#include "QtRocket/rocket/InstanceMap.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/ReferenceType.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Chars.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Strings.h"
#include "QtRocket/util/Transformation.h"

namespace QtRocket
{

namespace
{

/// Java's static configurationInstanceCount: the next configuration's instance id.
[[nodiscard]] std::atomic<int>& instanceCounter() noexcept
{
    static std::atomic<int> s_count{0};
    return s_count;
}

// ================================================================ the name substitution
//
// OpenRocket's MotorConfigurationSubstitutor (the only RocketSubstitutor), which
// RocketDescriptorImpl.format() applies to a configuration's raw name.

/// The substitution words (SUBSTITUTIONS' keys).
enum class SubstitutionKey
{
    MOTORS,
    MANUFACTURERS,
    CASES,
};

struct KeyWord
{
    SubstitutionKey     key;
    std::string_view    word;
    std::u32string_view codePoints;  ///< the word as code points
};

constexpr std::array<KeyWord, 3> kKeyWords{{
    {.key = SubstitutionKey::MOTORS, .word = "motors", .codePoints = U"motors"},
    {.key        = SubstitutionKey::MANUFACTURERS,
     .word       = "manufacturers",
     .codePoints = U"manufacturers"},
    {.key = SubstitutionKey::CASES, .word = "cases", .codePoints = U"cases"},
}};

/// The most rounds of substitution getName() runs (Java: unbounded).
constexpr int kMaxSubstitutionRounds = 100;

/// Java regex \s: [ \t\n\x0B\f\r].
[[nodiscard]] constexpr bool isJavaWhitespace(char32_t c) noexcept
{
    return c == U' ' || c == U'\t' || c == U'\n' || c == U'\x0B' || c == U'\f' || c == U'\r';
}

/// Whether @p c is one of the line terminators Java's '.' does not match: \n, \r, U+0085, U+2028,
/// U+2029.
[[nodiscard]] constexpr bool isLineTerminator(char32_t c) noexcept
{
    return c == U'\n' || c == U'\r' || c == U'\u0085' || c == U'\u2028' || c == U'\u2029';
}

/// A tag: the positions of its '{' and of its '}'.
struct Tag
{
    std::size_t open;
    std::size_t close;

    /// The text between the braces.
    [[nodiscard]] std::string_view content(std::string_view s) const
    {
        return s.substr(open + 1, close - open - 1);
    }
};

/// The next match of Java's \{(.*?)\} at or after @p from: a '{' and the first '}' after it with
/// no line terminator in between. The text is read as Java's decoder would have read it: a byte
/// that does not start a well-formed UTF-8 sequence is one U+FFFD (Strings::nextCodePoint()), so a
/// '}' right after a truncated sequence still closes the tag.
[[nodiscard]] std::optional<Tag> findTag(std::string_view s, std::size_t from)
{
    for (std::size_t open = from; open < s.size(); ++open)
    {
        if (s[open] != '{')
        {
            continue;
        }
        std::size_t pos = open + 1;
        while (pos < s.size())
        {
            if (s[pos] == '}')
            {
                return Tag{.open = open, .close = pos};
            }
            if (isLineTerminator(Strings::nextCodePoint(s, pos)))
            {
                break;
            }
        }
    }
    return std::nullopt;
}

/// The next match of Java's \{(.*?[^\s])\} at or after @p from: a '{' and the first '}' after it
/// that follows a non-whitespace character, with no line terminator before that character.
[[nodiscard]] std::optional<Tag> findNonBlankTag(std::string_view s, std::size_t from)
{
    for (std::size_t open = from; open < s.size(); ++open)
    {
        if (s[open] != '{')
        {
            continue;
        }
        std::size_t pos = open + 1;
        while (pos < s.size())
        {
            std::size_t    next      = pos;
            const char32_t codePoint = Strings::nextCodePoint(s, next);
            if (!isJavaWhitespace(codePoint) && next < s.size() && s[next] == '}')
            {
                return Tag{.open = open, .close = next};
            }
            // The character joins the lazy '.*?' part, which excludes line terminators.
            if (isLineTerminator(codePoint))
            {
                break;
            }
            pos = next;
        }
    }
    return std::nullopt;
}

/// containsSubstitution(): whether some non-blank tag's trimmed content contains a key word.
[[nodiscard]] bool containsSubstitution(std::string_view input)
{
    std::size_t from = 0;
    while (const std::optional<Tag> tag = findNonBlankTag(input, from))
    {
        const std::string_view tagContent = Strings::trim(tag->content(input));
        for (const KeyWord& keyWord : kKeyWords)
        {
            if (tagContent.contains(keyWord.word))
            {
                return true;
            }
        }
        from = tag->close + 1;
    }
    return false;
}

/// The key words of @p tagContent matched as \b(motors|manufacturers|cases)\b, in order, with
/// Java's \b (Strings::javaRegexWordBoundary()) over the content's code points.
[[nodiscard]] std::vector<const KeyWord*> findKeyWords(std::string_view tagContent)
{
    const std::u32string        text = Strings::toCodePoints(tagContent);
    std::vector<const KeyWord*> found;
    std::size_t                 pos = 0;
    while (pos < text.size())
    {
        const KeyWord* matched = nullptr;
        if (Strings::javaRegexWordBoundary(text, pos))
        {
            for (const KeyWord& keyWord : kKeyWords)
            {
                const std::size_t end = pos + keyWord.codePoints.size();
                if (std::u32string_view{text}.substr(pos).starts_with(keyWord.codePoints) &&
                    Strings::javaRegexWordBoundary(text, end))
                {
                    matched = &keyWord;
                    break;
                }
            }
        }
        if (matched != nullptr)
        {
            found.push_back(matched);
            pos += matched->codePoints.size();
        }
        else
        {
            ++pos;
        }
    }
    return found;
}

/// The data one key word gives for one motor (BaseSubstitutor.getData()).
[[nodiscard]] std::string substitutionData(SubstitutionKey key, const Motor& motor,
                                           const MotorConfiguration& motorConfig,
                                           const Preferences&        preferences)
{
    const auto* thrustCurveMotor = dynamic_cast<const ThrustCurveMotor*>(&motor);
    switch (key)
    {
        case SubstitutionKey::MOTORS:
            return motor.getMotorName(preferences, motorConfig.getEjectionDelay());
        case SubstitutionKey::MANUFACTURERS:
            return thrustCurveMotor != nullptr
                       ? thrustCurveMotor->getManufacturer().getDisplayName()
                       : std::string{};
        case SubstitutionKey::CASES:
            return thrustCurveMotor != nullptr ? thrustCurveMotor->getCaseInfo() : std::string{};
    }
    return {};
}

/// The data of every stage for one key word (BaseSubstitutor.substitute()): nullopt for an
/// inactive stage, else one string per motor of the stage's own acting mounts ("" for a mount
/// without a motor in the configuration).
using StageSubstitutes = std::map<const AxialStage*, std::optional<std::vector<std::string>>>;

[[nodiscard]] StageSubstitutes substituteKey(SubstitutionKey key, const Rocket& rocket,
                                             const FlightConfigurationId& fcid,
                                             const Preferences&           preferences)
{
    StageSubstitutes           stageMap;
    const FlightConfiguration& config = rocket.getFlightConfiguration(fcid);

    for (const AxialStage* stage : rocket.getStageList())
    {
        if (!config.isStageActive(stage->getStageNumber()))
        {
            stageMap[stage] = std::nullopt;
            continue;
        }
        std::vector<std::string>& dataList = stageMap[stage].emplace();

        for (const RocketComponent* child : stage->getAllChildren())
        {
            // Only the stage's own mounts, not those of a booster set inside it.
            const auto* mount = dynamic_cast<const MotorMount*>(child);
            if (&child->getStage() != stage || mount == nullptr || !mount->isMotorMount())
            {
                continue;
            }
            const MotorConfiguration& inst  = mount->getMotorConfig(fcid);
            const Motor*              motor = inst.getMotor().get();
            if (motor == nullptr)
            {
                dataList.emplace_back();
                continue;
            }
            const std::string data = substitutionData(key, *motor, inst, preferences);
            for (int i = 0; i < mount->getMotorCountIncludingAssemblyCopies(); i++)
            {
                dataList.push_back(data);
            }
        }
    }
    return stageMap;
}

/// One stage's text for one key word (getFinalSubstitute()): the only substitute, the first one
/// when the stage's text already has something, else the distinct non-empty substitutes in Java
/// string order, each prefixed with "<count>× " when it occurs more than once, joined by ", ".
[[nodiscard]] std::string finalSubstitute(const std::vector<std::string>& substitutes,
                                          const std::string&              existingSubstitution)
{
    if (substitutes.empty())
    {
        return {};
    }
    if (substitutes.size() == 1 || !existingSubstitution.empty())
    {
        return substitutes.front();
    }

    const auto javaLess = [](const std::string& a, const std::string& b) {
        return Strings::javaCompareTo(a, b) < 0;
    };
    std::map<std::string, int, decltype(javaLess)> motorCounts(javaLess);
    for (const std::string& motorData : substitutes)
    {
        if (!motorData.empty())
        {
            ++motorCounts[motorData];
        }
    }

    std::string result;
    for (const auto& [motorData, count] : motorCounts)
    {
        if (!result.empty())
        {
            result += ", ";
        }
        if (count > 1)
        {
            result += std::to_string(count) + std::string{Chars::kTimes} + " " + motorData;
        }
        else
        {
            result += motorData;
        }
    }
    return result;
}

/// The texts of every stage for the key words of one tag (combineSubstitutesForStages()).
[[nodiscard]] std::vector<std::string> combineSubstitutesForStages(
    const Rocket& rocket, const FlightConfiguration& config,
    const std::vector<StageSubstitutes>& stageSubstitutes,
    const std::vector<std::string_view>& separators)
{
    std::vector<std::string> combinations;
    for (const AxialStage* stage : rocket.getStageList())
    {
        if (!config.isStageActive(stage->getStageNumber()))
        {
            combinations.emplace_back();
            continue;
        }

        std::string stageSub;
        std::size_t idx = 0;
        for (const StageSubstitutes& substituteMap : stageSubstitutes)
        {
            const auto it = substituteMap.find(stage);
            if (it == substituteMap.end())
            {
                continue;
            }
            const std::optional<std::vector<std::string>>& substitutes = it->second;
            if (!substitutes.has_value() || substitutes->empty())
            {
                continue;
            }
            // The separator written before this key word in the tag.
            if (!stageSub.empty() && idx > 0)
            {
                QTROCKET_ASSERT(idx - 1 < separators.size());
                stageSub += separators[idx - 1];
            }
            stageSub += finalSubstitute(*substitutes, stageSub);
            idx++;
        }

        if (stageSub.empty())
        {
            stageSub = FlightConfiguration::kNoStageMotors;
        }
        combinations.push_back(std::move(stageSub));
    }

    const bool onlyEmpty = std::ranges::all_of(combinations, [](const std::string& s) {
        return s.empty() || s == FlightConfiguration::kNoStageMotors;
    });
    if (combinations.empty() || onlyEmpty)
    {
        return {std::string{FlightConfiguration::kNoMotors}};
    }
    return combinations;
}

/// substitute(): every tag of @p input replaced by its stages' texts joined by "; ".
[[nodiscard]] std::string substitute(std::string_view input, const Rocket& rocket,
                                     const FlightConfigurationId& configId,
                                     const Preferences&           preferences)
{
    std::string result;
    std::size_t last = 0;
    while (const std::optional<Tag> tag = findTag(input, last))
    {
        result.append(input.substr(last, tag->open - last));
        const std::string_view tagContent = Strings::trim(tag->content(input));

        // Step 1: the key words.
        const std::vector<const KeyWord*> foundKeys = findKeyWords(tagContent);

        // Step 2: the separators between them, located with indexOf() as Java does.
        std::vector<std::string_view> separators;
        std::size_t                   lastEnd = 0;
        for (std::size_t i = 0; i + 1 < foundKeys.size(); i++)
        {
            const std::size_t startOfThisKey = tagContent.find(foundKeys[i]->word, lastEnd);
            QTROCKET_ASSERT(startOfThisKey != std::string_view::npos);
            const std::size_t endOfThisKey = startOfThisKey + foundKeys[i]->word.size();
            const std::size_t startOfNextKey =
                tagContent.find(foundKeys[i + 1]->word, endOfThisKey);
            QTROCKET_ASSERT(startOfNextKey != std::string_view::npos);
            separators.push_back(tagContent.substr(endOfThisKey, startOfNextKey - endOfThisKey));
            lastEnd = startOfNextKey;
        }

        std::vector<StageSubstitutes> stageSubstitutes;
        stageSubstitutes.reserve(foundKeys.size());
        for (const KeyWord* keyWord : foundKeys)
        {
            stageSubstitutes.push_back(substituteKey(keyWord->key, rocket, configId, preferences));
        }

        const FlightConfiguration&     config = rocket.getFlightConfiguration(configId);
        const std::vector<std::string> combinations =
            combineSubstitutesForStages(rocket, config, stageSubstitutes, separators);
        for (std::size_t i = 0; i < combinations.size(); i++)
        {
            if (i > 0)
            {
                result += "; ";
            }
            result += combinations[i];
        }
        last = tag->close + 1;
    }
    result.append(input.substr(last));
    return result;
}

/// RocketDescriptorImpl.format(name, rocket, configId).
[[nodiscard]] std::string formatName(std::string name, const Rocket& rocket,
                                     const FlightConfigurationId& configId,
                                     const Preferences&           preferences)
{
    for (int round = 0; round < kMaxSubstitutionRounds && containsSubstitution(name); round++)
    {
        std::string next = substitute(name, rocket, configId, preferences);
        if (next == name)
        {
            break;  // Java would loop for ever here
        }
        name = std::move(next);
    }
    return name;
}

}  // namespace

// ================================================================================ basics

FlightConfiguration::FlightConfiguration(Rocket& rocket)
  : FlightConfiguration(rocket, FlightConfigurationId::defaultValueId())
{
}

FlightConfiguration::FlightConfiguration(Rocket& rocket, const FlightConfigurationId& fcid)
  : m_rocket(&rocket),
    m_fcid(fcid),
    m_configurationInstanceId(instanceCounter().fetch_add(1, std::memory_order_relaxed))
{
    updateStages();
    updateMotors();
    updateActiveInstances();
}

// ============================================================================ stage flags

void FlightConfiguration::clearAllStages()
{
    setAllStagesActive(false);
}

void FlightConfiguration::setAllStages()
{
    setAllStagesActive(true);
}

void FlightConfiguration::setAllStagesActive(bool active)
{
    for (auto& entry : m_stages)
    {
        entry.second.active = active;
    }
    updateMotors();
    updateActiveInstances();
}

void FlightConfiguration::copyStages(const FlightConfiguration& other)
{
    // A copy of other's flags first: other may be this configuration.
    const std::map<int, StageFlags> otherStages = other.m_stages;
    for (const auto& entry : otherStages)
    {
        const StageFlags& cur     = entry.second;
        m_stages[cur.stageNumber] = StageFlags{
            .active = cur.active, .stageNumber = cur.stageNumber, .stageId = cur.stageId};
    }
    updateMotors();
    updateActiveInstances();
}

void FlightConfiguration::copyStageActiveness(const FlightConfiguration& other)
{
    for (auto& entry : m_stages)
    {
        StageFlags& flags = entry.second;
        const auto  it    = other.m_stages.find(flags.stageNumber);
        if (it != other.m_stages.end())
        {
            flags.active = it->second.active;
        }
    }
    updateMotors();
    updateActiveInstances();
}

void FlightConfiguration::clearStage(int stageNumber)
{
    setStageActive(stageNumber, false);
}

void FlightConfiguration::clearStagesBelow(int stageNumber)
{
    // Not setStageActive(stageNumber, false, true), which would also clear the booster sets'
    // flags (Java's comment asks whether it should).
    const auto stageCount = static_cast<int>(m_rocket->getStageCount());
    for (int i = stageNumber; i < stageCount; i++)
    {
        setStageActive(i, false, false);
    }
}

void FlightConfiguration::clearStagesAbove(int stageNumber)
{
    for (int i = 0; i < stageNumber; i++)
    {
        setStageActive(i, false, false);
    }
}

void FlightConfiguration::activateStagesThrough(const AxialStage& stage)
{
    clearAllStages();
    for (int i = 0; i <= stage.getStageNumber(); i++)
    {
        setStageActive(i, true);
    }
}

void FlightConfiguration::setOnlyStage(int stageNumber)
{
    setAllStagesActive(false);
    setStageActive(stageNumber, true, false);
}

void FlightConfiguration::setStageActive(int stageNumber, bool active, bool activateSubStages)
{
    const auto it = m_stages.find(stageNumber);
    if (stageNumber < 0 || it == m_stages.end())
    {
        // Java logs "error: attempt to retrieve via a bad stage number".
        return;
    }
    it->second.active = active;
    if (activateSubStages)
    {
        // The sub-stages follow (Java: NullPointerException for one without a flag).
        if (const AxialStage* stage = m_rocket->getStage(stageNumber))
        {
            for (const AxialStage* subStage : stage->getSubStages())
            {
                const auto sub = m_stages.find(subStage->getStageNumber());
                if (sub != m_stages.end())
                {
                    sub->second.active = active;
                }
            }
        }
    }
    fireChangeEvent();
}

void FlightConfiguration::setStageActive(int stageNumber, bool active)
{
    setStageActive(stageNumber, active, true);
}

void FlightConfiguration::toggleStage(int stageNumber)
{
    const auto it = m_stages.find(stageNumber);
    if (stageNumber < 0 || it == m_stages.end())
    {
        // Java logs "error: attempt to retrieve via a bad stage number".
        return;
    }
    StageFlags& flags = it->second;
    flags.active      = !flags.active;
    if (const AxialStage* stage = m_rocket->getStage(stageNumber))
    {
        for (const AxialStage* subStage : stage->getSubStages())
        {
            const auto sub = m_stages.find(subStage->getStageNumber());
            if (sub != m_stages.end())
            {
                sub->second.active = flags.active;
            }
        }
    }
    fireChangeEvent();
}

bool FlightConfiguration::isStageActive(int stageNumber) const
{
    if (-1 == stageNumber)
    {
        return true;
    }
    // Stages with no children are inactive.
    const AxialStage* stage = m_rocket->getStage(stageNumber);
    const auto        it    = m_stages.find(stageNumber);
    return stage != nullptr && stage->getChildCount() > 0 && it != m_stages.end() &&
           it->second.active;
}

void FlightConfiguration::preloadStageActiveness(int stageNumber, bool isActive)
{
    if (!m_preloadStageActiveness)
    {
        m_preloadStageActiveness.emplace();
    }
    (*m_preloadStageActiveness)[stageNumber] = isActive;
}

void FlightConfiguration::applyPreloadedStageActiveness()
{
    if (!m_preloadStageActiveness)
    {
        return;
    }
    // A copy: setStageActive() does not touch the preloaded map, but keep the loop independent.
    const std::map<int, bool> preloaded = *m_preloadStageActiveness;
    for (const auto& [stageNumber, active] : preloaded)
    {
        setStageActive(stageNumber, active, false);
    }
    m_preloadStageActiveness.reset();
}

// ============================================================================= components

namespace
{

void addComponentsDepthFirst(RocketComponent& component, std::vector<RocketComponent*>& order,
                             const FlightConfiguration* activeOnly)
{
    if (activeOnly != nullptr && !activeOnly->isComponentActive(component))
    {
        return;
    }
    order.push_back(&component);
    for (RocketComponent* child : component.getChildren())
    {
        addComponentsDepthFirst(*child, order, activeOnly);
    }
}

}  // namespace

std::vector<RocketComponent*> FlightConfiguration::getAllComponents() const
{
    std::vector<RocketComponent*> traversalOrder;
    addComponentsDepthFirst(*m_rocket, traversalOrder, nullptr);
    return traversalOrder;
}

std::vector<RocketComponent*> FlightConfiguration::getAllActiveComponents() const
{
    std::vector<RocketComponent*> traversalOrder;
    addComponentsDepthFirst(*m_rocket, traversalOrder, this);
    return traversalOrder;
}

std::vector<RocketComponent*> FlightConfiguration::getCoreComponents() const
{
    std::deque<RocketComponent*>  toProcess{m_rocket};
    std::vector<RocketComponent*> toReturn;

    while (!toProcess.empty())
    {
        RocketComponent* comp = toProcess.front();
        toProcess.pop_front();

        if (comp->kind() != ComponentKind::ROCKET)
        {
            toReturn.push_back(comp);
        }
        std::ranges::copy_if(comp->getChildren(), std::back_inserter(toProcess),
                             [this](const RocketComponent* child) {
                                 if (child->kind() == ComponentKind::AXIAL_STAGE)
                                 {
                                     // Axial stages are still on the centreline; the exact class
                                     // is required, so that booster sets are left out.
                                     return isStageActive(child->getStageNumber());
                                 }
                                 // Booster sets and pod sets are skipped.
                                 return !isAssembly(child->kind());
                             });
    }
    return toReturn;
}

std::vector<RocketComponent*> FlightConfiguration::getActiveComponents() const
{
    const std::vector<AxialStage*> activeStages = getActiveStages();
    std::deque<RocketComponent*>   toProcess(activeStages.begin(), activeStages.end());
    std::vector<RocketComponent*>  toReturn;

    while (!toProcess.empty())
    {
        RocketComponent* comp = toProcess.front();
        toProcess.pop_front();

        toReturn.push_back(comp);
        std::ranges::copy_if(comp->getChildren(), std::back_inserter(toProcess),
                             [](const RocketComponent* child) { return !isStage(child->kind()); });
    }
    return toReturn;
}

void FlightConfiguration::updateActiveInstances()
{
    m_activeInstances.clear();
    m_extraRenderInstances.clear();
    addActiveContexts(*m_rocket, Transformation::kIdentity);
}

void FlightConfiguration::addActiveContexts(RocketComponent&      component,
                                            const Transformation& parentTransform)
{
    const int                     instanceCount = component.getInstanceCount();
    const std::vector<Coordinate> allOffsets    = component.getInstanceOffsets();
    const std::vector<double>     allAngles     = component.getInstanceAngles();
    // Java: ArrayIndexOutOfBoundsException for fewer offsets or angles than instances.
    QTROCKET_ASSERT(std::cmp_greater_equal(allOffsets.size(), instanceCount) &&
                    std::cmp_greater_equal(allAngles.size(), instanceCount));

    const Transformation compLocTransform   = Transformation::translation(component.getPosition());
    const Transformation componentTransform = parentTransform.applyTransformation(compLocTransform);

    const bool active = isComponentActive(component);
    // A booster set without children is inactive but still drawn when its stage is flagged
    // active (OpenRocket issue #1980).
    bool extraRender = false;
    if (!active && component.kind() == ComponentKind::PARALLEL_STAGE)
    {
        const auto it = m_stages.find(component.getStageNumber());
        extraRender   = it != m_stages.end() && it->second.active;
    }

    const std::vector<RocketComponent*> children = component.getChildren();
    for (int instanceNumber = 0; instanceNumber < instanceCount; instanceNumber++)
    {
        const auto           index           = static_cast<std::size_t>(instanceNumber);
        const Transformation offsetTransform = Transformation::translation(allOffsets[index]);
        const Transformation angleTransform  = Transformation::axialRotation(allAngles[index]);
        const Transformation currentTransform =
            componentTransform.applyTransformation(offsetTransform)
                .applyTransformation(angleTransform);

        if (active)
        {
            m_activeInstances.emplace(component, instanceNumber, currentTransform, parentTransform);
        }
        else if (extraRender)
        {
            m_extraRenderInstances.emplace(component, instanceNumber, currentTransform,
                                           parentTransform);
        }

        for (RocketComponent* child : children)
        {
            addActiveContexts(*child, currentTransform);
        }
    }
}

// ================================================================================= stages

std::vector<AxialStage*> FlightConfiguration::getAllStages() const
{
    std::vector<AxialStage*> stages;
    for (const auto& entry : m_stages)
    {
        if (m_rocket->getStage(entry.second.stageId) != nullptr)
        {
            stages.push_back(m_rocket->getStage(entry.second.stageId));
        }
    }
    return stages;
}

std::vector<AxialStage*> FlightConfiguration::getActiveStages() const
{
    std::vector<AxialStage*> activeStages;
    for (const auto& entry : m_stages)
    {
        const StageFlags& flags = entry.second;
        if (isStageActive(flags.stageNumber) && m_rocket->getStage(flags.stageId) != nullptr)
        {
            activeStages.push_back(m_rocket->getStage(flags.stageId));
        }
    }
    return activeStages;
}

int FlightConfiguration::getActiveStageCount() const
{
    return static_cast<int>(getActiveStages().size());
}

AxialStage* FlightConfiguration::getBottomStage() const
{
    AxialStage* bottomStage = nullptr;
    for (const auto& entry : m_stages)
    {
        if (isStageActive(entry.second.stageNumber))
        {
            bottomStage = m_rocket->getStage(entry.second.stageNumber);
        }
    }
    return bottomStage;
}

bool FlightConfiguration::isComponentActive(const RocketComponent& component) const
{
    return isStageActive(component.getStageNumber());
}

bool FlightConfiguration::isMountActive(const MotorMount& mount) const
{
    return isComponentActive(asComponent(mount));
}

bool FlightConfiguration::hasRecoveryDevice() const
{
    if (m_fcid.hasError())
    {
        return false;
    }
    return std::ranges::any_of(m_rocket->getStageList(), [this](const AxialStage* stage) {
        return isStageActive(stage->getStageNumber()) && stage->hasRecoveryDevice();
    });
}

void FlightConfiguration::updateStages()
{
    const std::map<int, StageFlags> stagesBackup = std::move(m_stages);
    m_stages.clear();
    for (const AxialStage* curStage : m_rocket->getStageList())
    {
        if (curStage == nullptr)
        {
            continue;
        }
        bool active = true;
        for (const auto& entry : stagesBackup)
        {
            if (entry.second.stageId == curStage->getId())
            {
                active = entry.second.active;
                break;
            }
        }
        m_stages[curStage->getStageNumber()] = StageFlags{.active      = active,
                                                          .stageNumber = curStage->getStageNumber(),
                                                          .stageId     = curStage->getId()};
    }
}

// =============================================================================== geometry

double FlightConfiguration::getReferenceLength() const
{
    if (m_rocket->getModId() != m_refLengthModId)
    {
        m_refLengthModId  = m_rocket->getModId();
        m_cachedRefLength = QtRocket::getReferenceLength(m_rocket->getReferenceType(), *this);
    }
    return m_cachedRefLength;
}

double FlightConfiguration::getReferenceArea() const
{
    return std::numbers::pi * MathUtil::pow2(getReferenceLength() / 2);
}

std::vector<Coordinate> FlightConfiguration::getBounds() const
{
    return getBoundingBoxAerodynamic().toCollection();
}

BoundingBox FlightConfiguration::getBoundingBoxAerodynamic() const
{
    // Java's modification id test is commented out here: the bounds are always recomputed.
    calculateBounds();
    if (m_cachedBoundsAerodynamic.isEmpty())
    {
        m_cachedBoundsAerodynamic = BoundingBox{Coordinate::kZero, Coordinate::kXUnit};
    }
    return m_cachedBoundsAerodynamic;
}

BoundingBox FlightConfiguration::getBoundingBox() const
{
    calculateBounds();
    if (m_cachedBounds.isEmpty())
    {
        m_cachedBounds = BoundingBox{Coordinate::kZero, Coordinate::kXUnit};
    }
    return m_cachedBounds;
}

void FlightConfiguration::calculateBounds() const
{
    BoundingBox rocketBoundsAerodynamic;  // of the aerodynamic components
    BoundingBox rocketBounds;             // of every component

    for (const auto& [key, contexts] : m_activeInstances)
    {
        QTROCKET_ASSERT(key != nullptr);
        const RocketComponent& component = *key;
        BoundingBox            componentBoundsAerodynamic;
        BoundingBox            componentBounds;

        if (const auto* boxBounded = dynamic_cast<const BoxBounded*>(&component))
        {
            // Fin sets, body components, ... give the box of one instance.
            const BoundingBox instanceBounds = boxBounded->getInstanceBoundingBox();
            if (instanceBounds.isEmpty())
            {
                // Non-physical (an assembly) or invalid bounds: skipped.
                continue;
            }
            for (const InstanceContext& context : contexts)
            {
                const BoundingBox transformed = instanceBounds.transform(context.transform);
                if (component.isAerodynamic())
                {
                    componentBoundsAerodynamic.update(transformed);
                }
                componentBounds.update(transformed);
            }
        }
        else if (!contexts.empty())
        {
            // The legacy case, components that are not BoxBounded (the mass objects), reproduced
            // from Java: it skips every context of a component after its first, and it iterates
            // the copy of the component's bounds taken before transforming them in place, so the
            // bounds count once and untransformed, in the component's own frame.
            for (const Coordinate& c : component.getComponentBounds())
            {
                if (component.isAerodynamic())
                {
                    componentBoundsAerodynamic.update(c);
                }
                componentBounds.update(c);
            }
        }

        rocketBoundsAerodynamic.update(componentBoundsAerodynamic);
        rocketBounds.update(componentBounds);
    }

    m_boundsModId             = m_rocket->getModId();
    m_cachedLengthAerodynamic = rocketBoundsAerodynamic.span().x;
    m_cachedLength            = rocketBounds.span().x;
    // All stages removed or inactive: no length.
    if (rocketBoundsAerodynamic.isEmpty())
    {
        m_cachedLengthAerodynamic = 0;
    }
    if (rocketBounds.isEmpty())
    {
        m_cachedLength = 0;
    }
    m_cachedBoundsAerodynamic = rocketBoundsAerodynamic;
    m_cachedBounds            = rocketBounds;
}

double FlightConfiguration::getLengthAerodynamic() const
{
    if (m_rocket->getModId() != m_boundsModId)
    {
        calculateBounds();
    }
    return m_cachedLengthAerodynamic;
}

double FlightConfiguration::getLength() const
{
    if (m_rocket->getModId() != m_boundsModId)
    {
        calculateBounds();
    }
    return m_cachedLength;
}

void FlightConfiguration::fireChangeEvent()
{
    m_modId          = ModId{};
    m_boundsModId    = ModId::invalid();
    m_refLengthModId = ModId::invalid();

    updateStages();
    updateMotors();
    updateActiveInstances();
}

// =================================================================================== name

std::string FlightConfiguration::getDefaultName(const Preferences& preferences)
{
    return preferences.getDefaultFlightConfigName();
}

bool FlightConfiguration::isNameOverridden(const Preferences& preferences) const
{
    return m_configurationName.has_value() && *m_configurationName != getDefaultName(preferences);
}

std::string FlightConfiguration::getName(const Preferences& preferences) const
{
    return formatName(getNameRaw(preferences), *m_rocket, m_fcid, preferences);
}

std::string FlightConfiguration::getNameRaw(const Preferences& preferences) const
{
    return m_configurationName ? *m_configurationName : getDefaultName(preferences);
}

void FlightConfiguration::setName(std::string_view newName)
{
    if (newName.empty())
    {
        m_configurationName.reset();
        return;
    }
    if (!m_fcid.isValid())
    {
        return;
    }
    if (m_configurationName && *m_configurationName == newName)
    {
        return;
    }
    m_configurationName = std::string{newName};
}

void FlightConfiguration::setNameRaw(const std::optional<std::string>& name)
{
    if (name)
    {
        setName(*name);
    }
    else
    {
        m_configurationName.reset();
    }
}

std::string FlightConfiguration::toString(const Preferences& preferences) const
{
    return getName(preferences);
}

// ================================================================================= motors

void FlightConfiguration::addMotor(const MotorConfiguration& motorConfig)
{
    // Java logs an error for an empty configuration and adds it all the same.
    const auto it = std::ranges::find(m_motors, motorConfig.getId(), &MotorConfiguration::getId);
    if (it != m_motors.end())
    {
        *it = motorConfig;
    }
    else
    {
        m_motors.push_back(motorConfig);
    }
    m_modId = ModId{};
}

void FlightConfiguration::updateMotors()
{
    m_motors.clear();

    for (const RocketComponent* comp : getActiveComponents())
    {
        const auto* mount = dynamic_cast<const MotorMount*>(comp);
        if (mount == nullptr || !mount->isMotorMount())
        {
            continue;
        }
        const MotorConfiguration& motorConfig = mount->getMotorConfig(m_fcid);
        if (motorConfig.isEmpty())
        {
            continue;
        }
        // Java: motors.put(mid, config), one entry per id.
        const auto it =
            std::ranges::find(m_motors, motorConfig.getMid(), &MotorConfiguration::getMid);
        if (it != m_motors.end())
        {
            *it = motorConfig;
        }
        else
        {
            m_motors.push_back(motorConfig);
        }
    }

    m_activeMotors.clear();
    for (const MotorConfiguration& config : m_motors)
    {
        if (isComponentActive(config.getMount()))
        {
            m_activeMotors.push_back(config);
        }
    }
}

void FlightConfiguration::clearAllMotors()
{
    for (RocketComponent* comp : getActiveComponents())
    {
        auto* mount = dynamic_cast<MotorMount*>(comp);
        if (mount != nullptr && mount->isMotorMount())
        {
            mount->reset(m_fcid);
        }
    }
    updateMotors();
}

InstanceMap FlightConfiguration::getLowestMotorInstances() const
{
    InstanceMap lowestMotorInstances;
    double      lowestAxialPosition = -std::numeric_limits<double>::infinity();
    bool        foundMotorInstance  = false;

    const auto collect = [&](const InstanceMap::Entry& entry) {
        RocketComponent& component  = *entry.first;
        const auto*      motorMount = dynamic_cast<const MotorMount*>(&component);
        if (!component.isVisible() || motorMount == nullptr)
        {
            return;
        }
        if (motorMount->getMotorConfig(m_fcid).getMotor() == nullptr)
        {
            return;
        }
        for (const InstanceContext& context : entry.second)
        {
            const double nozzleAxialPosition =
                context.getLocation().x + motorMount->getLength() + motorMount->getMotorOverhang();
            if (!foundMotorInstance ||
                nozzleAxialPosition > lowestAxialPosition + MathUtil::kEpsilon)
            {
                lowestMotorInstances.clear();
                lowestMotorInstances.put(component, {context});
                lowestAxialPosition = nozzleAxialPosition;
                foundMotorInstance  = true;
            }
            else if (MathUtil::equals(nozzleAxialPosition, lowestAxialPosition))
            {
                lowestMotorInstances.add(context);
            }
        }
    };

    for (const InstanceMap::Entry& entry : m_activeInstances)
    {
        collect(entry);
    }
    for (const InstanceMap::Entry& entry : m_extraRenderInstances)
    {
        collect(entry);
    }
    return lowestMotorInstances;
}

void FlightConfiguration::update()
{
    updateStages();
    updateMotors();
    updateActiveInstances();
}

void FlightConfiguration::forgetComponents(const RocketComponent& removedRoot)
{
    // The removed subtree is detached: its components have removedRoot as their root.
    const auto inRemoved = [&removedRoot](const RocketComponent& component) {
        return &component.getRoot() == &removedRoot;
    };
    m_activeInstances.eraseIf(inRemoved);
    m_extraRenderInstances.eraseIf(inRemoved);
    const auto mountInRemoved = [&inRemoved](const MotorConfiguration& config) {
        return inRemoved(asComponent(config.getMount()));
    };
    std::erase_if(m_motors, mountInRemoved);
    std::erase_if(m_activeMotors, mountInRemoved);
}

// ================================================================================ copying

FlightConfiguration FlightConfiguration::clone(Rocket& rocket) const
{
    // The constructor updates the stages.
    FlightConfiguration clone{rocket, m_fcid};
    clone.setNameRaw(m_configurationName);
    clone.copyStageActiveness(*this);
    clone.m_preloadStageActiveness = m_preloadStageActiveness;
    clone.m_modId                  = m_modId;
    // Java also copies the cached bounds, which nothing reads before they are recomputed (the
    // bounds ids are invalid); not reading them keeps a clone made on another thread (a
    // simulation's) from racing with the source's geometry getters.
    clone.m_boundsModId    = ModId::invalid();
    clone.m_refLengthModId = ModId::invalid();
    return clone;
}

FlightConfiguration FlightConfiguration::clone() const
{
    return clone(*m_rocket);
}

FlightConfiguration FlightConfiguration::copy(const FlightConfigurationId& newId) const
{
    // The constructor updates the stages.
    FlightConfiguration         copy{*m_rocket, newId};
    const FlightConfigurationId copyId = copy.getId();

    // The motors, copied to the new id in their mounts.
    const std::vector<MotorConfiguration> sourceMotors = m_motors;
    for (const MotorConfiguration& sourceMotor : sourceMotors)
    {
        MotorConfiguration cloneMotor = sourceMotor.copy(copyId);
        copy.addMotor(cloneMotor);
        cloneMotor.getMount().setMotorConfig(cloneMotor, copyId);
    }

    copy.copyStages(*this);
    copy.m_preloadStageActiveness = m_preloadStageActiveness;
    copy.m_modId                  = m_modId;
    // The cached bounds are not copied (see clone()).
    copy.m_boundsModId       = ModId::invalid();
    copy.m_refLengthModId    = ModId::invalid();
    copy.m_configurationName = m_configurationName;
    return copy;
}

// ================================================================================== debug

std::string FlightConfiguration::toDebug(const Preferences& preferences) const
{
    return m_fcid.toDebug() + " (#" + std::to_string(m_configurationInstanceId) + ") " +
           getName(preferences);
}

std::string FlightConfiguration::toStageListDetail(const Preferences& preferences) const
{
    std::string buffer;
    std::format_to(std::back_inserter(buffer), "\nDumping {} stages for config: {}: ({})(#: {})\n",
                   m_stages.size(), getName(preferences), m_fcid.toShortKey(),
                   m_configurationInstanceId);
    // Java's "    [%-2s][%4s]: %6s \n".
    const auto appendRow = [&buffer](std::string_view number, std::string_view active,
                                     std::string_view name) {
        std::format_to(std::back_inserter(buffer), "    [{:<2}][{:>4}]: {:>6} \n", number, active,
                       name);
    };
    appendRow("#", "?actv", "Name");
    for (const auto& entry : m_stages)
    {
        const StageFlags& flags = entry.second;
        const AxialStage* stage = m_rocket->getStage(flags.stageId);
        appendRow(flags.stageId.toString(), flags.active ? " on" : "off",
                  stage != nullptr ? stage->getName() : std::string{"null"});
    }
    buffer += '\n';
    return buffer;
}

std::string FlightConfiguration::toMotorDetail(const Preferences& preferences) const
{
    std::string buffer;
    std::format_to(std::back_inserter(buffer),
                   "\nDumping {:2} Motors for configuration {} ({})(#: {})\n", m_motors.size(),
                   getName(preferences), m_fcid.toShortKey(), m_configurationInstanceId);
    for (const MotorConfiguration& curConfig : m_motors)
    {
        const bool active =
            isStageActive(asComponent(curConfig.getMount()).getStage().getStageNumber());
        buffer += std::string{"    ("} + (active ? "active" : "      ") + ")" +
                  curConfig.toDebugDetail(preferences) + "\n";
    }
    buffer += '\n';
    return buffer;
}

}  // namespace QtRocket

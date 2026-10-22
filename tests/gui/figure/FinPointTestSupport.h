#pragma once

// Helpers of the tests of the freeform fin editor (FinPointSelection, FinPointEditHistory,
// FinPointFigure, FinPointScrollArea, FinPointTableModel, FreeformFinSetConfig): a design with
// freeform fin sets and the rocket's change events.

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "QtRocket/gui/figure/FinPointSelection.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket
{
class FreeformFinSet;
class OpenRocketDocument;
class Rocket;
}  // namespace QtRocket

namespace QtRocket::Gui::Test
{

/// Rocket > Stage > a body tube 30 cm long with a fixed radius of 2.1 cm (FreeformPanelProbe's) >
/// freeform fin sets, one per outline (an empty outline: FreeformFinSet's default, (0, 0), (2.5,
/// 5), (7.5, 5), (5, 0) cm), with the rocket's events on and an empty undo history. The fin sets
/// are found by id, so fins() stays right across an undo.
class FreeformDesign
{
public:
    /// The outlines as x, y, x, y, ... in m.
    using Outline = std::vector<double>;

    explicit FreeformDesign(const std::vector<Outline>& outlines = {Outline{}});
    ~FreeformDesign();
    FreeformDesign(const FreeformDesign&)            = delete;
    FreeformDesign& operator=(const FreeformDesign&) = delete;
    FreeformDesign(FreeformDesign&&)                 = delete;
    FreeformDesign& operator=(FreeformDesign&&)      = delete;

    [[nodiscard]] OpenRocketDocument& document() const noexcept { return *m_document; }
    [[nodiscard]] Rocket&             rocket() const;
    /// The fin set @p index as the rocket has it now.
    [[nodiscard]] FreeformFinSet& fins(std::size_t index = 0) const;
    [[nodiscard]] const Uuid&     id(std::size_t index = 0) const { return m_ids.at(index); }
    /// The selection of the fin set @p main with the fin sets @p others.
    [[nodiscard]] FinPointSelection selection(std::size_t                        main   = 0,
                                              std::initializer_list<std::size_t> others = {}) const;

private:
    std::unique_ptr<OpenRocketDocument> m_document;
    std::vector<Uuid>                   m_ids;
};

/// The points as "(x,y) (x,y) ...", each coordinate as Java writes a double.
[[nodiscard]] std::string pointsText(std::span<const Coordinate> points);

/// The fin points of @p fins as pointsText().
[[nodiscard]] std::string finPointsText(const FreeformFinSet& fins);

/// Records the change events @p rocket fires while it lives: their types.
class RocketEvents
{
public:
    explicit RocketEvents(Rocket& rocket);

    [[nodiscard]] const std::vector<int>& types() const noexcept { return *m_types; }
    [[nodiscard]] std::size_t             count() const noexcept { return m_types->size(); }
    void                                  clear() noexcept { m_types->clear(); }

private:
    std::shared_ptr<std::vector<int>>       m_types;
    ComponentChangeSignal::ScopedConnection m_connection;
};

}  // namespace QtRocket::Gui::Test

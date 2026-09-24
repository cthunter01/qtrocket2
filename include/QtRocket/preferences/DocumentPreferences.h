#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "QtRocket/util/Color.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket
{

/// The preferences saved inside a document rather than application-wide (OpenRocket:
/// preferences/DocumentPreferences, the `<docprefs>` element of an .ork file): a typed key/value
/// map plus the document's own materials.
///
/// Values (the ORPreferences typed accessors, the same eight methods as Preferences; there is no
/// common base type, see Preferences.h): each preference is a bool, int, double or std::string
/// and remembers which (DocumentPreference::type()); the .ork writer spells the type as
/// typeName(). Those four are
/// the only kinds a DocumentPreferences can hold: Java's putPreference() is private and reached
/// only through the four typed putters (putColor() stores a string), so the "number" and "list"
/// entry types OpenRocketSaver.writeEntry() knows for other entry kinds never occur in a
/// document preference, and the loader rejects them ("Number preferences are not supported",
/// "Nested preferences are not supported").
///
/// Reading with the wrong type (getInt() of a string value) gives the default. Deviation: Java
/// casts and throws ClassCastException; a stored type is file input, not an invariant.
///
/// Change notification (Java: AbstractChangeSource): changed() is emitted by the putters when
/// the value changes, in Java's sense of Objects.equals (so a double replaced by the same double
/// is no change, NaN equals NaN, and 0.0 differs from -0.0), and by removePreference() when the
/// key existed. Materials do not emit it (Java's Database has listeners of its own).
///
/// Materials (Java: three Database<Material> for BULK, SURFACE and LINE; getBulkMaterials(),
/// getSurfaceMaterials(), getLineMaterials(), getDatabase(Type), getAllMaterials(),
/// addMaterial(Material), removeMaterial(Material), getMaterialCount(Type),
/// getTotalMaterialCount()): material/ sits above preferences/ (plan 3.1), so a material here is
/// its Material.toStorableString() text, "TYPE|name|density|shearModulus|group", the form the
/// .ork `<docmaterials>` element holds. They are kept unique (by text, where Java compares with
/// Material.equals) and in insertion order; Java's Database sorts by name then density and
/// getAllMaterials() lists bulk, then surface, then line materials. The typed form of those three
/// databases is MaterialStorage (QtRocket/material/MaterialStorage.h: database(),
/// addMaterial(), removeMaterial(), allMaterials(), materialCount(), totalMaterialCount()); the
/// document group, above both, decides whether a document keeps its materials there instead.
///
/// A copy holds the same preferences and materials and none of the connections to changed().
class DocumentPreferences
{
public:
    // Keys of the preferences the rocket panel stores in a document (Java: PREF_*).
    static constexpr std::string_view kPrefShowWarnings      = "RocketPanel.showWarnings";
    static constexpr std::string_view kPref2DBackgroundColor = "RocketPanel.2DBackgroundColor";
    static constexpr std::string_view kPref3DBackgroundColor = "RocketPanel.3DBackgroundColor";
    static constexpr std::string_view kPref2DTextColor       = "RocketPanel.2DTextColor";
    static constexpr std::string_view kPref3DTextColor       = "RocketPanel.3DTextColor";
    static constexpr std::string_view kPref3DRenderQuality   = "RocketPanel.3DRenderQuality";
    static constexpr std::string_view kPref3DShadowsEnabled  = "RocketPanel.3DShadowsEnabled";
    static constexpr std::string_view kPref3DAmbientOcclusionEnabled =
        "RocketPanel.3DAmbientOcclusionEnabled";
    static constexpr std::string_view kPref3DRoughnessBumpEnabled =
        "RocketPanel.3DRoughnessBumpEnabled";
    static constexpr std::string_view kPref3DOriginAxesVisible = "RocketPanel.3DOriginAxesVisible";
    static constexpr std::string_view kPref3DLightVisualizersVisible =
        "RocketPanel.3DLightVisualizersVisible";
    static constexpr std::string_view kPref3DCameraPointOfInterestVisible =
        "RocketPanel.3DCameraPointOfInterestVisible";
    static constexpr std::string_view kPref3DRotateRocketOnDrag =
        "RocketPanel.3DRotateRocketOnDrag";
    static constexpr std::string_view kPref3DCaretScaleWithView =
        "RocketPanel.3DCaretScaleWithView";

    /// The kinds of value a preference holds, in Value's alternative order.
    enum class Type
    {
        BOOLEAN,
        INTEGER,
        DOUBLE,
        STRING,
    };

    using Value = std::variant<bool, int, double, std::string>;

    // DocumentPreference::type() is Value's index cast to Type, so the two orders must agree.
    static_assert(
        std::is_same_v<std::variant_alternative_t<static_cast<std::size_t>(Type::BOOLEAN), Value>,
                       bool> &&
            std::is_same_v<
                std::variant_alternative_t<static_cast<std::size_t>(Type::INTEGER), Value>, int> &&
            std::is_same_v<
                std::variant_alternative_t<static_cast<std::size_t>(Type::DOUBLE), Value>,
                double> &&
            std::is_same_v<
                std::variant_alternative_t<static_cast<std::size_t>(Type::STRING), Value>,
                std::string>,
        "Type must list the alternatives of Value in the same order");

    /// One stored value and its type (Java: DocumentPreferences.DocumentPreference).
    class DocumentPreference
    {
    public:
        explicit DocumentPreference(Value value) : m_value(std::move(value)) { }

        [[nodiscard]] const Value& value() const noexcept { return m_value; }
        [[nodiscard]] Type type() const noexcept { return static_cast<Type>(m_value.index()); }

        /// Java's Objects.equals(): same type and, for doubles, Double.equals (NaN equals NaN,
        /// 0.0 differs from -0.0).
        [[nodiscard]] bool operator==(const DocumentPreference& other) const;

    private:
        Value m_value;
    };

    using Map = std::map<std::string, DocumentPreference, std::less<>>;

    /// The .ork `type` attribute of a value type: "boolean", "integer", "double" or "string"
    /// (OpenRocketSaver.writeEntry with explicit number types).
    [[nodiscard]] static std::string_view typeName(Type type) noexcept;

    DocumentPreferences()  = default;
    ~DocumentPreferences() = default;
    /// Copies the preferences and materials, not the connections to changed().
    DocumentPreferences(const DocumentPreferences& other);
    DocumentPreferences& operator=(const DocumentPreferences& other);
    DocumentPreferences(DocumentPreferences&&) noexcept            = default;
    DocumentPreferences& operator=(DocumentPreferences&&) noexcept = default;

    // ----------------------------------------------------------- ORPreferences accessors

    [[nodiscard]] bool        getBoolean(std::string_view key, bool defaultValue) const;
    void                      putBoolean(std::string_view key, bool value);
    [[nodiscard]] int         getInt(std::string_view key, int defaultValue) const;
    void                      putInt(std::string_view key, int value);
    [[nodiscard]] double      getDouble(std::string_view key, double defaultValue) const;
    void                      putDouble(std::string_view key, double value);
    [[nodiscard]] std::string getString(std::string_view key, std::string_view defaultValue) const;
    void                      putString(std::string_view key, std::string_view value);

    /// A colour stored as "R,G,B" (a string value): the parsed colour, or nullopt when the key is
    /// absent, holds another type or does not parse (Java: getColor(key, null)).
    [[nodiscard]] std::optional<Color> getColor(std::string_view key) const;
    /// getColor(key), or @p defaultValue when that is nullopt.
    [[nodiscard]] Color getColor(std::string_view key, const Color& defaultValue) const;
    /// Stores stringifyColor(*value), or removes the key for nullopt (Java: putColor(key, null)).
    void putColor(std::string_view key, const std::optional<Color>& value);

    /// Parses "R,G,B" as DocumentPreferences.parseColor does: exactly three comma-separated
    /// fields (Java's split, so trailing empty fields are ignored), each trimmed then read as an
    /// Integer.parseInt integer and clamped to 0-255. Anything else is nullopt.
    [[nodiscard]] static std::optional<Color> parseColor(std::string_view text);
    /// "R,G,B" (the alpha is not stored).
    [[nodiscard]] static std::string stringifyColor(const Color& color);

    // ------------------------------------------------------------------------ the map

    /// Every preference by key, sorted by key (Java: getPreferencesMap(), a HashMap, whose order
    /// is unspecified).
    [[nodiscard]] const Map& preferences() const noexcept { return m_preferences; }
    /// The preference under @p key, or nullptr.
    [[nodiscard]] const DocumentPreference* find(std::string_view key) const noexcept;
    [[nodiscard]] bool                      contains(std::string_view key) const noexcept;
    [[nodiscard]] std::size_t               size() const noexcept { return m_preferences.size(); }
    /// Removes @p key; emits changed() when it was there (Java: removePreference()).
    void removePreference(std::string_view key);

    // ---------------------------------------------------------------------- materials

    /// Adds a material by its storable string; false (and nothing done) when it is already there
    /// (Java: addMaterial(), through Database.add()).
    bool addMaterial(std::string_view storableString);
    /// Removes the material; false when it was not there (Java: removeMaterial()).
    bool removeMaterial(std::string_view storableString);
    /// The materials in insertion order (Java: getAllMaterials()).
    [[nodiscard]] const std::vector<std::string>& materials() const noexcept { return m_materials; }
    /// Java: getTotalMaterialCount().
    [[nodiscard]] std::size_t materialCount() const noexcept { return m_materials.size(); }

    /// Emitted when a preference is stored with a new value or removed (see the class comment).
    [[nodiscard]] Signal<>& changed() noexcept { return m_changed; }

private:
    /// Java: putPreference(key, value): stores unless the same value is already there.
    void putPreference(std::string_view key, Value value);

    /// The stored value under @p key when it holds a T, else nullopt.
    template <class T>
    [[nodiscard]] std::optional<T> getAs(std::string_view key) const;

    Map                      m_preferences;
    std::vector<std::string> m_materials;
    Signal<>                 m_changed;
};

}  // namespace QtRocket

#pragma once

#include <filesystem>
#include <optional>
#include <utility>

namespace QtRocket
{

class AttachmentFactory;
class ComponentPresetDatabase;
class MaterialStorage;
class MotorFinder;
class OpenRocketDocument;
class Preferences;
class SimulationExtensionRegistry;

/// What the loader of a design file works with: the file's format version, the document it
/// fills, and everything it has to ask someone else for (OpenRocket's
/// file/DocumentLoadingContext). GeneralRocketLoader makes one for a load, a copy of the one it
/// was given in which it sets the document, the file version, the attachment factory and the
/// design's directory, and every handler of that load reads the same one, by reference.
///
/// Java's context holds the file version, the motor finder, the attachment factory and the
/// document. Four more are what Java's loader takes from the application's global objects
/// (Application, Databases, the Guice injector); QtRocket has none, so they are handed in here.
/// The design's directory is QtRocket's own (see getDesignDirectory()).
///
/// Every pointer is borrowed: the context owns nothing, and whoever sets a pointer keeps the
/// object alive for as long as the comment of its getter says. A context is a plain value and
/// may be copied.
///
/// Deviations from OpenRocket:
/// - The pointers the methods of these objects can be called through are to const where the
///   loader only asks (the motor finder, the attachment factory, the application's materials,
///   the presets and the extension providers).
/// - Java's context starts with a FileSystemAttachmentFactory without a base directory as its
///   attachment factory. Here getAttachmentFactory() returns such a factory while none is set
///   (and after nullptr was set), so it never returns null, as in Java.
class DocumentLoadingContext
{
public:
    /// The version of the file's format as OpenRocketHandler reads it from the root element:
    /// major * 100 + minor (DocumentConfig::kFileVersionDivisor), so "1.10" is 110, and 0 when
    /// the file does not say or says something else. Only the motor handler reads it (the
    /// digest of a motor counts from 1.4 on).
    [[nodiscard]] int getFileVersion() const noexcept { return m_fileVersion; }
    void              setFileVersion(int fileVersion) noexcept { m_fileVersion = fileVersion; }

    /// Who finds the motor a <motor> element names; null until one is set, and the motor
    /// handler needs one (MotorHandler::getMotor() is a BugError without). It is used while the
    /// file loads and not kept.
    [[nodiscard]] const MotorFinder* getMotorFinder() const noexcept { return m_motorFinder; }
    void setMotorFinder(const MotorFinder* motorFinder) noexcept { m_motorFinder = motorFinder; }

    /// The document being filled; null until one is set. The handlers put the rocket, the
    /// simulations, the decal images and the document's own materials and preferences into it.
    [[nodiscard]] OpenRocketDocument* getOpenRocketDocument() const noexcept { return m_document; }
    void setOpenRocketDocument(OpenRocketDocument* document) noexcept { m_document = document; }

    /// Who makes the attachments the file names (decal images, embedded thrust curves); never
    /// null (see the class comment). The factory is used while the file loads and not kept: an
    /// attachment it made does not refer back to it.
    [[nodiscard]] const AttachmentFactory* getAttachmentFactory() const noexcept;
    /// Sets the factory; nullptr brings back the one without a base directory.
    void setAttachmentFactory(const AttachmentFactory* attachmentFactory) noexcept
    {
        m_attachmentFactory = attachmentFactory;
    }

    /// The application's materials, in which the loader looks a component's material up before
    /// it makes a material of the document (Java: Databases); null for an application without
    /// materials, in which every material of a file is the document's own. They are read while
    /// the file loads and not kept: a component holds a copy of its material.
    [[nodiscard]] const MaterialStorage* getApplicationMaterials() const noexcept
    {
        return m_applicationMaterials;
    }
    void setApplicationMaterials(const MaterialStorage* materials) noexcept
    {
        m_applicationMaterials = materials;
    }

    /// The preference store the simulations of the file are made with (Java: the application's
    /// preferences); null until one is set, and a file with a simulation needs one. Unlike the
    /// others it is kept: the SimulationOptions and the Simulation objects of the loaded
    /// document go on reading and writing it, so it must outlive the document's simulations.
    [[nodiscard]] Preferences* getPreferences() const noexcept { return m_preferences; }
    void setPreferences(Preferences* preferences) noexcept { m_preferences = preferences; }

    /// The component presets a <preset> element is looked up in (Java: the application's
    /// ComponentPresetDao); null, like an empty database, finds none, so that every <preset>
    /// gives OpenRocket's warning and the component keeps no link to a preset. It is read while
    /// the file loads and not kept: a component shares the ownership of the preset it found.
    [[nodiscard]] const ComponentPresetDatabase* getComponentPresetDatabase() const noexcept
    {
        return m_componentPresetDatabase;
    }
    void setComponentPresetDatabase(const ComponentPresetDatabase* database) noexcept
    {
        m_componentPresetDatabase = database;
    }

    /// The providers of the simulation extensions an <extension> element names (Java: the
    /// injector's set of SimulationExtensionProvider); null knows no extension id, so that
    /// every extension of the file is kept as an unknown one with OpenRocket's warning. It is
    /// asked while the file loads and not kept: an extension it made does not refer back to it.
    [[nodiscard]] const SimulationExtensionRegistry* getSimulationExtensionRegistry() const noexcept
    {
        return m_simulationExtensionRegistry;
    }
    void setSimulationExtensionRegistry(const SimulationExtensionRegistry* registry) noexcept
    {
        m_simulationExtensionRegistry = registry;
    }

    /// The directory of the design file that is being loaded, or nullopt when the design has
    /// none: it is read from bytes in memory and the caller named no directory for them.
    /// GeneralRocketLoader sets it, to the directory of the file for a load from a path ("."
    /// for a file that is named without one) and to the base directory of a load from bytes.
    /// Not OpenRocket's: it is what a file named by a design, a lookup table of a simulation,
    /// will be resolved against and confined to (the user's decision of 2026-10-08; nothing
    /// reads it yet, and CsvLookupHandler still resolves such a name against the current
    /// directory of the process). The path is kept as it was given, not made absolute.
    [[nodiscard]] const std::optional<std::filesystem::path>& getDesignDirectory() const noexcept
    {
        return m_designDirectory;
    }
    void setDesignDirectory(std::optional<std::filesystem::path> directory) noexcept
    {
        m_designDirectory = std::move(directory);
    }

private:
    int                m_fileVersion{0};
    const MotorFinder* m_motorFinder{nullptr};
    /// Null stands for the factory without a base directory.
    const AttachmentFactory*             m_attachmentFactory{nullptr};
    OpenRocketDocument*                  m_document{nullptr};
    const MaterialStorage*               m_applicationMaterials{nullptr};
    Preferences*                         m_preferences{nullptr};
    const ComponentPresetDatabase*       m_componentPresetDatabase{nullptr};
    const SimulationExtensionRegistry*   m_simulationExtensionRegistry{nullptr};
    std::optional<std::filesystem::path> m_designDirectory;
};

}  // namespace QtRocket

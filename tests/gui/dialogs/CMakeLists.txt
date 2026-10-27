# The tests of the GUI area dialogs (gui/src/dialogs): XTests.cpp for the class X, lower_snake_case_tests.cpp
# for tests about no one class, in alphabetical order (ignoring case).
target_sources(QtRocket_gui_tests PRIVATE
    DecalNotFoundDialogTests.cpp
    DefaultUnitSelectorTests.cpp
    DesignPreferencesPanelTests.cpp
    DetailDialogTests.cpp
    ErrorWarningDialogTests.cpp
    GeneralPreferencesPanelTests.cpp
    InternalErrorDialogTests.cpp
    MessageListTests.cpp
    ModalDialogTests.cpp
    OptionDialogTests.cpp
    PrefChoiceSelectorTests.cpp
    preferences_pages_golden_tests.cpp
    PreferencesDialogTests.cpp
    PreferencesPageTranscript.cpp
    PreferencesStoreTests.cpp
    SaveDesignInfoDialogTests.cpp
    StorageOptionsDialogTests.cpp
    ThrustCurveMotorPlotDialogTests.cpp
    UnitsPreferencesPanelTests.cpp
    WarningDialogTests.cpp
)

# The motor chooser (dialogs/motor/).
add_subdirectory(motor)

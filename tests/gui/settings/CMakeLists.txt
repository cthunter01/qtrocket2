# The tests of the GUI area settings (gui/src/settings): XTests.cpp for the class X, lower_snake_case_tests.cpp
# for tests about no one class, in alphabetical order (ignoring case).
target_sources(QtRocket_gui_tests PRIVATE
    GuiPreferenceKeysTests.cpp
    PreferencesTranscript.cpp
    SettingsPreferencesTests.cpp
    WindowGeometryTests.cpp
)

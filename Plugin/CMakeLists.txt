# Totality as a VST3 and a standalone (Phase 5). After Ephemeris' Plugin/CMakeLists.txt at d047d79.
juce_add_plugin(Totality
    COMPANY_NAME                "Rene Weller"
    BUNDLE_ID                   "com.reneweller.totality"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    PLUGIN_MANUFACTURER_CODE    Rnwl
    PLUGIN_CODE                 Totl
    FORMATS                     VST3 Standalone
    VST3_CATEGORIES             Instrument Synth
    VERSION                     ${PROJECT_VERSION}
    PRODUCT_NAME                "Totality"
    ICON_BIG                    "${CMAKE_SOURCE_DIR}/Deploy/totality.png")
target_sources(Totality PRIVATE PluginProcessor.cpp PluginEditor.cpp EditorTheme.cpp EditorEclipse.cpp EditorPerform.cpp
                             EditorStyle.cpp UpdateCheck.cpp Frame.cpp)
# The frame's pieces (01.10.2026): the picture behind the panel and the manual's prose for the help (F1).
juce_add_binary_data(TotalityData
    HEADER_NAME TotalityData.h
    NAMESPACE TotalityData
    SOURCES Resources/backdrop.jpg ${CMAKE_SOURCE_DIR}/Tools/manual/chapters.txt)
target_compile_definitions(Totality PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    _CRT_SECURE_NO_WARNINGS)
target_link_libraries(Totality
    PRIVATE
        TotalityCore
        TotalityData
        juce::juce_audio_utils
        juce::juce_osc
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# Into bin/<preset> after every build (cmake/Family.cmake, 01.10.2026): the standalone and the VST3 bundle.
family_bin(Totality_Standalone)
family_bin(Totality_VST3 VST3)

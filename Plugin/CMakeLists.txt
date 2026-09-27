# Umbra as a VST3 and a standalone (Phase 5). After Ephemeris' Plugin/CMakeLists.txt at d047d79.
juce_add_plugin(Umbra
    COMPANY_NAME                "Rene Weller"
    BUNDLE_ID                   "com.reneweller.umbra"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    PLUGIN_MANUFACTURER_CODE    Rnwl
    PLUGIN_CODE                 Umbr
    FORMATS                     VST3 Standalone
    VST3_CATEGORIES             Instrument Synth
    VERSION                     ${PROJECT_VERSION}
    PRODUCT_NAME                "Umbra"
    ICON_BIG                    "${CMAKE_SOURCE_DIR}/Deploy/umbra.png")
target_sources(Umbra PRIVATE PluginProcessor.cpp PluginEditor.cpp EditorTheme.cpp EditorEclipse.cpp EditorPerform.cpp
                             EditorStyle.cpp UpdateCheck.cpp)
target_compile_definitions(Umbra PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    _CRT_SECURE_NO_WARNINGS)
target_link_libraries(Umbra
    PRIVATE
        UmbraCore
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

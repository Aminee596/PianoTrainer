cmake_minimum_required(VERSION 3.22)

project(PianoTrainer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PIANO_BUILD_APP   "Build the JUCE desktop app"        ON)
option(PIANO_BUILD_TESTS "Build the core engine unit tests"  ON)
option(PIANO_UI_DEV_SERVER "Load the menus from the Vite dev server (npm run dev in ui/) instead of the embedded build" OFF)

add_library(piano_core STATIC
    src/core/Score.cpp
    src/core/PitchDetector.cpp
    src/core/NoteListener.cpp
    src/core/WaitModeFollower.cpp
    src/core/MidiReader.cpp
    src/core/PracticeSession.cpp
    src/core/ChordDetector.cpp
    src/core/NoteTracker.cpp
    src/core/Spectrum.cpp
    src/core/KeyAttacks.cpp
    src/core/KeyFingerprints.cpp
    src/core/KeyMatcher.cpp
    src/core/KeyLearner.cpp
    src/core/Transkun.cpp
)
target_include_directories(piano_core PUBLIC src)

if(MSVC)
    target_compile_options(piano_core PRIVATE /W4)
else()
    target_compile_options(piano_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(PIANO_BUILD_APP)
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        9.0.5
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE
    )
    FetchContent_MakeAvailable(JUCE)

    if(WIN32)
        set(PIANO_WEBVIEW2_VERSION 1.0.3485.44)
        set(PIANO_WEBVIEW2_NUGET "${CMAKE_BINARY_DIR}/nuget")
        set(PIANO_WEBVIEW2_DIR  "${PIANO_WEBVIEW2_NUGET}/Microsoft.Web.WebView2.${PIANO_WEBVIEW2_VERSION}")
        if(NOT EXISTS "${PIANO_WEBVIEW2_DIR}/build/native/include/WebView2.h")
            message(STATUS "Downloading the WebView2 SDK ${PIANO_WEBVIEW2_VERSION}")
            file(DOWNLOAD "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/${PIANO_WEBVIEW2_VERSION}"
                 "${PIANO_WEBVIEW2_NUGET}/webview2.zip" STATUS webview2_status TLS_VERIFY ON)
            list(GET webview2_status 0 webview2_code)
            if(NOT webview2_code EQUAL 0)
                message(FATAL_ERROR "Couldn't download the WebView2 SDK: ${webview2_status}")
            endif()
            file(ARCHIVE_EXTRACT INPUT "${PIANO_WEBVIEW2_NUGET}/webview2.zip" DESTINATION "${PIANO_WEBVIEW2_DIR}")
        endif()
        set(JUCE_WEBVIEW2_PACKAGE_LOCATION "${PIANO_WEBVIEW2_NUGET}")
    endif()

    juce_add_gui_app(PianoTrainer
        PRODUCT_NAME                  "Piano Trainer"
        ICON_BIG                      "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.png"
        COMPANY_NAME                  "PianoTrainer"
        VERSION                       ${PROJECT_VERSION}
        NEEDS_CURL                    FALSE
        NEEDS_WEB_BROWSER             TRUE
        NEEDS_WEBVIEW2                TRUE
        MICROPHONE_PERMISSION_ENABLED TRUE
        MICROPHONE_PERMISSION_TEXT    "Piano Trainer listens to your piano to check the notes you play."
    )

    target_sources(PianoTrainer PRIVATE
        src/app/Main.cpp
        src/app/AppShell.cpp
        src/app/AppSettings.cpp
        src/app/Library.cpp
        src/app/WebMenu.cpp
        src/app/PracticeScreen.cpp
        src/app/PianoPlayer.cpp
        src/app/SamplePiano.cpp
        src/app/BuiltInPiano.cpp
        src/app/Embedded.cpp
        src/app/DarkTitleBar.cpp
        src/app/DetectionLog.cpp
        src/app/AudioEngine.cpp
        src/app/Theme.cpp
        src/app/Icons.cpp
        src/app/PianoLookAndFeel.cpp
        src/app/Widgets.cpp
        src/app/TopBar.cpp
        src/app/BottomBar.cpp
        src/app/PianoRollView.cpp
        src/app/KeyboardView.cpp
        src/app/TranskunModel.cpp
        src/app/FingerprintLearner.cpp
    )

    juce_add_binary_data(PianoTrainerFonts
        NAMESPACE Fonts
        HEADER_NAME Fonts.h
        SOURCES
            assets/fonts/Manrope-Medium.ttf
            assets/fonts/Manrope-SemiBold.ttf
            assets/fonts/Manrope-Bold.ttf
            assets/fonts/Manrope-ExtraBold.ttf
            assets/fonts/JetBrainsMono-Medium.ttf
            assets/fonts/JetBrainsMono-Bold.ttf
    )

    juce_add_binary_data(PianoTrainerWebUI
        NAMESPACE WebUI
        HEADER_NAME WebUI.h
        SOURCES
            ui/dist/index.html
    )

    target_compile_definitions(PianoTrainer PRIVATE
        JUCE_WEB_BROWSER=1
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
        PIANO_UI_DEV_SERVER=$<BOOL:${PIANO_UI_DEV_SERVER}>
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:PianoTrainer,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:PianoTrainer,JUCE_VERSION>"
    )

    target_link_libraries(PianoTrainer
        PRIVATE
            piano_core
            PianoTrainerFonts
            PianoTrainerWebUI
            juce::juce_audio_utils
            juce::juce_gui_extra
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags
    )

    option(PIANO_KEY_LEARNING "Learn the keyboard's sound from playing (ONNX Runtime)" ON)
    if(PIANO_KEY_LEARNING AND WIN32)
        set(PIANO_ORT_VERSION 1.20.1)
        set(PIANO_ORT_ROOT "${CMAKE_BINARY_DIR}/onnxruntime")
        set(PIANO_ORT_DIR  "${PIANO_ORT_ROOT}/onnxruntime-win-x64-${PIANO_ORT_VERSION}")
        if(NOT EXISTS "${PIANO_ORT_DIR}/include/onnxruntime_cxx_api.h")
            message(STATUS "Downloading ONNX Runtime ${PIANO_ORT_VERSION}")
            file(DOWNLOAD "https://github.com/microsoft/onnxruntime/releases/download/v${PIANO_ORT_VERSION}/onnxruntime-win-x64-${PIANO_ORT_VERSION}.zip"
                 "${PIANO_ORT_ROOT}/onnxruntime.zip" STATUS ort_status TLS_VERIFY ON)
            list(GET ort_status 0 ort_code)
            if(NOT ort_code EQUAL 0)
                message(FATAL_ERROR "Couldn't download ONNX Runtime: ${ort_status} (or configure with -DPIANO_KEY_LEARNING=OFF)")
            endif()
            file(ARCHIVE_EXTRACT INPUT "${PIANO_ORT_ROOT}/onnxruntime.zip" DESTINATION "${PIANO_ORT_ROOT}")
        endif()
        target_include_directories(PianoTrainer PRIVATE "${PIANO_ORT_DIR}/include")
        target_link_libraries(PianoTrainer PRIVATE "${PIANO_ORT_DIR}/lib/onnxruntime.lib")
        target_link_libraries(PianoTrainer PRIVATE delayimp)
        target_link_options(PianoTrainer PRIVATE "/DELAYLOAD:onnxruntime.dll")
        target_compile_definitions(PianoTrainer PRIVATE
            PIANO_HAS_ONNXRUNTIME=1
            PIANO_EMBEDDED_ONNXRUNTIME=1
            ORT_API_MANUAL_INIT
            PIANO_ORT_VERSION_STRING="${PIANO_ORT_VERSION}")
        list(APPEND piano_resources "ONNXRUNTIME_DLL RCDATA \"${PIANO_ORT_DIR}/lib/onnxruntime.dll\"")
        list(APPEND piano_resource_files "${PIANO_ORT_DIR}/lib/onnxruntime.dll")

        set(PIANO_MODEL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/transkun")
        if(EXISTS "${PIANO_MODEL_DIR}/transkun.onnx")
            list(APPEND piano_resources
                "TRANSKUN_ONNX RCDATA \"${PIANO_MODEL_DIR}/transkun.onnx\""
                "TRANSKUN_MELS RCDATA \"${PIANO_MODEL_DIR}/freq2mels.f32\""
                "TRANSKUN_WINDOWS RCDATA \"${PIANO_MODEL_DIR}/windows.f32\"")
            list(APPEND piano_resource_files
                "${PIANO_MODEL_DIR}/transkun.onnx"
                "${PIANO_MODEL_DIR}/freq2mels.f32"
                "${PIANO_MODEL_DIR}/windows.f32")
        endif()
    endif()

    set(PIANO_PIECES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/pieces")
    set(PIANO_PIECES_EXCLUDE
        "Offenbach - Can-can.mid"
        "Hans Zimmer - Interstellar.mid")
    if(WIN32 AND EXISTS "${PIANO_PIECES_DIR}")
        set(pieces_stage "${CMAKE_CURRENT_BINARY_DIR}/BuiltInPieces")
        set(pieces_zip "${CMAKE_CURRENT_BINARY_DIR}/BuiltInPieces.zip")
        file(GLOB pieces_files CONFIGURE_DEPENDS RELATIVE "${PIANO_PIECES_DIR}" "${PIANO_PIECES_DIR}/*.mid" "${PIANO_PIECES_DIR}/*.midi")
        set(pieces_names "")
        set(pieces_depends "")
        foreach(piece IN LISTS pieces_files)
            if(piece MATCHES "^[ -~]+$" AND NOT piece IN_LIST PIANO_PIECES_EXCLUDE)
                list(APPEND pieces_names "${piece}")
                list(APPEND pieces_depends "${PIANO_PIECES_DIR}/${piece}")
            endif()
        endforeach()
    endif()
    if(WIN32 AND pieces_names)
        add_custom_command(OUTPUT "${pieces_zip}"
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${pieces_stage}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${pieces_stage}/pieces"
            COMMAND ${CMAKE_COMMAND} -E copy ${pieces_names} "${pieces_stage}/pieces"
            COMMAND ${CMAKE_COMMAND} -E chdir "${pieces_stage}" ${CMAKE_COMMAND} -E tar cf "${pieces_zip}" --format=zip pieces
            WORKING_DIRECTORY "${PIANO_PIECES_DIR}"
            DEPENDS ${pieces_depends}
            VERBATIM)
        list(APPEND piano_resources "PIANO_PIECES RCDATA \"${pieces_zip}\"")
        list(APPEND piano_resource_files "${pieces_zip}")
    endif()

    if(WIN32 AND piano_resources)
        set(resources_rc "${CMAKE_CURRENT_BINARY_DIR}/PianoResources.rc")
        list(JOIN piano_resources "\n" resources_text)
        file(CONFIGURE OUTPUT "${resources_rc}" CONTENT "${resources_text}\n")
        set_source_files_properties("${resources_rc}" PROPERTIES OBJECT_DEPENDS "${piano_resource_files}")
        target_sources(PianoTrainer PRIVATE "${resources_rc}")
    endif()

    set(PIANO_BUILT_IN_SOUND "${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/SalamanderGrandPiano")
    if(WIN32 AND EXISTS "${PIANO_BUILT_IN_SOUND}")
        set(piano_zip "${CMAKE_CURRENT_BINARY_DIR}/BuiltInPiano.zip")
        set(piano_rc "${CMAKE_CURRENT_BINARY_DIR}/BuiltInPiano.rc")
        file(GLOB_RECURSE piano_files CONFIGURE_DEPENDS "${PIANO_BUILT_IN_SOUND}/*")
        get_filename_component(piano_parent "${PIANO_BUILT_IN_SOUND}" DIRECTORY)
        get_filename_component(piano_name "${PIANO_BUILT_IN_SOUND}" NAME)
        add_custom_command(OUTPUT "${piano_zip}"
            COMMAND ${CMAKE_COMMAND} -E tar cf "${piano_zip}" --format=zip "${piano_name}"
            WORKING_DIRECTORY "${piano_parent}"
            DEPENDS ${piano_files}
            VERBATIM)
        file(CONFIGURE OUTPUT "${piano_rc}" CONTENT "PIANO_SAMPLES RCDATA \"${piano_zip}\"\n")
        set_source_files_properties("${piano_rc}" PROPERTIES OBJECT_DEPENDS "${piano_zip}")
        target_sources(PianoTrainer PRIVATE "${piano_rc}" "${piano_zip}")
        target_compile_definitions(PianoTrainer PRIVATE PIANO_BUILT_IN_PIANO=1)
    endif()

    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT PianoTrainer)

    find_program(NPM_EXECUTABLE NAMES npm.cmd npm)
    if(NPM_EXECUTABLE)
        add_custom_target(webui
            COMMAND "${NPM_EXECUTABLE}" install
            COMMAND "${NPM_EXECUTABLE}" run build
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/ui"
            COMMENT "Building the web UI"
            USES_TERMINAL)
    endif()
endif()

if(PIANO_BUILD_TESTS)
    enable_testing()
    add_executable(piano_tests tests/test_core.cpp)
    target_link_libraries(piano_tests PRIVATE piano_core)
    target_compile_definitions(piano_tests PRIVATE PIANO_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
    add_test(NAME core COMMAND piano_tests)
endif()

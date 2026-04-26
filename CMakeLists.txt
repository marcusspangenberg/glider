cmake_minimum_required(VERSION 3.24)
project(glider)

set(CMAKE_CXX_STANDARD 20)

find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)
find_package(SDL2_mixer CONFIG REQUIRED)

include(FetchContent)
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9b)
FetchContent_MakeAvailable(imgui)

add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer2.cpp)
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui_lib PUBLIC SDL2::SDL2)

add_executable(glider
    main.cpp
    HouseReader.h
    HouseData.h
    ObjectType.h
    Resources.cpp
    Resources.h
    Colors.h
    HouseReader.cpp
    Renderer.cpp
    Renderer.h
    GameState.h
    Constants.h
    PlayerState.h
    Rect.h
    Game.cpp
    Game.h
    FileBrowser.cpp
    FileBrowser.h
    Preferences.cpp
    Preferences.h
    TitleScreen.cpp
    TitleScreen.h
    SoundResources.cpp
    SoundResources.h
)
target_link_libraries(glider PRIVATE
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
    $<IF:$<TARGET_EXISTS:SDL2_image::SDL2_image>,SDL2_image::SDL2_image,SDL2_image::SDL2_image-static>
    $<IF:$<TARGET_EXISTS:SDL2_mixer::SDL2_mixer>,SDL2_mixer::SDL2_mixer,SDL2_mixer::SDL2_mixer-static>
    imgui_lib
)
if(NOT APPLE)
    configure_file(fonts/glyphs_sans.png ${CMAKE_BINARY_DIR}/fonts/glyphs_sans.png COPYONLY)
    file(COPY sounds DESTINATION ${CMAKE_BINARY_DIR})
    file(COPY images DESTINATION ${CMAKE_BINARY_DIR})
    file(COPY "The House" "The House 2" DESTINATION ${CMAKE_BINARY_DIR})
endif()

if(NOT APPLE)
    install(TARGETS glider RUNTIME DESTINATION .)
    install(DIRECTORY fonts images sounds DESTINATION .)
    install(FILES "The House" "The House 2" DESTINATION .)
endif()

if(WIN32)
    set_target_properties(glider PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

add_executable(glidereditor
    editor/main_editor.cpp
    editor/Editor.cpp
    editor/Editor.h
    FileBrowser.cpp
    FileBrowser.h
    editor/ProcessLauncher.cpp
    editor/ProcessLauncher.h
    HouseWriter.cpp
    HouseWriter.h
    HouseReader.cpp
    HouseReader.h
    HouseData.h
    ObjectType.h
    Renderer.cpp
    Renderer.h
    Resources.cpp
    Resources.h
    GameState.h
    PlayerState.h
    Rect.h
    Constants.h
    Colors.h
    WindowGeometry.h
    GameOverState.h
    Random.h
)
target_include_directories(glidereditor PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(glidereditor PRIVATE
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
    $<IF:$<TARGET_EXISTS:SDL2_image::SDL2_image>,SDL2_image::SDL2_image,SDL2_image::SDL2_image-static>
    imgui_lib
)

if(WIN32)
    set_target_properties(glidereditor PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

if(APPLE)
    set(ICNS_FILE "${CMAKE_SOURCE_DIR}/macos/glider.icns")

    set_target_properties(glider PROPERTIES
        OUTPUT_NAME "Glider"
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Glider"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
        MACOSX_BUNDLE_ICON_FILE "glider.icns"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.glider.glider"
        XCODE_GENERATE_SCHEME TRUE
    )
    add_custom_command(TARGET glider POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            "${ICNS_FILE}"
            "$<TARGET_BUNDLE_CONTENT_DIR:glider>/Resources/glider.icns"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/images"
            "$<TARGET_BUNDLE_CONTENT_DIR:glider>/Resources/images"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/fonts"
            "$<TARGET_BUNDLE_CONTENT_DIR:glider>/Resources/fonts"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/sounds"
            "$<TARGET_BUNDLE_CONTENT_DIR:glider>/Resources/sounds"
        COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/The House"
            "$<TARGET_BUNDLE_CONTENT_DIR:glider>/Resources/The House"
        COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/The House 2"
            "$<TARGET_BUNDLE_CONTENT_DIR:glider>/Resources/The House 2"
        COMMAND xattr -cr "$<TARGET_BUNDLE_DIR:glider>"
        COMMENT "Populating Glider.app bundle"
    )

    set_target_properties(glidereditor PROPERTIES
        OUTPUT_NAME "GliderEditor"
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Glider Editor"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
        MACOSX_BUNDLE_ICON_FILE "glider.icns"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.glider.editor"
        XCODE_GENERATE_SCHEME TRUE
    )
    add_custom_command(TARGET glidereditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            "${ICNS_FILE}"
            "$<TARGET_BUNDLE_CONTENT_DIR:glidereditor>/Resources/glider.icns"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/images"
            "$<TARGET_BUNDLE_CONTENT_DIR:glidereditor>/Resources/images"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/fonts"
            "$<TARGET_BUNDLE_CONTENT_DIR:glidereditor>/Resources/fonts"
        COMMAND xattr -cr "$<TARGET_BUNDLE_DIR:glidereditor>"
        COMMENT "Populating GliderEditor.app bundle"
    )
endif()

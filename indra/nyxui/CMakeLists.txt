project(nyxui)

include(00-Common)
include(LLCommon)

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)

# The Nyx interface shell: Dear ImGui drawn through LL's renderer (gGL and the UI shader), so it
# runs on whichever backend the viewer is using.
add_library(nyxui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    nyxui_gl.cpp
    nyxui_input.cpp
    nyxui_theme.cpp
    nyxui_widgets.cpp
    nyxui_fx.cpp
    nyxui_images.cpp
    nyxui_dock.cpp
    nyxui_shell.cpp
    nyxui_palette.cpp
    nyxui.h
    nyxui_dock.h
)

target_include_directories(nyxui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${IMGUI_DIR})
target_compile_definitions(nyxui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS IMGUI_USE_WCHAR32 IMGUI_DEFINE_MATH_OPERATORS)
target_link_libraries(nyxui PUBLIC llrender llwindow llxml llcommon llmath)

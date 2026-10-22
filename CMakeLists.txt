cmake_minimum_required(VERSION 3.21)
project(conV VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets DBus)

add_library(conv_core
    src/core/converter.cpp
    src/core/icon.cpp
    src/core/storage.cpp
    src/core/theme.cpp
    src/core/theme_source.cpp
)
target_include_directories(conv_core PUBLIC src)
target_link_libraries(conv_core PUBLIC Qt6::Core Qt6::Gui)

add_library(conv_ui
    src/ui/convert_page.cpp
    src/ui/desktop_file_picker.cpp
    src/ui/modal.cpp
    src/ui/modal_backdrop.cpp
    src/ui/style.cpp
    src/ui/theme_watcher.cpp
    src/ui/widgets.cpp
    src/ui/window.cpp
)
target_link_libraries(conv_ui PUBLIC conv_core Qt6::Widgets Qt6::DBus)

add_executable(conV src/main.cpp src/cli.cpp)
target_link_libraries(conV PRIVATE conv_ui)

foreach(target conv_core conv_ui conV)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
endforeach()

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(core_tests tests/core_tests.cpp)
    target_link_libraries(core_tests PRIVATE conv_core Qt6::Test)
    add_test(NAME core COMMAND core_tests)
    add_executable(ui_tests tests/ui_tests.cpp)
    target_link_libraries(ui_tests PRIVATE conv_ui Qt6::Test)
    add_test(NAME ui COMMAND ui_tests)
    set_tests_properties(ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME cli COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_test.sh $<TARGET_FILE:conV>)
endif()

include(GNUInstallDirs)
install(TARGETS conV RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES assets/io.github.conv.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES assets/io.github.conv.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)

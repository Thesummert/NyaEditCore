cmake_minimum_required(VERSION 3.21)
project(NyaEditCore VERSION 0.1.0 LANGUAGES CXX)

if(NOT DEFINED QT_VERSION_MAJOR)
    find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
endif()
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)
find_package(ICU 60 REQUIRED COMPONENTS uc i18n)

add_library(nya_core STATIC
    AppStorage.cpp AppStorage.h
    Encoding.cpp Encoding.h
    FileIO.cpp FileIO.h
    FileSearch.cpp FileSearch.h
    LspClient.cpp LspClient.h
    Search.cpp Search.h
    TreeSitter.cpp TreeSitter.h)
target_compile_features(nya_core PUBLIC cxx_std_17)
set_target_properties(nya_core PROPERTIES AUTOMOC ON CXX_EXTENSIONS OFF)
target_include_directories(nya_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(nya_core PUBLIC Qt${QT_VERSION_MAJOR}::Core ICU::uc ICU::i18n)
if(MSVC)
    target_compile_options(nya_core PRIVATE /W4 /utf-8)
else()
    target_compile_options(nya_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

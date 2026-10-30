find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)

add_executable(nya_tests CoreTests.cpp)
add_executable(nya_storage_tests StorageTests.cpp)
add_executable(nya_tree_sitter_tests TreeSitterTests.cpp)
foreach(test_target IN ITEMS nya_tests nya_storage_tests nya_tree_sitter_tests)
    target_link_libraries(${test_target} PRIVATE nya_core Qt${QT_VERSION_MAJOR}::Test)
    # UI integration tests also invoke the Tree-sitter fixture beside their executable.
    set_target_properties(${test_target} PROPERTIES
        AUTOMOC ON
        CXX_EXTENSIONS OFF
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
    if(MSVC)
        target_compile_options(${test_target} PRIVATE /utf-8)
    endif()
    add_test(NAME ${test_target} COMMAND ${test_target})
    set_tests_properties(${test_target} PROPERTIES TIMEOUT 90)
endforeach()

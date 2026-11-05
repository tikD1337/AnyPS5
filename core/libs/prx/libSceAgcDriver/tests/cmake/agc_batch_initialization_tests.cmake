add_test_executable(agc_batch_initialization_tests EXCLUDE_FROM_ALL tests/BatchInitialization.cpp)

target_include_directories(agc_batch_initialization_tests PRIVATE ${LIBS_INCLUDE_DIR})
set_target_properties(agc_batch_initialization_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_host_test_executable(agc_batch_initialization_tests)

if(BUILD_TESTING)
    set_target_properties(agc_batch_initialization_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_batch_initialization COMMAND agc_batch_initialization_tests)
    set_tests_properties(agc_batch_initialization PROPERTIES TIMEOUT 30)
endif()

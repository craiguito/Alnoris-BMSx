set(_cad_regression_test_candidates
    "${TEST_BINARY_DIR}/tests-bin/cad_regression_tests.exe"
    "${TEST_BINARY_DIR}/Debug/cad_regression_tests.exe"
    "${TEST_BINARY_DIR}/Release/cad_regression_tests.exe"
    "${TEST_BINARY_DIR}/RelWithDebInfo/cad_regression_tests.exe"
    "${TEST_BINARY_DIR}/MinSizeRel/cad_regression_tests.exe"
)

set(_cad_regression_test_executable "")
foreach(_cad_regression_test_candidate IN LISTS _cad_regression_test_candidates)
    if (EXISTS "${_cad_regression_test_candidate}")
        set(_cad_regression_test_executable "${_cad_regression_test_candidate}")
        break()
    endif()
endforeach()

if (_cad_regression_test_executable STREQUAL "")
    message(FATAL_ERROR "Unable to locate cad_regression_tests.exe under ${TEST_BINARY_DIR}")
endif()

execute_process(
    COMMAND "${_cad_regression_test_executable}"
    WORKING_DIRECTORY "${TEST_BINARY_DIR}"
    RESULT_VARIABLE _cad_regression_test_result
    OUTPUT_VARIABLE _cad_regression_test_stdout
    ERROR_VARIABLE _cad_regression_test_stderr
)

if (NOT _cad_regression_test_stdout STREQUAL "")
    message("${_cad_regression_test_stdout}")
endif()

if (NOT _cad_regression_test_stderr STREQUAL "")
    message("${_cad_regression_test_stderr}")
endif()

if (NOT _cad_regression_test_result EQUAL 0)
    message(FATAL_ERROR "cad_regression_tests exited with code ${_cad_regression_test_result}")
endif()

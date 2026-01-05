# This script runs Minifier 4 and compares the output with expected manually written gold-standards.
# Usage: cmake -DEXE=... -DIN=... -DOUT_DIR=... -DOUT_NAME=... -DEXP=... -P test_runner.cmake.

# 1. Convert paths to Native Windows style.
file(TO_NATIVE_PATH "${IN}"      IN_NATIVE)
file(TO_NATIVE_PATH "${OUT_DIR}" OUT_DIR_NATIVE)
file(TO_NATIVE_PATH "${EXE}"     EXE_NATIVE)

# 2. Construct the Expected Filename (input_min.ext).
get_filename_component(BASENAME "${OUT_NAME}" NAME_WE)
get_filename_component(EXTENSION "${OUT_NAME}" EXT)
set(GENERATED_FILE "${OUT_DIR}/${BASENAME}_min${EXTENSION}")

# 3. Clean previous run
file(REMOVE "${GENERATED_FILE}")

# 4. Run Minifier
#    Flags: -hd (Headless/No GUI), -d (Defaults/Run), -i (Input), -op (Output Path).
#    We use WORKING_DIRECTORY to ensure safe relative path resolution if the app relies on it.
execute_process(
    COMMAND "${EXE_NATIVE}" -hd -d -i "${IN_NATIVE}" -op "${OUT_DIR_NATIVE}"
    WORKING_DIRECTORY "${OUT_DIR}"
    RESULT_VARIABLE RUN_RESULT
    OUTPUT_VARIABLE APP_OUT
    ERROR_VARIABLE APP_ERR
)

# 5. Check for Crash (Non-zero exit code).
if(NOT RUN_RESULT EQUAL 0)
    message(FATAL_ERROR "Execution failed (Exit Code: ${RUN_RESULT})\nOutput: ${APP_OUT}\nError: ${APP_ERR}")
endif()

# 6. Check for Missing Output (Zero exit code but failed logic).
if(NOT EXISTS "${GENERATED_FILE}")
    message(STATUS "========== APP STDOUT ==========")
    message(STATUS "${APP_OUT}")
    message(STATUS "========== APP STDERR ==========")
    message(STATUS "${APP_ERR}")
    message(STATUS "================================")
    message(FATAL_ERROR "Output file was not created at: ${GENERATED_FILE}")
endif()

# 7. Compare Content.
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${GENERATED_FILE}" "${EXP}"
    RESULT_VARIABLE CMP_RESULT
)

if(NOT CMP_RESULT EQUAL 0)
    message(FATAL_ERROR "Validation Failed: Content mismatch.\nGenerated: ${GENERATED_FILE}\nExpected:  ${EXP}")
endif()
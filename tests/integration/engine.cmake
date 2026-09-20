string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef run_id)
set(root "${WORK}/${run_id}")
file(MAKE_DIRECTORY "${root}")
execute_process(COMMAND "${BUILDER}" build --source "${SOURCE}" --output "${root}/bundle"
    RESULT_VARIABLE result OUTPUT_QUIET ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Cannot build real dictionary: ${error}")
endif()
file(WRITE "${root}/commands.txt" "12125145154121\npick 1\nfail\npick 1\nok\nquit\n")
execute_process(COMMAND "${CONSOLE}" "${root}/bundle/dictionary.sidx"
    INPUT_FILE "${root}/commands.txt" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error ENCODING UTF-8)
if(NOT result EQUAL 0 OR output MATCHES "ERROR" OR NOT output MATCHES "臺 \\[exact\\]" OR
   NOT output MATCHES "SIMULATED_FAILURE composition preserved" OR NOT output MATCHES "SIMULATED_COMMIT 臺" OR
   NOT output MATCHES "STATE idle revision=[0-9]+ strokes= total=0")
    message(FATAL_ERROR "Real engine flow failed: ${output}\n${error}")
endif()
file(WRITE "${root}/transcript.txt" "${output}")
message(STATUS "Real Conway lookup, failed commit recovery and successful commit passed")

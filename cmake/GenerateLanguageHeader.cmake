function(generate_language_header JSON_FILE HEADER_FILE VAR_NAME)
    if(NOT EXISTS "${JSON_FILE}")
        message(FATAL_ERROR "Language JSON not found: ${JSON_FILE}")
    endif()

    file(READ "${JSON_FILE}" JSON_CONTENT)

    string(REPLACE "\\" "\\\\" JSON_CONTENT "${JSON_CONTENT}")
    string(REPLACE "\"" "\\\"" JSON_CONTENT "${JSON_CONTENT}")
    string(REPLACE "\n" "\\n\"\n\"" JSON_CONTENT "${JSON_CONTENT}")

    file(WRITE "${HEADER_FILE}"
            "#pragma once
#include <stddef.h>

static constexpr char ${VAR_NAME}[] =
\"${JSON_CONTENT}\";
"
    )
endfunction()
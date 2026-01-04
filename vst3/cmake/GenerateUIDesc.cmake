if(NOT DEFINED INPUT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "GenerateUIDesc.cmake requires INPUT and OUTPUT.")
endif()

if(NOT DEFINED PROJECT_VERSION)
  set(PROJECT_VERSION "0.0.0")
endif()

string(TIMESTAMP BUILD_TIME "%Y-%m-%d %H:%M")
set(BEAT_BUILD_STAMP "${PROJECT_VERSION} (${BUILD_TIME})")

configure_file("${INPUT}" "${OUTPUT}" @ONLY)

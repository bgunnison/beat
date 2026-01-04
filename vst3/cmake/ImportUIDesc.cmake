if(NOT DEFINED INPUT_BUNDLE OR NOT DEFINED OUTPUT_TEMPLATE)
  message(FATAL_ERROR "ImportUIDesc.cmake requires INPUT_BUNDLE and OUTPUT_TEMPLATE.")
endif()

if(NOT EXISTS "${INPUT_BUNDLE}")
  message(STATUS "ImportUIDesc: bundle UI not found at ${INPUT_BUNDLE}")
  return()
endif()

file(READ "${INPUT_BUNDLE}" CONTENTS)

# Restore the build stamp placeholder in the title.
string(REGEX REPLACE "title=\"Beat MIDI Generator v[^\"]*\"" "title=\"Beat MIDI Generator v@BEAT_BUILD_STAMP@\"" CONTENTS "${CONTENTS}")

file(WRITE "${OUTPUT_TEMPLATE}" "${CONTENTS}")
message(STATUS "ImportUIDesc: updated ${OUTPUT_TEMPLATE} from ${INPUT_BUNDLE}")

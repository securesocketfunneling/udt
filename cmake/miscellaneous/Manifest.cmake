# Create a manifest.json.in file based on manifest.json.in in your
# OUTPUT_DIR/manifest.json
#
# This is the kind of output you can get:
#
# {
#   "project": "your_project",
#   "version": "1.0rc3-15",
#   "commit": "31c1735b9f681a899ca449f732034ed6198d461c"
# }
#
# This file can be included in any package/resources...

if(__create_manifest)
	return()
endif()
set(__create_manifest YES)

set(CURRENT_SCRIPT_DIR ${CMAKE_CURRENT_LIST_DIR})

function(create_manifest OUTPUT_DIR PROJECT_NAME VERSION COMMIT)
  # create the project's manifest with COMMIT and VERSION
  configure_file("${CURRENT_SCRIPT_DIR}/manifest.json.in"
    "${OUTPUT_DIR}/manifest.json" @ONLY)
endfunction()

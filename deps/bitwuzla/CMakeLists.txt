include(ExternalProject)

set (BITWUZLA_DEP       "${CMAKE_SOURCE_DIR}/deps/bitwuzla")
set (BITWUZLA_BASE      "${CMAKE_BINARY_DIR}/bitwuzla")
set (BITWUZLA_DOWNLOAD  "${BITWUZLA_BASE}/download")
set (BITWUZLA_BUILD     "${BITWUZLA_DOWNLOAD}/build")
set (BITWUZLA_INCLUDES  "${BITWUZLA_BASE}/include")
set (BITWUZLA_LIB       "${BITWUZLA_BASE}/lib/libbitwuzla.a")
set (CADICAL_LIB        "${BITWUZLA_DOWNLOAD}/deps/install/lib/libcadical.a")
set (TOR2PARSER_LIB     "${BITWUZLA_DOWNLOAD}/deps/install/lib/libbtor2parser.a")
set (HAVE_BITWUZLA TRUE)

message("-- BITWUZLA version 0.9.10 : download and build libbitwuzla.a in ${BITWUZLA_BASE}")

ExternalProject_Add(bitwuzla_extern
  PREFIX "${BITWUZLA_BASE}"
  GIT_REPOSITORY "https://github.com/bitwuzla/bitwuzla"
  GIT_TAG "origin/main"
  GIT_PROGRESS TRUE
  SOURCE_DIR "${BITWUZLA_DOWNLOAD}"
  PATCH_COMMAND "${BITWUZLA_DEP}/run_configure.sh" ${BITWUZLA_DOWNLOAD} ${BITWUZLA_BASE}
  CMAKE_ARGS "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
  BINARY_DIR "${BITWUZLA_BUILD}"
  INSTALL_DIR "${BITWUZLA_BASE}"
  BUILD_BYPRODUCTS "${BITWUZLA_LIB}" "${CADICAL_LIB}" "${TOR2PARSER_LIB}"
  )

add_library ( bitwuzla::bitwuzla   STATIC IMPORTED GLOBAL )
add_library ( bitwuzla::cadical    STATIC IMPORTED GLOBAL )
add_library ( bitwuzla::tor2parser STATIC IMPORTED GLOBAL )

#  INTERFACE_INCLUDE_DIRECTORIES ${BITWUZLA_INCLUDES}

set_target_properties (bitwuzla::bitwuzla   PROPERTIES IMPORTED_LOCATION ${BITWUZLA_LIB} )
set_target_properties (bitwuzla::cadical    PROPERTIES IMPORTED_LOCATION ${CADICAL_LIB})
set_target_properties (bitwuzla::tor2parser PROPERTIES IMPORTED_LOCATION ${TOR2PARSER_LIB})

add_dependencies ( bitwuzla::bitwuzla   bitwuzla_extern )
add_dependencies ( bitwuzla::cadical    bitwuzla_extern )
add_dependencies ( bitwuzla::tor2parser bitwuzla_extern )

mark_as_advanced (
  HAVE_BITWUZLA
  BITWUZLA_LIB
  BITWUZLA_INCLUDES
  )
  


file(GLOB BAMBOO_CORE_GO_SRCS bamboo-core/*.go)
file(GLOB BAMBOO_GO_SRCS *.go)

# Custom command for 'go build -buildmode=c-archive ...'
# to create a library from Go codes.
add_custom_command(OUTPUT bamboo-core.a bamboo-core.h
    DEPENDS ${BAMBOO_GO_SRCS} ${BAMBOO_CORE_GO_SRCS} go.mod
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND env go build -buildmode=c-archive
                -o "${CMAKE_CURRENT_BINARY_DIR}/bamboo-core.a"
            ${BAMBOO_GO_SRCS})
add_custom_target(bamboo-core DEPENDS bamboo-core.a)
# Add a custom target for the library.
add_library(Bamboo::Core UNKNOWN IMPORTED GLOBAL)
add_dependencies(Bamboo::Core bamboo-core)
set_target_properties(Bamboo::Core PROPERTIES
     IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/bamboo-core.a"
     INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_BINARY_DIR}")
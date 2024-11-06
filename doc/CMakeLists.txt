message(STATUS "Processing doc directory...")

find_package(Doxygen REQUIRED)

configure_file(Doxyfile.in Doxyfile)

add_custom_target(
    doc
    COMMAND ${DOXYGEN_EXECUTABLE} Doxyfile
    BYPRODUCTS html Doxyfile
    COMMENT "Generating API documentation with Doxygen..."
    VERBATIM
)

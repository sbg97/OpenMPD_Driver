# Source files
set(SOURCES
    Levitator.cpp
)

# Header files
set(HEADERS
    Levitator.h
)

# Create the executable or library
add_library(Levitator SHARED ${SOURCES} ${HEADERS})

target_link_libraries(Levitator PRIVATE AsierInho_DLLV2)

# Add include directories
target_include_directories( Levitator PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

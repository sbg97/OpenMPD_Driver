set(SOURCES
    src/AsierInho_V2.cpp
    src/AsierInhoImpl_V2.cpp
    src/AsierInho_V2_CWrapper.cpp
)
set(HEADERS
    include/AsierInho_V2.h
    include/AsierInho_V2_CWrapper.h
    include/AsierInho_V2_Prerequisites.h
    include/AsierInhoImpl_V2.h
    include/ParseBoardConfig.h
)

# Create the executable or library
add_library(AsierInho_DLLV2 SHARED ${SOURCES} ${HEADERS})

# Add include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Preprocessor definitions
target_compile_definitions(AsierInho_DLLV2 PRIVATE
    _CRT_SECURE_NO_WARNINGS
    ASIERINHO_V2_NONCLIENT_BUILD
    HAVE_STRUCT_TIMESPEC
    _MBCS
)

# Find the ftd2xx library
find_library(FTD2XX_LIBRARY 
    NAMES ftd2xx
    PATHS /usr/local/lib
    NO_DEFAULT_PATH
)

# Link the library
target_link_libraries(AsierInho_DLLV2
    ${FTD2XX_LIBRARY}
)
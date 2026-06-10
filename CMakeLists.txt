cmake_minimum_required(VERSION 3.10)
project(OpenMPD_Driver)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-O3 -Wall -Wextra")

# Add subdirectories
# add_subdirectory(AsierInho_DLL)
add_subdirectory(AsierInho_DLLV2)
add_subdirectory(GenerateBoardConfigurationFiles)
add_subdirectory(TestTransducers)
add_subdirectory(TestFTDI_Write)
add_subdirectory(Levitator)
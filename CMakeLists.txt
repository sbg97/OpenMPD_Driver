cmake_minimum_required(VERSION 3.10)
project(OpenMPD_Driver)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add subdirectories
# add_subdirectory(AsierInho_DLL)
add_subdirectory(AsierInho_DLLV2)
add_subdirectory(Levitator)
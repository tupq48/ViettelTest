cmake_minimum_required(VERSION 3.10)
project(ViettelTest)

# Set C standard
set(CMAKE_C_STANDARD 99)

# Include directories
include_directories(include)

# Add executable for AMF
add_executable(amf src/amf/main.c src/amf/AMF.c)

# Add executable for gNodeB
add_executable(gnodeb src/gnodeb/main.c src/gnodeb/gNodeB.c)
target_link_libraries(gnodeb pthread)

# Add executable for UE
add_executable(ue src/ue/main.c src/ue/userEquipment.c)
target_link_libraries(ue pthread)
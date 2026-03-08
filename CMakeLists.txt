cmake_minimum_required(VERSION 3.10)
project(ViettelTest)

# Set C standard
set(CMAKE_C_STANDARD 99)

# Include directories
include_directories(
    include/
    include/gnodeb
    include/worker
    include/amf
    include/ue
    include/config
)


# Source files
# AMF Sources
set(AMF_SOURCES
    src/amf/main.c
    src/amf/AMF.c
)

# gNodeB Sources
set(GNODEB_SOURCES
    src/gnodeb/main.c
    src/gnodeb/gNodeB.c
    src/worker/worker.c
)

# UE Sources
set(UE_SOURCES
    src/ue/main.c
    src/ue/userEquipment.c
)

# --- Executables ---

# Add executable for AMF
add_executable(amf ${AMF_SOURCES})

# Add executable for gNodeB
add_executable(gnodeb ${GNODEB_SOURCES})
target_link_libraries(gnodeb pthread)

# Add executable for UE
add_executable(ue ${UE_SOURCES})
target_link_libraries(ue pthread)
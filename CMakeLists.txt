cmake_minimum_required(VERSION 3.16)

project(parallel_os_utils LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(MPI REQUIRED)
find_package(OpenMP REQUIRED)

set(POU_COMMON_SOURCES
    src/common/Cli.cpp
    src/common/Metrics.cpp
    src/common/MpiWork.cpp
    src/common/StringSearch.cpp
    src/common/Traversal.cpp
)

function(add_pou_common name)
    add_library(${name} ${POU_COMMON_SOURCES})
    target_include_directories(${name} PUBLIC include)
endfunction()

add_pou_common(pou_common_serial)

add_pou_common(pou_common_omp)
target_compile_definitions(pou_common_omp PUBLIC USE_OPENMP)
target_link_libraries(pou_common_omp PUBLIC OpenMP::OpenMP_CXX)

add_pou_common(pou_common_mpi_omp)
target_compile_definitions(pou_common_mpi_omp PUBLIC USE_MPI USE_OPENMP OMPI_SKIP_MPICXX)
target_link_libraries(pou_common_mpi_omp PUBLIC MPI::MPI_CXX OpenMP::OpenMP_CXX)

function(add_pou_tool target source common label)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE ${common})
    target_compile_definitions(${target} PRIVATE TOOL_NAME=\"${label}\")
endfunction()

add_pou_tool(pfind_serial src/tools/pfind.cpp pou_common_serial pfind_serial)
add_pou_tool(pgrep_serial src/tools/pgrep.cpp pou_common_serial pgrep_serial)

add_pou_tool(pfind_omp src/tools/pfind.cpp pou_common_omp pfind_omp)
add_pou_tool(pgrep_omp src/tools/pgrep.cpp pou_common_omp pgrep_omp)

add_pou_tool(pfind src/tools/pfind.cpp pou_common_mpi_omp pfind_mpi_omp)
add_pou_tool(pgrep src/tools/pgrep.cpp pou_common_mpi_omp pgrep_mpi_omp)
add_executable(pfind_mpi_omp ALIAS pfind)
add_executable(pgrep_mpi_omp ALIAS pgrep)

install(TARGETS pfind pgrep pfind_serial pgrep_serial pfind_omp pgrep_omp RUNTIME DESTINATION bin)

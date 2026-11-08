


function(find_cuda)
    if(NOT BPFTIME_CUDA_ROOT)
        message(FATAL_ERROR "To use NV attach, set BPFTIME_CUDA_ROOT to the root of CUDA installation, such as /usr/local/cuda-12.6")
    endif()

    # Detect target platform based on CMAKE_SYSTEM_PROCESSOR
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        # CUDA 12 for Jetson installs targets/aarch64-linux; CUDA 13 installs
        # only targets/sbsa-linux, for Arm servers and Jetson alike.
        if(EXISTS "${BPFTIME_CUDA_ROOT}/targets/aarch64-linux")
            set(CUDA_TARGET_ARCH "aarch64-linux")
        else()
            set(CUDA_TARGET_ARCH "sbsa-linux")
        endif()
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
        set(CUDA_TARGET_ARCH "x86_64-linux")
    else()
        message(WARNING "Unsupported architecture ${CMAKE_SYSTEM_PROCESSOR}, defaulting to x86_64-linux")
        set(CUDA_TARGET_ARCH "x86_64-linux")
    endif()
    set(_CUDA_TARGET_DIR "${BPFTIME_CUDA_ROOT}/targets/${CUDA_TARGET_ARCH}")
    # Fail here, not in the first compile that includes cuda.h.
    if(NOT EXISTS "${_CUDA_TARGET_DIR}/include/cuda.h")
        message(FATAL_ERROR "cuda.h not found in ${_CUDA_TARGET_DIR}/include: "
            "set BPFTIME_CUDA_ROOT (${BPFTIME_CUDA_ROOT}) to a CUDA installation "
            "for ${CMAKE_SYSTEM_PROCESSOR}")
    endif()

    set(CUDA_LIBRARY_PATH ${BPFTIME_CUDA_ROOT}/targets/${CUDA_TARGET_ARCH}/lib/ ${BPFTIME_CUDA_ROOT}/targets/${CUDA_TARGET_ARCH}/lib/stubs/ ${BPFTIME_CUDA_ROOT}/extras/CUPTI/lib64/ PARENT_SCOPE)

    # Detect CUDA version from version.json or version.txt
    if(EXISTS "${BPFTIME_CUDA_ROOT}/version.json")
        file(READ "${BPFTIME_CUDA_ROOT}/version.json" CUDA_VERSION_JSON)
        # Match the cuda section and extract version number (e.g., "12.8.1" -> major=12, minor=8)
        string(REGEX MATCH "\"cuda\"[^{]*\\{[^}]*\"version\"[^\"]*\"([0-9]+)\\.([0-9]+)" _ "${CUDA_VERSION_JSON}")
        set(CUDA_VERSION_MAJOR ${CMAKE_MATCH_1})
        set(CUDA_VERSION_MINOR ${CMAKE_MATCH_2})
    elseif(EXISTS "${BPFTIME_CUDA_ROOT}/version.txt")
        file(READ "${BPFTIME_CUDA_ROOT}/version.txt" CUDA_VERSION_TXT)
        string(REGEX MATCH "CUDA Version ([0-9]+)\\.([0-9]+)" _ "${CUDA_VERSION_TXT}")
        set(CUDA_VERSION_MAJOR ${CMAKE_MATCH_1})
        set(CUDA_VERSION_MINOR ${CMAKE_MATCH_2})
    else()
        # Try to extract from path name as fallback (e.g., cuda-13.0)
        string(REGEX MATCH "cuda-([0-9]+)\\.([0-9]+)" _ "${BPFTIME_CUDA_ROOT}")
        set(CUDA_VERSION_MAJOR ${CMAKE_MATCH_1})
        set(CUDA_VERSION_MINOR ${CMAKE_MATCH_2})
    endif()

    # CUPTI headers: extras/CUPTI/include, a versioned layout, or (CUDA 13)
    # the target's include directory; the first one that has cupti.h.
    set(_CUDA_CUPTI_INCLUDE "${_CUDA_TARGET_DIR}/include")
    foreach(_dir "${BPFTIME_CUDA_ROOT}/extras/CUPTI/include"
            "${BPFTIME_CUDA_ROOT}/cuda-${CUDA_VERSION_MAJOR}.${CUDA_VERSION_MINOR}/extras/CUPTI/include")
        if(EXISTS "${_dir}/cupti.h")
            set(_CUDA_CUPTI_INCLUDE "${_dir}")
            break()
        endif()
    endforeach()
    set(CUDA_INCLUDE_PATH ${BPFTIME_CUDA_ROOT}/targets/${CUDA_TARGET_ARCH}/include ${_CUDA_CUPTI_INCLUDE} PARENT_SCOPE)

    message(STATUS "Detected CUDA version: ${CUDA_VERSION_MAJOR}.${CUDA_VERSION_MINOR}")

    # Check if libcupti_static.a exists, prefer static library
    if(EXISTS "${BPFTIME_CUDA_ROOT}/extras/CUPTI/lib64/libcupti_static.a")
        set(CUDA_CUPTI_LIB "libcupti_static.a")
        message(STATUS "CUDA ${CUDA_VERSION_MAJOR}.${CUDA_VERSION_MINOR}: Using libcupti_static.a")
    else()
        set(CUDA_CUPTI_LIB "cupti")
        message(STATUS "CUDA ${CUDA_VERSION_MAJOR}.${CUDA_VERSION_MINOR}: Using cupti (dynamic library)")
    endif()

    # nvPTXCompiler API is required; prefer static when present.
    find_library(NVPTXCOMPILER_LIB NAMES nvptxcompiler_static nvptxcompiler
        PATHS ${BPFTIME_CUDA_ROOT}/targets/${CUDA_TARGET_ARCH}/lib/ ${BPFTIME_CUDA_ROOT}/lib64 NO_DEFAULT_PATH)
    if(NVPTXCOMPILER_LIB)
        set(CUDA_LIBS cuda cudart ${NVPTXCOMPILER_LIB} ${CUDA_CUPTI_LIB} nvrtc PARENT_SCOPE)
    else()
        message(WARNING "nvptxcompiler library not found; targets using nvPTXCompiler API may fail to link")
        set(CUDA_LIBS cuda cudart ${CUDA_CUPTI_LIB} nvrtc PARENT_SCOPE)
    endif()
endfunction()

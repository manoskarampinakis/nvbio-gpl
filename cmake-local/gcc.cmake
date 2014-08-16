if (CMAKE_COMPILER_IS_GNUCC)
    # common GCC compiler flags
    # we add -Wno-unknown-pragmas because of nvcc's #pragma unroll
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-unknown-pragmas -Wstrict-aliasing=0")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-unknown-pragmas -Wstrict-aliasing=0 -Wno-unused-local-typedefs")
    
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "Intel")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -msse4.2 -mpopcnt")
    endif()

    if(WERROR)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
    endif()

    if(PROFILING)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pg")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
    endif()
endif()


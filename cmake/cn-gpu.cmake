if (WITH_CN_GPU AND CMAKE_SIZEOF_VOID_P EQUAL 8)

    if (XMRIG_ARM)
        set(CN_GPU_SOURCES src/crypto/cn/gpu/cn_gpu_arm.cpp)

        if (CMAKE_CXX_COMPILER_ID MATCHES GNU OR CMAKE_CXX_COMPILER_ID MATCHES Clang)
            set_source_files_properties(src/crypto/cn/gpu/cn_gpu_arm.cpp PROPERTIES COMPILE_FLAGS "-O3")
        endif()
    else()
        set(CN_GPU_SOURCES src/crypto/cn/gpu/cn_gpu_avx.cpp src/crypto/cn/gpu/cn_gpu_ssse3.cpp)

        if (CMAKE_CXX_COMPILER_ID MATCHES GNU OR CMAKE_CXX_COMPILER_ID MATCHES Clang)
            set_source_files_properties(src/crypto/cn/gpu/cn_gpu_avx.cpp PROPERTIES COMPILE_FLAGS "-O3 -mavx2")
            set_source_files_properties(src/crypto/cn/gpu/cn_gpu_ssse3.cpp PROPERTIES COMPILE_FLAGS "-O3")
        elseif (CMAKE_CXX_COMPILER_ID MATCHES MSVC)
            set_source_files_properties(src/crypto/cn/gpu/cn_gpu_avx.cpp PROPERTIES COMPILE_FLAGS "/arch:AVX")
        endif()
    endif()
else()
    set(CN_GPU_SOURCES "")

    add_definitions(/DXMRIG_NO_CN_GPU)
endif()

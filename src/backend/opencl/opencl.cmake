if (WITH_OPENCL)
    add_definitions(/DCL_TARGET_OPENCL_VERSION=200)
    add_definitions(/DCL_USE_DEPRECATED_OPENCL_1_2_APIS)
    add_definitions(/DXMRIG_FEATURE_OPENCL)

    set(HEADERS_BACKEND_OPENCL
        src/backend/opencl/OclBackend.h
        src/backend/opencl/OclConfig.h
        src/backend/opencl/OclLaunchData.h
        src/backend/opencl/OclThread.h
        src/backend/opencl/OclThreads.h
        src/backend/opencl/wrappers/OclDevice.h
        src/backend/opencl/wrappers/OclError.h
        src/backend/opencl/wrappers/OclLib.h
        src/backend/opencl/wrappers/OclPlatform.h
        src/backend/opencl/wrappers/OclVendor.h
       )

    set(SOURCES_BACKEND_OPENCL
        src/backend/opencl/OclBackend.cpp
        src/backend/opencl/OclConfig.cpp
        src/backend/opencl/OclLaunchData.cpp
        src/backend/opencl/OclThread.cpp
        src/backend/opencl/OclThreads.cpp
        src/backend/opencl/wrappers/OclDevice.cpp
        src/backend/opencl/wrappers/OclError.cpp
        src/backend/opencl/wrappers/OclLib.cpp
        src/backend/opencl/wrappers/OclPlatform.cpp
       )
else()
    remove_definitions(/DXMRIG_FEATURE_OPENCL)

    set(HEADERS_BACKEND_OPENCL "")
    set(SOURCES_BACKEND_OPENCL "")
endif()

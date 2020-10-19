add_subdirectory(uriparser2)
add_subdirectory(mio)
add_subdirectory(euca-cutils)
add_subdirectory(fmt)

if (ENABLE_PYCSPOT)
    add_subdirectory(pybind11)
endif()

if (ENABLE_IN_TREE_ZMQ)
    include (FetchContent)
    include (ExternalProject)

    FetchContent_Declare(zeromq
            URL https://github.com/zeromq/libzmq/releases/download/v4.3.3/zeromq-4.3.3.tar.gz
    )

    FetchContent_Declare(czmq
            URL https://github.com/zeromq/czmq/releases/download/v4.2.0/czmq-4.2.0.tar.gz
    )

    FetchContent_MakeAvailable(zeromq czmq)

#    ExternalProject_Add(zeromq
#            URL https://github.com/zeromq/libzmq/releases/download/v4.3.3/zeromq-4.3.3.tar.gz
#            PREFIX ${CMAKE_CURRENT_BINARY_DIR}/zeromq
#            CMAKE_ARGS -DENABLE_WS=OFF -DWITH_LIBBSD=OFF -DBUILD_SHARED=OFF -DWITH_LIBSODIUM=OFF -DENABLE_CURVE=OFF -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/deps
#    )
#
#    # zmq developers do not realize zmq is a C++ project and declare czmq to be a pure C project.
#    # This causes CMake to link czmq binaries with gcc, which _does not_ link libstdc++. This causes
#    # a lot of undefined symbol errors. Therefore, we override the czmq linker command to use g++
#    # rather than gcc.
#
#    if (${CMAKE_C_COMPILER_ID} MATCHES "GNU")
#        set(CZMQ_LINKER_COMMAND "<CMAKE_C_COMPILER> <FLAGS> <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES> -lstdc++")
#    endif()
#
#    ExternalProject_Add(czmq
#            URL https://github.com/zeromq/czmq/releases/download/v4.2.0/czmq-4.2.0.tar.gz
#            PREFIX ${CMAKE_CURRENT_BINARY_DIR}/czmq
#            CMAKE_ARGS -DCMAKE_C_LINK_EXECUTABLE='${CZMQ_LINKER_COMMAND}' -DCZMQ_WITH_UUID=OFF -DCZMQ_WITH_SYSTEMD=OFF -DCZMQ_WITH_LZ4=OFF -DCZMQ_WITH_LIBCURL=OFF -DCZMQ_WITH_NSS=OFF -DCZMQ_WITH_LIBMICROHTTPD=OFF -DCZMQ_BUILD_SHARED=OFF -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/deps
#            DEPENDS zeromq
#    )
endif()

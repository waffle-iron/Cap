FIND_PACKAGE(Git REQUIRED)
IF(GIT_FOUND AND EXISTS ${CMAKE_SOURCE_DIR}/.git/HEAD)
    CONFIGURE_FILE(
        ${CMAKE_SOURCE_DIR}/cmake/QueryGitRevision.cmake.in
        ${CMAKE_BINARY_DIR}/cmake/QueryGitRevision.cmake
        @ONLY
        )
    INCLUDE(${CMAKE_BINARY_DIR}/cmake/QueryGitRevision.cmake)
    SET_PROPERTY(DIRECTORY ${CMAKE_SOURCE_DIR} APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/.git/HEAD)
    ADD_CUSTOM_COMMAND(
        OUTPUT ${CMAKE_BINARY_DIR}/cpp/source/version.cc
        DEPENDS ${CMAKE_SOURCE_DIR}/.git/${GIT_HEAD_REF}
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_BINARY_DIR}/cmake/QueryGitRevision.cmake
        COMMENT "Query git revision"
        )
    ADD_CUSTOM_TARGET(
        version.cc ALL
        DEPENDS ${CMAKE_BINARY_DIR}/cpp/source/version.cc
        )
ENDIF()

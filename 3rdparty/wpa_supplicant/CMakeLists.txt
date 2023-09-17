
# wpa_client
add_library(wpa_client SHARED IMPORTED)
set_target_properties(wpa_client PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_LIST_DIR}/include
    IMPORTED_LOCATION ${CMAKE_CURRENT_LIST_DIR}/lib/libwpa_client.so
)

install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/lib/
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    FILES_MATCHING PATTERN "*.so*"
)

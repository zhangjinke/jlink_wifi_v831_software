project(zlog LANGUAGES C)

set(ZLOG_SRC_FILES_C
    buf.c
    category.c
    category_table.c
    conf.c
    event.c
    format.c
    level.c
    level_list.c
    mdc.c
    record.c
    record_table.c
    rotater.c
    rule.c
    spec.c
    thread.c
    zc_arraylist.c
    zc_hashtable.c
    zc_profile.c
    zc_util.c
    zlog.c
    zlog-chk-conf.c
)
add_library(zlog SHARED ${ZLOG_SRC_FILES_C})

target_include_directories(zlog INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
# set_target_properties(zlog PROPERTIES PUBLIC_HEADER zlog.h)

install(TARGETS zlog
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    # PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

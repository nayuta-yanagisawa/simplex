SET(SIMPLEX_SOURCES ha_simplex.cc lock_handler.cc)
MYSQL_ADD_PLUGIN(simplex ${SIMPLEX_SOURCES} STORAGE_ENGINE MODULE_ONLY
                 COMPONENT simplex-engine)

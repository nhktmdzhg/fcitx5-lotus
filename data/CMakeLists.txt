install(DIRECTORY icons/scalable DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor"
        PATTERN .* EXCLUDE
        PATTERN *~ EXCLUDE)

install(FILES dictionaries/vietnamese.cm.dict DESTINATION "${FCITX_INSTALL_PKGDATADIR}/vmk")

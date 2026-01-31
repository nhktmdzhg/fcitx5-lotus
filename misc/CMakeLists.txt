install(FILES fcitx5-vmk-server@.service
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/systemd/system)

install(FILES 99-vmk.rules
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/udev/rules.d)

install(FILES module-fcitx5-vmk.conf
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/modules-load.d)

install(FILES user-vmk.conf
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/sysusers.d)

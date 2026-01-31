set(fcitx_vmk_sources
  vmk.cpp
)

add_fcitx5_addon(vmk ${fcitx_vmk_sources})
target_link_libraries(vmk
	Fcitx5::Core
	Fcitx5::Config
	Bamboo::Core
	Pthread::Pthread
	X11::X11
)

target_include_directories(vmk PRIVATE ${PROJECT_BINARY_DIR})
install(TARGETS vmk DESTINATION "${CMAKE_INSTALL_LIBDIR}/fcitx5")

fcitx5_translate_desktop_file(vmk.conf.in vmk.conf)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/vmk.conf" DESTINATION "${CMAKE_INSTALL_DATADIR}/fcitx5/inputmethod")
configure_file(vmk-addon.conf.in.in vmk-addon.conf.in)
fcitx5_translate_desktop_file("${CMAKE_CURRENT_BINARY_DIR}/vmk-addon.conf.in" vmk-addon.conf)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/vmk-addon.conf" RENAME vmk.conf DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon")
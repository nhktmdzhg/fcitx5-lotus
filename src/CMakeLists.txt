set(fcitx_lotus_sources
  lotus.cpp
  lotus-engine.cpp
  lotus-state.cpp
  lotus-candidates.cpp
  lotus-utils.cpp
  lotus-monitor.cpp
)

add_library(lotus MODULE ${fcitx_lotus_sources})
set_target_properties(lotus PROPERTIES OUTPUT_NAME "lotus")
target_link_libraries(lotus
	Fcitx5::Core
	Fcitx5::Config
	Bamboo::Core
	Pthread::Pthread
	X11::X11
)

target_include_directories(lotus PRIVATE
    ${PROJECT_BINARY_DIR}
    ${PROJECT_SOURCE_DIR}/data/emoji
)
install(TARGETS lotus DESTINATION "${CMAKE_INSTALL_LIBDIR}/fcitx5")

fcitx5_translate_desktop_file(lotus.conf.in lotus.conf)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/lotus.conf" DESTINATION "${CMAKE_INSTALL_DATADIR}/fcitx5/inputmethod")
configure_file(lotus-addon.conf.in.in lotus-addon.conf.in)
fcitx5_translate_desktop_file("${CMAKE_CURRENT_BINARY_DIR}/lotus-addon.conf.in" lotus-addon.conf)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/lotus-addon.conf" RENAME lotus.conf DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon")
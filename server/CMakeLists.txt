find_package(PkgConfig REQUIRED)

pkg_check_modules(LIBINPUT REQUIRED IMPORTED_TARGET libinput)
pkg_check_modules(LIBUDEV  REQUIRED IMPORTED_TARGET libudev)

add_executable(fcitx5-vmk-server vmk-server.cpp)

target_link_libraries(fcitx5-vmk-server
    Fcitx5::Core
    Fcitx5::Config
    Bamboo::Core
    Pthread::Pthread
    X11::X11
    PkgConfig::LIBINPUT
    PkgConfig::LIBUDEV
)

install(TARGETS fcitx5-vmk-server DESTINATION ${CMAKE_INSTALL_BINDIR})

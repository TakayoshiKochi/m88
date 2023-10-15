cmake_minimum_required(VERSION 3.20)

project(m88 C CXX)

# MSVC or Ninja on Windows
if (WIN32)
    set(LIB_TYPE STATIC)
    include_directories(${CMAKE_SOURCE_DIR}/third_party/gtest/googletest/include)
    link_directories(${CMAKE_SOURCE_DIR}/third_party/gtest/lib/x64)
endif (WIN32)

if (MSVC)
    set(CMAKE_CXX_STANDARD 20)
    add_compile_definitions(NOMINMAX)
    add_compile_definitions(STRICT)
    add_compile_definitions(WIN32_LEAN_AND_MEAN)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/utf-8)
    #add_compile_options(/source-charset:utf-8)

    # https://gitlab.kitware.com/cmake/community/wikis/FAQ#how-can-i-build-my-msvc-application-with-a-static-runtime
    string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_DEBUG          ${CMAKE_CXX_FLAGS_DEBUG})
    string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_MINSIZEREL     ${CMAKE_CXX_FLAGS_MINSIZEREL})
    string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELEASE        ${CMAKE_CXX_FLAGS_RELEASE})
    string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_CXX_FLAGS_RELWITHDEBINFO})
endif (MSVC)

set(Z80_SHARED_LIBS NO  CACHE BOOL "")
set(Z80_WITH_EXECUTE YES CACHE BOOL "")
set(Z80_WITH_FULL_IM0 YES CACHE BOOL "")
set(Z80_WITH_Q YES CACHE BOOL "")
set(Z80_WITH_IM0_RETX_NOTIFICATIONS YES CACHE BOOL "")
set(Z80_WITH_ZILOG_NMOS_LD_A_IR_BUG YES CACHE BOOL "")

#add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/abseil)
#add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/glog)
add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/gtest)
add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/Z80)
add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/Zeta)

add_library(common ${LIB_TYPE}
        src/common/bmp_codec.h
        src/common/bmp_codec.cpp
        src/common/device.h
        src/common/device.cpp
        src/common/diag.h
        src/common/diag.cpp
        src/common/draw.h
        src/common/error.h
        src/common/error.cpp
        src/common/file.h
        src/common/file_.cpp
        src/common/floppy.h
        src/common/floppy.cpp
        src/common/io_bus.h
        src/common/io_bus.cpp
        src/common/image_codec.h
        src/common/image_codec.cpp
        src/common/memory_manager.h
        src/common/memory_manager.cpp
        src/common/memory_bus.h
        src/common/memory_bus.cpp
        src/common/misc.h
        src/common/png_codec.h
        src/common/png_codec.cpp
        src/common/scheduler.h
        src/common/scheduler.cpp
        src/common/sequencer.h
        src/common/sequencer.cpp
        src/common/scoped_comptr.h
        src/common/scoped_handle.h
        src/common/soundsrc.h
        src/common/srcbuf.h
        src/common/srcbuf.cpp
        src/common/status.h
        src/common/status.cpp
        src/common/threadable.h
        src/common/time_keeper.h
        src/common/time_keeper.cpp)

target_include_directories(common
        PUBLIC ${CMAKE_SOURCE_DIR}/src
        PUBLIC ${CMAKE_SOURCE_DIR}/third_party)

# TODO: remove this dependency
target_link_libraries(common Z80)

add_executable(common_unittests
        src/common/device_test.cc
        src/common/io_bus_test.cc
        src/common/scheduler_test.cc)

target_link_libraries(common_unittests
        PRIVATE gtest gtest_main
        common)

add_library(devices ${LIB_TYPE}
        src/devices/z80.h
        src/devices/z80.cpp
        src/devices/z80c.cpp
        src/devices/z80diag.h
        src/devices/z80diag.cpp
        src/devices/z80x.h
        src/devices/z80x.cpp)

target_include_directories(devices
        PUBLIC ${CMAKE_SOURCE_DIR}/src
        PUBLIC ${CMAKE_SOURCE_DIR}/third_party)

target_link_libraries(devices Z80)

add_executable(devices_unittests
        src/devices/fmgen_test.cc
        src/devices/fmtimer_test.cc
        src/devices/opna_test.cc
        src/devices/psg_test.cc
        src/devices/z80c_test.cc)

target_link_libraries(devices_unittests
        PRIVATE gtest gtest_main
        common devices fmgen)

add_library(fmgen ${LIB_TYPE}
        src/devices/fmgen.h
        src/devices/fmgen.cpp
        src/devices/fmtimer.h
        src/devices/fmtimer.cpp
        src/devices/opm.h
        src/devices/opm.cpp
        src/devices/opna.h
        src/devices/opna.cpp
        src/devices/psg.h
        src/devices/psg.cpp)

target_include_directories(fmgen
        PUBLIC ${CMAKE_SOURCE_DIR}/src
        PUBLIC ${CMAKE_SOURCE_DIR}/third_party)

add_library(pc88core ${LIB_TYPE}
        src/pc88/base.h
        src/pc88/base.cpp
        src/pc88/beep.h
        src/pc88/beep.cpp
        src/pc88/calendar.h
        src/pc88/calendar.cpp
        src/pc88/crtc.h
        src/pc88/crtc.cpp
        src/pc88/fdc.h
        src/pc88/fdc.cpp
        src/pc88/fdu.h
        src/pc88/fdu.cpp
        src/pc88/intc.h
        src/pc88/intc.cpp
        src/pc88/joypad.h
        src/pc88/joypad.cpp
        src/pc88/kanjirom.h
        src/pc88/kanjirom.cpp
        src/pc88/memory.h
        src/pc88/memory.cpp
        src/pc88/mouse.h
        src/pc88/mouse.cpp
        src/pc88/opnif.h
        src/pc88/opnif.cpp
        src/pc88/pc88.h
        src/pc88/pc88.cpp
        src/pc88/pd8257.h
        src/pc88/pd8257.cpp
        src/pc88/pio.h
        src/pc88/pio.cpp
        src/pc88/screen.h
        src/pc88/screen.cpp
        src/pc88/sio.h
        src/pc88/sio.cpp
        src/pc88/subsys.h
        src/pc88/subsys.cpp)

target_include_directories(pc88core
        PUBLIC ${CMAKE_SOURCE_DIR}/src
        PUBLIC ${CMAKE_SOURCE_DIR}/third_party)

target_link_libraries(pc88core Z80)

add_executable(pc88core_unittests
        src/pc88/base_test.cc
        src/pc88/beep_test.cc
        src/pc88/calendar_test.cc
        src/pc88/config_test.cc
        src/pc88/crtc_test.cc
        src/pc88/pc88_test.cc)

target_link_libraries(pc88core_unittests
        PRIVATE gtest gtest_main
        common devices fmgen pc88core pc88shell)

add_library(pc88shell ${LIB_TYPE}
        src/services/disk_image_holder.h
        src/services/disk_image_holder.cpp
        src/services/diskmgr.h
        src/services/diskmgr.cpp
        src/services/ioview.h
        src/services/ioview.cpp
        src/services/memview.h
        src/services/memview.cpp
        src/services/sound.h
        src/services/sound.cpp
        src/services/tapemgr.h
        src/services/tapemgr.cpp)

target_include_directories(pc88shell
        PUBLIC ${CMAKE_SOURCE_DIR}/src
        PUBLIC ${CMAKE_SOURCE_DIR}/third_party)

target_link_libraries(pc88shell devices)

add_library(win32 ${LIB_TYPE}
        src/win32/88config.h
        src/win32/88config.cpp
        src/win32/about.h
        src/win32/about.cpp
        src/win32/cfgpage.h
        src/win32/cfgpage.cpp
        src/win32/extdev.h
        src/win32/extdev.cpp
        src/win32/file.h
        src/win32/file.cpp
        src/win32/filetest.h
        src/win32/filetest.cpp
        src/win32/guid.cpp
        src/win32/module.h
        src/win32/module.cpp
        src/win32/newdisk.h
        src/win32/newdisk.cpp
        src/win32/screen/drawd3d12.h
        src/win32/screen/drawd3d12.cpp
        src/win32/screen/pixel_shader.h
        src/win32/screen/vertex_shader.h
        src/win32/screen/windraw.h
        src/win32/screen/windraw.cpp
        src/win32/sound/sounddrv.h
        src/win32/sound/soundds.h
        src/win32/sound/soundds.cpp
        src/win32/sound/soundds2.h
        src/win32/sound/soundds2.cpp
        src/win32/sound/soundwo.h
        src/win32/sound/soundwo.cpp
        src/win32/sound/winsound.h
        src/win32/sound/winsound.cpp
        src/win32/status_win.h
        src/win32/status_win.cpp
        src/win32/ui.h
        src/win32/ui.cpp
        src/win32/wincfg.h
        src/win32/wincfg.cpp
        src/win32/wincore.h
        src/win32/wincore.cpp
        src/win32/winexapi.h
        src/win32/winexapi.cpp
        src/win32/winjoy.h
        src/win32/winjoy.cpp
        src/win32/winkeyif.h
        src/win32/winkeyif.cpp
        src/win32/winmouse.h
        src/win32/winmouse.cpp)

target_include_directories(win32
        PUBLIC ${CMAKE_SOURCE_DIR}/src
        PUBLIC ${CMAKE_SOURCE_DIR}/third_party)

target_link_libraries(win32 devices)

add_executable(win32_unittests
        src/pc88/pc88_test.cc)

target_link_libraries(win32_unittests
        PRIVATE gtest gtest_main
        common devices fmgen pc88core pc88shell win32)

add_library(win32mon ${LIB_TYPE}
        src/win32/monitor/basmon.h
        src/win32/monitor/basmon.cpp
        src/win32/monitor/codemon.h
        src/win32/monitor/codemon.cpp
        src/win32/monitor/iomon.h
        src/win32/monitor/iomon.cpp
        src/win32/monitor/loadmon.h
        src/win32/monitor/loadmon.cpp
        src/win32/monitor/memmon.h
        src/win32/monitor/memmon.cpp
        src/win32/monitor/mvmon.h
        src/win32/monitor/mvmon.cpp
        src/win32/monitor/regmon.h
        src/win32/monitor/regmon.cpp
        src/win32/monitor/soundmon.h
        src/win32/monitor/soundmon.cpp
        src/win32/monitor/winmon.h
        src/win32/monitor/winmon.cpp)

target_include_directories(win32mon
        PUBLIC ${CMAKE_SOURCE_DIR}/src
        PUBLIC ${CMAKE_SOURCE_DIR}/third_party)

target_link_libraries(win32mon devices)

add_executable(win32mon_unittests
        src/pc88/pc88_test.cc)

target_link_libraries(win32mon_unittests
        PUBLIC gtest gtest_main
        common devices fmgen pc88core pc88shell win32 win32mon)

add_library(romeo ${LIB_TYPE}
        src/piccolo/piioctl.h
        src/win32/romeo/c86ctl.h
        src/win32/romeo/piccolo.h
        src/win32/romeo/piccolo.cpp
        src/win32/romeo/piccolo_gimic.h
        src/win32/romeo/piccolo_gimic.cpp
        src/win32/romeo/piccolo_romeo.h
        src/win32/romeo/piccolo_romeo.cpp
        src/win32/romeo/romeo.h
        src/win32/romeo/piccolo_scci.h
        src/win32/romeo/piccolo_scci.cpp
        src/win32/romeo/scci.h
        src/win32/romeo/SCCIDefines.h)

target_include_directories(romeo
        PUBLIC ${CMAKE_SOURCE_DIR}/src
        PUBLIC ${CMAKE_SOURCE_DIR}/third_party)

add_library(zlib ${LIB_TYPE}
        src/zlib/adler32.c
        src/zlib/compress.c
        src/zlib/crc32.c
        src/zlib/crc32.h
        src/zlib/deflate.c
        src/zlib/deflate.h
        src/zlib/inffast.c
        src/zlib/inffixed.h
        src/zlib/inflate.c
        src/zlib/inflate.h
        src/zlib/inftrees.c
        src/zlib/inftrees.h
        src/zlib/trees.c
        src/zlib/trees.h
        src/zlib/uncompr.c
        src/zlib/zconf.h
        src/zlib/zlib.h
        src/zlib/zutil.c
        src/zlib/zutil.h)

add_library(libpng ${LIB_TYPE}
        third_party/libpng/png.c
        third_party/libpng/png.h
        third_party/libpng/pngconf.h
        third_party/libpng/pngdebug.h
        third_party/libpng/pngerror.c
        third_party/libpng/pngget.c
        third_party/libpng/pnginfo.h
        third_party/libpng/pngmem.c
        third_party/libpng/pngpread.c
        third_party/libpng/pngpriv.h
        third_party/libpng/pngread.c
        third_party/libpng/pngrio.c
        third_party/libpng/pngrtran.c
        third_party/libpng/pngrutil.c
        third_party/libpng/pngset.c
        third_party/libpng/pngstruct.h
        third_party/libpng/pngtrans.c
        third_party/libpng/pngwio.c
        third_party/libpng/pngwrite.c
        third_party/libpng/pngwtran.c
        third_party/libpng/pngwutil.c)

target_include_directories(libpng
        PUBLIC ${CMAKE_SOURCE_DIR}/src/zlib)

add_executable(m88 WIN32
        src/win32/main.cpp
        src/win32/m88.rc
        src/win32/dpiaware.manifest
        src/win32/utf8.manifest)

target_link_libraries(m88
        PRIVATE common devices fmgen pc88core pc88shell win32 win32mon romeo
        PRIVATE zlib libpng
        PRIVATE winmm.lib comctl32.lib dwmapi.lib d3d12.lib dxgi.lib d3dcompiler.lib)

add_library(cdif SHARED
        cdif/src/aspi.h
        cdif/src/aspi.cpp
        cdif/src/aspidef.h
        cdif/src/cdctrl.h
        cdif/src/cdctrl.cpp
        cdif/src/cdif.h
        cdif/src/cdif.cpp
        cdif/src/cdif.rc
        cdif/src/cdrom.h
        cdif/src/cdrom.cpp
        cdif/src/cdromdef.h
        cdif/src/config.h
        cdif/src/config.cpp
        cdif/src/guid.cpp
        cdif/src/moduleif.cpp
        cdif/src/resource.h)

# modules

target_include_directories(cdif
        PRIVATE ${CMAKE_SOURCE_DIR}
        PRIVATE ${CMAKE_SOURCE_DIR}/third_party)

target_link_libraries(cdif
        common devices)

add_custom_command(TARGET cdif POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:cdif>
        cdif.m88
        COMMENT "Copying 'cdif.dll' library to 'cdif.m88'")

add_library(diskdrv SHARED
        diskdrv/src/diskio.h
        diskdrv/src/diskio.cpp
        diskdrv/src/guid.cpp
        diskdrv/src/moduleif.cpp
        src/win32/file.h
        src/win32/file.cpp)

target_include_directories(diskdrv
        PRIVATE ${CMAKE_SOURCE_DIR}
        PRIVATE ${CMAKE_SOURCE_DIR}/third_party)

target_link_libraries(diskdrv
        common devices)

add_custom_command(TARGET diskdrv POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:diskdrv>
        diskdrv.m88
        COMMENT "Copying 'diskdrv.dll' library to 'diskdrv.m88'")

add_library(sample1 ${LIB_TYPE}
        sample1/src/guid.cpp
        sample1/src/moduleif.cpp
        sample1/src/sine.h
        sample1/src/sine.cpp)

target_include_directories(sample1
        PRIVATE ${CMAKE_SOURCE_DIR}
        PRIVATE ${CMAKE_SOURCE_DIR}/third_party)

target_link_libraries(sample1
        common devices)

add_library(sample2 ${LIB_TYPE}
        sample2/src/config.h
        sample2/src/config.cpp
        sample2/src/guid.cpp
        sample2/src/mem.h
        sample2/src/mem.cpp
        sample2/src/moduleif.cpp
        sample2/src/resource.h)

target_include_directories(sample2
        PRIVATE ${CMAKE_SOURCE_DIR}
        PRIVATE ${CMAKE_SOURCE_DIR}/third_party)

target_link_libraries(sample2
        common devices)

# tools

add_executable(writetag
        tools/writetag.cpp)

add_custom_command(TARGET writetag POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:writetag>
        ${CMAKE_SOURCE_DIR}/bin
        COMMENT "Copying 'writetag.exe' binary to 'bin'")

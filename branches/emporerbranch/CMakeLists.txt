# The project's name is VBA-M it uses C and C++ code
PROJECT(VBA-M C CXX)

cmake_minimum_required( VERSION 2.6.0 )
if( COMMAND cmake_policy )
    cmake_policy( SET CMP0003 NEW )
    cmake_policy( SET CMP0005 OLD )
endif( COMMAND cmake_policy )
SET( CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMakeScripts )
#Output all binaries at top level
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})

option( ENABLE_SDL "Build the SDL port" ON )
option( ENABLE_GTK "Build the GTK+ GUI" ON )
option( ENABLE_WX "Build the wxWidgets port" ON )
option( ENABLE_DEBUGGER "Enable the debugger" ON )
option( ENABLE_NLS "Enable translations" ON )
option( ENABLE_ASM_CORE "Enable x86 ASM CPU cores" OFF )
option( ENABLE_ASM_SCALERS "Enable x86 ASM graphic filters" OFF )
option( ENABLE_LINK "Enable GBA linking functionality" ON )
option( ENABLE_LIRC "Enable LIRC support" OFF )
option( ENABLE_FFMPEG "Enable ffmpeg A/V recording" OFF )
if(ENABLE_ASM_SCALERS)
    option( ENABLE_MMX "Enable MMX" OFF )
endif(ENABLE_ASM_SCALERS)
option( ENABLE_GBA_LOGGING "Enable extended GBA logging" ON )
if( ENABLE_GBA_LOGGING )
   ADD_DEFINITIONS (-DGBA_LOGGING )
endif( ENABLE_GBA_LOGGING )
if(ENABLE_MMX)
  ADD_DEFINITIONS (-DMMX)
endif(ENABLE_MMX)

if ( WIN32 )
set(CMAKE_EXE_LINKER_FLAGS "-static")
endif ( WIN32 )
# The SDL port can't be built without debugging support
if( NOT ENABLE_DEBUGGER AND ENABLE_SDL )
        message( SEND_ERROR "The SDL port can't be built without debugging support" )
endif( NOT ENABLE_DEBUGGER AND ENABLE_SDL )

# Set the version number with -DVERSION=X.X.X-uber
IF( NOT VERSION )
    SET( VERSION "1.8.0-SVN" )
   
    IF(EXISTS "${CMAKE_SOURCE_DIR}/.svn")
        FIND_PACKAGE(Subversion)
        IF(SUBVERSION_FOUND)
            Subversion_WC_INFO(${CMAKE_SOURCE_DIR} SVN_INFO)
            SET( VERSION "1.8.0-SVN${SVN_INFO_WC_REVISION}" )
        ENDIF(SUBVERSION_FOUND)
    ENDIF(EXISTS "${CMAKE_SOURCE_DIR}/.svn")
ENDIF( NOT VERSION )

# Fill in SDLMAIN_LIBRARY on OS X manually to avoid using SDLMain.m
# OS X users will have to compile and install SDL from source.
if( APPLE )
    SET(SDLMAIN_LIBRARY "-lSDLmain")
endif( APPLE )

# Check for nasm
if( ENABLE_ASM_SCALERS )
    ENABLE_LANGUAGE( ASM_NASM )
endif( ENABLE_ASM_SCALERS )

# Look for some dependencies using CMake scripts
FIND_PACKAGE ( ZLIB REQUIRED )
FIND_PACKAGE ( PNG REQUIRED )
FIND_PACKAGE ( OpenGL REQUIRED )
FIND_PACKAGE ( SDL REQUIRED )

if( ENABLE_LINK )
    FIND_PACKAGE ( SFML REQUIRED )
endif( ENABLE_LINK )
# set the standard libraries all ports use
SET(VBAMCORE_LIBS
    vbamcore
    fex
    ${SDL_LIBRARY}
    ${SFML_LIBRARY}
    ${OPENGL_LIBRARIES}
    ${ZLIB_LIBRARY}
    ${PNG_LIBRARY}
)

if(ENABLE_FFMPEG)
    FIND_PACKAGE ( PkgConfig REQUIRED )

    PKG_CHECK_MODULES(FFMPEG REQUIRED libavcodec libavformat libswscale libavutil)
endif(ENABLE_FFMPEG)

if(NOT ENABLE_FFMPEG)
  ADD_DEFINITIONS(-DNO_FFMPEG)
endif(NOT ENABLE_FFMPEG)

IF( ENABLE_LIRC )
        SET( WITHLIRC 1 )
ELSE( ENABLE_LIRC )
        SET( WITHLIRC 0 )
ENDIF( ENABLE_LIRC )

# Set the default install dir
IF( NOT DATA_INSTALL_DIR )
    SET( DATA_INSTALL_DIR "share/vbam" )
ENDIF( NOT DATA_INSTALL_DIR )

SET( PKGDATADIR ${CMAKE_INSTALL_PREFIX}/${DATA_INSTALL_DIR} )

# Set the configuration file location
IF( NOT SYSCONFDIR )
    SET( SYSCONFDIR "/etc" )
ENDIF( NOT SYSCONFDIR )

# C defines
ADD_DEFINITIONS (-DHAVE_NETINET_IN_H -DHAVE_ARPA_INET_H -DHAVE_ZLIB_H -DFINAL_VERSION -DSDL -DUSE_OPENGL -DSYSCONFDIR='"${SYSCONFDIR}"' -DWITH_LIRC='${WITHLIRC}')
ADD_DEFINITIONS (-DVERSION='"${VERSION}"' -DPKGDATADIR='"${PKGDATADIR}"' -DPACKAGE='')

if( ENABLE_LINK )
    # IPC linking code needs sem_timedwait which can be either in librt or pthreads
    FIND_LIBRARY(RT_LIB rt)
    IF(RT_LIB)
       SET(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${RT_LIB})
       SET(VBAMCORE_LIBS ${VBAMCORE_LIBS} ${RT_LIB})
    ENDIF(RT_LIB)
    
    FIND_LIBRARY(PTHREAD_LIB pthread)
    IF(PTHREAD_LIB)
       SET(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${PTHREAD_LIB})
       SET(VBAMCORE_LIBS ${VBAMCORE_LIBS} ${PTHREAD_LIB})
    ENDIF(PTHREAD_LIB)
    
    INCLUDE(CheckFunctionExists)
    CHECK_FUNCTION_EXISTS(sem_timedwait SEM_TIMEDWAIT)
    IF( SEM_TIMEDWAIT)
        ADD_DEFINITIONS (-DHAVE_SEM_TIMEDWAIT)
    ENDIF( SEM_TIMEDWAIT)
else( ENABLE_LINK )
    ADD_DEFINITIONS (-DNO_LINK)
endif( ENABLE_LINK )

# The debugger is enabled by default
if( NOT ENABLE_DEBUGGER )
    ADD_DEFINITIONS (-DNO_DEBUGGER)
else( NOT ENABLE_DEBUGGER )
    ADD_DEFINITIONS (-DBKPT_SUPPORT)
endif( NOT ENABLE_DEBUGGER )

# The ASM core is disabled by default because we don't know on which platform we are
IF( NOT ENABLE_ASM_CORE )
    ADD_DEFINITIONS (-DC_CORE)
ENDIF( NOT ENABLE_ASM_CORE )

# Enable internationalization
if( ENABLE_NLS )
    SET( LOCALEDIR ${CMAKE_INSTALL_PREFIX}/share/locale )
    ADD_DEFINITIONS ( -DENABLE_NLS )
    ADD_DEFINITIONS ( -DLOCALEDIR=\\\"${LOCALEDIR}\\\" )
    # for now, only GBALink.cpp uses gettext() directly
    IF(ENABLE_LINK)
        FIND_PATH(LIBINTL_INC libintl.h )
        FIND_LIBRARY(LIBINTL_LIB intl )
	IF(LIBINTL_LIB)
            SET(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${LIBINTL_LIB})
            SET(VBAMCORE_LIBS ${VBAMCORE_LIBS} ${LIBINTL_LIB})
	ENDIF(LIBINTL_LIB)
        INCLUDE(CheckFunctionExists)
        CHECK_FUNCTION_EXISTS(gettext GETTEXT_FN)
        IF(NOT LIBINTL_INC OR NOT GETTEXT_FN)
            message( SEND_ERROR "NLS requires libintl" )
        ENDIF(NOT LIBINTL_INC OR NOT GETTEXT_FN)
        INCLUDE_DIRECTORIES(${LIBINTL_INC})
    ENDIF(ENABLE_LINK)
endif( ENABLE_NLS )

# Compiler flags
IF ( WIN32 )
    SET( CMAKE_ASM_NASM_FLAGS "-I$(CMAKE_SOURCE_DIR)/src/filters/hq/asm/ -O1 -w-orphan-labels")
ELSE ( WIN32 )
    SET( CMAKE_ASM_NASM_FLAGS "-I$(CMAKE_SOURCE_DIR)/src/filters/hq/asm/ -O1 -DELF -w-orphan-labels")
ENDIF ( WIN32 )

SET( CMAKE_C_FLAGS_RELEASE "-O3")
SET( CMAKE_CXX_FLAGS_RELEASE "-O3")
SET( CMAKE_C_FLAGS_DEBUG "-g -Wall")
SET( CMAKE_CXX_FLAGS_DEBUG "-g -Wall")

add_subdirectory (fex)
SET( CMAKE_CXX_FLAGS -std=c++11 )
SET(SRC_MAIN
    src/Util.cpp
    src/common/Patch.cpp
    src/common/memgzio.c
    src/common/SoundSDL.cpp
)

if(ENABLE_FFMPEG)
    SET(SRC_MAIN ${SRC_MAIN} src/common/ffmpeg.cpp)
endif(ENABLE_FFMPEG)

SET(SRC_GBA
    src/gba/agbprint.cpp
    src/gba/bios.cpp
    src/gba/BreakpointStructures.cpp
    src/gba/Cheats.cpp
    src/gba/CheatSearch.cpp
    src/gba/debugger-expr-lex.cpp
    src/gba/debugger-expr-yacc.cpp
    src/gba/EEprom.cpp
    src/gba/ereader.cpp
    src/gba/Flash.cpp
    src/gba/GBA.cpp
    src/gba/GBAGfx.cpp
    src/gba/GBALink.cpp
    src/gba/GBASockClient.cpp
    src/gba/GBA-thumb.cpp
    src/gba/GBA-arm.cpp
    src/gba/gbafilter.cpp
    src/gba/Globals.cpp
    src/gba/Mode0.cpp
    src/gba/Mode1.cpp
    src/gba/Mode2.cpp
    src/gba/Mode3.cpp
    src/gba/Mode4.cpp
    src/gba/Mode5.cpp
    src/gba/RTC.cpp
    src/gba/Sound.cpp
    src/gba/Sram.cpp
)

SET(SRC_GB
    src/gb/GB.cpp
    src/gb/gbCheats.cpp
    src/gb/gbDis.cpp
    src/gb/gbGfx.cpp
    src/gb/gbGlobals.cpp
    src/gb/gbMemory.cpp
    src/gb/gbPrinter.cpp
    src/gb/gbSGB.cpp
    src/gb/gbSound.cpp
)

SET(SRC_APU
    src/apu/Blip_Buffer.cpp
    src/apu/Effects_Buffer.cpp
    src/apu/Gb_Apu.cpp
    src/apu/Gb_Apu_State.cpp
    src/apu/Gb_Oscs.cpp
    src/apu/Multi_Buffer.cpp
)

SET(SRC_SDL
    src/sdl/SDL.cpp
    src/sdl/filters.cpp
    src/sdl/text.cpp
    src/sdl/inputSDL.cpp
    src/sdl/expr.cpp
    src/sdl/exprNode.cpp
    src/sdl/expr-lex.cpp
)

#Do not use this file directly.  Always use the top level CMakeLists.txt file
#File extractors so the user doesn't have to extract the rom before playing it

# Source files definition

SET(SRC_FILTERS
    src/filters/filters.cpp
    src/filters/new_interframe.cpp
    src/filters/2xSaI.cpp
    src/filters/admame.cpp
    src/filters/bilinear.cpp
    src/filters/hq2x.cpp
    src/filters/interframe.cpp
    src/filters/pixel.cpp
    src/filters/scanline.cpp
    src/filters/simpleFilter.cpp
    src/filters/xbrzfilter.cpp
    src/filters/xBRZ/xbrz.cpp
    src/filters/sdl.cpp
)

SET(SRC_HQ_C
    src/filters/hq/c/hq_implementation.cpp
)

SET(SRC_HQ_ASM
    src/filters/hq/asm/hq3x_16.asm
    src/filters/hq/asm/hq3x_32.asm
    src/filters/hq/asm/hq4x_16.asm
    src/filters/hq/asm/hq4x_32.asm
    src/filters/hq/asm/hq3x32.cpp
)

if( ENABLE_ASM_SCALERS )
    SET(SRC_FILTERS ${SRC_FILTERS} ${SRC_HQ_ASM})
else( ENABLE_ASM_SCALERS )
    SET(SRC_FILTERS ${SRC_FILTERS} ${SRC_HQ_C})
    ADD_DEFINITIONS ( -DNO_ASM )
endif( ENABLE_ASM_SCALERS )

if( ENABLE_DEBUGGER )
    SET(SRC_DEBUGGER
        src/gba/armdis.cpp
        src/gba/elf.cpp
        src/gba/remote.cpp
    )
endif( ENABLE_DEBUGGER )

INCLUDE_DIRECTORIES(
    ${ZLIB_INCLUDE_DIR}
    fex
    ${PNG_INCLUDE_DIR}
    ${SDL_INCLUDE_DIR}
)

IF( ENABLE_FFMPEG )
    INCLUDE_DIRECTORIES(
        ${FFMPEG_INCLUDE_DIRS}
    )
ENDIF( ENABLE_FFMPEG )

ADD_LIBRARY (
    vbamcore
    ${PROJECT_SRCS}
    ${SRC_MAIN}
    ${SRC_GBA}
    ${SRC_GB}
    ${SRC_APU}
    ${SRC_FILTERS}
    ${SRC_DEBUGGER}
)

IF( ENABLE_SDL )
    ADD_EXECUTABLE (
        vbam
        WIN32
        ${SRC_SDL}
    )

    IF( WIN32 )
        SET( WIN32_LIBRARIES wsock32 )
    ENDIF( WIN32 )

    IF( ENABLE_LIRC )
        SET( LIRC_CLIENT_LIBRARY lirc_client )
    ENDIF( ENABLE_LIRC )

    TARGET_LINK_LIBRARIES (
        vbam
        ${VBAMCORE_LIBS}
        ${WIN32_LIBRARIES}
        ${LIRC_CLIENT_LIBRARY}
    )
	IF( WIN32 )
		INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/vbam${CMAKE_EXECUTABLE_SUFFIX} DESTINATION bin)
	ELSE( WIN32 )
		INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/vbam DESTINATION bin)
	ENDIF( WIN32 )
		INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/sdl/vbam.cfg-example
				DESTINATION ${SYSCONFDIR}
				RENAME vbam.cfg)
ENDIF( ENABLE_SDL )

IF( ENABLE_GTK )
	add_subdirectory (src/gtk)
ENDIF( ENABLE_GTK )

IF( ENABLE_WX )
    add_subdirectory(src/wx)
ENDIF( ENABLE_WX )

if( ENABLE_GTK OR ENABLE_WX )
    # Native Language Support
    if( ENABLE_NLS )
        add_subdirectory(po)
    endif( ENABLE_NLS )
endif( ENABLE_GTK OR ENABLE_WX )
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/vba-over.ini DESTINATION ${DATA_INSTALL_DIR})
IF(NOT APPLE)
    INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/gtk/icons/ DESTINATION share/icons/hicolor PATTERN ".svn" EXCLUDE)
ENDIF(NOT APPLE)
SET(CPACK_GENERATOR "ZIP")
SET(CPACK_PACKAGE_VERSION_MAJOR "1")
SET(CPACK_PACKAGE_VERSION_MINOR "8")
SET(CPACK_PACKAGE_VERSION_PATCH "0-SVN1292")
INCLUDE(CPack)
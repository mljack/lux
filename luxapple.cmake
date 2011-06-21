cmake_minimum_required(VERSION 2.4)
IF(COMMAND cmake_policy)
	cmake_policy(SET CMP0003 NEW)
ENDIF(COMMAND cmake_policy)
MESSAGE(STATUS "CMAKE VERSION DETECTED " ${CMAKE_VERSION})

PROJECT(lux)
SET(VERSION 0.8)

SET(LUX_CMAKE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${LUX_CMAKE_DIR}")
option(LUXRAYS_DISABLE_OPENCL OFF)

# Dade - uncomment to obtain verbose building output
#SET(CMAKE_VERBOSE_MAKEFILE true)

#Setting Universal Binary Properties and Pylux configuration, only for Mac OS X
# generate with xcode/crosscompile, setting: ( darwin - 10.6 - gcc - g++ - MacOSX10.6.sdk - Find from root, then native system )
IF(APPLE)
	########## OSX_OPTIONS ###########
	option(OSX_OPTION_PYLUX "Build a blender compatible pylux" ON)
	option(OSX_OPTION_CLANG "Build with CLANG compiler and LTO ( XCODE4 )" ON)
	option(OSX_OPTION_UNIVERSAL "Forcre compile universal" OFF)
	option(OSX_OPTION_DYNAMIC_BUILD "Link lux apps to shared corelib" ON)
	###################################
#	SET(OSX_INSTALL_PATH /Applications/LuxRender_dyntest ) # in work
	SET(CMAKE_OSX_DEPLOYMENT_TARGET 10.6)
	IF(CMAKE_VERSION VERSION_LESS 2.8.1)
		SET(CMAKE_OSX_ARCHITECTURES i386;x86_64) # valid arches
	ELSE(CMAKE_VERSION VERSION_LESS 2.8.1)
		SET(CMAKE_XCODE_ATTRIBUTE_ARCHS i386\ x86_64) # valid arches
		SET(CMAKE_XCODE_ATTRIBUTE_DEPLOYMENT_POSTPROCESSING YES) # strip symbols
	ENDIF(CMAKE_VERSION VERSION_LESS 2.8.1)
	if(OSX_OPTION_UNIVERSAL)
		set(CMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH NO)
	endif(OSX_OPTION_UNIVERSAL)
	SET(CMAKE_OSX_SYSROOT /Developer/SDKs/MacOSX10.6.sdk)
	set(CMAKE_CONFIGURATION_TYPES Release)
	SET(OSX_SHARED_CORELIB ${CMAKE_SOURCE_DIR}/../macos/lib/corelib/liblux.dylib)
ENDIF(APPLE)


#############################################################################
#############################################################################
### check for the CPU we build for                                        ###
#############################################################################
#############################################################################

IF(NOT APPLE)
	EXECUTE_PROCESS(
		COMMAND ${CMAKE_C_COMPILER} -dumpmachine
		OUTPUT_VARIABLE MACHINE
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	MESSAGE(STATUS "Building for target ${MACHINE}")

	STRING(REGEX MATCH "(i.86-*)|(athlon-*)|(pentium-*)" _mach_x86 ${MACHINE})
	IF (_mach_x86)
		SET(ARCH_X86 1)
	ENDIF (_mach_x86)

	STRING(REGEX MATCH "(x86_64-*)|(X86_64-*)|(AMD64-*)|(amd64-*)" _mach_x86_64 ${MACHINE})
	IF (_mach_x86_64)
		SET(ARCH_X86_64 1)
    SET(LIB_SUFFIX 64)
		#jromang - Hack to avoid boost bug on x64  Ubuntu 8.10 and Fedora 10 (http://www.luxrender.net/mantis/view.php?id=433)
		ADD_DEFINITIONS(-DBOOST_NO_INTRINSIC_INT64_T)
	ENDIF (_mach_x86_64)

	STRING(REGEX MATCH "(ppc-*)|(powerpc-*)" _mach_ppc ${MACHINE})
	IF (_mach_ppc)
		SET(ARCH_PPC 1)
	ENDIF (_mach_ppc)
ENDIF(NOT APPLE)


#############################################################################
#############################################################################
##########################      Find LuxRays       ##########################
#############################################################################
#############################################################################
IF(APPLE)
	FIND_PATH(LUXRAYS_INCLUDE_DIRS NAMES luxrays/luxrays.h PATHS ../macos/include/LuxRays)
	FIND_LIBRARY(LUXRAYS_LIBRARY libluxrays.a ../macos/lib/LuxRays)
ELSE(APPLE)
	FIND_PATH(LUXRAYS_INCLUDE_DIRS NAMES luxrays/luxrays.h PATHS ../luxrays/include)
	FIND_LIBRARY(LUXRAYS_LIBRARY luxrays ../luxrays/lib)
ENDIF(APPLE)

MESSAGE(STATUS "LuxRays include directory: " ${LUXRAYS_INCLUDE_DIRS})
MESSAGE(STATUS "LuxRays library directory: " ${LUXRAYS_LIBRARY})


#############################################################################
#############################################################################
###########################      Find OpenCL       ##########################
#############################################################################
#############################################################################

if(LUXRAYS_DISABLE_OPENCL)
    set(OCL_LIBRARY "")
else(LUXRAYS_DISABLE_OPENCL)
    FIND_PATH(OPENCL_INCLUDE_DIRS NAMES CL/cl.hpp OpenCL/cl.hpp PATHS /usr/src/opencl-sdk/include /usr/local/cuda/include)
    FIND_LIBRARY(OPENCL_LIBRARY OpenCL /usr/src/opencl-sdk/lib/x86_64)

    MESSAGE(STATUS "OpenCL include directory: " ${OPENCL_INCLUDE_DIRS})
    MESSAGE(STATUS "OpenCL library directory: " ${OPENCL_LIBRARY})
endif(LUXRAYS_DISABLE_OPENCL)


#############################################################################
#############################################################################
###########################      Find OpenGL       ##########################
#############################################################################
#############################################################################

FIND_PACKAGE(OpenGL)

message(STATUS "OpenGL include directory: " "${OPENGL_INCLUDE_DIRS}")
message(STATUS "OpenGL library: " "${OPENGL_LIBRARY}")


#############################################################################
#############################################################################
###########################      Find BISON       ###########################
#############################################################################
#############################################################################

FIND_PACKAGE(BISON REQUIRED)
IF (NOT BISON_FOUND)
	MESSAGE(FATAL_ERROR "bison not found - aborting")
ENDIF (NOT BISON_FOUND)


#############################################################################
#############################################################################
###########################      Find FLEX        ###########################
#############################################################################
#############################################################################

FIND_PACKAGE(FLEX REQUIRED)
IF (NOT FLEX_FOUND)
	MESSAGE(FATAL_ERROR "flex not found - aborting")
ENDIF (NOT FLEX_FOUND)



#############################################################################
#############################################################################
########################### BOOST LIBRARIES SETUP ###########################
#############################################################################
#############################################################################

IF(APPLE)
	SET(BOOST_ROOT ../macos)
ENDIF(APPLE)

FIND_PACKAGE(Boost 1.43 COMPONENTS python REQUIRED)

SET(Boost_python_FOUND ${Boost_FOUND})
SET(Boost_python_LIBRARIES ${Boost_LIBRARIES})
SET(Boost_FOUND)
SET(Boost_LIBRARIES)

FIND_PACKAGE(Boost 1.43 COMPONENTS thread program_options filesystem serialization iostreams regex system REQUIRED)

IF(Boost_FOUND)
	MESSAGE(STATUS "Boost library directory: " ${Boost_LIBRARY_DIRS})
	MESSAGE(STATUS "Boost include directory: " ${Boost_INCLUDE_DIRS})
ELSE(Boost_FOUND)
	MESSAGE(FATAL_ERROR "Could not find Boost")
ENDIF(Boost_FOUND)


#############################################################################
#############################################################################
######################### OPENEXR LIBRARIES SETUP ###########################
#############################################################################
#############################################################################

# !!!!freeimage needs headers from or matched with freeimage !!!!
IF(APPLE)
	FIND_PATH(OPENEXR_INCLUDE_DIRS
		OpenEXRConfig.h
		PATHS
		../macos/include/OpenEXR
		NO_DEFAULT_PATH
	)
ELSE(APPLE)
	FIND_PATH(OPENEXR_INCLUDE_DIRS
		ImfXdr.h
		PATHS
		/usr/local/include/OpenEXR
		/usr/include/OpenEXR
		/sw/include/OpenEXR
		/opt/local/include/OpenEXR
		/opt/csw/include/OpenEXR
		/opt/include/OpenEXR
	)
	SET(OPENEXR_LIBRARIES Half IlmImf Iex Imath)
ENDIF(APPLE)

#############################################################################
#############################################################################
########################### PNG   LIBRARIES SETUP ###########################
#############################################################################
#############################################################################
# - Find the native PNG includes and library
#
# This module defines
#  PNG_INCLUDE_DIR, where to find png.h, etc.
#  PNG_LIBRARIES, the libraries to link against to use PNG.
#  PNG_DEFINITIONS - You should ADD_DEFINITONS(${PNG_DEFINITIONS}) before compiling code that includes png library files.
#  PNG_FOUND, If false, do not try to use PNG.
# also defined, but not for general use are
#  PNG_LIBRARY, where to find the PNG library.
# None of the above will be defined unles zlib can be found.
# PNG depends on Zlib
IF(APPLE)
	FIND_PATH(PNG_INCLUDE_DIR
		pngconf.h
		PATHS
		../macos//include
		NO_DEFAULT_PATH
	)
ELSE(APPLE)
	FIND_PACKAGE(PNG)
	IF(NOT PNG_FOUND)
		MESSAGE(STATUS "Warning : could not find PNG - building without png support")
	ENDIF(NOT PNG_FOUND)
ENDIF(APPLE)



#############################################################################
#############################################################################
###########################   FREEIMAGE LIBRARIES    ########################
#############################################################################
#############################################################################

IF(APPLE)
	FIND_PATH(FREEIMAGE_INCLUDE_DIRS
		freeimage.h
		PATHS
	../macos//include
	NO_DEFAULT_PATH
	)
	FIND_LIBRARY(FREEIMAGE_LIBRARIES
		libfreeimage.a
		PATHS
	../macos//lib
	NO_DEFAULT_PATH
	)
ELSE(APPLE)
	FIND_PACKAGE(FreeImage REQUIRED)

	IF(FREEIMAGE_FOUND)
		MESSAGE(STATUS "FreeImage library directory: " ${FREEIMAGE_LIBRARIES})
		MESSAGE(STATUS "FreeImage include directory: " ${FREEIMAGE_INCLUDE_PATH})
	ELSE(FREEIMAGE_FOUND)
		MESSAGE(FATAL_ERROR "Could not find FreeImage")
	ENDIF(FREEIMAGE_FOUND)
ENDIF(APPLE)

#############################################################################
#############################################################################
############################ THREADING LIBRARIES ############################
#############################################################################
#############################################################################
IF(APPLE)
	FIND_PATH(THREADS_INCLUDE_DIRS
		pthread.h
		PATHS
		/usr/include/pthread
	)
	SET(THREADS_LIBRARIES pthread)
ELSE(APPLE)
	FIND_PACKAGE(Threads REQUIRED)
ENDIF(APPLE)

#############################################################################
#############################################################################
###########################   SYSTEM LIBRARIES    ###########################
#############################################################################
#############################################################################
IF(APPLE)
	SET(SYS_LIBRARIES z bz2)
ENDIF(APPLE)

#############################################################################
#############################################################################
############################    DOXYGEN          ############################
#############################################################################
#############################################################################

FIND_PACKAGE(Doxygen)

IF (DOXYGEN_FOUND)
	MESSAGE( STATUS "Found Doxygen and generating documentation" )
	
	SET(DOXYGEN_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/doxygen/doxygen.template)
	SET(DOXYGEN_INPUT ${CMAKE_CURRENT_BINARY_DIR}/doxygen.conf)
	SET(DOXYGEN_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/doc)
	SET(DOXYGEN_OUTPUT ${DOXYGEN_OUTPUT_DIR}/html/index.html)
	
	MESSAGE( STATUS "Doxygen output:" ${DOXYGEN_OUTPUT} )
	
	IF(DOXYGEN_DOT_FOUND)
		MESSAGE( STATUS "Found dot" )
		SET(DOXYGEN_DOT_CONF "HAVE_DOT = YES")
	ENDIF(DOXYGEN_DOT_FOUND)
	
	ADD_CUSTOM_COMMAND( 
	OUTPUT ${DOXYGEN_OUTPUT}
	#creating custom doxygen.conf
	COMMAND cp ${DOXYGEN_TEMPLATE} ${DOXYGEN_INPUT}
	COMMAND echo "INPUT = " ${CMAKE_CURRENT_SOURCE_DIR} >> ${DOXYGEN_INPUT}
	COMMAND echo "OUTPUT_DIRECTORY = " ${DOXYGEN_OUTPUT_DIR} >> ${DOXYGEN_INPUT}
	COMMAND echo ${DOXYGEN_DOT_CONF} >> ${DOXYGEN_INPUT}
	#launch doxygen
	COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_INPUT}
	DEPENDS ${DOXYGEN_TEMPLATE}
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	)
	
	ADD_CUSTOM_TARGET(apidoc ALL DEPENDS ${DOXYGEN_OUTPUT})
ENDIF (DOXYGEN_FOUND)


#############################################################################
#All dependencies OK !
#############################################################################

#Generate the config.h file
CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/config.h.cmake ${CMAKE_BINARY_DIR}/config.h)
ADD_DEFINITIONS(-DLUX_USE_CONFIG_H)

#############################################################################
#############################################################################
#########################      COMPILER FLAGS     ###########################
#############################################################################
#############################################################################
IF(APPLE)
	# Jens - default Mac compiler options with OpenGL
	ADD_DEFINITIONS(-O3 -ftree-vectorize -msse -msse2 -msse3 -mssse3 -fvariable-expansion-in-unroller -fpic -Wall -DHAVE_PTHREAD_H )
ELSE(APPLE)
	# Dade - default compiler options
	ADD_DEFINITIONS(-O3 -msse2 -mfpmath=sse -ftree-vectorize -funroll-loops -Wall -fPIC -DHAVE_PTHREAD_H)
	# The QBVH accelerator needs to be compiled with much reduced optimizations
	# otherwise gcc produces incorrect code and ruins the render on 64bits machines
	SET_SOURCE_FILES_PROPERTIES(accelerators/qbvhaccel.cpp COMPILE_FLAGS "-O1")
ENDIF(APPLE)

#############################################################################
# Compiler flags for specific setup
#############################################################################

# Jens - default Mac compiler options with OpenGL
#ADD_DEFINITIONS(-O3 -ftree-vectorize -msse -msse2 -msse3 -mssse3 -fvariable-expansion-in-unroller -fpic -Wall -DHAVE_PTHREAD_H )
# Jens - testing Mac compiler options (debug)
#ADD_DEFINITIONS(-DHAVE_PTHREAD_H)

# Dade - GCC Profiling (remember to uncomment the line at the end of file too)
#ADD_DEFINITIONS(-pg -g -O2 -msse2 -mfpmath=sse -ftree-vectorize -Wall -DLUX_USE_OPENGL -DHAVE_PTHREAD_H)

# Dade - GCC 2 pass optimization (remember to uncomment the line at the end of file too)
#ADD_DEFINITIONS(-O3 --coverage -march=prescott -mfpmath=sse -ftree-vectorize -funroll-loops -ffast-math -Wall -DLUX_USE_OPENGL -DHAVE_PTHREAD_H)
#ADD_DEFINITIONS(-O3 -fbranch-probabilities -march=prescott -mfpmath=sse -ftree-vectorize -funroll-loops -ffast-math -Wall -DLUX_USE_OPENGL -DHAVE_PTHREAD_H)

# Dade - my settings
#ADD_DEFINITIONS(-g -O0 -DLUX_USE_OPENGL -DHAVE_PTHREAD_H)
#ADD_DEFINITIONS(-O3 -march=prescott -msse2 -mfpmath=sse -ftree-vectorize -funroll-loops -Wall -DLUX_USE_OPENGL -DHAVE_PTHREAD_H)
#ADD_DEFINITIONS(-O3 -march=athlon-xp -m3dnow -msse2 -mfpmath=sse -ftree-vectorize -funroll-loops -Wall -DLUX_USE_OPENGL -DHAVE_PTHREAD_H )

# Dade - Intel CC settings (double pass, 32bit, remember to uncomment the line at the end of file too)
#  rm -rf CMakeCache.txt CMakeFiles
#  CC=/opt/intel/cc/10.1.015/bin/icc CXX=/opt/intel/cc/10.1.015/bin/icpc cmake lux
# Pass 1
#ADD_DEFINITIONS(-prof-gen -prof-dir /tmp  -O3 -ipo -mtune=core2 -xT -unroll -fp-model fast=2 -rcd -no-prec-div -DLUX_USE_OPENGL -DHAVE_PTHREAD_H '-D"__sync_fetch_and_add(ptr,addend)=_InterlockedExchangeAdd(const_cast<void*>(reinterpret_cast<volatile void*>(ptr)), addend)"')
# Pass 2
#ADD_DEFINITIONS(-prof-use -prof-dir /tmp  -O3 -ipo -mtune=core2 -xT -unroll -fp-model fast=2 -rcd -no-prec-div -DLUX_USE_OPENGL -DHAVE_PTHREAD_H '-D"__sync_fetch_and_add(ptr,addend)=_InterlockedExchangeAdd(const_cast<void*>(reinterpret_cast<volatile void*>(ptr)), addend)"')
 
# Dade - Intel CC settings (single pass, 32bit)
#ADD_DEFINITIONS(-O3 -ip -mtune=core2 -xT -unroll -fp-model fast=2 -rcd -no-prec-div -DLUX_USE_OPENGL -DHAVE_PTHREAD_H '-D"__sync_fetch_and_add(ptr,addend)=_InterlockedExchangeAdd(const_cast<void*>(reinterpret_cast<volatile void*>(ptr)), addend)"')

# Dade - Intel CC settings (single pass, 64bit)
#ADD_DEFINITIONS(-O3 -ip -mtune=core2 -xT -unroll -fp-model fast=2 -rcd -no-prec-div -DLUX_USE_OPENGL -DHAVE_PTHREAD_H '-D"__sync_fetch_and_add(ptr,addend)=_InterlockedExchangeAdd(const_cast<void*>(reinterpret_cast<volatile void*>(ptr)), addend)"' -DBOOST_NO_INTRINSIC_INT64_T "-D'__builtin_vsnprintf(__out, __size, __fmt, __args)'='__builtin_vsnprintf(__out, __size, __fmt, (char *) __args)'") 

#ADD_DEFINITIONS(-O3 -msse -mfpmath=sse -ftree-vectorize -Wall -DLUX_USE_OPENGL -DHAVE_PTHREAD_H)
#ADD_DEFINITIONS(-O3 -march=athlon-xp -mmmx -m3dnow -msse -mfpmath=sse -ftree-vectorize -Wall -DLUX_USE_OPENGL -DHAVE_PTHREAD_H )
#ADD_DEFINITIONS(-O3 -Wall -msse -msse2 -DLUX_USE_SSE -DLUX_USE_OPENGL -DHAVE_PTHREAD_H)
#ADD_DEFINITIONS(-g -Wall -msse -DLUX_USE_OPENGL -DHAVE_PTHREAD_H)
#ICC definitions
#ADD_DEFINITIONS(-O3 -ipo -no-prec-div -static -fp-model fast=2 -rcd)

#############################################################################
#############################################################################
#########################      CUSTOM COMMAND     ###########################
#############################################################################
#############################################################################

# Create custom command for bison/yacc
BISON_TARGET(LuxParser ${CMAKE_SOURCE_DIR}/core/luxparse.y ${CMAKE_BINARY_DIR}/luxparse.cpp)
if(APPLE AND !APPLE_64)
	EXECUTE_PROCESS(COMMAND mv ${CMAKE_SOURCE_DIR}/luxparse.cpp.h ${CMAKE_BINARY_DIR}/luxparse.hpp)
ENDIF(APPLE AND !APPLE_64)
SET_SOURCE_FILES_PROPERTIES(${CMAKE_BINARY_DIR}/core/luxparse.cpp GENERATED)

# Create custom command for flex/lex
FLEX_TARGET(LuxLexer ${CMAKE_SOURCE_DIR}/core/luxlex.l ${CMAKE_BINARY_DIR}/luxlex.cpp)
SET_SOURCE_FILES_PROPERTIES(${CMAKE_BINARY_DIR}/luxlex.cpp GENERATED)
ADD_FLEX_BISON_DEPENDENCY(LuxLexer LuxParser)

#############################################################################
#############################################################################
##########  APPLE CUSTOM GUI_TYPE MACOSX_BUNDLE AND BUILD TARGETS ###########
#############################################################################
#############################################################################
IF(APPLE)
	SET(GUI_TYPE MACOSX_BUNDLE)
	# SET(MACOSX_BUNDLE_LONG_VERSION_STRING "${OPENSCENEGRAPH_MAJOR_VERSION}.${OPENSCENEGRAPH_MINOR_VERSION}.${OPENSCENEGRAPH_PATCH_VERSION}")
	# Short Version is the "marketing version". It is the version
	# the user sees in an information panel.
	SET(MACOSX_BUNDLE_SHORT_VERSION_STRING "${VERSION}")
	# Bundle version is the version the OS looks at.
	SET(MACOSX_BUNDLE_BUNDLE_VERSION "${VERSION}")
	SET(MACOSX_BUNDLE_GUI_IDENTIFIER "org.luxrender.luxrender" )
	SET(MACOSX_BUNDLE_BUNDLE_NAME "Luxrender" )
	SET(MACOSX_BUNDLE_ICON_FILE "luxrender.icns")
	# SET(MACOSX_BUNDLE_COPYRIGHT "")
	# SET(MACOSX_BUNDLE_INFO_STRING "Info string, localized?")
	IF(OSX_OPTION_DYNAMIC_BUILD)	
		ADD_CUSTOM_TARGET(DYNAMIC_BUILD DEPENDS luxShared luxrender luxconsole luxmerger luxcomp pylux )
		ADD_CUSTOM_COMMAND(
			TARGET DYNAMIC_BUILD POST_BUILD
			COMMAND rm -rf release/luxrender.app/Contents/Resources
			COMMAND mkdir release/luxrender.app/Contents/Resources
			COMMAND cp ../macos/icons/luxrender.icns release/luxrender.app/Contents/Resources
			COMMAND cp ../macos/icons/luxscene.icns release/luxrender.app/Contents/Resources
			COMMAND cp ../macos/icons/luxfilm.icns release/luxrender.app/Contents/Resources
			COMMAND cp ../macos/plists/09/Info.plist release/luxrender.app/Contents
			COMMAND install_name_tool -change ${CMAKE_BINARY_DIR}/Release/liblux.dylib @loader_path/liblux.dylib release/luxrender.app/Contents/MacOS/luxrender
			COMMAND mv release/luxrender.app release/LuxRender.app
#			COMMAND macdeployqt release/LuxRender.app ### uncomment for bundling Qt frameworks ###
			COMMAND install_name_tool -change ${CMAKE_BINARY_DIR}/Release/liblux.dylib @loader_path/liblux.dylib Release/luxconsole
			COMMAND mv release/luxconsole ${CMAKE_BINARY_DIR}/Release/LuxRender.app/Contents/MacOS/luxconsole
			COMMAND install_name_tool -change ${CMAKE_BINARY_DIR}/Release/liblux.dylib @loader_path/liblux.dylib Release/luxcomp
			COMMAND mv release/luxcomp ${CMAKE_BINARY_DIR}/Release/LuxRender.app/Contents/MacOS/luxcomp
			COMMAND install_name_tool -change ${CMAKE_BINARY_DIR}/Release/liblux.dylib @loader_path/liblux.dylib Release/luxmerger
			COMMAND mv release/luxmerger ${CMAKE_BINARY_DIR}/Release/LuxRender.app/Contents/MacOS/luxmerger
			COMMAND install_name_tool -id @loader_path/liblux.dylib release/liblux.dylib
			COMMAND mv release/liblux.dylib ${CMAKE_BINARY_DIR}/Release/LuxRender.app/Contents/MacOS/liblux.dylib
			)

	ELSE(OSX_OPTION_DYNAMIC_BUILD)
		ADD_CUSTOM_TARGET(STATIC_BUILD DEPENDS luxrender luxconsole luxmerger luxcomp pylux)
		ADD_CUSTOM_COMMAND(
			TARGET STATIC_BUILD POST_BUILD
			COMMAND rm -rf release/luxrender.app/Contents/Resources
			COMMAND mkdir release/luxrender.app/Contents/Resources
			COMMAND cp ../macos/icons/luxrender.icns release/luxrender.app/Contents/Resources
			COMMAND cp ../macos/icons/luxscene.icns release/luxrender.app/Contents/Resources
			COMMAND cp ../macos/icons/luxfilm.icns release/luxrender.app/Contents/Resources
			COMMAND cp ../macos/plists/09/Info.plist release/luxrender.app/Contents
			COMMAND mv release/luxrender.app release/LuxRender.app
			COMMAND mv release/luxconsole ${CMAKE_BINARY_DIR}/Release/LuxRender.app/Contents/MacOS/luxconsole
			COMMAND mv release/luxmerger ${CMAKE_BINARY_DIR}/Release/LuxRender.app/Contents/MacOS/luxmerger
			COMMAND mv release/luxcomp ${CMAKE_BINARY_DIR}/Release/LuxRender.app/Contents/MacOS/luxcomp
#			COMMAND macdeployqt release/LuxRender.app ### uncomment for bundling Qt frameworks ###
			)

	ENDIF(OSX_OPTION_DYNAMIC_BUILD)
ENDIF(APPLE)

#############################################################################
#############################################################################
#####################  SOURCE FILES FOR static liblux.a  ####################
#############################################################################
#############################################################################

SET(lux_core_reflection_src
	core/reflection/bxdf.cpp
	core/reflection/fresnel.cpp
	core/reflection/microfacetdistribution.cpp
	core/reflection/bxdf/asperity.cpp
	core/reflection/bxdf/brdftobtdf.cpp
	core/reflection/bxdf/cooktorrance.cpp
	core/reflection/bxdf/fresnelblend.cpp
	core/reflection/bxdf/lafortune.cpp
	core/reflection/bxdf/lambertian.cpp
	core/reflection/bxdf/microfacet.cpp
	core/reflection/bxdf/nulltransmission.cpp
	core/reflection/bxdf/orennayar.cpp
	core/reflection/bxdf/schlickbrdf.cpp
	core/reflection/bxdf/schlickscatter.cpp
	core/reflection/bxdf/schlicktranslucentbtdf.cpp
	core/reflection/bxdf/specularreflection.cpp
	core/reflection/bxdf/speculartransmission.cpp
	core/reflection/fresnel/fresnelcauchy.cpp
	core/reflection/fresnel/fresnelconductor.cpp
	core/reflection/fresnel/fresneldielectric.cpp
	core/reflection/fresnel/fresnelgeneral.cpp
	core/reflection/fresnel/fresnelnoop.cpp
	core/reflection/fresnel/fresnelslick.cpp
	core/reflection/microfacetdistribution/anisotropic.cpp
	core/reflection/microfacetdistribution/beckmann.cpp
	core/reflection/microfacetdistribution/blinn.cpp
	core/reflection/microfacetdistribution/schlickdistribution.cpp
	core/reflection/microfacetdistribution/wardisotropic.cpp
	)
SET(lux_core_src
	${CMAKE_BINARY_DIR}/luxparse.cpp
	${CMAKE_BINARY_DIR}/luxlex.cpp
	core/api.cpp
	core/camera.cpp
	core/cameraresponse.cpp
	core/color.cpp
	core/stats.cpp
	core/context.cpp
	core/contribution.cpp
	core/dynload.cpp
	core/epsilon.cpp
	core/exrio.cpp
	core/filedata.cpp
	core/film.cpp
	core/geometry/bbox.cpp
	core/geometry/matrix4x4.cpp
	core/geometry/quaternion.cpp
	core/geometry/raydifferential.cpp
	core/geometry/transform.cpp
	core/igiio.cpp
	core/imagereader.cpp
	core/light.cpp
	core/material.cpp
	core/mc.cpp
	core/motionsystem.cpp
	core/osfunc.cpp
	core/paramset.cpp
	core/photonmap.cpp
	core/pngio.cpp
	core/primitive.cpp
	core/queryable/queryable.cpp
	core/queryable/queryableattribute.cpp
	core/queryable/queryableregistry.cpp
	${lux_core_reflection_src}
	core/renderfarm.cpp
	core/renderinghints.cpp
	core/sampling.cpp
	core/scene.cpp
	core/shape.cpp
	core/spd.cpp
	core/spectrum.cpp
	core/spectrumwavelengths.cpp
	core/texture.cpp
	core/tgaio.cpp
	core/timer.cpp
	core/transport.cpp
	core/util.cpp
	core/volume.cpp
	server/renderserver.cpp
	)
SET(lux_accelerators_src
	accelerators/bruteforce.cpp
	accelerators/bvhaccel.cpp
	accelerators/qbvhaccel.cpp
	accelerators/tabreckdtree.cpp
	accelerators/unsafekdtree.cpp
	)
SET(lux_cameras_src
	cameras/environment.cpp
	cameras/perspective.cpp
	cameras/orthographic.cpp
	cameras/realistic.cpp
	)
SET(lux_films_src
	film/fleximage.cpp
	)
SET(lux_filters_src
	filters/box.cpp
	filters/gaussian.cpp
	filters/mitchell.cpp
	filters/sinc.cpp
	filters/triangle.cpp
	)
SET(lux_integrators_src
	integrators/bidirectional.cpp
	integrators/directlighting.cpp
	integrators/distributedpath.cpp
	integrators/emission.cpp
	integrators/exphotonmap.cpp
	integrators/igi.cpp
	integrators/multi.cpp
	integrators/path.cpp
	integrators/single.cpp
	integrators/sppm.cpp
	)
SET(lux_lights_src
	lights/area.cpp
	lights/distant.cpp
	lights/infinite.cpp
	lights/infinitesample.cpp
	lights/pointlight.cpp
	lights/projection.cpp
	lights/sphericalfunction/photometricdata_ies.cpp
	lights/sphericalfunction/sphericalfunction.cpp
	lights/sphericalfunction/sphericalfunction_ies.cpp
	lights/spot.cpp
	lights/sky.cpp
	lights/sun.cpp
	)
SET(lux_materials_src
	materials/carpaint.cpp
	materials/glass.cpp
	materials/glass2.cpp
	materials/glossy.cpp
	materials/glossy2.cpp
	materials/glossytranslucent.cpp
	materials/matte.cpp
	materials/mattetranslucent.cpp
	materials/metal.cpp
	materials/mirror.cpp
	materials/mixmaterial.cpp
	materials/null.cpp
	materials/roughglass.cpp
	materials/scattermaterial.cpp
	materials/shinymetal.cpp
	materials/velvet.cpp
	)
SET(lux_pixelsamplers_src
	pixelsamplers/hilbertpx.cpp
	pixelsamplers/linear.cpp
	pixelsamplers/lowdiscrepancypx.cpp
	pixelsamplers/tilepx.cpp
	pixelsamplers/vegas.cpp
	)
SET(lux_samplers_src
	samplers/erpt.cpp
	samplers/lowdiscrepancy.cpp
	samplers/metrosampler.cpp
	samplers/random.cpp
	)
if(LUXRAYS_DISABLE_OPENCL)
SET(lux_renderers_src
	renderers/samplerrenderer.cpp
	renderers/sppmrenderer.cpp
	renderers/sppm/photonsampler.cpp
	renderers/sppm/lookupaccel.cpp
	renderers/sppm/hashgrid.cpp
	renderers/sppm/hitpoints.cpp
	renderers/sppm/hybridhashgrid.cpp
	renderers/sppm/kdtree.cpp
	)
else(LUXRAYS_DISABLE_OPENCL)
SET(lux_renderers_src
	renderers/samplerrenderer.cpp
	renderers/sppmrenderer.cpp
	renderers/sppm/photonsampler.cpp
	renderers/sppm/lookupaccel.cpp
	renderers/sppm/hashgrid.cpp
	renderers/sppm/hitpoints.cpp
	renderers/sppm/hybridhashgrid.cpp
	renderers/sppm/kdtree.cpp
	renderers/hybridrenderer.cpp
	renderers/hybridsamplerrenderer.cpp
	)
endif(LUXRAYS_DISABLE_OPENCL)
SET(lux_shapes_src
	shapes/cone.cpp
	shapes/cylinder.cpp
	shapes/disk.cpp
	shapes/heightfield.cpp
	shapes/hyperboloid.cpp
	shapes/lenscomponent.cpp
	shapes/loopsubdiv.cpp
	shapes/mesh.cpp
	shapes/meshbarytriangle.cpp
	shapes/meshmicrodisplacementtriangle.cpp
	shapes/meshquadrilateral.cpp
	shapes/meshwaldtriangle.cpp
	shapes/nurbs.cpp
	shapes/paraboloid.cpp
	shapes/plymesh.cpp
	shapes/plymesh/rply.c
	shapes/sphere.cpp
	shapes/stlmesh.cpp
	shapes/torus.cpp
	)
SET(lux_spds_src
	spds/blackbodyspd.cpp
	spds/equalspd.cpp
	spds/frequencyspd.cpp
	spds/gaussianspd.cpp
	spds/irregular.cpp
	spds/regular.cpp
	spds/rgbillum.cpp
	spds/rgbrefl.cpp
	)
SET(lux_blender_textures_src
	textures/blender_base.cpp
	textures/blender_blend.cpp
	textures/blender_clouds.cpp
	textures/blender_distortednoise.cpp
	textures/blender_magic.cpp
	textures/blender_marble.cpp
	textures/blender_musgrave.cpp
	textures/blender_noise.cpp
	textures/blender_noiselib.cpp
	textures/blender_stucci.cpp
	textures/blender_texlib.cpp
	textures/blender_voronoi.cpp
	textures/blender_wood.cpp
	)
SET(lux_uniform_textures_src
	textures/blackbody.cpp
	textures/constant.cpp
	textures/equalenergy.cpp
	textures/frequencytexture.cpp
	textures/gaussiantexture.cpp
	textures/irregulardata.cpp
	textures/lampspectrum.cpp
	textures/regulardata.cpp
	textures/tabulateddata.cpp
	)
SET(lux_fresnel_textures_src
	textures/cauchytexture.cpp
	textures/sellmeiertexture.cpp
	textures/tabulatedfresnel.cpp
	)
SET(lux_textures_src
	${lux_uniform_textures_src}
	${lux_blender_textures_src}
	${lux_fresnel_textures_src}
	textures/band.cpp
	textures/bilerp.cpp
	textures/brick.cpp
	textures/checkerboard.cpp
	textures/dots.cpp
	textures/fbm.cpp
	textures/harlequin.cpp
	textures/imagemap.cpp
	textures/marble.cpp
	textures/mix.cpp
	textures/multimix.cpp
	textures/scale.cpp
	textures/uv.cpp
	textures/uvmask.cpp
	textures/windy.cpp
	textures/wrinkled.cpp
	)
SET(lux_tonemaps_src
	tonemaps/contrast.cpp
	tonemaps/lineartonemap.cpp
	tonemaps/maxwhite.cpp
	tonemaps/nonlinear.cpp
	tonemaps/reinhard.cpp
	)
SET(lux_volumes_src
	volumes/clearvolume.cpp
	volumes/cloud.cpp
	volumes/exponential.cpp
	volumes/homogeneous.cpp
	volumes/volumegrid.cpp
	)

SET(lux_lib_src
	${lux_core_src}
	${lux_accelerators_src}
	${lux_cameras_src}
	${lux_films_src}
	${lux_filters_src}
	${lux_integrators_src}
	${lux_lights_src}
	${lux_materials_src}
	${lux_pixelsamplers_src}
	${lux_renderers_src}
	${lux_samplers_src}
	${lux_shapes_src}
	${lux_spds_src}
	${lux_textures_src}
	${lux_tonemaps_src}
	${lux_volumes_src}
	)
INCLUDE_DIRECTORIES(SYSTEM
	${Boost_INCLUDE_DIRS}
	${CMAKE_SOURCE_DIR}/core/external
	${PNG_INCLUDE_DIR}
	${OPENEXR_INCLUDE_DIRS}
	${LUXRAYS_INCLUDE_DIRS}
	${OPENCL_INCLUDE_DIRS}
	)
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/core
	${CMAKE_SOURCE_DIR}/core/queryable
	${CMAKE_SOURCE_DIR}/core/reflection
	${CMAKE_SOURCE_DIR}/core/reflection/bxdf
	${CMAKE_SOURCE_DIR}/core/reflection/fresnel
	${CMAKE_SOURCE_DIR}/core/reflection/microfacetdistribution
	${CMAKE_SOURCE_DIR}/spds
	${CMAKE_SOURCE_DIR}/lights/sphericalfunction
	${CMAKE_SOURCE_DIR}
	${CMAKE_BINARY_DIR}
	)

SET( LUXQTGUI_SRCS
	qtgui/main.cpp
	qtgui/histogramview.cpp
	qtgui/mainwindow.cpp
	qtgui/renderview.cpp
	qtgui/luxapp.cpp
	qtgui/aboutdialog.cpp
	qtgui/advancedinfowidget.cpp
	qtgui/lightgroupwidget.cpp
	qtgui/tonemapwidget.cpp
	qtgui/lenseffectswidget.cpp
	qtgui/colorspacewidget.cpp
	qtgui/gammawidget.cpp
	qtgui/noisereductionwidget.cpp
	qtgui/histogramwidget.cpp
	qtgui/panewidget.cpp
	qtgui/batchprocessdialog.cpp
	qtgui/openexroptionsdialog.cpp
	qtgui/guiutil.cpp
	)
SET( LUXQTGUI_MOC
	qtgui/histogramview.hxx
	qtgui/mainwindow.hxx
	qtgui/aboutdialog.hxx
	qtgui/advancedinfowidget.hxx
	qtgui/luxapp.hxx
	qtgui/renderview.hxx
	qtgui/lightgroupwidget.hxx
	qtgui/tonemapwidget.hxx
	qtgui/lenseffectswidget.hxx
	qtgui/colorspacewidget.hxx
	qtgui/gammawidget.hxx
	qtgui/noisereductionwidget.hxx
	qtgui/histogramwidget.hxx
	qtgui/panewidget.hxx
	qtgui/batchprocessdialog.hxx
	qtgui/openexroptionsdialog.hxx
	)
SET(LUXQTGUI_UIS
	qtgui/luxrender.ui
	qtgui/aboutdialog.ui
	qtgui/advancedinfo.ui
	qtgui/lightgroup.ui
	qtgui/tonemap.ui
	qtgui/lenseffects.ui
	qtgui/colorspace.ui
	qtgui/gamma.ui
	qtgui/noisereduction.ui
	qtgui/histogram.ui
	qtgui/pane.ui
	qtgui/batchprocessdialog.ui
	qtgui/openexroptionsdialog.ui
	)
SET( LUXQTGUI_RCS
	qtgui/icons.qrc
	qtgui/splash.qrc
	qtgui/images.qrc
	)

#############################################################################
#############################################################################
#####################            LINKER INFO           ######################
#############################################################################
#############################################################################

#############################################################################
# Here we build the static core library liblux.a
#############################################################################

LINK_DIRECTORIES(${LINK_DIRECTORIES} ${Boost_LIBRARY_DIRS} )

ADD_LIBRARY(luxStatic STATIC ${lux_lib_src} )
IF( NOT CMAKE_VERSION VERSION_LESS 2.8.3 AND OSX_OPTION_CLANG) # only cmake >= 2.8.1 supports per target attributes
	SET_TARGET_PROPERTIES(luxStatic PROPERTIES XCODE_ATTRIBUTE_GCC_VERSION com.apple.compilers.llvm.clang.1_0) # for testing new CLANG2.0, will be ignored for other OS
	SET_TARGET_PROPERTIES(luxStatic PROPERTIES XCODE_ATTRIBUTE_LLVM_LTO NO ) # disabled due breaks bw compatibility
ENDIF()
#TARGET_LINK_LIBRARIES(luxStatic ${FREEIMAGE_LIBRARIES} ${Boost_LIBRARIES} )

#############################################################################
# Here we build the shared core library liblux.so
#############################################################################

ADD_LIBRARY(luxShared SHARED
	cpp_api/lux_api.cpp
	cpp_api/lux_wrapper_factories.cpp
)
IF(APPLE)
	IF( NOT CMAKE_VERSION VERSION_LESS 2.8.3 AND OSX_OPTION_CLANG) # only cmake >= 2.8.1 supports per target attributes
		SET_TARGET_PROPERTIES(luxShared PROPERTIES XCODE_ATTRIBUTE_GCC_VERSION com.apple.compilers.llvm.clang.1_0) # for testing new CLANG2.0, will be ignored for other OS
		SET_TARGET_PROPERTIES(luxShared PROPERTIES XCODE_ATTRIBUTE_LLVM_LTO NO ) # disabled due breaks bw compatibility
	ENDIF()
	TARGET_LINK_LIBRARIES(luxShared -all_load luxStatic -noall_load ${LUXRAYS_LIBRARY} ${OPENCL_LIBRARY} ${OPENGL_LIBRARY} ${FREEIMAGE_LIBRARIES} ${Boost_LIBRARIES} ${SYS_LIBRARIES} )
ELSE(APPLE)
	TARGET_LINK_LIBRARIES(luxShared -Wl,--whole-archive luxStatic -Wl,--no-whole-archive ${LUXRAYS_LIBRARY} ${OPENCL_LIBRARY} ${OPENGL_LIBRARY} ${FREEIMAGE_LIBRARIES} ${Boost_LIBRARIES} ${SYS_LIBRARIES} ${OPENEXR_LIBRARIES} ${PNG_LIBRARY})
ENDIF(APPLE)

# Make CMake output both libs with the same name
SET_TARGET_PROPERTIES(luxStatic luxShared PROPERTIES OUTPUT_NAME lux)

#############################################################################
#Here we build the python module
#############################################################################

#############################################################################
#############################################################################
###########################   PYTHON LIBRARIES    ###########################
#############################################################################
#############################################################################

# SET(CMAKE_USE_PYTHON_VERSION 3.2)

IF(APPLE)
	IF(OSX_OPTION_PYLUX)
		# use Blender python libs for static compiling !
		SET(PYTHON_LIBRARIES ${CMAKE_SOURCE_DIR}/../macos/lib/BF_pythonlibs/py32_uni_intel/libbf_python_ext.a ${CMAKE_SOURCE_DIR}/../macos/lib/BF_pythonlibs/py32_uni_intel/libbf_python.a)
		SET(PYTHON_INCLUDE_PATH ${CMAKE_SOURCE_DIR}/../macos/include/Python3.2)
		SET(PYTHONLIBS_FOUND ON)
	ELSE(OSX_OPTION_PYLUX)
		# compile pylux for genral purpose against Python framework
		FIND_LIBRARY(PYTHON_LIBRARY Python )
		FIND_PATH(PYTHON_INCLUDE_PATH python.h )
		MARK_AS_ADVANCED (PYTHON_LIBRARY)
		SET(PYTHONLIBS_FOUND on)
		SET(PYTHON_LIBRARIES ${PYTHON_LIBRARY})
	ENDIF(OSX_OPTION_PYLUX)
ELSE(APPLE)
	IF(PYTHON_CUSTOM)
		IF (NOT PYTHON_LIBRARIES)
			MESSAGE(FATAL_ERROR " PYTHON_CUSTOM set but PYTHON_LIBRARIES NOT set.")
		ENDIF (NOT PYTHON_LIBRARIES)
		IF (NOT PYTHON_INCLUDE_PATH)
			MESSAGE(FATAL_ERROR " PYTHON_CUSTOM set but PYTHON_INCLUDE_PATH NOT set.")
		ENDIF (NOT PYTHON_INCLUDE_PATH)
	ELSE(PYTHON_CUSTOM)
		FIND_PACKAGE(PythonLibs)
	ENDIF(PYTHON_CUSTOM)
ENDIF(APPLE)

IF(PYTHONLIBS_FOUND OR PYTHON_CUSTOM)

	MESSAGE(STATUS "Python library directory: " ${PYTHON_LIBRARIES} )
	MESSAGE(STATUS "Python include directory: " ${PYTHON_INCLUDE_PATH} )

	INCLUDE_DIRECTORIES(SYSTEM ${PYTHON_INCLUDE_PATH})
	IF(APPLE)
		ADD_LIBRARY(pylux MODULE python/binding.cpp)
		IF( NOT CMAKE_VERSION VERSION_LESS 2.8.3) # only cmake >= 2.8.3 supports per target attributes
		 SET_TARGET_PROPERTIES(pylux PROPERTIES XCODE_ATTRIBUTE_DEPLOYMENT_POSTPROCESSING NO) # exclude pylux from strip symbols
		ENDIF( NOT CMAKE_VERSION VERSION_LESS 2.8.3)
		TARGET_LINK_LIBRARIES(pylux -Wl,-undefined -Wl,dynamic_lookup -all_load luxStatic -noall_load ${CMAKE_THREAD_LIBS_INIT} ${LUXRAYS_LIBRARY} ${OPENCL_LIBRARY} ${OPENGL_LIBRARY} ${FREEIMAGE_LIBRARIES} ${Boost_LIBRARIES} ${EXTRA_LIBS} ${SYS_LIBRARIES} ${PYTHON_LIBRARIES} ${Boost_python_LIBRARIES})
		ADD_CUSTOM_COMMAND(
			TARGET pylux POST_BUILD
			COMMAND mv release/libpylux.so release/pylux.so
		)
		ADD_CUSTOM_COMMAND(
			TARGET pylux POST_BUILD
			COMMAND cp ${CMAKE_SOURCE_DIR}/python/pyluxconsole.py release/pyluxconsole.py
		)
	ELSE(APPLE)
		ADD_LIBRARY(pylux SHARED python/binding.cpp)
		TARGET_LINK_LIBRARIES(pylux -Wl,--whole-archive luxStatic -Wl,--no-whole-archive ${CMAKE_THREAD_LIBS_INIT} ${LUXRAYS_LIBRARY} ${OPENCL_LIBRARY} ${OPENGL_LIBRARY} ${FREEIMAGE_LIBRARIES} ${Boost_LIBRARIES} ${SYS_LIBRARIES} ${PYTHON_LIBRARIES} ${Boost_python_LIBRARIES} ${OPENEXR_LIBRARIES} ${PNG_LIBRARY})
		IF(CYGWIN)
			ADD_CUSTOM_COMMAND(
				TARGET pylux POST_BUILD
				COMMAND mv cygpylux.dll pylux.dll
			)
		ELSE(CYGWIN)
			ADD_CUSTOM_COMMAND(
				TARGET pylux POST_BUILD
				COMMAND mv libpylux.so pylux.so
			)
		ENDIF(CYGWIN)
		ADD_CUSTOM_COMMAND(
			TARGET pylux POST_BUILD
			COMMAND cp ${CMAKE_SOURCE_DIR}/python/pyluxconsole.py pyluxconsole.py
		)
	ENDIF(APPLE)
ELSE(PYTHONLIBS_FOUND OR PYTHON_CUSTOM)
	MESSAGE( STATUS "Warning : could not find Python libraries - not building python module")
ENDIF(PYTHONLIBS_FOUND OR PYTHON_CUSTOM)

#############################################################################
#Here we build the console executable
#############################################################################
ADD_EXECUTABLE(luxconsole console/luxconsole.cpp)
IF(APPLE)
	IF(OSX_OPTION_DYNAMIC_BUILD)
		TARGET_LINK_LIBRARIES(luxconsole ${OSX_SHARED_CORELIB} ${CMAKE_THREAD_LIBS_INIT} )
	ELSE(OSX_OPTION_DYNAMIC_BUILD)
		TARGET_LINK_LIBRARIES(luxconsole -Wl,-undefined -Wl,dynamic_lookup -all_load luxStatic -noall_load ${CMAKE_THREAD_LIBS_INIT} ${LUXRAYS_LIBRARY} ${OPENCL_LIBRARY} ${OPENGL_LIBRARY} ${FREEIMAGE_LIBRARIES} ${SYS_LIBRARIES} ${Boost_LIBRARIES} )
	ENDIF(OSX_OPTION_DYNAMIC_BUILD)
ELSE(APPLE)
	TARGET_LINK_LIBRARIES(luxconsole -Wl,--whole-archive luxStatic -Wl,--no-whole-archive ${CMAKE_THREAD_LIBS_INIT} ${LUXRAYS_LIBRARY} ${OPENCL_LIBRARY} ${OPENGL_LIBRARY} ${FREEIMAGE_LIBRARIES} ${Boost_LIBRARIES}  ${SYS_LIBRARIES} ${OPENEXR_LIBRARIES} ${PNG_LIBRARY})
ENDIF(APPLE)

#############################################################################
#Here we build the LuxMerger executable
#############################################################################
ADD_EXECUTABLE(luxmerger tools/luxmerger.cpp)
IF(APPLE)
	IF(OSX_OPTION_DYNAMIC_BUILD)
		TARGET_LINK_LIBRARIES(luxmerger ${OSX_SHARED_CORELIB} ${CMAKE_THREAD_LIBS_INIT} )
	ELSE(OSX_OPTION_DYNAMIC_BUILD)
		TARGET_LINK_LIBRARIES(luxmerger -Wl,-undefined -Wl,dynamic_lookup -all_load luxStatic -noall_load ${CMAKE_THREAD_LIBS_INIT} ${LUXRAYS_LIBRARY} ${OPENCL_LIBRARY} ${OPENGL_LIBRARY} ${FREEIMAGE_LIBRARIES} ${SYS_LIBRARIES} ${Boost_LIBRARIES} )
	ENDIF(OSX_OPTION_DYNAMIC_BUILD)
ELSE(APPLE)
	TARGET_LINK_LIBRARIES(luxmerger -Wl,--whole-archive luxStatic -Wl,--no-whole-archive ${CMAKE_THREAD_LIBS_INIT} ${LUXRAYS_LIBRARY} ${OPENCL_LIBRARY} ${OPENGL_LIBRARY} ${FREEIMAGE_LIBRARIES} ${Boost_LIBRARIES}  ${SYS_LIBRARIES} ${OPENEXR_LIBRARIES} ${PNG_LIBRARY})
ENDIF(APPLE)

#############################################################################
#Here we build the LuxComp executable
#############################################################################
ADD_EXECUTABLE(luxcomp tools/luxcomp.cpp)
IF(APPLE)
	IF(OSX_OPTION_DYNAMIC_BUILD)
		TARGET_LINK_LIBRARIES(luxcomp ${OSX_SHARED_CORELIB} ${CMAKE_THREAD_LIBS_INIT} )
	ELSE(OSX_OPTION_DYNAMIC_BUILD)
		TARGET_LINK_LIBRARIES(luxcomp -Wl,-undefined -Wl,dynamic_lookup -all_load luxStatic -noall_load ${CMAKE_THREAD_LIBS_INIT} ${LUXRAYS_LIBRARY} ${OPENCL_LIBRARY} ${OPENGL_LIBRARY} ${FREEIMAGE_LIBRARIES} ${SYS_LIBRARIES} ${Boost_LIBRARIES} )
	ENDIF(OSX_OPTION_DYNAMIC_BUILD)
ELSE(APPLE)
	TARGET_LINK_LIBRARIES(luxcomp -Wl,--whole-archive luxStatic -Wl,--no-whole-archive ${CMAKE_THREAD_LIBS_INIT} ${LUXRAYS_LIBRARY} ${OPENCL_LIBRARY} ${OPENGL_LIBRARY} ${FREEIMAGE_LIBRARIES} ${Boost_LIBRARIES}  ${SYS_LIBRARIES} ${OPENEXR_LIBRARIES} ${PNG_LIBRARY})
ENDIF(APPLE)

#############################################################################
#Here we build the QT4 GUI executable
#############################################################################

#############################################################################
#############################################################################
############################   QT4 LIBRARIES    #############################
#############################################################################
#############################################################################

FIND_PACKAGE(Qt4 4.6.0 COMPONENTS QtCore QtGui)
IF(QT4_FOUND)
	MESSAGE(STATUS "Qt library directory: " ${QT_LIBRARY_DIR} )
	MESSAGE( STATUS "Qt include directory: " ${QT_INCLUDE_DIR} )
	INCLUDE(${QT_USE_FILE})

	QT4_ADD_RESOURCES( LUXQTGUI_RC_SRCS ${LUXQTGUI_RCS}) 
	QT4_WRAP_UI( LUXQTGUI_UI_HDRS ${LUXQTGUI_UIS} )
	QT4_WRAP_CPP( LUXQTGUI_MOC_SRCS ${LUXQTGUI_MOC} )

	#file (GLOB TRANSLATIONS_FILES qtgui/translations/*.ts)
	#qt4_create_translation(QM_FILES ${FILES_TO_TRANSLATE} ${TRANSLATIONS_FILES})

	#ADD_EXECUTABLE(luxrender ${GUI_TYPE} ${LUXQTGUI_SRCS} ${LUXQTGUI_MOC_SRCS} ${LUXQTGUI_RC_SRCS} ${LUXQTGUI_UI_HDRS} ${QM_FILES})
	ADD_EXECUTABLE(luxrender ${GUI_TYPE} ${LUXQTGUI_SRCS} ${LUXQTGUI_MOC_SRCS} ${LUXQTGUI_RC_SRCS} ${LUXQTGUI_UI_HDRS})
	IF(APPLE)
		IF( NOT CMAKE_VERSION VERSION_LESS 2.8.3) # only cmake >= 2.8.1 supports per target attributes
			SET_TARGET_PROPERTIES(luxrender PROPERTIES XCODE_ATTRIBUTE_GCC_VERSION 4.2) # QT will not play with xcode4 compiler default llvm-gcc-4.2 !
		ENDIF( NOT CMAKE_VERSION VERSION_LESS 2.8.3)
		INCLUDE_DIRECTORIES (SYSTEM /Developer/Headers/FlatCarbon /usr/local/include)
		FIND_LIBRARY(CARBON_LIBRARY Carbon)
		FIND_LIBRARY(QT_LIBRARY QtCore QtGui)
		FIND_LIBRARY(AGL_LIBRARY AGL )
		FIND_LIBRARY(APP_SERVICES_LIBRARY ApplicationServices )

		MESSAGE(STATUS ${CARBON_LIBRARY})
		MARK_AS_ADVANCED (CARBON_LIBRARY)
		MARK_AS_ADVANCED (QT_LIBRARY)
		MARK_AS_ADVANCED (AGL_LIBRARY)
		MARK_AS_ADVANCED (APP_SERVICES_LIBRARY)
		SET(EXTRA_LIBS ${CARBON_LIBRARY} ${AGL_LIBRARY} ${APP_SERVICES_LIBRARY})
		IF(OSX_OPTION_DYNAMIC_BUILD)
			TARGET_LINK_LIBRARIES(luxrender ${OSX_SHARED_CORELIB} ${QT_LIBRARIES} ${EXTRA_LIBS} )
		ELSE(OSX_OPTION_DYNAMIC_BUILD)
			TARGET_LINK_LIBRARIES(luxrender -Wl,-undefined -Wl,dynamic_lookup -all_load luxStatic -noall_load ${QT_LIBRARIES} ${LUXRAYS_LIBRARY} ${OPENCL_LIBRARY} ${OPENGL_LIBRARY} ${FREEIMAGE_LIBRARIES} ${Boost_LIBRARIES} ${EXTRA_LIBS} ${SYS_LIBRARIES} )
		ENDIF(OSX_OPTION_DYNAMIC_BUILD)
	ELSE(APPLE)
	TARGET_LINK_LIBRARIES(luxrender -Wl,--whole-archive luxStatic -Wl,--no-whole-archive ${QT_LIBRARIES} ${LUXRAYS_LIBRARY} ${OPENCL_LIBRARY} ${OPENGL_LIBRARY} ${FREEIMAGE_LIBRARIES} ${Boost_LIBRARIES} ${ZLIB_LIBRARIES} ${BZ2_LIBRARIES}  ${SYS_LIBRARIES} ${OPENEXR_LIBRARIES} ${PNG_LIBRARY})
	ENDIF(APPLE)
ELSE(QT4_FOUND)
	MESSAGE( STATUS "Warning : could not find Qt - not building Qt GUI")
ENDIF(QT4_FOUND)

#############################################################################

#Install target
IF(APPLE)

#	TODO: custom install

ELSE(APPLE)
	INSTALL(TARGETS luxconsole luxmerger RUNTIME DESTINATION bin)
	IF(QT4_FOUND)
		INSTALL(TARGETS luxrender RUNTIME DESTINATION bin)
	ENDIF(QT4_FOUND)

#Install API/Library
INSTALL(FILES ${CMAKE_SOURCE_DIR}/core/api.h DESTINATION include/luxrender/)
INSTALL(TARGETS luxStatic DESTINATION lib${LIB_SUFFIX})
#	ELSE(FIND_LIBRARY_USE_LIB64_PATHS) ARCHIVE DESTINATION lib
#	ENDIF(FIND_LIBRARY_USE_LIB64_PATHS)


#Install Desktop files
INSTALL(FILES ${CMAKE_SOURCE_DIR}/wxgui/luxrender.svg DESTINATION share/pixmaps/)
INSTALL(FILES ${CMAKE_SOURCE_DIR}/wxgui/luxrender.desktop DESTINATION share/applications/)

#Source package target
ADD_CUSTOM_TARGET(package mkdir lux-${VERSION}
	COMMAND cp -R ${CMAKE_SOURCE_DIR}/* lux-${VERSION}
	COMMAND tar -cf ${CMAKE_BINARY_DIR}/lux-${VERSION}.tar lux-${VERSION}
	COMMAND bzip2 --best ${CMAKE_BINARY_DIR}/lux-${VERSION}.tar
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
	DEPENDS ${lux_lib_src}
	COMMENT Building source package
)
ENDIF(APPLE)
# Dade - GCC Profiling (remember to uncomment the line in the middle of file too)
#SET_TARGET_PROPERTIES(luxconsole luxmerger luxrender PROPERTIES LINK_FLAGS "-pg")

# Dade - GCC 2 pass optimization (remember to uncomment the line in the middle of file too)
#SET_TARGET_PROPERTIES(luxconsole luxmerger luxrender PROPERTIES LINK_FLAGS "--coverage")

# Dade - Intel Compiler optimization
#REMOVE_DEFINITIONS(-ipo)

IF(APPLE)
	MESSAGE(STATUS "")
	MESSAGE(STATUS "################ GENERATED XCODE PROJECT INFORMATION ################")
	MESSAGE(STATUS "")
	MESSAGE(STATUS "OSX_DEPLOYMENT_TARGET : " ${CMAKE_OSX_DEPLOYMENT_TARGET})
	IF(CMAKE_VERSION VERSION_LESS 2.8.1)
		MESSAGE(STATUS "Setting CMAKE_OSX_ARCHITECTURES ( cmake lower 2.8 method ): " ${CMAKE_OSX_ARCHITECTURES})
	ELSE(CMAKE_VERSION VERSION_LESS 2.8.1)
		MESSAGE(STATUS "CMAKE_XCODE_ATTRIBUTE_ARCHS ( cmake 2.8 or higher method ): " ${CMAKE_XCODE_ATTRIBUTE_ARCHS})
	ENDIF(CMAKE_VERSION VERSION_LESS 2.8.1)
	MESSAGE(STATUS "OSX SDK SETTING : " ${CMAKE_OSX_SYSROOT})
	MESSAGE(STATUS "BUILD_CONFIGURATION_TYPE : " ${CMAKE_CONFIGURATION_TYPES})
	MESSAGE(STATUS "OSX_CORE_BUILD_WITH_CLANG : " ${OSX_OPTION_CLANG})
	IF(OSX_OPTION_PYLUX)
		MESSAGE(STATUS "PYLUX CONFIGURED FOR BLENDER 2.5 USE")
	ELSE(OSX_OPTION_PYLUX)
		MESSAGE(STATUS "PYLUX CONFIGURED FOR GENERAL PURPOSE USE")
	ENDIF(OSX_OPTION_PYLUX)
	MESSAGE(STATUS "ALWAYS_BUILD_UNIVERSAL : " ${OSX_OPTION_UNIVERSAL})
	MESSAGE(STATUS "USE SHARED CORELIB : " ${OSX_OPTION_DYNAMIC_BUILD})
#	MESSAGE(STATUS "INSTALL_LOCATION : " ${OSX_INSTALL_PATH})
	MESSAGE(STATUS "")
	MESSAGE(STATUS "#####################################################################")
ENDIF(APPLE)
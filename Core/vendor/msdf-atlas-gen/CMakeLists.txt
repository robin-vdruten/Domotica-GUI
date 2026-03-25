cmake_minimum_required(VERSION 3.15)
project(msdf_all)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)

add_library(freetype STATIC)

set(FREETYPE_SRC
    msdfgen/freetype/src/autofit/autofit.c
    msdfgen/freetype/src/base/ftbase.c
    msdfgen/freetype/src/base/ftbbox.c
    msdfgen/freetype/src/base/ftbdf.c
    msdfgen/freetype/src/base/ftbitmap.c
    msdfgen/freetype/src/base/ftcid.c
    msdfgen/freetype/src/base/ftdebug.c
    msdfgen/freetype/src/base/ftfstype.c
    msdfgen/freetype/src/base/ftgasp.c
    msdfgen/freetype/src/base/ftglyph.c
    msdfgen/freetype/src/base/ftgxval.c
    msdfgen/freetype/src/base/ftinit.c
    msdfgen/freetype/src/base/ftmm.c
    msdfgen/freetype/src/base/ftotval.c
    msdfgen/freetype/src/base/ftpatent.c
    msdfgen/freetype/src/base/ftpfr.c
    msdfgen/freetype/src/base/ftstroke.c
    msdfgen/freetype/src/base/ftsynth.c
    msdfgen/freetype/src/base/ftsystem.c
    msdfgen/freetype/src/base/fttype1.c
    msdfgen/freetype/src/base/ftwinfnt.c
    msdfgen/freetype/src/bdf/bdf.c
    msdfgen/freetype/src/bzip2/ftbzip2.c
    msdfgen/freetype/src/cache/ftcache.c
    msdfgen/freetype/src/cff/cff.c
    msdfgen/freetype/src/cid/type1cid.c
    msdfgen/freetype/src/gzip/ftgzip.c
    msdfgen/freetype/src/lzw/ftlzw.c
    msdfgen/freetype/src/pcf/pcf.c
    msdfgen/freetype/src/pfr/pfr.c
    msdfgen/freetype/src/psaux/psaux.c
    msdfgen/freetype/src/pshinter/pshinter.c
    msdfgen/freetype/src/psnames/psnames.c
    msdfgen/freetype/src/raster/raster.c
    msdfgen/freetype/src/sdf/sdf.c
    msdfgen/freetype/src/sfnt/sfnt.c
    msdfgen/freetype/src/smooth/smooth.c
    msdfgen/freetype/src/truetype/truetype.c
    msdfgen/freetype/src/type1/type1.c
    msdfgen/freetype/src/type42/type42.c
    msdfgen/freetype/src/winfonts/winfnt.c
)

target_sources(freetype PRIVATE ${FREETYPE_SRC})

target_include_directories(freetype PUBLIC
    msdfgen/freetype/include
)

target_compile_definitions(freetype PUBLIC
    FT2_BUILD_LIBRARY
    _CRT_SECURE_NO_WARNINGS
    _CRT_NONSTDC_NO_WARNINGS
)

add_library(Freetype::Freetype ALIAS freetype)

add_library(msdfgen STATIC)
# ...rest of file unchanged...

file(GLOB_RECURSE MSDFGEN_SRC
    msdfgen/core/*.h
    msdfgen/core/*.hpp
    msdfgen/core/*.cpp
    msdfgen/ext/*.h
    msdfgen/ext/*.hpp
    msdfgen/ext/*.cpp
    msdfgen/lib/*.cpp
    msdfgen/include/*.h
)

target_sources(msdfgen PRIVATE ${MSDFGEN_SRC})

target_include_directories(msdfgen PUBLIC
    msdfgen/include
)

target_compile_definitions(msdfgen PUBLIC
    MSDFGEN_USE_CPP11
)

target_link_libraries(msdfgen PRIVATE Freetype::Freetype)

# ── msdf-atlas-gen ──
add_library(msdf-atlas-gen STATIC)

file(GLOB_RECURSE MSDF_ATLAS_GEN_SRC
    msdf-atlas-gen/*.h
    msdf-atlas-gen/*.hpp
    msdf-atlas-gen/*.cpp
)

target_sources(msdf-atlas-gen PRIVATE ${MSDF_ATLAS_GEN_SRC})

target_include_directories(msdf-atlas-gen PUBLIC
    msdf-atlas-gen
    msdfgen
    msdfgen/include
)

target_compile_definitions(msdf-atlas-gen PUBLIC
    _CRT_SECURE_NO_WARNINGS
)

target_link_libraries(msdf-atlas-gen PUBLIC
    msdfgen
)

message(STATUS "✓ FreeType, msdfgen and msdf-atlas-gen configured")
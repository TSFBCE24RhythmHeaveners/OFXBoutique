cmake_minimum_required(VERSION 3.16)
project(boutique_ofx CXX)

# purzOS OFX plugins — native DaVinci Resolve effects ported from the web tools.
# Builds the vendored OFX C++ Support library once, then each plugin as an
# `.ofx` module bundled into <Name>.ofx.bundle/Contents/Win64/<Name>.ofx.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Plugins are shared modules; the static support lib linked into them must be
# position-independent or the GCC/Clang linker rejects it (R_X86_64_PC32 / -fPIC).
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(OFX "${CMAKE_CURRENT_SOURCE_DIR}/openfx")
set(OFX_INCLUDES
  "${OFX}/include"
  "${OFX}/Support/include"
  "${OFX}/Support/Plugins/include")

# --- OFX C++ Support library (static) ---------------------------------------
file(GLOB SUPPORT_SRC "${OFX}/Support/Library/*.cpp")
add_library(ofxsupport STATIC ${SUPPORT_SRC})
target_include_directories(ofxsupport PUBLIC ${OFX_INCLUDES})
if(WIN32)
  target_compile_definitions(ofxsupport PUBLIC NOMINMAX WIN32 _WINDOWS WINDOWS)
endif()

# --- arch subdir inside the bundle ------------------------------------------
if(WIN32)
  set(OFX_ARCH "Win64")
elseif(APPLE)
  set(OFX_ARCH "MacOS")
else()
  set(OFX_ARCH "Linux-x86-64")
endif()

# add_ofx_plugin(<TargetName> <source.cpp>...)
function(add_ofx_plugin name)
  add_library(${name} MODULE ${ARGN})
  target_link_libraries(${name} PRIVATE ofxsupport)
  target_include_directories(${name} PRIVATE ${OFX_INCLUDES})
  if(WIN32)
    target_compile_definitions(${name} PRIVATE NOMINMAX WIN32 _WINDOWS WINDOWS)
  endif()
  set_target_properties(${name} PROPERTIES PREFIX "" SUFFIX ".ofx")
  set(contents "${CMAKE_BINARY_DIR}/bundles/${name}.ofx.bundle/Contents")
  set(bundle "${contents}/${OFX_ARCH}")
  add_custom_command(TARGET ${name} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${bundle}"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${name}>" "${bundle}/${name}.ofx"
    COMMENT "Bundling ${name}.ofx")

  # macOS bundles need an Info.plist or the host silently ignores them.
  if(APPLE)
    set(plist "${CMAKE_BINARY_DIR}/plists/${name}.Info.plist")
    file(WRITE "${plist}"
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">
<plist version=\"1.0\">
<dict>
	<key>CFBundleName</key><string>BoutiqueFX ${name}</string>
	<key>CFBundleExecutable</key><string>${name}.ofx</string>
	<key>CFBundleIdentifier</key><string>com.boutiquefx.${name}</string>
	<key>CFBundlePackageType</key><string>BNDL</string>
	<key>CFBundleVersion</key><string>1.0</string>
	<key>CFBundleShortVersionString</key><string>1.0</string>
</dict>
</plist>
")
    add_custom_command(TARGET ${name} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy "${plist}" "${contents}/Info.plist"
      COMMENT "Adding Info.plist to ${name}.ofx.bundle")
  endif()
endfunction()

add_ofx_plugin(BayerDither "${CMAKE_CURRENT_SOURCE_DIR}/dither/dither.cpp")

# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "C:/Users/CmonMeow/Desktop/Shipwright/build/x64/_deps/stormlib-src")
  file(MAKE_DIRECTORY "C:/Users/CmonMeow/Desktop/Shipwright/build/x64/_deps/stormlib-src")
endif()
file(MAKE_DIRECTORY
  "C:/Users/CmonMeow/Desktop/Shipwright/build/x64/_deps/stormlib-build"
  "C:/Users/CmonMeow/Desktop/Shipwright/build/x64/_deps/stormlib-subbuild/stormlib-populate-prefix"
  "C:/Users/CmonMeow/Desktop/Shipwright/build/x64/_deps/stormlib-subbuild/stormlib-populate-prefix/tmp"
  "C:/Users/CmonMeow/Desktop/Shipwright/build/x64/_deps/stormlib-subbuild/stormlib-populate-prefix/src/stormlib-populate-stamp"
  "C:/Users/CmonMeow/Desktop/Shipwright/build/x64/_deps/stormlib-subbuild/stormlib-populate-prefix/src"
  "C:/Users/CmonMeow/Desktop/Shipwright/build/x64/_deps/stormlib-subbuild/stormlib-populate-prefix/src/stormlib-populate-stamp"
)

set(configSubDirs Debug)
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/CmonMeow/Desktop/Shipwright/build/x64/_deps/stormlib-subbuild/stormlib-populate-prefix/src/stormlib-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/Users/CmonMeow/Desktop/Shipwright/build/x64/_deps/stormlib-subbuild/stormlib-populate-prefix/src/stormlib-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()

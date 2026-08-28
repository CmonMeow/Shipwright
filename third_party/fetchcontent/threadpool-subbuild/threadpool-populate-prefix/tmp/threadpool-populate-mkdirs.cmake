# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "C:/Users/CmonMeow/Desktop/Shipwright/third_party/fetchcontent/threadpool-src")
  file(MAKE_DIRECTORY "C:/Users/CmonMeow/Desktop/Shipwright/third_party/fetchcontent/threadpool-src")
endif()
file(MAKE_DIRECTORY
  "C:/Users/CmonMeow/Desktop/Shipwright/third_party/fetchcontent/threadpool-build"
  "C:/Users/CmonMeow/Desktop/Shipwright/third_party/fetchcontent/threadpool-subbuild/threadpool-populate-prefix"
  "C:/Users/CmonMeow/Desktop/Shipwright/third_party/fetchcontent/threadpool-subbuild/threadpool-populate-prefix/tmp"
  "C:/Users/CmonMeow/Desktop/Shipwright/third_party/fetchcontent/threadpool-subbuild/threadpool-populate-prefix/src/threadpool-populate-stamp"
  "C:/Users/CmonMeow/Desktop/Shipwright/third_party/fetchcontent/threadpool-subbuild/threadpool-populate-prefix/src"
  "C:/Users/CmonMeow/Desktop/Shipwright/third_party/fetchcontent/threadpool-subbuild/threadpool-populate-prefix/src/threadpool-populate-stamp"
)

set(configSubDirs Debug)
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/CmonMeow/Desktop/Shipwright/third_party/fetchcontent/threadpool-subbuild/threadpool-populate-prefix/src/threadpool-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/Users/CmonMeow/Desktop/Shipwright/third_party/fetchcontent/threadpool-subbuild/threadpool-populate-prefix/src/threadpool-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()

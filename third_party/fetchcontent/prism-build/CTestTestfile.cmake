# CMake generated Testfile for 
# Source directory: C:/Users/CmonMeow/Desktop/Shipwright/third_party/fetchcontent/prism-src
# Build directory: C:/Users/CmonMeow/Desktop/Shipwright/third_party/fetchcontent/prism-build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
if(CTEST_CONFIGURATION_TYPE MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
  add_test(prism "prism" "../examples/script.opengl.fs")
  set_tests_properties(prism PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/CmonMeow/Desktop/Shipwright/third_party/fetchcontent/prism-src/CMakeLists.txt;135;add_test;C:/Users/CmonMeow/Desktop/Shipwright/third_party/fetchcontent/prism-src/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
  add_test(prism "prism" "../examples/script.opengl.fs")
  set_tests_properties(prism PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/CmonMeow/Desktop/Shipwright/third_party/fetchcontent/prism-src/CMakeLists.txt;135;add_test;C:/Users/CmonMeow/Desktop/Shipwright/third_party/fetchcontent/prism-src/CMakeLists.txt;0;")
else()
  add_test(prism NOT_AVAILABLE)
endif()

# CMake generated Testfile for 
# Source directory: D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/ukey2-src/src/main/cpp/test/securegcm
# Build directory: D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/ukey2-build/src/main/cpp/test/securegcm
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
if(CTEST_CONFIGURATION_TYPE MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
  add_test(ukey2_test "D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/ukey2-build/src/main/cpp/test/securegcm/Debug/ukey2_test.exe")
  set_tests_properties(ukey2_test PROPERTIES  _BACKTRACE_TRIPLES "D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/ukey2-src/src/main/cpp/test/securegcm/CMakeLists.txt;28;add_test;D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/ukey2-src/src/main/cpp/test/securegcm/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
  add_test(ukey2_test "D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/ukey2-build/src/main/cpp/test/securegcm/Release/ukey2_test.exe")
  set_tests_properties(ukey2_test PROPERTIES  _BACKTRACE_TRIPLES "D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/ukey2-src/src/main/cpp/test/securegcm/CMakeLists.txt;28;add_test;D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/ukey2-src/src/main/cpp/test/securegcm/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
  add_test(ukey2_test "D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/ukey2-build/src/main/cpp/test/securegcm/MinSizeRel/ukey2_test.exe")
  set_tests_properties(ukey2_test PROPERTIES  _BACKTRACE_TRIPLES "D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/ukey2-src/src/main/cpp/test/securegcm/CMakeLists.txt;28;add_test;D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/ukey2-src/src/main/cpp/test/securegcm/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
  add_test(ukey2_test "D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/ukey2-build/src/main/cpp/test/securegcm/RelWithDebInfo/ukey2_test.exe")
  set_tests_properties(ukey2_test PROPERTIES  _BACKTRACE_TRIPLES "D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/ukey2-src/src/main/cpp/test/securegcm/CMakeLists.txt;28;add_test;D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/ukey2-src/src/main/cpp/test/securegcm/CMakeLists.txt;0;")
else()
  add_test(ukey2_test NOT_AVAILABLE)
endif()

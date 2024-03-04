# CMake generated Testfile for 
# Source directory: D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/securemessage-src/cpp/test/securemessage
# Build directory: D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/securemessage-build/cpp/test/securemessage
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
if(CTEST_CONFIGURATION_TYPE MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
  add_test(securemessage_test "D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/securemessage-build/cpp/test/securemessage/Debug/securemessage_test.exe")
  set_tests_properties(securemessage_test PROPERTIES  _BACKTRACE_TRIPLES "D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/securemessage-src/cpp/test/securemessage/CMakeLists.txt;30;add_test;D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/securemessage-src/cpp/test/securemessage/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
  add_test(securemessage_test "D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/securemessage-build/cpp/test/securemessage/Release/securemessage_test.exe")
  set_tests_properties(securemessage_test PROPERTIES  _BACKTRACE_TRIPLES "D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/securemessage-src/cpp/test/securemessage/CMakeLists.txt;30;add_test;D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/securemessage-src/cpp/test/securemessage/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
  add_test(securemessage_test "D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/securemessage-build/cpp/test/securemessage/MinSizeRel/securemessage_test.exe")
  set_tests_properties(securemessage_test PROPERTIES  _BACKTRACE_TRIPLES "D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/securemessage-src/cpp/test/securemessage/CMakeLists.txt;30;add_test;D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/securemessage-src/cpp/test/securemessage/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
  add_test(securemessage_test "D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/securemessage-build/cpp/test/securemessage/RelWithDebInfo/securemessage_test.exe")
  set_tests_properties(securemessage_test PROPERTIES  _BACKTRACE_TRIPLES "D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/securemessage-src/cpp/test/securemessage/CMakeLists.txt;30;add_test;D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/securemessage-src/cpp/test/securemessage/CMakeLists.txt;0;")
else()
  add_test(securemessage_test NOT_AVAILABLE)
endif()

cmake_minimum_required(VERSION 3.0.2)
project(square_mission)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(catkin REQUIRED COMPONENTS
  roscpp
  mavros_msgs
  template
)

catkin_package(
  CATKIN_DEPENDS roscpp mavros_msgs template
)

include_directories(${catkin_INCLUDE_DIRS})

add_executable(square src/square.cpp)
target_link_libraries(square ${catkin_LIBRARIES})
add_dependencies(square ${catkin_EXPORTED_TARGETS})

install(TARGETS square
  RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

install(DIRECTORY launch config
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}
)

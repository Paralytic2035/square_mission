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

add_executable(square_mission_node src/square.cpp)
target_link_libraries(square_mission_node ${catkin_LIBRARIES})
add_dependencies(square_mission_node ${catkin_EXPORTED_TARGETS})

install(TARGETS square_mission_node
  RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

install(DIRECTORY launch config
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}
)

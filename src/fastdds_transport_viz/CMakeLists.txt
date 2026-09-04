cmake_minimum_required(VERSION 3.16)
project(fastdds_transport_viz)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(fastrtps REQUIRED)
find_package(fastcdr REQUIRED)
find_package(nlohmann_json REQUIRED)

# --- core library: DDS/ROS independent (model, decision, names) ---------------
add_library(${PROJECT_NAME}_core
  src/model.cpp
  src/decision.cpp
  src/ros_names.cpp)
target_include_directories(${PROJECT_NAME}_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

# --- observers + renderers -----------------------------------------------------
# Generated Fast DDS statistics types (see third_party/fastdds_statistics_types/NOTICE).
set(STATS_TYPES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/fastdds_statistics_types)
add_library(${PROJECT_NAME}_stats_types STATIC
  ${STATS_TYPES_DIR}/types.cxx
  ${STATS_TYPES_DIR}/typesv1.cxx
  ${STATS_TYPES_DIR}/typesPubSubTypes.cxx)
target_include_directories(${PROJECT_NAME}_stats_types PUBLIC ${STATS_TYPES_DIR})
target_link_libraries(${PROJECT_NAME}_stats_types PUBLIC fastrtps fastcdr)
target_compile_options(${PROJECT_NAME}_stats_types PRIVATE -w)
set_target_properties(${PROJECT_NAME}_stats_types PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(${PROJECT_NAME}_lib
  src/discovery_observer.cpp
  src/ros_graph_resolver.cpp
  src/stats_observer.cpp
  src/render_table.cpp
  src/render_json.cpp)
target_link_libraries(${PROJECT_NAME}_lib PRIVATE ${PROJECT_NAME}_stats_types)
target_link_libraries(${PROJECT_NAME}_lib PUBLIC
  ${PROJECT_NAME}_core
  rclcpp::rclcpp
  fastrtps
  fastcdr
  nlohmann_json::nlohmann_json)

add_executable(transport_viz src/main.cpp)
target_link_libraries(transport_viz PRIVATE ${PROJECT_NAME}_lib)

# --- verification nodes ---------------------------------------------------------
foreach(node bounded_pub bounded_sub unbounded_pub large_array_pub)
  add_executable(${node} src/test_nodes/${node}.cpp)
  target_link_libraries(${node} PRIVATE rclcpp::rclcpp ${std_msgs_TARGETS})
endforeach()

install(TARGETS
  transport_viz bounded_pub bounded_sub unbounded_pub large_array_pub
  DESTINATION lib/${PROJECT_NAME})
install(TARGETS ${PROJECT_NAME}_core ${PROJECT_NAME}_lib ${PROJECT_NAME}_stats_types
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY config DESTINATION share/${PROJECT_NAME})

# --- tests -------------------------------------------------------------------------
if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_decision test/test_decision.cpp)
  target_link_libraries(test_decision ${PROJECT_NAME}_core)

  find_package(launch_testing_ament_cmake REQUIRED)
  add_launch_test(test/launch/test_same_host_shm.py TIMEOUT 60)
  add_launch_test(test/launch/test_same_host_udp.py TIMEOUT 60)
  add_launch_test(test/launch/test_stats.py TIMEOUT 90)
endif()

ament_package()

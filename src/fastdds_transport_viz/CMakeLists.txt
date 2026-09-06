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
# Fast DDS 3.x (Kilted / Rolling) exports the `fastdds` package, 2.x (Jazzy) `fastrtps`.
find_package(fastdds QUIET)
if(fastdds_FOUND)
  set(FTV_FASTDDS_TARGET fastdds)
  set(FTV_FASTDDS_MAJOR 3)
else()
  find_package(fastrtps REQUIRED)
  set(FTV_FASTDDS_TARGET fastrtps)
  set(FTV_FASTDDS_MAJOR 2)
endif()
find_package(fastcdr REQUIRED)
message(STATUS "fastdds_transport_viz: building against Fast DDS ${FTV_FASTDDS_MAJOR}.x (${FTV_FASTDDS_TARGET})")
find_package(nlohmann_json REQUIRED)

# --- core library: DDS/ROS independent (model, decision, names) ---------------
add_library(${PROJECT_NAME}_core
  src/model.cpp
  src/decision.cpp
  src/ros_names.cpp
  src/shm_info.cpp)
target_include_directories(${PROJECT_NAME}_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

# --- observers + renderers -----------------------------------------------------
# Generated Fast DDS statistics types (see third_party/fastdds_statistics_types/NOTICE).
if(FTV_FASTDDS_MAJOR EQUAL 3)
  set(STATS_TYPES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/fastdds_statistics_types_v3)
  set(STATS_TYPES_SOURCES
    ${STATS_TYPES_DIR}/typesPubSubTypes.cxx
    ${STATS_TYPES_DIR}/typesTypeObjectSupport.cxx)
else()
  set(STATS_TYPES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/fastdds_statistics_types)
  set(STATS_TYPES_SOURCES
    ${STATS_TYPES_DIR}/types.cxx
    ${STATS_TYPES_DIR}/typesv1.cxx
    ${STATS_TYPES_DIR}/typesPubSubTypes.cxx)
endif()
add_library(${PROJECT_NAME}_stats_types STATIC ${STATS_TYPES_SOURCES})
target_include_directories(${PROJECT_NAME}_stats_types PUBLIC ${STATS_TYPES_DIR})
target_link_libraries(${PROJECT_NAME}_stats_types PUBLIC ${FTV_FASTDDS_TARGET} fastcdr)
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
  ${FTV_FASTDDS_TARGET}
  fastcdr
  nlohmann_json::nlohmann_json)

add_executable(transport_viz src/main.cpp)
target_link_libraries(transport_viz PRIVATE ${PROJECT_NAME}_lib)

# --- verification nodes ---------------------------------------------------------
foreach(node bounded_pub bounded_sub unbounded_pub unbounded_sub large_array_pub large_array_sub)
  add_executable(${node} src/test_nodes/${node}.cpp)
  target_link_libraries(${node} PRIVATE rclcpp::rclcpp ${std_msgs_TARGETS})
endforeach()

install(TARGETS
  transport_viz bounded_pub bounded_sub unbounded_pub unbounded_sub large_array_pub large_array_sub
  DESTINATION lib/${PROJECT_NAME})
install(TARGETS ${PROJECT_NAME}_core ${PROJECT_NAME}_lib ${PROJECT_NAME}_stats_types
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)

# Web viewer (repository-level web/ directory) and its live server.
set(WEB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../web)
if(EXISTS ${WEB_DIR}/index.html)
  install(DIRECTORY ${WEB_DIR}/ DESTINATION share/${PROJECT_NAME}/web
    PATTERN "serve.py" EXCLUDE PATTERN "README.md" EXCLUDE)
  install(PROGRAMS ${WEB_DIR}/serve.py RENAME transport_viz_web DESTINATION lib/${PROJECT_NAME})
else()
  message(WARNING "web/ not found next to the package; transport_viz_web will not be installed")
endif()
install(DIRECTORY config DESTINATION share/${PROJECT_NAME})

# --- tests -------------------------------------------------------------------------
if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_decision test/test_decision.cpp)
  target_link_libraries(test_decision ${PROJECT_NAME}_core)
  ament_add_gtest(test_shm_info test/test_shm_info.cpp)
  target_link_libraries(test_shm_info ${PROJECT_NAME}_core)
  ament_add_gtest(test_render test/test_render.cpp)
  target_link_libraries(test_render ${PROJECT_NAME}_lib)
  ament_add_gtest(test_render_json test/test_render_json.cpp)
  target_link_libraries(test_render_json ${PROJECT_NAME}_lib)

  find_package(launch_testing_ament_cmake REQUIRED)
  add_launch_test(test/launch/test_same_host_shm.py TIMEOUT 60)
  add_launch_test(test/launch/test_same_host_udp.py TIMEOUT 60)
  add_launch_test(test/launch/test_stats.py TIMEOUT 90)
  add_launch_test(test/launch/test_json_schema_live.py TIMEOUT 90)

  find_package(ament_cmake_pytest REQUIRED)
  ament_add_pytest_test(test_json_schema test/test_json_schema.py)
  ament_add_pytest_test(test_web_serve test/test_web_serve.py)
  ament_add_pytest_test(test_cli_args test/test_cli_args.py)
  add_launch_test(test/launch/test_web_live.py TIMEOUT 90)
  add_launch_test(test/launch/test_udpv6.py TIMEOUT 90)
  add_launch_test(test/launch/test_localhost_range.py TIMEOUT 90)
  add_launch_test(test/launch/test_discovery_server.py TIMEOUT 120)
  add_launch_test(test/launch/test_large_data_same_host.py TIMEOUT 90)
  add_launch_test(test/launch/test_large_shm.py TIMEOUT 120)
  add_launch_test(test/launch/test_datasharing_stats.py TIMEOUT 120)
  add_launch_test(test/launch/test_shm.py TIMEOUT 90)
  add_launch_test(test/launch/test_watch.py TIMEOUT 120)
  add_launch_test(test/launch/test_unbounded_vs_bounded.py TIMEOUT 90)
endif()

ament_package()

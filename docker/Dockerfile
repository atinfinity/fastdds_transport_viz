# Development / test image for fastdds_transport_viz.
# Multi-arch: uses whatever ros:<distro> variant matches the host (x86_64 or arm64).
# ROS_DISTRO build argument: jazzy (default, Fast DDS 2.14), humble (Fast DDS 2.6, no
# statistics module), lyrical or rolling (Fast DDS 3.x).
ARG ROS_DISTRO=jazzy
FROM ros:${ROS_DISTRO}
ARG ROS_DISTRO

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        git \
        iproute2 \
        jq \
        nlohmann-json3-dev \
        python3-colcon-common-extensions \
        python3-jsonschema \
        python3-pytest \
        ros-${ROS_DISTRO}-demo-nodes-cpp \
        ros-${ROS_DISTRO}-rmw-fastrtps-dynamic-cpp \
        ros-${ROS_DISTRO}-launch-testing \
        ros-${ROS_DISTRO}-launch-testing-ros \
        ros-${ROS_DISTRO}-ament-cmake-gtest \
        ros-${ROS_DISTRO}-ament-lint-auto \
        ros-${ROS_DISTRO}-ament-lint-common \
    && rm -rf /var/lib/apt/lists/*

ENV RMW_IMPLEMENTATION=rmw_fastrtps_cpp

# Every distribution builds into its own tree in the mounted repository,
# /ws/build/<distro>/{build,install,log}, so that switching ROS_DISTRO never reuses the CMake
# caches or setup files of another one (#123). colcon reads the bases from this defaults
# file, so plain `colcon build`, `colcon test` and `colcon test-result` need no options.
RUN mkdir -p /etc/colcon && printf '%s\n' \
        'build:' \
        "  build-base: /ws/build/${ROS_DISTRO}/build" \
        "  install-base: /ws/build/${ROS_DISTRO}/install" \
        'test:' \
        "  build-base: /ws/build/${ROS_DISTRO}/build" \
        "  install-base: /ws/build/${ROS_DISTRO}/install" \
        'test-result:' \
        "  test-result-base: /ws/build/${ROS_DISTRO}/build" \
        > /etc/colcon/defaults.yaml
ENV COLCON_DEFAULTS_FILE=/etc/colcon/defaults.yaml \
    COLCON_LOG_PATH=/ws/build/${ROS_DISTRO}/log

WORKDIR /ws
COPY docker/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]
CMD ["bash"]

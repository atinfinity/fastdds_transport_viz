# Development / test image for fastdds_transport_viz.
# Multi-arch: uses whatever ros:<distro> variant matches the host (x86_64 or arm64).
# ROS_DISTRO build argument: jazzy (default, Fast DDS 2.14), humble (Fast DDS 2.6, no
# statistics module), kilted or rolling (Fast DDS 3.x).
ARG ROS_DISTRO=jazzy
FROM ros:${ROS_DISTRO}
ARG ROS_DISTRO

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        git \
        jq \
        nlohmann-json3-dev \
        python3-colcon-common-extensions \
        python3-jsonschema \
        python3-pytest \
        ros-${ROS_DISTRO}-demo-nodes-cpp \
        ros-${ROS_DISTRO}-launch-testing \
        ros-${ROS_DISTRO}-launch-testing-ros \
        ros-${ROS_DISTRO}-ament-cmake-gtest \
        ros-${ROS_DISTRO}-ament-lint-auto \
        ros-${ROS_DISTRO}-ament-lint-common \
    && rm -rf /var/lib/apt/lists/*

ENV RMW_IMPLEMENTATION=rmw_fastrtps_cpp
WORKDIR /ws
COPY docker/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]
CMD ["bash"]

# Development / test image for fastdds_transport_viz.
# Multi-arch: uses whatever ros:jazzy variant matches the host (x86_64 or arm64).
FROM ros:jazzy

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
        ros-jazzy-demo-nodes-cpp \
        ros-jazzy-launch-testing \
        ros-jazzy-launch-testing-ros \
        ros-jazzy-ament-cmake-gtest \
        ros-jazzy-ament-lint-auto \
        ros-jazzy-ament-lint-common \
    && rm -rf /var/lib/apt/lists/*

ENV RMW_IMPLEMENTATION=rmw_fastrtps_cpp
WORKDIR /ws
COPY docker/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]
CMD ["bash"]

# Development / test image for fastdds_transport_viz.
# Multi-arch: uses whatever ros:<distro> variant matches the host (x86_64 or arm64).
# ROS_DISTRO build argument: jazzy (default, Fast DDS 2.14), humble (Fast DDS 2.6, no
# statistics module), lyrical or rolling (Fast DDS 3.x).
#
# Stages: `dev` (the default, last stage) is the image every compose service uses; `nav2`
# adds Nav2 and the TurtleBot3 simulation for the scale verification (#74,
# scripts/scale_test.sh nav2) and is only built when asked for (compose service nav2_tb3).
# `release` is the ready-to-run image published on GHCR by .github/workflows/release.yml
# (#78): the packages built into /opt/fastdds_transport_viz on ros:<distro>-ros-core, no
# build tools and no /ws mount (`docker build --target release`).
ARG ROS_DISTRO=jazzy
FROM ros:${ROS_DISTRO} AS base
ARG ROS_DISTRO

ENV DEBIAN_FRONTEND=noninteractive
# Upgrade before installing: the ros:<distro> image lags the apt repo, and freshly published
# packages (demo_nodes_cpp, example_interfaces) next to the stale preinstalled typesupport
# (service_msgs) make the talker die with an undefined symbol (#162, #171).
RUN apt-get update && apt-get upgrade -y --no-install-recommends \
    && apt-get install -y --no-install-recommends \
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

# Nav2 with the TurtleBot3 simulation (Gazebo, run headless), Jazzy only: no Nav2 binaries for
# Lyrical yet, and Kilted, which has them, is out of scope since 1.1.0 (the 1.0.0 tag still
# builds there).
FROM base AS nav2
ARG ROS_DISTRO
RUN apt-get update && apt-get install -y --no-install-recommends \
        ros-${ROS_DISTRO}-nav2-bringup \
        ros-${ROS_DISTRO}-nav2-minimal-tb3-sim \
    && rm -rf /var/lib/apt/lists/*

# Build of the release image: the tagged sources, without tests, into one merged install
# tree. rosdep also records the apt packages of the exec dependencies for the runtime stage;
# --reinstall lists the ones this image already has as well.
FROM base AS release-build
ARG ROS_DISTRO
COPY src /src/src
COPY web /src/web
WORKDIR /src
RUN apt-get update \
    && rosdep update --rosdistro "${ROS_DISTRO}" \
    && rosdep install --from-paths src --ignore-src --rosdistro "${ROS_DISTRO}" -y \
        -t buildtool -t build -t build_export \
    && rosdep install --from-paths src --ignore-src --rosdistro "${ROS_DISTRO}" \
        -t exec -t buildtool_export --simulate --reinstall \
        | sed -n 's/^ *\(sudo -H \)\{0,1\}apt-get install \(-y \)\{0,1\}//p' | sort -u > /exec-apt.txt \
    && test -s /exec-apt.txt \
    && rm -rf /var/lib/apt/lists/*
RUN . "/opt/ros/${ROS_DISTRO}/setup.sh" \
    && colcon --log-base /tmp/colcon-log build --merge-install \
        --build-base /tmp/colcon-build --install-base /opt/fastdds_transport_viz \
        --cmake-args -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release \
        --event-handlers console_cohesion+

# The published image. Upgraded before installing for the same reason as `base` (#171).
# rmw_fastrtps_dynamic_cpp is a test dependency of the package, installed here so that
# RMW_IMPLEMENTATION can match nodes that use it.
FROM ros:${ROS_DISTRO}-ros-core AS release
ARG ROS_DISTRO
ARG VERSION=unknown
LABEL org.opencontainers.image.title="fastdds_transport_viz" \
      org.opencontainers.image.description="Shows which transport (SHM, data-sharing, UDP, TCP) each ROS 2 / Fast DDS publisher-subscriber pair uses: ros2 transport list" \
      org.opencontainers.image.source="https://github.com/atinfinity/fastdds_transport_viz" \
      org.opencontainers.image.documentation="https://atinfinity.github.io/fastdds_transport_viz/getting-started/" \
      org.opencontainers.image.licenses="Apache-2.0" \
      org.opencontainers.image.version="${VERSION}"
ENV DEBIAN_FRONTEND=noninteractive
COPY --from=release-build /exec-apt.txt /tmp/exec-apt.txt
RUN apt-get update && apt-get upgrade -y --no-install-recommends \
    && xargs -a /tmp/exec-apt.txt apt-get install -y --no-install-recommends \
        "ros-${ROS_DISTRO}-rmw-fastrtps-dynamic-cpp" \
    && rm -rf /var/lib/apt/lists/* /tmp/exec-apt.txt
COPY --from=release-build /opt/fastdds_transport_viz /opt/fastdds_transport_viz
ENV RMW_IMPLEMENTATION=rmw_fastrtps_cpp
COPY docker/release_entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]
CMD ["ros2", "transport", "list"]

FROM base AS dev

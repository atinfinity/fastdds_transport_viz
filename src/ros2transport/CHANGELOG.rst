^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Changelog for package ros2transport
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

2.0.1 (2026-09-22)
-------------------
* ``ros2 transport list --help`` gives the right ``--timeout`` default with ``--stats``
  (30 s, not 5 s) and describes the ``--stats`` settle rule under ``--quiet`` instead of
  calling it ignored, as the binary's own ``--help`` does (#224).

2.0.0 (2026-09-22)
-------------------
Breaking change: ``ros2 transport list`` exits 1 on an RMW other than ``rmw_fastrtps_cpp`` /
``rmw_fastrtps_dynamic_cpp`` instead of running (#72). When ``RMW_IMPLEMENTATION`` names
another middleware it prints the binary's message under a ``ros2 transport:`` prefix before
spawning the binary; otherwise the binary checks the RMW itself.

* A REP 2004 quality declaration, ``QUALITY_DECLARATION.md``: Quality Level 3, like
  ``fastdds_transport_viz`` (#87).
* ``ros2 transport list --csv`` passes ``--csv`` to the binary: one CSV row per pair
  (#83). ``diff`` does not take it.
* ``ros2 transport diff BEFORE AFTER``: runs ``transport_viz diff`` with the two documents
  and the ``--key``, ``--changes-only`` and view/rendering options; a missing input file is
  reported before the binary is started (#77).
* ``rmw_fastrtps_dynamic_cpp`` is supported like ``rmw_fastrtps_cpp`` (#73); the live
  test follows ``RMW_IMPLEMENTATION`` instead of pinning ``rmw_fastrtps_cpp``.
* Pass ``--advise`` through to ``transport_viz``; ``ros2 transport codes`` prints the
  remedy of each code after its description.

1.1.0 (2026-09-12)
-------------------
* Pass ``--locators`` through to ``transport_viz``.
* Supported distributions: ROS 2 Lyrical Luth replaces Kilted (EOL December 2026).

1.0.0 (2026-09-06)
-------------------
* Initial release: ``ros2 transport list``/``ros2 transport codes`` ``ros2cli``
  extension, a thin wrapper around the ``transport_viz`` binary of
  ``fastdds_transport_viz``.

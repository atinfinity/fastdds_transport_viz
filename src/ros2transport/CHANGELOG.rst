^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Changelog for package ros2transport
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Forthcoming
-----------
* ``ros2 transport list`` exits 1 with the same message as the binary when
  ``RMW_IMPLEMENTATION`` names another middleware, before spawning it.
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

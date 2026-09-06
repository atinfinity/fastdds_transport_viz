^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Changelog for package fastdds_transport_viz
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

1.0.0 (2026-09-06)
-------------------
* Initial release: predicts which Fast DDS transport (``UDPv4``, ``UDPv6``, ``TCPv4``/
  ``TCPv6``, ``SHM``, zero-copy ``DATA_SHARING``) each ROS 2 topic is communicated over
  and why, from Fast DDS discovery data alone; every verdict carries machine-readable
  reason codes (``--explain``, ``ros2 transport codes``).
* QoS request/offer compatibility check: pairs whose reliability, durability, deadline,
  liveliness, ownership or partition do not match are shown as ``NONE`` with the policy
  that breaks them.
* Measurement with ``--stats`` via the Fast DDS statistics module: the transport that
  actually carried packets, payload rate (``RATE``), write-to-notification latency
  (``LATENCY``), lost/resent packets (``LOSS``), host names and process ids, and proof of
  zero-copy data-sharing delivery through ``HISTORY_LATENCY``/``DATA_COUNT``; a
  measurement that contradicts the prediction is flagged.
* Several front-ends: a colored table, ``--watch`` (live terminal view marking
  added/changed/removed pairs), ``--json`` with a published JSON Schema, and a web viewer
  (graph/table view, live updates through ``transport_viz_web``).
* ``--topic``/``--node`` regex filters, ``--all`` for services/actions and non-ROS DDS
  topics.
* Shared-memory report: ``/dev/shm`` capacity, Fast DDS segments/ports/data-sharing
  histories in it, stale leftovers, and whether the observed nodes actually share it.
* Supports ROS 2 Humble (Fast DDS 2.6, prediction only — no statistics module in the
  binary), Jazzy (Fast DDS 2.14) and Kilted (Fast DDS 3.2), on x86_64 and arm64.

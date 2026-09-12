^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Changelog for package fastdds_transport_viz
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

1.1.0 (2026-09-12)
-------------------
* ``--locators``: a line under each pair of the verbose table with the locator the tool
  selected from the reader's announced locators and, with ``--stats``, the locators that
  actually carried packets. Implies ``-v``; ignored with ``--json``. Toggled with ``l``
  in ``--watch``.
  For an SHM verdict the selected locator is the reader's ``/dev/shm`` port, so it lines
  up with the ``fastrtps_port<N>`` files in the shared-memory line.
* JSON gains ``pairs[].locator`` (nullable, with a ``multicast`` flag) and
  ``pairs[].measured.locators[]`` (per-locator ``packets``/``bytes`` as deltas over the
  observation, so they are a breakdown of ``measured.packets``/``bytes``). Both are
  always emitted, not gated by ``--locators``; ``schema_version`` stays 1.
* New warning ``measured-locator-mismatch``: the transport kind agrees but the locator
  the prediction selected carried no packets at all, so the traffic took another locator
  the reader also announced - typically a different interface of a multi-homed host.
* ``--watch`` marks a pair whose selected or measured locator changes.
* Web viewer: the selected locator is marked in the reader's locator list, and the
  ``Measured`` column carries the addresses next to their transports.
* Supported distributions: ROS 2 Lyrical Luth (Fast DDS 3.6) replaces Kilted, which
  reaches EOL in December 2026. Humble, Jazzy and Rolling are unchanged.

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

^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Changelog for package fastdds_transport_viz
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Forthcoming
-----------
* Node names across IPC namespaces: the tool reads ``ros_discovery_info`` itself, with a
  reader on its raw participant that announces only the non-SHM unicast locators the
  participant listens on (like the statistics readers), and names the endpoints the ROS
  graph API cannot name from it. rclcpp's participant announces SHM, so a same-host node in
  another IPC namespace than the tool wrote those samples into its own ``/dev/shm`` and the
  tool showed ``_NODE_NAMESPACE_UNKNOWN_/_NODE_NAME_UNKNOWN_``. A name that is still unknown
  (a node or tool with SHM only, or a node of another ROS distribution) is now empty, like a
  raw DDS endpoint's, instead of merging every such endpoint into one node: the table labels
  it by GUID and the web viewer shows its participant. ``diff`` and the web viewer read the
  unknown name of older JSON documents as empty, so ``diff --key node`` matches such pairs by
  GUID where it matched them by that name before. New dependencies ``rmw_dds_common`` and
  ``rosidl_typesupport_fastrtps_cpp``. Integration scenarios ``hostnet_noipc_shm`` and
  ``hostnet_split_*`` check the node names (#112).
* ``--stats`` on a split IPC pair: the new warning
  ``shm-ipc-namespace-split-but-non-shm-traffic`` flags non-SHM packets (UDP, TCP) measured
  during the observation between endpoints that both announce SHM and were judged to be in
  different IPC namespaces. Fast DDS sends same-host traffic between them over SHM only, so
  the split detection may be wrong; the verdict stays ``NONE`` / ``certain`` and the
  description asks for a report. Data-sharing splits without SHM on one side are not flagged
  (the other endpoints of the two participants use UDP), nor are kinds seen only before the
  observation (#111).
* Same host id, separate IPC namespaces, data-sharing: a data-sharing pair whose endpoints
  use different ``/dev/shm`` is ``NONE`` / ``certain`` with ``shm-ipc-namespace-split``
  instead of ``DATA_SHARING`` (Fast DDS pairs them on QoS alone, the reader cannot open the
  writer's history and no sample arrives). The evidence is the SHM port evidence of #101
  when both announce SHM, or the new ``datasharing-reader-segment-not-visible`` /
  ``datasharing-writer-segment-not-visible`` (the writer's history is in the tool's
  ``/dev/shm`` and the reader's notification segment is not, or the reverse), which works
  without the SHM transport. The warning's description covers data-sharing segments and its
  remedy adds ``data_sharing`` OFF. Such a reader no longer makes the writer's other
  data-sharing readers ``datasharing-ambiguous-mixed-readers`` under ``--stats``. The
  ``shm`` object counts the readers' notification segments in the new
  ``datasharing_notifications`` instead of ``datasharing_unmatched``. Integration scenarios
  ``hostnet_split_datasharing`` and ``hostnet_split_datasharing_udp`` (#110).
* ``--stats`` across IPC namespaces: the statistics readers announce only the non-SHM
  unicast locators the tool's participant listens on (UDP, or TCP with ``LARGE_DATA``),
  read once from a probe reader. A same-host writer in another IPC namespace than the tool
  selected SHM for its statistics, wrote them into its own ``/dev/shm`` and showed
  ``stats-not-enabled-on-writer``; they now arrive over the network stack, and a split
  pair shows its undelivered SHM traffic. The ``stats-not-enabled-on-writer`` description
  adds a writer with no transport in common with the statistics readers (SHM only).
  Integration scenario ``hostnet_split_stats`` (#106).
* Same host id, separate IPC namespaces: a pair whose participants listen for SHM in
  different ``/dev/shm`` is ``NONE`` / ``certain`` with the warning
  ``shm-ipc-namespace-split`` instead of ``SHM`` (Fast DDS selects SHM there and every
  sample is lost). The evidence is ``shm-port-collision`` (both announce the same SHM port
  number, which one participant per IPC namespace can listen on) or
  ``shm-reader-port-not-visible`` / ``shm-writer-port-not-visible`` (from the tool's IPC
  namespace every port of one side is held and a port of the other is not). ``--stats``
  keeps such a pair ``NONE``, since the writer's SHM traffic is expected, and adds
  ``shm-ipc-namespace-split-but-delivered`` when a delivery is proven. New sample
  ``web/sample/shm_split.json``; integration scenarios ``hostnet_split_shm`` and
  ``hostnet_split_shm_visible``. The descriptions of ``shm-not-visible`` and
  ``host-id-match-but-ip-differs`` point to the new warning (#101).
* Shared-memory line: the tool's own participants are found with
  ``DomainParticipantFactory::lookup_participants()`` instead of through endpoints that
  resolve to its node name (on Lyrical and Rolling rclcpp creates none, so the tool's rmw
  participant counted as a node), and its own SHM ports include every port lock the
  process holds (the discovery participant announces none). Either gap let a node in
  another IPC namespace with a colliding port number look visible. The ``shm-not-visible``
  description adds that nodes with the same host id in different IPC namespaces still
  pick SHM between themselves and lose every message (#51).
* Web viewer: compare two documents (``Compare with…``, ``?src=a&diff=b[&key=guid]``) and
  highlight the ``changes`` object of a ``transport_viz diff --json`` file or a live frame:
  ``+`` / ``~`` / ``-`` marks in the table with ``before → after`` transports and ghost
  rows, halos and dotted ghost edges in the graph, a ``changes only`` switch and the
  ``changes:`` summary; live marks stay three frames like ``--watch``. The comparison is
  a JavaScript port of ``diff_snapshots()`` tested against the binary's output on the
  shared fixtures (``web/sample/diff.json``). Hidden elements no longer show through a
  ``display`` rule (the live indicator was visible on a static page) (#77).
* ``transport_viz diff <before.json> <after.json>``: compare two saved ``--json``
  documents without observing anything (change a profile or an environment variable, run
  again, see what moved). The after snapshot is printed with the ``+`` / ``~`` / ``-``
  marks, ghost rows and ``changes:`` line of ``--watch``, or with ``--json`` as the after
  document plus the ``changes`` object. Pairs are matched by ``(topic, writer node, reader
  node)`` by default (``--key node``), so restarting the nodes between the two runs is not
  a change (the node key also ignores the locator port numbers a restart renumbers);
  ``--key guid`` matches by GUIDs as ``--watch`` does. ``--changes-only`` keeps
  the topics that moved; ``--topic``, ``--node``, ``--all`` and the rendering options apply
  to both documents. One document may be ``-`` (stdin) and a ``--watch --json`` log counts
  by its last document. Exit status 0 without changes, 1 with, 2 on an error (#77).
* The ``changes`` object (``--watch --json`` and ``diff --json``) gains ``key``
  (``guid`` / ``node``), ``writer_node`` / ``reader_node`` in every pair key, the before
  GUIDs in ``changed_pairs[].from`` and, for ``diff``, ``before`` (``observed_at`` and
  ``domain`` of the before document); ``schema_version`` stays 1. New library functions
  ``parse_json()`` (the inverse of ``render_json()``) and ``diff_snapshots()``.
* The tool refuses to start on an RMW other than ``rmw_fastrtps_cpp`` (exit 1, the
  message names the RMW and the fix), asking the RMW layer itself before any participant
  is created, so an unset ``RMW_IMPLEMENTATION`` resolves to the distro's default.
* ``rmw_fastrtps_dynamic_cpp`` is supported: the full test suite and the multi-container
  scenarios pass on it (Jazzy and Lyrical locally; CI runs the suite on it for Humble,
  Jazzy and Lyrical on x86_64), so it is accepted silently instead of with a warning
  (#73). ``compose.yaml`` honours ``RMW_IMPLEMENTATION`` from the host shell.
* ``--advise``: what to change to get past the reason codes in use. Every code now has a
  remedy (one sentence naming the environment variable, XML element or QoS policy) or an
  explicit none. The flag adds a ``fix <code>: ...`` line under each pair for its codes
  that have one and the remedy under each code of the legend (implies ``-v`` and
  ``--explain``; toggled with ``f`` in ``--watch``). ``--list-codes`` prints the remedy after
  each description, ``--json`` carries ``reason_code_remedies`` (same keys as
  ``reason_code_descriptions``, ``null`` for none; always emitted, ``schema_version``
  stays 1) and the web viewer shows them under the descriptions. The remedies that a few
  descriptions used to contain (``shm-stale-files``, ``shm-nearly-full``,
  ``stats-not-enabled-on-writer``, ``stats-writer-instance-limit-suspected``,
  ``datasharing-unverified-by-traffic``, ``no-traffic-observed``,
  ``latency-clock-skew-suspected``) moved into the remedy, so each is said once.

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

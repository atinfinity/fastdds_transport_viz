^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Changelog for package fastdds_transport_viz
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Forthcoming
-----------
* The web viewer's live mode keeps the frames it receives (#218). From the second frame on,
  the replay timeline follows the newest frame and the charts of a selected pair cover the
  kept frames. A move on the timeline stops on that frame while the frames keep coming
  (``live: viewing #k of N``), ``live ▶|`` / End / **Resume** follow the newest frame again,
  and **Save recording** downloads the kept frames as JSON Lines. The history lives in the
  page, bounded by ``?history=<MB>`` (default 512, ``0`` keeps none); over the bound the
  oldest tenth is dropped at once. ``transport_viz_web`` numbers its ``document`` events
  with SSE ``id:``, so the viewer counts the frames a slow page skipped, does not add the
  document sent again on a reconnect, and tells a restarted server. The series of a pair
  that leaves a recording for good now reads absent to the end (it read undefined past its
  first allocation, which the charts drew as values).
* The web viewer replays recordings (#82). ``transport_viz_web --record FILE`` writes every
  document of the live stream to ``FILE`` as it arrives - the same JSON Lines that
  ``transport_viz --watch --json > FILE`` prints - and the viewer opens such a file (Open,
  drag & drop, ``?src=``) with a timeline: a slider, previous / next frame (also the arrow
  keys), previous / next frame with changes, a tick per such frame, ``&frame=N`` in the
  address, and the ``diff`` key that tells how a pair is followed across frames. Each frame
  is marked with its own ``changes``; the selection follows its pair or its nodes, and the
  card of a selected pair charts its transport, ``delivered/s``, per-interval latency and
  lost packets over the whole recording, with a click to jump. The file is read in chunks
  and never held whole, so a recording of hundreds of megabytes opens in a couple of
  seconds. A file with one document is still a document, **Compare with…** leaves the
  replay, and a recording stands for its last document wherever one document is expected.
  Documents are unchanged (``schema_version`` 1). ``scripts/integration_test.sh
  record_flip`` records a transport flip on every image; ``web/sample/recording.jsonl`` is
  its Jazzy capture.
* Type matching now mirrors Fast DDS 3.x (#213). Where both endpoints announce an XTypes
  ``TypeInformation`` - every ROS 2 endpoint on Lyrical and later - Fast DDS matches them
  when the complete *or* the minimal type identifiers agree, and ignores the type names.
  The tool now does the same. A pair whose identifiers both differ is ``NONE`` with the
  reason ``type-information-mismatch`` (it was a warning on a pair that kept its
  transport, and was checked only when a side lacked a ROS 2 type hash), which on Lyrical
  also covers two versions of one ROS 2 message: those were shown with a transport and
  ``type-hash-mismatch``, although Fast DDS never matches them. Two names for one type
  are matched, with the reason ``type-names-differ-same-type``, instead of ``NONE`` with
  ``type-name-mismatch``. A peer created with ``fastdds.type_propagation=minimal_bandwidth``
  announces only the minimal identifier, which ``--json`` now records as
  ``type_information_minimal_hash`` on each endpoint (``schema_version`` unchanged, empty
  when read from an older document). ``--stats`` warns
  ``type-information-mismatch-but-delivered`` if such a pair is proven delivered anyway.
  Fast DDS 2.x announces no ``TypeInformation``, so Humble and Jazzy keep the name rule.
* Services and actions are one row each under ``--all``, instead of their raw ``rq/`` /
  ``rr/`` topics (#84). A service is two DDS topics that both demangle to the same ROS
  name, so ``--all`` printed two rows with identical text; an action is eight of them, the
  five ``rcl_action`` members under ``<action>/_action/``. They now collect into one
  ``SERVICE`` / ``ACTION`` row per client-server pair, named after the service or action
  and after both sides (participant prefix where the node name is unknown, as service
  endpoints carry none). The two sides are *directions of travel* rather than request and
  reply, which is what lets an action fit on one line: ``PUBS`` and ``SUBS`` count the
  member pairs each way, so a complete service reads ``1``/``1`` and a complete action
  ``3``/``5``, and ``feedback`` and ``status`` fold in with the replies. Endpoints in no
  pair keep a half-open row (``- -> /talker``), so an uncalled parameter service stays
  visible - on a single node that turns 14 rows into 7. ``ACTION`` is claimed only when the
  names *and* the types agree, because ``/_action/`` is not reserved and a plain service
  may wear an action's exact DDS names (``ros2 action list`` is itself fooled by a
  ``feedback`` / ``status`` pair); a group that fails the check falls back to ``SERVICE``
  or to plain topics. ``<node>/_service_event`` stays a plain topic, being visible in the
  default view. ``--json`` gains ``kind`` / ``group`` / ``direction`` on each entry of
  ``topics[]`` (``schema_version`` unchanged, the raw topics still in the document, and a
  document written before them is classified again when read back), and the web viewer's
  Table tab groups the same way with a ``SERVICE`` / ``ACTION`` badge; the graph is
  unchanged. ``web/sample/services.json`` is a capture to try it on.
* Measured why the Humble type-definition gap cannot be closed, and said so in the
  Limitations (#193). Fast DDS carries an older description of a type than the ROS 2 type
  hash (``TypeIdV1``, ``TypeObjectV1``, ``TypeInformation``), which looked like a way to
  tell two definitions of one message apart on Humble, where the rmw announces no
  REP-2011 hash. It is not: no Humble endpoint announces any of the three (0 of 36 across
  17 type names, under ``rmw_fastrtps_cpp`` and ``rmw_fastrtps_dynamic_cpp`` alike), and
  neither does Jazzy (0 of 40) - ``rmw_fastrtps``'s ``TypeSupport`` is a plain
  ``TopicDataType``, so nothing is registered in the ``TypeObjectFactory`` that Fast DDS
  2.x fills those parameters from, and they never reach the wire. The gap is
  publisher-side and no setting of the tool's own participant can recover a value the
  sender never wrote, so no detection is shipped and no field is added. The four
  Limitations sections now state this as a measured fact with the remedy (rebuild every
  node against the same message package; observe the same graph from a machine with Jazzy
  or later, where the tool does report the mismatch), and ``docs/development.md`` records
  the numbers and the Fast DDS source chain. Fast DDS 3.x does fill the XTypes 1.3
  ``type_information`` on every endpoint, which is tracked separately for the non-ROS-peer
  case.
* Pairs delivered inside one process are named, and no longer keep a ``--stats`` one-shot
  waiting (#201). Fast DDS hands a sample from a writer to a reader of the *same process*
  inside the participant (``intraprocess_delivery``, ``FULL`` by default) and puts nothing
  on a transport - it beats data-sharing too - so no ``RTPS_SENT`` or ``DATA_COUNT``
  counter can ever move for such a pair, while ``HISTORY_LATENCY`` proves the delivery.
  The tool recognises it from the GUID prefixes (equal first 8 bytes of an eProsima
  prefix, what ``RTPSDomainImpl::should_intraprocess_between()`` compares), gives the pair
  the reason ``intra-process`` on every run and every distribution, and shows
  ``(intra-process)`` in the ``MEASURED`` column instead of ``(unmeasured, delivered)``.
  A same-process data-sharing pair gets ``intra-process`` in place of
  ``datasharing-unverified-by-traffic`` and no ``certain`` upgrade from a silent
  ``DATA_COUNT``. Such pairs leave ``stats.pairs_delivered``, as data-sharing pairs
  already did, so they no longer feed ``pairs_delivered_unmeasured`` /
  ``pairs_delivered_absent``, ``rtps-sent-absent`` or the ``stats_watch_coverage``
  denominator. The settle rule gains "nothing is measurable": ``stats.measurable_pairs``
  counts the pairs whose two ends are in different processes (over-approximating QoS
  compatibility, the tool's own participants excluded), and a run with 0 of them settles
  at the 5 s minimum window rather than burning ``--timeout`` and then warning that no
  ``RTPS_SENT`` entry towards a reader was measured. A publisher-only system settles the
  same way. ``stats.measurable_pairs`` is additive in ``--json``; ``schema_version``
  stays 1. The ``rtps-sent-absent`` remedy no longer blames Fast DDS 3.6 alone: the
  statistics writers are in pull mode on every version, so ``config/statistics.xml`` is
  the answer everywhere.
* A multicast-only pair now lets a ``--stats`` one-shot settle (#196): the settle rule
  counted ``RTPS_SENT`` instances towards a discovered reader's *unicast* port only, while
  the measurement attributes packets to any locator the reader receives on, so a run whose
  readers announce a multicast group and no unicast locator settled at ``--timeout``
  instead and printed "no measured RTPS_SENT entry to a discovered reader" although every
  packet had been attributed. The rule now counts an instance whose destination is a
  reader's unicast ``(kind, port)`` or a multicast group the reader announces, matched on
  the whole locator; the ``#179`` exclusion of metatraffic and the tool's own ports is
  unchanged, and needs no exception for ``239.255.0.1:7400``, which is a participant
  locator that no endpoint announces. No output or schema change.
* ``transport_viz_web`` asks the browser to reconnect a second after a lost connection
  (#191): the ``/events`` stream now opens with ``retry: 1000`` instead of leaving the
  browser's own default (3 s in Chrome), so the viewer's "live: connection lost,
  reconnecting…" banner clears sooner. The server already sends the latest document to
  every new connection, so a reconnect heals the view without waiting for the next
  ``--interval``; the browser test covers the banner and the recovery.
* Type mismatches on the same topic (#85): a writer and a reader whose DDS type names
  differ are a ``NONE`` / ``certain`` pair with the reason ``type-name-mismatch``
  (it used to be a topic-level ``unmatched_reasons`` entry, so the two endpoints were not
  shown as a pair at all), and no traffic is attributed to it -
  ``type-name-mismatch-but-delivered`` when the statistics say otherwise. The same type
  name with two different ROS 2 type hashes (REP-2011) keeps its transport and gains the
  warning ``type-hash-mismatch``: on Fast DDS 2.x the pair matches and delivers while the
  subscription gets nothing, on 3.x it never matches. The hash is read out of the
  endpoint's ``USER_DATA`` and published as ``type_hash`` on every endpoint of
  ``--json`` (``""`` when it announces none, as on Humble); a ``type hash`` row in the web
  viewer's endpoint panel. Additive, ``schema_version`` stays 1.
* Per-endpoint data-sharing segment visibility in ``--json`` (#163):
  ``datasharing_segment_visibility`` (``visible``, ``not-visible``, ``unprobed``) on every
  writer and reader, the evidence behind ``datasharing-*-segment-not-visible``; probed only
  for an endpoint on the tool's host whose QoS announces data-sharing. Round-tripped by
  ``diff``; a ``data-sharing`` row in the web viewer's endpoint panel. Additive.
* Per-participant SHM ports and visibility in ``--json`` (#125): a ``participants``
  array with one entry per discovered participant (``guid_prefix``, ``host_id``, ``host``,
  ``host_name``, ``own``, ``shm_visibility``) and its announced SHM ports on the tool's
  host with the lock state probed from the tool's IPC namespace (``held``, ``own``,
  ``absent``, ``stale``, ``unknown``, ``unprobed``), ``announced_by`` and ``proof``;
  ``shm.unknown_ports`` next to ``missing_ports``. The web viewer shows it as an ``shm``
  row of the endpoint panel. Additive, ``schema_version`` stays 1.
* A delivered rate per pair (#143): the ``HZ`` column of the table (right of ``LATENCY``),
  ``measured.delivered_per_s`` in the document and ``Hz`` in the web viewer count the
  ``HISTORY_LATENCY`` samples the reader-side participant reports, one per delivered sample
  on every path (UDP, SHM, same-process, data-sharing), as (samples - 1) / (last - first
  source timestamp): the whole observation in a one-shot run, the last 5 s in ``--watch``
  (``delivered_per_s_window_s``). ``null`` and a blank cell under two samples. When the
  tool's reader misses samples of the reporting participant (sequence gaps; the ceiling is
  the depth of 100 every 50 ms, 2000 samples/s), every rate read from it is a lower bound:
  ``≥`` in the table, ``delivered_per_s_lower_bound`` in the document. The
  ``HISTORY_LATENCY`` reader depth goes from 10 to 100; the medium scale rung stays within
  its budgets. ``throughput_bytes_per_s`` stays ``null``, the diff ignores the rate, the
  properties are optional and the schema version is unchanged. ``rate_load`` and the
  ``rate_stats`` integration scenario verify the rate at 10, 100 and 1000 Hz on SHM,
  same-process and data-sharing pairs.
* An unmeasured pair is no longer blamed on the tool when no lost sample explains it (#152).
  ``stats.pairs_delivered_absent`` counts the pairs with a delivery proof and no measured
  packet that the tool never saw an ``RTPS_SENT`` instance for, and every such pair of a run
  that lost no counter sample; they raise the new document-level warning
  ``rtps-sent-absent`` (one stderr line, the ``statistics:`` footer, the web viewer), whose
  remedy is the shipped statistics profile: on Fast DDS 3.6 the statistics writers deliver
  almost only on their 3 s periodic heartbeat. ``stats-samples-lost`` now counts only the
  pairs a loss can explain, and both warnings can appear in one run. The property is
  optional, so documents written earlier still validate and the schema version is unchanged.
* A ``--watch`` frame on a large graph is two to five times faster (#135): 152 ms instead of
  344 ms at 20 processes and 2400 pairs with ``--stats``, 1.6 s instead of 8.9 s at 40
  processes. The ROS graph is queried only on the frames that follow a discovery event or a
  ``ros_discovery_info`` sample, or come within 5 s of the last event, instead of twice per
  topic on every frame; and the previous frame is kept by move for its ghost rows instead of
  being copied, summarized and given its statistics a second time. ``scripts/scale_test.sh``
  also times a ``--watch`` run without ``--stats``.
* The tool keeps up with the statistics its readers are sent, on systems where it used to
  miss most of them (#141). Two things changed. ``StatsObserver`` now owns a thread that
  takes from every reader every 50 ms, for as long as the observer lives -- in both modes
  and while ``--watch`` is paused; until now the only drain during ``--watch`` was the one
  ``snapshot()`` does inside ``collect()``, once per ``--interval`` (two seconds by default)
  and not at all while paused, and the readers keep only the newest sample of an instance,
  so what bounds the loss is the time between two takes. And the readers no longer take the
  Fast DDS default resource limits: the statistics topics are keyed, the default allows ten
  instances per reader, and past that a reader stops receiving a participant's samples
  altogether -- the same limit the tool already warns about on the writer side. Instances
  and ``max_samples`` are now unlimited and only ``max_samples_per_instance`` is bounded,
  because a total cap would refuse samples once enough instances exist and a refusal is
  counted as ``samples_rejected``. At 20 processes and 2400 pairs this takes the statistics
  coverage from 0.947 to 1.0 and the one-shot table from one measured pair to all 2400.
  ``HISTORY_LATENCY`` also goes best-effort and volatile: it is by far the loudest topic,
  nothing it carries is cumulative, it is the only one that never reads ``first``, and
  receiving it reliably costs more than it is worth -- measured at the same size, its
  acknacks and retransmissions dropped the coverage to 0.746 and took the ``--watch`` frame
  p95 to 20 s. Its losses are now visible, counted apart in
  ``stats.samples_lost_latency`` (below), which is a report rather than a regression: the
  samples were being discarded before too, silently, by a reliable reader overwriting its
  own unread history.
* ``stats.writers_incompatible_qos`` counts the statistics DataWriters the readers could not
  match because the writers' QoS is incompatible with theirs (#141). Nothing such a writer
  publishes is ever received and nothing is counted as lost either -- a reader is only told
  about the samples of writers it did match -- so that loss was invisible, which is the one
  thing #134 set out to end. Writers, not samples: folding them into ``samples_rejected``
  would take back the meaning #134 gave it. The property is optional, so documents written
  earlier still validate and the schema version is unchanged, and it raises no warning code:
  with ``FASTDDS_STATISTICS`` set as the docs recommend the number is zero, and it is the
  profile of the observed nodes that would have to change for it not to be.
* Losing a ``HISTORY_LATENCY`` sample and losing a counter sample are no longer one number
  (#141). The latency reader is best-effort by design -- that is what lets the tool keep up
  -- so it reports every sequence gap in the loudest topic there is, and ``samples_lost``
  therefore grew by orders of magnitude in the very change that took the statistics coverage
  at 20 processes from 0.74 to 1.0: the tool measures every pair and says louder than ever
  that it cannot keep up. ``stats.samples_lost_latency`` now counts that part apart -- as a
  part of ``samples_lost``, so nothing about the existing field changes -- and the
  ``stats-samples-lost`` warning, the one stderr line, the table's ``statistics:`` footer,
  the web viewer's meta bar and the scale harness's ``stats_dropped_samples`` budget all
  judge ``samples_lost`` without it and name the latency losses separately. The two are not
  the same failure: a counter is read as ``last - first`` over the observation window, so
  losing a counter sample shortens the window that difference covers and, when it leaves an
  instance with fewer than two samples in it, costs the entity its measurement, while a
  latency sample is one observation reduced to a mean and a max, so losing one only
  coarsens a number the pair still shows. The property is optional, so documents written
  earlier still validate and the schema version is unchanged.
* The ``RATE`` column is gone, and the tool no longer subscribes to
  ``PUBLICATION_THROUGHPUT``. That statistic is not a rate: Fast DDS publishes one sample
  per ``write()`` whose value is that sample's payload divided by the interval since the
  same writer's previous ``write()``, so a writer that sends one burst and then falls
  silent was shown orders of magnitude too fast (``/parameter_events`` reached 1.7 GB/s at
  scale) and an idle writer kept its last value. Averaging cannot repair it either: the
  statistics readers keep one sample per instance of a counter topic and are drained
  every 50 ms, so everything but the newest value is gone.
  ``PUBLICATION_THROUGHPUT_TOPIC`` is out of the documented ``FASTDDS_STATISTICS`` value and
  out of the shipped XML profiles. The JSON keys stay so
  that documents written earlier keep validating, fixed to ``null``
  (``measured.throughput_bytes_per_s``, ``topics[].throughput_bytes_per_s``) and to ``{}``
  (``stats.throughput``); the schema version is unchanged. The cumulative counters an
  honest rate can be computed from are already in the document -- ``stats.data_count`` and
  ``stats.traffic`` next to ``observation_seconds`` -- and ``docs/statistics.md`` says how;
  bringing a real rate column back is #143 (#137).
* ``RTPS_LOST`` was matched in the reverse direction: the ``lost`` of a pair was what the
  writer's participant missed from the reader's, and real writer → reader loss never
  showed. Fast DDS publishes ``RTPS_LOST`` from the receiving participant with the sender's
  GUID and the receiver's own locator the sender addressed; a pair now counts what the
  reader's participant reports from the writer's participant on the reader's unicast
  locators (multicast destinations are left out). The loss belongs to the participant pair:
  every pair between the same two participants shows it, and the topic's ``lost_packets``
  counts each report once. When the reader's participant publishes no ``RTPS_LOST`` the
  loss is unknown: ``lost_packets`` is ``null`` in the JSON (pair and topic) and the
  ``LOSS`` column shows ``- lost``, while the other reliability counters stay. The
  ``stats.lost[]`` keys are renamed to ``src_participant_guid_prefix`` / ``dst_locator``,
  with the new ``reporter_participant_guid_prefix`` (schema version unchanged; documents
  with the old ``receiver_participant_guid_prefix`` / ``from_locator`` still load). A
  ``diff`` of a capture taken before this change against one taken after it can show
  ``rtps-packets-lost`` moving between pairs. New integration scenario
  ``stats_loss_multi_container`` (#122). The loss on a multicast destination stays out of
  the count, and #130 measured what it would have been: Fast DDS spends one statistics
  sequence number per socket a multicast send goes out on, so a sender with N interfaces
  reads as exactly N lost packets per message at a receiver in another network namespace,
  while nothing is lost on the wire (``scripts/multicast_stamping_test.sh``, #130).
* ``stats-writer-instance-limit-suspected``: the remedy named
  ``FASTDDS_DEFAULT_PROFILES_FILE``, which Fast DDS 2.x does not read, while 2.x (Jazzy's
  2.14) is where the statistics DataWriters keep the 10-instance limit. It now names
  ``FASTRTPS_DEFAULT_PROFILES_FILE`` for Fast DDS 2.x and ``FASTDDS_DEFAULT_PROFILES_FILE``
  for 3.x. Fast DDS 3.5 made the limit unlimited by default, so a tool built with 3.5 or
  later (Lyrical, Rolling) no longer reports the warning, which could only be a false alarm
  there; such pairs get ``delivered-without-measured-traffic`` or ``no-traffic-observed``
  (#127).
* Native-buffer companions (``rmw_fastrtps_cpp`` on Lyrical and later): a writer or reader
  of a type with an unbounded ``uint8[]`` field gets a companion on ``<topic>/_buf_cpu`` in
  the same participant, and when every subscription supports native buffers the samples go
  through the companions only. The parent pair showed 0 DATA submessages, no heartbeats and
  no delivery while the data flowed. The tool links each companion to its parent (same
  participant, kind and type, several candidates told apart by the entity key) and adds the
  companion's per-entity counters (delivered samples, DATA_COUNT, resends, heartbeats, gaps,
  acknacks, nackfrags, throughput, latency) to the parent pair, which gets
  ``buffer-companion-folded``. The companion topic keeps its own numbers with
  ``buffer-companion`` and is shown only with ``--all`` (and with "hide ROS internal
  topics" off in the web viewer) when all its endpoints are linked; an unlinked one stays
  visible with ``buffer-companion-unmatched``. New optional endpoint field
  ``buffer_parent_guid`` in the JSON (schema version unchanged). A ``diff`` of a capture
  taken before this change against one taken after it lists the companion pairs as removed
  unless ``--all`` is given (#119).
* Split IPC namespaces seen from one side's namespace (``shm-reader-port-not-visible`` /
  ``shm-writer-port-not-visible``): a participant counts as listening in the tool's
  namespace when every SHM port of it is held there and one of them is announced by no
  other participant and by an endpoint other than its ``ros_discovery_info`` reader. A held
  port whose number another participant announced too made the participant undecidable, so
  a talker whose 7000+ number a second node in the listener's namespace also took was
  ``SHM`` / ``certain`` without a warning while nothing arrived. The reader's 7000+ port no
  longer counts as proof: any Fast DDS participant of the namespace takes such numbers,
  whatever its domain. Humble, with one SHM port per participant numbered per IPC
  namespace, cannot tell this case. New integration scenario
  ``hostnet_split_shm_shared_port`` (#118).
* ``--stats``: ``stats.participants_with_stats`` (and the footer's "statistics from N
  participant(s)") lists the participants that published a statistics sample, taken from
  the sample's writer GUID. It also held the participants named in a sample: the remote
  sender of ``RTPS_LOST`` (the tool's own participants, from the nodes' lost multicast
  discovery datagrams, also of earlier runs since statistics are TRANSIENT_LOCAL) and the
  remote writer of ``HISTORY_LATENCY``. When only the reader's participant had statistics,
  the writer's counted as having them, so ``stats-not-enabled-on-writer`` was missing and
  a data-sharing pair could get ``datasharing-confirmed-no-traffic`` with ``certain``
  confidence. Older JSON documents can keep the extra prefixes. The launch test adds a pair
  with statistics on the listener only; ``stats_multi_container`` and
  ``hostnet_split_stats`` check the exact set (#113).
* Node names across IPC namespaces: the tool reads ``ros_discovery_info`` itself, with a
  reader on its raw participant that announces only the non-SHM unicast locators the
  participant listens on (like the statistics readers), and names the endpoints the ROS
  graph API cannot name from it. rclcpp's participant announces SHM, so a same-host node in
  another IPC namespace than the tool wrote those samples into its own ``/dev/shm`` and the
  tool showed ``_NODE_NAMESPACE_UNKNOWN_/_NODE_NAME_UNKNOWN_``. On Humble the reader has a
  participant of its own without the SHM transport (Fast DDS 2.6 gives a participant with
  SHM only the SHM locator of a same-host endpoint). A name that is still unknown
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

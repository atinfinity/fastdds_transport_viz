// Generated from sample.json so that the viewer can show it when opened from file://
window.TRANSPORT_VIZ_SAMPLE = {
 "domain": 0,
 "local_host_id": "010f40ec",
 "observation_seconds": 6.007788128,
 "observed_at": "2026-09-12T13:36:04Z",
 "reason_code_descriptions": {
  "both-shm-locators": "Both endpoints announce a SHM locator. On the same host Fast DDS then uses the shared memory transport exclusively for user data (discovery still goes over UDP).",
  "common-udpv4-locator": "The reader announces a UDPv4 locator and the writer speaks UDPv4.",
  "datasharing-disabled-writer": "The writer announces data-sharing OFF (explicitly disabled, or AUTO resolved to OFF because the type is unbounded / the history memory policy is not preallocated).",
  "measured-shm-traffic": "Statistics show RTPS packets from the writer's participant to the reader's SHM locator.",
  "measured-udpv4-traffic": "Statistics show RTPS packets from the writer's participant to the reader's UDPv4 locator.",
  "no-matching-reader": "No subscription was discovered for this topic.",
  "reader-no-shm-locator": "The reader's participant announces no SHM locator: SHM transport is not instantiated on its side (e.g. FASTDDS_BUILTIN_TRANSPORTS=UDPv4, or an XML profile without SHM).",
  "same-host-guid": "Writer and reader GUID prefixes share the same first 4 bytes, which is how Fast DDS decides both participants run on the same host.",
  "shm-stale-files": "Fast DDS files in the shared-memory directory whose lock nobody holds: their owner process ended without cleaning up (a crash or a kill). They keep consuming /dev/shm.",
  "stats-writer-instance-limit-suspected": "The writer's participant reports traffic to 10 or more locators but none to this reader. Before Fast DDS 3.5 the statistics DataWriter keeps the default resource limit of 10 instances (one per destination locator), so counters for further locators are never published.",
  "writer-no-shm-locator": "The writer's participant announces no SHM locator: SHM transport is not instantiated on its side (e.g. FASTDDS_BUILTIN_TRANSPORTS=UDPv4, or an XML profile without SHM)."
 },
 "reason_code_remedies": {
  "both-shm-locators": null,
  "common-udpv4-locator": null,
  "datasharing-disabled-writer": "Enable data-sharing on the writer: <data_sharing><kind>ON</kind> in its data_writer XML profile, a bounded (fixed-size) message type and a PREALLOCATED or PREALLOCATED_WITH_REALLOC <historyMemoryPolicy>.",
  "measured-shm-traffic": null,
  "measured-udpv4-traffic": null,
  "no-matching-reader": "Start a subscription on this topic, or check the topic name, namespace and remappings of the node expected to subscribe to it.",
  "reader-no-shm-locator": "Enable SHM on the reader's participant: unset FASTDDS_BUILTIN_TRANSPORTS or set it to DEFAULT / LARGE_DATA (Fast DDS >= 2.11), or add a SHM <transport_descriptor> to its XML participant profile.",
  "same-host-guid": null,
  "shm-stale-files": "Run 'fastdds shm clean' to remove the files whose owner is gone.",
  "stats-writer-instance-limit-suspected": "Start the observed nodes with FASTRTPS_DEFAULT_PROFILES_FILE (Fast DDS 2.x) or FASTDDS_DEFAULT_PROFILES_FILE (3.x, where ROS 2 nodes also accept FASTRTPS_) pointing at this package's config/statistics.xml (a data_writer profile per statistics alias whose <resourceLimitsQos> sets max_instances to 0).",
  "writer-no-shm-locator": "Enable SHM on the writer's participant: unset FASTDDS_BUILTIN_TRANSPORTS or set it to DEFAULT / LARGE_DATA (Fast DDS >= 2.11), or add a SHM <transport_descriptor> to its XML participant profile."
 },
 "schema_version": 1,
 "shm": {
  "available": true,
  "checked_ports": [
   7000,
   7001,
   7002,
   7003,
   7413,
   7415,
   7417,
   7419
  ],
  "datasharing_histories": 0,
  "datasharing_notifications": 0,
  "datasharing_unmatched": 0,
  "fastdds_bytes": 6123456,
  "free_bytes": 4153659392,
  "missing_ports": [],
  "nodes_visible": true,
  "other_host_participants": 0,
  "path": "/dev/shm",
  "ports": 12,
  "segments": 10,
  "stale_ports": 1,
  "stale_segments": 4,
  "total_bytes": 4159885312,
  "used_bytes": 6225920,
  "warnings": [
   "shm-stale-files"
  ]
 },
 "stats": {
  "data_count": {
   "01.0f.40.ec.35.00.5d.09.00.00.00.00|00.00.01.03": {
    "first": 24,
    "last": 24,
    "samples": 1
   },
   "01.0f.40.ec.35.00.5d.09.00.00.00.00|00.00.10.03": {
    "first": 12,
    "last": 12,
    "samples": 1
   },
   "01.0f.40.ec.36.00.87.0c.00.00.00.00|00.00.01.03": {
    "first": 25,
    "last": 26,
    "samples": 2
   },
   "01.0f.40.ec.36.00.87.0c.00.00.00.00|00.00.10.03": {
    "first": 12,
    "last": 12,
    "samples": 1
   },
   "01.0f.40.ec.37.00.26.ca.00.00.00.00|00.00.01.03": {
    "first": 27,
    "last": 27,
    "samples": 1
   },
   "01.0f.40.ec.37.00.26.ca.00.00.00.00|00.00.10.03": {
    "first": 24,
    "last": 24,
    "samples": 1
   },
   "01.0f.40.ec.37.00.26.ca.00.00.00.00|00.00.14.03": {
    "first": 10,
    "last": 14,
    "samples": 2
   },
   "01.0f.40.ec.38.00.58.91.00.00.00.00|00.00.01.03": {
    "first": 4,
    "last": 5,
    "samples": 2
   },
   "01.0f.40.ec.52.00.86.93.00.00.00.00|00.00.01.03": {
    "first": 5,
    "last": 5,
    "samples": 1
   },
   "01.0f.40.ec.52.00.86.93.00.00.00.00|00.00.14.03": {
    "first": 54,
    "last": 78,
    "samples": 2
   }
  },
  "enabled": true,
  "lost": [],
  "participants_with_stats": [
   "01.0f.40.ec.35.00.5d.09.00.00.00.00",
   "01.0f.40.ec.36.00.87.0c.00.00.00.00",
   "01.0f.40.ec.37.00.26.ca.00.00.00.00",
   "01.0f.40.ec.38.00.58.91.00.00.00.00",
   "01.0f.40.ec.52.00.86.93.00.00.00.00"
  ],
  "physical": {
   "01.0f.40.ec.35.00.5d.09.00.00.00.00": {
    "host": "d87e498ecc1f:14564170186182098944",
    "process": "53",
    "user": "root"
   },
   "01.0f.40.ec.36.00.87.0c.00.00.00.00": {
    "host": "d87e498ecc1f:14564170186182098944",
    "process": "54",
    "user": "root"
   },
   "01.0f.40.ec.37.00.26.ca.00.00.00.00": {
    "host": "d87e498ecc1f:14564170186182098944",
    "process": "55",
    "user": "root"
   },
   "01.0f.40.ec.38.00.58.91.00.00.00.00": {
    "host": "d87e498ecc1f:14564170186182098944",
    "process": "56",
    "user": "root"
   },
   "01.0f.40.ec.52.00.86.93.00.00.00.00": {
    "host": "d87e498ecc1f:14564170186182098944",
    "process": "82",
    "user": "root"
   }
  },
  "samples": 233,
  "statistics_writers": [
   {
    "participant_guid_prefix": "01.0f.40.ec.35.00.5d.09.00.00.00.00",
    "topic": "_fastdds_statistics_acknack_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.35.00.5d.09.00.00.00.00",
    "topic": "_fastdds_statistics_data_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.35.00.5d.09.00.00.00.00",
    "topic": "_fastdds_statistics_gap_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.35.00.5d.09.00.00.00.00",
    "topic": "_fastdds_statistics_heartbeat_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.35.00.5d.09.00.00.00.00",
    "topic": "_fastdds_statistics_history2history_latency"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.35.00.5d.09.00.00.00.00",
    "topic": "_fastdds_statistics_nackfrag_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.35.00.5d.09.00.00.00.00",
    "topic": "_fastdds_statistics_physical_data"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.35.00.5d.09.00.00.00.00",
    "topic": "_fastdds_statistics_publication_throughput"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.35.00.5d.09.00.00.00.00",
    "topic": "_fastdds_statistics_resent_datas"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.35.00.5d.09.00.00.00.00",
    "topic": "_fastdds_statistics_rtps_lost"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.35.00.5d.09.00.00.00.00",
    "topic": "_fastdds_statistics_rtps_sent"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.36.00.87.0c.00.00.00.00",
    "topic": "_fastdds_statistics_acknack_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.36.00.87.0c.00.00.00.00",
    "topic": "_fastdds_statistics_data_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.36.00.87.0c.00.00.00.00",
    "topic": "_fastdds_statistics_gap_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.36.00.87.0c.00.00.00.00",
    "topic": "_fastdds_statistics_heartbeat_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.36.00.87.0c.00.00.00.00",
    "topic": "_fastdds_statistics_history2history_latency"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.36.00.87.0c.00.00.00.00",
    "topic": "_fastdds_statistics_nackfrag_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.36.00.87.0c.00.00.00.00",
    "topic": "_fastdds_statistics_physical_data"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.36.00.87.0c.00.00.00.00",
    "topic": "_fastdds_statistics_publication_throughput"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.36.00.87.0c.00.00.00.00",
    "topic": "_fastdds_statistics_resent_datas"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.36.00.87.0c.00.00.00.00",
    "topic": "_fastdds_statistics_rtps_lost"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.36.00.87.0c.00.00.00.00",
    "topic": "_fastdds_statistics_rtps_sent"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.37.00.26.ca.00.00.00.00",
    "topic": "_fastdds_statistics_acknack_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.37.00.26.ca.00.00.00.00",
    "topic": "_fastdds_statistics_data_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.37.00.26.ca.00.00.00.00",
    "topic": "_fastdds_statistics_gap_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.37.00.26.ca.00.00.00.00",
    "topic": "_fastdds_statistics_heartbeat_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.37.00.26.ca.00.00.00.00",
    "topic": "_fastdds_statistics_history2history_latency"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.37.00.26.ca.00.00.00.00",
    "topic": "_fastdds_statistics_nackfrag_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.37.00.26.ca.00.00.00.00",
    "topic": "_fastdds_statistics_physical_data"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.37.00.26.ca.00.00.00.00",
    "topic": "_fastdds_statistics_publication_throughput"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.37.00.26.ca.00.00.00.00",
    "topic": "_fastdds_statistics_resent_datas"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.37.00.26.ca.00.00.00.00",
    "topic": "_fastdds_statistics_rtps_lost"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.37.00.26.ca.00.00.00.00",
    "topic": "_fastdds_statistics_rtps_sent"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.38.00.58.91.00.00.00.00",
    "topic": "_fastdds_statistics_acknack_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.38.00.58.91.00.00.00.00",
    "topic": "_fastdds_statistics_data_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.38.00.58.91.00.00.00.00",
    "topic": "_fastdds_statistics_gap_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.38.00.58.91.00.00.00.00",
    "topic": "_fastdds_statistics_heartbeat_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.38.00.58.91.00.00.00.00",
    "topic": "_fastdds_statistics_history2history_latency"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.38.00.58.91.00.00.00.00",
    "topic": "_fastdds_statistics_nackfrag_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.38.00.58.91.00.00.00.00",
    "topic": "_fastdds_statistics_physical_data"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.38.00.58.91.00.00.00.00",
    "topic": "_fastdds_statistics_publication_throughput"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.38.00.58.91.00.00.00.00",
    "topic": "_fastdds_statistics_resent_datas"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.38.00.58.91.00.00.00.00",
    "topic": "_fastdds_statistics_rtps_lost"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.38.00.58.91.00.00.00.00",
    "topic": "_fastdds_statistics_rtps_sent"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.52.00.86.93.00.00.00.00",
    "topic": "_fastdds_statistics_acknack_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.52.00.86.93.00.00.00.00",
    "topic": "_fastdds_statistics_data_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.52.00.86.93.00.00.00.00",
    "topic": "_fastdds_statistics_gap_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.52.00.86.93.00.00.00.00",
    "topic": "_fastdds_statistics_heartbeat_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.52.00.86.93.00.00.00.00",
    "topic": "_fastdds_statistics_history2history_latency"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.52.00.86.93.00.00.00.00",
    "topic": "_fastdds_statistics_nackfrag_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.52.00.86.93.00.00.00.00",
    "topic": "_fastdds_statistics_physical_data"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.52.00.86.93.00.00.00.00",
    "topic": "_fastdds_statistics_publication_throughput"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.52.00.86.93.00.00.00.00",
    "topic": "_fastdds_statistics_resent_datas"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.52.00.86.93.00.00.00.00",
    "topic": "_fastdds_statistics_rtps_lost"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.52.00.86.93.00.00.00.00",
    "topic": "_fastdds_statistics_rtps_sent"
   }
  ],
  "throughput": {
   "01.0f.40.ec.35.00.5d.09.00.00.00.00|00.00.01.03": {
    "last": 391427.75,
    "mean": 391427.75,
    "samples": 1
   },
   "01.0f.40.ec.35.00.5d.09.00.00.00.00|00.00.03.03": {
    "last": 115.97248840332031,
    "mean": 116.17436599731445,
    "samples": 2
   },
   "01.0f.40.ec.35.00.5d.09.00.00.00.00|00.00.10.03": {
    "last": 12324657.0,
    "mean": 12324657.0,
    "samples": 1
   },
   "01.0f.40.ec.36.00.87.0c.00.00.00.00|00.00.01.03": {
    "last": 647140.8125,
    "mean": 647140.8125,
    "samples": 1
   },
   "01.0f.40.ec.36.00.87.0c.00.00.00.00|00.00.03.03": {
    "last": 119.97345733642578,
    "mean": 120.18891525268555,
    "samples": 2
   },
   "01.0f.40.ec.36.00.87.0c.00.00.00.00|00.00.10.03": {
    "last": 1800281.25,
    "mean": 1800281.25,
    "samples": 1
   },
   "01.0f.40.ec.37.00.26.ca.00.00.00.00|00.00.01.03": {
    "last": 577330.9375,
    "mean": 577330.9375,
    "samples": 1
   },
   "01.0f.40.ec.37.00.26.ca.00.00.00.00|00.00.03.03": {
    "last": 112.05616760253906,
    "mean": 112.14136123657227,
    "samples": 2
   },
   "01.0f.40.ec.37.00.26.ca.00.00.00.00|00.00.10.03": {
    "last": 4676923.0,
    "mean": 4676923.0,
    "samples": 1
   },
   "01.0f.40.ec.37.00.26.ca.00.00.00.00|00.00.14.03": {
    "last": 23.020750045776367,
    "mean": 23.044191360473633,
    "samples": 2
   },
   "01.0f.40.ec.38.00.58.91.00.00.00.00|00.00.01.03": {
    "last": 471583.0,
    "mean": 471583.0,
    "samples": 1
   },
   "01.0f.40.ec.38.00.58.91.00.00.00.00|00.00.03.03": {
    "last": 62.819637298583984,
    "mean": 64.38638877868652,
    "samples": 2
   },
   "01.0f.40.ec.38.00.58.91.00.00.00.00|00.00.10.03": {
    "last": 23852116.0,
    "mean": 23852116.0,
    "samples": 1
   },
   "01.0f.40.ec.52.00.86.93.00.00.00.00|00.00.01.03": {
    "last": 1016374.3125,
    "mean": 1016374.3125,
    "samples": 1
   },
   "01.0f.40.ec.52.00.86.93.00.00.00.00|00.00.10.03": {
    "last": 23412350.0,
    "mean": 23412350.0,
    "samples": 1
   },
   "01.0f.40.ec.52.00.86.93.00.00.00.00|00.00.14.03": {
    "last": 79.92034912109375,
    "mean": 80.71406936645508,
    "samples": 2
   }
  },
  "traffic": [
   {
    "bytes": 5572.0,
    "bytes_first": 5572.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7000
    },
    "packets": 25,
    "packets_first": 25,
    "src_participant_guid_prefix": "01.0f.40.ec.35.00.5d.09.00.00.00.00"
   },
   {
    "bytes": 25012.0,
    "bytes_first": 24504.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7410
    },
    "packets": 54,
    "packets_first": 53,
    "src_participant_guid_prefix": "01.0f.40.ec.35.00.5d.09.00.00.00.00"
   },
   {
    "bytes": 4248.0,
    "bytes_first": 4012.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7411
    },
    "packets": 19,
    "packets_first": 17,
    "src_participant_guid_prefix": "01.0f.40.ec.35.00.5d.09.00.00.00.00"
   },
   {
    "bytes": 23852.0,
    "bytes_first": 23344.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7414
    },
    "packets": 52,
    "packets_first": 51,
    "src_participant_guid_prefix": "01.0f.40.ec.35.00.5d.09.00.00.00.00"
   },
   {
    "bytes": 23332.0,
    "bytes_first": 22824.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7416
    },
    "packets": 33,
    "packets_first": 32,
    "src_participant_guid_prefix": "01.0f.40.ec.35.00.5d.09.00.00.00.00"
   },
   {
    "bytes": 23172.0,
    "bytes_first": 22664.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7418
    },
    "packets": 31,
    "packets_first": 30,
    "src_participant_guid_prefix": "01.0f.40.ec.35.00.5d.09.00.00.00.00"
   },
   {
    "bytes": 4984.0,
    "bytes_first": 4984.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7001
    },
    "packets": 23,
    "packets_first": 23,
    "src_participant_guid_prefix": "01.0f.40.ec.35.00.5d.09.00.00.00.00"
   },
   {
    "bytes": 940.0,
    "bytes_first": 940.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7002
    },
    "packets": 5,
    "packets_first": 5,
    "src_participant_guid_prefix": "01.0f.40.ec.35.00.5d.09.00.00.00.00"
   },
   {
    "bytes": 1756.0,
    "bytes_first": 1520.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7415
    },
    "packets": 14,
    "packets_first": 12,
    "src_participant_guid_prefix": "01.0f.40.ec.35.00.5d.09.00.00.00.00"
   },
   {
    "bytes": 1264.0,
    "bytes_first": 1152.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7417
    },
    "packets": 10,
    "packets_first": 9,
    "src_participant_guid_prefix": "01.0f.40.ec.35.00.5d.09.00.00.00.00"
   },
   {
    "bytes": 5936.0,
    "bytes_first": 5824.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7001
    },
    "packets": 27,
    "packets_first": 26,
    "src_participant_guid_prefix": "01.0f.40.ec.36.00.87.0c.00.00.00.00"
   },
   {
    "bytes": 5616.0,
    "bytes_first": 5504.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7002
    },
    "packets": 25,
    "packets_first": 24,
    "src_participant_guid_prefix": "01.0f.40.ec.36.00.87.0c.00.00.00.00"
   },
   {
    "bytes": 944.0,
    "bytes_first": 832.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7003
    },
    "packets": 5,
    "packets_first": 4,
    "src_participant_guid_prefix": "01.0f.40.ec.36.00.87.0c.00.00.00.00"
   },
   {
    "bytes": 24216.0,
    "bytes_first": 23736.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7412
    },
    "packets": 57,
    "packets_first": 56,
    "src_participant_guid_prefix": "01.0f.40.ec.36.00.87.0c.00.00.00.00"
   },
   {
    "bytes": 4064.0,
    "bytes_first": 3704.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7413
    },
    "packets": 20,
    "packets_first": 17,
    "src_participant_guid_prefix": "01.0f.40.ec.36.00.87.0c.00.00.00.00"
   },
   {
    "bytes": 23752.0,
    "bytes_first": 23272.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7414
    },
    "packets": 56,
    "packets_first": 55,
    "src_participant_guid_prefix": "01.0f.40.ec.36.00.87.0c.00.00.00.00"
   },
   {
    "bytes": 2724.0,
    "bytes_first": 2364.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7415
    },
    "packets": 19,
    "packets_first": 16,
    "src_participant_guid_prefix": "01.0f.40.ec.36.00.87.0c.00.00.00.00"
   },
   {
    "bytes": 22296.0,
    "bytes_first": 21816.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7416
    },
    "packets": 33,
    "packets_first": 32,
    "src_participant_guid_prefix": "01.0f.40.ec.36.00.87.0c.00.00.00.00"
   },
   {
    "bytes": 1264.0,
    "bytes_first": 1028.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7417
    },
    "packets": 10,
    "packets_first": 8,
    "src_participant_guid_prefix": "01.0f.40.ec.36.00.87.0c.00.00.00.00"
   },
   {
    "bytes": 22220.0,
    "bytes_first": 21740.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7418
    },
    "packets": 32,
    "packets_first": 31,
    "src_participant_guid_prefix": "01.0f.40.ec.36.00.87.0c.00.00.00.00"
   },
   {
    "bytes": 5824.0,
    "bytes_first": 5824.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7000
    },
    "packets": 27,
    "packets_first": 27,
    "src_participant_guid_prefix": "01.0f.40.ec.37.00.26.ca.00.00.00.00"
   },
   {
    "bytes": 25144.0,
    "bytes_first": 24636.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7410
    },
    "packets": 56,
    "packets_first": 55,
    "src_participant_guid_prefix": "01.0f.40.ec.37.00.26.ca.00.00.00.00"
   },
   {
    "bytes": 5576.0,
    "bytes_first": 4796.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7411
    },
    "packets": 30,
    "packets_first": 24,
    "src_participant_guid_prefix": "01.0f.40.ec.37.00.26.ca.00.00.00.00"
   },
   {
    "bytes": 25140.0,
    "bytes_first": 24632.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7412
    },
    "packets": 56,
    "packets_first": 55,
    "src_participant_guid_prefix": "01.0f.40.ec.37.00.26.ca.00.00.00.00"
   },
   {
    "bytes": 23380.0,
    "bytes_first": 22872.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7416
    },
    "packets": 34,
    "packets_first": 33,
    "src_participant_guid_prefix": "01.0f.40.ec.37.00.26.ca.00.00.00.00"
   },
   {
    "bytes": 23244.0,
    "bytes_first": 22736.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7418
    },
    "packets": 32,
    "packets_first": 31,
    "src_participant_guid_prefix": "01.0f.40.ec.37.00.26.ca.00.00.00.00"
   },
   {
    "bytes": 5824.0,
    "bytes_first": 5824.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7000
    },
    "packets": 27,
    "packets_first": 27,
    "src_participant_guid_prefix": "01.0f.40.ec.37.00.26.ca.00.00.00.00"
   },
   {
    "bytes": 936.0,
    "bytes_first": 936.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7002
    },
    "packets": 5,
    "packets_first": 5,
    "src_participant_guid_prefix": "01.0f.40.ec.37.00.26.ca.00.00.00.00"
   },
   {
    "bytes": 5320.0,
    "bytes_first": 4540.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7413
    },
    "packets": 28,
    "packets_first": 22,
    "src_participant_guid_prefix": "01.0f.40.ec.37.00.26.ca.00.00.00.00"
   },
   {
    "bytes": 1400.0,
    "bytes_first": 1276.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7417
    },
    "packets": 11,
    "packets_first": 10,
    "src_participant_guid_prefix": "01.0f.40.ec.37.00.26.ca.00.00.00.00"
   },
   {
    "bytes": 924.0,
    "bytes_first": 924.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7000
    },
    "packets": 5,
    "packets_first": 5,
    "src_participant_guid_prefix": "01.0f.40.ec.38.00.58.91.00.00.00.00"
   },
   {
    "bytes": 25052.0,
    "bytes_first": 24544.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7410
    },
    "packets": 54,
    "packets_first": 53,
    "src_participant_guid_prefix": "01.0f.40.ec.38.00.58.91.00.00.00.00"
   },
   {
    "bytes": 25052.0,
    "bytes_first": 24544.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7412
    },
    "packets": 54,
    "packets_first": 53,
    "src_participant_guid_prefix": "01.0f.40.ec.38.00.58.91.00.00.00.00"
   },
   {
    "bytes": 23892.0,
    "bytes_first": 23384.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7414
    },
    "packets": 52,
    "packets_first": 51,
    "src_participant_guid_prefix": "01.0f.40.ec.38.00.58.91.00.00.00.00"
   },
   {
    "bytes": 23052.0,
    "bytes_first": 22544.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7418
    },
    "packets": 29,
    "packets_first": 28,
    "src_participant_guid_prefix": "01.0f.40.ec.38.00.58.91.00.00.00.00"
   },
   {
    "bytes": 3556.0,
    "bytes_first": 3048.0,
    "dst_locator": {
     "address": "239.255.0.1",
     "kind": "UDPv4",
     "port": 7400
    },
    "packets": 7,
    "packets_first": 6,
    "src_participant_guid_prefix": "01.0f.40.ec.38.00.58.91.00.00.00.00"
   },
   {
    "bytes": 924.0,
    "bytes_first": 924.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7000
    },
    "packets": 5,
    "packets_first": 5,
    "src_participant_guid_prefix": "01.0f.40.ec.38.00.58.91.00.00.00.00"
   },
   {
    "bytes": 940.0,
    "bytes_first": 940.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7003
    },
    "packets": 5,
    "packets_first": 5,
    "src_participant_guid_prefix": "01.0f.40.ec.38.00.58.91.00.00.00.00"
   },
   {
    "bytes": 1128.0,
    "bytes_first": 1004.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7413
    },
    "packets": 9,
    "packets_first": 8,
    "src_participant_guid_prefix": "01.0f.40.ec.38.00.58.91.00.00.00.00"
   },
   {
    "bytes": 1872.0,
    "bytes_first": 1624.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7419
    },
    "packets": 15,
    "packets_first": 13,
    "src_participant_guid_prefix": "01.0f.40.ec.38.00.58.91.00.00.00.00"
   },
   {
    "bytes": 924.0,
    "bytes_first": 924.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7000
    },
    "packets": 5,
    "packets_first": 5,
    "src_participant_guid_prefix": "01.0f.40.ec.52.00.86.93.00.00.00.00"
   },
   {
    "bytes": 24668.0,
    "bytes_first": 24160.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7410
    },
    "packets": 51,
    "packets_first": 50,
    "src_participant_guid_prefix": "01.0f.40.ec.52.00.86.93.00.00.00.00"
   },
   {
    "bytes": 1256.0,
    "bytes_first": 1008.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7411
    },
    "packets": 10,
    "packets_first": 8,
    "src_participant_guid_prefix": "01.0f.40.ec.52.00.86.93.00.00.00.00"
   },
   {
    "bytes": 23300.0,
    "bytes_first": 22792.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7412
    },
    "packets": 47,
    "packets_first": 46,
    "src_participant_guid_prefix": "01.0f.40.ec.52.00.86.93.00.00.00.00"
   },
   {
    "bytes": 24652.0,
    "bytes_first": 24144.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7414
    },
    "packets": 51,
    "packets_first": 50,
    "src_participant_guid_prefix": "01.0f.40.ec.52.00.86.93.00.00.00.00"
   },
   {
    "bytes": 24652.0,
    "bytes_first": 24144.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7416
    },
    "packets": 51,
    "packets_first": 50,
    "src_participant_guid_prefix": "01.0f.40.ec.52.00.86.93.00.00.00.00"
   },
   {
    "bytes": 3556.0,
    "bytes_first": 3048.0,
    "dst_locator": {
     "address": "239.255.0.1",
     "kind": "UDPv4",
     "port": 7400
    },
    "packets": 7,
    "packets_first": 6,
    "src_participant_guid_prefix": "01.0f.40.ec.52.00.86.93.00.00.00.00"
   },
   {
    "bytes": 940.0,
    "bytes_first": 940.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7000
    },
    "packets": 5,
    "packets_first": 5,
    "src_participant_guid_prefix": "01.0f.40.ec.52.00.86.93.00.00.00.00"
   },
   {
    "bytes": 924.0,
    "bytes_first": 924.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7001
    },
    "packets": 5,
    "packets_first": 5,
    "src_participant_guid_prefix": "01.0f.40.ec.52.00.86.93.00.00.00.00"
   },
   {
    "bytes": 1128.0,
    "bytes_first": 880.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7413
    },
    "packets": 9,
    "packets_first": 7,
    "src_participant_guid_prefix": "01.0f.40.ec.52.00.86.93.00.00.00.00"
   }
  ]
 },
 "topics": [
  {
   "dds_topic": "rt/bounded",
   "is_ros_topic": true,
   "latency_s": 0.000524895,
   "lost_packets": 0,
   "pairs": [
    {
     "confidence": "certain",
     "locator": {
      "address": "",
      "kind": "SHM",
      "multicast": false,
      "port": 7417
     },
     "measured": {
      "available": true,
      "bytes": 0.0,
      "bytes_total": 0.0,
      "data_submessages": 24,
      "delivered": true,
      "delivered_samples": 2,
      "latency_s": {
       "last": 0.00038641500000000003,
       "max": 0.000663375,
       "mean": 0.000524895,
       "min": 0.00038641500000000003,
       "samples": 2
      },
      "locators": [],
      "packets": 0,
      "packets_total": 0,
      "reliability": {
       "acknacks": 2,
       "gaps": 0,
       "heartbeats": 3,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 80.71406936645508,
      "transports": []
     },
     "reader_guid": "01.0f.40.ec.38.00.58.91.00.00.00.00|00.00.14.04",
     "reader_host": "d87e498ecc1f",
     "reader_node": "/bounded_sub",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "both-shm-locators"
     ],
     "transport": "SHM",
     "warnings": [
      "stats-writer-instance-limit-suspected"
     ],
     "writer_guid": "01.0f.40.ec.52.00.86.93.00.00.00.00|00.00.14.03",
     "writer_host": "d87e498ecc1f",
     "writer_node": "/bounded_pub"
    }
   ],
   "readers": [
    {
     "datasharing_history_bytes": null,
     "dds_topic": "rt/bounded",
     "dds_type": "std_msgs::msg::dds_::Int32_",
     "guid": "01.0f.40.ec.38.00.58.91.00.00.00.00|00.00.14.04",
     "host": "d87e498ecc1f",
     "host_id": "010f40ec",
     "host_name": "d87e498ecc1f:14564170186182098944",
     "multicast_locators": [],
     "node": "/bounded_sub",
     "participant_guid_prefix": "01.0f.40.ec.38.00.58.91.00.00.00.00",
     "process": "56",
     "qos": {
      "data_sharing": "OFF",
      "data_sharing_domain_ids": [],
      "deadline_s": null,
      "durability": "VOLATILE",
      "liveliness": "AUTOMATIC",
      "liveliness_lease_s": null,
      "ownership": "SHARED",
      "partitions": [],
      "reliability": "RELIABLE"
     },
     "ros_topic": "/bounded",
     "ros_type": "std_msgs/msg/Int32",
     "unicast_locators": [
      {
       "address": "",
       "kind": "SHM",
       "port": 7417
      },
      {
       "address": "127.0.0.1",
       "kind": "UDPv4",
       "port": 7417
      }
     ]
    }
   ],
   "resent_datas": 0,
   "throughput_bytes_per_s": 80.71406936645508,
   "topic": "/bounded",
   "type": "std_msgs/msg/Int32",
   "unmatched_reasons": [],
   "writers": [
    {
     "datasharing_history_bytes": null,
     "dds_topic": "rt/bounded",
     "dds_type": "std_msgs::msg::dds_::Int32_",
     "guid": "01.0f.40.ec.52.00.86.93.00.00.00.00|00.00.14.03",
     "host": "d87e498ecc1f",
     "host_id": "010f40ec",
     "host_name": "d87e498ecc1f:14564170186182098944",
     "multicast_locators": [],
     "node": "/bounded_pub",
     "participant_guid_prefix": "01.0f.40.ec.52.00.86.93.00.00.00.00",
     "process": "82",
     "qos": {
      "data_sharing": "OFF",
      "data_sharing_domain_ids": [],
      "deadline_s": null,
      "durability": "VOLATILE",
      "liveliness": "AUTOMATIC",
      "liveliness_lease_s": null,
      "ownership": "SHARED",
      "partitions": [],
      "reliability": "RELIABLE"
     },
     "ros_topic": "/bounded",
     "ros_type": "std_msgs/msg/Int32",
     "unicast_locators": [
      {
       "address": "",
       "kind": "SHM",
       "port": 7419
      },
      {
       "address": "127.0.0.1",
       "kind": "UDPv4",
       "port": 7419
      }
     ]
    }
   ]
  },
  {
   "dds_topic": "rt/chatter",
   "is_ros_topic": true,
   "latency_s": 0.0003064375,
   "lost_packets": 0,
   "pairs": [
    {
     "confidence": "certain",
     "locator": {
      "address": "",
      "kind": "SHM",
      "multicast": false,
      "port": 7413
     },
     "measured": {
      "available": true,
      "bytes": 780.0,
      "bytes_total": 5320.0,
      "data_submessages": 4,
      "delivered": true,
      "delivered_samples": 2,
      "latency_s": {
       "last": 0.000564584,
       "max": 0.000564584,
       "mean": 0.0003064375,
       "min": 4.8291e-05,
       "samples": 2
      },
      "locators": [
       {
        "address": "",
        "bytes": 780.0,
        "kind": "SHM",
        "packets": 6,
        "port": 7413
       }
      ],
      "packets": 6,
      "packets_total": 28,
      "reliability": {
       "acknacks": 1,
       "gaps": 0,
       "heartbeats": 1,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 23.044191360473633,
      "transports": [
       "SHM"
      ]
     },
     "reader_guid": "01.0f.40.ec.35.00.5d.09.00.00.00.00|00.00.14.04",
     "reader_host": "d87e498ecc1f",
     "reader_node": "/listener",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "both-shm-locators",
      "measured-shm-traffic"
     ],
     "transport": "SHM",
     "warnings": [],
     "writer_guid": "01.0f.40.ec.37.00.26.ca.00.00.00.00|00.00.14.03",
     "writer_host": "d87e498ecc1f",
     "writer_node": "/talker"
    },
    {
     "confidence": "certain",
     "locator": {
      "address": "127.0.0.1",
      "kind": "UDPv4",
      "multicast": false,
      "port": 7411
     },
     "measured": {
      "available": true,
      "bytes": 780.0,
      "bytes_total": 5576.0,
      "data_submessages": 4,
      "delivered": true,
      "delivered_samples": 2,
      "latency_s": {
       "last": 0.0005291240000000001,
       "max": 0.0005291240000000001,
       "mean": 0.00028370800000000003,
       "min": 3.8292000000000005e-05,
       "samples": 2
      },
      "locators": [
       {
        "address": "127.0.0.1",
        "bytes": 780.0,
        "kind": "UDPv4",
        "packets": 6,
        "port": 7411
       }
      ],
      "packets": 6,
      "packets_total": 30,
      "reliability": {
       "acknacks": 1,
       "gaps": 0,
       "heartbeats": 1,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 23.044191360473633,
      "transports": [
       "UDPv4"
      ]
     },
     "reader_guid": "01.0f.40.ec.36.00.87.0c.00.00.00.00|00.00.14.04",
     "reader_host": "d87e498ecc1f",
     "reader_node": "/listener_udp",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "reader-no-shm-locator",
      "common-udpv4-locator",
      "measured-udpv4-traffic"
     ],
     "transport": "UDPv4",
     "warnings": [],
     "writer_guid": "01.0f.40.ec.37.00.26.ca.00.00.00.00|00.00.14.03",
     "writer_host": "d87e498ecc1f",
     "writer_node": "/talker"
    }
   ],
   "readers": [
    {
     "datasharing_history_bytes": null,
     "dds_topic": "rt/chatter",
     "dds_type": "std_msgs::msg::dds_::String_",
     "guid": "01.0f.40.ec.35.00.5d.09.00.00.00.00|00.00.14.04",
     "host": "d87e498ecc1f",
     "host_id": "010f40ec",
     "host_name": "d87e498ecc1f:14564170186182098944",
     "multicast_locators": [],
     "node": "/listener",
     "participant_guid_prefix": "01.0f.40.ec.35.00.5d.09.00.00.00.00",
     "process": "53",
     "qos": {
      "data_sharing": "OFF",
      "data_sharing_domain_ids": [],
      "deadline_s": null,
      "durability": "VOLATILE",
      "liveliness": "AUTOMATIC",
      "liveliness_lease_s": null,
      "ownership": "SHARED",
      "partitions": [],
      "reliability": "RELIABLE"
     },
     "ros_topic": "/chatter",
     "ros_type": "std_msgs/msg/String",
     "unicast_locators": [
      {
       "address": "",
       "kind": "SHM",
       "port": 7413
      },
      {
       "address": "127.0.0.1",
       "kind": "UDPv4",
       "port": 7413
      }
     ]
    },
    {
     "datasharing_history_bytes": null,
     "dds_topic": "rt/chatter",
     "dds_type": "std_msgs::msg::dds_::String_",
     "guid": "01.0f.40.ec.36.00.87.0c.00.00.00.00|00.00.14.04",
     "host": "d87e498ecc1f",
     "host_id": "010f40ec",
     "host_name": "d87e498ecc1f:14564170186182098944",
     "multicast_locators": [],
     "node": "/listener_udp",
     "participant_guid_prefix": "01.0f.40.ec.36.00.87.0c.00.00.00.00",
     "process": "54",
     "qos": {
      "data_sharing": "OFF",
      "data_sharing_domain_ids": [],
      "deadline_s": null,
      "durability": "VOLATILE",
      "liveliness": "AUTOMATIC",
      "liveliness_lease_s": null,
      "ownership": "SHARED",
      "partitions": [],
      "reliability": "RELIABLE"
     },
     "ros_topic": "/chatter",
     "ros_type": "std_msgs/msg/String",
     "unicast_locators": [
      {
       "address": "127.0.0.1",
       "kind": "UDPv4",
       "port": 7411
      }
     ]
    }
   ],
   "resent_datas": 0,
   "throughput_bytes_per_s": 23.044191360473633,
   "topic": "/chatter",
   "type": "std_msgs/msg/String",
   "unmatched_reasons": [],
   "writers": [
    {
     "datasharing_history_bytes": null,
     "dds_topic": "rt/chatter",
     "dds_type": "std_msgs::msg::dds_::String_",
     "guid": "01.0f.40.ec.37.00.26.ca.00.00.00.00|00.00.14.03",
     "host": "d87e498ecc1f",
     "host_id": "010f40ec",
     "host_name": "d87e498ecc1f:14564170186182098944",
     "multicast_locators": [],
     "node": "/talker",
     "participant_guid_prefix": "01.0f.40.ec.37.00.26.ca.00.00.00.00",
     "process": "55",
     "qos": {
      "data_sharing": "OFF",
      "data_sharing_domain_ids": [],
      "deadline_s": null,
      "durability": "VOLATILE",
      "liveliness": "AUTOMATIC",
      "liveliness_lease_s": null,
      "ownership": "SHARED",
      "partitions": [],
      "reliability": "RELIABLE"
     },
     "ros_topic": "/chatter",
     "ros_type": "std_msgs/msg/String",
     "unicast_locators": [
      {
       "address": "",
       "kind": "SHM",
       "port": 7415
      },
      {
       "address": "127.0.0.1",
       "kind": "UDPv4",
       "port": 7415
      }
     ]
    }
   ]
  },
  {
   "dds_topic": "rt/parameter_events",
   "is_ros_topic": true,
   "latency_s": 0.18907820800000003,
   "lost_packets": 0,
   "pairs": [
    {
     "confidence": "certain",
     "locator": {
      "address": "",
      "kind": "SHM",
      "multicast": false,
      "port": 7413
     },
     "measured": {
      "available": true,
      "bytes": 0.0,
      "bytes_total": 0.0,
      "data_submessages": 0,
      "delivered": true,
      "delivered_samples": 1,
      "latency_s": {
       "last": 1.041e-06,
       "max": 1.041e-06,
       "mean": 1.041e-06,
       "min": 1.041e-06,
       "samples": 1
      },
      "locators": [],
      "packets": 0,
      "packets_total": 0,
      "reliability": {
       "acknacks": 1,
       "gaps": 1,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 12324657.0,
      "transports": []
     },
     "reader_guid": "01.0f.40.ec.35.00.5d.09.00.00.00.00|00.00.11.04",
     "reader_host": "d87e498ecc1f",
     "reader_node": "/listener",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "both-shm-locators"
     ],
     "transport": "SHM",
     "warnings": [
      "stats-writer-instance-limit-suspected"
     ],
     "writer_guid": "01.0f.40.ec.35.00.5d.09.00.00.00.00|00.00.10.03",
     "writer_host": "d87e498ecc1f",
     "writer_node": "/listener"
    },
    {
     "confidence": "certain",
     "locator": {
      "address": "127.0.0.1",
      "kind": "UDPv4",
      "multicast": false,
      "port": 7411
     },
     "measured": {
      "available": true,
      "bytes": 236.0,
      "bytes_total": 4248.0,
      "data_submessages": 0,
      "delivered": true,
      "delivered_samples": 1,
      "latency_s": {
       "last": 0.18907820800000003,
       "max": 0.18907820800000003,
       "mean": 0.18907820800000003,
       "min": 0.18907820800000003,
       "samples": 1
      },
      "locators": [
       {
        "address": "127.0.0.1",
        "bytes": 236.0,
        "kind": "UDPv4",
        "packets": 2,
        "port": 7411
       }
      ],
      "packets": 2,
      "packets_total": 19,
      "reliability": {
       "acknacks": 1,
       "gaps": 1,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 12324657.0,
      "transports": [
       "UDPv4"
      ]
     },
     "reader_guid": "01.0f.40.ec.36.00.87.0c.00.00.00.00|00.00.11.04",
     "reader_host": "d87e498ecc1f",
     "reader_node": "/listener_udp",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "reader-no-shm-locator",
      "common-udpv4-locator",
      "measured-udpv4-traffic"
     ],
     "transport": "UDPv4",
     "warnings": [],
     "writer_guid": "01.0f.40.ec.35.00.5d.09.00.00.00.00|00.00.10.03",
     "writer_host": "d87e498ecc1f",
     "writer_node": "/listener"
    },
    {
     "confidence": "certain",
     "locator": {
      "address": "",
      "kind": "SHM",
      "multicast": false,
      "port": 7415
     },
     "measured": {
      "available": true,
      "bytes": 236.0,
      "bytes_total": 1756.0,
      "data_submessages": 0,
      "delivered": false,
      "delivered_samples": 0,
      "latency_s": null,
      "locators": [
       {
        "address": "",
        "bytes": 236.0,
        "kind": "SHM",
        "packets": 2,
        "port": 7415
       }
      ],
      "packets": 2,
      "packets_total": 14,
      "reliability": {
       "acknacks": 2,
       "gaps": 1,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 12324657.0,
      "transports": [
       "SHM"
      ]
     },
     "reader_guid": "01.0f.40.ec.37.00.26.ca.00.00.00.00|00.00.11.04",
     "reader_host": "d87e498ecc1f",
     "reader_node": "/talker",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "both-shm-locators",
      "measured-shm-traffic"
     ],
     "transport": "SHM",
     "warnings": [],
     "writer_guid": "01.0f.40.ec.35.00.5d.09.00.00.00.00|00.00.10.03",
     "writer_host": "d87e498ecc1f",
     "writer_node": "/listener"
    },
    {
     "confidence": "certain",
     "locator": {
      "address": "",
      "kind": "SHM",
      "multicast": false,
      "port": 7417
     },
     "measured": {
      "available": true,
      "bytes": 112.0,
      "bytes_total": 1264.0,
      "data_submessages": 0,
      "delivered": false,
      "delivered_samples": 0,
      "latency_s": null,
      "locators": [
       {
        "address": "",
        "bytes": 112.0,
        "kind": "SHM",
        "packets": 1,
        "port": 7417
       }
      ],
      "packets": 1,
      "packets_total": 10,
      "reliability": {
       "acknacks": 2,
       "gaps": 1,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 12324657.0,
      "transports": [
       "SHM"
      ]
     },
     "reader_guid": "01.0f.40.ec.38.00.58.91.00.00.00.00|00.00.11.04",
     "reader_host": "d87e498ecc1f",
     "reader_node": "/bounded_sub",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "both-shm-locators",
      "measured-shm-traffic"
     ],
     "transport": "SHM",
     "warnings": [],
     "writer_guid": "01.0f.40.ec.35.00.5d.09.00.00.00.00|00.00.10.03",
     "writer_host": "d87e498ecc1f",
     "writer_node": "/listener"
    },
    {
     "confidence": "certain",
     "locator": {
      "address": "",
      "kind": "SHM",
      "multicast": false,
      "port": 7419
     },
     "measured": {
      "available": true,
      "bytes": 0.0,
      "bytes_total": 0.0,
      "data_submessages": 0,
      "delivered": false,
      "delivered_samples": 0,
      "latency_s": null,
      "locators": [],
      "packets": 0,
      "packets_total": 0,
      "reliability": {
       "acknacks": 2,
       "gaps": 1,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 12324657.0,
      "transports": []
     },
     "reader_guid": "01.0f.40.ec.52.00.86.93.00.00.00.00|00.00.11.04",
     "reader_host": "d87e498ecc1f",
     "reader_node": "/bounded_pub",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "both-shm-locators"
     ],
     "transport": "SHM",
     "warnings": [
      "stats-writer-instance-limit-suspected"
     ],
     "writer_guid": "01.0f.40.ec.35.00.5d.09.00.00.00.00|00.00.10.03",
     "writer_host": "d87e498ecc1f",
     "writer_node": "/listener"
    },
    {
     "confidence": "certain",
     "locator": {
      "address": "127.0.0.1",
      "kind": "UDPv4",
      "multicast": false,
      "port": 7413
     },
     "measured": {
      "available": true,
      "bytes": 360.0,
      "bytes_total": 4064.0,
      "data_submessages": 0,
      "delivered": true,
      "delivered_samples": 1,
      "latency_s": {
       "last": 0.188924416,
       "max": 0.188924416,
       "mean": 0.188924416,
       "min": 0.188924416,
       "samples": 1
      },
      "locators": [
       {
        "address": "127.0.0.1",
        "bytes": 360.0,
        "kind": "UDPv4",
        "packets": 3,
        "port": 7413
       }
      ],
      "packets": 3,
      "packets_total": 20,
      "reliability": {
       "acknacks": 1,
       "gaps": 0,
       "heartbeats": 1,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 1800281.25,
      "transports": [
       "UDPv4"
      ]
     },
     "reader_guid": "01.0f.40.ec.35.00.5d.09.00.00.00.00|00.00.11.04",
     "reader_host": "d87e498ecc1f",
     "reader_node": "/listener",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "writer-no-shm-locator",
      "common-udpv4-locator",
      "measured-udpv4-traffic"
     ],
     "transport": "UDPv4",
     "warnings": [],
     "writer_guid": "01.0f.40.ec.36.00.87.0c.00.00.00.00|00.00.10.03",
     "writer_host": "d87e498ecc1f",
     "writer_node": "/listener_udp"
    },
    {
     "confidence": "certain",
     "locator": {
      "address": "127.0.0.1",
      "kind": "UDPv4",
      "multicast": false,
      "port": 7411
     },
     "measured": {
      "available": true,
      "bytes": 0.0,
      "bytes_total": 0.0,
      "data_submessages": 0,
      "delivered": true,
      "delivered_samples": 1,
      "latency_s": {
       "last": 5.9166e-05,
       "max": 5.9166e-05,
       "mean": 5.9166e-05,
       "min": 5.9166e-05,
       "samples": 1
      },
      "locators": [],
      "packets": 0,
      "packets_total": 0,
      "reliability": {
       "acknacks": 1,
       "gaps": 0,
       "heartbeats": 1,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 1800281.25,
      "transports": []
     },
     "reader_guid": "01.0f.40.ec.36.00.87.0c.00.00.00.00|00.00.11.04",
     "reader_host": "d87e498ecc1f",
     "reader_node": "/listener_udp",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "writer-no-shm-locator",
      "reader-no-shm-locator",
      "common-udpv4-locator"
     ],
     "transport": "UDPv4",
     "warnings": [
      "stats-writer-instance-limit-suspected"
     ],
     "writer_guid": "01.0f.40.ec.36.00.87.0c.00.00.00.00|00.00.10.03",
     "writer_host": "d87e498ecc1f",
     "writer_node": "/listener_udp"
    },
    {
     "confidence": "certain",
     "locator": {
      "address": "127.0.0.1",
      "kind": "UDPv4",
      "multicast": false,
      "port": 7415
     },
     "measured": {
      "available": true,
      "bytes": 360.0,
      "bytes_total": 2724.0,
      "data_submessages": 0,
      "delivered": true,
      "delivered_samples": 1,
      "latency_s": {
       "last": 0.188938832,
       "max": 0.188938832,
       "mean": 0.188938832,
       "min": 0.188938832,
       "samples": 1
      },
      "locators": [
       {
        "address": "127.0.0.1",
        "bytes": 360.0,
        "kind": "UDPv4",
        "packets": 3,
        "port": 7415
       }
      ],
      "packets": 3,
      "packets_total": 19,
      "reliability": {
       "acknacks": 2,
       "gaps": 0,
       "heartbeats": 1,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 1800281.25,
      "transports": [
       "UDPv4"
      ]
     },
     "reader_guid": "01.0f.40.ec.37.00.26.ca.00.00.00.00|00.00.11.04",
     "reader_host": "d87e498ecc1f",
     "reader_node": "/talker",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "writer-no-shm-locator",
      "common-udpv4-locator",
      "measured-udpv4-traffic"
     ],
     "transport": "UDPv4",
     "warnings": [],
     "writer_guid": "01.0f.40.ec.36.00.87.0c.00.00.00.00|00.00.10.03",
     "writer_host": "d87e498ecc1f",
     "writer_node": "/listener_udp"
    },
    {
     "confidence": "certain",
     "locator": {
      "address": "127.0.0.1",
      "kind": "UDPv4",
      "multicast": false,
      "port": 7417
     },
     "measured": {
      "available": true,
      "bytes": 236.0,
      "bytes_total": 1264.0,
      "data_submessages": 0,
      "delivered": false,
      "delivered_samples": 0,
      "latency_s": null,
      "locators": [
       {
        "address": "127.0.0.1",
        "bytes": 236.0,
        "kind": "UDPv4",
        "packets": 2,
        "port": 7417
       }
      ],
      "packets": 2,
      "packets_total": 10,
      "reliability": {
       "acknacks": 2,
       "gaps": 0,
       "heartbeats": 1,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 1800281.25,
      "transports": [
       "UDPv4"
      ]
     },
     "reader_guid": "01.0f.40.ec.38.00.58.91.00.00.00.00|00.00.11.04",
     "reader_host": "d87e498ecc1f",
     "reader_node": "/bounded_sub",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "writer-no-shm-locator",
      "common-udpv4-locator",
      "measured-udpv4-traffic"
     ],
     "transport": "UDPv4",
     "warnings": [],
     "writer_guid": "01.0f.40.ec.36.00.87.0c.00.00.00.00|00.00.10.03",
     "writer_host": "d87e498ecc1f",
     "writer_node": "/listener_udp"
    },
    {
     "confidence": "certain",
     "locator": {
      "address": "127.0.0.1",
      "kind": "UDPv4",
      "multicast": false,
      "port": 7419
     },
     "measured": {
      "available": true,
      "bytes": 0.0,
      "bytes_total": 0.0,
      "data_submessages": 0,
      "delivered": false,
      "delivered_samples": 0,
      "latency_s": null,
      "locators": [],
      "packets": 0,
      "packets_total": 0,
      "reliability": {
       "acknacks": 2,
       "gaps": 0,
       "heartbeats": 1,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 1800281.25,
      "transports": []
     },
     "reader_guid": "01.0f.40.ec.52.00.86.93.00.00.00.00|00.00.11.04",
     "reader_host": "d87e498ecc1f",
     "reader_node": "/bounded_pub",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "writer-no-shm-locator",
      "common-udpv4-locator"
     ],
     "transport": "UDPv4",
     "warnings": [
      "stats-writer-instance-limit-suspected"
     ],
     "writer_guid": "01.0f.40.ec.36.00.87.0c.00.00.00.00|00.00.10.03",
     "writer_host": "d87e498ecc1f",
     "writer_node": "/listener_udp"
    },
    {
     "confidence": "certain",
     "locator": {
      "address": "",
      "kind": "SHM",
      "multicast": false,
      "port": 7413
     },
     "measured": {
      "available": true,
      "bytes": 780.0,
      "bytes_total": 5320.0,
      "data_submessages": 0,
      "delivered": true,
      "delivered_samples": 1,
      "latency_s": {
       "last": 0.18788558400000002,
       "max": 0.18788558400000002,
       "mean": 0.18788558400000002,
       "min": 0.18788558400000002,
       "samples": 1
      },
      "locators": [
       {
        "address": "",
        "bytes": 780.0,
        "kind": "SHM",
        "packets": 6,
        "port": 7413
       }
      ],
      "packets": 6,
      "packets_total": 28,
      "reliability": {
       "acknacks": 1,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 4676923.0,
      "transports": [
       "SHM"
      ]
     },
     "reader_guid": "01.0f.40.ec.35.00.5d.09.00.00.00.00|00.00.11.04",
     "reader_host": "d87e498ecc1f",
     "reader_node": "/listener",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "both-shm-locators",
      "measured-shm-traffic"
     ],
     "transport": "SHM",
     "warnings": [],
     "writer_guid": "01.0f.40.ec.37.00.26.ca.00.00.00.00|00.00.10.03",
     "writer_host": "d87e498ecc1f",
     "writer_node": "/talker"
    },
    {
     "confidence": "certain",
     "locator": {
      "address": "127.0.0.1",
      "kind": "UDPv4",
      "multicast": false,
      "port": 7411
     },
     "measured": {
      "available": true,
      "bytes": 780.0,
      "bytes_total": 5576.0,
      "data_submessages": 0,
      "delivered": true,
      "delivered_samples": 1,
      "latency_s": {
       "last": 0.18786392000000002,
       "max": 0.18786392000000002,
       "mean": 0.18786392000000002,
       "min": 0.18786392000000002,
       "samples": 1
      },
      "locators": [
       {
        "address": "127.0.0.1",
        "bytes": 780.0,
        "kind": "UDPv4",
        "packets": 6,
        "port": 7411
       }
      ],
      "packets": 6,
      "packets_total": 30,
      "reliability": {
       "acknacks": 1,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 4676923.0,
      "transports": [
       "UDPv4"
      ]
     },
     "reader_guid": "01.0f.40.ec.36.00.87.0c.00.00.00.00|00.00.11.04",
     "reader_host": "d87e498ecc1f",
     "reader_node": "/listener_udp",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "reader-no-shm-locator",
      "common-udpv4-locator",
      "measured-udpv4-traffic"
     ],
     "transport": "UDPv4",
     "warnings": [],
     "writer_guid": "01.0f.40.ec.37.00.26.ca.00.00.00.00|00.00.10.03",
     "writer_host": "d87e498ecc1f",
     "writer_node": "/talker"
    },
    {
     "confidence": "certain",
     "locator": {
      "address": "",
      "kind": "SHM",
      "multicast": false,
      "port": 7415
     },
     "measured": {
      "available": true,
      "bytes": 0.0,
      "bytes_total": 0.0,
      "data_submessages": 0,
      "delivered": true,
      "delivered_samples": 1,
      "latency_s": {
       "last": 1.416e-06,
       "max": 1.416e-06,
       "mean": 1.416e-06,
       "min": 1.416e-06,
       "samples": 1
      },
      "locators": [],
      "packets": 0,
      "packets_total": 0,
      "reliability": {
       "acknacks": 2,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 4676923.0,
      "transports": []
     },
     "reader_guid": "01.0f.40.ec.37.00.26.ca.00.00.00.00|00.00.11.04",
     "reader_host": "d87e498ecc1f",
     "reader_node": "/talker",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "both-shm-locators"
     ],
     "transport": "SHM",
     "warnings": [
      "stats-writer-instance-limit-suspected"
     ],
     "writer_guid": "01.0f.40.ec.37.00.26.ca.00.00.00.00|00.00.10.03",
     "writer_host": "d87e498ecc1f",
     "writer_node": "/talker"
    },
    {
     "confidence": "certain",
     "locator": {
      "address": "",
      "kind": "SHM",
      "multicast": false,
      "port": 7417
     },
     "measured": {
      "available": true,
      "bytes": 124.0,
      "bytes_total": 1400.0,
      "data_submessages": 0,
      "delivered": false,
      "delivered_samples": 0,
      "latency_s": null,
      "locators": [
       {
        "address": "",
        "bytes": 124.0,
        "kind": "SHM",
        "packets": 1,
        "port": 7417
       }
      ],
      "packets": 1,
      "packets_total": 11,
      "reliability": {
       "acknacks": 2,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 4676923.0,
      "transports": [
       "SHM"
      ]
     },
     "reader_guid": "01.0f.40.ec.38.00.58.91.00.00.00.00|00.00.11.04",
     "reader_host": "d87e498ecc1f",
     "reader_node": "/bounded_sub",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "both-shm-locators",
      "measured-shm-traffic"
     ],
     "transport": "SHM",
     "warnings": [],
     "writer_guid": "01.0f.40.ec.37.00.26.ca.00.00.00.00|00.00.10.03",
     "writer_host": "d87e498ecc1f",
     "writer_node": "/talker"
    },
    {
     "confidence": "certain",
     "locator": {
      "address": "",
      "kind": "SHM",
      "multicast": false,
      "port": 7419
     },
     "measured": {
      "available": true,
      "bytes": 0.0,
      "bytes_total": 0.0,
      "data_submessages": 0,
      "delivered": false,
      "delivered_samples": 0,
      "latency_s": null,
      "locators": [],
      "packets": 0,
      "packets_total": 0,
      "reliability": {
       "acknacks": 2,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 4676923.0,
      "transports": []
     },
     "reader_guid": "01.0f.40.ec.52.00.86.93.00.00.00.00|00.00.11.04",
     "reader_host": "d87e498ecc1f",
     "reader_node": "/bounded_pub",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "both-shm-locators"
     ],
     "transport": "SHM",
     "warnings": [
      "stats-writer-instance-limit-suspected"
     ],
     "writer_guid": "01.0f.40.ec.37.00.26.ca.00.00.00.00|00.00.10.03",
     "writer_host": "d87e498ecc1f",
     "writer_node": "/talker"
    },
    {
     "confidence": "certain",
     "locator": {
      "address": "",
      "kind": "SHM",
      "multicast": false,
      "port": 7413
     },
     "measured": {
      "available": true,
      "bytes": 124.0,
      "bytes_total": 1128.0,
      "data_submessages": 0,
      "delivered": false,
      "delivered_samples": 0,
      "latency_s": null,
      "locators": [
       {
        "address": "",
        "bytes": 124.0,
        "kind": "SHM",
        "packets": 1,
        "port": 7413
       }
      ],
      "packets": 1,
      "packets_total": 9,
      "reliability": {
       "acknacks": 1,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 23852116.0,
      "transports": [
       "SHM"
      ]
     },
     "reader_guid": "01.0f.40.ec.35.00.5d.09.00.00.00.00|00.00.11.04",
     "reader_host": "d87e498ecc1f",
     "reader_node": "/listener",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "both-shm-locators",
      "measured-shm-traffic"
     ],
     "transport": "SHM",
     "warnings": [],
     "writer_guid": "01.0f.40.ec.38.00.58.91.00.00.00.00|00.00.10.03",
     "writer_host": "d87e498ecc1f",
     "writer_node": "/bounded_sub"
    },
    {
     "confidence": "certain",
     "locator": {
      "address": "127.0.0.1",
      "kind": "UDPv4",
      "multicast": false,
      "port": 7411
     },
     "measured": {
      "available": true,
      "bytes": 0.0,
      "bytes_total": 0.0,
      "data_submessages": 0,
      "delivered": false,
      "delivered_samples": 0,
      "latency_s": null,
      "locators": [],
      "packets": 0,
      "packets_total": 0,
      "reliability": {
       "acknacks": 1,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 23852116.0,
      "transports": []
     },
     "reader_guid": "01.0f.40.ec.36.00.87.0c.00.00.00.00|00.00.11.04",
     "reader_host": "d87e498ecc1f",
     "reader_node": "/listener_udp",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "reader-no-shm-locator",
      "common-udpv4-locator"
     ],
     "transport": "UDPv4",
     "warnings": [
      "stats-writer-instance-limit-suspected"
     ],
     "writer_guid": "01.0f.40.ec.38.00.58.91.00.00.00.00|00.00.10.03",
     "writer_host": "d87e498ecc1f",
     "writer_node": "/bounded_sub"
    },
    {
     "confidence": "certain",
     "locator": {
      "address": "",
      "kind": "SHM",
      "multicast": false,
      "port": 7415
     },
     "measured": {
      "available": true,
      "bytes": 0.0,
      "bytes_total": 0.0,
      "data_submessages": 0,
      "delivered": false,
      "delivered_samples": 0,
      "latency_s": null,
      "locators": [],
      "packets": 0,
      "packets_total": 0,
      "reliability": {
       "acknacks": 2,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 23852116.0,
      "transports": []
     },
     "reader_guid": "01.0f.40.ec.37.00.26.ca.00.00.00.00|00.00.11.04",
     "reader_host": "d87e498ecc1f",
     "reader_node": "/talker",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "both-shm-locators"
     ],
     "transport": "SHM",
     "warnings": [
      "stats-writer-instance-limit-suspected"
     ],
     "writer_guid": "01.0f.40.ec.38.00.58.91.00.00.00.00|00.00.10.03",
     "writer_host": "d87e498ecc1f",
     "writer_node": "/bounded_sub"
    },
    {
     "confidence": "certain",
     "locator": {
      "address": "",
      "kind": "SHM",
      "multicast": false,
      "port": 7417
     },
     "measured": {
      "available": true,
      "bytes": 0.0,
      "bytes_total": 0.0,
      "data_submessages": 0,
      "delivered": true,
      "delivered_samples": 1,
      "latency_s": {
       "last": 8.33e-07,
       "max": 8.33e-07,
       "mean": 8.33e-07,
       "min": 8.33e-07,
       "samples": 1
      },
      "locators": [],
      "packets": 0,
      "packets_total": 0,
      "reliability": {
       "acknacks": 2,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 23852116.0,
      "transports": []
     },
     "reader_guid": "01.0f.40.ec.38.00.58.91.00.00.00.00|00.00.11.04",
     "reader_host": "d87e498ecc1f",
     "reader_node": "/bounded_sub",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "both-shm-locators"
     ],
     "transport": "SHM",
     "warnings": [
      "stats-writer-instance-limit-suspected"
     ],
     "writer_guid": "01.0f.40.ec.38.00.58.91.00.00.00.00|00.00.10.03",
     "writer_host": "d87e498ecc1f",
     "writer_node": "/bounded_sub"
    },
    {
     "confidence": "certain",
     "locator": {
      "address": "",
      "kind": "SHM",
      "multicast": false,
      "port": 7419
     },
     "measured": {
      "available": true,
      "bytes": 248.0,
      "bytes_total": 1872.0,
      "data_submessages": 0,
      "delivered": false,
      "delivered_samples": 0,
      "latency_s": null,
      "locators": [
       {
        "address": "",
        "bytes": 248.0,
        "kind": "SHM",
        "packets": 2,
        "port": 7419
       }
      ],
      "packets": 2,
      "packets_total": 15,
      "reliability": {
       "acknacks": 2,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 23852116.0,
      "transports": [
       "SHM"
      ]
     },
     "reader_guid": "01.0f.40.ec.52.00.86.93.00.00.00.00|00.00.11.04",
     "reader_host": "d87e498ecc1f",
     "reader_node": "/bounded_pub",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "both-shm-locators",
      "measured-shm-traffic"
     ],
     "transport": "SHM",
     "warnings": [],
     "writer_guid": "01.0f.40.ec.38.00.58.91.00.00.00.00|00.00.10.03",
     "writer_host": "d87e498ecc1f",
     "writer_node": "/bounded_sub"
    },
    {
     "confidence": "certain",
     "locator": {
      "address": "",
      "kind": "SHM",
      "multicast": false,
      "port": 7413
     },
     "measured": {
      "available": true,
      "bytes": 248.0,
      "bytes_total": 1128.0,
      "data_submessages": 0,
      "delivered": false,
      "delivered_samples": 0,
      "latency_s": null,
      "locators": [
       {
        "address": "",
        "bytes": 248.0,
        "kind": "SHM",
        "packets": 2,
        "port": 7413
       }
      ],
      "packets": 2,
      "packets_total": 9,
      "reliability": {
       "acknacks": 1,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 23412350.0,
      "transports": [
       "SHM"
      ]
     },
     "reader_guid": "01.0f.40.ec.35.00.5d.09.00.00.00.00|00.00.11.04",
     "reader_host": "d87e498ecc1f",
     "reader_node": "/listener",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "both-shm-locators",
      "measured-shm-traffic"
     ],
     "transport": "SHM",
     "warnings": [],
     "writer_guid": "01.0f.40.ec.52.00.86.93.00.00.00.00|00.00.10.03",
     "writer_host": "d87e498ecc1f",
     "writer_node": "/bounded_pub"
    },
    {
     "confidence": "certain",
     "locator": {
      "address": "127.0.0.1",
      "kind": "UDPv4",
      "multicast": false,
      "port": 7411
     },
     "measured": {
      "available": true,
      "bytes": 248.0,
      "bytes_total": 1256.0,
      "data_submessages": 0,
      "delivered": false,
      "delivered_samples": 0,
      "latency_s": null,
      "locators": [
       {
        "address": "127.0.0.1",
        "bytes": 248.0,
        "kind": "UDPv4",
        "packets": 2,
        "port": 7411
       }
      ],
      "packets": 2,
      "packets_total": 10,
      "reliability": {
       "acknacks": 1,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 23412350.0,
      "transports": [
       "UDPv4"
      ]
     },
     "reader_guid": "01.0f.40.ec.36.00.87.0c.00.00.00.00|00.00.11.04",
     "reader_host": "d87e498ecc1f",
     "reader_node": "/listener_udp",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "reader-no-shm-locator",
      "common-udpv4-locator",
      "measured-udpv4-traffic"
     ],
     "transport": "UDPv4",
     "warnings": [],
     "writer_guid": "01.0f.40.ec.52.00.86.93.00.00.00.00|00.00.10.03",
     "writer_host": "d87e498ecc1f",
     "writer_node": "/bounded_pub"
    },
    {
     "confidence": "certain",
     "locator": {
      "address": "",
      "kind": "SHM",
      "multicast": false,
      "port": 7415
     },
     "measured": {
      "available": true,
      "bytes": 0.0,
      "bytes_total": 0.0,
      "data_submessages": 0,
      "delivered": false,
      "delivered_samples": 0,
      "latency_s": null,
      "locators": [],
      "packets": 0,
      "packets_total": 0,
      "reliability": {
       "acknacks": 2,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 23412350.0,
      "transports": []
     },
     "reader_guid": "01.0f.40.ec.37.00.26.ca.00.00.00.00|00.00.11.04",
     "reader_host": "d87e498ecc1f",
     "reader_node": "/talker",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "both-shm-locators"
     ],
     "transport": "SHM",
     "warnings": [
      "stats-writer-instance-limit-suspected"
     ],
     "writer_guid": "01.0f.40.ec.52.00.86.93.00.00.00.00|00.00.10.03",
     "writer_host": "d87e498ecc1f",
     "writer_node": "/bounded_pub"
    },
    {
     "confidence": "certain",
     "locator": {
      "address": "",
      "kind": "SHM",
      "multicast": false,
      "port": 7417
     },
     "measured": {
      "available": true,
      "bytes": 0.0,
      "bytes_total": 0.0,
      "data_submessages": 0,
      "delivered": false,
      "delivered_samples": 0,
      "latency_s": null,
      "locators": [],
      "packets": 0,
      "packets_total": 0,
      "reliability": {
       "acknacks": 2,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 23412350.0,
      "transports": []
     },
     "reader_guid": "01.0f.40.ec.38.00.58.91.00.00.00.00|00.00.11.04",
     "reader_host": "d87e498ecc1f",
     "reader_node": "/bounded_sub",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "both-shm-locators"
     ],
     "transport": "SHM",
     "warnings": [
      "stats-writer-instance-limit-suspected"
     ],
     "writer_guid": "01.0f.40.ec.52.00.86.93.00.00.00.00|00.00.10.03",
     "writer_host": "d87e498ecc1f",
     "writer_node": "/bounded_pub"
    },
    {
     "confidence": "certain",
     "locator": {
      "address": "",
      "kind": "SHM",
      "multicast": false,
      "port": 7419
     },
     "measured": {
      "available": true,
      "bytes": 0.0,
      "bytes_total": 0.0,
      "data_submessages": 0,
      "delivered": true,
      "delivered_samples": 1,
      "latency_s": {
       "last": 7.92e-07,
       "max": 7.92e-07,
       "mean": 7.92e-07,
       "min": 7.92e-07,
       "samples": 1
      },
      "locators": [],
      "packets": 0,
      "packets_total": 0,
      "reliability": {
       "acknacks": 2,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 23412350.0,
      "transports": []
     },
     "reader_guid": "01.0f.40.ec.52.00.86.93.00.00.00.00|00.00.11.04",
     "reader_host": "d87e498ecc1f",
     "reader_node": "/bounded_pub",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "both-shm-locators"
     ],
     "transport": "SHM",
     "warnings": [
      "stats-writer-instance-limit-suspected"
     ],
     "writer_guid": "01.0f.40.ec.52.00.86.93.00.00.00.00|00.00.10.03",
     "writer_host": "d87e498ecc1f",
     "writer_node": "/bounded_pub"
    }
   ],
   "readers": [
    {
     "datasharing_history_bytes": null,
     "dds_topic": "rt/parameter_events",
     "dds_type": "rcl_interfaces::msg::dds_::ParameterEvent_",
     "guid": "01.0f.40.ec.35.00.5d.09.00.00.00.00|00.00.11.04",
     "host": "d87e498ecc1f",
     "host_id": "010f40ec",
     "host_name": "d87e498ecc1f:14564170186182098944",
     "multicast_locators": [],
     "node": "/listener",
     "participant_guid_prefix": "01.0f.40.ec.35.00.5d.09.00.00.00.00",
     "process": "53",
     "qos": {
      "data_sharing": "OFF",
      "data_sharing_domain_ids": [],
      "deadline_s": null,
      "durability": "VOLATILE",
      "liveliness": "AUTOMATIC",
      "liveliness_lease_s": null,
      "ownership": "SHARED",
      "partitions": [],
      "reliability": "RELIABLE"
     },
     "ros_topic": "/parameter_events",
     "ros_type": "rcl_interfaces/msg/ParameterEvent",
     "unicast_locators": [
      {
       "address": "",
       "kind": "SHM",
       "port": 7413
      },
      {
       "address": "127.0.0.1",
       "kind": "UDPv4",
       "port": 7413
      }
     ]
    },
    {
     "datasharing_history_bytes": null,
     "dds_topic": "rt/parameter_events",
     "dds_type": "rcl_interfaces::msg::dds_::ParameterEvent_",
     "guid": "01.0f.40.ec.36.00.87.0c.00.00.00.00|00.00.11.04",
     "host": "d87e498ecc1f",
     "host_id": "010f40ec",
     "host_name": "d87e498ecc1f:14564170186182098944",
     "multicast_locators": [],
     "node": "/listener_udp",
     "participant_guid_prefix": "01.0f.40.ec.36.00.87.0c.00.00.00.00",
     "process": "54",
     "qos": {
      "data_sharing": "OFF",
      "data_sharing_domain_ids": [],
      "deadline_s": null,
      "durability": "VOLATILE",
      "liveliness": "AUTOMATIC",
      "liveliness_lease_s": null,
      "ownership": "SHARED",
      "partitions": [],
      "reliability": "RELIABLE"
     },
     "ros_topic": "/parameter_events",
     "ros_type": "rcl_interfaces/msg/ParameterEvent",
     "unicast_locators": [
      {
       "address": "127.0.0.1",
       "kind": "UDPv4",
       "port": 7411
      }
     ]
    },
    {
     "datasharing_history_bytes": null,
     "dds_topic": "rt/parameter_events",
     "dds_type": "rcl_interfaces::msg::dds_::ParameterEvent_",
     "guid": "01.0f.40.ec.37.00.26.ca.00.00.00.00|00.00.11.04",
     "host": "d87e498ecc1f",
     "host_id": "010f40ec",
     "host_name": "d87e498ecc1f:14564170186182098944",
     "multicast_locators": [],
     "node": "/talker",
     "participant_guid_prefix": "01.0f.40.ec.37.00.26.ca.00.00.00.00",
     "process": "55",
     "qos": {
      "data_sharing": "OFF",
      "data_sharing_domain_ids": [],
      "deadline_s": null,
      "durability": "VOLATILE",
      "liveliness": "AUTOMATIC",
      "liveliness_lease_s": null,
      "ownership": "SHARED",
      "partitions": [],
      "reliability": "RELIABLE"
     },
     "ros_topic": "/parameter_events",
     "ros_type": "rcl_interfaces/msg/ParameterEvent",
     "unicast_locators": [
      {
       "address": "",
       "kind": "SHM",
       "port": 7415
      },
      {
       "address": "127.0.0.1",
       "kind": "UDPv4",
       "port": 7415
      }
     ]
    },
    {
     "datasharing_history_bytes": null,
     "dds_topic": "rt/parameter_events",
     "dds_type": "rcl_interfaces::msg::dds_::ParameterEvent_",
     "guid": "01.0f.40.ec.38.00.58.91.00.00.00.00|00.00.11.04",
     "host": "d87e498ecc1f",
     "host_id": "010f40ec",
     "host_name": "d87e498ecc1f:14564170186182098944",
     "multicast_locators": [],
     "node": "/bounded_sub",
     "participant_guid_prefix": "01.0f.40.ec.38.00.58.91.00.00.00.00",
     "process": "56",
     "qos": {
      "data_sharing": "OFF",
      "data_sharing_domain_ids": [],
      "deadline_s": null,
      "durability": "VOLATILE",
      "liveliness": "AUTOMATIC",
      "liveliness_lease_s": null,
      "ownership": "SHARED",
      "partitions": [],
      "reliability": "RELIABLE"
     },
     "ros_topic": "/parameter_events",
     "ros_type": "rcl_interfaces/msg/ParameterEvent",
     "unicast_locators": [
      {
       "address": "",
       "kind": "SHM",
       "port": 7417
      },
      {
       "address": "127.0.0.1",
       "kind": "UDPv4",
       "port": 7417
      }
     ]
    },
    {
     "datasharing_history_bytes": null,
     "dds_topic": "rt/parameter_events",
     "dds_type": "rcl_interfaces::msg::dds_::ParameterEvent_",
     "guid": "01.0f.40.ec.52.00.86.93.00.00.00.00|00.00.11.04",
     "host": "d87e498ecc1f",
     "host_id": "010f40ec",
     "host_name": "d87e498ecc1f:14564170186182098944",
     "multicast_locators": [],
     "node": "/bounded_pub",
     "participant_guid_prefix": "01.0f.40.ec.52.00.86.93.00.00.00.00",
     "process": "82",
     "qos": {
      "data_sharing": "OFF",
      "data_sharing_domain_ids": [],
      "deadline_s": null,
      "durability": "VOLATILE",
      "liveliness": "AUTOMATIC",
      "liveliness_lease_s": null,
      "ownership": "SHARED",
      "partitions": [],
      "reliability": "RELIABLE"
     },
     "ros_topic": "/parameter_events",
     "ros_type": "rcl_interfaces/msg/ParameterEvent",
     "unicast_locators": [
      {
       "address": "",
       "kind": "SHM",
       "port": 7419
      },
      {
       "address": "127.0.0.1",
       "kind": "UDPv4",
       "port": 7419
      }
     ]
    }
   ],
   "resent_datas": 0,
   "throughput_bytes_per_s": 66066327.25,
   "topic": "/parameter_events",
   "type": "rcl_interfaces/msg/ParameterEvent",
   "unmatched_reasons": [],
   "writers": [
    {
     "datasharing_history_bytes": null,
     "dds_topic": "rt/parameter_events",
     "dds_type": "rcl_interfaces::msg::dds_::ParameterEvent_",
     "guid": "01.0f.40.ec.35.00.5d.09.00.00.00.00|00.00.10.03",
     "host": "d87e498ecc1f",
     "host_id": "010f40ec",
     "host_name": "d87e498ecc1f:14564170186182098944",
     "multicast_locators": [],
     "node": "/listener",
     "participant_guid_prefix": "01.0f.40.ec.35.00.5d.09.00.00.00.00",
     "process": "53",
     "qos": {
      "data_sharing": "OFF",
      "data_sharing_domain_ids": [],
      "deadline_s": null,
      "durability": "VOLATILE",
      "liveliness": "AUTOMATIC",
      "liveliness_lease_s": null,
      "ownership": "SHARED",
      "partitions": [],
      "reliability": "RELIABLE"
     },
     "ros_topic": "/parameter_events",
     "ros_type": "rcl_interfaces/msg/ParameterEvent",
     "unicast_locators": [
      {
       "address": "",
       "kind": "SHM",
       "port": 7413
      },
      {
       "address": "127.0.0.1",
       "kind": "UDPv4",
       "port": 7413
      }
     ]
    },
    {
     "datasharing_history_bytes": null,
     "dds_topic": "rt/parameter_events",
     "dds_type": "rcl_interfaces::msg::dds_::ParameterEvent_",
     "guid": "01.0f.40.ec.36.00.87.0c.00.00.00.00|00.00.10.03",
     "host": "d87e498ecc1f",
     "host_id": "010f40ec",
     "host_name": "d87e498ecc1f:14564170186182098944",
     "multicast_locators": [],
     "node": "/listener_udp",
     "participant_guid_prefix": "01.0f.40.ec.36.00.87.0c.00.00.00.00",
     "process": "54",
     "qos": {
      "data_sharing": "OFF",
      "data_sharing_domain_ids": [],
      "deadline_s": null,
      "durability": "VOLATILE",
      "liveliness": "AUTOMATIC",
      "liveliness_lease_s": null,
      "ownership": "SHARED",
      "partitions": [],
      "reliability": "RELIABLE"
     },
     "ros_topic": "/parameter_events",
     "ros_type": "rcl_interfaces/msg/ParameterEvent",
     "unicast_locators": [
      {
       "address": "127.0.0.1",
       "kind": "UDPv4",
       "port": 7411
      }
     ]
    },
    {
     "datasharing_history_bytes": null,
     "dds_topic": "rt/parameter_events",
     "dds_type": "rcl_interfaces::msg::dds_::ParameterEvent_",
     "guid": "01.0f.40.ec.37.00.26.ca.00.00.00.00|00.00.10.03",
     "host": "d87e498ecc1f",
     "host_id": "010f40ec",
     "host_name": "d87e498ecc1f:14564170186182098944",
     "multicast_locators": [],
     "node": "/talker",
     "participant_guid_prefix": "01.0f.40.ec.37.00.26.ca.00.00.00.00",
     "process": "55",
     "qos": {
      "data_sharing": "OFF",
      "data_sharing_domain_ids": [],
      "deadline_s": null,
      "durability": "VOLATILE",
      "liveliness": "AUTOMATIC",
      "liveliness_lease_s": null,
      "ownership": "SHARED",
      "partitions": [],
      "reliability": "RELIABLE"
     },
     "ros_topic": "/parameter_events",
     "ros_type": "rcl_interfaces/msg/ParameterEvent",
     "unicast_locators": [
      {
       "address": "",
       "kind": "SHM",
       "port": 7415
      },
      {
       "address": "127.0.0.1",
       "kind": "UDPv4",
       "port": 7415
      }
     ]
    },
    {
     "datasharing_history_bytes": null,
     "dds_topic": "rt/parameter_events",
     "dds_type": "rcl_interfaces::msg::dds_::ParameterEvent_",
     "guid": "01.0f.40.ec.38.00.58.91.00.00.00.00|00.00.10.03",
     "host": "d87e498ecc1f",
     "host_id": "010f40ec",
     "host_name": "d87e498ecc1f:14564170186182098944",
     "multicast_locators": [],
     "node": "/bounded_sub",
     "participant_guid_prefix": "01.0f.40.ec.38.00.58.91.00.00.00.00",
     "process": "56",
     "qos": {
      "data_sharing": "OFF",
      "data_sharing_domain_ids": [],
      "deadline_s": null,
      "durability": "VOLATILE",
      "liveliness": "AUTOMATIC",
      "liveliness_lease_s": null,
      "ownership": "SHARED",
      "partitions": [],
      "reliability": "RELIABLE"
     },
     "ros_topic": "/parameter_events",
     "ros_type": "rcl_interfaces/msg/ParameterEvent",
     "unicast_locators": [
      {
       "address": "",
       "kind": "SHM",
       "port": 7417
      },
      {
       "address": "127.0.0.1",
       "kind": "UDPv4",
       "port": 7417
      }
     ]
    },
    {
     "datasharing_history_bytes": null,
     "dds_topic": "rt/parameter_events",
     "dds_type": "rcl_interfaces::msg::dds_::ParameterEvent_",
     "guid": "01.0f.40.ec.52.00.86.93.00.00.00.00|00.00.10.03",
     "host": "d87e498ecc1f",
     "host_id": "010f40ec",
     "host_name": "d87e498ecc1f:14564170186182098944",
     "multicast_locators": [],
     "node": "/bounded_pub",
     "participant_guid_prefix": "01.0f.40.ec.52.00.86.93.00.00.00.00",
     "process": "82",
     "qos": {
      "data_sharing": "OFF",
      "data_sharing_domain_ids": [],
      "deadline_s": null,
      "durability": "VOLATILE",
      "liveliness": "AUTOMATIC",
      "liveliness_lease_s": null,
      "ownership": "SHARED",
      "partitions": [],
      "reliability": "RELIABLE"
     },
     "ros_topic": "/parameter_events",
     "ros_type": "rcl_interfaces/msg/ParameterEvent",
     "unicast_locators": [
      {
       "address": "",
       "kind": "SHM",
       "port": 7419
      },
      {
       "address": "127.0.0.1",
       "kind": "UDPv4",
       "port": 7419
      }
     ]
    }
   ]
  },
  {
   "dds_topic": "rt/rosout",
   "is_ros_topic": true,
   "latency_s": null,
   "lost_packets": null,
   "pairs": [],
   "readers": [],
   "resent_datas": null,
   "throughput_bytes_per_s": 412.8910312652588,
   "topic": "/rosout",
   "type": "rcl_interfaces/msg/Log",
   "unmatched_reasons": [
    "no-matching-reader"
   ],
   "writers": [
    {
     "datasharing_history_bytes": null,
     "dds_topic": "rt/rosout",
     "dds_type": "rcl_interfaces::msg::dds_::Log_",
     "guid": "01.0f.40.ec.35.00.5d.09.00.00.00.00|00.00.03.03",
     "host": "d87e498ecc1f",
     "host_id": "010f40ec",
     "host_name": "d87e498ecc1f:14564170186182098944",
     "multicast_locators": [],
     "node": "/listener",
     "participant_guid_prefix": "01.0f.40.ec.35.00.5d.09.00.00.00.00",
     "process": "53",
     "qos": {
      "data_sharing": "OFF",
      "data_sharing_domain_ids": [],
      "deadline_s": null,
      "durability": "TRANSIENT_LOCAL",
      "liveliness": "AUTOMATIC",
      "liveliness_lease_s": null,
      "ownership": "SHARED",
      "partitions": [],
      "reliability": "RELIABLE"
     },
     "ros_topic": "/rosout",
     "ros_type": "rcl_interfaces/msg/Log",
     "unicast_locators": [
      {
       "address": "",
       "kind": "SHM",
       "port": 7413
      },
      {
       "address": "127.0.0.1",
       "kind": "UDPv4",
       "port": 7413
      }
     ]
    },
    {
     "datasharing_history_bytes": null,
     "dds_topic": "rt/rosout",
     "dds_type": "rcl_interfaces::msg::dds_::Log_",
     "guid": "01.0f.40.ec.36.00.87.0c.00.00.00.00|00.00.03.03",
     "host": "d87e498ecc1f",
     "host_id": "010f40ec",
     "host_name": "d87e498ecc1f:14564170186182098944",
     "multicast_locators": [],
     "node": "/listener_udp",
     "participant_guid_prefix": "01.0f.40.ec.36.00.87.0c.00.00.00.00",
     "process": "54",
     "qos": {
      "data_sharing": "OFF",
      "data_sharing_domain_ids": [],
      "deadline_s": null,
      "durability": "TRANSIENT_LOCAL",
      "liveliness": "AUTOMATIC",
      "liveliness_lease_s": null,
      "ownership": "SHARED",
      "partitions": [],
      "reliability": "RELIABLE"
     },
     "ros_topic": "/rosout",
     "ros_type": "rcl_interfaces/msg/Log",
     "unicast_locators": [
      {
       "address": "127.0.0.1",
       "kind": "UDPv4",
       "port": 7411
      }
     ]
    },
    {
     "datasharing_history_bytes": null,
     "dds_topic": "rt/rosout",
     "dds_type": "rcl_interfaces::msg::dds_::Log_",
     "guid": "01.0f.40.ec.37.00.26.ca.00.00.00.00|00.00.03.03",
     "host": "d87e498ecc1f",
     "host_id": "010f40ec",
     "host_name": "d87e498ecc1f:14564170186182098944",
     "multicast_locators": [],
     "node": "/talker",
     "participant_guid_prefix": "01.0f.40.ec.37.00.26.ca.00.00.00.00",
     "process": "55",
     "qos": {
      "data_sharing": "OFF",
      "data_sharing_domain_ids": [],
      "deadline_s": null,
      "durability": "TRANSIENT_LOCAL",
      "liveliness": "AUTOMATIC",
      "liveliness_lease_s": null,
      "ownership": "SHARED",
      "partitions": [],
      "reliability": "RELIABLE"
     },
     "ros_topic": "/rosout",
     "ros_type": "rcl_interfaces/msg/Log",
     "unicast_locators": [
      {
       "address": "",
       "kind": "SHM",
       "port": 7415
      },
      {
       "address": "127.0.0.1",
       "kind": "UDPv4",
       "port": 7415
      }
     ]
    },
    {
     "datasharing_history_bytes": null,
     "dds_topic": "rt/rosout",
     "dds_type": "rcl_interfaces::msg::dds_::Log_",
     "guid": "01.0f.40.ec.38.00.58.91.00.00.00.00|00.00.03.03",
     "host": "d87e498ecc1f",
     "host_id": "010f40ec",
     "host_name": "d87e498ecc1f:14564170186182098944",
     "multicast_locators": [],
     "node": "/bounded_sub",
     "participant_guid_prefix": "01.0f.40.ec.38.00.58.91.00.00.00.00",
     "process": "56",
     "qos": {
      "data_sharing": "OFF",
      "data_sharing_domain_ids": [],
      "deadline_s": null,
      "durability": "TRANSIENT_LOCAL",
      "liveliness": "AUTOMATIC",
      "liveliness_lease_s": null,
      "ownership": "SHARED",
      "partitions": [],
      "reliability": "RELIABLE"
     },
     "ros_topic": "/rosout",
     "ros_type": "rcl_interfaces/msg/Log",
     "unicast_locators": [
      {
       "address": "",
       "kind": "SHM",
       "port": 7417
      },
      {
       "address": "127.0.0.1",
       "kind": "UDPv4",
       "port": 7417
      }
     ]
    },
    {
     "datasharing_history_bytes": null,
     "dds_topic": "rt/rosout",
     "dds_type": "rcl_interfaces::msg::dds_::Log_",
     "guid": "01.0f.40.ec.52.00.86.93.00.00.00.00|00.00.03.03",
     "host": "d87e498ecc1f",
     "host_id": "010f40ec",
     "host_name": "d87e498ecc1f:14564170186182098944",
     "multicast_locators": [],
     "node": "/bounded_pub",
     "participant_guid_prefix": "01.0f.40.ec.52.00.86.93.00.00.00.00",
     "process": "82",
     "qos": {
      "data_sharing": "OFF",
      "data_sharing_domain_ids": [],
      "deadline_s": null,
      "durability": "TRANSIENT_LOCAL",
      "liveliness": "AUTOMATIC",
      "liveliness_lease_s": null,
      "ownership": "SHARED",
      "partitions": [],
      "reliability": "RELIABLE"
     },
     "ros_topic": "/rosout",
     "ros_type": "rcl_interfaces/msg/Log",
     "unicast_locators": [
      {
       "address": "",
       "kind": "SHM",
       "port": 7419
      },
      {
       "address": "127.0.0.1",
       "kind": "UDPv4",
       "port": 7419
      }
     ]
    }
   ]
  }
 ]
};

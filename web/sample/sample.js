// Generated from sample.json so that the viewer can show it when opened from file://
window.TRANSPORT_VIZ_SAMPLE = {
 "domain": 0,
 "local_host_id": "010f40ec",
 "observation_seconds": 6.029612795,
 "observed_at": "2026-09-09T14:20:36Z",
 "reason_code_descriptions": {
  "both-shm-locators": "Both endpoints announce a SHM locator. On the same host Fast DDS then uses the shared memory transport exclusively for user data (discovery still goes over UDP).",
  "common-udpv4-locator": "The reader announces a UDPv4 locator and the writer speaks UDPv4.",
  "datasharing-disabled-writer": "The writer announces data-sharing OFF (explicitly disabled, or AUTO resolved to OFF because the type is unbounded / the history memory policy is not preallocated).",
  "measured-shm-traffic": "Statistics show RTPS packets from the writer's participant to the reader's SHM locator.",
  "measured-udpv4-traffic": "Statistics show RTPS packets from the writer's participant to the reader's UDPv4 locator.",
  "no-matching-reader": "No subscription was discovered for this topic.",
  "reader-no-shm-locator": "The reader's participant announces no SHM locator: SHM transport is not instantiated on its side (e.g. FASTDDS_BUILTIN_TRANSPORTS=UDPv4, or an XML profile without SHM).",
  "same-host-guid": "Writer and reader GUID prefixes share the same first 4 bytes, which is how Fast DDS decides both participants run on the same host.",
  "stats-writer-instance-limit-suspected": "The writer's participant reports traffic to 10 or more locators but none to this reader. The Fast DDS statistics DataWriter keeps the default resource limit of 10 instances (one per destination locator), so counters for further locators are never published. Raise it with a data_writer XML profile named after the alias used in FASTDDS_STATISTICS (RTPS_SENT_TOPIC) whose <resourceLimitsQos> sets max_instances to 0; the package ships config/statistics.xml for this.",
  "writer-no-shm-locator": "The writer's participant announces no SHM locator: SHM transport is not instantiated on its side (e.g. FASTDDS_BUILTIN_TRANSPORTS=UDPv4, or an XML profile without SHM)."
 },
 "schema_version": 1,
 "shm": {
  "available": true,
  "checked_ports": [
   7000,
   7001,
   7002,
   7003,
   7411,
   7415,
   7417,
   7419
  ],
  "datasharing_histories": 0,
  "datasharing_unmatched": 0,
  "fastdds_bytes": 3873376,
  "free_bytes": 4155928576,
  "missing_ports": [],
  "nodes_visible": true,
  "other_host_participants": 0,
  "path": "/dev/shm",
  "ports": 11,
  "segments": 6,
  "stale_ports": 0,
  "stale_segments": 0,
  "total_bytes": 4159885312,
  "used_bytes": 3956736,
  "warnings": []
 },
 "stats": {
  "data_count": {
   "01.0f.40.ec.ae.00.cc.68.00.00.00.00|00.00.01.03": {
    "first": 5,
    "last": 5,
    "samples": 1
   },
   "01.0f.40.ec.b1.00.02.4d.00.00.00.00|00.00.01.03": {
    "first": 5,
    "last": 5,
    "samples": 1
   },
   "01.0f.40.ec.b8.00.a3.cd.00.00.00.00|00.00.01.03": {
    "first": 5,
    "last": 5,
    "samples": 1
   },
   "01.0f.40.ec.b8.00.a3.cd.00.00.00.00|00.00.14.03": {
    "first": 64,
    "last": 119,
    "samples": 56
   },
   "01.0f.40.ec.bf.00.9b.65.00.00.00.00|00.00.01.03": {
    "first": 5,
    "last": 5,
    "samples": 1
   },
   "01.0f.40.ec.c0.00.a2.17.00.00.00.00|00.00.01.03": {
    "first": 5,
    "last": 5,
    "samples": 1
   },
   "01.0f.40.ec.c0.00.a2.17.00.00.00.00|00.00.14.03": {
    "first": 12,
    "last": 24,
    "samples": 7
   }
  },
  "enabled": true,
  "lost": [],
  "participants_with_stats": [
   "01.0f.40.ec.03.01.63.25.00.00.00.00",
   "01.0f.40.ec.ae.00.cc.68.00.00.00.00",
   "01.0f.40.ec.b1.00.02.4d.00.00.00.00",
   "01.0f.40.ec.b8.00.a3.cd.00.00.00.00",
   "01.0f.40.ec.bf.00.9b.65.00.00.00.00",
   "01.0f.40.ec.c0.00.a2.17.00.00.00.00"
  ],
  "physical": {
   "01.0f.40.ec.ae.00.cc.68.00.00.00.00": {
    "host": "c4d031b385df:15082593216380207104",
    "process": "174",
    "user": "root"
   },
   "01.0f.40.ec.b1.00.02.4d.00.00.00.00": {
    "host": "c4d031b385df:15082593216380207104",
    "process": "177",
    "user": "root"
   },
   "01.0f.40.ec.b8.00.a3.cd.00.00.00.00": {
    "host": "c4d031b385df:15082593216380207104",
    "process": "184",
    "user": "root"
   },
   "01.0f.40.ec.bf.00.9b.65.00.00.00.00": {
    "host": "c4d031b385df:15082593216380207104",
    "process": "191",
    "user": "root"
   },
   "01.0f.40.ec.c0.00.a2.17.00.00.00.00": {
    "host": "c4d031b385df:15082593216380207104",
    "process": "192",
    "user": "root"
   }
  },
  "samples": 545,
  "statistics_writers": [
   {
    "participant_guid_prefix": "01.0f.40.ec.ae.00.cc.68.00.00.00.00",
    "topic": "_fastdds_statistics_acknack_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.ae.00.cc.68.00.00.00.00",
    "topic": "_fastdds_statistics_data_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.ae.00.cc.68.00.00.00.00",
    "topic": "_fastdds_statistics_gap_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.ae.00.cc.68.00.00.00.00",
    "topic": "_fastdds_statistics_heartbeat_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.ae.00.cc.68.00.00.00.00",
    "topic": "_fastdds_statistics_history2history_latency"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.ae.00.cc.68.00.00.00.00",
    "topic": "_fastdds_statistics_nackfrag_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.ae.00.cc.68.00.00.00.00",
    "topic": "_fastdds_statistics_physical_data"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.ae.00.cc.68.00.00.00.00",
    "topic": "_fastdds_statistics_publication_throughput"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.ae.00.cc.68.00.00.00.00",
    "topic": "_fastdds_statistics_resent_datas"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.ae.00.cc.68.00.00.00.00",
    "topic": "_fastdds_statistics_rtps_lost"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.ae.00.cc.68.00.00.00.00",
    "topic": "_fastdds_statistics_rtps_sent"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.b1.00.02.4d.00.00.00.00",
    "topic": "_fastdds_statistics_acknack_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.b1.00.02.4d.00.00.00.00",
    "topic": "_fastdds_statistics_data_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.b1.00.02.4d.00.00.00.00",
    "topic": "_fastdds_statistics_gap_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.b1.00.02.4d.00.00.00.00",
    "topic": "_fastdds_statistics_heartbeat_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.b1.00.02.4d.00.00.00.00",
    "topic": "_fastdds_statistics_history2history_latency"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.b1.00.02.4d.00.00.00.00",
    "topic": "_fastdds_statistics_nackfrag_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.b1.00.02.4d.00.00.00.00",
    "topic": "_fastdds_statistics_physical_data"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.b1.00.02.4d.00.00.00.00",
    "topic": "_fastdds_statistics_publication_throughput"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.b1.00.02.4d.00.00.00.00",
    "topic": "_fastdds_statistics_resent_datas"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.b1.00.02.4d.00.00.00.00",
    "topic": "_fastdds_statistics_rtps_lost"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.b1.00.02.4d.00.00.00.00",
    "topic": "_fastdds_statistics_rtps_sent"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00",
    "topic": "_fastdds_statistics_acknack_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00",
    "topic": "_fastdds_statistics_data_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00",
    "topic": "_fastdds_statistics_gap_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00",
    "topic": "_fastdds_statistics_heartbeat_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00",
    "topic": "_fastdds_statistics_history2history_latency"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00",
    "topic": "_fastdds_statistics_nackfrag_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00",
    "topic": "_fastdds_statistics_physical_data"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00",
    "topic": "_fastdds_statistics_publication_throughput"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00",
    "topic": "_fastdds_statistics_resent_datas"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00",
    "topic": "_fastdds_statistics_rtps_lost"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00",
    "topic": "_fastdds_statistics_rtps_sent"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.bf.00.9b.65.00.00.00.00",
    "topic": "_fastdds_statistics_acknack_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.bf.00.9b.65.00.00.00.00",
    "topic": "_fastdds_statistics_data_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.bf.00.9b.65.00.00.00.00",
    "topic": "_fastdds_statistics_gap_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.bf.00.9b.65.00.00.00.00",
    "topic": "_fastdds_statistics_heartbeat_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.bf.00.9b.65.00.00.00.00",
    "topic": "_fastdds_statistics_history2history_latency"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.bf.00.9b.65.00.00.00.00",
    "topic": "_fastdds_statistics_nackfrag_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.bf.00.9b.65.00.00.00.00",
    "topic": "_fastdds_statistics_physical_data"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.bf.00.9b.65.00.00.00.00",
    "topic": "_fastdds_statistics_publication_throughput"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.bf.00.9b.65.00.00.00.00",
    "topic": "_fastdds_statistics_resent_datas"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.bf.00.9b.65.00.00.00.00",
    "topic": "_fastdds_statistics_rtps_lost"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.bf.00.9b.65.00.00.00.00",
    "topic": "_fastdds_statistics_rtps_sent"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.c0.00.a2.17.00.00.00.00",
    "topic": "_fastdds_statistics_acknack_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.c0.00.a2.17.00.00.00.00",
    "topic": "_fastdds_statistics_data_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.c0.00.a2.17.00.00.00.00",
    "topic": "_fastdds_statistics_gap_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.c0.00.a2.17.00.00.00.00",
    "topic": "_fastdds_statistics_heartbeat_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.c0.00.a2.17.00.00.00.00",
    "topic": "_fastdds_statistics_history2history_latency"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.c0.00.a2.17.00.00.00.00",
    "topic": "_fastdds_statistics_nackfrag_count"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.c0.00.a2.17.00.00.00.00",
    "topic": "_fastdds_statistics_physical_data"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.c0.00.a2.17.00.00.00.00",
    "topic": "_fastdds_statistics_publication_throughput"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.c0.00.a2.17.00.00.00.00",
    "topic": "_fastdds_statistics_resent_datas"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.c0.00.a2.17.00.00.00.00",
    "topic": "_fastdds_statistics_rtps_lost"
   },
   {
    "participant_guid_prefix": "01.0f.40.ec.c0.00.a2.17.00.00.00.00",
    "topic": "_fastdds_statistics_rtps_sent"
   }
  ],
  "throughput": {
   "01.0f.40.ec.ae.00.cc.68.00.00.00.00|00.00.01.03": {
    "last": 909052.4375,
    "mean": 909052.4375,
    "samples": 1
   },
   "01.0f.40.ec.ae.00.cc.68.00.00.00.00|00.00.03.03": {
    "last": 115.54203796386719,
    "mean": 115.98549979073661,
    "samples": 7
   },
   "01.0f.40.ec.ae.00.cc.68.00.00.00.00|00.00.10.03": {
    "last": 22109090.0,
    "mean": 22109090.0,
    "samples": 1
   },
   "01.0f.40.ec.b1.00.02.4d.00.00.00.00|00.00.01.03": {
    "last": 1015936.8125,
    "mean": 1015936.8125,
    "samples": 1
   },
   "01.0f.40.ec.b1.00.02.4d.00.00.00.00|00.00.03.03": {
    "last": 119.47822570800781,
    "mean": 119.98543112618583,
    "samples": 7
   },
   "01.0f.40.ec.b1.00.02.4d.00.00.00.00|00.00.10.03": {
    "last": 21941854.0,
    "mean": 21941854.0,
    "samples": 1
   },
   "01.0f.40.ec.b8.00.a3.cd.00.00.00.00|00.00.01.03": {
    "last": 1043610.125,
    "mean": 1043610.125,
    "samples": 1
   },
   "01.0f.40.ec.b8.00.a3.cd.00.00.00.00|00.00.10.03": {
    "last": 24457352.0,
    "mean": 24457352.0,
    "samples": 1
   },
   "01.0f.40.ec.b8.00.a3.cd.00.00.00.00|00.00.14.03": {
    "last": 82.10556030273438,
    "mean": 80.1455409186227,
    "samples": 56
   },
   "01.0f.40.ec.bf.00.9b.65.00.00.00.00|00.00.01.03": {
    "last": 606033.9375,
    "mean": 606033.9375,
    "samples": 1
   },
   "01.0f.40.ec.bf.00.9b.65.00.00.00.00|00.00.03.03": {
    "last": 65.97801971435547,
    "mean": 64.94523239135742,
    "samples": 3
   },
   "01.0f.40.ec.bf.00.9b.65.00.00.00.00|00.00.10.03": {
    "last": 26481296.0,
    "mean": 26481296.0,
    "samples": 1
   },
   "01.0f.40.ec.c0.00.a2.17.00.00.00.00|00.00.01.03": {
    "last": 846209.0,
    "mean": 846209.0,
    "samples": 1
   },
   "01.0f.40.ec.c0.00.a2.17.00.00.00.00|00.00.03.03": {
    "last": 111.51862335205078,
    "mean": 111.99365779331752,
    "samples": 7
   },
   "01.0f.40.ec.c0.00.a2.17.00.00.00.00|00.00.10.03": {
    "last": 18516262.0,
    "mean": 18516262.0,
    "samples": 1
   },
   "01.0f.40.ec.c0.00.a2.17.00.00.00.00|00.00.14.03": {
    "last": 23.90005874633789,
    "mean": 23.427134105137416,
    "samples": 7
   }
  },
  "traffic": [
   {
    "bytes": 1052.0,
    "bytes_first": 1052.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7001
    },
    "packets": 6,
    "packets_first": 6,
    "src_participant_guid_prefix": "01.0f.40.ec.ae.00.cc.68.00.00.00.00"
   },
   {
    "bytes": 26292.0,
    "bytes_first": 25784.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7412
    },
    "packets": 57,
    "packets_first": 56,
    "src_participant_guid_prefix": "01.0f.40.ec.ae.00.cc.68.00.00.00.00"
   },
   {
    "bytes": 752.0,
    "bytes_first": 752.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7413
    },
    "packets": 6,
    "packets_first": 6,
    "src_participant_guid_prefix": "01.0f.40.ec.ae.00.cc.68.00.00.00.00"
   },
   {
    "bytes": 24848.0,
    "bytes_first": 24340.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7414
    },
    "packets": 42,
    "packets_first": 41,
    "src_participant_guid_prefix": "01.0f.40.ec.ae.00.cc.68.00.00.00.00"
   },
   {
    "bytes": 25672.0,
    "bytes_first": 25164.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7416
    },
    "packets": 50,
    "packets_first": 49,
    "src_participant_guid_prefix": "01.0f.40.ec.ae.00.cc.68.00.00.00.00"
   },
   {
    "bytes": 25672.0,
    "bytes_first": 25164.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7418
    },
    "packets": 50,
    "packets_first": 49,
    "src_participant_guid_prefix": "01.0f.40.ec.ae.00.cc.68.00.00.00.00"
   },
   {
    "bytes": 20916.0,
    "bytes_first": 20408.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7420
    },
    "packets": 31,
    "packets_first": 30,
    "src_participant_guid_prefix": "01.0f.40.ec.ae.00.cc.68.00.00.00.00"
   },
   {
    "bytes": 21044.0,
    "bytes_first": 20412.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7422
    },
    "packets": 32,
    "packets_first": 30,
    "src_participant_guid_prefix": "01.0f.40.ec.ae.00.cc.68.00.00.00.00"
   },
   {
    "bytes": 4064.0,
    "bytes_first": 3556.0,
    "dst_locator": {
     "address": "239.255.0.1",
     "kind": "UDPv4",
     "port": 7400
    },
    "packets": 8,
    "packets_first": 7,
    "src_participant_guid_prefix": "01.0f.40.ec.ae.00.cc.68.00.00.00.00"
   },
   {
    "bytes": 1052.0,
    "bytes_first": 1052.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7001
    },
    "packets": 6,
    "packets_first": 6,
    "src_participant_guid_prefix": "01.0f.40.ec.ae.00.cc.68.00.00.00.00"
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
    "src_participant_guid_prefix": "01.0f.40.ec.ae.00.cc.68.00.00.00.00"
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
    "src_participant_guid_prefix": "01.0f.40.ec.ae.00.cc.68.00.00.00.00"
   },
   {
    "bytes": 844.0,
    "bytes_first": 844.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7004
    },
    "packets": 4,
    "packets_first": 4,
    "src_participant_guid_prefix": "01.0f.40.ec.ae.00.cc.68.00.00.00.00"
   },
   {
    "bytes": 1748.0,
    "bytes_first": 1624.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7415
    },
    "packets": 14,
    "packets_first": 13,
    "src_participant_guid_prefix": "01.0f.40.ec.ae.00.cc.68.00.00.00.00"
   },
   {
    "bytes": 1128.0,
    "bytes_first": 1128.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7417
    },
    "packets": 9,
    "packets_first": 9,
    "src_participant_guid_prefix": "01.0f.40.ec.ae.00.cc.68.00.00.00.00"
   },
   {
    "bytes": 1128.0,
    "bytes_first": 1128.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7419
    },
    "packets": 9,
    "packets_first": 9,
    "src_participant_guid_prefix": "01.0f.40.ec.ae.00.cc.68.00.00.00.00"
   },
   {
    "bytes": 632.0,
    "bytes_first": 632.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7421
    },
    "packets": 5,
    "packets_first": 5,
    "src_participant_guid_prefix": "01.0f.40.ec.ae.00.cc.68.00.00.00.00"
   },
   {
    "bytes": 944.0,
    "bytes_first": 832.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7000
    },
    "packets": 5,
    "packets_first": 4,
    "src_participant_guid_prefix": "01.0f.40.ec.b1.00.02.4d.00.00.00.00"
   },
   {
    "bytes": 944.0,
    "bytes_first": 832.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7002
    },
    "packets": 5,
    "packets_first": 4,
    "src_participant_guid_prefix": "01.0f.40.ec.b1.00.02.4d.00.00.00.00"
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
    "src_participant_guid_prefix": "01.0f.40.ec.b1.00.02.4d.00.00.00.00"
   },
   {
    "bytes": 944.0,
    "bytes_first": 832.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7004
    },
    "packets": 5,
    "packets_first": 4,
    "src_participant_guid_prefix": "01.0f.40.ec.b1.00.02.4d.00.00.00.00"
   },
   {
    "bytes": 848.0,
    "bytes_first": 128.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7005
    },
    "packets": 4,
    "packets_first": 1,
    "src_participant_guid_prefix": "01.0f.40.ec.b1.00.02.4d.00.00.00.00"
   },
   {
    "bytes": 23960.0,
    "bytes_first": 23480.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7410
    },
    "packets": 45,
    "packets_first": 44,
    "src_participant_guid_prefix": "01.0f.40.ec.b1.00.02.4d.00.00.00.00"
   },
   {
    "bytes": 876.0,
    "bytes_first": 752.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7411
    },
    "packets": 7,
    "packets_first": 6,
    "src_participant_guid_prefix": "01.0f.40.ec.b1.00.02.4d.00.00.00.00"
   },
   {
    "bytes": 22456.0,
    "bytes_first": 21976.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7414
    },
    "packets": 30,
    "packets_first": 29,
    "src_participant_guid_prefix": "01.0f.40.ec.b1.00.02.4d.00.00.00.00"
   },
   {
    "bytes": 1748.0,
    "bytes_first": 1376.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7415
    },
    "packets": 14,
    "packets_first": 11,
    "src_participant_guid_prefix": "01.0f.40.ec.b1.00.02.4d.00.00.00.00"
   },
   {
    "bytes": 22452.0,
    "bytes_first": 21972.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7416
    },
    "packets": 30,
    "packets_first": 29,
    "src_participant_guid_prefix": "01.0f.40.ec.b1.00.02.4d.00.00.00.00"
   },
   {
    "bytes": 1252.0,
    "bytes_first": 1128.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7417
    },
    "packets": 10,
    "packets_first": 9,
    "src_participant_guid_prefix": "01.0f.40.ec.b1.00.02.4d.00.00.00.00"
   },
   {
    "bytes": 22456.0,
    "bytes_first": 21976.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7418
    },
    "packets": 30,
    "packets_first": 29,
    "src_participant_guid_prefix": "01.0f.40.ec.b1.00.02.4d.00.00.00.00"
   },
   {
    "bytes": 1376.0,
    "bytes_first": 1252.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7419
    },
    "packets": 11,
    "packets_first": 10,
    "src_participant_guid_prefix": "01.0f.40.ec.b1.00.02.4d.00.00.00.00"
   },
   {
    "bytes": 19060.0,
    "bytes_first": 9364.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7420
    },
    "packets": 21,
    "packets_first": 12,
    "src_participant_guid_prefix": "01.0f.40.ec.b1.00.02.4d.00.00.00.00"
   },
   {
    "bytes": 760.0,
    "bytes_first": 760.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7421
    },
    "packets": 6,
    "packets_first": 6,
    "src_participant_guid_prefix": "01.0f.40.ec.b1.00.02.4d.00.00.00.00"
   },
   {
    "bytes": 19188.0,
    "bytes_first": 9368.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7422
    },
    "packets": 22,
    "packets_first": 12,
    "src_participant_guid_prefix": "01.0f.40.ec.b1.00.02.4d.00.00.00.00"
   },
   {
    "bytes": 3840.0,
    "bytes_first": 2880.0,
    "dst_locator": {
     "address": "239.255.0.1",
     "kind": "UDPv4",
     "port": 7400
    },
    "packets": 8,
    "packets_first": 6,
    "src_participant_guid_prefix": "01.0f.40.ec.b1.00.02.4d.00.00.00.00"
   },
   {
    "bytes": 1052.0,
    "bytes_first": 1052.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7001
    },
    "packets": 6,
    "packets_first": 6,
    "src_participant_guid_prefix": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00"
   },
   {
    "bytes": 24000.0,
    "bytes_first": 23492.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7410
    },
    "packets": 34,
    "packets_first": 33,
    "src_participant_guid_prefix": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00"
   },
   {
    "bytes": 24640.0,
    "bytes_first": 24132.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7412
    },
    "packets": 54,
    "packets_first": 53,
    "src_participant_guid_prefix": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00"
   },
   {
    "bytes": 876.0,
    "bytes_first": 876.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7413
    },
    "packets": 7,
    "packets_first": 7,
    "src_participant_guid_prefix": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00"
   },
   {
    "bytes": 24124.0,
    "bytes_first": 23616.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7414
    },
    "packets": 35,
    "packets_first": 34,
    "src_participant_guid_prefix": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00"
   },
   {
    "bytes": 36888.0,
    "bytes_first": 36380.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7418
    },
    "packets": 57,
    "packets_first": 56,
    "src_participant_guid_prefix": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00"
   },
   {
    "bytes": 20184.0,
    "bytes_first": 19676.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7420
    },
    "packets": 23,
    "packets_first": 22,
    "src_participant_guid_prefix": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00"
   },
   {
    "bytes": 20312.0,
    "bytes_first": 19680.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7422
    },
    "packets": 24,
    "packets_first": 22,
    "src_participant_guid_prefix": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00"
   },
   {
    "bytes": 4064.0,
    "bytes_first": 3556.0,
    "dst_locator": {
     "address": "239.255.0.1",
     "kind": "UDPv4",
     "port": 7400
    },
    "packets": 8,
    "packets_first": 7,
    "src_participant_guid_prefix": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00"
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
    "src_participant_guid_prefix": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00"
   },
   {
    "bytes": 940.0,
    "bytes_first": 940.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7001
    },
    "packets": 5,
    "packets_first": 5,
    "src_participant_guid_prefix": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00"
   },
   {
    "bytes": 1052.0,
    "bytes_first": 1052.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7003
    },
    "packets": 6,
    "packets_first": 6,
    "src_participant_guid_prefix": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00"
   },
   {
    "bytes": 844.0,
    "bytes_first": 844.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7004
    },
    "packets": 4,
    "packets_first": 4,
    "src_participant_guid_prefix": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00"
   },
   {
    "bytes": 876.0,
    "bytes_first": 876.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7411
    },
    "packets": 7,
    "packets_first": 7,
    "src_participant_guid_prefix": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00"
   },
   {
    "bytes": 876.0,
    "bytes_first": 876.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7415
    },
    "packets": 7,
    "packets_first": 7,
    "src_participant_guid_prefix": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00"
   },
   {
    "bytes": 18776.0,
    "bytes_first": 10788.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7419
    },
    "packets": 133,
    "packets_first": 77,
    "src_participant_guid_prefix": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00"
   },
   {
    "bytes": 632.0,
    "bytes_first": 632.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7421
    },
    "packets": 5,
    "packets_first": 5,
    "src_participant_guid_prefix": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00"
   },
   {
    "bytes": 1292.0,
    "bytes_first": 1180.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7001
    },
    "packets": 8,
    "packets_first": 7,
    "src_participant_guid_prefix": "01.0f.40.ec.bf.00.9b.65.00.00.00.00"
   },
   {
    "bytes": 23888.0,
    "bytes_first": 23380.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7410
    },
    "packets": 33,
    "packets_first": 32,
    "src_participant_guid_prefix": "01.0f.40.ec.bf.00.9b.65.00.00.00.00"
   },
   {
    "bytes": 26088.0,
    "bytes_first": 25580.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7412
    },
    "packets": 56,
    "packets_first": 55,
    "src_participant_guid_prefix": "01.0f.40.ec.bf.00.9b.65.00.00.00.00"
   },
   {
    "bytes": 1004.0,
    "bytes_first": 880.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7413
    },
    "packets": 8,
    "packets_first": 7,
    "src_participant_guid_prefix": "01.0f.40.ec.bf.00.9b.65.00.00.00.00"
   },
   {
    "bytes": 24012.0,
    "bytes_first": 23504.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7414
    },
    "packets": 34,
    "packets_first": 33,
    "src_participant_guid_prefix": "01.0f.40.ec.bf.00.9b.65.00.00.00.00"
   },
   {
    "bytes": 24012.0,
    "bytes_first": 23504.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7416
    },
    "packets": 34,
    "packets_first": 33,
    "src_participant_guid_prefix": "01.0f.40.ec.bf.00.9b.65.00.00.00.00"
   },
   {
    "bytes": 20184.0,
    "bytes_first": 12312.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7420
    },
    "packets": 23,
    "packets_first": 15,
    "src_participant_guid_prefix": "01.0f.40.ec.bf.00.9b.65.00.00.00.00"
   },
   {
    "bytes": 20312.0,
    "bytes_first": 12316.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7422
    },
    "packets": 24,
    "packets_first": 15,
    "src_participant_guid_prefix": "01.0f.40.ec.bf.00.9b.65.00.00.00.00"
   },
   {
    "bytes": 4064.0,
    "bytes_first": 3048.0,
    "dst_locator": {
     "address": "239.255.0.1",
     "kind": "UDPv4",
     "port": 7400
    },
    "packets": 8,
    "packets_first": 6,
    "src_participant_guid_prefix": "01.0f.40.ec.bf.00.9b.65.00.00.00.00"
   },
   {
    "bytes": 940.0,
    "bytes_first": 828.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7000
    },
    "packets": 5,
    "packets_first": 4,
    "src_participant_guid_prefix": "01.0f.40.ec.bf.00.9b.65.00.00.00.00"
   },
   {
    "bytes": 940.0,
    "bytes_first": 828.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7001
    },
    "packets": 5,
    "packets_first": 4,
    "src_participant_guid_prefix": "01.0f.40.ec.bf.00.9b.65.00.00.00.00"
   },
   {
    "bytes": 1164.0,
    "bytes_first": 1052.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7002
    },
    "packets": 7,
    "packets_first": 6,
    "src_participant_guid_prefix": "01.0f.40.ec.bf.00.9b.65.00.00.00.00"
   },
   {
    "bytes": 844.0,
    "bytes_first": 128.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7004
    },
    "packets": 4,
    "packets_first": 1,
    "src_participant_guid_prefix": "01.0f.40.ec.bf.00.9b.65.00.00.00.00"
   },
   {
    "bytes": 876.0,
    "bytes_first": 752.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7411
    },
    "packets": 7,
    "packets_first": 6,
    "src_participant_guid_prefix": "01.0f.40.ec.bf.00.9b.65.00.00.00.00"
   },
   {
    "bytes": 876.0,
    "bytes_first": 752.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7415
    },
    "packets": 7,
    "packets_first": 6,
    "src_participant_guid_prefix": "01.0f.40.ec.bf.00.9b.65.00.00.00.00"
   },
   {
    "bytes": 2612.0,
    "bytes_first": 1744.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7417
    },
    "packets": 21,
    "packets_first": 14,
    "src_participant_guid_prefix": "01.0f.40.ec.bf.00.9b.65.00.00.00.00"
   },
   {
    "bytes": 632.0,
    "bytes_first": 632.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7421
    },
    "packets": 5,
    "packets_first": 5,
    "src_participant_guid_prefix": "01.0f.40.ec.bf.00.9b.65.00.00.00.00"
   },
   {
    "bytes": 1048.0,
    "bytes_first": 1048.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7001
    },
    "packets": 6,
    "packets_first": 6,
    "src_participant_guid_prefix": "01.0f.40.ec.c0.00.a2.17.00.00.00.00"
   },
   {
    "bytes": 26004.0,
    "bytes_first": 25496.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7410
    },
    "packets": 60,
    "packets_first": 59,
    "src_participant_guid_prefix": "01.0f.40.ec.c0.00.a2.17.00.00.00.00"
   },
   {
    "bytes": 25784.0,
    "bytes_first": 25276.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7412
    },
    "packets": 58,
    "packets_first": 57,
    "src_participant_guid_prefix": "01.0f.40.ec.c0.00.a2.17.00.00.00.00"
   },
   {
    "bytes": 3276.0,
    "bytes_first": 2324.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7413
    },
    "packets": 25,
    "packets_first": 18,
    "src_participant_guid_prefix": "01.0f.40.ec.c0.00.a2.17.00.00.00.00"
   },
   {
    "bytes": 25052.0,
    "bytes_first": 24544.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7416
    },
    "packets": 39,
    "packets_first": 38,
    "src_participant_guid_prefix": "01.0f.40.ec.c0.00.a2.17.00.00.00.00"
   },
   {
    "bytes": 24444.0,
    "bytes_first": 23936.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7418
    },
    "packets": 38,
    "packets_first": 37,
    "src_participant_guid_prefix": "01.0f.40.ec.c0.00.a2.17.00.00.00.00"
   },
   {
    "bytes": 20120.0,
    "bytes_first": 19612.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7420
    },
    "packets": 23,
    "packets_first": 22,
    "src_participant_guid_prefix": "01.0f.40.ec.c0.00.a2.17.00.00.00.00"
   },
   {
    "bytes": 20248.0,
    "bytes_first": 19616.0,
    "dst_locator": {
     "address": "127.0.0.1",
     "kind": "UDPv4",
     "port": 7422
    },
    "packets": 24,
    "packets_first": 22,
    "src_participant_guid_prefix": "01.0f.40.ec.c0.00.a2.17.00.00.00.00"
   },
   {
    "bytes": 4064.0,
    "bytes_first": 3556.0,
    "dst_locator": {
     "address": "239.255.0.1",
     "kind": "UDPv4",
     "port": 7400
    },
    "packets": 8,
    "packets_first": 7,
    "src_participant_guid_prefix": "01.0f.40.ec.c0.00.a2.17.00.00.00.00"
   },
   {
    "bytes": 1048.0,
    "bytes_first": 1048.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7000
    },
    "packets": 6,
    "packets_first": 6,
    "src_participant_guid_prefix": "01.0f.40.ec.c0.00.a2.17.00.00.00.00"
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
    "src_participant_guid_prefix": "01.0f.40.ec.c0.00.a2.17.00.00.00.00"
   },
   {
    "bytes": 936.0,
    "bytes_first": 936.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7003
    },
    "packets": 5,
    "packets_first": 5,
    "src_participant_guid_prefix": "01.0f.40.ec.c0.00.a2.17.00.00.00.00"
   },
   {
    "bytes": 968.0,
    "bytes_first": 968.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7004
    },
    "packets": 5,
    "packets_first": 5,
    "src_participant_guid_prefix": "01.0f.40.ec.c0.00.a2.17.00.00.00.00"
   },
   {
    "bytes": 3276.0,
    "bytes_first": 2324.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7411
    },
    "packets": 25,
    "packets_first": 18,
    "src_participant_guid_prefix": "01.0f.40.ec.c0.00.a2.17.00.00.00.00"
   },
   {
    "bytes": 1004.0,
    "bytes_first": 1004.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7417
    },
    "packets": 8,
    "packets_first": 8,
    "src_participant_guid_prefix": "01.0f.40.ec.c0.00.a2.17.00.00.00.00"
   },
   {
    "bytes": 1132.0,
    "bytes_first": 1132.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7419
    },
    "packets": 9,
    "packets_first": 9,
    "src_participant_guid_prefix": "01.0f.40.ec.c0.00.a2.17.00.00.00.00"
   },
   {
    "bytes": 760.0,
    "bytes_first": 760.0,
    "dst_locator": {
     "address": "",
     "kind": "SHM",
     "port": 7421
    },
    "packets": 6,
    "packets_first": 6,
    "src_participant_guid_prefix": "01.0f.40.ec.c0.00.a2.17.00.00.00.00"
   }
  ]
 },
 "topics": [
  {
   "dds_topic": "rt/bounded",
   "is_ros_topic": true,
   "latency_s": 0.00038563053571428584,
   "lost_packets": 0,
   "pairs": [
    {
     "confidence": "certain",
     "locator": null,
     "measured": {
      "available": true,
      "bytes": 7988.0,
      "bytes_total": 18776.0,
      "data_submessages": 55,
      "delivered": true,
      "delivered_samples": 56,
      "latency_s": {
       "last": 0.000492207,
       "max": 0.000555292,
       "mean": 0.00038563053571428584,
       "min": 0.00017395800000000002,
       "samples": 56
      },
      "locators": [
       {
        "address": "",
        "bytes": 7988.0,
        "kind": "SHM",
        "packets": 56,
        "port": 7419
       }
      ],
      "packets": 56,
      "packets_total": 133,
      "reliability": {
       "acknacks": 5,
       "gaps": 0,
       "heartbeats": 6,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 80.1455409186227,
      "transports": [
       "SHM"
      ]
     },
     "reader_guid": "01.0f.40.ec.bf.00.9b.65.00.00.00.00|00.00.14.04",
     "reader_host": "c4d031b385df",
     "reader_node": "/bounded_sub",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "both-shm-locators",
      "measured-shm-traffic"
     ],
     "transport": "SHM",
     "warnings": [],
     "writer_guid": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00|00.00.14.03",
     "writer_host": "c4d031b385df",
     "writer_node": "/bounded_pub"
    }
   ],
   "readers": [
    {
     "datasharing_history_bytes": null,
     "dds_topic": "rt/bounded",
     "dds_type": "std_msgs::msg::dds_::Int32_",
     "guid": "01.0f.40.ec.bf.00.9b.65.00.00.00.00|00.00.14.04",
     "host": "c4d031b385df",
     "host_id": "010f40ec",
     "host_name": "c4d031b385df:15082593216380207104",
     "multicast_locators": [],
     "node": "/bounded_sub",
     "participant_guid_prefix": "01.0f.40.ec.bf.00.9b.65.00.00.00.00",
     "process": "191",
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
   ],
   "resent_datas": 0,
   "throughput_bytes_per_s": 80.1455409186227,
   "topic": "/bounded",
   "type": "std_msgs/msg/Int32",
   "unmatched_reasons": [],
   "writers": [
    {
     "datasharing_history_bytes": null,
     "dds_topic": "rt/bounded",
     "dds_type": "std_msgs::msg::dds_::Int32_",
     "guid": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00|00.00.14.03",
     "host": "c4d031b385df",
     "host_id": "010f40ec",
     "host_name": "c4d031b385df:15082593216380207104",
     "multicast_locators": [],
     "node": "/bounded_pub",
     "participant_guid_prefix": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00",
     "process": "184",
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
   ]
  },
  {
   "dds_topic": "rt/chatter",
   "is_ros_topic": true,
   "latency_s": 0.00039034442857142866,
   "lost_packets": 0,
   "pairs": [
    {
     "confidence": "certain",
     "locator": null,
     "measured": {
      "available": true,
      "bytes": 952.0,
      "bytes_total": 3276.0,
      "data_submessages": 12,
      "delivered": true,
      "delivered_samples": 7,
      "latency_s": {
       "last": 0.000431166,
       "max": 0.000522874,
       "mean": 0.00039034442857142866,
       "min": 7.829000000000001e-05,
       "samples": 7
      },
      "locators": [
       {
        "address": "",
        "bytes": 952.0,
        "kind": "SHM",
        "packets": 7,
        "port": 7411
       }
      ],
      "packets": 7,
      "packets_total": 25,
      "reliability": {
       "acknacks": 1,
       "gaps": 0,
       "heartbeats": 1,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 23.427134105137416,
      "transports": [
       "SHM"
      ]
     },
     "reader_guid": "01.0f.40.ec.ae.00.cc.68.00.00.00.00|00.00.14.04",
     "reader_host": "c4d031b385df",
     "reader_node": "/listener",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "both-shm-locators",
      "measured-shm-traffic"
     ],
     "transport": "SHM",
     "warnings": [],
     "writer_guid": "01.0f.40.ec.c0.00.a2.17.00.00.00.00|00.00.14.03",
     "writer_host": "c4d031b385df",
     "writer_node": "/talker"
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
      "bytes": 952.0,
      "bytes_total": 3276.0,
      "data_submessages": 12,
      "delivered": true,
      "delivered_samples": 7,
      "latency_s": {
       "last": 0.000353457,
       "max": 0.0005307910000000001,
       "mean": 0.00037013028571428576,
       "min": 7.850000000000001e-05,
       "samples": 7
      },
      "locators": [
       {
        "address": "127.0.0.1",
        "bytes": 952.0,
        "kind": "UDPv4",
        "packets": 7,
        "port": 7413
       }
      ],
      "packets": 7,
      "packets_total": 25,
      "reliability": {
       "acknacks": 1,
       "gaps": 0,
       "heartbeats": 1,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 23.427134105137416,
      "transports": [
       "UDPv4"
      ]
     },
     "reader_guid": "01.0f.40.ec.b1.00.02.4d.00.00.00.00|00.00.14.04",
     "reader_host": "c4d031b385df",
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
     "writer_guid": "01.0f.40.ec.c0.00.a2.17.00.00.00.00|00.00.14.03",
     "writer_host": "c4d031b385df",
     "writer_node": "/talker"
    }
   ],
   "readers": [
    {
     "datasharing_history_bytes": null,
     "dds_topic": "rt/chatter",
     "dds_type": "std_msgs::msg::dds_::String_",
     "guid": "01.0f.40.ec.ae.00.cc.68.00.00.00.00|00.00.14.04",
     "host": "c4d031b385df",
     "host_id": "010f40ec",
     "host_name": "c4d031b385df:15082593216380207104",
     "multicast_locators": [],
     "node": "/listener",
     "participant_guid_prefix": "01.0f.40.ec.ae.00.cc.68.00.00.00.00",
     "process": "174",
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
       "port": 7411
      },
      {
       "address": "127.0.0.1",
       "kind": "UDPv4",
       "port": 7411
      }
     ]
    },
    {
     "datasharing_history_bytes": null,
     "dds_topic": "rt/chatter",
     "dds_type": "std_msgs::msg::dds_::String_",
     "guid": "01.0f.40.ec.b1.00.02.4d.00.00.00.00|00.00.14.04",
     "host": "c4d031b385df",
     "host_id": "010f40ec",
     "host_name": "c4d031b385df:15082593216380207104",
     "multicast_locators": [],
     "node": "/listener_udp",
     "participant_guid_prefix": "01.0f.40.ec.b1.00.02.4d.00.00.00.00",
     "process": "177",
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
       "port": 7413
      }
     ]
    }
   ],
   "resent_datas": 0,
   "throughput_bytes_per_s": 23.427134105137416,
   "topic": "/chatter",
   "type": "std_msgs/msg/String",
   "unmatched_reasons": [],
   "writers": [
    {
     "datasharing_history_bytes": null,
     "dds_topic": "rt/chatter",
     "dds_type": "std_msgs::msg::dds_::String_",
     "guid": "01.0f.40.ec.c0.00.a2.17.00.00.00.00|00.00.14.03",
     "host": "c4d031b385df",
     "host_id": "010f40ec",
     "host_name": "c4d031b385df:15082593216380207104",
     "multicast_locators": [],
     "node": "/talker",
     "participant_guid_prefix": "01.0f.40.ec.c0.00.a2.17.00.00.00.00",
     "process": "192",
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
   "latency_s": 1.0000000000000002e-06,
   "lost_packets": 0,
   "pairs": [
    {
     "confidence": "certain",
     "locator": null,
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
       "acknacks": 0,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 22109090.0,
      "transports": []
     },
     "reader_guid": "01.0f.40.ec.ae.00.cc.68.00.00.00.00|00.00.11.04",
     "reader_host": "c4d031b385df",
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
     "writer_guid": "01.0f.40.ec.ae.00.cc.68.00.00.00.00|00.00.10.03",
     "writer_host": "c4d031b385df",
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
      "bytes": 0.0,
      "bytes_total": 752.0,
      "data_submessages": 0,
      "delivered": false,
      "delivered_samples": 0,
      "latency_s": null,
      "locators": [
       {
        "address": "127.0.0.1",
        "bytes": 0.0,
        "kind": "UDPv4",
        "packets": 0,
        "port": 7413
       }
      ],
      "packets": 0,
      "packets_total": 6,
      "reliability": {
       "acknacks": 0,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 22109090.0,
      "transports": [
       "UDPv4"
      ]
     },
     "reader_guid": "01.0f.40.ec.b1.00.02.4d.00.00.00.00|00.00.11.04",
     "reader_host": "c4d031b385df",
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
     "writer_guid": "01.0f.40.ec.ae.00.cc.68.00.00.00.00|00.00.10.03",
     "writer_host": "c4d031b385df",
     "writer_node": "/listener"
    },
    {
     "confidence": "certain",
     "locator": null,
     "measured": {
      "available": true,
      "bytes": 0.0,
      "bytes_total": 1128.0,
      "data_submessages": 0,
      "delivered": false,
      "delivered_samples": 0,
      "latency_s": null,
      "locators": [
       {
        "address": "",
        "bytes": 0.0,
        "kind": "SHM",
        "packets": 0,
        "port": 7417
       }
      ],
      "packets": 0,
      "packets_total": 9,
      "reliability": {
       "acknacks": 0,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 22109090.0,
      "transports": [
       "SHM"
      ]
     },
     "reader_guid": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00|00.00.11.04",
     "reader_host": "c4d031b385df",
     "reader_node": "/bounded_pub",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "both-shm-locators",
      "measured-shm-traffic"
     ],
     "transport": "SHM",
     "warnings": [],
     "writer_guid": "01.0f.40.ec.ae.00.cc.68.00.00.00.00|00.00.10.03",
     "writer_host": "c4d031b385df",
     "writer_node": "/listener"
    },
    {
     "confidence": "certain",
     "locator": null,
     "measured": {
      "available": true,
      "bytes": 0.0,
      "bytes_total": 1128.0,
      "data_submessages": 0,
      "delivered": false,
      "delivered_samples": 0,
      "latency_s": null,
      "locators": [
       {
        "address": "",
        "bytes": 0.0,
        "kind": "SHM",
        "packets": 0,
        "port": 7419
       }
      ],
      "packets": 0,
      "packets_total": 9,
      "reliability": {
       "acknacks": 0,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 22109090.0,
      "transports": [
       "SHM"
      ]
     },
     "reader_guid": "01.0f.40.ec.bf.00.9b.65.00.00.00.00|00.00.11.04",
     "reader_host": "c4d031b385df",
     "reader_node": "/bounded_sub",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "both-shm-locators",
      "measured-shm-traffic"
     ],
     "transport": "SHM",
     "warnings": [],
     "writer_guid": "01.0f.40.ec.ae.00.cc.68.00.00.00.00|00.00.10.03",
     "writer_host": "c4d031b385df",
     "writer_node": "/listener"
    },
    {
     "confidence": "certain",
     "locator": null,
     "measured": {
      "available": true,
      "bytes": 124.0,
      "bytes_total": 1748.0,
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
        "port": 7415
       }
      ],
      "packets": 1,
      "packets_total": 14,
      "reliability": {
       "acknacks": 0,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 22109090.0,
      "transports": [
       "SHM"
      ]
     },
     "reader_guid": "01.0f.40.ec.c0.00.a2.17.00.00.00.00|00.00.11.04",
     "reader_host": "c4d031b385df",
     "reader_node": "/talker",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "both-shm-locators",
      "measured-shm-traffic"
     ],
     "transport": "SHM",
     "warnings": [],
     "writer_guid": "01.0f.40.ec.ae.00.cc.68.00.00.00.00|00.00.10.03",
     "writer_host": "c4d031b385df",
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
      "bytes": 124.0,
      "bytes_total": 876.0,
      "data_submessages": 0,
      "delivered": false,
      "delivered_samples": 0,
      "latency_s": null,
      "locators": [
       {
        "address": "127.0.0.1",
        "bytes": 124.0,
        "kind": "UDPv4",
        "packets": 1,
        "port": 7411
       }
      ],
      "packets": 1,
      "packets_total": 7,
      "reliability": {
       "acknacks": 0,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 21941854.0,
      "transports": [
       "UDPv4"
      ]
     },
     "reader_guid": "01.0f.40.ec.ae.00.cc.68.00.00.00.00|00.00.11.04",
     "reader_host": "c4d031b385df",
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
     "writer_guid": "01.0f.40.ec.b1.00.02.4d.00.00.00.00|00.00.10.03",
     "writer_host": "c4d031b385df",
     "writer_node": "/listener_udp"
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
      "bytes": 0.0,
      "bytes_total": 0.0,
      "data_submessages": 0,
      "delivered": true,
      "delivered_samples": 1,
      "latency_s": {
       "last": 8.32e-07,
       "max": 8.32e-07,
       "mean": 8.32e-07,
       "min": 8.32e-07,
       "samples": 1
      },
      "locators": [],
      "packets": 0,
      "packets_total": 0,
      "reliability": {
       "acknacks": 0,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 21941854.0,
      "transports": []
     },
     "reader_guid": "01.0f.40.ec.b1.00.02.4d.00.00.00.00|00.00.11.04",
     "reader_host": "c4d031b385df",
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
     "writer_guid": "01.0f.40.ec.b1.00.02.4d.00.00.00.00|00.00.10.03",
     "writer_host": "c4d031b385df",
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
      "bytes": 124.0,
      "bytes_total": 1252.0,
      "data_submessages": 0,
      "delivered": false,
      "delivered_samples": 0,
      "latency_s": null,
      "locators": [
       {
        "address": "127.0.0.1",
        "bytes": 124.0,
        "kind": "UDPv4",
        "packets": 1,
        "port": 7417
       }
      ],
      "packets": 1,
      "packets_total": 10,
      "reliability": {
       "acknacks": 0,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 21941854.0,
      "transports": [
       "UDPv4"
      ]
     },
     "reader_guid": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00|00.00.11.04",
     "reader_host": "c4d031b385df",
     "reader_node": "/bounded_pub",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "writer-no-shm-locator",
      "common-udpv4-locator",
      "measured-udpv4-traffic"
     ],
     "transport": "UDPv4",
     "warnings": [],
     "writer_guid": "01.0f.40.ec.b1.00.02.4d.00.00.00.00|00.00.10.03",
     "writer_host": "c4d031b385df",
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
      "bytes": 124.0,
      "bytes_total": 1376.0,
      "data_submessages": 0,
      "delivered": false,
      "delivered_samples": 0,
      "latency_s": null,
      "locators": [
       {
        "address": "127.0.0.1",
        "bytes": 124.0,
        "kind": "UDPv4",
        "packets": 1,
        "port": 7419
       }
      ],
      "packets": 1,
      "packets_total": 11,
      "reliability": {
       "acknacks": 0,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 21941854.0,
      "transports": [
       "UDPv4"
      ]
     },
     "reader_guid": "01.0f.40.ec.bf.00.9b.65.00.00.00.00|00.00.11.04",
     "reader_host": "c4d031b385df",
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
     "writer_guid": "01.0f.40.ec.b1.00.02.4d.00.00.00.00|00.00.10.03",
     "writer_host": "c4d031b385df",
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
      "bytes": 372.0,
      "bytes_total": 1748.0,
      "data_submessages": 0,
      "delivered": false,
      "delivered_samples": 0,
      "latency_s": null,
      "locators": [
       {
        "address": "127.0.0.1",
        "bytes": 372.0,
        "kind": "UDPv4",
        "packets": 3,
        "port": 7415
       }
      ],
      "packets": 3,
      "packets_total": 14,
      "reliability": {
       "acknacks": 0,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 21941854.0,
      "transports": [
       "UDPv4"
      ]
     },
     "reader_guid": "01.0f.40.ec.c0.00.a2.17.00.00.00.00|00.00.11.04",
     "reader_host": "c4d031b385df",
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
     "writer_guid": "01.0f.40.ec.b1.00.02.4d.00.00.00.00|00.00.10.03",
     "writer_host": "c4d031b385df",
     "writer_node": "/listener_udp"
    },
    {
     "confidence": "certain",
     "locator": null,
     "measured": {
      "available": true,
      "bytes": 0.0,
      "bytes_total": 876.0,
      "data_submessages": 0,
      "delivered": false,
      "delivered_samples": 0,
      "latency_s": null,
      "locators": [
       {
        "address": "",
        "bytes": 0.0,
        "kind": "SHM",
        "packets": 0,
        "port": 7411
       }
      ],
      "packets": 0,
      "packets_total": 7,
      "reliability": {
       "acknacks": 0,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 24457352.0,
      "transports": [
       "SHM"
      ]
     },
     "reader_guid": "01.0f.40.ec.ae.00.cc.68.00.00.00.00|00.00.11.04",
     "reader_host": "c4d031b385df",
     "reader_node": "/listener",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "both-shm-locators",
      "measured-shm-traffic"
     ],
     "transport": "SHM",
     "warnings": [],
     "writer_guid": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00|00.00.10.03",
     "writer_host": "c4d031b385df",
     "writer_node": "/bounded_pub"
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
      "bytes": 0.0,
      "bytes_total": 876.0,
      "data_submessages": 0,
      "delivered": false,
      "delivered_samples": 0,
      "latency_s": null,
      "locators": [
       {
        "address": "127.0.0.1",
        "bytes": 0.0,
        "kind": "UDPv4",
        "packets": 0,
        "port": 7413
       }
      ],
      "packets": 0,
      "packets_total": 7,
      "reliability": {
       "acknacks": 0,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 24457352.0,
      "transports": [
       "UDPv4"
      ]
     },
     "reader_guid": "01.0f.40.ec.b1.00.02.4d.00.00.00.00|00.00.11.04",
     "reader_host": "c4d031b385df",
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
     "writer_guid": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00|00.00.10.03",
     "writer_host": "c4d031b385df",
     "writer_node": "/bounded_pub"
    },
    {
     "confidence": "certain",
     "locator": null,
     "measured": {
      "available": true,
      "bytes": 0.0,
      "bytes_total": 0.0,
      "data_submessages": 0,
      "delivered": true,
      "delivered_samples": 1,
      "latency_s": {
       "last": 7.49e-07,
       "max": 7.49e-07,
       "mean": 7.49e-07,
       "min": 7.49e-07,
       "samples": 1
      },
      "locators": [],
      "packets": 0,
      "packets_total": 0,
      "reliability": {
       "acknacks": 0,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 24457352.0,
      "transports": []
     },
     "reader_guid": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00|00.00.11.04",
     "reader_host": "c4d031b385df",
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
     "writer_guid": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00|00.00.10.03",
     "writer_host": "c4d031b385df",
     "writer_node": "/bounded_pub"
    },
    {
     "confidence": "certain",
     "locator": null,
     "measured": {
      "available": true,
      "bytes": 7988.0,
      "bytes_total": 18776.0,
      "data_submessages": 0,
      "delivered": false,
      "delivered_samples": 0,
      "latency_s": null,
      "locators": [
       {
        "address": "",
        "bytes": 7988.0,
        "kind": "SHM",
        "packets": 56,
        "port": 7419
       }
      ],
      "packets": 56,
      "packets_total": 133,
      "reliability": {
       "acknacks": 0,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 24457352.0,
      "transports": [
       "SHM"
      ]
     },
     "reader_guid": "01.0f.40.ec.bf.00.9b.65.00.00.00.00|00.00.11.04",
     "reader_host": "c4d031b385df",
     "reader_node": "/bounded_sub",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "both-shm-locators",
      "measured-shm-traffic"
     ],
     "transport": "SHM",
     "warnings": [],
     "writer_guid": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00|00.00.10.03",
     "writer_host": "c4d031b385df",
     "writer_node": "/bounded_pub"
    },
    {
     "confidence": "certain",
     "locator": null,
     "measured": {
      "available": true,
      "bytes": 0.0,
      "bytes_total": 876.0,
      "data_submessages": 0,
      "delivered": false,
      "delivered_samples": 0,
      "latency_s": null,
      "locators": [
       {
        "address": "",
        "bytes": 0.0,
        "kind": "SHM",
        "packets": 0,
        "port": 7415
       }
      ],
      "packets": 0,
      "packets_total": 7,
      "reliability": {
       "acknacks": 0,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 24457352.0,
      "transports": [
       "SHM"
      ]
     },
     "reader_guid": "01.0f.40.ec.c0.00.a2.17.00.00.00.00|00.00.11.04",
     "reader_host": "c4d031b385df",
     "reader_node": "/talker",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "both-shm-locators",
      "measured-shm-traffic"
     ],
     "transport": "SHM",
     "warnings": [],
     "writer_guid": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00|00.00.10.03",
     "writer_host": "c4d031b385df",
     "writer_node": "/bounded_pub"
    },
    {
     "confidence": "certain",
     "locator": null,
     "measured": {
      "available": true,
      "bytes": 124.0,
      "bytes_total": 876.0,
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
        "port": 7411
       }
      ],
      "packets": 1,
      "packets_total": 7,
      "reliability": {
       "acknacks": 0,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 26481296.0,
      "transports": [
       "SHM"
      ]
     },
     "reader_guid": "01.0f.40.ec.ae.00.cc.68.00.00.00.00|00.00.11.04",
     "reader_host": "c4d031b385df",
     "reader_node": "/listener",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "both-shm-locators",
      "measured-shm-traffic"
     ],
     "transport": "SHM",
     "warnings": [],
     "writer_guid": "01.0f.40.ec.bf.00.9b.65.00.00.00.00|00.00.10.03",
     "writer_host": "c4d031b385df",
     "writer_node": "/bounded_sub"
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
      "bytes": 124.0,
      "bytes_total": 1004.0,
      "data_submessages": 0,
      "delivered": false,
      "delivered_samples": 0,
      "latency_s": null,
      "locators": [
       {
        "address": "127.0.0.1",
        "bytes": 124.0,
        "kind": "UDPv4",
        "packets": 1,
        "port": 7413
       }
      ],
      "packets": 1,
      "packets_total": 8,
      "reliability": {
       "acknacks": 0,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 26481296.0,
      "transports": [
       "UDPv4"
      ]
     },
     "reader_guid": "01.0f.40.ec.b1.00.02.4d.00.00.00.00|00.00.11.04",
     "reader_host": "c4d031b385df",
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
     "writer_guid": "01.0f.40.ec.bf.00.9b.65.00.00.00.00|00.00.10.03",
     "writer_host": "c4d031b385df",
     "writer_node": "/bounded_sub"
    },
    {
     "confidence": "certain",
     "locator": null,
     "measured": {
      "available": true,
      "bytes": 868.0,
      "bytes_total": 2612.0,
      "data_submessages": 0,
      "delivered": false,
      "delivered_samples": 0,
      "latency_s": null,
      "locators": [
       {
        "address": "",
        "bytes": 868.0,
        "kind": "SHM",
        "packets": 7,
        "port": 7417
       }
      ],
      "packets": 7,
      "packets_total": 21,
      "reliability": {
       "acknacks": 0,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 26481296.0,
      "transports": [
       "SHM"
      ]
     },
     "reader_guid": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00|00.00.11.04",
     "reader_host": "c4d031b385df",
     "reader_node": "/bounded_pub",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "both-shm-locators",
      "measured-shm-traffic"
     ],
     "transport": "SHM",
     "warnings": [],
     "writer_guid": "01.0f.40.ec.bf.00.9b.65.00.00.00.00|00.00.10.03",
     "writer_host": "c4d031b385df",
     "writer_node": "/bounded_sub"
    },
    {
     "confidence": "certain",
     "locator": null,
     "measured": {
      "available": true,
      "bytes": 0.0,
      "bytes_total": 0.0,
      "data_submessages": 0,
      "delivered": true,
      "delivered_samples": 1,
      "latency_s": {
       "last": 7.49e-07,
       "max": 7.49e-07,
       "mean": 7.49e-07,
       "min": 7.49e-07,
       "samples": 1
      },
      "locators": [],
      "packets": 0,
      "packets_total": 0,
      "reliability": {
       "acknacks": 0,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 26481296.0,
      "transports": []
     },
     "reader_guid": "01.0f.40.ec.bf.00.9b.65.00.00.00.00|00.00.11.04",
     "reader_host": "c4d031b385df",
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
     "writer_guid": "01.0f.40.ec.bf.00.9b.65.00.00.00.00|00.00.10.03",
     "writer_host": "c4d031b385df",
     "writer_node": "/bounded_sub"
    },
    {
     "confidence": "certain",
     "locator": null,
     "measured": {
      "available": true,
      "bytes": 124.0,
      "bytes_total": 876.0,
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
        "port": 7415
       }
      ],
      "packets": 1,
      "packets_total": 7,
      "reliability": {
       "acknacks": 0,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 26481296.0,
      "transports": [
       "SHM"
      ]
     },
     "reader_guid": "01.0f.40.ec.c0.00.a2.17.00.00.00.00|00.00.11.04",
     "reader_host": "c4d031b385df",
     "reader_node": "/talker",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "both-shm-locators",
      "measured-shm-traffic"
     ],
     "transport": "SHM",
     "warnings": [],
     "writer_guid": "01.0f.40.ec.bf.00.9b.65.00.00.00.00|00.00.10.03",
     "writer_host": "c4d031b385df",
     "writer_node": "/bounded_sub"
    },
    {
     "confidence": "certain",
     "locator": null,
     "measured": {
      "available": true,
      "bytes": 952.0,
      "bytes_total": 3276.0,
      "data_submessages": 0,
      "delivered": false,
      "delivered_samples": 0,
      "latency_s": null,
      "locators": [
       {
        "address": "",
        "bytes": 952.0,
        "kind": "SHM",
        "packets": 7,
        "port": 7411
       }
      ],
      "packets": 7,
      "packets_total": 25,
      "reliability": {
       "acknacks": 0,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 18516262.0,
      "transports": [
       "SHM"
      ]
     },
     "reader_guid": "01.0f.40.ec.ae.00.cc.68.00.00.00.00|00.00.11.04",
     "reader_host": "c4d031b385df",
     "reader_node": "/listener",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "both-shm-locators",
      "measured-shm-traffic"
     ],
     "transport": "SHM",
     "warnings": [],
     "writer_guid": "01.0f.40.ec.c0.00.a2.17.00.00.00.00|00.00.10.03",
     "writer_host": "c4d031b385df",
     "writer_node": "/talker"
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
      "bytes": 952.0,
      "bytes_total": 3276.0,
      "data_submessages": 0,
      "delivered": false,
      "delivered_samples": 0,
      "latency_s": null,
      "locators": [
       {
        "address": "127.0.0.1",
        "bytes": 952.0,
        "kind": "UDPv4",
        "packets": 7,
        "port": 7413
       }
      ],
      "packets": 7,
      "packets_total": 25,
      "reliability": {
       "acknacks": 0,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 18516262.0,
      "transports": [
       "UDPv4"
      ]
     },
     "reader_guid": "01.0f.40.ec.b1.00.02.4d.00.00.00.00|00.00.11.04",
     "reader_host": "c4d031b385df",
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
     "writer_guid": "01.0f.40.ec.c0.00.a2.17.00.00.00.00|00.00.10.03",
     "writer_host": "c4d031b385df",
     "writer_node": "/talker"
    },
    {
     "confidence": "certain",
     "locator": null,
     "measured": {
      "available": true,
      "bytes": 0.0,
      "bytes_total": 1004.0,
      "data_submessages": 0,
      "delivered": false,
      "delivered_samples": 0,
      "latency_s": null,
      "locators": [
       {
        "address": "",
        "bytes": 0.0,
        "kind": "SHM",
        "packets": 0,
        "port": 7417
       }
      ],
      "packets": 0,
      "packets_total": 8,
      "reliability": {
       "acknacks": 0,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 18516262.0,
      "transports": [
       "SHM"
      ]
     },
     "reader_guid": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00|00.00.11.04",
     "reader_host": "c4d031b385df",
     "reader_node": "/bounded_pub",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "both-shm-locators",
      "measured-shm-traffic"
     ],
     "transport": "SHM",
     "warnings": [],
     "writer_guid": "01.0f.40.ec.c0.00.a2.17.00.00.00.00|00.00.10.03",
     "writer_host": "c4d031b385df",
     "writer_node": "/talker"
    },
    {
     "confidence": "certain",
     "locator": null,
     "measured": {
      "available": true,
      "bytes": 0.0,
      "bytes_total": 1132.0,
      "data_submessages": 0,
      "delivered": false,
      "delivered_samples": 0,
      "latency_s": null,
      "locators": [
       {
        "address": "",
        "bytes": 0.0,
        "kind": "SHM",
        "packets": 0,
        "port": 7419
       }
      ],
      "packets": 0,
      "packets_total": 9,
      "reliability": {
       "acknacks": 0,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 18516262.0,
      "transports": [
       "SHM"
      ]
     },
     "reader_guid": "01.0f.40.ec.bf.00.9b.65.00.00.00.00|00.00.11.04",
     "reader_host": "c4d031b385df",
     "reader_node": "/bounded_sub",
     "reasons": [
      "same-host-guid",
      "datasharing-disabled-writer",
      "both-shm-locators",
      "measured-shm-traffic"
     ],
     "transport": "SHM",
     "warnings": [],
     "writer_guid": "01.0f.40.ec.c0.00.a2.17.00.00.00.00|00.00.10.03",
     "writer_host": "c4d031b385df",
     "writer_node": "/talker"
    },
    {
     "confidence": "certain",
     "locator": null,
     "measured": {
      "available": true,
      "bytes": 0.0,
      "bytes_total": 0.0,
      "data_submessages": 0,
      "delivered": true,
      "delivered_samples": 1,
      "latency_s": {
       "last": 1.0000000000000002e-06,
       "max": 1.0000000000000002e-06,
       "mean": 1.0000000000000002e-06,
       "min": 1.0000000000000002e-06,
       "samples": 1
      },
      "locators": [],
      "packets": 0,
      "packets_total": 0,
      "reliability": {
       "acknacks": 0,
       "gaps": 0,
       "heartbeats": 0,
       "lost_packets": 0,
       "nackfrags": 0,
       "resent_datas": 0
      },
      "throughput_bytes_per_s": 18516262.0,
      "transports": []
     },
     "reader_guid": "01.0f.40.ec.c0.00.a2.17.00.00.00.00|00.00.11.04",
     "reader_host": "c4d031b385df",
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
     "writer_guid": "01.0f.40.ec.c0.00.a2.17.00.00.00.00|00.00.10.03",
     "writer_host": "c4d031b385df",
     "writer_node": "/talker"
    }
   ],
   "readers": [
    {
     "datasharing_history_bytes": null,
     "dds_topic": "rt/parameter_events",
     "dds_type": "rcl_interfaces::msg::dds_::ParameterEvent_",
     "guid": "01.0f.40.ec.ae.00.cc.68.00.00.00.00|00.00.11.04",
     "host": "c4d031b385df",
     "host_id": "010f40ec",
     "host_name": "c4d031b385df:15082593216380207104",
     "multicast_locators": [],
     "node": "/listener",
     "participant_guid_prefix": "01.0f.40.ec.ae.00.cc.68.00.00.00.00",
     "process": "174",
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
       "port": 7411
      },
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
     "guid": "01.0f.40.ec.b1.00.02.4d.00.00.00.00|00.00.11.04",
     "host": "c4d031b385df",
     "host_id": "010f40ec",
     "host_name": "c4d031b385df:15082593216380207104",
     "multicast_locators": [],
     "node": "/listener_udp",
     "participant_guid_prefix": "01.0f.40.ec.b1.00.02.4d.00.00.00.00",
     "process": "177",
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
       "port": 7413
      }
     ]
    },
    {
     "datasharing_history_bytes": null,
     "dds_topic": "rt/parameter_events",
     "dds_type": "rcl_interfaces::msg::dds_::ParameterEvent_",
     "guid": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00|00.00.11.04",
     "host": "c4d031b385df",
     "host_id": "010f40ec",
     "host_name": "c4d031b385df:15082593216380207104",
     "multicast_locators": [],
     "node": "/bounded_pub",
     "participant_guid_prefix": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00",
     "process": "184",
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
     "guid": "01.0f.40.ec.bf.00.9b.65.00.00.00.00|00.00.11.04",
     "host": "c4d031b385df",
     "host_id": "010f40ec",
     "host_name": "c4d031b385df:15082593216380207104",
     "multicast_locators": [],
     "node": "/bounded_sub",
     "participant_guid_prefix": "01.0f.40.ec.bf.00.9b.65.00.00.00.00",
     "process": "191",
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
    },
    {
     "datasharing_history_bytes": null,
     "dds_topic": "rt/parameter_events",
     "dds_type": "rcl_interfaces::msg::dds_::ParameterEvent_",
     "guid": "01.0f.40.ec.c0.00.a2.17.00.00.00.00|00.00.11.04",
     "host": "c4d031b385df",
     "host_id": "010f40ec",
     "host_name": "c4d031b385df:15082593216380207104",
     "multicast_locators": [],
     "node": "/talker",
     "participant_guid_prefix": "01.0f.40.ec.c0.00.a2.17.00.00.00.00",
     "process": "192",
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
    }
   ],
   "resent_datas": 0,
   "throughput_bytes_per_s": 113505854.0,
   "topic": "/parameter_events",
   "type": "rcl_interfaces/msg/ParameterEvent",
   "unmatched_reasons": [],
   "writers": [
    {
     "datasharing_history_bytes": null,
     "dds_topic": "rt/parameter_events",
     "dds_type": "rcl_interfaces::msg::dds_::ParameterEvent_",
     "guid": "01.0f.40.ec.ae.00.cc.68.00.00.00.00|00.00.10.03",
     "host": "c4d031b385df",
     "host_id": "010f40ec",
     "host_name": "c4d031b385df:15082593216380207104",
     "multicast_locators": [],
     "node": "/listener",
     "participant_guid_prefix": "01.0f.40.ec.ae.00.cc.68.00.00.00.00",
     "process": "174",
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
       "port": 7411
      },
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
     "guid": "01.0f.40.ec.b1.00.02.4d.00.00.00.00|00.00.10.03",
     "host": "c4d031b385df",
     "host_id": "010f40ec",
     "host_name": "c4d031b385df:15082593216380207104",
     "multicast_locators": [],
     "node": "/listener_udp",
     "participant_guid_prefix": "01.0f.40.ec.b1.00.02.4d.00.00.00.00",
     "process": "177",
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
       "port": 7413
      }
     ]
    },
    {
     "datasharing_history_bytes": null,
     "dds_topic": "rt/parameter_events",
     "dds_type": "rcl_interfaces::msg::dds_::ParameterEvent_",
     "guid": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00|00.00.10.03",
     "host": "c4d031b385df",
     "host_id": "010f40ec",
     "host_name": "c4d031b385df:15082593216380207104",
     "multicast_locators": [],
     "node": "/bounded_pub",
     "participant_guid_prefix": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00",
     "process": "184",
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
     "guid": "01.0f.40.ec.bf.00.9b.65.00.00.00.00|00.00.10.03",
     "host": "c4d031b385df",
     "host_id": "010f40ec",
     "host_name": "c4d031b385df:15082593216380207104",
     "multicast_locators": [],
     "node": "/bounded_sub",
     "participant_guid_prefix": "01.0f.40.ec.bf.00.9b.65.00.00.00.00",
     "process": "191",
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
    },
    {
     "datasharing_history_bytes": null,
     "dds_topic": "rt/parameter_events",
     "dds_type": "rcl_interfaces::msg::dds_::ParameterEvent_",
     "guid": "01.0f.40.ec.c0.00.a2.17.00.00.00.00|00.00.10.03",
     "host": "c4d031b385df",
     "host_id": "010f40ec",
     "host_name": "c4d031b385df:15082593216380207104",
     "multicast_locators": [],
     "node": "/talker",
     "participant_guid_prefix": "01.0f.40.ec.c0.00.a2.17.00.00.00.00",
     "process": "192",
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
   "throughput_bytes_per_s": 412.9098211015974,
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
     "guid": "01.0f.40.ec.ae.00.cc.68.00.00.00.00|00.00.03.03",
     "host": "c4d031b385df",
     "host_id": "010f40ec",
     "host_name": "c4d031b385df:15082593216380207104",
     "multicast_locators": [],
     "node": "/listener",
     "participant_guid_prefix": "01.0f.40.ec.ae.00.cc.68.00.00.00.00",
     "process": "174",
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
       "port": 7411
      },
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
     "guid": "01.0f.40.ec.b1.00.02.4d.00.00.00.00|00.00.03.03",
     "host": "c4d031b385df",
     "host_id": "010f40ec",
     "host_name": "c4d031b385df:15082593216380207104",
     "multicast_locators": [],
     "node": "/listener_udp",
     "participant_guid_prefix": "01.0f.40.ec.b1.00.02.4d.00.00.00.00",
     "process": "177",
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
       "port": 7413
      }
     ]
    },
    {
     "datasharing_history_bytes": null,
     "dds_topic": "rt/rosout",
     "dds_type": "rcl_interfaces::msg::dds_::Log_",
     "guid": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00|00.00.03.03",
     "host": "c4d031b385df",
     "host_id": "010f40ec",
     "host_name": "c4d031b385df:15082593216380207104",
     "multicast_locators": [],
     "node": "/bounded_pub",
     "participant_guid_prefix": "01.0f.40.ec.b8.00.a3.cd.00.00.00.00",
     "process": "184",
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
     "guid": "01.0f.40.ec.bf.00.9b.65.00.00.00.00|00.00.03.03",
     "host": "c4d031b385df",
     "host_id": "010f40ec",
     "host_name": "c4d031b385df:15082593216380207104",
     "multicast_locators": [],
     "node": "/bounded_sub",
     "participant_guid_prefix": "01.0f.40.ec.bf.00.9b.65.00.00.00.00",
     "process": "191",
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
    },
    {
     "datasharing_history_bytes": null,
     "dds_topic": "rt/rosout",
     "dds_type": "rcl_interfaces::msg::dds_::Log_",
     "guid": "01.0f.40.ec.c0.00.a2.17.00.00.00.00|00.00.03.03",
     "host": "c4d031b385df",
     "host_id": "010f40ec",
     "host_name": "c4d031b385df:15082593216380207104",
     "multicast_locators": [],
     "node": "/talker",
     "participant_guid_prefix": "01.0f.40.ec.c0.00.a2.17.00.00.00.00",
     "process": "192",
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
    }
   ]
  }
 ]
};

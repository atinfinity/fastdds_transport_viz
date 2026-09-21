# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""--stats: nodes started with FASTDDS_STATISTICS => measured SHM traffic and host names."""
import json
import os
import signal
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
from _common import (  # noqa: E402
    Base, description, node_action, run_tool, skip_without_statistics, topic,
    transport_viz_json, udpv4_only_env)

from ament_index_python.packages import (  # noqa: E402
    get_package_prefix, get_package_share_directory)
import launch_testing  # noqa: E402

STATS_ENV = {
    'FASTDDS_STATISTICS': (
        'RTPS_SENT_TOPIC;RTPS_LOST_TOPIC;HISTORY_LATENCY_TOPIC;PHYSICAL_DATA_TOPIC;'
        'DATA_COUNT_TOPIC;RESENT_DATAS_TOPIC;'
        'HEARTBEAT_COUNT_TOPIC;ACKNACK_COUNT_TOPIC;NACKFRAG_COUNT_TOPIC;GAP_COUNT_TOPIC'),
    # lift the statistics writers' 10-instance resource limit (see README)
    'FASTRTPS_DEFAULT_PROFILES_FILE': os.path.join(
        get_package_share_directory('fastdds_transport_viz'), 'config', 'statistics.xml'),
}
NOSTATS_TOPIC = ['--ros-args', '-r', 'chatter:=chatter_nostats']


def generate_test_description():
    return description([
        node_action('demo_nodes_cpp', 'talker', 'talker', STATS_ENV),
        node_action('demo_nodes_cpp', 'listener', 'listener', STATS_ENV),
        node_action('demo_nodes_cpp', 'listener', 'listener_udp',
                    {**STATS_ENV, **udpv4_only_env()}),
        # statistics on the listener only, on a topic of their own (#113)
        node_action('demo_nodes_cpp', 'talker', 'talker_nostats', None, NOSTATS_TOPIC),
        node_action('demo_nodes_cpp', 'listener', 'listener_nostats', STATS_ENV, NOSTATS_TOPIC),
    ]), {}


@skip_without_statistics
class TestStats(Base):

    def test_measured_transports(self):
        doc = None
        for _ in range(4):
            doc = transport_viz_json(['--stats'], timeout=6.0)
            chatter = topic(doc, '/chatter')
            all_measured = all(p['measured']['transports'] for p in chatter['pairs'])
            if len(chatter['pairs']) == 2 and all_measured:
                break
        self.assertTrue(doc['stats']['enabled'])
        self.assertGreater(doc['stats']['samples'], 0, doc['stats'])
        pairs = {p['reader_node']: p for p in chatter['pairs']}
        self.assertEqual(set(pairs), {'/listener', '/listener_udp'}, chatter)

        shm = pairs['/listener']
        self.assertEqual(shm['transport'], 'SHM')
        self.assertTrue(shm['measured']['available'], shm)
        self.assertEqual(shm['measured']['transports'], ['SHM'], shm)
        self.assertGreater(shm['measured']['packets'], 0)
        self.assertIn('measured-shm-traffic', shm['reasons'])
        self.assertNotIn('measured-transport-mismatch', shm['warnings'])

        udp = pairs['/listener_udp']
        self.assertEqual(udp['transport'], 'UDPv4')
        self.assertEqual(udp['measured']['transports'], ['UDPv4'], udp)
        self.assertIn('measured-udpv4-traffic', udp['reasons'])
        self.assertNotIn('measured-transport-mismatch', udp['warnings'])

        # #137: there is no rate any more; the keys stay as null placeholders
        self.assertIsNone(chatter['throughput_bytes_per_s'], chatter)
        self.assertIsNone(shm['measured']['throughput_bytes_per_s'], shm)
        self.assertEqual(doc['stats']['throughput'], {}, doc['stats'])
        # the cumulative counters a rate could be rebuilt from are still collected
        self.assertTrue(doc['stats']['data_count'], doc['stats'])
        self.assertGreaterEqual(shm['measured']['packets_total'], shm['measured']['packets'])

        # PHYSICAL_DATA gives host names and processes
        writer = chatter['writers'][0]
        self.assertTrue(writer['host_name'], writer)
        self.assertTrue(writer['process'], writer)
        # table/JSON host label is the hostname part of "<hostname>:<numeric id>"
        self.assertEqual(shm['writer_host'], writer['host_name'].split(':')[0])

    def test_latency_values(self):
        doc = transport_viz_json(['--stats'], timeout=6.0)
        chatter = topic(doc, '/chatter')
        pair = next(p for p in chatter['pairs'] if p['reader_node'] == '/listener')
        lat = pair['measured']['latency_s']
        self.assertIsNotNone(lat, pair)
        self.assertGreater(lat['samples'], 0, lat)
        self.assertGreater(lat['mean'], 0.0, lat)            # same host: exact, positive
        self.assertLess(lat['mean'], 1.0, lat)               # and well below a second
        self.assertLessEqual(lat['min'], lat['mean'])
        self.assertLessEqual(lat['mean'], lat['max'])
        self.assertIsNotNone(chatter['latency_s'])
        self.assertNotIn('latency-clock-skew-suspected', pair['warnings'])

    def test_reliability_counters(self):
        doc = transport_viz_json(['--stats'], timeout=6.0)
        chatter = topic(doc, '/chatter')
        for pair in chatter['pairs']:
            rel = pair['measured']['reliability']
            self.assertIsNotNone(rel, pair)
            self.assertEqual(rel['lost_packets'], 0, pair)          # one host: nothing lost
            self.assertNotIn('rtps-packets-lost', pair['warnings'])
            for key in ('resent_datas', 'heartbeats', 'gaps', 'acknacks', 'nackfrags'):
                self.assertGreaterEqual(rel[key], 0, (key, rel))
        self.assertEqual(chatter['lost_packets'], 0, chatter)
        self.assertIsInstance(chatter['resent_datas'], int)
        self.assertIsInstance(doc['stats']['lost'], list)
        for entry in doc['stats']['lost']:
            for key in ('reporter_participant_guid_prefix', 'src_participant_guid_prefix',
                        'dst_locator', 'packets', 'packets_first'):
                self.assertIn(key, entry)

    def test_no_samples_lost_on_a_small_system(self):
        """
        A small, quiet system loses no statistics sample (#134).

        The loss counters are cumulative and shared by all ten readers, so this is the
        invariant worth asserting: five nodes give the tool nothing to fall behind on. The
        stderr line and the `stats-samples-lost` code come from the same predicate, so an
        empty `warnings` means nothing was printed either. Provoking a real loss would need
        a load this test cannot carry (see #141).
        """
        doc = transport_viz_json(['--stats'], timeout=6.0)
        stats = doc['stats']
        self.assertEqual(stats['samples_lost'], 0, stats)
        self.assertEqual(stats['samples_rejected'], 0, stats)
        # nothing here changes the statistics writers' QoS, so every one of them matches (#141)
        self.assertEqual(stats['writers_incompatible_qos'], 0, stats)
        self.assertEqual(stats['warnings'], [], stats)
        # the talker/listener pairs are delivered and measured; not pinned to 0 unmeasured,
        # which large samples on a slow machine can rightly produce without any loss (#147)
        self.assertGreater(stats['pairs_delivered'], 0, stats)
        self.assertLessEqual(stats['pairs_delivered_unmeasured'], stats['pairs_delivered'], stats)
        # the burst from before the readers matched is counted apart and never warns
        self.assertGreaterEqual(stats['samples_lost_at_start'], 0, stats)

    def test_statistics_sources_are_the_publishers(self):
        """
        Statistics on the reader's participant only: the writer's still has none.

        The listener's HISTORY_LATENCY names the talker's writer and the nodes report the
        tool's multicast discovery in RTPS_LOST; neither makes a participant a source (#113).
        """
        doc = pair = None
        for _ in range(4):
            doc = transport_viz_json(['--stats'], timeout=6.0)
            nostats = topic(doc, '/chatter_nostats')
            if len(nostats['pairs']) == 1:
                pair = nostats['pairs'][0]
                reader = nostats['readers'][0]
                if reader['participant_guid_prefix'] in doc['stats']['participants_with_stats']:
                    break
        self.assertIsNotNone(pair, nostats)
        sources = set(doc['stats']['participants_with_stats'])
        writer, reader = nostats['writers'][0], nostats['readers'][0]
        self.assertIn(reader['participant_guid_prefix'], sources, doc['stats'])
        self.assertNotIn(writer['participant_guid_prefix'], sources, doc['stats'])
        self.assertFalse(pair['measured']['available'], pair)
        self.assertIn('stats-not-enabled-on-writer', pair['warnings'])

        # every source is one of the nodes started with statistics: none of the tool's own
        chatter = topic(doc, '/chatter')
        with_stats = {e['participant_guid_prefix'] for e in (
            chatter['writers'] + chatter['readers'] + nostats['readers'])}
        self.assertLessEqual(sources, with_stats, doc['stats'])

    def test_tool_with_statistics_in_its_own_environment(self):
        """
        Statistics enabled for the tool's own environment too.

        FASTDDS_STATISTICS set for the tool too (the nodes' environment): the tool must
        drop it for its own participants, otherwise Fast DDS 2.14 deadlocks in
        on_rtps_sent() and this call never returns.
        """
        doc = transport_viz_json(['--stats'], timeout=6.0, env=STATS_ENV)
        self.assertTrue(doc['stats']['enabled'])
        chatter = topic(doc, '/chatter')
        pair = next(p for p in chatter['pairs'] if p['reader_node'] == '/listener')
        self.assertTrue(pair['measured']['available'], pair)

    def test_default_one_shot_settles(self):
        """
        The default --stats one-shot stops on the settle rule (#168), not on its 30 s cap.

        Five nodes: discovery is quiet, every RTPS_SENT writer heard from and the measured
        traffic entries complete well within the cap, so the run ends a few seconds after the
        5 s minimum (the 3 s window on the entries), and says so in the document.
        """
        cmd = ['ros2', 'run', 'fastdds_transport_viz', 'transport_viz', '--json', '--stats']
        start = time.monotonic()
        out = run_tool(cmd, timeout=60)
        wall = time.monotonic() - start
        doc = json.loads(out.stdout)
        stats = doc['stats']
        self.assertEqual(doc['discovery']['stopped_on'], 'settled', (doc['discovery'], stats))
        self.assertTrue(stats['settled'], stats)
        self.assertGreater(stats['writers_announced'], 0, stats)
        self.assertEqual(stats['writers_heard'], stats['writers_announced'], stats)
        self.assertGreaterEqual(stats['settled_at_s'], 5.0, stats)
        self.assertLess(stats['settled_at_s'], 15.0, stats)
        self.assertLess(wall, 25.0, (wall, stats))
        self.assertNotIn('not heard from', out.stderr)
        # and every pair with a delivery proof carries a measured packet
        self.assertEqual(stats['pairs_delivered_unmeasured'], 0, stats)

    def test_quiet_zero_one_shot_measures_its_pairs(self):
        """
        A --quiet 0 one-shot counts its measured instances like any other run (#200).

        The refresh that hands the drain the reader destinations an RTPS_SENT instance is
        judged against used to live inside the settle rule, which --quiet 0 skips: the run
        reported 0 measured instances and warned that nothing reached a discovered reader,
        however many packets its pairs carried. Every other launch test runs --quiet 0.
        """
        cmd = ['ros2', 'run', 'fastdds_transport_viz', 'transport_viz', '--json', '--stats',
               '--timeout', '10', '--quiet', '0']
        out = run_tool(cmd, timeout=60)
        doc = json.loads(out.stdout)
        stats = doc['stats']
        self.assertEqual(doc['discovery']['stopped_on'], 'timeout', doc['discovery'])
        self.assertGreater(stats['measurable_pairs'], 0, stats)
        self.assertGreater(stats['measured_instances'], 0, stats)
        # the warning of #179 is about a system where nothing was measured, not about this one
        self.assertNotIn('no measured RTPS_SENT', out.stderr)

    def test_watch_frames_measure_their_pairs(self):
        """
        Every --watch --stats frame counts its measured instances (#200).

        A watch runs no settle rule, so before #200 nothing ever handed it the destinations and
        every frame read 0 while its pairs carried hundreds of packets. The count is cumulative
        over the run and the first frame is drawn after the 5 s counter window, so each frame
        has to be above zero.
        """
        # the binary itself, not `ros2 run`: SIGINT has to reach the watch loop
        binary = os.path.join(get_package_prefix('fastdds_transport_viz'), 'lib',
                              'fastdds_transport_viz', 'transport_viz')
        proc = subprocess.Popen(
            [binary, '--watch', '--json', '--stats', '--interval', '1'],
            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
        frames = []
        try:
            deadline = time.monotonic() + 60
            for line in proc.stdout:     # one compact document per line (JSON Lines)
                if line.startswith('{'):
                    frames.append(json.loads(line))
                if len(frames) >= 3 or time.monotonic() > deadline:
                    break
        finally:
            proc.send_signal(signal.SIGINT)   # rclcpp's handler ends the watch loop
            try:
                proc.communicate(timeout=20)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.communicate()
        self.assertGreaterEqual(len(frames), 3, frames)
        for index, doc in enumerate(frames):
            stats = doc['stats']
            self.assertGreater(stats['measurable_pairs'], 0, (index, stats))
            self.assertGreater(stats['measured_instances'], 0, (index, stats))


@launch_testing.post_shutdown_test()
class TestShutdown(Base):

    def test_exit_codes(self, proc_info):
        pass

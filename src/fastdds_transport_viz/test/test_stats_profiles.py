# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""
The installed statistics profiles: what CMake generated from the templates (#152, #154, #159).

Fast DDS takes the whole DataWriter QoS from a profile named after a statistics alias, so
what these files say is what the observed nodes' statistics writers do. The writer
profiles live once in config/statistics_writers.xml.in and are pasted into every template,
generated per Fast DDS major (2.x drops a profile with an element it cannot parse), so
the checks run on the installed files, not on the templates.
"""
import os
import pathlib
import xml.etree.ElementTree as ET

from ament_index_python.packages import get_package_share_directory
import pytest

NS = {'p': 'http://www.eprosima.com/XMLSchemas/fastRTPS_Profiles'}
ALIASES = ['RTPS_SENT_TOPIC', 'HISTORY_LATENCY_TOPIC', 'DATA_COUNT_TOPIC', 'RTPS_LOST_TOPIC',
           'RESENT_DATAS_TOPIC', 'HEARTBEAT_COUNT_TOPIC', 'ACKNACK_COUNT_TOPIC',
           'NACKFRAG_COUNT_TOPIC', 'GAP_COUNT_TOPIC']
STATS_FLOW_CONTROLLER = 'FastDDSStatisticsFlowControllerDefault'

SHARE = pathlib.Path(get_package_share_directory('fastdds_transport_viz'))
# the shipped profiles and the test_large_shm.py fixture, all generated from the same fragment
GENERATED = [SHARE / 'config' / 'statistics.xml', SHARE / 'config' / 'datasharing_auto_stats.xml',
             SHARE / 'test' / 'large_shm_stats.xml']
FIXTURE = GENERATED[-1]


def fastdds_major():
    """2 on Humble (2.6) and Jazzy (2.14), 3 from Kilted on (as test/launch/_common.py)."""
    return 2 if os.environ.get('ROS_DISTRO') in ('humble', 'jazzy') else 3


def writer_profiles(path):
    root = ET.parse(path).getroot()
    profiles = {w.get('profile_name'): w for w in root.iterfind('.//p:data_writer', NS)}
    return {alias: profiles[alias] for alias in ALIASES if alias in profiles}


def text(elem, path):
    found = elem.find(path, NS)
    return None if found is None else found.text


@pytest.mark.parametrize('path', GENERATED, ids=lambda p: p.name)
def test_every_alias_has_a_profile(path):
    assert sorted(writer_profiles(path)) == sorted(ALIASES)


@pytest.mark.parametrize('path', GENERATED, ids=lambda p: p.name)
def test_writer_qos_matches_the_statistics_module(path):
    for alias, writer in writer_profiles(path).items():
        assert text(writer, 'p:qos/p:reliability/p:kind') == 'RELIABLE', alias
        assert text(writer, 'p:qos/p:durability/p:kind') == 'TRANSIENT_LOCAL', alias
        assert text(writer, 'p:qos/p:publishMode/p:kind') == 'ASYNCHRONOUS', alias
        assert text(writer, 'p:qos/p:publishMode/p:flow_controller_name') == \
            STATS_FLOW_CONTROLLER, alias
        assert text(writer, 'p:topic/p:historyQos/p:depth') == '10', alias
        assert text(writer, 'p:topic/p:resourceLimitsQos/p:max_instances') == '0', alias
        # push mode is deliberate (#154): a pull-mode writer sends RELIABLE remote readers
        # data only after a heartbeat / ACKNACK round trip
        assert writer.find('p:propertiesPolicy', NS) is None, alias


@pytest.mark.parametrize('path', GENERATED, ids=lambda p: p.name)
def test_heartbeat_period_only_on_fastdds_3(path):
    for alias, writer in writer_profiles(path).items():
        period = writer.find('p:times/p:heartbeat_period', NS)
        if alias == 'HISTORY_LATENCY_TOPIC' or fastdds_major() < 3:
            assert period is None, alias
        else:
            assert (text(period, 'p:sec'), text(period, 'p:nanosec')) == ('0', '500000000'), alias
        assert writer.find('p:times/p:heartbeatPeriod', NS) is None, alias


def test_fixture_keeps_the_large_shm_transport():
    """The half of the fixture that is not generated: the 16 MB SHM segment of #33."""
    root = ET.parse(FIXTURE).getroot()
    participant = root.find('.//p:participant[@profile_name="large_shm"]', NS)
    assert participant is not None and participant.get('is_default_profile') == 'true'
    assert text(participant, 'p:rtps/p:useBuiltinTransports') == 'false'
    transports = participant.iterfind('p:rtps/p:userTransports/p:transport_id', NS)
    assert [t.text for t in transports] == ['shm_large', 'udp_default']
    shm = root.find('.//p:transport_descriptor[p:transport_id="shm_large"]', NS)
    assert text(shm, 'p:type') == 'SHM'
    assert text(shm, 'p:segment_size') == '16777216'
    assert text(shm, 'p:maxMessageSize') == '4194304'

# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""The shipped sample documents must satisfy schema/transport_viz.schema.json."""
import json
import pathlib

import jsonschema
import pytest

# Draft 2020-12 where available (Jazzy+); Humble's jsonschema 3.2 validates the same
# schema with its newest draft (the schema only uses $defs, $ref, enum, type, const).
VALIDATOR = (
    getattr(jsonschema, 'Draft202012Validator', None) or
    jsonschema.validators.validator_for({'$schema': 'http://json-schema.org/draft-07/schema#'}))

REPO = pathlib.Path(__file__).resolve().parents[3]
SCHEMA = REPO / 'schema' / 'transport_viz.schema.json'
SAMPLES = (sorted((REPO / 'web' / 'sample').glob('*.json')) +
           sorted((REPO / 'src' / 'fastdds_transport_viz' / 'test' / 'fixtures').glob('*.json')))


def load(path):
    with open(path) as f:
        return json.load(f)


def test_schema_itself_is_valid():
    VALIDATOR.check_schema(load(SCHEMA))


@pytest.mark.parametrize('sample', SAMPLES, ids=[s.name for s in SAMPLES])
def test_sample_matches_schema(sample):
    assert SAMPLES, 'no sample documents found'
    VALIDATOR(load(SCHEMA)).validate(load(sample))


@pytest.mark.parametrize('name', ['sample.json', 'shm_split.json'])
def test_sample_descriptions_cover_used_codes(name):
    doc = load(REPO / 'web' / 'sample' / name)
    used = set()
    for t in doc['topics']:
        used.update(t['unmatched_reasons'])
        for p in t['pairs']:
            used.update(p['reasons'])
            used.update(p['warnings'])
    assert used <= set(doc['reason_code_descriptions'])


def test_sample_remedies_share_the_keys_of_the_descriptions():
    doc = load(REPO / 'web' / 'sample' / 'sample.json')
    assert set(doc['reason_code_remedies']) == set(doc['reason_code_descriptions'])
    # a normal state has nothing to change, a reader without SHM does
    assert doc['reason_code_remedies']['same-host-guid'] is None
    assert 'FASTDDS_BUILTIN_TRANSPORTS' in doc['reason_code_remedies']['reader-no-shm-locator']


def test_split_sample_has_the_lost_shm_pair():
    doc = load(REPO / 'web' / 'sample' / 'shm_split.json')
    chatter = next(t for t in doc['topics'] if t['topic'] == '/chatter')
    [pair] = chatter['pairs']
    assert pair['transport'] == 'NONE'
    assert pair['warnings'] == ['shm-ipc-namespace-split']
    assert 'shm-port-collision' in pair['reasons']
    assert 'ipc: host' in doc['reason_code_remedies']['shm-ipc-namespace-split']


def test_participants_fixture_uses_every_lock_and_visibility_value():
    """fixtures/participants.json (#125): every enum value of the schema at least once."""
    schema = load(SCHEMA)
    doc = load(REPO / 'src' / 'fastdds_transport_viz' / 'test' / 'fixtures' / 'participants.json')
    locks = {p['lock'] for d in doc['participants'] for p in d['shm_ports']}
    assert locks == set(schema['$defs']['shm_port']['properties']['lock']['enum'])
    visibilities = {d['shm_visibility'] for d in doc['participants']}
    assert visibilities == set(
        schema['$defs']['participant']['properties']['shm_visibility']['enum'])
    assert doc['shm']['unknown_ports'] == []


def test_datasharing_fixture_uses_every_segment_visibility_value():
    """fixtures/datasharing.json (#163): every enum value of the schema at least once."""
    schema = load(SCHEMA)
    doc = load(REPO / 'src' / 'fastdds_transport_viz' / 'test' / 'fixtures' / 'datasharing.json')
    endpoints = [e for t in doc['topics'] for e in t['writers'] + t['readers']]
    seen = {e['datasharing_segment_visibility'] for e in endpoints}
    assert seen == set(
        schema['$defs']['endpoint']['properties']['datasharing_segment_visibility']['enum'])
    # a writer with a history here is visible; unprobed only where data-sharing is OFF
    for e in endpoints:
        if isinstance(e['datasharing_history_bytes'], int):
            assert e['datasharing_segment_visibility'] == 'visible', e['guid']
        if e['datasharing_segment_visibility'] == 'unprobed':
            assert e['qos']['data_sharing'] == 'OFF', e['guid']


def _resolve(node, schema):
    """Follow a local `$ref` to the node it names."""
    for _ in range(20):
        if not (isinstance(node, dict) and '$ref' in node):
            break
        target = schema
        for part in node['$ref'].lstrip('#/').split('/'):
            target = target[part]
        node = target
    return node


def _branches(node, schema):
    """
    Return the node and every allOf/anyOf/oneOf branch under it.

    A key any one of them declares is declared. They are unioned instead of checked one by one
    because `additionalProperties` and `allOf` do not compose in JSON Schema itself:
    changes.changed_pairs[] carries `from` and `to` in one branch and the pair key in the
    other, and neither branch alone sees both.
    """
    node = _resolve(node, schema)
    if not isinstance(node, dict):
        return []
    out = [node]
    for keyword in ('allOf', 'anyOf', 'oneOf'):
        for branch in node.get(keyword, []):
            out += _branches(branch, schema)
    return out


def undeclared_keys(doc, node, schema, path):
    """
    Return the keys of `doc` that no property of `node` declares (#199).

    A node that declares no properties at all is a free-form object (`stats.physical`,
    `stats.data_count`, ...) and has nothing to violate; one whose `additionalProperties` is a
    schema has its values walked against that schema.
    """
    found = []
    branches = _branches(node, schema)
    if isinstance(doc, dict):
        properties, additional = {}, None
        for branch in branches:
            properties.update(branch.get('properties', {}))
            if additional is None:
                additional = branch.get('additionalProperties')
        for key, value in doc.items():
            if key in properties:
                found += undeclared_keys(value, properties[key], schema, f'{path}.{key}')
            elif isinstance(additional, dict):
                found += undeclared_keys(value, additional, schema, f'{path}.*')
            elif properties:
                found.append(f'{path}.{key}')
    elif isinstance(doc, list):
        items = next((b['items'] for b in branches if 'items' in b), None)
        if items is not None:
            for entry in doc:
                found += undeclared_keys(entry, items, schema, f'{path}[]')
    return found


@pytest.mark.parametrize('sample', SAMPLES, ids=[s.name for s in SAMPLES])
def test_sample_carries_no_key_the_schema_leaves_undeclared(sample):
    """
    Every key the tool writes has to be declared (#199).

    `stats.measured_instances` was rendered by --json and parsed back by diff since #179 while
    the schema never named it, and nothing failed: $defs.stats takes additional properties, so
    validation cannot tell a field the schema documents from one it forgot. The schema stays
    permissive on purpose - a document from a newer tool must not be rejected by an older
    schema - which is why the completeness of the declarations is a test here rather than
    `additionalProperties: false` there.
    """
    schema = load(SCHEMA)
    assert undeclared_keys(load(sample), schema, schema, sample.name) == []

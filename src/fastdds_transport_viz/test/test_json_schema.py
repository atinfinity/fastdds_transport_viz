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

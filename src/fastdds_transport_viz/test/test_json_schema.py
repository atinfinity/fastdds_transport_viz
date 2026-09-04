# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""The shipped sample documents must satisfy schema/transport_viz.schema.json."""
import json
import pathlib

import jsonschema
import pytest

REPO = pathlib.Path(__file__).resolve().parents[3]
SCHEMA = REPO / 'schema' / 'transport_viz.schema.json'
SAMPLES = sorted((REPO / 'web' / 'sample').glob('*.json'))


def load(path):
    with open(path) as f:
        return json.load(f)


def test_schema_itself_is_valid():
    jsonschema.Draft202012Validator.check_schema(load(SCHEMA))


@pytest.mark.parametrize('sample', SAMPLES, ids=[s.name for s in SAMPLES])
def test_sample_matches_schema(sample):
    assert SAMPLES, 'no sample documents found'
    jsonschema.Draft202012Validator(load(SCHEMA)).validate(load(sample))


def test_sample_descriptions_cover_used_codes():
    doc = load(REPO / 'web' / 'sample' / 'sample.json')
    used = set()
    for t in doc['topics']:
        used.update(t['unmatched_reasons'])
        for p in t['pairs']:
            used.update(p['reasons'])
            used.update(p['warnings'])
    assert used <= set(doc['reason_code_descriptions'])

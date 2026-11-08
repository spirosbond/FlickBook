"""Runs every tests/scenarios/*.json in each available display mode against its golden images."""
from pathlib import Path

import pytest
from conftest import MODES

from flicktwin import run_scenario

SCENARIOS = sorted((Path(__file__).parent / "scenarios").glob("*.json"))


@pytest.mark.parametrize("gray", MODES)
@pytest.mark.parametrize("scenario", SCENARIOS, ids=[p.stem for p in SCENARIOS])
def test_scenario(scenario, gray):
    run_scenario(scenario, grayscale=gray)

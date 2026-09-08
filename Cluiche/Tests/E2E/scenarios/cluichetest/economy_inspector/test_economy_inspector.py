"""EconomyTestStage — inspector data source E2E.

Verifies that EconomyInspectorModule activates its four data sources when
EconomyTestStageModule calls Bind(), and that the stage reaches all five
game checkpoints while the sources are live.

Topics asserted:
  economy.schema      — schema name + resources array
  economy.instances   — instances array with per-resource values
  economy.modifiers   — instances key exists
  economy.events      — events key exists (ring buffer or delta)

Checkpoints (in approximate firing order):
  economy.modifier_activates      10s  — frame 60 fires market_bonus
  economy.event_count_nonzero      5s  — any observer event fired
  economy.first_transfer          10s  — first gold deposit
  economy.treasury_above_500      20s  — treasury accumulates 500 gold
  economy.consumer_spent          25s  — consumer drains 30 gold
"""

_STAGE = "EconomyTestStage"

_TOPICS = [
    "economy.schema",
    "economy.instances",
    "economy.modifiers",
    "economy.events",
]

# Ordered roughly by when they fire; all timeouts are from stage start.
_CHECKPOINTS = [
    ("economy.modifier_activates",  10.0),
    ("economy.event_count_nonzero",  5.0),
    ("economy.first_transfer",      10.0),
    ("economy.treasury_above_500",  20.0),
    ("economy.consumer_spent",      25.0),
]


def test_economy_inspector_sources(dia_client):
    """Navigate to EconomyTestStage; assert inspector topics deliver valid payloads."""
    dia_client.navigate_to(_STAGE)
    try:
        # Subscribe to all four topics before waiting so no update is missed.
        for topic in _TOPICS:
            dia_client.subscribe_topic(topic)

        # Wait for initial payloads from each source.
        payloads = {}
        for topic in _TOPICS:
            payloads[topic] = dia_client.wait_for_topic(topic, timeout_s=10.0)

        # --- economy.schema ---
        schema = payloads["economy.schema"]
        resources = schema.get("resources")
        assert isinstance(resources, list) and len(resources) > 0, \
            f"economy.schema: expected non-empty 'resources' array, got: {schema}"
        assert "resource_name" in resources[0], \
            f"economy.schema: first resource missing 'resource_name', got: {resources[0]}"

        # --- economy.instances ---
        instances_payload = payloads["economy.instances"]
        instances = instances_payload.get("instances")
        assert isinstance(instances, list) and len(instances) > 0, \
            f"economy.instances: expected non-empty 'instances' array, got: {instances_payload}"

        # --- economy.modifiers ---
        modifiers_payload = payloads["economy.modifiers"]
        assert "instances" in modifiers_payload, \
            f"economy.modifiers: expected 'instances' key, got: {modifiers_payload}"

        # --- economy.events ---
        events_payload = payloads["economy.events"]
        assert "events" in events_payload, \
            f"economy.events: expected 'events' key, got: {events_payload}"

        # Assert all game checkpoints pass (proves the stage ran economy logic
        # while the inspector sources were active).
        for cp, timeout in _CHECKPOINTS:
            result = dia_client.poll_checkpoint(cp, timeout_s=timeout)
            assert result["passed"], f"{cp} failed: {result['message']}"

    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")

# Tools/orchestrator/scenarios/cluichetest/behaviourtree/test_bt_e2e.py

def test_behaviour_tree_guard_patrol_chase(dia_client):
    dia_client.navigate_to("BehaviourTreeTestStage")

    r = dia_client.poll_checkpoint("bt.patrol_started", timeout_s=5.0)
    assert r["passed"], f"Guards never started patrolling: {r['message']}"

    r = dia_client.poll_checkpoint("bt.chase_triggered", timeout_s=10.0)
    assert r["passed"], f"No guard entered chase: {r['message']}"

    r = dia_client.poll_checkpoint("bt.parallel_fired", timeout_s=12.0)
    assert r["passed"], f"Parallel node did not fire both actions: {r['message']}"

    r = dia_client.poll_checkpoint("bt.multi_state_divergence", timeout_s=15.0)
    assert r["passed"], f"Guards never diverged to different states: {r['message']}"

    r = dia_client.poll_checkpoint("bt.target_lost", timeout_s=18.0)
    assert r["passed"], f"Guard never lost target and returned to patrol: {r['message']}"

    metrics = dia_client.query_metrics("cluichetest.bt.*")
    assert metrics["cluichetest.bt.total_chases"] >= 1
    assert metrics["cluichetest.bt.total_patrol_waypoints"] >= 3
    assert metrics["cluichetest.bt.decorator_cycles"] >= 3

    # Determinism: second pass must match frame count
    first_frames = metrics["cluichetest.bt.total_frames"]
    dia_client.navigate_to("Boot")
    dia_client.navigate_to("BehaviourTreeTestStage")
    dia_client.poll_checkpoint("bt.target_lost", timeout_s=25.0)
    metrics2 = dia_client.query_metrics("cluichetest.bt.*")
    assert metrics2["cluichetest.bt.total_frames"] == first_frames, \
        f"Non-deterministic: {first_frames} vs {metrics2['cluichetest.bt.total_frames']}"

    dia_client.navigate_to("Boot")

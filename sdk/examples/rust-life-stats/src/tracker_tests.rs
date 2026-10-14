use super::{DeathDetails, Tracker, display_text};

fn lines(values: &[Vec<u8>]) -> Vec<&str> {
    values
        .iter()
        .map(|line| std::str::from_utf8(line).unwrap())
        .collect()
}

#[test]
fn damage_kills_and_final_damage_update() {
    let mut tracker = Tracker::default();
    tracker.observe(1, true, 0.0, 100);
    tracker.damage(25, 10, 2, 0.5);
    tracker.death(1, 2, b"Alice", b"ak47", true, 1.0, DeathDetails::default());
    tracker.death(3, 1, b"Bob", b"deagle", false, 2.0, DeathDetails::default());
    tracker.damage(75, 5, 2, 2.05);
    tracker.tick(0.1);
    assert!(tracker.take_report().console.is_empty());
    tracker.tick(0.16);
    let report = tracker.take_report();
    assert_eq!(
        lines(&report.chat),
        [
            "[Life Stats] Life ended. Damage taken: 100 HP, 15 armor. Kills: 1.",
            "[Life Stats] Killed by Bob (deagle).",
            "[Life Stats] Killed Alice (ak47, headshot).",
        ]
    );
    assert_eq!(
        lines(&report.console),
        [
            "[Life Stats] Life ended. Damage taken: 100 HP, 15 armor. Kills: 1.",
            "[Life Stats] Bullet damage -25 HP, -10 armor (-00:01.500)",
            "[Life Stats] Killed Alice (ak47, headshot) (-00:01.000)",
            "[Life Stats] Damage from Bob (deagle) -75 HP, -5 armor (Death)",
        ]
    );
    tracker.round_end(3.0);
    tracker.tick(1.0);
    assert!(tracker.take_report().console.is_empty());
}

#[test]
fn fatal_overkill_uses_health_before_damage() {
    let mut tracker = Tracker::default();
    tracker.observe(1, true, 1.0, 100);
    tracker.health(4, 10.0, true);
    tracker.damage(96, 0, 64, 10.0);
    tracker.health(0, 15.405, true);
    tracker.death(
        1,
        1,
        b"Self",
        b"grenade",
        false,
        15.405,
        DeathDetails::default(),
    );
    tracker.damage(95, 0, 64, 15.405);
    tracker.tick(0.3);
    assert_eq!(
        lines(&tracker.take_report().console),
        [
            "[Life Stats] Life ended. Damage taken: 100 HP, 0 armor. Overkill: 91 HP. Kills: 0.",
            "[Life Stats] Explosion damage -96 HP (-00:05.405)",
            "[Life Stats] Self-inflicted damage (grenade) -4 HP (91 HP overkill) (Death)",
        ]
    );
}

#[test]
fn healing_does_not_turn_into_overkill() {
    let mut tracker = Tracker::default();
    tracker.observe(1, true, 0.0, 100);
    tracker.damage(80, 0, 2, 1.0);
    tracker.health(20, 1.0, true);
    tracker.health(50, 2.0, true);
    tracker.damage(50, 0, 2, 3.0);
    tracker.death(2, 1, b"Bob", b"ak47", false, 3.0, DeathDetails::default());
    tracker.tick(0.3);
    assert_eq!(
        tracker.take_report().console[0],
        b"[Life Stats] Life ended. Damage taken: 130 HP, 0 armor. Kills: 0."
    );
}

#[test]
fn nonfatal_hit_does_not_borrow_later_suicide_cause() {
    let mut tracker = Tracker::default();
    tracker.observe(1, true, 0.0, 100);
    tracker.damage(30, 0, 0, 1.0);
    tracker.death(1, 1, b"Self", b"world", false, 1.1, DeathDetails::default());
    tracker.tick(0.3);
    assert_eq!(
        lines(&tracker.take_report().console),
        [
            "[Life Stats] Life ended. Damage taken: 30 HP, 0 armor. Kills: 0.",
            "[Life Stats] Damage taken -30 HP (-00:00.100)",
            "[Life Stats] Self-inflicted death (world).",
        ]
    );
}

#[test]
fn stale_alive_snapshot_and_hud_reset_do_not_confirm_respawn() {
    for reset_hud in [false, true] {
        let mut tracker = Tracker::default();
        tracker.observe(1, true, 0.0, 100);
        tracker.death(2, 1, b"Bob", b"ak47", false, 1.0, DeathDetails::default());
        if reset_hud {
            tracker.reset_hud();
        }
        tracker.observe(1, true, 1.1, 100);
        assert!(tracker.take_report().console.is_empty());
        tracker.damage(100, 0, 2, 1.1);
        tracker.tick(0.3);
        let report = tracker.take_report();
        assert_eq!(report.console.len(), 2);
        assert_eq!(
            report.console[1],
            b"[Life Stats] Damage from Bob (ak47) -100 HP (Death)"
        );
    }
}

#[test]
fn round_report_is_once_and_next_life_starts_clean() {
    let mut tracker = Tracker::default();
    tracker.observe(1, true, 0.0, 100);
    tracker.damage(10, 1, 2, 0.125);
    tracker.round_end(1.0);
    tracker.tick(0.3);
    let report = tracker.take_report();
    assert_eq!(report.console.len(), 2);
    assert_eq!(
        report.console[1],
        b"[Life Stats] Bullet damage -10 HP, -1 armor (-00:00.875)"
    );
    tracker.observe(1, true, 2.0, 90);
    tracker.round_end(2.0);
    tracker.tick(0.3);
    assert!(tracker.take_report().console.is_empty());
    tracker.new_round();
    tracker.reset_hud();
    tracker.observe(1, true, 4.0, 100);
    tracker.round_end(5.0);
    tracker.tick(0.3);
    assert_eq!(
        lines(&tracker.take_report().chat),
        ["[Life Stats] Round ended. Damage taken: 0 HP, 0 armor. Kills: 0."]
    );
}

#[test]
fn missing_round_end_does_not_invent_a_late_report() {
    let mut tracker = Tracker::default();
    tracker.observe(1, true, 0.0, 100);
    tracker.damage(20, 0, 0, 0.5);
    tracker.new_round();
    assert!(tracker.take_report().console.is_empty());
    tracker.reset_hud();
    tracker.observe(1, true, 1.0, 100);
    tracker.reset_hud();
    assert!(tracker.take_report().console.is_empty());
}

#[test]
fn spectators_unrelated_deaths_and_world_deaths_do_not_invent_attribution() {
    let mut tracker = Tracker::default();
    tracker.observe(1, false, 0.0, -1);
    tracker.reset_hud();
    tracker.observe(1, false, 1.0, -1);
    tracker.damage(100, 0, 0, 1.0);
    tracker.round_end(1.0);
    tracker.tick(0.3);
    assert!(tracker.take_report().console.is_empty());
    tracker.new_round();
    tracker.observe(1, true, 3.0, -1);
    tracker.death(2, 3, b"Other", b"ak47", true, 3.0, DeathDetails::default());
    tracker.death(
        0,
        1,
        b"",
        b"worldspawn",
        false,
        4.0,
        DeathDetails::default(),
    );
    tracker.tick(0.3);
    assert_eq!(
        lines(&tracker.take_report().chat),
        ["[Life Stats] Life ended. Damage taken: 0 HP, 0 armor. Kills: 0."]
    );
}

#[test]
fn suicides_and_posthumous_kills_do_not_count() {
    let mut tracker = Tracker::default();
    tracker.observe(1, true, 0.0, -1);
    tracker.death(
        1,
        1,
        b"Self",
        b"hegrenade",
        false,
        1.0,
        DeathDetails::default(),
    );
    tracker.death(
        1,
        2,
        b"Late",
        b"hegrenade",
        false,
        1.1,
        DeathDetails::default(),
    );
    tracker.tick(0.3);
    assert_eq!(
        lines(&tracker.take_report().chat),
        [
            "[Life Stats] Life ended. Damage taken: 0 HP, 0 armor. Kills: 0.",
            "[Life Stats] Self-inflicted death (hegrenade).",
        ]
    );
    tracker.reset();
    tracker.tick(0.3);
    assert!(tracker.take_report().console.is_empty());
}

#[test]
fn names_cannot_inject_lines_or_formatting() {
    let mut tracker = Tracker::default();
    tracker.observe(1, true, 0.0, -1);
    tracker.death(
        1,
        2,
        b"Alice\n%s1\x03",
        b"ak47",
        false,
        1.0,
        DeathDetails::default(),
    );
    tracker.round_end(2.0);
    tracker.tick(0.3);
    let report = tracker.take_report();
    assert_eq!(report.chat[1], b"[Life Stats] Killed Alice  s1  (ak47).");
    assert_eq!(display_text("абв".as_bytes(), 3), "а".as_bytes());
    assert_eq!(display_text(b"A\xffB", 48), b"A\xffB");
}

#[test]
fn assists_do_not_include_suicides_posthumous_assists_or_other_lives() {
    let extras = || DeathDetails {
        assister: 1,
        assister_name: b"Self".to_vec(),
        tags: vec!["flash assist"],
    };
    let mut tracker = Tracker::default();
    tracker.observe(1, true, 0.0, 100);
    tracker.death(2, 2, b"Alice", b"grenade", false, 1.0, extras());
    tracker.death(0, 2, b"Alice", b"world", false, 1.0, extras());
    tracker.death(2, 3, b"Bob", b"ak47", false, 2.0, extras());
    tracker.death(2, 1, b"Alice", b"ak47", false, 3.0, DeathDetails::default());
    tracker.death(2, 3, b"Bob", b"ak47", false, 3.1, extras());
    tracker.tick(0.3);
    assert_eq!(
        tracker.take_report().console[0],
        b"[Life Stats] Life ended. Damage taken: 0 HP, 0 armor. Kills: 0. Assists: 1."
    );
    tracker.new_round();
    tracker.observe(1, true, 5.0, 100);
    tracker.round_end(6.0);
    tracker.tick(0.3);
    assert_eq!(
        tracker.take_report().console[0],
        b"[Life Stats] Round ended. Damage taken: 0 HP, 0 armor. Kills: 0."
    );
}

#[test]
fn invalid_damage_and_elapsed_values_do_not_change_reports() {
    let mut tracker = Tracker::default();
    tracker.observe(1, true, 0.0, 100);
    for (health, armor, time) in [
        (-1, 0, 1.0),
        (256, 0, 1.0),
        (0, -1, 1.0),
        (0, 256, 1.0),
        (10, 0, f64::NAN),
    ] {
        tracker.damage(health, armor, 2, time);
    }
    tracker.round_end(2.0);
    for elapsed in [0.0, -1.0, f64::NAN, f64::INFINITY] {
        tracker.tick(elapsed);
    }
    assert!(tracker.take_report().console.is_empty());
    tracker.tick(0.25);
    assert_eq!(
        lines(&tracker.take_report().console),
        ["[Life Stats] Round ended. Damage taken: 0 HP, 0 armor. Kills: 0."]
    );
}

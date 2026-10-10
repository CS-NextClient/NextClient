mod tracker;

use nc::{Host, Plugin, Result, Session};
use nextclient_plugin as nc;
use serde_json::Value;
use std::collections::VecDeque;
use tracker::{DeathDetails, Tracker};

const KILL_TAGS: [(&str, &str); 9] = [
    ("killer_blind", "blinded killer"),
    ("noscope", "no-scope"),
    ("penetrated", "through wall"),
    ("through_smoke", "through smoke"),
    ("assisted_flash", "flash assist"),
    ("domination_began", "domination began"),
    ("domination", "domination"),
    ("revenge", "revenge"),
    ("in_air", "airborne killer"),
];

struct LifeStats {
    tracker: Tracker,
    names: [Vec<u8>; 33],
    chat: VecDeque<Vec<u8>>,
    clock: f64,
    next_chat: f64,
    game_time: f64,
    discard_backlog: bool,
    await_boundary: bool,
    chat_enabled: bool,
}

impl Default for LifeStats {
    fn default() -> Self {
        Self {
            tracker: Tracker::default(),
            names: std::array::from_fn(|_| Vec::new()),
            chat: VecDeque::new(),
            clock: 0.0,
            next_chat: 0.0,
            game_time: 0.0,
            discard_backlog: false,
            await_boundary: false,
            chat_enabled: false,
        }
    }
}

fn colored(line: &[u8]) -> Vec<u8> {
    match line.strip_prefix(b"[Life Stats]") {
        Some(rest) => [b"\x04[Life Stats]\x01".as_slice(), rest].concat(),
        None => line.to_vec(),
    }
}

fn integer(data: &Value, key: &str) -> std::result::Result<i32, nc::Error> {
    data.get(key)
        .and_then(Value::as_i64)
        .and_then(|v| i32::try_from(v).ok())
        .ok_or(nc::Error)
}

fn boolean(data: &Value, key: &str) -> std::result::Result<bool, nc::Error> {
    data.get(key).and_then(Value::as_bool).ok_or(nc::Error)
}

fn string<'a>(data: &'a Value, key: &str) -> std::result::Result<&'a str, nc::Error> {
    data.get(key).and_then(Value::as_str).ok_or(nc::Error)
}

impl LifeStats {
    fn reset(&mut self) {
        self.tracker.reset();
        for name in &mut self.names {
            name.clear();
        }
        self.chat.clear();
        self.next_chat = self.clock;
    }

    fn observe(&mut self, host: &Host<'_>, event_time: Option<f64>) {
        if let Some(session) = host.session() {
            self.game_time = f64::from(session.time);
        }
        if let Some(player) = host.player() {
            self.tracker.observe(
                player.index,
                (event_time.is_some() || player.health > 0) && player.flags & nc::ACTIVE != 0,
                event_time.unwrap_or(self.game_time),
                if event_time.is_some() {
                    -1
                } else {
                    player.health
                },
            );
        }
    }

    fn name(&mut self, host: &Host<'_>, index: i32) -> Vec<u8> {
        if !(1..=32).contains(&index) {
            return Vec::new();
        }
        if let Some(info) = host.player_info(index) {
            let length = info
                .name
                .iter()
                .position(|&c| c == 0)
                .unwrap_or(info.name.len());
            self.names[index as usize] = info.name[..length].to_vec();
        }
        self.names[index as usize].clone()
    }
}

impl Plugin for LifeStats {
    fn load(&mut self, host: &Host<'_>) -> Result {
        host.plugin_choice(
            "output",
            "Output",
            "Вывод",
            0,
            "Console\nConsole + Chat",
            "Консоль\nКонсоль + чат",
        )?;
        self.chat_enabled = host.setting("output", 0) == 1;
        self.reset();
        for name in [
            "player.damage",
            "player.health",
            "player.death",
            "round.end",
            "round.start",
            "hud.reset",
            "hud.init",
            "connection.changed",
            "map.changed",
            "player.left",
            "player.joined",
        ] {
            host.subscribe_event(name, true)?;
        }
        Ok(())
    }

    fn setting_changed(&mut self, _host: &Host<'_>, id: &str, value: i32) -> Result {
        if id == "output" {
            self.chat_enabled = value == 1;
            if !self.chat_enabled {
                self.chat.clear();
            }
            self.next_chat = self.clock;
        }
        Ok(())
    }

    fn event(&mut self, host: &Host<'_>, name: &str, json: &str) -> Result {
        let data: Value = serde_json::from_str(json).map_err(|_| nc::Error)?;
        data.as_object().ok_or(nc::Error)?;
        if name == "sdk.overflow" {
            self.reset();
            self.discard_backlog = true;
            self.await_boundary = true;
            let warning = colored(
                b"[Life Stats] Events were lost; statistics resume at the next spawn or round.",
            );
            let _ = host.console_print_bytes(&warning);
            if self.chat_enabled {
                self.chat.push_back(warning);
            }
            return Ok(());
        }
        // The overflow notice precedes surviving old events, including stale reset boundaries.
        if self.discard_backlog {
            return Ok(());
        }
        if self.await_boundary {
            if !matches!(
                name,
                "hud.reset" | "hud.init" | "map.changed" | "round.start"
            ) {
                return Ok(());
            }
            self.reset();
            self.await_boundary = false;
        }
        match name {
            "hud.init" | "map.changed" => {
                self.reset();
                return Ok(());
            }
            "connection.changed" => {
                if !boolean(&data, "connected")? {
                    self.reset();
                }
                return Ok(());
            }
            "player.left" | "player.joined" => {
                let index = integer(&data, "index")?;
                if (1..=32).contains(&index) {
                    self.names[index as usize] = if name == "player.joined" {
                        string(&data, "name")?.as_bytes().to_vec()
                    } else {
                        Vec::new()
                    };
                }
                return Ok(());
            }
            _ => {}
        }
        let time = match data.get("time") {
            Some(value) => value.as_f64().ok_or(nc::Error)?,
            None => self.game_time,
        };
        if name == "hud.reset" {
            if self.tracker.local() == 0 {
                self.observe(host, None);
            }
            self.tracker.reset_hud();
            return Ok(());
        }
        if self.tracker.local() == 0 {
            self.observe(host, Some(time));
        }
        match name {
            "player.damage" => {
                let bits = match data.get("bits") {
                    Some(value) => value
                        .as_u64()
                        .and_then(|v| u32::try_from(v).ok())
                        .ok_or(nc::Error)?,
                    None => 0,
                };
                self.tracker.damage(
                    integer(&data, "health")?,
                    integer(&data, "armor")?,
                    bits,
                    time,
                );
            }
            "player.health" if data.get("health").is_some() => {
                let active = host
                    .player()
                    .is_some_and(|player| player.flags & nc::ACTIVE != 0);
                self.tracker.health(integer(&data, "health")?, time, active);
            }
            "player.death" => {
                let killer = integer(&data, "killer")?;
                let victim = integer(&data, "victim")?;
                let other = if victim == self.tracker.local() {
                    killer
                } else {
                    victim
                };
                let mut extras = DeathDetails::default();
                if data.get("assister").is_some() {
                    extras.assister = integer(&data, "assister")?;
                }
                extras.assister_name = self.name(host, extras.assister);
                if let Some(details) = data.get("kill_details") {
                    details.as_object().ok_or(nc::Error)?;
                    for (key, text) in KILL_TAGS {
                        let enabled = if details.get(key).is_some() {
                            boolean(details, key)?
                        } else {
                            false
                        };
                        let domination_began =
                            if key == "domination" && details.get("domination_began").is_some() {
                                boolean(details, "domination_began")?
                            } else {
                                false
                            };
                        if enabled && !domination_began {
                            extras.tags.push(text);
                        }
                    }
                }
                let name = self.name(host, other);
                self.tracker.death(
                    killer,
                    victim,
                    &name,
                    string(&data, "weapon")?.as_bytes(),
                    boolean(&data, "headshot")?,
                    time,
                    extras,
                );
            }
            "round.end" => self.tracker.round_end(time),
            "round.start" => self.tracker.new_round(),
            _ => {}
        }
        Ok(())
    }

    fn frame(&mut self, host: &Host<'_>, session: &Session) -> Result {
        self.clock += f64::from(session.frame_time);
        self.game_time = f64::from(session.time);
        if session.flags & nc::CONNECTED == 0 {
            self.reset();
            self.discard_backlog = false;
            self.await_boundary = false;
            return Ok(());
        }
        let backlog = host
            .query_interface("nextclient.events", 1)
            .and_then(|extension| extension.call("stats", "{}").ok())
            .and_then(|json| serde_json::from_str::<Value>(&json).ok())
            .and_then(|data| data.get("queued").and_then(Value::as_u64))
            .is_none_or(|queued| queued != 0);
        if !backlog {
            self.discard_backlog = false;
            if !self.await_boundary {
                self.observe(host, None);
                self.tracker.tick(f64::from(session.frame_time));
            }
        }
        let report = self.tracker.take_report();
        for line in report.console {
            let _ = host.console_print_bytes(&colored(&line));
        }
        if self.chat_enabled {
            for line in report.chat {
                if self.chat.len() < 1024 {
                    self.chat.push_back(colored(&line));
                }
            }
        }
        if let Some(line) = self.chat.front() {
            if self.clock >= self.next_chat {
                if host.chat_print_bytes(line).is_ok() {
                    self.chat.pop_front();
                }
                self.next_chat = self.clock + 0.6;
            }
        }
        for index in 1..=session.max_clients.min(32) {
            self.name(host, index);
        }
        Ok(())
    }
}

nc::export_plugin!(
    LifeStats,
    r#"{
  "schema":1,"id":"org.nextclient.life_stats","name":"Life Stats","author":"NextClient",
  "description":"Shows damage taken, kills, assists and your killer in the console, with optional local chat, after death or round end.",
  "translations":{"ru":{"name":"Статистика жизни","description":"Показывает полученный урон, убийства, помощь в убийствах и вашего убийцу в консоли после смерти или окончания раунда; по выбору добавляет вывод в локальный чат."}},
  "version":"1.0.0","sdk":"1.0.0","abi":1,"api":1,"compatibility_revision":1,
  "permissions":["chat.print"]
}"#
);

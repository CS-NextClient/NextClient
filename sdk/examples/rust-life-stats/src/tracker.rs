#[derive(Default)]
pub struct Report {
    pub console: Vec<Vec<u8>>,
    pub chat: Vec<Vec<u8>>,
}

pub struct DeathDetails {
    pub assister: i32,
    pub assister_name: Vec<u8>,
    pub tags: Vec<&'static str>,
}

impl Default for DeathDetails {
    fn default() -> Self {
        Self {
            assister: -1,
            assister_name: Vec::new(),
            tags: Vec::new(),
        }
    }
}

#[derive(Default, PartialEq, Eq, Clone, Copy)]
enum End {
    #[default]
    None,
    Death,
    Round,
}

#[derive(Default)]
struct Entry {
    time: f64,
    damage: bool,
    lethal: bool,
    health: i32,
    armor: i32,
    overkill: i32,
    bits: u32,
    text: Vec<u8>,
}

pub struct Tracker {
    local: i32,
    remaining_health: i32,
    reported_health: i32,
    alive: bool,
    active: bool,
    round_closed: bool,
    hud_reset: bool,
    health: u64,
    armor: u64,
    overkill: u64,
    kills: u64,
    assists: u64,
    end: End,
    end_time: f64,
    pending_elapsed: f64,
    killer: Vec<u8>,
    killer_console: Vec<u8>,
    fatal_source: Vec<u8>,
    entries: Vec<Entry>,
    victims: Vec<Vec<u8>>,
    reports: Report,
}

impl Default for Tracker {
    fn default() -> Self {
        Self {
            local: 0,
            remaining_health: -1,
            reported_health: -1,
            alive: false,
            active: false,
            round_closed: false,
            hud_reset: false,
            health: 0,
            armor: 0,
            overkill: 0,
            kills: 0,
            assists: 0,
            end: End::None,
            end_time: 0.0,
            pending_elapsed: 0.0,
            killer: Vec::new(),
            killer_console: Vec::new(),
            fatal_source: Vec::new(),
            entries: Vec::new(),
            victims: Vec::new(),
            reports: Report::default(),
        }
    }
}

// Player names are copied engine bytes and need not be UTF-8.
pub fn display_text(value: &[u8], limit: usize) -> Vec<u8> {
    let mut result: Vec<u8> = value
        .iter()
        .map(|&c| {
            if c < 32 || c == 127 || c == b'%' {
                b' '
            } else {
                c
            }
        })
        .collect();
    if result.len() > limit {
        let mut cut = limit;
        while cut > 0 && result[cut] & 0xc0 == 0x80 {
            cut -= 1;
        }
        result.truncate(cut);
    }
    result
}

fn offset(seconds: f64) -> String {
    let ms = (seconds.clamp(0.0, 86400.0) * 1000.0).round() as u64;
    format!(
        "(-{:02}:{:02}.{:03})",
        ms / 60000,
        ms / 1000 % 60,
        ms % 1000
    )
}

fn damage_type(bits: u32) -> &'static [u8] {
    if bits & (1 << 5) != 0 {
        b"Fall damage"
    } else if bits & ((1 << 6) | (1 << 24)) != 0 {
        b"Explosion damage"
    } else if bits & (1 << 14) != 0 {
        b"Drowning damage"
    } else if bits & (1 << 3) != 0 {
        b"Burn damage"
    } else if bits & (1 << 1) != 0 {
        b"Bullet damage"
    } else {
        b"Damage taken"
    }
}

impl Tracker {
    pub fn local(&self) -> i32 {
        self.local
    }

    fn begin(&mut self) {
        self.active = true;
        self.hud_reset = false;
        self.end = End::None;
        self.health = 0;
        self.armor = 0;
        self.overkill = 0;
        self.kills = 0;
        self.assists = 0;
        self.remaining_health = -1;
        self.reported_health = -1;
        self.killer.clear();
        self.killer_console.clear();
        self.fatal_source.clear();
        self.entries.clear();
        self.victims.clear();
    }

    pub fn observe(&mut self, local: i32, alive: bool, time: f64, health: i32) {
        if !(1..=32).contains(&local) {
            return;
        }
        if self.local != local {
            self.reset();
            self.local = local;
        }
        if alive && !self.alive && !self.round_closed {
            if self.end != End::None {
                self.flush();
            }
            if !self.active {
                self.begin();
            }
        }
        if health >= 0 && (alive || health == 0) {
            self.update_health(health, time);
        }
        if !alive && self.alive {
            self.finish(End::Death, time);
        }
        self.alive = alive;
    }

    pub fn health(&mut self, health: i32, time: f64, active_player: bool) {
        if health < 0 {
            return;
        }
        if self.hud_reset {
            self.hud_reset = false;
            if health > 0
                && active_player
                && self.local != 0
                && !self.round_closed
                && (!self.active || self.end == End::Death)
            {
                self.flush();
                self.begin();
                self.alive = true;
            }
        }
        self.update_health(health, time);
    }

    fn update_health(&mut self, health: i32, time: f64) {
        if !self.active {
            return;
        }
        if self.remaining_health < 0 && health > 0 {
            self.remaining_health = health;
        } else if self.reported_health >= 0
            && health > self.reported_health
            && self.end == End::None
        {
            self.remaining_health += health - self.reported_health;
        }
        self.reported_health = health;
        if health == 0 {
            self.finish(End::Death, time);
        }
    }

    pub fn damage(&mut self, health: i32, armor: i32, bits: u32, time: f64) {
        if !self.active
            || !(0..=255).contains(&health)
            || !(0..=255).contains(&armor)
            || !time.is_finite()
        {
            return;
        }
        let applied = if self.remaining_health >= 0 {
            health.min(self.remaining_health)
        } else {
            health
        };
        let overkill = health - applied;
        if self.remaining_health >= 0 {
            self.remaining_health -= applied;
        }
        self.health += applied as u64;
        self.armor += armor as u64;
        self.overkill += overkill as u64;
        if self.entries.len() < 4096 && (health != 0 || armor != 0) {
            self.entries.push(Entry {
                time,
                damage: true,
                lethal: health > 0 && self.remaining_health == 0,
                health: applied,
                armor,
                overkill,
                bits,
                ..Entry::default()
            });
        }
    }

    fn finish(&mut self, end: End, time: f64) {
        if !self.active || (self.end != End::None && !(self.end == End::Round && end == End::Death))
        {
            return;
        }
        self.end = end;
        self.end_time = time;
        self.pending_elapsed = 0.0;
    }

    pub fn death(
        &mut self,
        killer: i32,
        victim: i32,
        name: &[u8],
        weapon: &[u8],
        headshot: bool,
        time: f64,
        extras: DeathDetails,
    ) {
        if !self.active || !(1..=32).contains(&victim) || !(0..=32).contains(&killer) {
            return;
        }
        let display = display_text(name, 48);
        let mut details = display_text(weapon, 32);
        if headshot {
            if !details.is_empty() {
                details.extend_from_slice(b", ");
            }
            details.extend_from_slice(b"headshot");
        }
        if !details.is_empty() {
            details = [b" (".as_slice(), &details, b")"].concat();
        }
        let mut extra_text = Vec::new();
        for tag in extras.tags {
            if !extra_text.is_empty() {
                extra_text.extend_from_slice(b", ");
            }
            extra_text.extend(display_text(tag.as_bytes(), 48));
        }
        let assister = display_text(&extras.assister_name, 48);
        if extras.assister > 0
            && extras.assister <= 32
            && extras.assister != self.local
            && !assister.is_empty()
        {
            if !extra_text.is_empty() {
                extra_text.extend_from_slice(b", ");
            }
            extra_text.extend_from_slice(b"assisted by ");
            extra_text.extend(assister);
        }
        let mut console_details = details.clone();
        if !extra_text.is_empty() {
            console_details.extend([b" [".as_slice(), &extra_text, b"]"].concat());
        }
        if victim == self.local {
            self.finish(End::Death, time);
            self.end_time = time;
            if killer == self.local {
                self.killer = [b"Self-inflicted death".as_slice(), &details, b"."].concat();
                self.killer_console =
                    [b"Self-inflicted death".as_slice(), &console_details, b"."].concat();
                self.fatal_source =
                    [b"Self-inflicted damage".as_slice(), &console_details].concat();
            } else if killer != 0 && !display.is_empty() {
                self.killer = [b"Killed by ".as_slice(), &display, &details, b"."].concat();
                self.killer_console =
                    [b"Killed by ".as_slice(), &display, &console_details, b"."].concat();
                self.fatal_source =
                    [b"Damage from ".as_slice(), &display, &console_details].concat();
            }
        } else if killer == self.local && self.end == End::None {
            self.kills += 1;
            if !display.is_empty() && self.victims.len() < 512 {
                self.victims
                    .push([b"Killed ".as_slice(), &display, &details, b"."].concat());
                if self.entries.len() < 4096 {
                    self.entries.push(Entry {
                        time,
                        text: [b"Killed ".as_slice(), &display, &console_details].concat(),
                        ..Entry::default()
                    });
                }
            }
        } else if extras.assister == self.local
            && killer != 0
            && killer != victim
            && self.end == End::None
        {
            self.assists += 1;
            if !display.is_empty() && self.victims.len() < 512 {
                self.victims
                    .push([b"Assisted in killing ".as_slice(), &display, &details, b"."].concat());
                if self.entries.len() < 4096 {
                    self.entries.push(Entry {
                        time,
                        text: [
                            b"Assisted in killing ".as_slice(),
                            &display,
                            &console_details,
                        ]
                        .concat(),
                        ..Entry::default()
                    });
                }
            }
        }
    }

    pub fn round_end(&mut self, time: f64) {
        self.finish(End::Round, time);
        self.round_closed = true;
    }

    fn flush(&mut self) {
        if !self.active || self.end == End::None {
            return;
        }
        let prefix = b"[Life Stats] ";
        let reason = if self.end == End::Round {
            "Round ended. "
        } else {
            "Life ended. "
        };
        let mut summary = format!(
            "[Life Stats] {reason}Damage taken: {} HP, {} armor.",
            self.health, self.armor
        );
        if self.overkill != 0 {
            summary += &format!(" Overkill: {} HP.", self.overkill);
        }
        summary += &format!(" Kills: {}.", self.kills);
        if self.assists != 0 {
            summary += &format!(" Assists: {}.", self.assists);
        }
        self.reports.chat.push(summary.as_bytes().to_vec());
        self.reports.console.push(summary.into_bytes());
        if !self.killer.is_empty() {
            self.reports
                .chat
                .push([prefix.as_slice(), &self.killer].concat());
        }
        for victim in &self.victims {
            self.reports.chat.push([prefix.as_slice(), victim].concat());
        }
        let fatal = if self.end == End::Death {
            self.entries
                .iter()
                .rposition(|entry| entry.lethal && (entry.time - self.end_time).abs() <= 0.5)
        } else {
            None
        };
        for (i, entry) in self.entries.iter().enumerate() {
            let mut line = prefix.to_vec();
            if entry.damage {
                line.extend_from_slice(if fatal == Some(i) && !self.fatal_source.is_empty() {
                    &self.fatal_source
                } else {
                    damage_type(entry.bits)
                });
                line.extend(format!(" -{} HP", entry.health).bytes());
                if entry.armor != 0 {
                    line.extend(format!(", -{} armor", entry.armor).bytes());
                }
                if entry.overkill != 0 {
                    line.extend(format!(" ({} HP overkill)", entry.overkill).bytes());
                }
            } else {
                line.extend_from_slice(&entry.text);
            }
            if fatal == Some(i) {
                line.extend_from_slice(b" (Death)");
            } else {
                line.extend(format!(" {}", offset(self.end_time - entry.time)).bytes());
            }
            self.reports.console.push(line);
        }
        if fatal.is_none() && !self.killer_console.is_empty() {
            self.reports
                .console
                .push([prefix.as_slice(), &self.killer_console].concat());
        }
        self.active = false;
    }

    pub fn tick(&mut self, elapsed: f64) {
        if self.end != End::None && elapsed.is_finite() && elapsed > 0.0 {
            self.pending_elapsed += elapsed;
            if self.pending_elapsed >= 0.25 {
                self.flush();
            }
        }
    }

    pub fn new_round(&mut self) {
        self.flush();
        self.active = false;
        self.alive = false;
        self.round_closed = false;
        self.hud_reset = true;
        self.end = End::None;
    }

    pub fn reset_hud(&mut self) {
        self.hud_reset = true;
    }

    pub fn reset(&mut self) {
        *self = Self::default();
    }

    pub fn take_report(&mut self) -> Report {
        std::mem::take(&mut self.reports)
    }
}

#[cfg(test)]
#[path = "tracker_tests.rs"]
mod tests;

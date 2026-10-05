use nextclient_plugin::{self as nc, Host, Plugin, Result};

#[cfg(feature = "abi-tests")]
include!(concat!(env!("OUT_DIR"), "/abi_layout.rs"));

#[cfg(feature = "abi-tests")]
#[unsafe(no_mangle)]
pub extern "C" fn nc_test_abi_layout() -> *const std::ffi::c_char {
    static LAYOUT: std::sync::OnceLock<std::ffi::CString> = std::sync::OnceLock::new();
    LAYOUT
        .get_or_init(|| std::ffi::CString::new(abi_layout()).unwrap())
        .as_ptr()
}

#[derive(Default)]
struct Events;
impl Plugin for Events {
    fn load(&mut self, host: &Host<'_>) -> Result {
        // The same versioned extension tables are usable without C++ or bindgen.
        let events = host
            .query_interface("nextclient.events", 1)
            .ok_or(nc::Error)?;
        let _stats = events.call("stats", "{}")?;
        let starts = host
            .store_get("starts")
            .and_then(|v| v.parse::<u64>().ok())
            .unwrap_or(0);
        host.store_set("starts", &starts.saturating_add(1).to_string())?;
        host.subscribe_event("player.health", true)?;
        host.register_command("status")?;
        Ok(())
    }
    fn event(&mut self, host: &Host<'_>, name: &str, json: &str) -> Result {
        if name == "player.health" {
            host.store_set("last_health", json)?;
        }
        Ok(())
    }
    fn console_command(&mut self, host: &Host<'_>, _: &str, _: &[&str]) -> Result {
        if let Some(connection) = host.game_data("connection", 0) {
            host.console_print(&connection)?;
        }
        Ok(())
    }
}
nc::export_plugin!(
    Events,
    r#"{
    "schema":1,"id":"org.nextclient.events","name":"Events example","author":"NextClient",
    "description":"Stores the last health event and a startup counter. Run nc.org.nextclient.events.status for connection information.",
    "translations":{"ru":{"name":"Пример событий","description":"Сохраняет последнее событие здоровья и число запусков. Команда nc.org.nextclient.events.status выводит состояние подключения."}},
    "version":"1.0.0","sdk":"1.0.0","abi":1,"api":1,"compatibility_revision":1,"permissions":[]
}"#
);

//! NextClient ABI 1. Plugin code is Rust-only; no bindings generator or C++
//! shim is required. The host calls callbacks on one game thread.
use std::{
    ffi::{CStr, CString, c_char, c_void},
    marker::PhantomData,
    rc::Rc,
};

pub const ABI: u32 = 1;
pub const API: u32 = 1;
pub const SDK_VERSION: &str = "1.0.0";
pub const UI_SETTINGS: u32 = 1;
pub const UI_DRAW: u32 = 2;
pub const UI_HIDE: u32 = 4;
pub const PLAYER_WRITE: u32 = 8;
pub const CVARS_READ: u32 = 16;
pub const CVARS_WRITE: u32 = 32;
pub const AUDIO_PLAY: u32 = 64;
pub const CVARS_CREATE: u32 = 128;
pub const CHAT_READ: u32 = 256;
pub const CHAT_SEND: u32 = 512;
pub const CONNECTION_CONNECT: u32 = 1024;
pub const CONNECTION_DISCONNECT: u32 = 2048;
pub const CHAT_PRINT: u32 = 4096;
pub const MESSAGES_READ: u32 = 8192;
pub const MESSAGES_FILTER: u32 = 16384;
pub const UI_WINDOWS: u32 = 32768;
pub const UI_INPUT: u32 = 65536;
pub const SERVICES_CALL: u32 = 131072;
pub const HUD: u32 = 1;
pub const CROSSHAIR: u32 = 2;
pub const HEALTH: u32 = 4;
pub const RADAR: u32 = 8;
pub const DEATH_NOTICES: u32 = 16;
pub const CONNECTED: u32 = 1;
pub const IN_GAME: u32 = 2;
pub const ENTITY_PLAYER: u32 = 1;
pub const ENTITY_LOCAL: u32 = 2;
pub const ENTITY_HEALTH: u32 = 4;
pub const VALID: u32 = 1;
pub const ACTIVE: u32 = 2;
pub const CAN_JUMP: u32 = 4;
pub const GROUNDED: u32 = 8;
pub const JUMP_HELD: u32 = 16;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct Command {
    pub size: u32,
    pub buttons: u32,
    pub view_angles: [f32; 3],
    pub forward_move: f32,
    pub side_move: f32,
    pub up_move: f32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct Player {
    pub size: u32,
    pub flags: u32,
    pub frame_time: f32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct PlayerState {
    pub size: u32,
    pub flags: u32,
    pub index: i32,
    pub health: i32,
    pub armor: i32,
    pub weapon_id: i32,
    pub weapons: u32,
    pub position: [f32; 3],
    pub velocity: [f32; 3],
    pub view_angles: [f32; 3],
    pub view_offset: [f32; 3],
    pub fov: f32,
    pub max_speed: f32,
    pub water_level: i32,
    pub move_type: i32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct Entity {
    pub size: u32,
    pub flags: u32,
    pub index: i32,
    pub model_index: i32,
    pub owner: i32,
    pub team: i32,
    pub health: i32,
    pub position: [f32; 3],
    pub angles: [f32; 3],
    pub velocity: [f32; 3],
    pub mins: [f32; 3],
    pub maxs: [f32; 3],
    pub sequence: i32,
    pub effects: i32,
    pub move_type: i32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct Weapon {
    pub size: u32,
    pub id: i32,
    pub owned: i32,
    pub clip: i32,
    pub reloading: i32,
    pub state: i32,
    pub next_primary_attack: f32,
    pub next_secondary_attack: f32,
    pub idle_time: f32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct DrawContext {
    pub size: u32,
    pub width: i32,
    pub height: i32,
    pub intermission: i32,
    pub time: f32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct Session {
    pub size: u32,
    pub flags: u32,
    pub max_clients: i32,
    pub width: i32,
    pub height: i32,
    pub time: f32,
    pub frame_time: f32,
    pub map: [u8; 128],
}
impl Default for Session {
    fn default() -> Self {
        Self {
            size: 0,
            flags: 0,
            max_clients: 0,
            width: 0,
            height: 0,
            time: 0.0,
            frame_time: 0.0,
            map: [0; 128],
        }
    }
}
impl Session {
    pub fn map_name(&self) -> String {
        copied_string(&self.map)
    }
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct PlayerInfo {
    pub size: u32,
    pub index: i32,
    pub ping: i32,
    pub packet_loss: i32,
    pub local: i32,
    pub spectator: i32,
    pub name: [u8; 128],
    pub model: [u8; 128],
}
impl Default for PlayerInfo {
    fn default() -> Self {
        Self {
            size: 0,
            index: 0,
            ping: 0,
            packet_loss: 0,
            local: 0,
            spectator: 0,
            name: [0; 128],
            model: [0; 128],
        }
    }
}
impl PlayerInfo {
    pub fn name(&self) -> String {
        copied_string(&self.name)
    }
    pub fn model(&self) -> String {
        copied_string(&self.model)
    }
}
fn copied_string(bytes: &[u8]) -> String {
    let length = bytes.iter().position(|b| *b == 0).unwrap_or(bytes.len());
    String::from_utf8_lossy(&bytes[..length]).into_owned()
}
#[repr(C)]
#[doc(hidden)]
pub struct RawControl {
    pub size: u32,
    pub id: *const c_char,
    pub tab: *const c_char,
    pub kind: u32,
    pub label_en: *const c_char,
    pub label_ru: *const c_char,
    pub initial: i32,
    pub minimum: i32,
    pub maximum: i32,
    pub choices_en: *const c_char,
    pub choices_ru: *const c_char,
}
#[repr(C)]
#[doc(hidden)]
pub struct RawHost {
    pub size: u32,
    pub abi: u32,
    pub api: u32,
    pub context: *mut c_void,
    pub log: unsafe extern "C" fn(*mut c_void, *const c_char),
    pub add_tab:
        unsafe extern "C" fn(*mut c_void, *const c_char, *const c_char, *const c_char) -> i32,
    pub add_control: unsafe extern "C" fn(*mut c_void, *const RawControl) -> i32,
    pub get_setting: unsafe extern "C" fn(*mut c_void, *const c_char, i32) -> i32,
    pub permissions: unsafe extern "C" fn(*mut c_void) -> u32,
    pub register_setting: unsafe extern "C" fn(*mut c_void, *const c_char, i32, i32, i32) -> i32,
    pub set_setting: unsafe extern "C" fn(*mut c_void, *const c_char, i32) -> i32,
    pub register_command: unsafe extern "C" fn(*mut c_void, *const c_char) -> i32,
    pub get_player: unsafe extern "C" fn(*mut c_void, *mut PlayerState) -> i32,
    pub get_entity: unsafe extern "C" fn(*mut c_void, i32, *mut Entity) -> i32,
    pub get_weapon: unsafe extern "C" fn(*mut c_void, i32, *mut Weapon) -> i32,
    pub read_cvar: unsafe extern "C" fn(*mut c_void, *const c_char, *mut c_char, u32) -> u32,
    pub write_cvar: unsafe extern "C" fn(*mut c_void, *const c_char, *const c_char) -> i32,
    pub hide_ui: unsafe extern "C" fn(*mut c_void, u32, i32) -> i32,
    pub draw_rect: unsafe extern "C" fn(*mut c_void, i32, i32, i32, i32, u32) -> i32,
    pub get_session: unsafe extern "C" fn(*mut c_void, *mut Session) -> i32,
    pub get_player_info: unsafe extern "C" fn(*mut c_void, i32, *mut PlayerInfo) -> i32,
    pub world_to_screen: unsafe extern "C" fn(*mut c_void, *const f32, *mut f32) -> i32,
    pub measure_text: unsafe extern "C" fn(*mut c_void, *const c_char, *mut i32, *mut i32) -> i32,
    pub draw_text: unsafe extern "C" fn(*mut c_void, i32, i32, *const c_char, u32) -> i32,
    pub play_sound: unsafe extern "C" fn(*mut c_void, *const c_char, f32) -> i32,
    pub console_print: unsafe extern "C" fn(*mut c_void, *const c_char) -> i32,
    pub subscribe_event: unsafe extern "C" fn(*mut c_void, *const c_char, i32) -> i32,
    pub game_data: unsafe extern "C" fn(*mut c_void, *const c_char, i32, *mut c_char, u32) -> u32,
    pub create_cvar: unsafe extern "C" fn(*mut c_void, *const c_char, *const c_char, i32) -> i32,
    pub watch_cvar: unsafe extern "C" fn(*mut c_void, *const c_char, i32) -> i32,
    pub send_chat: unsafe extern "C" fn(*mut c_void, *const c_char, i32) -> i32,
    pub connect: unsafe extern "C" fn(*mut c_void, *const c_char, u32) -> i32,
    pub disconnect: unsafe extern "C" fn(*mut c_void) -> i32,
    pub store_get: unsafe extern "C" fn(*mut c_void, *const c_char, *mut c_char, u32) -> u32,
    pub store_set: unsafe extern "C" fn(*mut c_void, *const c_char, *const c_char) -> i32,
    pub store_delete: unsafe extern "C" fn(*mut c_void, *const c_char) -> i32,
    pub store_keys: unsafe extern "C" fn(*mut c_void, *mut c_char, u32) -> u32,
    pub chat_print: unsafe extern "C" fn(*mut c_void, *const c_char) -> i32,
    pub query_interface:
        unsafe extern "C" fn(*mut c_void, *const c_char, u32) -> *const RawExtension,
}
pub type MessageFilter =
    unsafe extern "C" fn(*mut c_void, *const c_char, *const u8, u32, *mut u8, *mut u32) -> i32;
#[repr(C)]
pub struct RawExtension {
    pub size: u32,
    pub version: u32,
    pub call: unsafe extern "C" fn(*mut c_void, *const c_char, *const c_char) -> u64,
    pub read_result: unsafe extern "C" fn(*mut c_void, u64, *mut c_char, u32) -> u32,
    pub release_result: unsafe extern "C" fn(*mut c_void, u64),
    pub post: Option<unsafe extern "C" fn(u64, *const c_char) -> i32>,
    pub set_filter: Option<
        unsafe extern "C" fn(*mut c_void, *const c_char, Option<MessageFilter>, *mut c_void) -> i32,
    >,
}
/// A versioned, main-thread interface borrowed during a plugin callback.
pub struct Extension<'a> {
    raw: &'a RawExtension,
    context: *mut c_void,
    _thread: PhantomData<Rc<()>>,
}
/// May be copied into a worker. Stop and join plugin workers before unload.
#[derive(Clone, Copy)]
pub struct PostToken {
    token: u64,
    post: unsafe extern "C" fn(u64, *const c_char) -> i32,
}
impl PostToken {
    pub fn post(&self, json: &str) -> Result {
        let json = CString::new(json).map_err(|_| Error)?;
        status(unsafe { (self.post)(self.token, json.as_ptr()) })
    }
}
impl Extension<'_> {
    pub fn call(&self, operation: &str, json: &str) -> std::result::Result<String, Error> {
        let [operation, json] = strings([operation, json])?;
        let handle = unsafe { (self.raw.call)(self.context, operation.as_ptr(), json.as_ptr()) };
        if handle == 0 {
            return Err(Error);
        }
        let result = json_result(|out, size| unsafe {
            (self.raw.read_result)(self.context, handle, out, size)
        });
        unsafe { (self.raw.release_result)(self.context, handle) };
        result.ok_or(Error)
    }
    /// Use the token returned by nextclient.tasks/token. Stale tokens are rejected.
    pub fn post_token(&self, token: u64) -> Option<PostToken> {
        self.raw.post.map(|post| PostToken { token, post })
    }
    /// # Safety
    /// Callback/user must remain valid until removal/unload; catch all panics.
    /// Do not retain input bytes or write beyond the output capacity.
    pub unsafe fn set_filter(
        &self,
        name: &str,
        callback: Option<MessageFilter>,
        user: *mut c_void,
    ) -> Result {
        let name = CString::new(name).map_err(|_| Error)?;
        let set = self.raw.set_filter.ok_or(Error)?;
        status(unsafe { set(self.context, name.as_ptr(), callback, user) })
    }
}
#[repr(C)]
#[doc(hidden)]
pub struct RawPlugin {
    pub size: u32,
    pub abi: u32,
    pub api: u32,
    pub load: extern "C" fn(*const RawHost) -> i32,
    pub unload: extern "C" fn(),
    pub command: extern "C" fn(*mut Command, *const Player) -> i32,
    pub setting_changed: extern "C" fn(*const c_char, i32) -> i32,
    pub action: extern "C" fn(*const c_char) -> i32,
    pub console_command: extern "C" fn(*const c_char, i32, *const *const c_char) -> i32,
    pub draw: extern "C" fn(*const DrawContext) -> i32,
    pub frame: extern "C" fn(*const Session) -> i32,
    pub event: extern "C" fn(*const c_char, *const c_char) -> i32,
}

#[derive(Debug)]
pub struct Error;
pub type Result = std::result::Result<(), Error>;
#[derive(Clone, Copy)]
pub enum Kind {
    Checkbox = 1,
    Slider = 2,
    Choice = 3,
    Button = 4,
}
pub struct Control<'a> {
    pub id: &'a str,
    pub tab: &'a str,
    pub kind: Kind,
    pub en: &'a str,
    pub ru: &'a str,
    pub initial: i32,
    pub min: i32,
    pub max: i32,
    pub choices_en: &'a str,
    pub choices_ru: &'a str,
}
/// A borrowed, main-thread-only host. Do not retain it after `unload`.
/// The SDK keeps it inaccessible to plugin code outside its callback lifetime.
pub struct Host<'a> {
    raw: &'a RawHost,
    _thread: PhantomData<Rc<()>>,
}
impl Host<'_> {
    pub fn query_interface(&self, name: &str, version: u32) -> Option<Extension<'_>> {
        let name = CString::new(name).ok()?;
        let raw = unsafe {
            (self.raw.query_interface)(self.raw.context, name.as_ptr(), version).as_ref()?
        };
        if raw.size < size_of::<RawExtension>() as u32 || raw.version != version {
            return None;
        }
        Some(Extension {
            raw,
            context: self.raw.context,
            _thread: PhantomData,
        })
    }
    pub fn permissions(&self) -> u32 {
        unsafe { (self.raw.permissions)(self.raw.context) }
    }
    pub fn register_setting(&self, id: &str, initial: i32, min: i32, max: i32) -> Result {
        let id = CString::new(id).map_err(|_| Error)?;
        status(unsafe {
            (self.raw.register_setting)(self.raw.context, id.as_ptr(), initial, min, max)
        })
    }
    pub fn set_setting(&self, id: &str, value: i32) -> Result {
        let id = CString::new(id).map_err(|_| Error)?;
        status(unsafe { (self.raw.set_setting)(self.raw.context, id.as_ptr(), value) })
    }
    /// Registers `nc.<plugin-id>.<id>`; available during load only.
    pub fn register_command(&self, id: &str) -> Result {
        let id = CString::new(id).map_err(|_| Error)?;
        status(unsafe { (self.raw.register_command)(self.raw.context, id.as_ptr()) })
    }
    pub fn player(&self) -> Option<PlayerState> {
        let mut value = PlayerState {
            size: size_of::<PlayerState>() as u32,
            ..Default::default()
        };
        (unsafe { (self.raw.get_player)(self.raw.context, &mut value) } != 0).then_some(value)
    }
    pub fn entity(&self, index: i32) -> Option<Entity> {
        let mut value = Entity {
            size: size_of::<Entity>() as u32,
            ..Default::default()
        };
        (unsafe { (self.raw.get_entity)(self.raw.context, index, &mut value) } != 0)
            .then_some(value)
    }
    pub fn weapon(&self, id: i32) -> Option<Weapon> {
        let mut value = Weapon {
            size: size_of::<Weapon>() as u32,
            ..Default::default()
        };
        (unsafe { (self.raw.get_weapon)(self.raw.context, id, &mut value) } != 0).then_some(value)
    }
    pub fn read_cvar(&self, name: &str) -> Option<String> {
        let name = CString::new(name).ok()?;
        let size = unsafe {
            (self.raw.read_cvar)(self.raw.context, name.as_ptr(), std::ptr::null_mut(), 0)
        };
        if size == 0 || size > 65536 {
            return None;
        }
        let mut bytes = vec![0u8; size as usize];
        if unsafe {
            (self.raw.read_cvar)(
                self.raw.context,
                name.as_ptr(),
                bytes.as_mut_ptr().cast(),
                size,
            )
        } != size
        {
            return None;
        }
        bytes.pop();
        String::from_utf8(bytes).ok()
    }
    pub fn write_cvar(&self, name: &str, value: &str) -> Result {
        let [name, value] = strings([name, value])?;
        status(unsafe { (self.raw.write_cvar)(self.raw.context, name.as_ptr(), value.as_ptr()) })
    }
    pub fn hide_ui(&self, element: u32, hide: bool) -> Result {
        status(unsafe { (self.raw.hide_ui)(self.raw.context, element, i32::from(hide)) })
    }
    pub fn draw_rect(&self, x: i32, y: i32, width: i32, height: i32, rgba: u32) -> Result {
        status(unsafe { (self.raw.draw_rect)(self.raw.context, x, y, width, height, rgba) })
    }
    pub fn session(&self) -> Option<Session> {
        let mut value = Session {
            size: size_of::<Session>() as u32,
            ..Default::default()
        };
        (unsafe { (self.raw.get_session)(self.raw.context, &mut value) } != 0).then_some(value)
    }
    pub fn player_info(&self, index: i32) -> Option<PlayerInfo> {
        let mut value = PlayerInfo {
            size: size_of::<PlayerInfo>() as u32,
            ..Default::default()
        };
        (unsafe { (self.raw.get_player_info)(self.raw.context, index, &mut value) } != 0)
            .then_some(value)
    }
    /// Screen pixels; None behind the camera. Successful points can be off-screen.
    pub fn world_to_screen(&self, world: [f32; 3]) -> Option<[f32; 2]> {
        let mut screen = [0.0; 2];
        (unsafe {
            (self.raw.world_to_screen)(self.raw.context, world.as_ptr(), screen.as_mut_ptr())
        } != 0)
            .then_some(screen)
    }
    pub fn measure_text(&self, text: &str) -> Option<[i32; 2]> {
        let text = CString::new(text).ok()?;
        let (mut width, mut height) = (0, 0);
        (unsafe {
            (self.raw.measure_text)(self.raw.context, text.as_ptr(), &mut width, &mut height)
        } != 0)
            .then_some([width, height])
    }
    /// Single line in the engine console font; RGB is 0xRRGGBB. Draw callback only.
    pub fn draw_text(&self, x: i32, y: i32, text: &str, rgb: u32) -> Result {
        let text = CString::new(text).map_err(|_| Error)?;
        status(unsafe { (self.raw.draw_text)(self.raw.context, x, y, text.as_ptr(), rgb) })
    }
    /// Relative WAV path under sound/, volume in 0..=1.
    pub fn play_sound(&self, path: &str, volume: f32) -> Result {
        let path = CString::new(path).map_err(|_| Error)?;
        status(unsafe { (self.raw.play_sound)(self.raw.context, path.as_ptr(), volume) })
    }
    pub fn subscribe_event(&self, name: &str, enable: bool) -> Result {
        let name = CString::new(name).map_err(|_| Error)?;
        status(unsafe {
            (self.raw.subscribe_event)(self.raw.context, name.as_ptr(), i32::from(enable))
        })
    }
    /// JSON snapshot; no engine pointers. See the SDK schema for section names.
    pub fn game_data(&self, section: &str, index: i32) -> Option<String> {
        let section = CString::new(section).ok()?;
        json_result(|out, size| unsafe {
            (self.raw.game_data)(self.raw.context, section.as_ptr(), index, out, size)
        })
    }
    pub fn create_cvar(&self, id: &str, initial: &str, archive: bool) -> Result {
        let [id, initial] = strings([id, initial])?;
        status(unsafe {
            (self.raw.create_cvar)(
                self.raw.context,
                id.as_ptr(),
                initial.as_ptr(),
                i32::from(archive),
            )
        })
    }
    pub fn watch_cvar(&self, name: &str, enable: bool) -> Result {
        let name = CString::new(name).map_err(|_| Error)?;
        status(unsafe { (self.raw.watch_cvar)(self.raw.context, name.as_ptr(), i32::from(enable)) })
    }
    pub fn send_chat(&self, text: &str, team: bool) -> Result {
        let text = CString::new(text).map_err(|_| Error)?;
        status(unsafe { (self.raw.send_chat)(self.raw.context, text.as_ptr(), i32::from(team)) })
    }
    pub fn connect(&self, host: &str, port: u16) -> Result {
        let host = CString::new(host).map_err(|_| Error)?;
        status(unsafe { (self.raw.connect)(self.raw.context, host.as_ptr(), u32::from(port)) })
    }
    pub fn disconnect(&self) -> Result {
        status(unsafe { (self.raw.disconnect)(self.raw.context) })
    }
    pub fn chat_print(&self, text: &str) -> Result {
        let [text] = strings([text])?;
        status(unsafe { (self.raw.chat_print)(self.raw.context, text.as_ptr()) })
    }
    /// Returns serialized JSON, including the string "null" for a stored JSON null.
    pub fn store_get(&self, key: &str) -> Option<String> {
        let key = CString::new(key).ok()?;
        json_result(|out, size| unsafe {
            (self.raw.store_get)(self.raw.context, key.as_ptr(), out, size)
        })
    }
    pub fn store_set(&self, key: &str, json: &str) -> Result {
        let [key, json] = strings([key, json])?;
        status(unsafe { (self.raw.store_set)(self.raw.context, key.as_ptr(), json.as_ptr()) })
    }
    pub fn store_delete(&self, key: &str) -> Result {
        let key = CString::new(key).map_err(|_| Error)?;
        status(unsafe { (self.raw.store_delete)(self.raw.context, key.as_ptr()) })
    }
    pub fn store_keys(&self) -> Option<String> {
        json_result(|out, size| unsafe { (self.raw.store_keys)(self.raw.context, out, size) })
    }
    /// Permission-free console output with no plugin ID prefix.
    pub fn console_print(&self, text: &str) -> Result {
        let text = CString::new(text).map_err(|_| Error)?;
        status(unsafe { (self.raw.console_print)(self.raw.context, text.as_ptr()) })
    }
    #[doc(hidden)]
    /// # Safety
    /// All function pointers and the context must belong to the active host.
    pub unsafe fn from_raw(raw: &RawHost) -> std::result::Result<Host<'_>, Error> {
        if raw.size < size_of::<RawHost>() as u32 || raw.abi != ABI || raw.api < API {
            return Err(Error);
        }
        Ok(Host {
            raw,
            _thread: PhantomData,
        })
    }
    pub fn log(&self, message: &str) {
        if let Ok(s) = CString::new(message) {
            unsafe { (self.raw.log)(self.raw.context, s.as_ptr()) }
        }
    }
    pub fn tab(&self, id: &str, en: &str, ru: &str) -> Result {
        let [id, en, ru] = strings([id, en, ru])?;
        if unsafe { (self.raw.add_tab)(self.raw.context, id.as_ptr(), en.as_ptr(), ru.as_ptr()) }
            != 0
        {
            Ok(())
        } else {
            Err(Error)
        }
    }
    pub fn control(&self, c: Control<'_>) -> Result {
        let [id, tab, en, ru, choices_en, choices_ru] =
            strings([c.id, c.tab, c.en, c.ru, c.choices_en, c.choices_ru])?;
        let raw = RawControl {
            size: size_of::<RawControl>() as u32,
            id: id.as_ptr(),
            tab: tab.as_ptr(),
            kind: c.kind as u32,
            label_en: en.as_ptr(),
            label_ru: ru.as_ptr(),
            initial: c.initial,
            minimum: c.min,
            maximum: c.max,
            choices_en: choices_en.as_ptr(),
            choices_ru: choices_ru.as_ptr(),
        };
        if unsafe { (self.raw.add_control)(self.raw.context, &raw) } != 0 {
            Ok(())
        } else {
            Err(Error)
        }
    }
    pub fn checkbox(&self, id: &str, tab: &str, en: &str, ru: &str, initial: bool) -> Result {
        self.control(Control {
            id,
            tab,
            kind: Kind::Checkbox,
            en,
            ru,
            initial: i32::from(initial),
            min: 0,
            max: 1,
            choices_en: "",
            choices_ru: "",
        })
    }
    pub fn setting(&self, id: &str, fallback: i32) -> i32 {
        CString::new(id)
            .map(|s| unsafe { (self.raw.get_setting)(self.raw.context, s.as_ptr(), fallback) })
            .unwrap_or(fallback)
    }
}
fn json_result(read: impl Fn(*mut c_char, u32) -> u32) -> Option<String> {
    let size = read(std::ptr::null_mut(), 0);
    if size == 0 || size > 1024 * 1024 + 1 {
        return None;
    }
    let mut bytes = vec![0; size as usize];
    if read(bytes.as_mut_ptr().cast(), size) != size || bytes.last() != Some(&0) {
        return None;
    }
    bytes.pop();
    String::from_utf8(bytes).ok()
}
fn status(value: i32) -> Result {
    if value != 0 { Ok(()) } else { Err(Error) }
}
fn strings<const N: usize>(values: [&str; N]) -> std::result::Result<[CString; N], Error> {
    let mut result = Vec::with_capacity(N);
    for v in values {
        result.push(CString::new(v).map_err(|_| Error)?);
    }
    result.try_into().map_err(|_| Error)
}
pub trait Plugin: Default {
    fn event(&mut self, _host: &Host<'_>, _name: &str, _json: &str) -> Result {
        Ok(())
    }
    fn load(&mut self, _host: &Host<'_>) -> Result {
        Ok(())
    }
    fn unload(&mut self) {}
    fn command(&mut self, _host: &Host<'_>, _command: &mut Command, _player: &Player) -> Result {
        Ok(())
    }
    fn setting_changed(&mut self, _host: &Host<'_>, _id: &str, _value: i32) -> Result {
        Ok(())
    }
    fn action(&mut self, _host: &Host<'_>, _id: &str) -> Result {
        Ok(())
    }
    fn console_command(&mut self, _host: &Host<'_>, _id: &str, _args: &[&str]) -> Result {
        Ok(())
    }
    fn frame(&mut self, _host: &Host<'_>, _session: &Session) -> Result {
        Ok(())
    }
    fn draw(&mut self, _host: &Host<'_>, _context: &DrawContext) -> Result {
        Ok(())
    }
}
#[doc(hidden)]
pub fn protect(f: impl FnOnce() -> Result) -> i32 {
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(f)) {
        Ok(Ok(())) => 0,
        _ => -1,
    }
}
#[doc(hidden)]
pub const fn metadata<const N: usize>(s: &str) -> [u8; N] {
    let mut bytes = [0; N];
    let mut n = 0;
    while n < s.len() {
        bytes[n] = s.as_bytes()[n];
        n += 1;
    }
    bytes
}
#[doc(hidden)]
/// # Safety
/// `s` must be a host-provided, valid NUL-terminated string for this callback.
pub unsafe fn host_string<'a>(s: *const c_char) -> std::result::Result<&'a str, Error> {
    if s.is_null() {
        return Err(Error);
    }
    unsafe { CStr::from_ptr(s) }.to_str().map_err(|_| Error)
}

/// Export one plugin and its static JSON manifest. Panics with unwind semantics
/// are contained; panic=abort, memory corruption and native faults are not.
#[macro_export]
macro_rules! export_plugin {
    ($ty:ty, $manifest:literal) => {
        #[used]
        #[unsafe(no_mangle)]
        #[unsafe(link_section = ".nclmeta")]
        pub static nc_plugin_manifest: [u8; $manifest.len()+1] = $crate::metadata($manifest);
        std::thread_local! { static NC_INSTANCE: std::cell::RefCell<Option<$ty>> = const { std::cell::RefCell::new(None) }; }
        std::thread_local! { static NC_HOST: std::cell::Cell<*const $crate::RawHost> = const { std::cell::Cell::new(std::ptr::null()) }; }
        fn nc_call(f: impl FnOnce(&mut $ty, &$crate::Host<'_>) -> $crate::Result) -> i32 {
            $crate::protect(|| NC_HOST.with(|h| {
                let raw = unsafe { h.get().as_ref() }.ok_or($crate::Error)?;
                let host = unsafe { $crate::Host::from_raw(raw) }?;
                NC_INSTANCE.with(|s| f(s.borrow_mut().as_mut().ok_or($crate::Error)?, &host))
            }))
        }
        extern "C" fn nc_load(raw: *const $crate::RawHost) -> i32 {
            $crate::protect(|| {
                let raw = unsafe { raw.as_ref() }.ok_or($crate::Error)?;
                let host = unsafe { $crate::Host::from_raw(raw) }?;
                NC_HOST.with(|h| h.set(raw));
                NC_INSTANCE.with(|state| {
                    let mut state = state.borrow_mut(); *state = Some(<$ty>::default());
                    <$ty as $crate::Plugin>::load(state.as_mut().unwrap(), &host)
                })
            })
        }
        extern "C" fn nc_unload() {
            $crate::protect(|| NC_INSTANCE.with(|s| { if let Some(mut p)=s.borrow_mut().take() { <$ty as $crate::Plugin>::unload(&mut p); } Ok(()) }));
            NC_HOST.with(|h| h.set(std::ptr::null()));
        }
        extern "C" fn nc_command(c: *mut $crate::Command, p: *const $crate::Player) -> i32 {
            nc_call(|plugin, host| {
                let c=unsafe { c.as_mut() }.ok_or($crate::Error)?; let p=unsafe { p.as_ref() }.ok_or($crate::Error)?;
                <$ty as $crate::Plugin>::command(plugin,host,c,p)
            })
        }
        extern "C" fn nc_setting(id: *const std::ffi::c_char, value: i32) -> i32 {
            nc_call(|plugin, host| <$ty as $crate::Plugin>::setting_changed(plugin,host,unsafe { $crate::host_string(id) }?,value))
        }
        extern "C" fn nc_action(id: *const std::ffi::c_char) -> i32 {
            nc_call(|plugin, host| <$ty as $crate::Plugin>::action(plugin,host,unsafe { $crate::host_string(id) }?))
        }
        extern "C" fn nc_console(id: *const std::ffi::c_char, argc: i32, argv: *const *const std::ffi::c_char) -> i32 {
            nc_call(|plugin, host| {
                if argc < 0 || argc > 63 || (argc != 0 && argv.is_null()) { return Err($crate::Error); }
                let mut args = Vec::new();
                for i in 0..argc { args.push(unsafe { $crate::host_string(*argv.add(i as usize)) }?); }
                <$ty as $crate::Plugin>::console_command(plugin, host, unsafe { $crate::host_string(id) }?, &args)
            })
        }
        extern "C" fn nc_draw(context: *const $crate::DrawContext) -> i32 {
            nc_call(|plugin, host| <$ty as $crate::Plugin>::draw(plugin, host, unsafe { context.as_ref() }.ok_or($crate::Error)?))
        }
        extern "C" fn nc_frame(context: *const $crate::Session) -> i32 {
            nc_call(|plugin, host| <$ty as $crate::Plugin>::frame(plugin, host, unsafe { context.as_ref() }.ok_or($crate::Error)?))
        }
        extern "C" fn nc_event(name: *const std::ffi::c_char, json: *const std::ffi::c_char) -> i32 {
            nc_call(|plugin, host| <$ty as $crate::Plugin>::event(plugin, host, unsafe { $crate::host_string(name) }?, unsafe { $crate::host_string(json) }?))
        }
        #[unsafe(no_mangle)]
        pub extern "C" fn nc_plugin_entry() -> *const $crate::RawPlugin {
            static API: $crate::RawPlugin = $crate::RawPlugin { size: std::mem::size_of::<$crate::RawPlugin>() as u32,
                abi:$crate::ABI,api:$crate::API,load:nc_load,unload:nc_unload,command:nc_command,setting_changed:nc_setting,action:nc_action,
                console_command:nc_console,draw:nc_draw,frame:nc_frame,event:nc_event };
            &API
        }
    };
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn abi_layout() {
        assert_eq!(size_of::<Command>(), 32);
        assert_eq!(size_of::<Player>(), 12);
        #[cfg(target_pointer_width = "32")]
        {
            assert_eq!(size_of::<RawHost>(), 156);
            assert_eq!(size_of::<RawExtension>(), 28);
            assert_eq!(size_of::<RawPlugin>(), 48);
            assert_eq!(size_of::<RawControl>(), 44);
        }
        assert_eq!(size_of::<PlayerState>(), 92);
        assert_eq!(size_of::<Entity>(), 100);
        assert_eq!(size_of::<Weapon>(), 36);
        assert_eq!(size_of::<DrawContext>(), 20);
        assert_eq!(size_of::<Session>(), 156);
        assert_eq!(size_of::<PlayerInfo>(), 280);
    }
    #[test]
    fn snapshot_strings_are_bounded() {
        let mut info = PlayerInfo::default();
        info.name[..5].copy_from_slice(b"Alex\0");
        assert_eq!(info.name(), "Alex");
        info.model.fill(b'x');
        assert_eq!(info.model().len(), 128);
        info.name[0] = 255;
        assert!(info.name().starts_with('�'));
        assert!(Session::default().map_name().is_empty());
    }
    #[test]
    fn catches_panics() {
        assert_eq!(protect(|| panic!("contained")), -1);
    }
    #[test]
    fn nul_terminated_metadata() {
        assert_eq!(metadata::<3>("{}"), [b'{', b'}', 0]);
    }
}

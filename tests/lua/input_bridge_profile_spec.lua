-- SPDX-License-Identifier: MIT
local runtime_root = os.getenv("F23B_RUNTIME_ROOT") or "Mods/aircraft"
_ = function(value) return value end
-- Prove the profile shim changes only the six inputs that need door-first
-- sequencing while preserving the complete installed Hornet profile surface.
local commands = dofile(
    (runtime_root .. "/F-23B-Player/Cockpit/ExteriorVisuals/BridgeCommands.lua"))
local bridge = dofile(
    (runtime_root .. "/F-23B-Player/Input/F-23B/bridge_profile.lua"))

local native = {
    STICK_TRIGGER_2ND_DETENT = 3002,
    STICK_WEAPON_RELEASE = 3003,
    STICK_WEAPON_SELECT_FWD = 3009,
    STICK_WEAPON_SELECT_AFT = 3010,
    STICK_WEAPON_SELECT_IN = 3011,
    STICK_WEAPON_SELECT_DOWN = 3012,
}
local expected = {
    [3002] = commands.TRIGGER_SECOND_DETENT,
    [3003] = commands.WEAPON_RELEASE,
    [3009] = commands.SELECT_SPARROW,
    [3010] = commands.SELECT_GUN,
    [3011] = commands.SELECT_AMRAAM,
    [3012] = commands.SELECT_SIDEWINDER,
}
local native_order = { 3002, 3009, 3010, 3011, 3012, 3003 }
local profile = { keyCommands = {
    { down = 3002, up = 3002, cockpit_device_id = 13, name = "trigger" },
    { down = 3009, up = 3009, cockpit_device_id = 13, name = "sparrow" },
    { down = 3010, up = 3010, cockpit_device_id = 13, name = "gun" },
    { down = 3011, up = 3011, cockpit_device_id = 13, name = "amraam" },
    { down = 3012, up = 3012, cockpit_device_id = 13, name = "sidewinder" },
    { down = 3003, up = 3003, cockpit_device_id = 13, value_down = 1, value_up = 0, combos = {{key="SPACE"}} },
    { down = 7777, up = 7778, cockpit_device_id = 99, name = "unrelated" },
} }

local release_diff = { ["d3003pnilu3003cd13vd1vpnilvu0"] = {
    removed = {{key="SPACE"}}, added = {{key="JOY_BTN2"}}
} }
local result = bridge(profile, { HOTAS = 13 }, native, commands, release_diff)
assert(#profile.keyCommands[6].combos == 1 and profile.keyCommands[6].combos[1].key == "JOY_BTN2",
    "existing native release assignment was lost or a new binding was added")
assert(result == profile, "bridge replaced the installed profile table")
for index = 1, 6 do
    local row = profile.keyCommands[index]
    assert(row.down == expected[native_order[index]],
        "required Hornet HOTAS row was not rewritten")
    assert(row.up == row.down, "rewritten release edge does not match press edge")
    assert(row.cockpit_device_id == nil,
        "rewritten row still bypasses the exterior sequencer")
end
local unrelated = profile.keyCommands[7]
assert(unrelated.down == 7777 and unrelated.up == 7778 and
        unrelated.cockpit_device_id == 99,
    "bridge modified an unrelated installed Hornet input row")

-- Reproduce the DCS Input/Data.lua sandbox contract. external_profile's
-- second argument is what gives the installed profile its own valid `folder`;
-- a nested dofile cannot inherit an assignment from the caller's sandbox.
local real_dofile = dofile
local function exercise_wrapper(device_kind)
    local requested_path = nil
    local requested_folder = nil
    local hornet_folder = "./Mods/aircraft/FA-18C/Input/FA-18C/"
        .. device_kind .. "/"
    folder = (runtime_root .. "/F-23B-Player/Input/F-23B/")
        .. device_kind .. "/"
    external_profile = function(path, folder_new)
        requested_path = path
        requested_folder = folder_new
        return { keyCommands = {
            { down = 3002, up = 3002, cockpit_device_id = 13 },
            { down = 3009, up = 3009, cockpit_device_id = 13 },
            { down = 3010, up = 3010, cockpit_device_id = 13 },
            { down = 3011, up = 3011, cockpit_device_id = 13 },
            { down = 3012, up = 3012, cockpit_device_id = 13 },
            { down = 3003, up = 3003, cockpit_device_id = 13, value_down = 1, value_up = 0, combos = {{key="SPACE"}} },
    { down = 7777, up = 7778, cockpit_device_id = 99 },
        } }
    end
    dofile = function(path)
        if path == "./Mods/aircraft/FA-18C/Cockpit/Scripts/devices.lua" then
            devices = { HOTAS = 13 }
            return
        end
        if path == "./Mods/aircraft/FA-18C/Cockpit/Scripts/command_defs.lua" then
            hotas_commands = native
            return
        end
        return real_dofile(path)
    end
    local ok, wrapped = pcall(real_dofile,
        (runtime_root .. "/F-23B-Player/Input/F-23B/") .. device_kind
            .. "/default.lua")
    dofile = real_dofile
    assert(ok, wrapped)
    assert(requested_path == hornet_folder .. "default.lua",
        device_kind .. " wrapper requested the wrong installed profile")
    assert(requested_folder == hornet_folder,
        device_kind .. " wrapper did not supply the installed profile folder")
    assert(#wrapped.keyCommands == 7 and wrapped.keyCommands[7].down == 7777,
        device_kind .. " wrapper dropped an unrelated Hornet control")
    for index = 1, 6 do
        assert(wrapped.keyCommands[index].down == expected[native_order[index]],
            device_kind .. " wrapper did not sequence a required weapon row")
    end
end

exercise_wrapper("keyboard")
exercise_wrapper("joystick")
print("PASS: input overlay rewrites only trigger, weapon-release and A/A weapon-select rows")
print("PASS: keyboard and joystick wrappers preserve external Hornet profiles")

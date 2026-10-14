-- SPDX-License-Identifier: MIT
local runtime_root = os.getenv("F23B_RUNTIME_ROOT") or "Mods/aircraft"
-- Execute the production door-first Hornet trigger sequencer in a minimal DCS
-- device sandbox. This proves ordering and routing, not DCS runtime pixels.
local adapter_path =
    (runtime_root .. "/F-23B-Player/Cockpit/ExteriorVisuals/ExteriorVisualAdapter.lua")
local bridge_path =
    (runtime_root .. "/F-23B-Player/Cockpit/ExteriorVisuals/BridgeCommands.lua")

local MAIN_BAY = 1009
local GUN_DOOR = 1010
local LEFT_NOZZLE = 1011
local RIGHT_NOZZLE = 1012
local IR_CARRIER = 1013
local GUN_EFFECT = 350
local commands = dofile(bridge_path)

local native_mode = { [47]=0, [48]=0 }
local cockpit_device
local hornet_device
local aircraft
local output
local writes
local listened
local forwarded
local logs
local hud_text
local persist_writes
local activity
local requested_device
local clickable_return_value
local radar_target, radar_side
local radar_cue_requested
local params
local radar_failure
local ranking_scope
local project_harm_mask = 0
local original_loadlib = package.loadlib
package.loadlib = function()
    return function() return function(enabled, cue_ir)
        ranking_scope = enabled
        radar_cue_requested = cue_ir
        if radar_failure then error("radar callback failed") end
        local result = {1, radar_target, radar_side, 0, 0.2, -0.1,
            radar_side == 2 and cue_ir and 1 or 0}
        result[28] = project_harm_mask
        return unpack(result, 1, 28)
    end end
end
local mock_loadlib = package.loadlib

local function check(condition, message)
    if not condition then error(message, 2) end
end

local function near(actual, expected, tolerance)
    return math.abs(actual - expected) <= (tolerance or 1e-6)
end

local function value(argument)
    return output[argument] or 0.0
end

local function record(name, item)
    return "-----------------------------------------\n"
        .. name .. "\n" .. tostring(item or "") .. "\n"
end

local function missile_hud(weapon, count)
    return record("AA_Weapon_type", weapon)
        .. record("AA_Weapon_count", count)
end

local function gun_hud(rounds)
    return record("GUN_AA_label", "GUN")
        .. record("GUN_AA_Round_Data", rounds)
end

local function logged(fragment)
    for _, message in ipairs(logs) do
        if string.find(message, fragment, 1, true) then return true end
    end
    return false
end

local function forwarded_count(command, command_value)
    local count = 0
    for _, call in ipairs(forwarded) do
        if call.command == command and call.value == command_value then
            count = count + 1
        end
    end
    return count
end

function GetSelf()
    return cockpit_device
end

function GetDevice(id)
    if id == 0 then return {update_arguments=function() end, get_argument_value=function(_, a) return native_mode[a] end} end
    requested_device = id
    return hornet_device
end

function get_base_data()
    return {
        getEngineLeftRPM = function() return 70.0 end,
        getEngineRightRPM = function() return 72.0 end,
    }
end

function get_param_handle(name)
    params[name] = params[name] or {value = 0}
    return {
        get = function() return params[name].value end,
        set = function(_, value) params[name].value = value end,
    }
end

function make_default_activity(seconds)
    activity = seconds
end

-- The real cockpit: display 1 is the HUD. The HARM format with its station text is on a DDI or the AMPCD.
local display_text = {}
function list_indication(id)
    if id ~= nil and id ~= 1 then return display_text[id] or "" end
    return hud_text
end

function get_aircraft_draw_argument_value(argument)
    return aircraft[argument] or 0.0
end

function set_aircraft_draw_argument_value(argument, argument_value)
    output[argument] = argument_value
    if persist_writes then aircraft[argument] = argument_value end
    writes[#writes + 1] = { argument = argument, value = argument_value }
end

log = {
    INFO = 1,
    write = function(_, _, message) logs[#logs + 1] = tostring(message) end,
}

LockOn_Options = {
    script_path = (runtime_root .. "/F-23B-Player/Cockpit/ExteriorVisuals/"),
}

local allowed_writes = {
    [MAIN_BAY] = true,
    [GUN_DOOR] = true,
    [LEFT_NOZZLE] = true,
    [RIGHT_NOZZLE] = true,
    [IR_CARRIER] = true,
    [1021] = true,
    [1018] = true, [1019] = true, [1020] = true,
}

local function check_writes(context)
    for _, write in ipairs(writes) do
        check(allowed_writes[write.argument],
            context .. ": non-presentation write " .. tostring(write.argument))
        check(write.value >= 0.0 and write.value <= 1.0,
            context .. ": unclamped presentation write")
    end
end

local function fresh_adapter(initial_hud, action_return_value, initial_carrier)
    native_mode = {[47]=1, [48]=0, [135]=1}
    params = {WS_IR_MISSILE_LOCK={value=1}}
    radar_target, radar_side = 754, 2
    radar_failure = false
    project_harm_mask = 0
    aircraft = { [89] = 0.0, [90] = 0.0, [1022] = 1.0, [IR_CARRIER] = initial_carrier or 0.0 }
    output = {}
    writes = {}
    listened = {}
    forwarded = {}
    logs = {}
    hud_text = initial_hud or record("UNRELATED", "")
    display_text = {}
    persist_writes = true
    activity = nil
    requested_device = nil
    clickable_return_value = action_return_value
    if clickable_return_value == nil then clickable_return_value = true end
    cockpit_device = {}
    function cockpit_device:listen_command(command)
        listened[#listened + 1] = command
    end
    hornet_device = {}
    function hornet_device:performClickableAction(command, command_value, animated)
        forwarded[#forwarded + 1] = {
            command = command, value = command_value, animated = animated,
            main_bay = value(MAIN_BAY), gun_door = value(GUN_DOOR),
            ir_carrier = (1.0 - value(IR_CARRIER)),
            harm3 = value(1018), harm2 = value(1019), harm7 = value(1020),
        }
        return clickable_return_value
    end
    SetCommand = nil
    dofile(adapter_path)
    if initial_carrier ~= nil then
        check(near(aircraft[IR_CARRIER], 0.0), "player device load must restore the original IR carrier pose before post_initialize")
    end
    check(near(activity, 0.01), "adapter update rate drifted")
    check(#listened == 6, "adapter must own exactly six bridge commands")
    check(type(SetCommand) == "function", "adapter command handler missing")
    post_initialize()
    check(requested_device == 13, "adapter did not acquire Hornet HOTAS device 13")
    check(near(value(MAIN_BAY), 0.0), "main bay did not initialize closed")
    check(near(value(GUN_DOOR), 0.0), "gun door did not initialize closed")
end

local function advance(count)
    for _ = 1, count do update() end
end

local function reach(argument, target, limit, context)
    for _ = 1, limit do
        update()
        if near(value(argument), target) then return end
    end
    error(context .. ": argument " .. argument .. " did not reach " .. target, 2)
end

-- A livery can start both aircraft at the stowed pose. Restore the player
-- initialization pose before post_initialize, while leaving AI livery handling alone.
fresh_adapter(missile_hud("9X", 2), true, 1.0)
check(near(value(IR_CARRIER), 1.0), "player post_initialize did not restore the stowed pose")
print("PASS: player initialization preserves original carrier pose with stowed livery")

-- AMRAAM selection is forwarded immediately. A short trigger tap queues one
-- Hornet trigger pulse, but only after the 0.20-second main-bay travel.
fresh_adapter(missile_hud("9X", 2))
SetCommand(commands.SELECT_AMRAAM, 1.0)
SetCommand(commands.SELECT_AMRAAM, 0.0)
check(forwarded_count(3011, 1.0) == 1
        and forwarded_count(3011, 0.0) == 1,
    "AMRAAM selection was not forwarded unchanged")
SetCommand(commands.TRIGGER_SECOND_DETENT, 1.0)
SetCommand(commands.TRIGGER_SECOND_DETENT, 0.0)
advance(19)
check(forwarded_count(3002, 1.0) == 0,
    "missile trigger escaped before the bay reached open")
update()
check(near(value(MAIN_BAY), 1.0), "main bay did not open in 0.20 seconds")
check(forwarded_count(3002, 1.0) == 1,
    "queued missile trigger was not forwarded at locked-open")
check(near(forwarded[#forwarded].main_bay, 1.0),
    "missile trigger callback did not observe the fully published main bay")
check(logged("door locked open before trigger: argument=1009 weapon=AMRAAM"),
    "locked-open missile ordering was not logged")
advance(125)
check(forwarded_count(3002, 0.0) == 1,
    "short missile tap did not receive a bounded release edge")
advance(20)
check(near(value(MAIN_BAY), 1.0),
    "main bay did not remain open through the post-trigger hold")
reach(MAIN_BAY, 0.0, 30, "main bay close")
check(near(value(GUN_DOOR), 0.0), "missile route cross-opened gun door")

-- A stale 9X HUD cannot override an authoritative gun-selection command.
-- Immediate gun selection + trigger must also publish its pose at the call.
fresh_adapter(missile_hud("9X", 2))
SetCommand(commands.SELECT_GUN, 1.0)
SetCommand(commands.TRIGGER_SECOND_DETENT, 1.0)
advance(7)
check(forwarded_count(3002, 1.0) == 0, "gun trigger escaped before door travel")
update()
check(near(forwarded[#forwarded].gun_door, 1.0),
    "gun trigger callback did not observe the fully published gun door")
SetCommand(commands.TRIGGER_SECOND_DETENT, 0.0)

-- Gun selection pre-opens the archive-proven panel before the trigger is sent.
fresh_adapter(missile_hud("9X", 2))
SetCommand(commands.SELECT_GUN, 1.0)
SetCommand(commands.SELECT_GUN, 0.0)
advance(7)
check(forwarded_count(3002, 1.0) == 0,
    "selection alone fired the gun")
update()
check(near(value(GUN_DOOR), 1.0), "gun door did not pre-open in 0.08 seconds")
advance(30)
check(near(value(GUN_DOOR), 1.0), "stale missile HUD closed selected gun door")
SetCommand(commands.TRIGGER_SECOND_DETENT, 1.0)
update()
check(forwarded_count(3002, 1.0) == 1,
    "gun trigger was not forwarded behind an open gun door")
SetCommand(commands.TRIGGER_SECOND_DETENT, 0.0)
check(forwarded_count(3002, 0.0) == 1,
    "gun trigger release was not forwarded")
check(near(value(MAIN_BAY), 0.0), "gun route cross-opened main bay")

-- Live DCS executes performClickableAction but returns false. That return is
-- not an acknowledgement. It must produce exactly one down/up pair per press,
-- allow repeated shots without changing weapons, and never strand the gun.
fresh_adapter(missile_hud("AC", 3), false)
SetCommand(commands.SELECT_AMRAAM, 1.0)
SetCommand(commands.SELECT_AMRAAM, 0.0)
for _ = 1, 2 do
    SetCommand(commands.TRIGGER_SECOND_DETENT, 1.0)
    SetCommand(commands.TRIGGER_SECOND_DETENT, 0.0)
    reach(MAIN_BAY, 1.0, 25, "repeat-shot main bay open")
    advance(125)
end
check(forwarded_count(3002, 1.0) == 2
        and forwarded_count(3002, 0.0) == 2,
    "DCS false return prevented repeat missile trigger edges")
check(not logged("HOTAS forwarding failed"),
    "DCS false return was misclassified as a forwarding failure")

-- A follow-up request during closure reverses the same continuous door motion.
-- The next trigger edge remains withheld until the bay is fully open again.
advance(30)
check(value(MAIN_BAY) > 0.0 and value(MAIN_BAY) < 1.0,
    "main bay did not begin prompt post-shot closure")
local completed_missile_presses = forwarded_count(3002, 1.0)
SetCommand(commands.TRIGGER_SECOND_DETENT, 1.0)
SetCommand(commands.TRIGGER_SECOND_DETENT, 0.0)
while value(MAIN_BAY) < 0.999 do
    update()
    if value(MAIN_BAY) < 0.999 then
        check(forwarded_count(3002, 1.0) == completed_missile_presses,
            "follow-up missile fired before the closing bay reopened")
    end
end
check(forwarded_count(3002, 1.0) == completed_missile_presses + 1,
    "closing main bay did not reverse and deliver the follow-up shot")
advance(125)
check(forwarded_count(3002, 0.0) == completed_missile_presses + 1,
    "reopened-bay follow-up shot did not receive its release edge")

fresh_adapter(gun_hud(578), false)
SetCommand(commands.SELECT_GUN, 1.0)
SetCommand(commands.SELECT_GUN, 0.0)
reach(GUN_DOOR, 1.0, 10, "repeat-fire gun door open")
for _ = 1, 2 do
    SetCommand(commands.TRIGGER_SECOND_DETENT, 1.0)
    update()
    SetCommand(commands.TRIGGER_SECOND_DETENT, 0.0)
end
check(forwarded_count(3002, 1.0) == 2
        and forwarded_count(3002, 0.0) == 2,
    "DCS false return stranded the gun trigger or blocked repeat fire")
check(not logged("HOTAS forwarding failed"),
    "DCS false return was misclassified during gun fire")

-- Leaving gun mode closes the gun door in exactly 0.08 seconds. A lingering
-- native gun-effect hold must not keep the wrong door open or route the next
-- missile trigger through it.
aircraft[GUN_EFFECT] = 1.0
update()
aircraft[GUN_EFFECT] = 0.0
SetCommand(commands.SELECT_AMRAAM, 1.0)
SetCommand(commands.SELECT_AMRAAM, 0.0)
advance(7)
check(value(GUN_DOOR) > 0.0,
    "gun door closed faster than the archive-proven 0.08 seconds")
update()
check(near(value(GUN_DOOR), 0.0),
    "gun door did not close in 0.08 seconds after missile selection")
check(near(value(MAIN_BAY), 0.0),
    "missile selection opened the main bay without a trigger request")

-- Native gun effect 350 is authoritative even when the HUD is recognized as
-- a missile. It can never be suppressed by frozen display text again.
fresh_adapter(missile_hud("9X", 2))
advance(5)
local bay_before_gun_effect = value(MAIN_BAY)
aircraft[GUN_EFFECT] = 1.0
update()
check(near(value(GUN_DOOR), 0.125),
    "native gun effect did not start gun-door travel")
check(near(value(MAIN_BAY), 0),
    "native gun effect opened the missile bay")
aircraft[GUN_EFFECT] = 0.0
reach(GUN_DOOR, 1.0, 10, "gun-effect panel open")
advance(55)
check(near(value(GUN_DOOR), 1.0), "gun-effect hold ended too early")
reach(GUN_DOOR, 0.0, 30, "gun-effect panel close")

-- Independent nozzle smoothing suppresses native jitter without stopping short
-- of a latched target or hanging at the peak when the throttle comes back.
fresh_adapter(record("UNRELATED", "value"))
aircraft[90] = 0.8
aircraft[89] = 0.2
update()
check(value(LEFT_NOZZLE) > 0.0 and value(LEFT_NOZZLE) < 0.8,
    "left SERN input did not remain filtered")
check(value(RIGHT_NOZZLE) > 0.0 and value(RIGHT_NOZZLE) < 0.2,
    "right SERN input did not remain filtered")
advance(500)
check(near(value(LEFT_NOZZLE), 0.8, 0.035), "left SERN did not converge")
check(near(value(RIGHT_NOZZLE), 0.2, 0.035), "right SERN did not converge")
for tick = 1, 200 do
    aircraft[90] = 0.8 + (tick % 2 == 0 and 0.015 or -0.015)
    update()
    check(near(value(LEFT_NOZZLE), 0.8, 0.00001),
        "steady nozzle jitter moved the wedge")
end
aircraft[90] = 1.0
for tick = 1, 500 do
    local previous = value(LEFT_NOZZLE)
    update()
    check(value(LEFT_NOZZLE) >= previous and value(LEFT_NOZZLE) <= 1.0,
        "opening wedge reversed or overshot")
    check(value(LEFT_NOZZLE) - previous <= 0.020001,
        "opening wedge exceeded its travel-rate limit")
end
check(near(value(LEFT_NOZZLE), 1.0), "wedge stopped short of full open")
aircraft[90] = 0.0
update()
check(value(LEFT_NOZZLE) < 1.0, "wedge still hangs at its previous peak")
for tick = 1, 500 do
    local previous = value(LEFT_NOZZLE)
    update()
    check(value(LEFT_NOZZLE) <= previous and value(LEFT_NOZZLE) >= 0.0,
        "closing wedge reversed or overshot")
    check(previous - value(LEFT_NOZZLE) <= 0.006001,
        "closing wedge exceeded its travel-rate limit")
end
check(near(value(LEFT_NOZZLE), 0.0), "wedge stopped short of closed")
check(near(value(RIGHT_NOZZLE), 0.2), "left throttle moved the right wedge")

-- Cross-tick argument delivery monitoring remains active.
fresh_adapter(missile_hud("AC", 3))
SetCommand(commands.SELECT_AMRAAM, 1.0)
SetCommand(commands.SELECT_AMRAAM, 0.0)
SetCommand(commands.TRIGGER_SECOND_DETENT, 1.0)
update()
update()
check(logged("written1009=0.050 persisted1009=0.050"),
    "first nonzero main-bay write was not verified on the next tick")
persist_writes = false
update()
update()
check(logged("did not persist to the following update tick"),
    "dropped exterior write did not raise a warning")

check_writes("door-first Hornet bridge")
-- All missile selections stay concealed. A tap or held press delivers one
-- bounded native pulse, then closes even with the same weapon selected.
for _, selection in ipairs({commands.SELECT_SIDEWINDER, commands.SELECT_AMRAAM,
        commands.SELECT_SPARROW}) do
    for _, held in ipairs({false, true}) do
        fresh_adapter(missile_hud("9X", 2), false)
        SetCommand(selection, 1)
        SetCommand(selection, 0)
        advance(100)
        check(near(value(MAIN_BAY), 0), "missile selection exposed the bay")
        SetCommand(commands.TRIGGER_SECOND_DETENT, 1)
        if not held then SetCommand(commands.TRIGGER_SECOND_DETENT, 0) end
        advance(19)
        check(forwarded_count(3002, 1) == 0, "missile fired through a closed door")
        update()
        check(forwarded_count(3002, 1) == 1, "one press did not deliver launch")
        check(near(forwarded[#forwarded].main_bay, 1), "launch pose not published")
        advance(180)
        check(forwarded_count(3002, 1) == 1 and forwarded_count(3002, 0) == 1,
            "held missile trigger repeated or lacked release")
        check(near(value(MAIN_BAY), 0), "missile bay stayed open after shot")
        check(near(value(IR_CARRIER), 1), "door mounting correction moved")
        SetCommand(commands.TRIGGER_SECOND_DETENT, 0)
        check_writes("single press missile cycle")
    end
end
-- Releasing after the door opens must not cancel Hornet's delayed release.
fresh_adapter(missile_hud("9X", 2))
SetCommand(commands.SELECT_SIDEWINDER, 1)
SetCommand(commands.TRIGGER_SECOND_DETENT, 1)
advance(25)
SetCommand(commands.TRIGGER_SECOND_DETENT, 0)
check(forwarded_count(3002, 0) == 0, "physical release cancelled delayed shot")
advance(30)
check(forwarded_count(3002, 0) == 1, "delayed shot did not finish its pulse")
reach(MAIN_BAY, 0, 50, "mid-pulse physical release closes bay")
print("PASS: all missile selections stay concealed; tap/hold cycles open, fire once and close")
print("PASS: weapon selection is authoritative over stale Hornet HUD text")
print("PASS: gun and missile trigger edges are forwarded only behind the correct open door")
print("PASS: DCS false returns preserve repeat shots and paired gun releases")
print("PASS: main bay closes promptly and reverses safely for rapid follow-up fire")
print("PASS: main and gun bays close on their bounded post-fire/mode-change schedules")
print("PASS: native gun effect 350 is unconditional and cannot be HUD-suppressed")
fresh_adapter()
native_mode = {[47]=0, [48]=0}
advance(6)
check(value(1021) == 0, "NAV permits native lights")
native_mode[47] = 1
advance(6)
check(value(1021) == 1, "AA suppresses lights")
native_mode = {[47]=0, [48]=1}
advance(6)
check(value(1021) == 1, "AG suppresses lights")
hud_text = ""
advance(6)
check(value(1021) == 1, "HUD off does not release combat blackout")
native_mode = {[47]=0, [48]=0}
advance(6)
check(value(1021) == 0, "NAV restores visibility even with HUD off")
native_mode = {}
advance(6)
check(value(1021) == 1, "unavailable mode remains dark")
check_writes("light blackout never writes native dimmers or landing light")
print("PASS: adapter writes only 1009-1013/1021; doors, nozzles and native mode blackout")

-- Reproduce friendly selection before a queued shot, and during native delay.
fresh_adapter(missile_hud("120B", 3))
SetCommand(commands.SELECT_AMRAAM, 1)
SetCommand(commands.TRIGGER_SECOND_DETENT, 1)
advance(10)
radar_target, radar_side = 720, 1
advance(15)
check(forwarded_count(3002, 1) == 0, "queued shot reached a friendly target")
check(ranking_scope == true, "A/A radar selection did not enable native exclusion")
check(forwarded_count(3013, 1) == 0, "automatic Undesignate cycling remains active")
radar_target, radar_side = 754, 2
advance(30)
check(forwarded_count(3002, 1) == 0, "cancelled trigger fired at a replacement target")
SetCommand(commands.TRIGGER_SECOND_DETENT, 0)
SetCommand(commands.TRIGGER_SECOND_DETENT, 1)
advance(25)
check(forwarded_count(3002, 1) == 1, "fresh hostile shot was blocked")
radar_target, radar_side = 720, 1
update()
check(forwarded_count(3002, 0) == 1, "friendly selection did not cancel native launch delay")

for _, side in ipairs({0, -1}) do
    fresh_adapter(missile_hud("120B", 3))
    radar_side = side
    SetCommand(commands.SELECT_AMRAAM, 1)
    SetCommand(commands.TRIGGER_SECOND_DETENT, 1)
    advance(160)
    check(forwarded_count(3002, 1) == 0, "unverified radar target permitted a shot")
    check(forwarded_count(3013, 1) == 0, "unverified target was falsely classified friendly")
end
-- WVR IR acquisition and guns must not depend on the radar's target.
for _, selection in ipairs({commands.SELECT_SIDEWINDER, commands.SELECT_GUN}) do
    fresh_adapter(missile_hud("9X", 2))
    radar_target, radar_side = 720, 1
    SetCommand(selection, 1)
    SetCommand(commands.TRIGGER_SECOND_DETENT, 1)
    advance(40)
    check(forwarded_count(3002, 1) == 1, "radar guard blocked independent IR or gun")
    check(forwarded_count(3013, 1) == 0, "radar guard changed WVR designation")
    check(ranking_scope == (selection == commands.SELECT_SIDEWINDER),
        "friendly exclusion did not follow the A/A IR/gun scope")
end
fresh_adapter(missile_hud("9X", 2))
SetCommand(commands.SELECT_SIDEWINDER, 1)
SetCommand(commands.TRIGGER_SECOND_DETENT, 1)
aircraft[1022] = 0
advance(30)
check(forwarded_count(3002, 1) == 1,
    "Sidewinder request was blocked by an unverified cockpit cue")
aircraft[1022] = 1
SetCommand(commands.TRIGGER_SECOND_DETENT, 0)
SetCommand(commands.TRIGGER_SECOND_DETENT, 1)
advance(25)
check(forwarded_count(3002, 1) == 2, "fresh locked IR shot was blocked")
-- The IR seeker cue moved from the ED-hooked Hornet cockpit script into this adapter.
check(radar_cue_requested == true, "exterior adapter did not request the IR seeker cue")
local scope_failures = {}
local function scope_check(ok, name)
    print((ok and "PASS: " or "FAIL: ") .. name)
    if not ok then scope_failures[#scope_failures+1] = name end
end
for _, mode in ipairs({{[47]=0,[48]=0}, {[47]=0,[48]=1}, {[47]=1,[48]=1}}) do
    fresh_adapter(missile_hud("120B", 3))
    SetCommand(commands.SELECT_AMRAAM, 1)
    native_mode = mode -- Native NAV/A/G/lamp-test; stale wrapped A/A selection.
    radar_target, radar_side = 720, 1
    SetCommand(commands.TRIGGER_SECOND_DETENT, 1)
    advance(30)
    scope_check(ranking_scope == false and forwarded_count(3013, 1) == 0 and forwarded_count(3002, 1) == 1,
        "radar filter stays out of NAV/A/G/ambiguous master mode " .. mode[47] .. "/" .. mode[48])
end
fresh_adapter(missile_hud("120B", 3))
SetCommand(commands.SELECT_AMRAAM, 1)
aircraft[GUN_EFFECT] = 1
radar_target, radar_side = 720, 1
SetCommand(commands.TRIGGER_SECOND_DETENT, 1)
advance(15)
scope_check(ranking_scope == false and forwarded_count(3002, 1) == 1 and forwarded_count(3013, 1) == 0,
    "native gun fallback overrides stale missile selection for the radar guard")

fresh_adapter(missile_hud("120B", 3))
SetCommand(commands.SELECT_AMRAAM, 1)
radar_target, radar_side = 720, 1
update()
scope_check(ranking_scope == true, "A/A scope enters native exclusion")
radar_target, radar_side = 754, 2
native_mode = {[47]=0,[48]=0}
advance(5)
scope_check(ranking_scope == false, "NAV clears native exclusion")
native_mode = {[47]=1,[48]=0}
update()
scope_check(ranking_scope == true and forwarded_count(3013, 1) == 0,
    "A/A reentry enables exclusion without Undesignate")
check(#scope_failures == 0, table.concat(scope_failures, "; "))
-- The radar helper shares a device with doors, lights and nozzle animation.
-- Its protected failure must not stop those paths or independent IR/gun fire.
for _, selection in ipairs({commands.SELECT_SIDEWINDER, commands.SELECT_GUN}) do
    fresh_adapter(missile_hud("9X", 2))
    radar_failure = true
    aircraft[90], aircraft[89] = 1, 1
    SetCommand(selection, 1)
    SetCommand(commands.TRIGGER_SECOND_DETENT, 1)
    advance(40)
    check(forwarded_count(3002, 1) == 1, "radar Lua failure stopped independent fire")
    check(value(LEFT_NOZZLE) > 0 and value(RIGHT_NOZZLE) > 0,
        "radar Lua failure stopped nozzle animation")
    check(value(1021) == 1, "radar Lua failure stopped mode lighting")
end
print("PASS: radar Lua failure preserves independent IR/gun, door, nozzle and light updates")
package.loadlib = original_loadlib
print("PASS: native exclusion scope; no automatic Undesignate; radar shot guard; independent WVR IR and gun")

-- A/G release must not inherit the A/A radar guard or repeat while held.
for _, held in ipairs({false, true}) do
    fresh_adapter(missile_hud("120B", 3), false)
    SetCommand(commands.SELECT_AMRAAM, 1)
    native_mode = {[47]=0,[48]=1}
    radar_target, radar_side = 720, 1
    SetCommand(commands.WEAPON_RELEASE, 1)
    if not held then SetCommand(commands.WEAPON_RELEASE, 0) end
    advance(19)
    check(forwarded_count(3003, 1) == 0, "A/G release escaped before bay open")
    update()
    check(forwarded_count(3003, 1) == 1 and near(forwarded[#forwarded].main_bay, 1),
        "A/G release did not follow the published open bay")
    advance(220)
    check(forwarded_count(3003, 1) == 1 and forwarded_count(3003, 0) == 1,
        "A/G held release repeated or lacked a release edge")
    check(near(value(MAIN_BAY), 0), "A/G bay did not close after the pulse")
    check(forwarded_count(3002, 1) == 0 and ranking_scope == false,
        "A/G request used the A/A trigger or radar guard")
    SetCommand(commands.WEAPON_RELEASE, 0)
end
fresh_adapter()
native_mode = {[47]=0,[48]=1}
SetCommand(commands.WEAPON_RELEASE, 1)
advance(10)
native_mode = {[47]=0,[48]=0}
advance(100)
check(forwarded_count(3003, 1) == 0, "A/G pending release survived leaving A/G")
fresh_adapter()
native_mode = {[47]=0,[48]=0}
SetCommand(commands.WEAPON_RELEASE, 1)
SetCommand(commands.WEAPON_RELEASE, 0)
check(forwarded_count(3003, 1) == 1 and forwarded_count(3003, 0) == 1,
    "release outside A/G was not forwarded unchanged")
print("PASS: A/G tap/hold opens bay, releases once and closes; mode exit cancels; native binding preserved")

-- The exact project payload identity enables the three roof trapezes (stations 2, 3, 7).
-- Publish their deployed poses before consent, and retract before door closure.
package.loadlib = mock_loadlib
for _, held in ipairs({false, true}) do
    fresh_adapter(nil, false)
    display_text[held and 2 or 3] = record("HARM_Station", "STA 3")
    native_mode = {[47]=0,[48]=1}
    project_harm_mask = 4 + 8 + 128
    SetCommand(commands.WEAPON_RELEASE, 1)
    if not held then SetCommand(commands.WEAPON_RELEASE, 0) end
    advance(19)
    check(value(1018) == 0 and forwarded_count(3003, 1) == 0, "trapeze moved before doors opened")
    advance(19)
    check(value(1018) > 0 and value(1018) < 1 and forwarded_count(3003, 1) == 0,
        "native release escaped before full trapeze travel")
    for tick=1,40 do
        if forwarded_count(3003, 1) == 1 then break end
        update()
    end
    local call = forwarded[#forwarded]
    check(call.command == 3003 and call.value == 1 and call.main_bay == 1
        and near(call.harm3, 1) and near(call.harm2, 1) and near(call.harm7, 1),
        "native HARM consent did not observe all published carrier endpoints")
    local retracting = false
    for tick=1,250 do
        update()
        local carrier = value(1018)
        if carrier > 0 and carrier < 1 then
            retracting = true
            check(value(MAIN_BAY) == 1, "doors closed across a retracting HARM")
        end
    end
    check(retracting and value(1018) == 0 and value(1019) == 0 and value(1020) == 0
        and value(MAIN_BAY) == 0, "HARM did not retract and close")
    check(forwarded_count(3003, 1) == 1 and forwarded_count(3003, 0) == 1,
        "trapeze route repeated a held release")
end
-- A stock HARM on the selected station must keep the prior native route,
-- even when a project HARM occupies a different station.
fresh_adapter(); display_text[2] = record("HARM_Station", "STA 3")
native_mode = {[47]=0,[48]=1}; project_harm_mask = 4
SetCommand(commands.WEAPON_RELEASE, 1); SetCommand(commands.WEAPON_RELEASE, 0)
advance(20)
check(forwarded_count(3003, 1) == 1 and value(1018) == 0 and value(1019) == 0,
    "project carrier logic changed the selected stock HARM")
fresh_adapter(); display_text[2] = record("HARM_Station", "STA 3")
native_mode = {[47]=0,[48]=1}; project_harm_mask = 4 + 8 + 128
SetCommand(commands.WEAPON_RELEASE, 1); advance(28)
native_mode = {[47]=0,[48]=0}; advance(100)
check(forwarded_count(3003, 1) == 0 and value(1018) == 0 and value(MAIN_BAY) == 0,
    "mode exit left a pending HARM or an extended trapeze")
print("PASS: project HARM identity; doors open, trapezes deploy, consent, retract, close; stock route preserved")
fresh_adapter(); display_text[2] = record("HARM_Station", "STA 3")
native_mode = {[47]=0,[48]=1}; project_harm_mask = 8
SetCommand(commands.WEAPON_RELEASE, 1); advance(28)
display_text[2] = record("HARM_Station", "STA 2"); advance(100)
check(forwarded_count(3003, 1) == 0 and value(1018) == 0 and value(MAIN_BAY) == 0,
    "pending project consent fired after selection changed to a stock HARM")
-- The doors-1 flight: station text on the HUD alone must not be read; the HUD never carries it.
fresh_adapter(record("HARM_Station", "STA 7")); display_text[2] = record("HARM_Station", "STA 3")
native_mode = {[47]=0,[48]=1}; project_harm_mask = 8
SetCommand(commands.WEAPON_RELEASE, 1); SetCommand(commands.WEAPON_RELEASE, 0); advance(45)
check(value(1018) > 0, "the station text on the left DDI did not select the trapeze route")
-- No display shows the HARM format: each loaded project HARM still uses its trapeze.
fresh_adapter()
native_mode = {[47]=0,[48]=1}; project_harm_mask = 4 + 128
SetCommand(commands.WEAPON_RELEASE, 1); SetCommand(commands.WEAPON_RELEASE, 0); advance(45)
check(value(1019) > 0 and value(1020) > 0 and value(1018) == 0,
    "project HARMs did not use their trapezes without a HARM format on a display")
fresh_adapter()
native_mode = {[47]=0,[48]=1}; project_harm_mask = 0
SetCommand(commands.WEAPON_RELEASE, 1); SetCommand(commands.WEAPON_RELEASE, 0); advance(45)
check(forwarded_count(3003, 1) == 1 and value(1018) == 0 and value(1019) == 0 and value(1020) == 0,
    "a release without a project HARM moved a trapeze")
package.loadlib = original_loadlib

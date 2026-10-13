-- SPDX-License-Identifier: MIT
-- F-23 exterior weapon-door sequencer. The installed Hornet remains the sole
-- owner of SMS, ammunition, launch authorization, guidance and ballistics.
-- Missile selection leaves bays closed. One trigger edge queues door opening,
-- a native firing pulse, and prompt closure. IR stations move with their doors.

local UPDATE_STEP = 0.01
local HUD_SAMPLE_INTERVAL_TICKS = 5
local HUD_INDICATION_ID = 1

local MAIN_BAY_ARGUMENT = 1009
local GUN_DOOR_ARGUMENT = 1010
local F23B_LEFT_NOZZLE_ARGUMENT = 1011
local F23B_RIGHT_NOZZLE_ARGUMENT = 1012
local IR_CARRIER_ARGUMENT = 1013
local LIGHT_BLACKOUT_ARGUMENT = 1021

local HORNET_HOTAS_DEVICE = 13
local HORNET_TRIGGER_SECOND_DETENT = 3002
local HORNET_WEAPON_RELEASE = 3003
local HORNET_SELECT_SPARROW = 3009
local HORNET_SELECT_GUN = 3010
local HORNET_SELECT_AMRAAM = 3011
local HORNET_SELECT_SIDEWINDER = 3012
local HORNET_GUN_EFFECT_ARGUMENT = 350
local HORNET_LEFT_NOZZLE_ARGUMENT = 90
local HORNET_RIGHT_NOZZLE_ARGUMENT = 89

local MODE_NONE = 0
local MODE_MISSILE = 1
local MODE_GUN = 2
local PRESS_THRESHOLD = 0.5
local MAIN_BAY_OPEN_RATE = 5.0
local MAIN_BAY_CLOSE_RATE = 5.0
local MAIN_BAY_POST_TRIGGER_HOLD_SECONDS = 0.25
local TRIGGER_TAP_SECONDS = 0.05
-- Live release delays: IR about 0.20 s; MALICE about 1.11 s. Preserve a
-- bounded consent pulse through those native delays, independent of button up.
local IR_TRIGGER_SECONDS = 0.30
local RADAR_TRIGGER_SECONDS = 1.25
-- PROJECT CHOICE: one bounded A/G consent pulse behind the fully open bay.
local AG_RELEASE_SECONDS = 1.25
local GUN_DOOR_OPEN_RATE = 12.5
local GUN_DOOR_CLOSE_RATE = 12.5
local GUN_EFFECT_HOLD_SECONDS = 0.65

-- Passive SERN presentation: latch out native jitter, then ease continuously
-- toward that target. No peak hold or stop/start deadband on the moving flap.
local NOZZLE_DEADBAND = 0.03
local NOZZLE_OPEN_RATE = 2.0
local NOZZLE_CLOSE_RATE = 0.60
local NOZZLE_OPEN_RESPONSE_SECONDS = 0.20
local NOZZLE_CLOSE_RESPONSE_SECONDS = 0.30

local bridge_commands = dofile(
    LockOn_Options.script_path .. "BridgeCommands.lua")
local self = GetSelf()
local update_radar_upgrade = dofile(LockOn_Options.script_path .. "RadarUpgrade.lua")
-- Native Hornet seeker cue: point the Sidewinder seeker at a verified hostile radar target.
local ir_seeker_azimuth = get_param_handle("WS_IR_MISSILE_SEEKER_DESIRED_AZIMUTH")
local ir_seeker_elevation = get_param_handle("WS_IR_MISSILE_SEEKER_DESIRED_ELEVATION")
self:listen_command(bridge_commands.TRIGGER_SECOND_DETENT)
self:listen_command(bridge_commands.SELECT_SIDEWINDER)
self:listen_command(bridge_commands.SELECT_AMRAAM)
self:listen_command(bridge_commands.SELECT_GUN)
self:listen_command(bridge_commands.SELECT_SPARROW)
self:listen_command(bridge_commands.WEAPON_RELEASE)

local state = {
    main_bay = 0.0,
    gun_door = 0.0,
    selected_mode = MODE_NONE,
    selected_weapon = "UNSET",
    selection_command_seen = false,
    trigger_input = false,
    trigger_pending = false,
    native_trigger_down = false,
    trigger_tap_remaining = 0.0,
    release_input = false,
    release_pending = false,
    native_release_down = false,
    release_remaining = 0.0,
    main_bay_hold = 0.0,
    gun_effect_hold = 0.0,
    hud_sample_ticks = HUD_SAMPLE_INTERVAL_TICKS,
    hud_signature = nil,
    delivered_main_bay = nil,
    delivered_gun_door = nil,
    delivered_probe_pending = false,
    visual_probe_pending = false,
    persistence_mismatch_noted = false,
    hotas = nil,
    hotas_error_noted = false,
    previous_gun_effect = false,
    previous_left_nozzle = -1.0,
    previous_right_nozzle = -1.0,
    filtered_left_nozzle = 0.0,
    filtered_right_nozzle = 0.0,
    left_nozzle_target = 0.0,
    right_nozzle_target = 0.0,
}

local sensor_data = get_base_data()

local function note(message)
    if type(log) == "table" and type(log.write) == "function" then
        log.write("F23B_EXT_VIS", log.INFO, message)
    end
end

local function clamp01(value)
    if value < 0.0 then return 0.0 end
    if value > 1.0 then return 1.0 end
    return value
end

local function move_toward(value, target, rate)
    local step = rate * UPDATE_STEP
    if value < target then return math.min(target, value + step) end
    if value > target then return math.max(target, value - step) end
    return value
end

local function filter_nozzle(value, raw, target)
    raw = clamp01(raw)
    if math.abs(raw - target) > NOZZLE_DEADBAND or raw == 0.0 or raw == 1.0 then
        target = raw
    end
    local delta = target - value
    if math.abs(delta) < 0.00001 then return target, target end
    local response = delta > 0.0 and NOZZLE_OPEN_RESPONSE_SECONDS
        or NOZZLE_CLOSE_RESPONSE_SECONDS
    local rate = delta > 0.0 and NOZZLE_OPEN_RATE or NOZZLE_CLOSE_RATE
    local eased = value + delta * (1.0 - math.exp(-UPDATE_STEP / response))
    return move_toward(value, eased, rate), target
end

local function write_argument(argument, value)
    if type(set_aircraft_draw_argument_value) == "function" then
        set_aircraft_draw_argument_value(argument, clamp01(value))
    end
end

local function read_aircraft_argument(argument)
    if type(get_aircraft_draw_argument_value) ~= "function" then return 0.0 end
    return get_aircraft_draw_argument_value(argument) or 0.0
end

local function update_light_blackout()
    -- Installed MainPanel/lamps.lua maps native A/A and A/G status to 47/48.
    -- Refresh before reading: without update_arguments the cache stays stale.
    -- These status values survive HUD-off and minimum cockpit lighting.
    local ok, aa, ag = pcall(function()
        local panel = GetDevice(0)
        panel:update_arguments()
        return panel:get_argument_value(47), panel:get_argument_value(48)
    end)
    local blackout = not ok or type(aa) ~= "number" or type(ag) ~= "number"
        or aa > 0.001 or ag > 0.001
    write_argument(LIGHT_BLACKOUT_ARGUMENT, blackout and 1.0 or 0.0)
    -- Both lamps on can mean lamp test. Unknown/ambiguous mode must not
    -- send an automatic HOTAS command into another native sensor mode.
    local known = ok and type(aa) == "number" and type(ag) == "number"
    return known and aa > 0.001 and ag <= 0.001,
        known and ag > 0.001 and aa <= 0.001
end

local function trim(value)
    if type(value) ~= "string" then return nil end
    local result = string.gsub(value, "^%s+", "")
    result = string.gsub(result, "%s+$", "")
    if result == "" then return nil end
    return result
end

local function indication_value(indication, element_name)
    if type(indication) ~= "string" or indication == "" then return nil end
    local normalized = string.gsub(indication, "\r", "")
    for key, value in string.gmatch(
            normalized, "-----------------------------------------\n([^\n]+)\n([^\n]*)\n") do
        if key == element_name then return value end
    end
    return nil
end

local function sample_hud()
    if type(list_indication) ~= "function" then return end
    local ok, indication = pcall(list_indication, HUD_INDICATION_ID)
    if not ok or type(indication) ~= "string" or indication == "" then return end
    local gun_label = trim(indication_value(indication, "GUN_AA_label"))
    local weapon_type = trim(indication_value(indication, "AA_Weapon_type"))
    local weapon_count = trim(indication_value(indication, "AA_Weapon_count"))
    local hud_mode = MODE_NONE
    if gun_label == "GUN" then
        hud_mode = MODE_GUN
    elseif weapon_type ~= nil and (tonumber(weapon_count) or 1) > 0 then
        hud_mode = MODE_MISSILE
    end
    local signature = string.format("mode=%d weapon=%s count=%s",
        hud_mode, tostring(weapon_type), tostring(weapon_count))
    if signature ~= state.hud_signature then
        state.hud_signature = signature
        note("Hornet HUD diagnostic state: " .. signature)
    end
    -- HUD text can seed the initial mode only. Once a wrapped HOTAS selection
    -- arrives, stale display text can never overwrite command state.
    if not state.selection_command_seen and hud_mode ~= MODE_NONE then
        state.selected_mode = hud_mode
        state.selected_weapon = hud_mode == MODE_GUN and "GUN"
            or (weapon_type or "MISSILE")
    end
end

local function acquire_hotas()
    if state.hotas ~= nil then return state.hotas end
    if type(GetDevice) ~= "function" then return nil end
    local ok, device = pcall(GetDevice, HORNET_HOTAS_DEVICE)
    if ok and device ~= nil
        and type(device.performClickableAction) == "function" then
        state.hotas = device
        return device
    end
    return nil
end

local function forward_hornet(command, value, label, quiet)
    local hotas = acquire_hotas()
    if hotas == nil then
        if not state.hotas_error_noted then
            state.hotas_error_noted = true
            note("ERROR: installed Hornet HOTAS device 13 is unavailable")
        end
        return false
    end
    -- DCS performs this side effect but may return false even when the action
    -- was accepted. The protected-call status is the contract; treating the
    -- method's return value as an acknowledgement repeats trigger-down every
    -- frame and prevents the matching release from ever being sent.
    local ok, failure = pcall(hotas.performClickableAction, hotas,
        command, value, false)
    if not ok then
        note("ERROR: Hornet HOTAS forwarding failed for " .. label
            .. ": " .. tostring(failure))
        return false
    end
    if not quiet then
        note(string.format("forwarded Hornet HOTAS: %s command=%d value=%.1f",
            label, command, value))
    end
    return true
end

local function end_native_trigger()
    if state.native_trigger_down then
        if forward_hornet(HORNET_TRIGGER_SECOND_DETENT, 0.0,
                "trigger-second-detent") then
            state.native_trigger_down = false
            state.main_bay_hold = MAIN_BAY_POST_TRIGGER_HOLD_SECONDS
            return true
        end
        return false
    end
    return true
end

local function set_weapon_mode(mode, name)
    -- A wrapper event is authoritative even if it happens to agree with the
    -- initial HUD seed. Once seen, a stale HUD page must never retake control.
    state.selection_command_seen = true
    -- An explicit missile selection supersedes the bounded post-fire gun
    -- fallback. Clear it so the gun door closes immediately and cannot route
    -- the next missile trigger through the wrong opening.
    if mode ~= MODE_GUN then state.gun_effect_hold = 0.0 end
    if mode ~= state.selected_mode or name ~= state.selected_weapon then
        end_native_trigger()
        state.trigger_pending = false
        state.trigger_tap_remaining = 0.0
        state.main_bay_hold = 0.0
        state.selected_mode = mode
        state.selected_weapon = name
        state.visual_probe_pending = true
        note("authoritative HOTAS weapon selection: " .. name)
    end
end

local function end_native_release()
    if state.native_release_down then
        if not forward_hornet(HORNET_WEAPON_RELEASE, 0.0, "weapon-release") then
            return false
        end
        state.native_release_down = false
        state.main_bay_hold = MAIN_BAY_POST_TRIGGER_HOLD_SECONDS
    end
    return true
end

local selection_routes = {
    [bridge_commands.SELECT_SIDEWINDER] = {
        mode = MODE_MISSILE, name = "SIDEWINDER",
        hornet = HORNET_SELECT_SIDEWINDER,
    },
    [bridge_commands.SELECT_AMRAAM] = {
        mode = MODE_MISSILE, name = "AMRAAM",
        hornet = HORNET_SELECT_AMRAAM,
    },
    [bridge_commands.SELECT_GUN] = {
        mode = MODE_GUN, name = "GUN", hornet = HORNET_SELECT_GUN,
    },
    [bridge_commands.SELECT_SPARROW] = {
        mode = MODE_MISSILE, name = "SPARROW",
        hornet = HORNET_SELECT_SPARROW,
    },
}

function SetCommand(command, value)
    if command == bridge_commands.WEAPON_RELEASE then
        local _, air_to_ground = update_light_blackout()
        local pressed = value >= PRESS_THRESHOLD
        if air_to_ground then
            if pressed and not state.release_input then
                state.release_pending = true
                state.visual_probe_pending = true
                note("sequenced A/G weapon-release request")
            end
        elseif not state.release_pending and not state.native_release_down then
            forward_hornet(HORNET_WEAPON_RELEASE, value, "weapon-release")
        end
        state.release_input = pressed
        return
    end
    local route = selection_routes[command]
    if route ~= nil then
        if value >= PRESS_THRESHOLD then
            set_weapon_mode(route.mode, route.name)
        end
        forward_hornet(route.hornet, value, "select-" .. route.name)
        return
    end
    if command ~= bridge_commands.TRIGGER_SECOND_DETENT then return end
    local pressed = value >= PRESS_THRESHOLD
    if pressed and not state.trigger_input then
        state.trigger_input = true
        state.trigger_pending = true
        state.trigger_tap_remaining = 0.0
        state.main_bay_hold = 0.0
        state.visual_probe_pending = true
        note("sequenced trigger request: weapon=" .. state.selected_weapon)
    elseif not pressed and state.trigger_input then
        state.trigger_input = false
        if state.native_trigger_down and state.trigger_tap_remaining <= 0.0 then
            end_native_trigger()
        end
        -- A tap shorter than door travel remains queued and becomes one
        -- bounded native trigger pulse after the door reaches open.
    end
end

-- Restore the player pose before device post-initialization. AI keeps its livery pose.
write_argument(IR_CARRIER_ARGUMENT, 0.0)
note("player IR carrier pose at device load: argument1013=" .. read_aircraft_argument(IR_CARRIER_ARGUMENT))

make_default_activity(UPDATE_STEP)

function post_initialize()
    local build = dofile(LockOn_Options.script_path .. "BuildInfo.lua")
    note("BUILD source_commit=" .. build.source_commit
        .. " build_id=" .. build.build_id
        .. " asset_lock_sha256=" .. build.asset_lock_sha256)
    state.main_bay = 0.0
    state.gun_door = 0.0
    state.filtered_left_nozzle = clamp01(read_aircraft_argument(
        HORNET_LEFT_NOZZLE_ARGUMENT))
    state.filtered_right_nozzle = clamp01(read_aircraft_argument(
        HORNET_RIGHT_NOZZLE_ARGUMENT))
    state.previous_left_nozzle = state.filtered_left_nozzle
    state.previous_right_nozzle = state.filtered_right_nozzle
    state.left_nozzle_target = state.filtered_left_nozzle
    state.right_nozzle_target = state.filtered_right_nozzle
    state.previous_gun_effect = read_aircraft_argument(
        HORNET_GUN_EFFECT_ARGUMENT) > 0.01
    acquire_hotas()
    write_argument(MAIN_BAY_ARGUMENT, 0.0)
    write_argument(GUN_DOOR_ARGUMENT, 0.0)
    write_argument(IR_CARRIER_ARGUMENT, 1.0)
    write_argument(LIGHT_BLACKOUT_ARGUMENT, 1.0)
    write_argument(F23B_LEFT_NOZZLE_ARGUMENT, state.filtered_left_nozzle)
    write_argument(F23B_RIGHT_NOZZLE_ARGUMENT, state.filtered_right_nozzle)
    state.delivered_main_bay = 0.0
    state.delivered_gun_door = 0.0
    note("door-first Hornet trigger sequencer initialized")
end

function update()
    local air_to_air, air_to_ground = update_light_blackout()
    if not air_to_ground then
        state.release_pending = false
        state.release_remaining = 0.0
        end_native_release()
    end
    if state.delivered_main_bay ~= nil and state.delivered_gun_door ~= nil then
        local persisted_main = read_aircraft_argument(MAIN_BAY_ARGUMENT)
        local persisted_gun = read_aircraft_argument(GUN_DOOR_ARGUMENT)
        if state.delivered_probe_pending then
            note(string.format(
                "cross-tick delivery probe: written1009=%.3f persisted1009=%.3f written1010=%.3f persisted1010=%.3f",
                state.delivered_main_bay, persisted_main,
                state.delivered_gun_door, persisted_gun))
        end
        if not state.persistence_mismatch_noted
            and (math.abs(persisted_main - state.delivered_main_bay) > 0.02
                or math.abs(persisted_gun - state.delivered_gun_door) > 0.02) then
            state.persistence_mismatch_noted = true
            note("WARNING: exterior argument write did not persist to the following update tick")
        end
    end
    state.delivered_probe_pending = false

    state.hud_sample_ticks = state.hud_sample_ticks + 1
    if state.hud_sample_ticks >= HUD_SAMPLE_INTERVAL_TICKS then
        state.hud_sample_ticks = 0
        sample_hud()
    end

    local gun_effect = read_aircraft_argument(HORNET_GUN_EFFECT_ARGUMENT) > 0.01
    if gun_effect then
        state.gun_effect_hold = GUN_EFFECT_HOLD_SECONDS
        if not state.previous_gun_effect then
            state.visual_probe_pending = true
            note("authoritative native gun effect: argument 350")
        end
    else
        state.gun_effect_hold = math.max(0.0,
            state.gun_effect_hold - UPDATE_STEP)
    end
    state.main_bay_hold = math.max(0.0,
        state.main_bay_hold - UPDATE_STEP)

    local release_route = state.release_pending or state.native_release_down
    local gun_route = not release_route and ((not air_to_ground and state.selected_mode == MODE_GUN)
        or gun_effect or state.gun_effect_hold > 0.0)

    -- Exclude friendly/neutral tracks before native automatic ranking.
    -- The gun route bypasses ranking. IR keeps native seeker acquisition.
    local ir_weapon = state.selected_weapon == "SIDEWINDER"
        or state.selected_weapon == "9X" or state.selected_weapon == "9M"
    local radar_weapon = air_to_air and state.selected_mode == MODE_MISSILE
        and not ir_weapon and not gun_route
    local radar_target, target_side, cue_az, cue_el, cue_valid = update_radar_upgrade(
        air_to_air and not gun_route, ir_weapon and air_to_air)
    if ir_weapon and cue_valid and target_side == 2 then
        ir_seeker_azimuth:set(cue_az)
        ir_seeker_elevation:set(cue_el)
    end
    if radar_weapon and target_side ~= 2 then
        if state.trigger_pending or state.native_trigger_down then
            state.trigger_pending = false
            state.trigger_tap_remaining = 0.0
            end_native_trigger()
            note("radar shot cancelled: no verified opposing target; id="
                .. tostring(radar_target) .. " side=" .. tostring(target_side))
            if type(print_message_to_user) == "function" then
                print_message_to_user("F-23B: radar shot inhibited. No verified hostile target.")
            end
        end
    end
    local main_route = release_route or (not gun_route and (state.trigger_pending
        or state.native_trigger_down or state.main_bay_hold > 0.0))
    local main_target = main_route and 1.0 or 0.0
    local gun_target = gun_route and 1.0 or 0.0

    state.main_bay = move_toward(state.main_bay, main_target,
        main_target > state.main_bay and MAIN_BAY_OPEN_RATE
            or MAIN_BAY_CLOSE_RATE)
    state.gun_door = move_toward(state.gun_door, gun_target,
        gun_target > state.gun_door and GUN_DOOR_OPEN_RATE
            or GUN_DOOR_CLOSE_RATE)
    -- Publish the open pose before the Hornet receives the firing edge.
    -- Inspecting internal state alone leaves DCS one update behind.
    write_argument(MAIN_BAY_ARGUMENT, state.main_bay)
    write_argument(GUN_DOOR_ARGUMENT, state.gun_door)
    -- Keep the acquisition reference correction at its operational endpoint.
    -- The connector remains parented to the door; 1009 alone moves the store.
    write_argument(IR_CARRIER_ARGUMENT, 1.0)

    if state.release_pending and state.main_bay >= 0.999 then
        if forward_hornet(HORNET_WEAPON_RELEASE, 1.0, "weapon-release") then
            state.release_pending = false
            state.native_release_down = true
            state.release_remaining = AG_RELEASE_SECONDS
            note("door locked open before A/G release: argument=1009")
        end
    end
    if state.release_remaining > 0.0 then
        state.release_remaining = math.max(0.0, state.release_remaining - UPDATE_STEP)
        if state.release_remaining == 0.0 then end_native_release() end
    elseif state.native_release_down then
        end_native_release()
    end

    if state.trigger_pending then
        local ready = (gun_route and state.gun_door >= 0.999)
            or (not gun_route and state.main_bay >= 0.999)
        if ready and forward_hornet(HORNET_TRIGGER_SECOND_DETENT, 1.0,
                "trigger-second-detent") then
            state.trigger_pending = false
            state.native_trigger_down = true
            note(string.format(
                "door locked open before trigger: argument=%d weapon=%s",
                gun_route and GUN_DOOR_ARGUMENT or MAIN_BAY_ARGUMENT,
                state.selected_weapon))
            if not gun_route or not state.trigger_input then
                local ir_weapon = state.selected_weapon == "SIDEWINDER"
                    or state.selected_weapon == "9X" or state.selected_weapon == "9M"
                state.trigger_tap_remaining = gun_route and TRIGGER_TAP_SECONDS
                    or (ir_weapon and IR_TRIGGER_SECONDS or RADAR_TRIGGER_SECONDS)
            end
        end
    end
    if state.trigger_tap_remaining > 0.0 then
        state.trigger_tap_remaining = math.max(0.0,
            state.trigger_tap_remaining - UPDATE_STEP)
        if state.trigger_tap_remaining == 0.0 then end_native_trigger() end
    end
    -- A genuine Lua error must not strand the native trigger down. Keep
    -- retrying only its idempotent release after the pilot lets go.
    if state.native_trigger_down and not state.trigger_input
        and state.trigger_tap_remaining <= 0.0 then
        end_native_trigger()
    end

    local left_nozzle = read_aircraft_argument(HORNET_LEFT_NOZZLE_ARGUMENT)
    local right_nozzle = read_aircraft_argument(HORNET_RIGHT_NOZZLE_ARGUMENT)
    state.filtered_left_nozzle, state.left_nozzle_target = filter_nozzle(
        state.filtered_left_nozzle, left_nozzle, state.left_nozzle_target)
    state.filtered_right_nozzle, state.right_nozzle_target = filter_nozzle(
        state.filtered_right_nozzle, right_nozzle,
        state.right_nozzle_target)
    if math.abs(left_nozzle - state.previous_left_nozzle) >= 0.04
        or math.abs(right_nozzle - state.previous_right_nozzle) >= 0.04 then
        note(string.format(
            "nozzle sample: raw_left=%.3f raw_right=%.3f visual_left=%.3f visual_right=%.3f rpm_left=%.1f rpm_right=%.1f",
            left_nozzle, right_nozzle, state.filtered_left_nozzle,
            state.filtered_right_nozzle, sensor_data.getEngineLeftRPM(),
            sensor_data.getEngineRightRPM()))
        state.previous_left_nozzle = left_nozzle
        state.previous_right_nozzle = right_nozzle
    end

    write_argument(F23B_LEFT_NOZZLE_ARGUMENT, state.filtered_left_nozzle)
    write_argument(F23B_RIGHT_NOZZLE_ARGUMENT, state.filtered_right_nozzle)
    state.delivered_main_bay = state.main_bay
    state.delivered_gun_door = state.gun_door
    state.delivered_probe_pending = state.visual_probe_pending
    state.visual_probe_pending = false
    state.previous_gun_effect = gun_effect
end

need_to_be_closed = false

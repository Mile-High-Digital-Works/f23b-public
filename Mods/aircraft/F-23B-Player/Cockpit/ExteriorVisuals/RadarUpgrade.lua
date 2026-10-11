-- SPDX-License-Identifier: MIT
-- Loaded only by the F-23B-specific plugin system. No cockpit command is required.
local native_update
local attempted = false
local last_status
local last_target, last_side
local ranking_failed = false
local support_reported, support_target, profile_reported, alias_reported
local alias_ticks = 0
local energy_reported, sa_range_reported, decoy_reported, flight_decoy_reported
local connection_off = "F-23B weapon and radar connection OFF. MALICE, Block II, radar upgrades and SA 640 are unavailable. "
    .. "Flight, the native Hornet cockpit and radar, bays, lights and liveries remain available. "
    .. "Get a compatible F-23B release: github.com/Coaokalo/f23b-public/releases/latest"
local function report(message, failed, player_message)
    if type(log) == "table" and type(log.write) == "function" then
        log.write("F23B_RADAR", failed and log.ERROR or log.INFO, message)
    end
    if failed and type(print_message_to_user) == "function" then
        print_message_to_user(player_message or ("F-23B: " .. message .. ". See dcs.log."))
    end
end
local function connection_failed(message)
    report(message, true, connection_off)
end
local function check_old_connection()
    -- Read only. A slow DCS repair restores the stock file; the ZIP cannot do so.
    if type(io) ~= "table" or type(io.open) ~= "function" then return end
    pcall(function()
        local f = io.open("Mods/aircraft/FA-18C/Cockpit/Scripts/device_init.lua", "r")
        if not f then return end
        local content = f:read(262144); f:close()
        if content and content:find("-- BEGIN F23B INDEPENDENT WEAPONS", 1, true) then
            report("Earlier F-23B cockpit connection found", true,
                "Old F-23B cockpit connection found. Close DCS. Run Repair with 'Check all files (slow)' once, then reinstall F-23B. See INSTALL.md.")
        end
    end)
end
return function(ranking_scope, cue_ir)
    if not attempted then
        attempted = true
        check_old_connection()
        local path = LockOn_Options.script_path .. "../../bin/F23B_Radar.dll"
        if type(package) ~= "table" or type(package.loadlib) ~= "function" then
            connection_failed("Native library loading is unavailable"); return
        end
        local loaded, loader, reason = pcall(package.loadlib, path, "luaopen_f23b_radar")
        if not loaded or not loader then
            connection_failed("Load failed: " .. tostring(reason or loader)); return
        end
        local ok, result = pcall(loader)
        if not ok or type(result) ~= "function" then
            connection_failed("Initialization failed: " .. tostring(result)); return
        end
        native_update = result
    end
    if not native_update then return end
    local ok, status, target, side, ranking, az, el, cue_valid, support, grants, granted, profile, diag,
        alias, malice_type, blockii_type, alias_diag, launch_zone_diag, energy_status, energy_message, energy_diag,
        sa_range_status, decoy_status, decoy_rejects, flight_decoy_status, flight_decoy_rejects = pcall(
        native_update, ranking_scope == true, cue_ir == true)
    if not ok then
        connection_failed("Update failed: " .. tostring(status))
        native_update = nil
        return
    end
    if status ~= last_status then
        if status == 1 then
            report("Radar upgrade active: RWS/TWS scan 130 deg/s; detector gate 16 ms; signal margin +3 dB", false)
        elseif status < 0 then
            connection_failed("Compatibility or native-state check failed: " .. tostring(status))
            native_update = nil
            return nil, -1
        elseif last_status == 1 then
            report("Native settings restored outside RWS/TWS", false)
        end
        last_status = status
    end
    if target ~= last_target or side ~= last_side then
        report("Native target=" .. tostring(target) .. " coalition_class=" .. tostring(side), false)
        last_target, last_side = target, side
    end
    -- Missile support extension: native answer first; F-23B radar keeps support for a
    -- missile's own target while that target remains a radar or network trackfile.
    if support ~= nil and support ~= support_reported then
        report(support == 1 and "Missile support extension active: fired-upon MSI trackfiles"
            or "Missile support extension unavailable; native Hornet support only", false)
        support_reported = support
    end
    if granted and granted ~= 0 and granted ~= support_target then
        report("Missile support extended: target=" .. tostring(granted)
            .. " grants=" .. tostring(grants), false)
        support_target = granted
    end
    -- MALICE profile: widened INS limits, 15-degree seeker FOV, earlier handoff and seeker reference, MALICE only.
    if profile ~= nil and profile ~= profile_reported and profile ~= 0 then
        report(profile == 1 and ("MALICE profile active: update window 60 s, INS time 600 s, seeker FOV 15 deg, "
            .. "handoff 29.6 km, 5 m2 reference 40 km, Doppler filter 4 m/s")
            or ("MALICE profile unavailable (code " .. tostring(profile) .. "); native INS and seeker values apply"), false)
        profile_reported = profile
    end
    -- Read-only diagnostics per supported missile target: decision, trackfile state and source ages.
    if type(diag) == "string" then
        report("Missile support diag: " .. diag, false)
    end
    if type(launch_zone_diag) == "string" then
        report("Launch zone diag: " .. launch_zone_diag, false)
    end
    if energy_status and energy_status ~= 0 and energy_status ~= energy_reported then
        report(energy_status == 1 and "MALICE energy profile active: dynamic loft; second pulse at seeker activation"
            or ("MALICE energy profile unavailable (code " .. tostring(energy_status) .. "); no native energy change"), energy_status < 0)
        energy_reported = energy_status
    end
    if sa_range_status and sa_range_status ~= 0 and sa_range_status ~= sa_range_reported then
        report(sa_range_status == 1 and "SA scale extended to 640 NM (SCL cycles 40, 20, 10, 5, 640, 320)"
            or ("SA scale extension unavailable (code " .. tostring(sa_range_status) .. "); stock 320 NM limit"), sa_range_status < 0)
        sa_range_reported = sa_range_status
    end
    -- Block II flare rejection: the Hornet cockpit seeker does not take a flare as its target before launch.
    if decoy_status and decoy_status ~= 0 and decoy_status ~= decoy_reported then
        report(decoy_status == 1 and "Block II flare rejection active: the cockpit seeker ignores flares before launch"
            or ("Block II flare rejection unavailable (code " .. tostring(decoy_status) .. "); native seeker behaviour"),
            decoy_status < 0)
        decoy_reported = decoy_status
    end
    if flight_decoy_status and flight_decoy_status ~= 0 and flight_decoy_status ~= flight_decoy_reported then
        report(flight_decoy_status == 1 and "Block II flare rejection active in flight: a launched Block II ignores flares"
            or ("Block II in-flight flare rejection unavailable (code " .. tostring(flight_decoy_status)
                .. "); native missile behaviour"), flight_decoy_status < 0)
        flight_decoy_reported = flight_decoy_status
    end
    if ((decoy_rejects or 0) > 0 or (flight_decoy_rejects or 0) > 0) and alias_ticks % 1000 == 1 then
        report(string.format("Block II flare rejection diag: seeker=%d flight=%d", decoy_rejects or 0,
            flight_decoy_rejects or 0), false)
    end
    if type(energy_message) == "string" then report(energy_message, energy_message:sub(1, 5) == "ERROR") end
    if type(energy_diag) == "string" and alias_ticks % 1000 == 1 then report("MALICE energy diag: " .. energy_diag, false) end
    -- Independent weapons: the Hornet sees MALICE as its AMRAAM and Block II as its AIM-9X store.
    -- The F-23B Player entry script connects them before the cockpit initializes; this reports the
    -- state. Codes 2 and 3 are transient: waiting for FA18C.dll, then for the first payload request.
    if alias ~= nil and alias ~= alias_reported and alias ~= 0 and alias ~= -21 then
        if alias == 1 then
            report("Independent weapons connected: MALICE type " .. tostring(malice_type)
                .. ", Block II type " .. tostring(blockii_type) .. " use the native Hornet weapon path", false)
        elseif alias == 2 or alias == 3 then
            report("Independent weapons pending (code " .. tostring(alias) .. ")", false)
        else
            report("Independent weapons unavailable (code " .. tostring(alias) .. ")", true)
        end
        alias_reported = alias
    end
    alias_ticks = alias_ticks + 1
    if type(alias_diag) == "string" and alias_ticks % 250 == 1 then
        report("Independent weapons diag: " .. alias_diag, false)
    end
    if ranking and ranking < 0 and not ranking_failed then
        report("Native ranking exclusion failed its memory checks; launch guard remains active", true)
        ranking_failed = true
    end
    return target, side, az, el, cue_valid == 1
end

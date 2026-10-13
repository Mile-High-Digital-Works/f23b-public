-- SPDX-License-Identifier: MIT
-- Rewrite only the Hornet A/A select and trigger rows. The caller externally
-- composes the installed Hornet profile first, so every unrelated Hornet
-- binding and axis remains represented by the installed module. The caller
-- also resolves BridgeCommands from its DCS-provided Saved Games folder.
return function(profile, devices, hotas_commands, commands, native_key_diffs)
    if type(profile) ~= "table" or type(devices) ~= "table"
        or type(hotas_commands) ~= "table" or type(commands) ~= "table" then
        error("F-23B input bridge received an incomplete Hornet profile contract")
    end
    local replacements = {
        [hotas_commands.STICK_TRIGGER_2ND_DETENT] =
            commands.TRIGGER_SECOND_DETENT,
        [hotas_commands.STICK_WEAPON_RELEASE] = commands.WEAPON_RELEASE,
        [hotas_commands.STICK_WEAPON_SELECT_DOWN] = commands.SELECT_SIDEWINDER,
        [hotas_commands.STICK_WEAPON_SELECT_IN] = commands.SELECT_AMRAAM,
        [hotas_commands.STICK_WEAPON_SELECT_AFT] = commands.SELECT_GUN,
        [hotas_commands.STICK_WEAPON_SELECT_FWD] = commands.SELECT_SPARROW,
    }
    local replaced = {}
    for _, row in ipairs(profile.keyCommands or {}) do
        local native = row.down
        local replacement = replacements[native]
        if replacement ~= nil and row.cockpit_device_id == devices.HOTAS then
            -- Preserve an existing native release assignment without writing
            -- the owner's diff file. DCS hashes the numeric command/device,
            -- so the old release row would otherwise lose its saved combos.
            -- Only this newly wrapped command needs the compatibility read.
            if native == hotas_commands.STICK_WEAPON_RELEASE and native_key_diffs then
                local hash = "d" .. tostring(row.down) .. "p" .. tostring(row.pressed)
                    .. "u" .. tostring(row.up) .. "cd" .. tostring(row.cockpit_device_id)
                    .. "vd" .. tostring(row.value_down) .. "vp" .. tostring(row.value_pressed)
                    .. "vu" .. tostring(row.value_up)
                local diff = native_key_diffs[hash]
                if diff then
                    local function same(a, b)
                        if a.key ~= b.key then return false end
                        local ar, br = a.reformers or {}, b.reformers or {}
                        if #ar ~= #br then return false end
                        for _, v in ipairs(ar) do
                            local found = false
                            for _, w in ipairs(br) do if v == w then found = true end end
                            if not found then return false end
                        end
                        return true
                    end
                    row.combos = row.combos or {}
                    for _, combo in ipairs(diff.removed or {}) do
                        for i = #row.combos, 1, -1 do
                            if same(row.combos[i], combo) then table.remove(row.combos, i) end
                        end
                    end
                    for _, combo in ipairs(diff.added or {}) do
                        local found = false
                        for _, current in ipairs(row.combos) do
                            if same(current, combo) then found = true end
                        end
                        if not found then row.combos[#row.combos + 1] = combo end
                    end
                end
            end
            row.down = replacement
            if row.up == native then row.up = replacement end
            row.cockpit_device_id = nil
            replaced[native] = true
        end
    end
    for native in pairs(replacements) do
        if not replaced[native] then
            error("installed Hornet input profile is missing required HOTAS row "
                .. tostring(native))
        end
    end
    return profile
end

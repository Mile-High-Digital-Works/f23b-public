-- SPDX-License-Identifier: MIT
-- Rewrite only the Hornet A/A select and trigger rows. The caller externally
-- composes the installed Hornet profile first, so every unrelated Hornet
-- binding and axis remains represented by the installed module. The caller
-- also resolves BridgeCommands from its DCS-provided Saved Games folder.

-- DCS keys each saved binding by this hash (Scripts/Input/Utils.lua, getKeyCommandHash).
local function command_hash(row)
    return "d" .. tostring(row.down) .. "p" .. tostring(row.pressed) .. "u" .. tostring(row.up)
        .. "cd" .. tostring(row.cockpit_device_id) .. "vd" .. tostring(row.value_down)
        .. "vp" .. tostring(row.value_pressed) .. "vu" .. tostring(row.value_up)
end

local function copy(value)
    if type(value) ~= "table" then return value end
    local out = {}
    for k, v in pairs(value) do out[k] = copy(v) end
    return out
end

-- Lua text in the shape DCS writes for a .diff.lua file.
local function serialize(value, indent)
    if type(value) == "string" then return string.format("%q", value) end
    if type(value) == "number" then
        -- Whole numbers stay whole; other numbers keep full precision (axis curves).
        if value == math.floor(value) and math.abs(value) < 2 ^ 53 then
            return string.format("%d", value)
        end
        return string.format("%.17g", value)
    end
    if type(value) ~= "table" then return tostring(value) end
    local keys = {}
    for k in pairs(value) do keys[#keys + 1] = k end
    table.sort(keys, function(a, b)
        if type(a) == type(b) then return a < b end
        return type(a) == "number"
    end)
    local inner = indent .. "\t"
    local lines = { "{" }
    for _, k in ipairs(keys) do
        local key = type(k) == "string" and string.format("[%q]", k) or "[" .. tostring(k) .. "]"
        lines[#lines + 1] = inner .. key .. " = " .. serialize(value[k], inner) .. ","
    end
    lines[#lines + 1] = indent .. "}"
    return table.concat(lines, "\n")
end

local function read_saved(path)
    local f = loadfile(path)
    if not f then return nil end
    setfenv(f, {})
    local ok, result = pcall(f)
    return ok and type(result) == "table" and result or nil
end

-- DCS saves a binding as a difference from this profile, by command hash. A binding saved
-- under a native Hornet hash no longer matches a rewritten row, and the next save of the
-- controls drops it. Copy each such entry once to the rewritten hash in the owner's file,
-- before DCS reads that file. The old entries stay; a one-time backup keeps the original.
local function migrate(diff_file, moves)
    if not diff_file or not diff_file.path or type(diff_file.diff) ~= "table"
        or type(diff_file.diff.keyDiffs) ~= "table" or not next(moves) then return false end
    local ok, written = pcall(function()
        local path = diff_file.path
        local source = io.open(path, "rb")
        if not source then return false end
        local original = source:read("*a")
        source:close()
        local diff = copy(diff_file.diff)
        for new_hash, move in pairs(moves) do
            local entry = copy(diff.keyDiffs[move.old_hash])
            entry.name = move.name or entry.name
            diff.keyDiffs[new_hash] = entry
        end
        local backup_path = path .. ".f23b-before-input-migration"
        local backup = io.open(backup_path, "rb")
        if backup then
            backup:close()
        else
            backup = assert(io.open(backup_path, "wb"))
            backup:write(original)
            backup:close()
        end
        local out = assert(io.open(path, "wb"))
        out:write("local diff = " .. serialize(diff, "") .. "\nreturn diff\n")
        out:close()
        local check = read_saved(path)
        for new_hash in pairs(moves) do
            if not (check and check.keyDiffs and check.keyDiffs[new_hash]) then
                local restore = assert(io.open(path, "wb"))
                restore:write(original)
                restore:close()
                return false
            end
        end
        return true
    end)
    return ok and written == true
end

return function(profile, devices, hotas_commands, commands, native_key_diffs, diff_file)
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
    local moves, carried = {}, {}
    for _, row in ipairs(profile.keyCommands or {}) do
        local native = row.down
        local replacement = replacements[native]
        if replacement ~= nil and row.cockpit_device_id == devices.HOTAS then
            local old_hash = command_hash(row)
            row.down = replacement
            if row.up == native then row.up = replacement end
            row.cockpit_device_id = nil
            replaced[native] = true
            local new_hash = command_hash(row)
            local saved = native_key_diffs and native_key_diffs[old_hash]
            if saved and not native_key_diffs[new_hash] then
                moves[new_hash] = { old_hash = old_hash, name = row.name }
                carried[#carried + 1] = { row = row, diff = saved }
            end
        end
    end
    for native in pairs(replacements) do
        if not replaced[native] then
            error("installed Hornet input profile is missing required HOTAS row "
                .. tostring(native))
        end
    end
    -- If the owner's file cannot be updated, keep the saved assignment for this session.
    -- DCS then sees it as part of this profile and a later save of the controls drops it.
    if not migrate(diff_file, moves) then
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
        for _, item in ipairs(carried) do
            local row = item.row
            row.combos = row.combos or {}
            for _, combo in ipairs(item.diff.removed or {}) do
                for i = #row.combos, 1, -1 do
                    if same(row.combos[i], combo) then table.remove(row.combos, i) end
                end
            end
            for _, combo in ipairs(item.diff.added or {}) do
                local found = false
                for _, current in ipairs(row.combos) do
                    if same(current, combo) then found = true end
                end
                if not found then row.combos[#row.combos + 1] = combo end
            end
        end
    end
    return profile
end

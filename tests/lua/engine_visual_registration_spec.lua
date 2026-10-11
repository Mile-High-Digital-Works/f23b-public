-- SPDX-License-Identifier: MIT
local runtime_root = os.getenv("F23B_RUNTIME_ROOT") or "Mods/aircraft"
-- Execute the F-23B aircraft-database registration the way entry.lua does.
--
-- A wrapper module used to load this file, which put it in a sandbox with
-- neither pcall nor rawget; registration threw, add_aircraft was never called,
-- and DCS reported "unit F-23B not found" with no usable slot. entry.lua now
-- loads the aircraft file directly, so this runs the same direct dofile and
-- asserts the record DCS actually receives.
local module_path = (runtime_root .. "/F-23B")

local function check(condition, message)
    if not condition then error(message, 2) end
end

local function near(a, b)
    return math.abs(a - b) < 1e-9
end

current_mod_path = module_path
_ = function(text) return text end
MODULATION_AM = 0
WSTYPE_PLACEHOLDER = {}
WOLALIGHT_STROBES = 1
WOLALIGHT_LANDING_LIGHTS = 2
WOLALIGHT_TAXI_LIGHTS = 3
WOLALIGHT_NAVLIGHTS = 4
WOLALIGHT_FORMATION_LIGHTS = 5
gun_mount = function(name, ammunition, opts)
    return { name = name, ammunition = ammunition, opts = opts }
end
pylon = function(...) return { ... } end
aircraft_task = function(task) return task end
verbose_to_dmg_properties = function() return {} end

local registered = {}
add_aircraft = function(aircraft)
    registered[#registered + 1] = aircraft
    return aircraft
end

-- Match the real sandbox. dcs.log proved BOTH of these are absent when DCS
-- loads the aircraft database: pcall killed the wrapper adapter, and rawget in
-- the world-light table killed F-23B.lua itself, each producing
-- "unit F-23B not found" and no slot. A spec that leaves them defined passes on
-- a tree DCS cannot load, which is exactly how both defects shipped.
pcall = nil
xpcall = nil
rawget = nil

local visual_config = dofile(module_path .. "/Entry/VisualConfig.lua")

-- Exactly what entry.lua does.
dofile(module_path .. "/F-23B.lua")

check(#registered == 1, "expected exactly one registered aircraft, got " .. #registered)
local aircraft = registered[1]
check(aircraft.Name == "F-23B", "wrong aircraft registered: " .. tostring(aircraft.Name))

check(aircraft.RCS == 0.00001, "registered RCS differs from owner-selected project value")
local light_contract = {
    [WOLALIGHT_STROBES] = {193}, [WOLALIGHT_LANDING_LIGHTS] = {210},
    [WOLALIGHT_TAXI_LIGHTS] = {210}, [WOLALIGHT_NAVLIGHTS] = {190, 191, 192},
    [WOLALIGHT_FORMATION_LIGHTS] = {88},
}
check(aircraft.lights_data.typename == "collection", "missing native light collection")
for group, arguments in pairs(light_contract) do
    local collection = aircraft.lights_data.lights[group]
    check(collection and collection.typename == "collection", "missing light group")
    check(#collection.lights == #arguments, "duplicate or missing light controller")
    for i, argument in ipairs(arguments) do
        local light = collection.lights[i]
        check(light.argument == argument, "incorrect native light channel")
        check(light.typename == (group == WOLALIGHT_STROBES and "argnatostrobelight" or "argumentlight"),
            "exterior light must control exported model lights")
        check(light.position == nil and light.controller == nil, "obsolete fixed light remains")
        if group == WOLALIGHT_STROBES then check(light.period == 1.2, "incorrect native strobe period") end
    end
end
print("OFFLINE PASS: registered project RCS and complete Hornet model-light contract")

local nozzles = aircraft.engines_nozzles
check(type(nozzles) == "table", "engines_nozzles missing from the registered record")
check(type(nozzles[1]) == "table" and type(nozzles[2]) == "table", "F-23B must register two nozzles")

local effects = visual_config.engine_effects
check(effects.profile == "F23B_LICENSED_ENGINE_NATIVE_EFFECTS",
    "runtime did not select the purchased-engine native effects profile")
for index = 1, 2 do
    local nozzle = nozzles[index]
    local label = "nozzle " .. index .. ": "
    check(nozzle.afterburner_effect_texture == "afterburner_f-18c", label .. "does not use the installed Hornet volume texture")
    check(near(nozzle.diameter, 0.5106), label .. "does not use the September 4 plume diameter")
    check(near(nozzle.elevation, -1.5), label .. "does not use the Hornet elevation")
    check(near(nozzle.exhaust_length_ab, 2.67), label .. "must retain the September 4 plume length")
    check(near(nozzle.pos[1], -6.90), label .. "plume must start at the new nozzle exit")
    check(near(nozzle.exhaust_length_ab_K, 0.707), label .. "does not use the September 4 AB gain")
    check(near(nozzle.smokiness_level, 0.05), label .. "does not use the Hornet smoke level")
    check(nozzle.afterburner_circles_count == nil,
        label .. "must use the default native shock rings")
    check(type(nozzle.afterburner_light_color) == "table"
        and near(nozzle.afterburner_light_color[1], 0.18)
        and near(nozzle.afterburner_light_color[2], 0.07)
        and near(nozzle.afterburner_light_color[3], 0.02),
        label .. "must use the accepted dim orange light that does not flood the airframe")
    check(near(nozzle.pos[1], effects.origins[index][1]), label .. "origin x not applied")
    check(near(nozzle.pos[2], effects.origins[index][2]), label .. "origin y not applied")
    check(near(nozzle.pos[3], effects.origins[index][3]), label .. "origin z not applied")
end
check(aircraft.SFM_Data.engine.type == "TurboFan",
    "SFM engine type must be TurboFan so DCS treats the stack as a turbofan")

local left = nozzles[1]
local right = nozzles[2]
check(near(left.pos[1], right.pos[1])
    and math.abs(left.pos[2] - right.pos[2]) < 0.000001,
    "left and right nozzle X/Y must remain symmetric within measured tolerance")
check(left.pos[3] < 0 and right.pos[3] > 0
    and math.abs(math.abs(left.pos[3]) - math.abs(right.pos[3])) < 0.01,
    "left and right nozzle Z must follow their measured aperture centres")

local expected_launch_methods = { [1] = 0, [3] = 1, [4] = 1, [6] = 1, [9] = 0 }
for _, station in ipairs({ 1, 3, 4, 6, 9 }) do
    local pylon_record = aircraft.Pylons[station]
    check(type(pylon_record) == "table" and pylon_record[1] == station,
        "live bay station " .. station .. " is missing")
    check(pylon_record[2] == expected_launch_methods[station],
        "live bay station " .. station .. " lost its accepted rail/ejector method")
end
local gun_effect = aircraft.Guns[1].opts.effects[1]
check(gun_effect.name == "FireEffect" and gun_effect.arg == 350,
    "the native Hornet muzzle effect must remain on argument 350")
check(near(gun_effect.duration, 0.02),
    "the native Hornet muzzle effect must remain a brief 0.02-second pulse")

print("[PASS] plume: native axial/circle/light effects registered on both outlets")
print("[PASS] bay: Sidewinders use rails and AMRAAMs use ejectors; arg 350 remains a brief muzzle effect")
print("[PASS] F-23B registers through the direct entry.lua load path")

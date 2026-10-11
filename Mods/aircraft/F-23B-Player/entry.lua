-- SPDX-License-Identifier: MIT
-- Player half of the CJS-style Core/Player relationship. The Core dependency
-- supplies the distinct aircraft record and exterior. This plugin supplies
-- the activated Hornet relationship and owns no binary or donor payload.
local self_ID = "F-23B Player"
local aircraft_type = "F-23B"
local hornet_plugin = "F/A-18C"
local hornet_binary = "FA18C"
local hornet_root = "./Mods/aircraft/FA-18C"
local hornet_cockpit = hornet_root .. "/Cockpit/Scripts/"
local hornet_config = hornet_root .. "/FM/config.lua"
local hornet_comm = hornet_root .. "/comm.lua"

declare_plugin(self_ID, {
    installed = true,
    binaries = { "F23B_ForceBridge" },
    dirName = current_mod_path,
    displayName = _("F-23B Black Widow II Player"),
    fileMenuName = _("F-23B Black Widow II Player"),
    shortName = "F-23B Player",
    version = "1.5",
    state = "installed",
    developerName = _("F-23B Community Project"),
    info = _("F-23B Black Widow II player relationship. Requires the F-23B Core and an installed and activated DCS: F/A-18C."),
    rules = {
        ["F-23B Core"] = { required = true },
        [hornet_plugin] = { required = true }
    },
    InputProfiles = {
        -- Execute the complete installed Hornet profiles and replace only the
        -- A/A select/trigger rows with project-owned sequencing commands.
        [aircraft_type] = current_mod_path .. "/Input/F-23B/"
    }
})

-- CJS loads views first, then its FM configuration, assigns the installed
-- Hornet plugin/binary tuple, registers views, and finally calls make_flyable.
-- Use the installed Hornet files directly instead of redistributing its
-- cockpit, FM data, communications, or binary.
dofile(hornet_root .. "/Views.lua")
-- The Hornet eye is 0.05 m forward and 0.10 m above the cockpit origin.
-- Put that eye on the measured F-23 opening.
local f23_visual = dofile(current_mod_path .. "/../F-23B/Entry/VisualConfig.lua")
local hornet_eye = { 0.05, 0.10, 0.0 }
ViewSettings.Cockpit[1].EyePoint = {
    hornet_eye[1], hornet_eye[2], hornet_eye[3]
}
ViewSettings.Cockpit[1].CockpitLocalPoint = {
    f23_visual.cockpit_local_point[1] - hornet_eye[1],
    f23_visual.cockpit_local_point[2] - hornet_eye[2],
    f23_visual.cockpit_local_point[3] - hornet_eye[3]
}
dofile(hornet_config)

local derivative_flight_model = {}
for key, value in pairs(FA18C) do
    derivative_flight_model[key] = value
end
derivative_flight_model[1] = self_ID
derivative_flight_model[2] = "F23B_ForceBridge"
derivative_flight_model.config_path = hornet_config
derivative_flight_model.user_options = aircraft_type

-- Modest owner-test handling tune using the native registration interface
-- measured in the 334902c parameter flights. DCS axes are X roll, Y yaw,
-- Z pitch; component 4 is Ixy. Allocate a new vector so the stock Hornet
-- remains unchanged. Scale the cross term consistently with the roll axis
-- to preserve normalized roll/yaw coupling and a valid inertia tensor.
local native_inertia = FA18C.moment_of_inertia
derivative_flight_model.moment_of_inertia = {
    native_inertia[1] * 0.65,
    native_inertia[2],
    native_inertia[3] * 0.75,
    native_inertia[4] * math.sqrt(0.65)
}

-- SPDX-License-Identifier: MIT
-- Experimental measured gear fit. Execute after allocating derivative_flight_model.
-- Position/radius come from the accepted BLEND receipt. Spring/damping are
-- development estimates; this is not a claim of measured YF-23 strut behavior.
local configure_ground = function(derivative_flight_model, FA18C)
-- Provisional F-23 mass layout: ahead of the mains and on the engine-axis plane.
-- Keeping the Hornet's vertical CG with the lower F-23 thrust axes caused an
-- untrimmable low-speed power-on pitch moment at 9ac6a23. This is an explicit
-- simulator design assumption, not measured YF-23 mass-property data.
derivative_flight_model.center_of_mass = { -0.65, -0.320670754, 0.0 }
derivative_flight_model.suspension = {}
local radii = { 0.2661038514, 0.2678397436, 0.2678400503 }
local names = { 'WHEEL_F', 'WHEEL_L', 'WHEEL_R' }
for i, native in ipairs(FA18C.suspension) do
    local leg = {}
    for key, value in pairs(native) do
        if not string.find(key, 'spring2', 1, true)
            and not string.find(key, 'damper2', 1, true) then
            leg[key] = value
        end
    end
    leg.collision_shell_name = names[i]
    leg.wheel_radius = radii[i]
    leg.amortizer_min_length = 0.0
    leg.amortizer_max_length = 0.12
    leg.amortizer_basic_length = 0.12
    leg.amortizer_reduce_length = 0.06
    leg.amortizer_static_force = i == 1 and 3000.0 or 10000.0
    -- Nose: a quartic spring through the old static point (31 kN at 0.0837 m).
    -- The quadratic spring reached full 0.12 m travel near 61 kN in hard braking.
    -- This one reaches full travel near 121 kN. Ride height stays the same.
    leg.amortizer_spring_force_factor = i == 1 and 5.7142857e8 or 20e6
    leg.amortizer_spring_force_factor_rate = i == 1 and 4.0 or 2.0
    leg.allowable_hard_contact_length = 0.02
    derivative_flight_model.suspension[i] = leg
end
end

configure_ground(derivative_flight_model, FA18C)
make_view_settings(aircraft_type, ViewSettings, SnapViews)
make_flyable(aircraft_type, hornet_cockpit,
    derivative_flight_model, hornet_comm)

-- Add one exterior sequencer alongside the installed Hornet cockpit. It owns
-- only F-23 door/nozzle presentation, and forwards the original installed
-- Hornet HOTAS command after the applicable physical door reaches open.
add_plugin_systems("F-23B-Exterior-Visuals", "*",
    current_mod_path .. "/Cockpit/ExteriorVisuals/",
    { [aircraft_type] = {} })

plugin_done()

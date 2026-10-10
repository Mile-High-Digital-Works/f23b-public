-- SPDX-License-Identifier: MIT
-- Experimental measured gear fit. Execute after allocating derivative_flight_model.
-- Position/radius come from the accepted BLEND receipt. Spring/damping are
-- development estimates; this is not a claim of measured YF-23 strut behavior.
return function(derivative_flight_model, FA18C)
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

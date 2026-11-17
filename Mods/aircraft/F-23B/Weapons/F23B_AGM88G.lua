-- SPDX-License-Identifier: MIT
-- Project HARM simulation registered as an independent anti-radiation weapon.
-- The native Hornet retains cockpit weapon data through the payload integration.
-- This definition supplies projectile dynamics and the direct bay store.
-- Scalar range limits are configuration values; complete engagement ranges remain unverified.
local SIZE, MASS, MOTOR, GUIDANCE, LOAD, LIFT = .75, .75, 1.25, 1.4, 1.5, 1.64
local SHAPE = "F23B_AGM88G"
local MASS_KG, DIAMETER_M = 361 * MASS, .254 * SIZE
local MOTOR_EXIT_X = -2.1 * SIZE

local function curve(values, factor)
    local result = {}
    for i, value in ipairs(values) do result[i] = value * (factor or 1) end
    return result
end

local function motor(impulse, fuel, duration, width, opacity)
    return {
        impulse = impulse * MOTOR, fuel_mass = fuel, work_time = duration,
        nozzle_position = {{MOTOR_EXIT_X, 0, 0}}, nozzle_orientationXYZ = {{0, 0, 0}},
        nozzle_exit_area = .01368, tail_width = width,
        smoke_color = {.8, .8, .8}, smoke_transparency = opacity,
        smoke_opacity_type = 1, custom_smoke_dissipation_factor = .3,
    }
end

local function warhead()
    return {
        mass = 62.48, expl_mass = 18.42, piercing_mass = 12.496,
        caliber = 254, length = .56, fantom = 0,
        obj_factors = {1.376, 1.376}, other_factors = {1.376, 1.376, 1.376},
        concrete_factors = {1.376, 1.376, 1}, concrete_obj_factor = 1,
        cumulative_factor = 0, cumulative_thickness = 0,
    }
end

local lift = {1.14, 1.14, 1.14, 1.14, 1.14, 1.42, 1.46, 1.39, 1.32,
    1.15, 1.06, 1, .94, .89, .83, .78, .73, .69, .65, .61, .57}

local weapon = {
    category = CAT_MISSILES, name = "F23B_AGM88G", model = SHAPE,
    user_name = _("AGM-88G AARGM-ER (F-23B project-defined)"), display_name_short = "AGM-88G",
    scheme = "anti_rad_missile", class_name = "wAmmunitionAntiRad",
    wsTypeOfWeapon = {4, 4, 8, WSTYPE_PLACEHOLDER},
    M = MASS_KG, mass = MASS_KG, Diam = 254 * SIZE,
    Escort = 0, Head_Type = 3, Head_Form = 1, sigma = {10, 10, 10},
    H_max = 24400, H_min = -1, H_min_t = 0,
    D_max = 151000, D_min = 3500, Range_max = 134000,
    Life_Time = 400, Nr_max = 12 * LOAD,
    v_min = 170, v_mid = 400 * MOTOR, Mach_max = 3.2 * MOTOR,
    t_b = .02, t_acc = 5, t_marsh = 10 * 1.2,
    Fi_start = .35, Fi_rak = 3.14152, Fi_excort = 1, Fi_search = 99.9, OmViz_max = 99.9,
    Cx_pil = 4, Damage = 46.05, Reflection = .08, KillDistance = 7, SeekerGen = 3,
    default_dist_table = 1,
    exhaust = {.9, .9, .9, .2}, X_back = -1.506 * SIZE, Y_back = -.126 * SIZE, Z_back = 0,
    shape_table_data = {{name = "F23B_AGM88G", file = SHAPE, life = 1,
        fire = {0, 1}, username = "F23B_AGM88G", index = WSTYPE_PLACEHOLDER}},
    controller = {boost_start = 0, march_start = 3.4},
    boost = motor(235, 70, 3.4, .4, .2),
    march = motor(206, 57, 12.5 * 1.2, .3, .1),
    fm = {
        mass = MASS_KG, caliber = DIAMETER_M, L = DIAMETER_M, S = .051 * SIZE * SIZE,
        Ix = 3.5 * MASS * SIZE * SIZE, Iy = 320.5 * MASS * SIZE * SIZE,
        Iz = 320.5 * MASS * SIZE * SIZE,
        wind_sigma = 0, wind_time = 0, tail_first = 1, fins_part_val = 0, rotated_fins_inp = 0,
        delta_max = .34906585039887, draw_fins_conv = {1.5707963267949, 1, 1},
        Mxd = 17.19, Mxw = -44.5, model_roll = .78539816339745,
        fins_stall = 1, table_scale = .2, table_degree_values = 1,
        Cx0 = {.359, .359, .359, .359, .359, .533, .63, .639, .612, .586, .563,
            .541, .521, .501, .483, .466, .449, .433, .418, .404, .392},
        CxB = {.016, .016, .016, .016, .016, .022, .056, .0687, .0646, .0607, .0569,
            .0533, .0499, .0465, .0432, .0401, .0367, .0339, .031, .0282, .0255},
        K1 = {.0056, .0056, .0056, .0056, .0056, .0056, .0056, .0056, .0052, .0048,
            .0045, .0041, .0037, .0036, .0034, .0032, .0031, .003, .0029, .0027, .0026},
        K2 = {.0055, .0055, .0055, .0055, .0055, .0055, .0055, .0055, .0051, .0047,
            .0043, .0037, .0031, .0032, .0033, .0035, .0036, .0037, .0038, .0039, .004},
        Cya = curve(lift, LIFT), Cza = curve(lift, LIFT),
        Mya = {-.5, -.5}, Mza = {-.5, -.5}, Myw = {-2, -2}, Mzw = {-2, -2},
        A1trim = {10, 10}, A2trim = {10, 10},
    },
    proximity_fuze = {radius = 8, arm_delay = 1.6},
    warhead = warhead(), warhead_air = warhead(),
    seeker = {
        delay = 1.5, op_time = 200 * GUIDANCE,
        FOV = 2.0943951023932, max_w_LOS = .34906585039887,
        sens_near_dist = 100, sens_far_dist = 100000,
        keep_aim_time = 5, pos_memory_time = 150 * GUIDANCE, err_correct_time = 2,
        calc_aim_dist = 500000, blind_rad_val = 1,
        aim_y_offset = 2.5, aim_sigma = 4, ang_err_val = .00013962634015955, abs_err_val = 2,
        lock_manual_target_types_only = 0, filter_ignore_strings = {"EWR", "ewr"},
        guide_to_last_pos = true,
    },
    autopilot = {
        delay = 1, x_channel_delay = 1, op_time = 200 * GUIDANCE,
        Kconv = 3, Knv = .0024, Kd = .24, Ki = .01, Kout = 1, Kx = .04, Krx = 2,
        fins_limit = .34906585039887, fins_limit_x = .087266462599716,
        Areq_limit = 14 * LOAD, bang_bang = 0, max_signal_Fi = .20943951023932,
        rotate_fins_output = 0, alg = 0, PN_dist_data = {15000, 1, 5000, 1},
        null_roll = .78539816339745, min_a_conv_approach_vel = 100,
        K_heading_hor = .5, K_heading_ver = .3, K_loft = 3.3, loft_active_by_default = 1,
        loft_min_trig_ang = .087266462599716, loft_trig_ang = .34906585039887,
        loft_trig_max_alt = 3000, loft_trig_change_min_dist = 20000, loft_trig_change_max_dist = 35000,
        loft_min_dist = 5000, loft_min_add_pitch = .17453292519943,
        loft_add_pitch = .34906585039887, min_horiz_time = 3, max_pitch_cmd = .55850536063819,
    },
    conv = {seeker_add_pitch = -.34906585039887},
}
declare_weapon(weapon)
declare_loadout({
    category = CAT_MISSILES, CLSID = "{F23B-AGM88G}", Picture = "agm88.png",
    displayName = weapon.user_name, Count = 1, Weight = weapon.mass,
    attribute = weapon.wsTypeOfWeapon, wsTypeOfWeapon = weapon.wsTypeOfWeapon,
    Cx_pil = .0011 * SIZE * SIZE, Elements = {{ShapeName = SHAPE}},
})
return weapon

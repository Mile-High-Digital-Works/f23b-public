-- SPDX-License-Identifier: MIT
-- Project-defined AIM-9X Block II approximation, registered as an independent weapon.
-- The stock AIM-9X stays unchanged. The F-23B radar DLL presents this missile to the
-- Hornet as its AIM-9X store, F-23B player only.
-- Baseline: ED's stock AIM-9X definition (mass, diameter, drag and lift inputs, warhead).
-- Visual: project-original charcoal-grey mesh; stock overall length and diameter only.
-- Project decisions of October 9, each a stated rule and not a copy of another mod's values:
--   motor: 48.5 kg of propellant over 7 s at 262 s specific impulse;
--   airframe: stock 55 g x 1.3; 1.57 rad incidence while the motor burns; 0.5 s control delay;
--   seeker and fuze: stock 25 km reference x 1.2; stock 8 m fuze x 1.25; flare rejection in the helper;
--   envelope: 11 NM low and 16 NM high declared range; stock 60 s battery x 1.25;
--   launch-zone anchors: stock x 1.12, rounded to 100 m. Mean speed 850 m/s is from project flights.
-- Simulation values, not real motor or seeker data.
-- IR tracking remains native. Weapon-datalink/LOAL capability is not established.
local NM = 1852
local MOTOR_BURN_S, MOTOR_PROPELLANT_KG, MOTOR_SPECIFIC_IMPULSE_S = 7, 48.5, 262
local MOTOR_FLOW_KG_S = MOTOR_PROPELLANT_KG / MOTOR_BURN_S
local MOTOR_THRUST_N = MOTOR_FLOW_KG_S * 9.80665 * MOTOR_SPECIFIC_IMPULSE_S
local BATTERY_S = 75
local SHAPE = "F23B_AIM9X_BLOCKII"
local weapon = {
    category = CAT_AIR_TO_AIR, name = "F23B_AIM9X_BLOCKII",
    model = SHAPE,
    user_name = _("AIM-9X Block II (F-23B project-defined)"),
    display_name_short = "AIM-9XII",
    wsTypeOfWeapon = {4, 4, 7, WSTYPE_PLACEHOLDER},
    Escort = 0, Head_Type = 1, sigma = {1, 1, 1},
    M = 84.46, mass = 84.46, H_max = 18000, H_min = -1, Diam = 127,
    Cx_pil = 1.03, D_max = 11 * NM, D_min = 100, Head_Form = 0,
    Life_Time = BATTERY_S, Nr_max = 72, v_min = 95, v_mid = 850, Mach_max = 3.2,
    t_b = 0, t_acc = MOTOR_BURN_S, t_marsh = 0, Range_max = 16 * NM, H_min_t = 1,
    -- Preserve Hornet-native seeker acquisition/tracking geometry. The FC-host
    -- reference's 2-radian free-search cone is not a Hornet integration contract.
    -- gimbal travel and instantaneous search cone are separate parameters.
    Fi_start = 1.57, Fi_rak = math.pi, Fi_excort = 1.57,
    Fi_search = 0.09, OmViz_max = 1.1,
    warhead = predefined_warhead("AIM_9"),
    exhaust = {0.7, 0.7, 0.7, 0.08}, smoke_opacity_type = 1,
    X_back = -1.6, Y_back = 0, Z_back = 0, Reflection = 0.03, KillDistance = 10,
    SeekerGen = 4, SeekerSensivityDistance = 30000, ccm_k0 = 0.0005,
    SeekerCooled = true, x_wing_anim = -1, PN_gain = 6,
    shape_table_data = {{name = "F23B_AIM9X_BLOCKII", file = SHAPE, life = 1,
        fire = {0, 1}, username = "AIM-9XII", index = WSTYPE_PLACEHOLDER}},
    supersonic_A_coef_skew = 0.3, nozzle_exit_area = 0.0068,
    -- Native legacy solver: area/drag/lift, stage times, flow, force, guidance,
    -- then launch-zone estimates. Entries 16/23/30 are the boost contract.
    ModelData = {
        58, 0.35, 0.04, 0.08, 0.02, 0.05, 1.2, 1.0,
        1.2, 0.8, 1.0, 0.5, 1.57,
        -1, -1, MOTOR_BURN_S, 0, 0, 0, 1e9,
        0, 0, MOTOR_FLOW_KG_S, 0, 0, 0, 0,
        0, 0, MOTOR_THRUST_N, 0, 0, 0, 0,
        1e9, BATTERY_S, 0, 0.5, 1e9, 1e9, 0, 30, 0, 2.2, 1, 1,
        9, -13, -2.1, 17400, 6200, 30200, 11200, 11800, 3900,
        2500, 0.55, -0.01, 0.5,
    },
}
declare_weapon(weapon)
declare_loadout({
    category = CAT_AIR_TO_AIR, CLSID = "{F23B-AIM9X-BLOCKII}",
    Picture = "us_AIM-9X.png", displayName = weapon.user_name,
    attribute = weapon.wsTypeOfWeapon, wsTypeOfWeapon = weapon.wsTypeOfWeapon,
    Count = 1, Weight = weapon.M, Cx_pil = 0.0005,
    Elements = {{ShapeName = SHAPE, DrawArgs = {{1, 1}, {2, 1}}}},
})
return weapon

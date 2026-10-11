-- SPDX-License-Identifier: MIT
-- Project-original MALICE simulation; public dimensions, project-defined flight tune.
-- Independent weapon registered here; every stock AIM-120 definition stays unchanged.
-- The F-23B radar DLL presents it to the Hornet as its AMRAAM store, F-23B player only.
-- Option C: ED stock AIM-120C game values plus the stated project rule.
-- Fuel = 680.4 * (51.26 / 161.48) * 2.15, split five to one.
-- Burns = 6.5 * 2.4 and 6.5 * 2.7; impulse = 234 * 1.2 and 234 * 1.05.
-- Start = 0.4; fallback = 0.4 + 15.6 + 2.1 * 11.0.
-- These are DCS design choices. Range and terminal behavior require flights.
-- Network designation and midcourse support use native Hornet MSI services.
-- Model is a full-scale project approximation, not an official MALICE drawing.
-- See docs/design/F23B_MALICE_BLOCKII.md for limits and verification.
local MALICE_NAME = "F23B_AIM424_MALICE"
local MALICE_CLSID = F23B_MALICE_CLSID or "{F23B-AIM424-MALICE}"
local MALICE_MODEL = "F23B_MALICE"
local MALICE_SHAPE = "F23B_MALICE"
local MALICE_TYPE = { 4, 4, 7, WSTYPE_PLACEHOLDER }
local MALICE_MASS_KG = 680.4
local MALICE_ACTIVE_SEARCH_RANGE_M = 16.0 * 1852.0
local MALICE_SENSOR_FAR_RANGE_M = 40000.0
local malice_warhead = enhanced_a2a_warhead(60.0, 340.0)

local F23B_AIM424_MALICE = {
    category = CAT_AIR_TO_AIR,
    name = MALICE_NAME,
    user_name = _("AIM-424 MALICE (F-23B project-defined)"),
    displayName = _("AIM-424 MALICE - Active Radar AAM (project-defined)"),
    display_name_short = "AIM-424",
    model = MALICE_MODEL,
    scheme = "aa_missile_amraam2",
    class_name = "wAmmunitionSelfHoming",
    wsTypeOfWeapon = MALICE_TYPE,

    mass = MALICE_MASS_KG,
    M = MALICE_MASS_KG,
    Escort = 0,
    Head_Type = 2,
    sigma = { 1.0, 1.0, 1.0 },
    H_max = 30000.0,
    H_min = 1.0,
    Diam = 340.0,
    Cx_pil = 2.2,
    D_max = 480000.0,
    D_min = 800.0,
    Head_Form = 1,
    Life_Time = 600.0,
    Nr_max = 40,
    v_min = 140.0,
    v_mid = 2400.0,
    Mach_max = 8.0,
    t_b = 0.0,
    t_acc = 15.6,
    t_marsh = 17.55,
    Range_max = 500000.0,
    H_min_t = 1.0,
    Fi_start = math.rad(60.0),
    Fi_rak = math.pi,
    Fi_excort = math.rad(65.0),
    Fi_search = math.rad(70.0),
    OmViz_max = math.rad(45.0),
    exhaust = { 0.8, 0.8, 0.8, 0.04 },
    X_back = -2.055,
    Y_back = 0.0,
    Z_back = 0.0,
    Reflection = 0.035,
    KillDistance = 18.0,

    warhead = malice_warhead,
    warhead_air = malice_warhead,
    proximity_fuze = {
        radius = 18.0,
        arm_delay = 1.5,
    },

    SeekerGen = 4,
    ccm_k0 = 0.01,
    loft = 1,
    hoj = 1,
    loft_factor = 4.5,
    PN_gain = 4.0,
    supersonic_A_coef_skew = 0.09,
    nozzle_exit_area = 0.0125,
    active_radar_lock_dist = MALICE_ACTIVE_SEARCH_RANGE_M,

    shape_table_data = {
        {
            name = MALICE_NAME,
            file = MALICE_SHAPE,
            life = 1,
            fire = { 0, 1 },
            username = MALICE_NAME,
            index = WSTYPE_PLACEHOLDER,
        },
    },
    controller = {
        boost_start = 0.4,
        march_start = 39.1, -- Fallback without the DLL. Its checked native profile delays this controller timer.
    },

    boost = {
        impulse = 280.8,
        fuel_mass = 386.97363760,
        work_time = 15.6,
        nozzle_position = { { -2.055, 0.0, 0.0 } },
        nozzle_orientationXYZ = { { 0.0, 0.0, 0.0 } },
        nozzle_exit_area = 0.0132,
        tail_width = 0.42,
        smoke_color = { 0.82, 0.82, 0.82 },
        smoke_transparency = 0.20,
        custom_smoke_dissipation_factor = 0.20,
    },

    march = {
        impulse = 245.7,
        fuel_mass = 77.39472752,
        work_time = 17.55,
        nozzle_position = { { -2.055, 0.0, 0.0 } },
        nozzle_orientationXYZ = { { 0.0, 0.0, 0.0 } },
        nozzle_exit_area = 0.0125,
        tail_width = 0.26,
        smoke_color = { 0.82, 0.82, 0.82 },
        smoke_transparency = 0.025,
        custom_smoke_dissipation_factor = 0.20,
        smoke_opacity_type = 0,
    },

    fm = {
        mass = MALICE_MASS_KG,
        caliber = 0.34,
        wind_sigma = 0.0,
        wind_time = 0.0,
        tail_first = 0,
        fins_part_val = 0,
        rotated_fins_inp = 0,
        delta_max = math.rad(22.0),
        draw_fins_conv = { math.rad(90.0), 1, 1 },
        L = 0.34,
        S = math.pi * 0.17 * 0.17,
        Ix = 9.832,
        Iy = 962.691,
        Iz = 962.691,
        Mxd = 0.10 * 57.3,
        Mxw = -15.0,
        model_roll = math.rad(45.0),
        fins_stall = 1,
        table_scale = 0.2,
        table_degree_values = 1,

        Cx0 = {
            0.4000, 0.4020, 0.4040, 0.4060, 0.4080, 0.6800,
            0.7800, 0.7453, 0.7127, 0.6822, 0.6535, 0.6266,
            0.6014, 0.5778, 0.5556, 0.5348, 0.5153, 0.4970,
            0.4798, 0.4637, 0.4486, 0.4344, 0.4211, 0.4087,
            0.3970, 0.3860,
        },
        CxB = {
            0.0170, 0.0170, 0.0170, 0.0170, 0.0170, 0.0685,
            0.1200, 0.1093, 0.0997, 0.0910, 0.0833, 0.0764,
            0.0702, 0.0647, 0.0597, 0.0553, 0.0513, 0.0477,
            0.0445, 0.0416, 0.0391, 0.0368, 0.0347, 0.0329,
            0.0312, 0.0297,
        },
        K1 = {
            0.002500, 0.002500, 0.002500, 0.002500, 0.002500, 0.002500,
            0.002155, 0.001894, 0.001689, 0.001524, 0.001389, 0.001276,
            0.001179, 0.001096, 0.001025, 0.000962, 0.000906, 0.000856,
            0.000812, 0.000772, 0.000735, 0.000702, 0.000672, 0.000644,
            0.000619, 0.000595,
        },
        K2 = {
            -0.002400, -0.002400, -0.002400, -0.002400, -0.002400, -0.002400,
            -0.002069, -0.001818, -0.001622, -0.001463, -0.001333, -0.001224,
            -0.001132, -0.001053, -0.000984, -0.000923, -0.000870, -0.000822,
            -0.000779, -0.000741, -0.000706, -0.000674, -0.000645, -0.000619,
            -0.000594, -0.000571,
        },
        Cya = {
            0.3200, 0.3220, 0.3240, 0.3260, 0.3280, 0.3853,
            0.4427, 0.5000, 0.4865, 0.4739, 0.4621, 0.4512,
            0.4409, 0.4314, 0.4225, 0.4142, 0.4065, 0.3993,
            0.3926, 0.3863, 0.3805, 0.3751, 0.3700, 0.3653,
            0.3608, 0.3567,
        },
        Cza = {
            0.3200, 0.3220, 0.3240, 0.3260, 0.3280, 0.3853,
            0.4427, 0.5000, 0.4865, 0.4739, 0.4621, 0.4512,
            0.4409, 0.4314, 0.4225, 0.4142, 0.4065, 0.3993,
            0.3926, 0.3863, 0.3805, 0.3751, 0.3700, 0.3653,
            0.3608, 0.3567,
        },
        Mya = {
            -0.5508, -0.5559, -0.5785, -0.6431, -0.7552, -0.8546,
            -0.8546, -0.7373, -0.6130, -0.5383, -0.5050, -0.4885,
            -0.4763, -0.4649, -0.4539, -0.4432, -0.4326, -0.4224,
            -0.4124, -0.4026, -0.3930, -0.3837, -0.3746, -0.3657,
            -0.3571, -0.3486,
        },
        Mza = {
            -0.5508, -0.5559, -0.5785, -0.6431, -0.7552, -0.8546,
            -0.8546, -0.7373, -0.6130, -0.5383, -0.5050, -0.4885,
            -0.4763, -0.4649, -0.4539, -0.4432, -0.4326, -0.4224,
            -0.4124, -0.4026, -0.3930, -0.3837, -0.3746, -0.3657,
            -0.3571, -0.3486,
        },
        Myw = {
            -8.0008, -8.0151, -8.1482, -8.7584, -10.0200, -10.8000,
            -10.0200, -8.7584, -8.1238, -7.9433, -7.8817, -7.8338,
            -7.7869, -7.7403, -7.6940, -7.6480, -7.6022, -7.5568,
            -7.5115, -7.4666, -7.4219, -7.3775, -7.3334, -7.2895,
            -7.2459, -7.2026,
        },
        Mzw = {
            -8.0008, -8.0151, -8.1482, -8.7584, -10.0200, -10.8000,
            -10.0200, -8.7584, -8.1238, -7.9433, -7.8817, -7.8338,
            -7.7869, -7.7403, -7.6940, -7.6480, -7.6022, -7.5568,
            -7.5115, -7.4666, -7.4219, -7.3775, -7.3334, -7.2895,
            -7.2459, -7.2026,
        },
        A1trim = {
            28.000, 28.000, 28.000, 28.000, 28.000, 28.381,
            28.762, 29.143, 29.524, 29.905, 30.286, 30.667,
            31.048, 31.429, 31.810, 32.190, 32.571, 32.952,
            33.333, 33.714, 34.095, 34.476, 34.857, 35.238,
            35.619, 36.000,
        },
        A2trim = {
            28.000, 28.000, 28.000, 28.000, 28.000, 28.381,
            28.762, 29.143, 29.524, 29.905, 30.286, 30.667,
            31.048, 31.429, 31.810, 32.190, 32.571, 32.952,
            33.333, 33.714, 34.095, 34.476, 34.857, 35.238,
            35.619, 36.000,
        },
    },
    sensor = {
        delay = 1.5,
        op_time = 600.0,
        -- 15 degrees: the 140-degree cone admitted main-lobe clutter and blinded the beam seeker.
        FOV = math.rad(15.0),
        max_w_LOS = math.rad(140.0),
        sens_near_dist = 100.0,
        sens_far_dist = MALICE_SENSOR_FAR_RANGE_M,
        -- Enable the seeker's own distance trigger. Preserve native launch support.
        -- Engine predicate verified offline; Hornet scheme binding/flight remain OPEN.
        active_radar_lock_dist = MALICE_ACTIVE_SEARCH_RANGE_M,
        active_dist_trig_by_default = 1,
        ccm_k0 = 0.01,
        aim_sigma = 1.5,
        height_error_k = 8.0,
        height_error_max_vel = 25.0,
        height_error_max_h = 150.0,
        hoj = 1,
    },

    gimbal = {
        delay = 0.0,
        op_time = 600.0,
        pitch_max = math.rad(70.0),
        yaw_max = math.rad(70.0),
        max_tracking_rate = math.rad(40.0),
        tracking_gain = 55.0,
    },
    autopilot = {
        delay = 0.2,
        cmd_delay = 0.8,
        op_time = 600.0,
        Tf = 0.1,
        Knav = 4.0,
        Kd = 180.0,
        Ka = 16.0,
        T1 = 309.0,
        Tc = 0.06,
        Kx = 0.10,
        Krx = 2.0,
        gload_limit = 40.0,
        fins_limit = math.rad(18.0),
        fins_limit_x = math.rad(5.0),
        null_roll = math.rad(45.0),
        accel_coeffs = { 0.0, 11.5, -1.2, -0.25, 24.0, 0.00016926 },
        loft_active = 1,
        loft_factor = 4.5,
        loft_sin = math.sin(math.rad(30.0)),
        loft_off_range = 15000.0,
        dV0 = 430.0,
    },

    actuator = {
        Tf = 0.004,
        D = 260.0,
        T1 = 0.002,
        T2 = 0.005,
        max_omega = math.rad(420.0),
        max_delta = math.rad(22.0),
        fin_stall = 1,
        sim_count = 4,
    },
    ModelData = {
        58,
        0.32,
        0.022, 0.050, 0.010, -0.205, 0.055, 0.66,
        1.05, 0.56, 1.00, 0.50, 0.0,
        -1.0, -1.0, 15.6, 17.55, 0.0, 0.0, 1.0e9,
        0.0, 0.0, 386.97363760 / 15.6, 77.39472752 / 17.55, 0.0, 0.0, 0.0,
        0.0, 0.0, 386.97363760 / 15.6 * 9.80665 * 280.8, 77.39472752 / 17.55 * 9.80665 * 245.7, 0.0, 0.0, 0.0,
        1.0e9,
        600.0,
        0.0,
        1.0,
        45000.0,
        15000.0,
        math.rad(30.0),
        50.0,
        0.0,
        1.20,
        1.0,
        2.0,
        21.0, -25.0, -3.0,
        -- Launch-zone anchors [49..54], head-on/tail at 5, 10 and 1 km (owner decision, October 2, 2026).
        -- The owner's reference is a 350-mile (563 km) shot. The Hornet DLZ fits a quadratic through 1/5/10 km
        -- at altitude own + delta/2: these give about 522, 570 and 600 km head-on at 10, 12.5 and 15 km.
        372000.0, 186000.0,
        522000.0, 300000.0,
        200000.0, 90000.0,
        5000.0,
        0.42,
        -0.014,
        0.52,
    },
}

-- Project decision, September 29: reduce v3 drag by 20 percent for terminal energy.
-- Keep the 340 mm body, reference area, table shapes, and Mach 5 endpoint.
-- The multiplier is 0.84 * 0.8. Flight verification is recorded with v4.
for _, field in ipairs({"Cx0", "CxB"}) do
    for i, value in ipairs(F23B_AIM424_MALICE.fm[field]) do
        F23B_AIM424_MALICE.fm[field][i] = value * 0.672
    end
end
declare_weapon(F23B_AIM424_MALICE)

declare_loadout({
    category = CAT_AIR_TO_AIR,
    CLSID = MALICE_CLSID,
    Picture = "us_AIM-120C.png",
    displayName = _("AIM-424 MALICE (F-23B project-defined)"),
    attribute = F23B_AIM424_MALICE.wsTypeOfWeapon,
    wsTypeOfWeapon = F23B_AIM424_MALICE.wsTypeOfWeapon,
    Count = 1,
    Weight = MALICE_MASS_KG,
    Cx_pil = 0.00060,
    Elements = {
        {
            ShapeName = MALICE_SHAPE,
        },
    },
})

return F23B_AIM424_MALICE

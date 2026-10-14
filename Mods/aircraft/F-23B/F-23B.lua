-- SPDX-License-Identifier: MIT
-- F-23B is an exterior derivative of the installed full-fidelity F/A-18C.
-- The Hornet PFM and cockpit own every simulated system. This database record
-- supplies only the independent F-23 exterior, stores, effects, and identity.
local visual = dofile(current_mod_path .. "/Entry/VisualConfig.lua")

-- The old constant 00101 made every F-23B the same Link 16 participant.
-- The Hornet cockpit ranks a distinct participant separately from a raw radar hit.
local function link16_stn(unit)
    local unit_id = math.floor(tonumber(unit and unit.unitId) or 1)
    if unit_id < 1 then unit_id = 1 end
    local value = (unit_id + 128) % 32768
    local digits = {}
    for place = 1, 5 do
        digits[place] = string.char(48 + (value % 8))
        value = math.floor(value / 8)
    end
    return digits[5] .. digits[4] .. digits[3] .. digits[2] .. digits[1]
end

local function hornet_nozzle(position)
    local profile = visual.engine_effects
    return {
        pos = position,
        elevation = profile.elevation,
        diameter = profile.diameter,
        exhaust_length_ab = profile.exhaust_length_ab,
        exhaust_length_ab_K = profile.exhaust_length_ab_K,
        smokiness_level = profile.smokiness_level,
        afterburner_effect_texture = profile.afterburner_effect_texture,
        afterburner_circles_count = profile.afterburner_circles_count,
        afterburner_light_color = profile.afterburner_light_color
    }
end

local nozzle_origins = (visual.engine_effects and visual.engine_effects.origins) or visual.engines

local aircraft = {
    Name = "F-23B",
    DisplayName = _(visual.display_name),
    input_profile_entry = "F-23B",
    livery_entry = "F-23B",
    Picture = "F-23B.png",
    Rate = 1,
    Shape = visual.exterior_shape,
    shape_table_data = {
        {
            file = visual.exterior_shape,
            life = 20,
            vis = 3,
            desrt = "",
            fire = { 240, 2 },
            -- CJS retains the compiled Hornet ownership key even though its
            -- derivative Name and exterior shape are different.
            username = "FA-18C_hornet",
            index = WSTYPE_PLACEHOLDER,
            classname = "lLandPlane",
            positioning = "BYNORMAL"
        }
    },
    WorldID = WSTYPE_PLACEHOLDER,
    mapclasskey = "P0091000025",
    attribute = {
        wsType_Air, wsType_Airplane, wsType_Fighter, WSTYPE_PLACEHOLDER,
        "Multirole fighters", "Datalink", "Link16"
    },
    Categories = { "{78EFB7A2-FD52-4b57-A6A6-3BF0E1D6555F}", "Interceptor" },
    Countries = { "USA", "USAF Aggressors" },
    LandRWCategories = {
        [1] = { Name = "AircraftCarrier With Arresting Gear" }
    },
    TakeOffRWCategories = {
        [1] = { Name = "AircraftCarrier With Catapult" }
    },

    -- Hornet simulation metadata. These values intentionally match the
    -- installed FA-18C record; the F-23 model is a visual shell, not a new FM.
    M_empty = 11382,
    M_nominal = 16651,
    M_max = 23541,
    M_fuel_max = 4900,
    H_max = 18200,
    bank_angle_max = 78,
    average_fuel_consumption = 0.85,
    thrust_sum_max = 24000,
    thrust_sum_ab = 39160,
    flaps_maneuver = 0.5,
    stores_number = 10,
    IR_emission_coeff = 0.75,
    IR_emission_coeff_ab = 4.0,
    has_speedbrake = true,
    has_afteburner = true,
    has_differential_stabilizer = true,

    length = 17.07,
    height = 4.66,
    wing_area = 37.0,
    wing_span = 11.43,
    wing_type = FOLDED_WING,
    wing_tip_pos = { -2.466, 0.115, 5.73 },
    RCS = 0.00001,
    CAS_min = 62.0,
    V_opt = 180.0,
    V_take_off = 69.0,
    V_land = 65.0,
    V_max_sea_level = 361.1,
    V_max_h = 732,
    Vy_max = 254.0,
    Mach_max = 2.47,
    Ny_min = -3.0,
    Ny_max = 8.0, -- Project FBW cap; the transient check permits 0.3 g overshoot.
    Ny_max_e = 8.5, -- Project structural margin above the protected command.
    range = 1520.0,

    -- Measured F-23 landing-gear geometry on the standard Hornet animation
    -- arguments. The LODS uses F-23B-contact.edm for wheels and airframe lines.
    tand_gear_max = 3.73,
    nose_gear_pos = { 5.651507, -2.320485, -0.027196 },
    nose_gear_amortizer_direct_stroke = 0.0,
    nose_gear_amortizer_reversal_stroke = -0.12,
    nose_gear_amortizer_normal_weight_stroke = -0.06,
    nose_gear_wheel_diameter = 0.532208,
    nose_gear_door_close_after_retract = false,
    main_gear_pos = { -1.401347, -2.430469, 2.217924 },
    main_gear_amortizer_direct_stroke = 0.0,
    main_gear_amortizer_reversal_stroke = -0.12,
    main_gear_amortizer_normal_weight_stroke = -0.06,
    main_gear_wheel_diameter = 0.535680,
    main_gear_door_close_after_retract = false,

    engines_count = 2,
    -- Hornet renderer/state at the measured F-23 exhaust exits.
    engines_nozzles = {
        -- The Hornet uses one shared point light because its outlets are only
        -- 0.96 m apart. F-23 outlets are 2.62 m apart, so copying that
        -- one-sided light visibly unbalances them. Each F-23 outlet therefore
        -- receives the Hornet's same white burner light, while the stock
        -- renderer defaults and staged model geometry remain engine-driven.
        [1] = hornet_nozzle(nozzle_origins[1]),
        [2] = hornet_nozzle(nozzle_origins[2])
    },
    -- Keep the common logical bay cycle for the retained pylon method. The
    -- physical F-23 doors no longer consume argument 26; the passive Player
    -- overlay owns their dedicated presentation argument 1009 while Hornet
    -- SMS remains the sole owner of selection, consent and release.
    bomb_bay_movement = 2,
    crew_size = 1,
    crew_members = {
        [1] = {
            pos = visual.crew_position,
            canopy_pos = visual.canopy_position,
            g_suit = 5.0,
            ejection_seat_name = "pilot_f18_seat",
            pilot_name = "pilot_f18"
        }
    },
    fires_pos = {
        [1] = { -0.232, 0.262, 0.0 },
        [2] = { -1.938, 0.08, 1.344 },
        [3] = { -1.945, 0.056, -1.359 }
    },

    detection_range_max = 160,
    radar_can_see_ground = true,
    CanopyGeometry = { azimuth = { -155.0, 155.0 }, elevation = { -55.0, 90.0 } },
    HumanRadio = {
        frequency = 305.0,
        editable = true,
        minFrequency = 30.0,
        maxFrequency = 399.975,
        rangeFrequency = {
            { min = 30.0, max = 87.995, modulation = MODULATION_FM },
            { min = 118.0, max = 135.995, modulation = MODULATION_AM },
            { min = 136.0, max = 155.995, modulation = MODULATION_AM_AND_FM, modulationDef = MODULATION_FM },
            { min = 156.0, max = 173.995, modulation = MODULATION_FM },
            { min = 225.0, max = 399.975, modulation = MODULATION_AM_AND_FM, modulationDef = MODULATION_AM }
        },
        modulation = MODULATION_AM
    },
    TACAN_AA = true,
    passivCounterm = {
        CMDS_Edit = true,
        SingleChargeTotal = 120,
        chaff = { default = 60, increment = 10, chargeSz = 1 },
        flare = { default = 60, increment = 10, chargeSz = 1 },
        preferred_flare_kind = 2
    },
    chaff_flare_dispenser = {
        [1] = { dir = { -1.0, -0.4, -0.25 }, pos = { -6.2, -0.3, -0.8 } },
        [2] = { dir = { -1.0, -0.4, 0.25 }, pos = { -6.2, -0.3, 0.8 } }
    },
    Sensors = {
        RADAR = "AN/APG-73",
        RWR = "Abstract RWR"
    },
    Countermeasures = { ECM = { "AN/ALQ-165" } },
    EPLRS = true,
    connectDatalinks = { "Link16" },
    datalinks = { Link16 = "CoreMods\\aircraft\\FA-18C\\Datalinks\\Link16.lua" },
    AddPropAircraft = {
        {
            id = "HelmetMountedDevice",
            control = "comboList",
            label = _("Helmet Mounted Device"),
            values = {
                { id = 0, dispName = _("Not installed"), value = 0.5 },
                { id = 1, dispName = _("JHMCS"), value = 0.0 },
                { id = 2, dispName = _("NVG"), value = 1.0 }
            },
            defValue = 1,
            wCtrl = 150,
            playerOnly = true,
            arg = 509
        },
        { id = "VoiceCallsignLabel", control = "editbox", label = _("Voice Callsign Label"), defValue = "FT" },
        { id = "VoiceCallsignNumber", control = "editbox", label = _("Voice Callsign Number"), defValue = "11" },
        { id = "STN_L16", control = "editbox", label = _("STN"),
          getDefault = link16_stn, playerOnly = false }
    },

    -- Installed Hornet Mission Editor failure vocabulary. The owning Hornet
    -- cockpit implements these failures; this record only exposes its IDs.
    Failures = {
        { id = "Failure_Elec_UtilityBattery", label = _("Utility Battery FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_Elec_EmergencyBattery", label = _("Emergency Battery FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_Elec_LeftGenerator", label = _("Left Generator FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_Elec_RightGenerator", label = _("Right Generator FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_Elec_LeftTransformerRectifier", label = _("Left Transformer-Rectifier FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_Elec_RightTransformerRectifier", label = _("Right Transformer-Rectifier FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_Hyd_HYD1A_Leak", label = _("HYD 1A LEAKAGE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_Hyd_HYD1B_Leak", label = _("HYD 1B LEAKAGE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_Hyd_HYD2A_Leak", label = _("HYD 2A LEAKAGE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_Hyd_HYD2B_Leak", label = _("HYD 2B LEAKAGE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_Hyd_IsolatedHYD2BSystem_Leak", label = _("Isolated HYD 2B System LEAKAGE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_PP_EngL_Main_FFCS", label = _("Left Engine: Main Fuel Flow Control System FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_PP_EngR_Main_FFCS", label = _("Right Engine: Main Fuel Flow Control System FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_PP_EngL_AB_FFCS", label = _("Left Engine: AB Fuel Flow Control System FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_PP_EngR_AB_FFCS", label = _("Right Engine: AB Fuel Flow Control System FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_PP_EngL_Nozzle_CS", label = _("Left Engine: Nozzle Control System FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_PP_EngR_Nozzle_CS", label = _("Right Engine: Nozzle Control System FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_PP_EngL_OilLeak", label = _("Left Engine: Oil LEAKAGE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_PP_EngR_OilLeak", label = _("Right Engine: Oil LEAKAGE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_PP_LeftPTS", label = _("Left PTS FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_PP_RightPTS", label = _("Right PTS FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_PP_LeftAMAD_OilLeak", label = _("Left AMAD Oil LEAKAGE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_PP_RightAMAD_OilLeak", label = _("Right AMAD Oil LEAKAGE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_Fuel_LeftBoostPump", label = _("Left Boost Pump FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_Fuel_RightBoostPump", label = _("Right Boost Pump FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_Fuel_Tank1Transfer", label = _("Tank 1 Transfer FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_Fuel_Tank4Transfer", label = _("Tank 4 Transfer FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_Fuel_ExtTankTransferL", label = _("External Left Wing Tank Transfer FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_Fuel_ExtTankTransferR", label = _("External Right Wing Tank Transfer FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_Fuel_ExtTankTransferC", label = _("External Centerline Tank Transfer FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_Fuel_QuantityGaging", label = _("Fuel Quantity Gaging System FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_Gear_WOW", label = _("WOW System FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_Gear_NWS", label = _("NWS FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_ECS_Valve", label = _("ECS Valve FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_ECS_OBOGS", label = _("OBOGS FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_Ctrl_LEF", label = _("LEF FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_Ctrl_Aileron", label = _("Aileron FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_Ctrl_FCS_Ch1", label = _("FCS Channel 1 FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_Ctrl_FCS_Ch2", label = _("FCS Channel 2 FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_Ctrl_FCS_Ch3", label = _("FCS Channel 3 FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_Ctrl_FCS_Ch4", label = _("FCS Channel 4 FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_Comp_ADC", label = _("ADC FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_Comp_MC1", label = _("MC 1 FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_Comp_MC2", label = _("MC 2 FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_Sens_LeftPitotHeater", label = _("Left PITOT Heater FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
        { id = "Failure_Sens_RightPitotHeater", label = _("Right PITOT Heater FAILURE"), enable = false, hh = 0, mm = 0, mmint = 1, prob = 100 },
    },

    Guns = {
        gun_mount("M_61", { count = 578 }, {
            muzzle_pos_connector = "GUN_POINT",
            muzzle_pos = { 5.78, -1.03, 0.80 },
            effects = {
                -- Argument 350 remains the native muzzle-fire effect only.
                -- The mechanical gun panel is dedicated argument 1010 and is
                -- driven by the Player visual overlay without changing M61
                -- trigger, ammunition, rate or ballistics.
                { name = "FireEffect", arg = 350, duration = 0.02, attenuation = 2, light_pos = { 0.0, 0.0, 0.0 } },
                { name = "SmokeEffect", sparks_enabled = true }
            }
        })
    },

    -- F-23 bay geometry is retained, while logical station numbers and store
    -- classes follow the Hornet SMS contract. No CJS or protected ED file is
    -- copied; these CLSIDs resolve from the installed DCS database.
    -- October 13 bay (owner design): three roof trapezes carry the project HARM
    -- on stations 2, 3 and 7. Each bay door carries one MALICE (stations 4, 6)
    -- and one Block II rail (stations 1, 9). The doors open 150 degrees, so the
    -- door stores swing clear of the trapeze path. The centre trapeze also takes
    -- one MALICE for the three-MALICE loadout.
    -- The Hornet addresses a station frame by its connector name, so each
    -- station uses the connector with its own number.
    -- A store's origin sits at the connector plus attach_point_position. DCS
    -- consumes attach_point_position from the store record, not from the pylon
    -- options table. The pylon coordinates are the release positions with the
    -- doors open and the trapezes lowered.
    -- The AIM-9 stations are rails (0). The other stations are ejectors (1)
    -- with an aircraft-frame eject_dir straight down; the Hornet cheek-launch
    -- vectors are not valid in this bay. Type 2 is a hatch launcher and fired
    -- stores with the wrong mechanism.
    pylons_enumeration = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 },
    Pylons = {
        -- Door rail, port. Preserve the exposed zero-argument reference during native IR
        -- initialization. The player adapter stows it after initialization.
        pylon(1, 0, 3.44, -1.43, -0.80, {
            connector = "Pylon1", use_full_connector_position = true
        }, {
            { CLSID = "<CLEAN>" },
            { CLSID = "{F23B-AIM9X-BLOCKII}",
              attach_point_position = { -0.02, -0.043, 0.0 },
              Cx_gain_empty = 0.0, Cx_gain_item = 0 }
        }),
        -- Centre roof trapeze.
        pylon(2, 1, 3.15, -1.23, 0.0, {
            connector = "Pylon2", use_full_connector_position = true,
            eject_dir = { 0.0, -1.0, 0.0 }
        }, {
            { CLSID = "<CLEAN>" },
            { CLSID = "{F23B-AGM88G}",
              attach_point_position = { 0.15, -0.06, 0.0 },
              Cx_gain_empty = 0.0, Cx_gain_item = 0 },
            { CLSID = "{B06DD79A-F21E-4EB9-BD9D-AB3844618C93}",
              attach_point_position = { 0.15, -0.06, 0.0 },
              Cx_gain_empty = 0.0, Cx_gain_item = 0 },
            { CLSID = "{F23B-AIM424-MALICE}",
              attach_point_position = { 0.15, -0.095, 0.0 },
              forbidden = {
                  { station = 4, loadout = { "{40EF17B7-F508-45de-8566-6FFECC0C1AB8}" } },
                  { station = 6, loadout = { "{40EF17B7-F508-45de-8566-6FFECC0C1AB8}" } },
              },
              Cx_gain_empty = 0.0, Cx_gain_item = 0 },
            { CLSID = "{40EF17B7-F508-45de-8566-6FFECC0C1AB8}",
              attach_point_position = { 0.15, -0.085, 0.0 },
              Cx_gain_empty = 0.0, Cx_gain_item = 0 }
        }),
        -- Port roof trapeze.
        pylon(3, 1, 3.15, -1.16, -0.38, {
            connector = "Pylon3", use_full_connector_position = true,
            eject_dir = { 0.0, -1.0, 0.0 }
        }, {
            { CLSID = "<CLEAN>" },
            { CLSID = "{F23B-AGM88G}",
              attach_point_position = { 0.15, -0.06, 0.0 },
              Cx_gain_empty = 0.0, Cx_gain_item = 0 },
            { CLSID = "{B06DD79A-F21E-4EB9-BD9D-AB3844618C93}",
              attach_point_position = { 0.15, -0.06, 0.0 },
              Cx_gain_empty = 0.0, Cx_gain_item = 0 }
        }),
        -- Port door mount. The connector is at the MALICE centre and moves with the door.
        pylon(4, 1, 2.72, -1.60, -0.97, {
            connector = "Pylon4", use_full_connector_position = true,
            eject_dir = { 0.0, -1.0, 0.0 }
        }, {
            { CLSID = "<CLEAN>" },
            { CLSID = "{F23B-AIM424-MALICE}",
              attach_point_position = { 0.0, 0.0, 0.0 },
              forbidden = {
                  { station = 2, loadout = { "{40EF17B7-F508-45de-8566-6FFECC0C1AB8}" } },
                  { station = 6, loadout = { "{40EF17B7-F508-45de-8566-6FFECC0C1AB8}" } },
              },
              Cx_gain_empty = 0.0, Cx_gain_item = 0 },
            { CLSID = "{40EF17B7-F508-45de-8566-6FFECC0C1AB8}",
              attach_point_position = { 0.58, -0.02, 0.0 },
              Cx_gain_empty = 0.0, Cx_gain_item = 0 }
        }),
        pylon(5, 2, 0.0, 0.0, 0.0, {
            connector = "disable", DisplayName = _("UNUSED")
        }, {
            { CLSID = "<CLEAN>" }
        }),
        -- Starboard door mount.
        pylon(6, 1, 2.72, -1.60, 0.96, {
            connector = "Pylon6", use_full_connector_position = true,
            eject_dir = { 0.0, -1.0, 0.0 }
        }, {
            { CLSID = "<CLEAN>" },
            { CLSID = "{F23B-AIM424-MALICE}",
              attach_point_position = { 0.0, 0.0, 0.0 },
              forbidden = {
                  { station = 2, loadout = { "{40EF17B7-F508-45de-8566-6FFECC0C1AB8}" } },
                  { station = 4, loadout = { "{40EF17B7-F508-45de-8566-6FFECC0C1AB8}" } },
              },
              Cx_gain_empty = 0.0, Cx_gain_item = 0 },
            { CLSID = "{40EF17B7-F508-45de-8566-6FFECC0C1AB8}",
              attach_point_position = { 0.58, -0.02, 0.0 },
              Cx_gain_empty = 0.0, Cx_gain_item = 0 }
        }),
        -- Starboard roof trapeze.
        pylon(7, 1, 3.15, -1.16, 0.38, {
            connector = "Pylon7", use_full_connector_position = true,
            eject_dir = { 0.0, -1.0, 0.0 }
        }, {
            { CLSID = "<CLEAN>" },
            { CLSID = "{F23B-AGM88G}",
              attach_point_position = { 0.15, -0.06, 0.0 },
              Cx_gain_empty = 0.0, Cx_gain_item = 0 },
            { CLSID = "{B06DD79A-F21E-4EB9-BD9D-AB3844618C93}",
              attach_point_position = { 0.15, -0.06, 0.0 },
              Cx_gain_empty = 0.0, Cx_gain_item = 0 }
        }),
        pylon(8, 2, 0.0, 0.0, 0.0, {
            connector = "disable", DisplayName = _("UNUSED")
        }, {
            { CLSID = "<CLEAN>" }
        }),
        -- Door rail, starboard.
        pylon(9, 0, 3.44, -1.43, 0.78, {
            connector = "Pylon9", use_full_connector_position = true
        }, {
            { CLSID = "<CLEAN>" },
            { CLSID = "{F23B-AIM9X-BLOCKII}",
              attach_point_position = { -0.02, -0.047, 0.0 },
              Cx_gain_empty = 0.0, Cx_gain_item = 0 }
        }),
        pylon(10, 2, 0.0, 0.0, 0.0, {
            connector = "disable", DisplayName = _("UNUSED")
        }, {
            { CLSID = "<CLEAN>" }
        })
    },
    -- SEAD is for the HARM bay. A mission load resets a group task not in this list to CAP.
    Tasks = {
        aircraft_task(CAP), aircraft_task(Escort), aircraft_task(FighterSweep),
        aircraft_task(Intercept), aircraft_task(SEAD)
    },
    DefaultTask = aircraft_task(CAP),

    -- Exact Hornet AI/SFM fallback data. The player path is the installed
    -- Hornet PFM selected in entry.lua.
    SFM_Data = {
        aerodynamics = {
            Cy0 = 0, Mzalfa = 4.355, Mzalfadt = 0.8, kjx = 2.75, kjz = 0.00125,
            Czbe = -0.016, cx_gear = 0.0268, cx_flap = 0.23, cy_flap = 0.79, cx_brk = 0.08,
            table_data = {
                { 0, 0.0151, 0.07, 0.134, 0.0567, 0.5, 30, 2.4 },
                { 0.2, 0.0154, 0.07, 0.134, 0.056, 1.5, 30, 2.4 },
                { 0.4, 0.0156, 0.07, 0.134, 0.0549, 2.5, 30, 2.4 },
                { 0.6, 0.0164, 0.073, 0.134, 0.0474, 3.5, 30, 2.4 },
                { 0.7, 0.0172, 0.076, 0.134, 0.052, 3.5, 28.666666666667, 2.36 },
                { 0.8, 0.0201, 0.079, 0.144, 0.0607, 3.5, 27.333333333333, 2.32 },
                { 0.9, 0.0284, 0.083, 0.159, 0.0666, 3.5, 26, 2.28 },
                { 1, 0.0538, 0.085, 0.219, 0.0812, 3.5, 24.666666666667, 2.24 },
                { 1.05, 0.053618181818182, 0.085454545454545, 0.24854545454545, 0.080972727272727, 3.5, 24, 2.22 },
                { 1.1, 0.053436363636364, 0.085909090909091, 0.27809090909091, 0.080745454545455, 3.15, 18, 2.2 },
                { 1.11, 0.0534, 0.086, 0.284, 0.0807, 3.08, 17.9, 2.19 },
                { 1.2, 0.0493, 0.083, 0.35, 0.0784, 2.45, 17, 2.1 },
                { 1.3, 0.04536, 0.077, 0.4, 0.078, 1.75, 16, 2 },
                { 1.4, 0.0432, 0.062, 0.468, 0.0751, 1.625, 14.5, 1.9 },
                { 1.5, 0.0429, 0.054, 0.545, 0.0708, 1.5, 13, 1.8 },
                { 1.6, 0.0426, 0.046, 0.622, 0.0665, 1.2, 12.5, 1.6 },
                { 1.7, 0.04145, 0.0425, 0.743, 0.0618, 0.9, 12, 1.4 },
                { 1.8, 0.0403, 0.039, 0.864, 0.0571, 0.86, 11.4, 1.28 },
                { 2.2, 0.0377, 0.034, 1, 0.048, 0.7, 9, 0.8 },
                { 2.35, 0.0377, 0.033, 1, 0.0448, 0.7, 9, 0.8 },
                { 3.9, 0.0377, 0.033, 1, 0.0448, 0.7, 9, 0.8 }
            }
        },
        engine = {
            type = "TurboFan", Nmg = 64.1, Nominal_RPM = 16810.0,
            Nominal_Fan_RPM = 13270.0, Startup_Duration = 33.0,
            MinRUD = 0.1, MaxRUD = 1, MaksRUD = 0.85, ForsRUD = 0.91,
            hMaxEng = 19, dcx_eng = 0.0144, cemax = 1.24, cefor = 2.56,
            dpdh_m = 3500, dpdh_f = 6500,
            table_data = {
                { 0, 136000, 280000 }, { 0.2, 136000, 280000 },
                { 0.4, 146000, 280000 }, { 0.6, 160000, 274000 },
                { 0.7, 184000, 280000 }, { 0.8, 180000, 290000 },
                { 0.9, 172000, 286000 }, { 1, 120000, 286000 },
                { 1.11, 54000, 290000 }, { 1.2, 26000, 298000 },
                { 1.3, 14000, 290000 }, { 1.4, 9999.5525962323245, 294000 },
                { 1.6, 5903.7077377261176, 298000 }, { 1.8, 3457.3786131579791, 290000 },
                { 2.2, 2210.3358006520971, 226000 }, { 2.35, 1473.5572004347314, 188000 },
                { 3.9, 0, 60000 }
            }
        }
    },

    -- One ED Damage.lua cell for each shell and line in F-23B-contact.edm.
    -- Values follow the installed Hornet table. The F-23 model has no damage
    -- arguments. Debris models remain absent: the package has no ED assets.
    Damage = verbose_to_dmg_properties({
        ["NOSE_CENTER"] = {critical_damage = 3},
        ["NOSE_BOTTOM"] = {critical_damage = 3},
        ["COCKPIT"] = {critical_damage = 8},
        ["FUSELAGE_BOTTOM"] = {critical_damage = 4},
        ["FUSELAGE_TOP"] = {critical_damage = 4},
        ["TAIL_BOTTOM"] = {critical_damage = 3},
        ["ENGINE_L"] = {critical_damage = 2},
        ["ENGINE_R"] = {critical_damage = 2},
        ["WING_L_IN"] = {critical_damage = 5, deps_cells = {"WING_L_CENTER", "WING_L_OUT"}},
        ["WING_L_CENTER"] = {critical_damage = 4, deps_cells = {"WING_L_OUT"}},
        ["WING_L_OUT"] = {critical_damage = 3},
        ["WING_R_IN"] = {critical_damage = 5, deps_cells = {"WING_R_CENTER", "WING_R_OUT"}},
        ["WING_R_CENTER"] = {critical_damage = 4, deps_cells = {"WING_R_OUT"}},
        ["WING_R_OUT"] = {critical_damage = 3},
        ["FIN_L_TOP"] = {critical_damage = 4},
        ["FIN_L_CENTER"] = {critical_damage = 4},
        ["FIN_R_TOP"] = {critical_damage = 4},
        ["FIN_R_CENTER"] = {critical_damage = 4},
        ["WHEEL_F"] = {critical_damage = 3, args = {135}},
        ["WHEEL_L"] = {critical_damage = 3, args = {137}},
        ["WHEEL_R"] = {critical_damage = 3, args = {136}},
    }),

    mechanimations = {
        BombBay = {
            {
                Sequence = { { C = { { "Arg", 26, "to", 1, "in", 2, "sign", 1 } } } },
                Transition = { "Close", "Open" }
            },
            {
                Sequence = { { C = { { "Arg", 26, "to", 0, "in", 4.5, "sign", -1 } } } },
                Transition = { "Open", "Close" }
            }
        }
    },

    -- Hornet binary draw-argument contract. F-23-specific light locations
    -- are physical presentation only; switch state comes from Hornet systems.
    lights_data = {
        typename = "collection",
        lights = {
            [WOLALIGHT_STROBES] = { typename = "collection", lights = {
                {
                    typename = "argnatostrobelight", argument = 193, period = 1.2
                }
            } },
            [WOLALIGHT_LANDING_LIGHTS] = { typename = "collection", lights = {
                {
                    typename = "argumentlight", argument = 210
                }
            } },
            [WOLALIGHT_TAXI_LIGHTS] = { typename = "collection", lights = {
                {
                    typename = "argumentlight", argument = 210
                }
            } },
            [WOLALIGHT_NAVLIGHTS] = { typename = "collection", lights = {
                {
                    typename = "argumentlight", argument = 190
                },
                {
                    typename = "argumentlight", argument = 191
                },
                {
                    typename = "argumentlight", argument = 192
                }
            } },
            [WOLALIGHT_FORMATION_LIGHTS] = { typename = "collection", lights = {
                {
                    typename = "argumentlight", argument = 88
                }
            } }
        }
    },
    net_animation = {
        0, 1, 2, 3, 4, 5, 6,
        9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
        25, 26, 38, 84, 85, 88, 89, 90,
        101, 102, 103, 190, 191, 192, 193, 210, 274, 336, 350,
        -- Bay doors, gun door, IR reference and the three HARM trapezes.
        1009, 1010, 1013, 1018, 1019, 1020
    },
    ColdStartDefaultControls = {
        [15] = -1.0, [16] = -1.0, [9] = 1.0, [10] = 1.0,
        [17] = 0.5, [18] = -0.5, [11] = -0.2, [12] = -0.2
    },
    DTC = true
}

add_aircraft(aircraft)

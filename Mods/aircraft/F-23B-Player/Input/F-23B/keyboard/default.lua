-- SPDX-License-Identifier: MIT
local hornet_cockpit = "./Mods/aircraft/FA-18C/Cockpit/Scripts/"
local hornet_folder = "./Mods/aircraft/FA-18C/Input/FA-18C/keyboard/"
dofile(hornet_cockpit .. "devices.lua")
dofile(hornet_cockpit .. "command_defs.lua")
-- DCS gives every external profile its own environment. The second argument
-- is the supported way to set that profile's `folder`; assigning our caller's
-- global does not cross the sandbox boundary.
local profile = external_profile(
    hornet_folder .. "default.lua", hornet_folder)
-- Project files live under Saved Games, not the installation-relative ./Mods
-- tree. Resolve both from the input folder DCS supplied to this profile.
local bridge_commands = dofile(
    folder .. "../../../Cockpit/ExteriorVisuals/BridgeCommands.lua")
local bridge = dofile(folder .. "../bridge_profile.lua")
local path = deviceName and (require("lfs").writedir()
    .. "Config/Input/F-23B/keyboard/" .. deviceName .. ".diff.lua")
local saved = path and loadfile(path)
local diff = saved and saved()
return bridge(profile, devices, hotas_commands, bridge_commands, diff and diff.keyDiffs,
    diff and { path = path, diff = diff })

-- SPDX-License-Identifier: MIT
-- Binary-free F-23B core plugin. It owns the aircraft database record and
-- independent exterior assets. The sibling F-23B Player plugin owns the
-- flyable relationship to the installed F/A-18C, matching the split used by
-- both the installed Hornet and the inspected first-derivative reference.
local self_ID = "F-23B Core"
local visual_config = dofile(current_mod_path .. "/Entry/VisualConfig.lua")
local product_display_name = visual_config.display_name

declare_plugin(self_ID, {
    installed = true,
    dirName = current_mod_path,
    displayName = _(product_display_name),
    fileMenuName = _(product_display_name),
    shortName = "F-23B",
    version = visual_config.package_version,
    state = "installed",
    developerName = _("F-23B Community Project"),
    info = _("F-23B Black Widow II exterior/database core. The sibling F-23B Player plugin requires an installed and activated DCS: F/A-18C for flight, cockpit, avionics, systems, weapons logic and sound."),
    rules = {
        ["F/A-18C AI"] = { required = true }
    },
    encyclopedia_path = current_mod_path .. "/Encyclopedia",
    Skins = {
        { name = _("F-23B"), dir = "Theme" }
    },
    Missions = {
        { name = _("F-23B"), dir = "Missions" }
    },
    LogBook = {
        { name = _("F-23B"), type = "F-23B" }
    }
})

mount_vfs_model_path(current_mod_path .. "/Shapes")
mount_vfs_texture_path(current_mod_path .. "/Textures")
mount_vfs_liveries_path(current_mod_path .. "/Liveries")

dofile(current_mod_path .. "/Weapons/F23B_AIM424_MALICE.lua")
dofile(current_mod_path .. "/Weapons/F23B_AIM9X_BLOCKII.lua")
dofile(current_mod_path .. "/Weapons/F23B_AGM88G.lua")
dofile(current_mod_path .. "/F-23B.lua")

plugin_done()

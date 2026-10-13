-- SPDX-License-Identifier: MIT
-- Project-owned global input commands used only to sequence the F-23 doors
-- before forwarding the corresponding command to the installed Hornet HOTAS.
-- This dedicated 13000 block is disjoint from the active Hornet commands and
-- every command enumerated by the retired project cockpit implementation.
return {
    TRIGGER_SECOND_DETENT = 13001,
    SELECT_SIDEWINDER = 13002,
    SELECT_AMRAAM = 13003,
    SELECT_GUN = 13004,
    SELECT_SPARROW = 13005,
    WEAPON_RELEASE = 13006,
}

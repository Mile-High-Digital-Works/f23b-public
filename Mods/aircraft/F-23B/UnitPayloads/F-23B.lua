-- SPDX-License-Identifier: MIT
-- Hornet-recognized logical stations: door rails 1/9, door mounts 4/6, roof trapezes 2/3/7.
local AIM9X = "{F23B-AIM9X-BLOCKII}"
local AIM120C = "{40EF17B7-F508-45de-8566-6FFECC0C1AB8}"
local MALICE = "{F23B-AIM424-MALICE}"
local HARM = "{F23B-AGM88G}"
local tasks = { [1] = 11, [2] = 10, [3] = 18, [4] = 19 }
-- The SEAD preset also lists task 29 (SEAD), so the mission editor offers it for SEAD groups.
local sead_tasks = { [1] = 29, [2] = 11, [3] = 10, [4] = 18, [5] = 19 }

local unitPayloads = {
    ["name"] = "F-23B",
    ["payloads"] = {
        [1] = {
            ["name"] = "Air Superiority (2x AIM-9X Block II + 3x AIM-424 MALICE)",
            ["pylons"] = {
                [1] = { ["CLSID"] = AIM9X, ["num"] = 1 },
                [2] = { ["CLSID"] = MALICE, ["num"] = 2 },
                [3] = { ["CLSID"] = MALICE, ["num"] = 4 },
                [4] = { ["CLSID"] = MALICE, ["num"] = 6 },
                [5] = { ["CLSID"] = AIM9X, ["num"] = 9 }
            },
            ["tasks"] = tasks
        },
        [2] = {
            ["name"] = "Air Superiority (2x AIM-9X Block II + 3x AIM-120C)",
            ["pylons"] = {
                [1] = { ["CLSID"] = AIM9X, ["num"] = 1 },
                [2] = { ["CLSID"] = AIM120C, ["num"] = 2 },
                [3] = { ["CLSID"] = AIM120C, ["num"] = 4 },
                [4] = { ["CLSID"] = AIM120C, ["num"] = 6 },
                [5] = { ["CLSID"] = AIM9X, ["num"] = 9 }
            },
            ["tasks"] = tasks
        },
        [3] = {
            ["name"] = "Within Visual Range (2x AIM-9X Block II)",
            ["pylons"] = {
                [1] = { ["CLSID"] = AIM9X, ["num"] = 1 },
                [2] = { ["CLSID"] = AIM9X, ["num"] = 9 }
            },
            ["tasks"] = tasks
        },
        [4] = {
            ["name"] = "SEAD (2x AIM-9X Block II + 2x AIM-424 MALICE + 3x AGM-88G)",
            ["pylons"] = {
                [1] = { ["CLSID"] = AIM9X, ["num"] = 1 },
                [2] = { ["CLSID"] = HARM, ["num"] = 2 },
                [3] = { ["CLSID"] = HARM, ["num"] = 3 },
                [4] = { ["CLSID"] = MALICE, ["num"] = 4 },
                [5] = { ["CLSID"] = MALICE, ["num"] = 6 },
                [6] = { ["CLSID"] = HARM, ["num"] = 7 },
                [7] = { ["CLSID"] = AIM9X, ["num"] = 9 }
            },
            ["tasks"] = sead_tasks
        }
    },
    ["unitType"] = "F-23B"
}
return unitPayloads

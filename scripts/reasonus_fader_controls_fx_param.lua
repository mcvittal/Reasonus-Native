-- ReaSonus FaderPort 8/16: let every fader control one FX parameter on its own track.
-- Run once to enter the mode, run again to go back to track mode.
-- Useful for e.g. a gain plugin placed after the main fader: a second set of faders.

-- (part of) the FX name as shown in the FX chain. Empty = use the FX slot below
local fxName = "Gain"

-- With a name: 1 = first match, 2 = second match, ..., -1 = last match
-- Without a name: the 0-based FX slot
local fxInstance = -1

-- 0-based parameter index. See the parameter list of the plugin (or the FX param menu)
local fxParam = 0

-- Add the plugin to every track that does not have it yet (only used with a full FX name)
local addIfMissing = false

local EXT_SECTION = "ReaSonus"

local function addMissing()
  if not addIfMissing or fxName == "" then return end
  for t = 0, reaper.CountTracks(0) - 1 do
    local track = reaper.GetTrack(0, t)
    if reaper.TrackFX_AddByName(track, fxName, false, 0) == -1 then
      reaper.TrackFX_AddByName(track, fxName, false, -1)
    end
  end
end

local command = reaper.NamedCommandLookup("_REASONUS_FP8_SCRIPT_COMMAND")
if command == 0 then
  reaper.ShowMessageBox("ReaSonus is not loaded, or the script command action is missing.", "ReaSonus", 0)
  return
end

reaper.SetExtState(EXT_SECTION, "script_fx_name", fxName, false)
reaper.SetExtState(EXT_SECTION, "script_fx_instance", tostring(fxInstance), false)
reaper.SetExtState(EXT_SECTION, "script_fx_param", tostring(fxParam), false)

addMissing()

-- 600 = toggle the "faders control FX parameter" mode
reaper.Main_OnCommandEx(command, 600, 0)

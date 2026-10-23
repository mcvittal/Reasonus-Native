#ifndef CSURF_FP_8_FX_PARAM_MANAGER_C_
#define CSURF_FP_8_FX_PARAM_MANAGER_C_

#include "csurf_fp_8_channel_manager.hpp"

extern const int MOMENTARY_TIMEOUT;

/**
 * Every fader controls one parameter of one FX on its own track, e.g. the gain of a
 * gain plugin sitting after the main fader. What to control is handed over by a
 * script through ExtState (section ReaSonus):
 *
 *  - script_fx_name:     (part of) the FX name, as shown by TrackFX_GetFXName. When empty,
 *                        script_fx_instance is used as a 0-based FX slot.
 *  - script_fx_instance: With a name: the nth match (1 = first, -1 = last).
 *  - script_fx_param:    0-based parameter index.
 */
class CSurf_FP_8_FxParamManager : public CSurf_FP_8_ChannelManager {
    int mute_start[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int solo_start[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    std::string fx_name;
    int fx_instance = -1;
    int fx_param = 0;

    static int ReadInt(const char *key, const int default_value) {
        try {
            return std::stoi(DAW::GetExtState(key, std::to_string(default_value)));
        } catch (...) {
            return default_value;
        }
    }

    [[nodiscard]] int FindFx(MediaTrack *media_track) const {
        const int count = TrackFX_GetCount(media_track);

        if (fx_name.empty()) {
            return fx_instance >= 0 && fx_instance < count ? fx_instance : -1;
        }

        int found = -1;
        int matches = 0;
        for (int i = 0; i < count; i++) {
            char name[512] = {0};
            TrackFX_GetFXName(media_track, i, name, sizeof(name));
            if (std::string(name).find(fx_name) == std::string::npos) {
                continue;
            }
            matches++;
            found = i;
            if (fx_instance > 0 && matches == fx_instance) {
                return i;
            }
        }

        // -1 (or anything out of range) means: the last one we found
        return fx_instance > 0 ? -1 : found;
    }

public:
    CSurf_FP_8_FxParamManager(
        const std::vector<CSurf_FP_8_Track *> &tracks,
        CSurf_FP_8_Navigator *navigator,
        CSurf_Context *context,
        midi_Output *m_midiout) : CSurf_FP_8_ChannelManager(tracks, navigator, context, m_midiout) {
        fx_name = DAW::GetExtState("script_fx_name", "");
        fx_instance = ReadInt("script_fx_instance", -1);
        fx_param = ReadInt("script_fx_param", 0);

        CSurf_FP_8_FxParamManager::UpdateTracks(true);
    }

    ~CSurf_FP_8_FxParamManager() override {
    }

    void UpdateTracks(const bool force_update) override {
        const WDL_PtrList<MediaTrack> media_tracks = navigator->GetBankTracks();

        for (int i = 0; i < context->GetNbChannels(); i++) {
            MediaTrack *media_track;
            bool is_master_track = false;

            CSurf_FP_8_Track *track = tracks.at(i);
            if (context->GetMasterFaderMode() && i == context->GetNbChannels() - 1) {
                media_track = GetMasterTrack(nullptr);
                is_master_track = true;
            } else {
                media_track = media_tracks.Get(i);
            }

            if (!media_track || (CountTracks(nullptr) < i && !is_master_track)) {
                track->ClearTrack(true, force_update);
                continue;
            }

            const bool track_selected = DAW::IsTrackSelected(media_track);
            const bool is_armed = DAW::IsTrackArmed(media_track);
            const bool is_selected = (context->GetArm() && is_armed) || (!context->GetArm() && track_selected);

            SetTrackColors(media_track, is_selected, true);
            track->SetTrackColor(color, force_update);

            track->SetSelectButtonValue(!context->GetArm() && is_armed && !settings->GetDistractionFreeMode()
                                            ? BTN_VALUE_BLINK
                                            : BTN_VALUE_ON,
                                        force_update);
            track->SetMuteButtonValue(DAW::IsTrackMuted(media_track) ? BTN_VALUE_ON : BTN_VALUE_OFF, force_update);
            track->SetSoloButtonValue(DAW::IsTrackSoloed(media_track) ? BTN_VALUE_ON : BTN_VALUE_OFF, force_update);

            const int fx_index = FindFx(media_track);
            char fx_label[256] = {0};
            char param_label[256] = {0};
            char value_label[256] = {0};
            int fader_value = 0;

            if (fx_index > -1 && fx_param < TrackFX_GetNumParams(media_track, fx_index)) {
                const double value = TrackFX_GetParamNormalized(media_track, fx_index, fx_param);
                fader_value = static_cast<int>(value * 16383.0);
                TrackFX_GetFXName(media_track, fx_index, fx_label, sizeof(fx_label));
                TrackFX_GetParamName(media_track, fx_index, fx_param, param_label, sizeof(param_label));
                TrackFX_GetFormattedParamValue(media_track, fx_index, fx_param, value_label, sizeof(value_label));
                track->SetValueBarMode(VALUEBAR_MODE_FILL);
                track->SetValueBarValue(static_cast<int>(value * 127));
            } else {
                track->SetValueBarMode(VALUEBAR_MODE_OFF);
                track->SetValueBarValue(0);
                std::snprintf(fx_label, sizeof(fx_label), "no FX");
            }

            track->SetFaderValue(fader_value, force_update);

            track->SetDisplayMode(DISPLAY_MODE_2, force_update);
            track->SetDisplayLine(0, ALIGN_CENTER, DAW::GetTrackName(media_track).c_str(), NON_INVERT, force_update);
            track->SetDisplayLine(1, ALIGN_CENTER, fx_label, NON_INVERT, force_update);
            track->SetDisplayLine(2, ALIGN_CENTER, param_label, NON_INVERT, force_update);
            track->SetDisplayLine(3, ALIGN_CENTER, value_label, INVERT, force_update);
        }
    }

    void HandleSelectClick(const int index, const int value) override {
        if (value == 0) {
            return;
        }
        MediaTrack *media_track = navigator->GetTrackByIndex(index);

        if (context->GetArm()) {
            DAW::ToggleTrackArmed(media_track);
            return;
        }

        if (context->GetShiftChannelRight()) {
            DAW::SetSelectedTracksRange(media_track);
            return;
        }

        if (context->GetShiftChannelLeft()) {
            DAW::ToggleTrackSelected(media_track);
            return;
        }

        DAW::SetUniqueSelectedTrack(media_track);
    }

    void HandleMuteClick(const int index, const int value) override {
        const int now = GetTickCount();
        MediaTrack *media_track = navigator->GetTrackByIndex(index);

        if (value == 0 && settings->GetMuteSoloMomentary()) {
            if (now - mute_start[index] > MOMENTARY_TIMEOUT) {
                DAW::ToggleTrackMuted(media_track);
            }
            mute_start[index] = 0;
        } else if (value > 0) {
            mute_start[index] = now;
            DAW::ToggleTrackMuted(media_track);
        }
    }

    void HandleSoloClick(const int index, const int value) override {
        const int now = GetTickCount();
        MediaTrack *media_track = navigator->GetTrackByIndex(index);

        if (value == 0 && settings->GetMuteSoloMomentary()) {
            if (now - solo_start[index] > MOMENTARY_TIMEOUT) {
                DAW::ToggleTrackSoloed(media_track);
            }
            solo_start[index] = 0;
        } else if (value > 0) {
            solo_start[index] = now;
            DAW::ToggleTrackSoloed(media_track);
        }
    }

    void HandleFaderMove(const int index, const int msb, const int lsb) override {
        MediaTrack *media_track = navigator->GetTrackByIndex(index);
        if (!media_track) {
            return;
        }

        const int fx_index = FindFx(media_track);
        if (fx_index < 0 || fx_param >= TrackFX_GetNumParams(media_track, fx_index)) {
            return;
        }

        TrackFX_SetParamNormalized(media_track, fx_index, fx_param, int14ToNormalized(msb, lsb));
    }
};

#endif

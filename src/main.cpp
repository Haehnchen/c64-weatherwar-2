#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "assets/character_rom.hpp"
#include "audio/startup_sequence.hpp"
#include "audio/sid_timeline.hpp"
#include "game/first_turn.hpp"
#include "scenes/opening.hpp"
#include "game/scene_playback.hpp"
#include "scenes/setup.hpp"
#include "scenes/statistics_timing_data.hpp"
#include "video/character_frame.hpp"

#include <algorithm>
#include <array>
#include <deque>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace {
void require(bool result) {
    if (!result) throw std::runtime_error(SDL_GetError());
}

int run(int argc, char** argv) {
    const bool smoke = argc == 2 && std::string_view(argv[1]) == "--smoke-test";
    if (!smoke && argc != 1) {
        std::cerr << "Usage: weatherwar [--smoke-test]\n";
        return 2;
    }

    weatherwar::Opening opening(weatherwar::character_rom(), weatherwar::character_rom(true));
    const auto sequence = weatherwar::audio::builtin_sequence(weatherwar::audio::SequenceId::startup);
    const std::array tones{
        weatherwar::audio::builtin_sequence(weatherwar::audio::SequenceId::entry),
        weatherwar::audio::builtin_sequence(weatherwar::audio::SequenceId::first_name),
        weatherwar::audio::builtin_sequence(weatherwar::audio::SequenceId::second_name),
        weatherwar::audio::builtin_sequence(weatherwar::audio::SequenceId::board),
    };
    const auto round_tone = weatherwar::audio::builtin_sequence(weatherwar::audio::SequenceId::round);
    require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO));
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    require(SDL_CreateWindowAndRenderer("Weatherwar II - F11: Fullscreen", 960, 720,
                smoke ? SDL_WINDOW_HIDDEN : SDL_WINDOW_RESIZABLE, &window, &renderer));
    require(SDL_SetWindowMinimumSize(window, 320, 240));
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                             SDL_TEXTUREACCESS_STREAMING, 320, 200);
    if (!texture) throw std::runtime_error(SDL_GetError());
    require(SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST));
    // C64 display pixels are not square. Present the 320x200 texture as 4:3,
    // fitting the window without cropping or widescreen distortion.
    require(SDL_SetRenderLogicalPresentation(renderer, 320, 240, SDL_LOGICAL_PRESENTATION_LETTERBOX));
    auto toggle_fullscreen = [&] {
        require(SDL_SetWindowFullscreen(window, !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN)));
    };
    const SDL_AudioSpec spec{SDL_AUDIO_F32, 1, 48000};
    SDL_AudioStream* audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (!audio) throw std::runtime_error(SDL_GetError());
    weatherwar::audio::SidTimeline live_audio;
    auto live_opening = sequence;
    live_opening.end_cycle = sequence.events.back().cycle;
    live_audio.schedule(live_opening, 0);
    SDL_AudioSpec hardware_spec{};
    int hardware_frames = 0;
    require(SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(audio), &hardware_spec, &hardware_frames));
    if (hardware_spec.freq <= 0 || hardware_frames < 0) throw std::runtime_error("Invalid audio device format");
    const auto hardware_latency_cycles = static_cast<std::uint64_t>(hardware_frames) * 985248 / hardware_spec.freq;
    const auto lead_samples = static_cast<std::uint64_t>(hardware_frames) * 48000 / hardware_spec.freq + 2400;
    std::uint64_t estimated_presented_cycle = 0;
    auto pump_audio = [&] {
        const int queued = SDL_GetAudioStreamQueued(audio);
        if (queued < 0) throw std::runtime_error(SDL_GetError());
        const auto queued_samples = static_cast<std::uint64_t>(queued) / sizeof(float);
        const auto buffered_cycles = queued_samples * 985248 / 48000 + hardware_latency_cycles;
        // SDL exposes stream consumption, not an exact DAC timestamp. Subtract
        // one device buffer as a bounded latency estimate; keep it monotonic.
        if (live_audio.cycle() > buffered_cycles)
            estimated_presented_cycle = std::max(estimated_presented_cycle, live_audio.cycle() - buffered_cycles);
        // One hardware buffer plus ~50ms reservoir, never whole effects.
        if (queued_samples < lead_samples) {
            const auto cycles = ((lead_samples - queued_samples) * 985248 + 47999) / 48000;
            const auto samples = live_audio.render_until(live_audio.cycle() + cycles);
            if (!samples.empty())
                require(SDL_PutAudioStreamData(audio, samples.data(), static_cast<int>(samples.size() * sizeof(float))));
        }
    };
    pump_audio();
    require(SDL_ResumeAudioStreamDevice(audio));
    auto page_started = SDL_GetTicksNS();
    bool text_input_active = false;
    bool suppress_repeat_text = false;
    bool running = true;
    std::optional<weatherwar::Setup> setup;
    std::optional<weatherwar::FirstTurn> turn;
    std::optional<weatherwar::AttackSchedule> attack_schedule;
    std::deque<weatherwar::AttackSchedule> followup_schedules;
    std::size_t attack_event = 0;
    std::uint64_t tone_end_cycle = 0, attack_start_cycle = 0, statistics_end_cycle = 0;
    auto queue_tone = [&](const weatherwar::audio::StartupSequence& tone, std::uint64_t logical_end = 0) {
        auto scheduled = tone;
        scheduled.end_cycle = std::max(logical_end, tone.events.back().cycle);
        const auto start = live_audio.cycle();
        live_audio.schedule(scheduled, start);
        tone_end_cycle = start + tone.events.back().cycle;
        return start;
    };
    auto start_attack = [&] {
        if (!turn || !turn->playing_scene() || attack_schedule) return;
        auto plan = weatherwar::plan_scene_playback(*turn, round_tone);
        attack_schedule = std::move(plan.scenes.front());
        plan.scenes.pop_front();
        followup_schedules = std::move(plan.scenes);
        attack_event = 0;
        attack_start_cycle = queue_tone(plan.sound, plan.animation_end);
        tone_end_cycle += plan.round_pause_cycles;
    };
    auto play_tone = [&] {
        unsigned index = 0;
        switch (setup->phase()) {
        case weatherwar::SetupPhase::first_name_tone: index = 1; break;
        case weatherwar::SetupPhase::second_name_tone: index = 2; break;
        case weatherwar::SetupPhase::board_tone: index = 3; break;
        default: break;
        }
        queue_tone(tones[index]);
    };
    auto boundary = [&] {
        setup.emplace(opening.screen(), weatherwar::character_rom(),
                      opening.page() == weatherwar::OpeningPage::instructions_two);
        play_tone();
        for (const char key : opening.take_buffered_input()) {
            if (key == '\r') (void)setup->confirm();
            else if (key == '\b') setup->backspace();
            else setup->type(key);
        }
    };
    auto wants_text_input = [&] {
        return turn ? (turn->waiting_for_input() || turn->playing_scene() ||
                       turn->phase() == weatherwar::FirstTurnPhase::round_tone ||
                       turn->phase() == weatherwar::FirstTurnPhase::statistics)
            : setup ? (setup->waiting_for_name() || setup->playing_tone())
            : (opening.page() == weatherwar::OpeningPage::question ||
               opening.page() == weatherwar::OpeningPage::melody);
    };
    auto sync_text_input = [&] {
        const bool needed = wants_text_input();
        if (needed != text_input_active) {
            if (needed) require(SDL_StartTextInput(window));
            else require(SDL_StopTextInput(window));
            text_input_active = needed;
        }
    };
    while (running) {
        pump_audio();
        if (!turn || turn->phase() != weatherwar::FirstTurnPhase::statistics)
            statistics_end_cycle = 0;
        else if (statistics_end_cycle == 0)
            statistics_end_cycle = estimated_presented_cycle + weatherwar::statistics_timing_data::delay_cycles;
        if (turn && turn->phase() == weatherwar::FirstTurnPhase::statistics &&
            estimated_presented_cycle >= statistics_end_cycle) {
            turn->finish_statistics();
            page_started = SDL_GetTicksNS();
            if (turn->phase() == weatherwar::FirstTurnPhase::statistics)
                statistics_end_cycle = estimated_presented_cycle +
                    weatherwar::statistics_timing_data::delay_cycles;
            start_attack();
        }
        if (!setup && opening.page() == weatherwar::OpeningPage::melody && estimated_presented_cycle >= sequence.events.back().cycle) {
            opening.finish_melody();
            page_started = SDL_GetTicksNS();
            if (opening.boundary_requested()) boundary();
        }
        while (turn && turn->playing_scene() && attack_schedule) {
            const auto cycle = estimated_presented_cycle > attack_start_cycle ? estimated_presented_cycle - attack_start_cycle : 0;
            while (attack_event < attack_schedule->event_cycles.size() &&
                   attack_schedule->event_cycles[attack_event] <= cycle)
                turn->apply_scene_event(attack_event++);
            if (cycle >= attack_schedule->end_cycle) {
                const auto previous_end = attack_schedule->end_cycle;
                turn->finish_scene(); attack_schedule.reset();
                if (turn->playing_scene() && !followup_schedules.empty()) {
                    attack_schedule = std::move(followup_schedules.front());
                    followup_schedules.pop_front();
                    attack_start_cycle += previous_end;
                    attack_event = 0;
                    continue;
                }
                // The next round tone is already on the same uninterrupted PCM
                // timeline. Do not clear the stream or skip SID tail clocks here.
            }
            break;
        }
        if (turn && turn->phase() == weatherwar::FirstTurnPhase::round_tone &&
            estimated_presented_cycle >= tone_end_cycle) {
            turn->finish_tone(); page_started = SDL_GetTicksNS();
            start_attack();
        }
        if (!turn && setup && setup->playing_tone() &&
            estimated_presented_cycle >= tone_end_cycle) {
            setup->finish_tone();
            page_started = SDL_GetTicksNS();
            if (setup->playing_tone()) play_tone();
            if (setup->phase() == weatherwar::SetupPhase::board) {
                turn.emplace(*setup);
                turn->set_alternate_charset(weatherwar::character_rom(true));
                queue_tone(round_tone);
                for (const char key : setup->take_buffered_input()) {
                    if (key == '\r') turn->confirm();
                    else if (key == '\b') turn->backspace();
                    else turn->type(key);
                }
            }
        }
        // A buffered answer may begin a new scene after an already planned
        // chain ends (for example Y entered during the result effect).
        start_attack();
        sync_text_input();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            // Window controls are global, never forwarded to BASIC's input FIFO.
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F11) {
                if (!event.key.repeat) toggle_fullscreen();
                continue;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
                if (!event.key.repeat) {
                    if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) toggle_fullscreen();
                    else running = false;
                }
                continue;
            }
            // Default KERNAL repeat applies to SPACE/DEL/cursors, not letters,
            // digits or RETURN. SDL otherwise inherits desktop-wide repeat.
            if (event.type == SDL_EVENT_KEY_DOWN)
                suppress_repeat_text = event.key.repeat && event.key.key != SDLK_SPACE;
            if (event.type == SDL_EVENT_KEY_UP) suppress_repeat_text = false;
            if (event.type == SDL_EVENT_TEXT_INPUT && suppress_repeat_text) {
                suppress_repeat_text = false;
                continue;
            }
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (turn) {
                if (event.type == SDL_EVENT_TEXT_INPUT)
                    for (const char* p = event.text.text; *p; ++p) turn->type(*p);
                if (event.type == SDL_EVENT_KEY_DOWN && (!event.key.repeat || event.key.key == SDLK_BACKSPACE)) {
                    if (event.key.key == SDLK_ESCAPE) running = false;
                    else if (event.key.key == SDLK_BACKSPACE) turn->backspace();
                    else if (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER) {
                        const auto previous_phase = turn->phase();
                        turn->confirm();
                        if (turn->phase() == weatherwar::FirstTurnPhase::statistics &&
                            previous_phase != weatherwar::FirstTurnPhase::statistics) {
                            statistics_end_cycle = estimated_presented_cycle +
                                weatherwar::statistics_timing_data::delay_cycles;
                        }
                        start_attack();
                    }
                }
                continue;
            }
            if (setup) {
                if (event.type == SDL_EVENT_TEXT_INPUT)
                    for (const char* p = event.text.text; *p; ++p) setup->type(*p);
                if (event.type == SDL_EVENT_KEY_DOWN && (!event.key.repeat || event.key.key == SDLK_BACKSPACE)) {
                    if (event.key.key == SDLK_ESCAPE) running = false;
                    else if (event.key.key == SDLK_BACKSPACE) setup->backspace();
                    else if (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER)
                        if (setup->confirm()) play_tone();
                }
                continue;
            }
            if (event.type == SDL_EVENT_TEXT_INPUT &&
                (opening.page() == weatherwar::OpeningPage::question ||
                 opening.page() == weatherwar::OpeningPage::melody)) {
                for (const char* p = event.text.text; *p; ++p) opening.type(*p);
            }
            if (event.type != SDL_EVENT_KEY_DOWN ||
                (event.key.repeat && event.key.key != SDLK_BACKSPACE && event.key.key != SDLK_SPACE &&
                 event.key.key != SDLK_LEFT && event.key.key != SDLK_RIGHT &&
                 event.key.key != SDLK_UP && event.key.key != SDLK_DOWN)) continue;
            if (event.key.key == SDLK_ESCAPE) { running = false; continue; }
            if (opening.page() == weatherwar::OpeningPage::question ||
                opening.page() == weatherwar::OpeningPage::melody) {
                if (event.key.key == SDLK_BACKSPACE) opening.backspace();
                else if (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER) {
                    if (opening.confirm()) boundary();
                }
            } else if (opening.page() != weatherwar::OpeningPage::melody &&
                       event.key.key != SDLK_LSHIFT && event.key.key != SDLK_RSHIFT &&
                       event.key.key != SDLK_LCTRL && event.key.key != SDLK_RCTRL &&
                       event.key.key != SDLK_LALT && event.key.key != SDLK_RALT) {
                if (opening.advance(event.key.key == SDLK_R ? 'R' : ' ')) boundary();
            }
        }
        sync_text_input();
        const auto& current_screen = turn ? turn->screen() : setup ? setup->screen() : opening.screen();
        auto frame = current_screen.frame;
        // Visual cursor cadence follows the 20-PAL-IRQ editor divider. Matching
        // its initial phase and the complete KERNAL input editor remains separate.
        const auto cursor_frame = static_cast<std::uint64_t>(
            static_cast<double>(SDL_GetTicksNS() - page_started) * (985248.0 / (19656.0 * 1e9)));
        const bool editor_cursor = turn ? turn->waiting_for_input() : setup ? setup->waiting_for_name()
            : opening.page() == weatherwar::OpeningPage::question;
        if (editor_cursor && (cursor_frame / 20) % 2 == 0) {
            const auto position = current_screen.row * 40 + current_screen.column;
            frame.screen[position] ^= 128;
            frame.colors[position] = current_screen.color;
        }
        const auto pixels = weatherwar::render_character_frame(frame);
        require(SDL_UpdateTexture(texture, nullptr, pixels.data(), 320 * sizeof(std::uint32_t)));
        require(SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255));
        require(SDL_RenderClear(renderer));
        require(SDL_RenderTexture(renderer, texture, nullptr, nullptr));
        require(SDL_RenderPresent(renderer));
        if (smoke) break;
        SDL_Delay(10);
    }
    SDL_StopTextInput(window);
    SDL_DestroyAudioStream(audio);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    if (smoke) std::cout << "Native opening renderer and synthesized startup audio passed SDL smoke.\n";
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    try { return run(argc, argv); }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        SDL_Quit();
        return 1;
    }
}

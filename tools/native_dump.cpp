#include "assets/character_rom.hpp"
#include "audio/startup_sequence.hpp"
#include "audio/sid_chip.hpp"
#include "scenes/opening.hpp"
#include "scenes/setup.hpp"
#include "game/first_turn.hpp"
#include "weather/attack_timing.hpp"
#include "scenes/statistics_timing_data.hpp"
#include "game/scene_playback.hpp"
#include "video/frame_export.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {
int run(int argc, char** argv) {
    weatherwar::Opening opening(
        weatherwar::character_rom(),
        weatherwar::character_rom(true));
    if (argc == 3 && (std::string_view(argv[1]) == "--dump-computer" ||
                      std::string_view(argv[1]) == "--dump-nature" ||
                      std::string_view(argv[1]) == "--dump-match" ||
                      std::string_view(argv[1]) == "--dump-match-tie" ||
                      std::string_view(argv[1]) == "--dump-match-left" ||
                      std::string_view(argv[1]) == "--dump-human-sequence")) {
        const bool computer_sequence = std::string_view(argv[1]) == "--dump-computer";
        const bool stop_after_nature = std::string_view(argv[1]) == "--dump-nature";
        const bool tie_match = std::string_view(argv[1]) == "--dump-match-tie";
        const bool left_match = std::string_view(argv[1]) == "--dump-match-left";
        const bool dump_match = std::string_view(argv[1]) == "--dump-match" || tie_match || left_match;
        constexpr std::array<std::string_view, 18> tie_inputs{
            "L0", "L50", "L0", "T100", "H0", "H-50", "T0", "L-150", "L50",
            "L100", "H-50", "L-50", "R50", "T-100", "T-100", "H-150", "H-150", "H50"};
        unsigned human_inputs = 0;
        bool nature_seen = false;
        const std::filesystem::path directory(argv[2]);
        opening.show(weatherwar::OpeningPage::question); opening.type('N');
        weatherwar::Setup setup(opening.screen(), weatherwar::character_rom(), false);
        setup.finish_tone();
        for (char c : std::string(computer_sequence ? "COMPUTER" : "ALICE")) setup.type(c);
        setup.confirm(); setup.finish_tone();
        for (char c : std::string("BOB")) setup.type(c);
        setup.confirm(); setup.finish_tone(); setup.finish_tone();
        weatherwar::FirstTurn turn(setup);
        turn.set_alternate_charset(weatherwar::character_rom(true));
        auto dump = [&](const std::filesystem::path& path) {
            weatherwar::export_frame(turn.screen().frame, path);
            if (turn.waiting_for_input()) {
                auto cursor = turn.screen().frame;
                const auto cell = turn.screen().row * 40 + turn.screen().column;
                cursor.screen[cell] ^= 128;
                cursor.colors[cell] = turn.screen().color;
                weatherwar::export_frame(cursor, path.string() + "-cursor");
            }
            std::ofstream state(path.string() + "-state.json");
            state << std::setprecision(17) << "{\"F\":" << turn.left_structure() << ",\"G\":" << turn.right_structure()
                  << ",\"AA\":" << turn.cloud() << ",\"EE\":" << turn.wind() << ",\"A1\":" << turn.charge()
                  << ",\"M\":" << turn.turn_mode() << ",\"MM\":" << turn.round() << ",\"B\":" << turn.attacker()
                  << ",\"WW\":" << turn.weather_step() << ",\"NN\":" << turn.out_of_bounds_flag() << ",\"seed\":[";
            const auto seed = turn.random_seed();
            for (unsigned i = 0; i < seed.size(); ++i) state << (i ? "," : "") << unsigned(seed[i]);
            state << "],\"cursor\":[" << unsigned(turn.screen().column) << ',' << unsigned(turn.screen().row) << ']';
            const auto& decision = turn.computer_decision();
            state << ",\"computer_status\":\"" << (decision ? "attack" : "none") << '"';
            if (decision) state << ",\"PB\":" << decision->attempts;
            state << ",\"left_name\":\"" << turn.first_name() << "\",\"right_name\":\"" << turn.second_name()
                  << "\",\"left_score\":" << turn.left_score() << ",\"right_score\":" << turn.right_score();
            unsigned sprite_mask = 0;
            for (unsigned i = 0; i < turn.screen().frame.sprites.size(); ++i)
                if (turn.screen().frame.sprites[i].enabled) sprite_mask |= 1U << i;
            state << ",\"sprite_mask\":" << sprite_mask;
            state << ",\"phase\":\"" << (turn.phase() == weatherwar::FirstTurnPhase::nature_intro ? "nature_intro" :
                turn.phase() == weatherwar::FirstTurnPhase::result_effect ? "result_effect" :
                turn.phase() == weatherwar::FirstTurnPhase::result_prompt ? "result_prompt" :
                turn.phase() == weatherwar::FirstTurnPhase::replay_music ? "replay_music" :
                turn.phase() == weatherwar::FirstTurnPhase::replay_board ? "replay_board" :
                turn.phase() == weatherwar::FirstTurnPhase::statistics ? "statistics" :
                turn.phase() == weatherwar::FirstTurnPhase::ended ? "ended" :
                turn.phase() == weatherwar::FirstTurnPhase::attack ? "attack" :
                turn.phase() == weatherwar::FirstTurnPhase::weapon ? "weapon" : "round_tone") << '"';
            state << "}\n";
            if (!state) throw std::runtime_error("Cannot export computer state");
        };
        const auto export_scene = [&](const std::filesystem::path& path) {
            dump(path / "entry");
            const auto& source_events = turn.scene_events();
            const auto timing = weatherwar::schedule_events(source_events);
            std::ofstream events(path / "events.json");
            events << "[\n";
            weatherwar::audio::StartupSequence sound{timing.end_cycle, {}};
            for (std::size_t i = 0; i < source_events.size(); ++i) {
                const auto& event = source_events[i];
                events << (i ? ",\n" : "") << "{\"kind\":" << static_cast<unsigned>(event.kind)
                       << ",\"line\":" << event.basic_line << ",\"address\":" << event.address
                       << ",\"value\":" << unsigned(event.value) << ",\"cycle\":" << timing.event_cycles[i] << '}';
                if (event.kind == weatherwar::AttackEventKind::sid_write)
                    sound.events.push_back({timing.event_cycles[i], static_cast<std::uint8_t>(event.address - 0xd400), event.value});
                turn.apply_scene_event(i);
                if (event.text_screen) dump(path / ("text-" + std::to_string(i)));
            }
            events << "\n]\n";
            std::ofstream clock(path / "timing.json");
            clock << "{\"end_cycle\":" << timing.end_cycle << ",\"fallback_intervals\":" << timing.fallback_intervals
                  << ",\"interpolated_boundaries\":" << timing.interpolated_boundaries
                  << ",\"nature_sid_calibrated\":" << (timing.nature_sid_calibrated ? "true" : "false") << "}\n";
            weatherwar::audio::write_events(path / "scene.sid.txt", sound);
            if (!events || !clock) throw std::runtime_error("Cannot export match scene");
            turn.finish_scene();
            dump(path / "after");
        };
        for (unsigned move = 0; move < (computer_sequence ? 12U : 128U); ++move) {
            const auto path = directory / ("move-" + std::to_string(move));
            dump(path / "round-tone");
            turn.finish_tone();
            if (turn.waiting_for_weapon()) {
                dump(path / "human-prompt");
                if (computer_sequence && move >= 3) return 0;
                if (stop_after_nature && nature_seen) return 0;
                const std::string_view input = tie_match ? tie_inputs.at(human_inputs) :
                    left_match && !turn.attacker() ? "L150" : "H0";
                std::ofstream controls(path / "input.json");
                controls << "{\"weapon\":\"" << input.front() << "\",\"charge\":\"" << input.substr(1) << "\"}\n";
                if (!controls) throw std::runtime_error("Cannot export natural input itinerary");
                turn.type(input.front()); turn.confirm();
                for (char c : input.substr(1)) turn.type(c);
                turn.confirm(); ++human_inputs;
            }
            if (turn.phase() == weatherwar::FirstTurnPhase::nature_intro) {
                nature_seen = true;
                dump(path / "nature-entry");
                const auto& intro = turn.attack_result();
                weatherwar::export_frame(intro.screen.frame, path / "nature-after");
                const auto timing = weatherwar::schedule_attack(intro);
                const auto intro_path = path / "nature-intro";
                std::filesystem::create_directories(intro_path);
                std::ofstream events(intro_path / "events.json");
                weatherwar::audio::StartupSequence sound{timing.end_cycle, {}};
                events << "[\n";
                for (std::size_t i = 0; i < intro.events.size(); ++i) {
                    const auto& e = intro.events[i];
                    const auto kind = e.kind == weatherwar::AttackEventKind::sid_write ? "sid_write" :
                        e.kind == weatherwar::AttackEventKind::delay ? "delay" : "text_screen";
                    events << (i ? ",\n" : "") << "{\"kind\":\"" << kind << "\",\"line\":" << e.basic_line
                           << ",\"address\":" << e.address << ",\"value\":" << unsigned(e.value)
                           << ",\"loops\":" << e.delay_loop_count << ",\"cycle\":" << timing.event_cycles[i] << '}';
                    if (e.text_screen) weatherwar::export_frame(e.text_screen->frame, intro_path / ("text-" + std::to_string(i)));
                    if (e.kind == weatherwar::AttackEventKind::sid_write)
                        sound.events.push_back({timing.event_cycles[i], static_cast<std::uint8_t>(e.address - 0xd400), e.value});
                }
                events << "\n]\n";
                std::ofstream clock(intro_path / "timing.json");
                clock << "{\"end_cycle\":" << timing.end_cycle << ",\"fallback_intervals\":" << timing.fallback_intervals
                      << ",\"interpolated_boundaries\":" << timing.interpolated_boundaries
                      << ",\"nature_sid_calibrated\":" << (timing.nature_sid_calibrated ? "true" : "false") << "}\n";
                weatherwar::audio::write_events(intro_path / "intro.sid.txt", sound);
                if (!events || !clock) throw std::runtime_error("Cannot export nature intro events");
                turn.finish_attack();
            }
            if (turn.phase() != weatherwar::FirstTurnPhase::attack) {
                if (dump_match && turn.phase() == weatherwar::FirstTurnPhase::result_effect) {
                    export_scene(directory / "result");
                    dump(directory / "result-prompt");
                    const auto result_prompt = turn;
                    turn.type('N'); turn.confirm(); dump(directory / "end-N");
                    turn = result_prompt;
                    turn.type('S'); turn.confirm(); dump(directory / "result-statistics");
                    turn.finish_statistics(); dump(directory / "result-statistics-return");
                    turn.type('Y'); turn.confirm();
                    export_scene(directory / "replay-music");
                    export_scene(directory / "replay-board");
                    dump(directory / "replay-round-tone");
                    turn.finish_tone(); dump(directory / "replay-human-prompt");
                    turn.type('Q'); turn.confirm(); dump(directory / "replay-Q");
                    return 0;
                }
                dump(path / "boundary");
                return 0;
            }
            dump(path / "pre83");
            const auto& result = turn.attack_result();
            weatherwar::export_frame(result.screen.frame, path / "after");
            const auto schedule = weatherwar::schedule_attack(result);
            weatherwar::audio::StartupSequence sound{schedule.end_cycle, {}};
            std::ofstream timing(path / "timing.json");
            timing << "{\"end_cycle\":" << schedule.end_cycle << ",\"fallback_intervals\":" << schedule.fallback_intervals << "}\n";
            std::ofstream events(path / "events.json");
            events << "[\n";
            bool first = true;
            for (std::size_t i = 0; i < result.events.size(); ++i) {
                const auto& e = result.events[i];
                if (e.kind != weatherwar::AttackEventKind::sid_write && e.kind != weatherwar::AttackEventKind::vic_write) continue;
                events << (first ? "" : ",\n") << "{\"address\":" << e.address << ",\"value\":" << unsigned(e.value)
                       << ",\"line\":" << e.basic_line << ",\"cycle\":" << schedule.event_cycles[i] << "}";
                first = false;
                if (e.kind == weatherwar::AttackEventKind::sid_write)
                    sound.events.push_back({schedule.event_cycles[i], static_cast<std::uint8_t>(e.address - 0xd400), e.value});
            }
            events << "\n]\n";
            weatherwar::audio::write_events(path / "attack.sid.txt", sound);
            if (!events) throw std::runtime_error("Cannot export computer events");
            turn.finish_attack();
        }
        throw std::runtime_error("Natural sequence diagnostic exceeded bounded move count");
    }
    if (argc == 3 && std::string_view(argv[1]) == "--dump-attack") {
        const std::filesystem::path directory(argv[2]);
        opening.show(weatherwar::OpeningPage::question); opening.type('N');
        weatherwar::Setup setup(opening.screen(), weatherwar::character_rom(), false);
        setup.finish_tone();
        for (char c : std::string("ALICE")) setup.type(c);
        setup.confirm(); setup.finish_tone();
        for (char c : std::string("BOB")) setup.type(c);
        setup.confirm(); setup.finish_tone(); setup.finish_tone();
        for (const auto& [weapon, charge] : {std::pair{'H', "0"}, {'R', "100"}, {'H', "100"}, {'R', "0"}, {'H', "-150"}, {'R', "150"},
                {'L', "0"}, {'L', "-150"}, {'L', "150"}, {'T', "0"}, {'T', "150"}}) {
            const auto path = directory / (std::string(1, weapon) + charge);
            weatherwar::FirstTurn turn(setup); turn.finish_tone(); turn.type(weapon); turn.confirm();
            turn.set_alternate_charset(weatherwar::character_rom(true));
            for (const char* c = charge; *c; ++c) turn.type(*c);
            turn.confirm();
            const auto& result = turn.attack_result();
            const auto schedule = weatherwar::schedule_attack(result);
            weatherwar::export_frame(turn.screen().frame, path / "before");
            weatherwar::export_frame(result.screen.frame, path / "after");
            std::ofstream events(path / "events.json");
            events << "[\n";
            for (std::size_t i = 0; i < result.events.size(); ++i) {
                const auto& e = result.events[i];
                const char* kind = e.kind == weatherwar::AttackEventKind::screen_write ? "screen_write"
                    : e.kind == weatherwar::AttackEventKind::sid_write ? "sid_write"
                    : e.kind == weatherwar::AttackEventKind::delay ? "delay"
                    : e.kind == weatherwar::AttackEventKind::vic_write ? "vic_write" : "text_screen";
                events << (i ? ",\n" : "") << "{\"kind\":\"" << kind << "\",\"line\":" << e.basic_line
                       << ",\"address\":" << e.address << ",\"value\":" << unsigned(e.value)
                       << ",\"loops\":" << e.delay_loop_count << ",\"cycle\":" << schedule.event_cycles[i] << "}";
                if (e.text_screen) weatherwar::export_frame(e.text_screen->frame, path / ("text-" + std::to_string(i)));
            }
            events << "\n]\n";
            std::ofstream timing(path / "timing.json");
            timing << "{\"end_cycle\":" << schedule.end_cycle << ",\"fallback_intervals\":" << schedule.fallback_intervals << "}\n";
            if ((weapon == 'H' && std::string_view(charge) == "0") ||
                (weapon == 'R' && std::string_view(charge) == "100") || weapon == 'L' || weapon == 'T') {
                weatherwar::audio::SidChip attack_sid;
                for (const auto id : {weatherwar::audio::SequenceId::startup,
                                      weatherwar::audio::SequenceId::entry,
                                      weatherwar::audio::SequenceId::first_name,
                                      weatherwar::audio::SequenceId::second_name,
                                      weatherwar::audio::SequenceId::board,
                                      weatherwar::audio::SequenceId::round})
                    weatherwar::audio::render_sequence(attack_sid, weatherwar::audio::builtin_sequence(id));
                weatherwar::audio::StartupSequence sound{schedule.end_cycle + 98525, {}};
                for (std::size_t i = 0; i < result.events.size(); ++i)
                    if (result.events[i].kind == weatherwar::AttackEventKind::sid_write)
                        sound.events.push_back({schedule.event_cycles[i], static_cast<std::uint8_t>(result.events[i].address - 0xd400), result.events[i].value});
                weatherwar::audio::write_wav(path / "attack.wav", weatherwar::audio::render_sequence(attack_sid, sound));
                weatherwar::audio::write_events(path / "attack.sid.txt", sound);
            }
            const int ww = result.ww, nn = result.nn;
            std::ofstream end_state(path / "end-state.json");
            end_state << std::setprecision(17) << "{\"AA\":" << result.aa << ",\"WW\":" << ww << ",\"NN\":" << nn << "}\n";
            turn.finish_attack();
            weatherwar::export_frame(turn.screen().frame, path / "next-tone");
            turn.finish_tone();
            weatherwar::export_frame(turn.screen().frame, path / "next-weapon");
            auto cursor = turn.screen().frame;
            cursor.screen[turn.screen().row * 40 + turn.screen().column] ^= 128;
            cursor.colors[turn.screen().row * 40 + turn.screen().column] = turn.screen().color;
            weatherwar::export_frame(cursor, path / "next-weapon-cursor");
            std::ofstream state(path / "state.json");
            state << std::setprecision(17) << "{\"F\":" << turn.left_structure() << ",\"G\":" << turn.right_structure()
                  << ",\"AA\":" << turn.cloud() << ",\"EE\":" << turn.wind() << ",\"WW\":" << turn.weather_step() << ",\"NN\":" << turn.out_of_bounds_flag()
                  << ",\"M\":" << turn.turn_mode() << ",\"MM\":" << turn.round() << ",\"B\":" << turn.attacker() << ",\"seed\":[";
            const auto seed = turn.random_seed();
            for (unsigned i = 0; i < seed.size(); ++i) state << (i ? "," : "") << unsigned(seed[i]);
            state << "]}\n";
            if (!events || !state || !end_state) throw std::runtime_error("Cannot export attack artifacts");
        }
        return 0;
    }
    if (argc == 3 && (std::string_view(argv[1]) == "--dump-turn" || std::string_view(argv[1]) == "--dump-numeric")) {
        const std::filesystem::path directory(argv[2]);
        opening.show(weatherwar::OpeningPage::question); opening.type('N');
        weatherwar::Setup setup(opening.screen(), weatherwar::character_rom(), false);
        setup.finish_tone();
        for (char c : std::string("ALICE")) setup.type(c);
        setup.confirm(); setup.finish_tone();
        for (char c : std::string("BOB")) setup.type(c);
        setup.confirm(); setup.finish_tone(); setup.finish_tone();
        weatherwar::FirstTurn turn(setup);
        auto dump = [&](const weatherwar::FirstTurn& scene, const std::string& name) {
            weatherwar::export_frame(scene.screen().frame, directory / name);
            if (scene.waiting_for_input()) {
                auto cursor = scene.screen().frame;
                const auto offset = scene.screen().row * 40 + scene.screen().column;
                cursor.screen[offset] ^= 128; cursor.colors[offset] = scene.screen().color;
                weatherwar::export_frame(cursor, directory / (name + "-cursor"));
            }
        };
        dump(turn, "round-tone"); turn.finish_tone(); dump(turn, "weapon");
        std::ofstream state(directory / "state.json");
        state << std::setprecision(17) << "{\"F\":" << turn.left_structure() << ",\"G\":" << turn.right_structure()
              << ",\"AA\":" << turn.cloud() << ",\"EE\":" << turn.wind() << ",\"seed\":[";
        const auto seed = turn.random_seed();
        for (unsigned i = 0; i < seed.size(); ++i) state << (i ? "," : "") << static_cast<unsigned>(seed[i]);
        state << "]}\n";
        if (!state) throw std::runtime_error("Cannot export first-turn state");
        const auto selection_origin = turn;
        if (std::string_view(argv[1]) == "--dump-numeric") {
            for (const auto& [name, input] : {std::pair{"overflow", "1E39"}, {"underflow", "1E-99"}, {"invalid", "NO"}}) {
                auto selected = selection_origin;
                selected.type('H'); selected.confirm();
                dump(selected, std::string(name) + "/charge-prompt");
                for (char c : std::string(input)) selected.type(c);
                selected.confirm();
                dump(selected, std::string(name) + "/end");
                std::ofstream numeric_state(directory / name / "state.json");
                numeric_state << std::setprecision(17) << "{\"input\":\"" << input << "\",\"A1\":" << selected.charge() << ",\"seed\":[";
                const auto current_seed = selected.random_seed();
                for (unsigned i = 0; i < current_seed.size(); ++i) numeric_state << (i ? "," : "") << unsigned(current_seed[i]);
                numeric_state << "],\"cursor\":[" << unsigned(selected.screen().column) << ',' << unsigned(selected.screen().row)
                              << "],\"phase\":\"" << (selected.phase() == weatherwar::FirstTurnPhase::charge_preview ? "charge_preview" : "attack")
                              << "\",\"applied_scene_events\":0}\n";
                if (!numeric_state) throw std::runtime_error("Cannot export numeric state");
                if (selected.phase() == weatherwar::FirstTurnPhase::charge_preview) {
                    selected.type('0'); selected.confirm();
                    const auto timing = weatherwar::schedule_attack(selected.attack_result());
                    std::ofstream recovery_timing(directory / name / "recovery-timing.json");
                    recovery_timing << "{\"end_cycle\":" << timing.end_cycle << "}\n";
                    if (!recovery_timing) throw std::runtime_error("Cannot export recovery timing");
                    selected.finish_attack();
                    selected.finish_tone();
                    dump(selected, std::string(name) + "/recovered");
                }
            }
            return 0;
        }
        turn.type('X'); turn.confirm(); dump(turn, "invalid-weapon");
        for (char choice : std::string("HLRT")) {
            auto selected = selection_origin;
            selected.type(choice); selected.confirm(); dump(selected, std::string("charge-") + choice);
            if (choice == 'H') { selected.type('0'); dump(selected, "charge-H-zero"); }
        }
        auto statistics = selection_origin;
        statistics.type('S'); statistics.confirm();
        dump(statistics, "statistics-shown");
        weatherwar::export_frame(statistics.statistics_result().restored.frame, directory / "statistics-restored");
        std::ofstream statistics_timing(directory / "statistics-timing.json");
        statistics_timing << "{\"delay_loops\":" << statistics.statistics_delay_loops()
            << ",\"delay_cycles\":" << weatherwar::statistics_timing_data::delay_cycles
            << ",\"sid_stores\":0,\"timing_basis\":\"measured statistics line243-to244 interval\"}\n";
        if (!statistics_timing) throw std::runtime_error("Cannot export statistics timing");
        statistics.finish_statistics(); dump(statistics, "statistics-return");
        weatherwar::export_frame(statistics.statistics_cleanup_screen().frame, directory / "statistics-preprompt");
        auto buffered_statistics = selection_origin;
        buffered_statistics.type('S'); buffered_statistics.confirm();
        buffered_statistics.type('H'); buffered_statistics.confirm();
        buffered_statistics.finish_statistics();
        dump(buffered_statistics, "statistics-buffered-charge");
        auto quitting = selection_origin;
        quitting.type('Q'); quitting.confirm(); dump(quitting, "quit-final");
        return 0;
    }
    if (argc == 3 && std::string_view(argv[1]) == "--dump-setup") {
        const std::filesystem::path directory(argv[2]);
        opening.show(weatherwar::OpeningPage::question);
        opening.type('N');
        weatherwar::Setup setup(opening.screen(), weatherwar::character_rom(), false);
        setup.finish_tone();
        weatherwar::export_frame(setup.screen().frame, directory / "first-name");
        setup.type('A'); setup.confirm();
        weatherwar::export_frame(setup.screen().frame, directory / "short-name");
        for (char c : std::string("ALICE")) setup.type(c);
        setup.confirm(); setup.finish_tone();
        weatherwar::export_frame(setup.screen().frame, directory / "second-name");
        for (char c : std::string("BOB")) setup.type(c);
        setup.confirm(); setup.finish_tone(); setup.finish_tone();
        weatherwar::export_frame(setup.screen().frame, directory / "board");
        return 0;
    }
    if (argc == 3 && std::string_view(argv[1]) == "--dump-opening") {
        const std::filesystem::path directory(argv[2]);
        for (const auto& [page, name] : {
                 std::pair{weatherwar::OpeningPage::melody, "intro"},
                 std::pair{weatherwar::OpeningPage::question, "question"},
                 std::pair{weatherwar::OpeningPage::instructions_one, "instructions-one"},
                 std::pair{weatherwar::OpeningPage::instructions_two, "instructions-two"}}) {
            opening.show(page);
            weatherwar::export_frame(opening.screen().frame, directory / name);
            if (page == weatherwar::OpeningPage::question) {
                auto cursor = opening.screen().frame;
                const auto position = opening.screen().row * 40 + opening.screen().column;
                cursor.screen[position] ^= 128;
                cursor.colors[position] = opening.screen().color;
                weatherwar::export_frame(cursor, directory / "question-cursor");
            }
        }
        return 0;
    }
    const auto sequence = weatherwar::audio::builtin_sequence(weatherwar::audio::SequenceId::startup);
    weatherwar::audio::SidChip sid;
    const auto pcm = weatherwar::audio::render_sequence(sid, sequence);
    const std::array tones{
        weatherwar::audio::builtin_sequence(weatherwar::audio::SequenceId::entry),
        weatherwar::audio::builtin_sequence(weatherwar::audio::SequenceId::first_name),
        weatherwar::audio::builtin_sequence(weatherwar::audio::SequenceId::second_name),
        weatherwar::audio::builtin_sequence(weatherwar::audio::SequenceId::board),
    };
    const std::array<const char*, 4> tone_names{"entry", "first-name", "second-name", "board"};
    const auto round_tone = weatherwar::audio::builtin_sequence(weatherwar::audio::SequenceId::round);
    if (argc == 3 && std::string_view(argv[1]) == "--dump-turn-audio") {
        const std::filesystem::path directory(argv[2]);
        std::filesystem::create_directories(directory);
        for (const auto& tone : tones) weatherwar::audio::render_sequence(sid, tone);
        const auto samples = weatherwar::audio::render_sequence(sid, round_tone);
        weatherwar::audio::write_wav(directory / "round.wav", samples);
        weatherwar::audio::write_events(directory / "round.sid.txt", round_tone);
        return 0;
    }
    if (argc == 3 && std::string_view(argv[1]) == "--dump-setup-audio") {
        const std::filesystem::path directory(argv[2]);
        std::filesystem::create_directories(directory);
        for (unsigned i = 0; i < tones.size(); ++i) {
            const auto samples = weatherwar::audio::render_sequence(sid, tones[i]);
            weatherwar::audio::write_wav(directory / (std::string(tone_names[i]) + ".wav"), samples);
            weatherwar::audio::write_events(directory / (std::string(tone_names[i]) + ".sid.txt"), tones[i]);
        }
        return 0;
    }
    if (pcm.empty() || std::none_of(pcm.begin(), pcm.end(), [](float v) { return std::abs(v) > 0.001F; })) {
        throw std::runtime_error("Startup music produced no audible-level samples");
    }
    if (argc == 3 && std::string_view(argv[1]) == "--dump-audio") {
        weatherwar::audio::write_wav(argv[2], pcm);
        weatherwar::audio::write_events(std::string(argv[2]) + ".sid.txt", sequence);
        std::cout << pcm.size() << " mono samples at 48000 Hz; " << sequence.events.size() << " ordered SID writes\n";
        return 0;
    }
    if (argc == 3 && std::string_view(argv[1]) == "--dump-sequences") {
        const std::filesystem::path directory(argv[2]);
        std::filesystem::create_directories(directory);
        for (const auto& [id, name] : std::array{
                 std::pair{weatherwar::audio::SequenceId::startup, "startup"},
                 std::pair{weatherwar::audio::SequenceId::entry, "entry"},
                 std::pair{weatherwar::audio::SequenceId::first_name, "first-name"},
                 std::pair{weatherwar::audio::SequenceId::second_name, "second-name"},
                 std::pair{weatherwar::audio::SequenceId::board, "board"},
                 std::pair{weatherwar::audio::SequenceId::round, "round"}}) {
            const auto source = weatherwar::audio::builtin_sequence(id);
            std::ofstream output(directory / (std::string(name) + ".json"));
            output << "{\"end_cycle\":" << source.end_cycle << ",\"events\":[";
            for (std::size_t i = 0; i < source.events.size(); ++i) {
                const auto& event = source.events[i];
                output << (i ? "," : "") << "{\"cycle\":" << event.cycle
                       << ",\"register\":" << unsigned(event.reg)
                       << ",\"value\":" << unsigned(event.value) << '}';
            }
            output << "]}\n";
            if (!output) throw std::runtime_error("Cannot export built-in SID sequence");
        }
        return 0;
    }
    std::cerr << "Usage: native_dump --dump-opening DIR | --dump-setup DIR | --dump-turn DIR | --dump-attack DIR | --dump-computer DIR | --dump-nature DIR | --dump-match DIR | --dump-match-tie DIR | --dump-match-left DIR | --dump-numeric DIR | --dump-human-sequence DIR | --dump-audio WAV | --dump-setup-audio DIR | --dump-turn-audio DIR | --dump-sequences DIR\n";
    return 2;
}
} // namespace

int main(int argc, char** argv) {
    try { return run(argc, argv); }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

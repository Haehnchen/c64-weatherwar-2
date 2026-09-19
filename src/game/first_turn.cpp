#include "game/first_turn.hpp"
#include "scenes/setup_data.hpp"
#include "game/turn_data.hpp"
#include "basic/basic_input.hpp"
#include "basic/basic_value.hpp"
#include "scenes/final_score.hpp"
#include "game/match_result.hpp"
#include "weather/nature_intro.hpp"
#include "audio/replay_music.hpp"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <stdexcept>

namespace weatherwar {
using namespace turn_data;
FirstTurn::FirstTurn(const Setup& setup, BasicRandom::Seed seed)
    : screen_(setup.screen()), random_(seed), name_(setup.first_name()), first_(setup.first_name()), second_(setup.second_name()) {
    if (setup.phase() != SetupPhase::board) throw std::invalid_argument("Turn requires completed setup");
    normal_charset_ = alternate_charset_ = screen_.frame.charset;
    begin();
}
void FirstTurn::text(const std::string& value) { for (unsigned char c : value) screen_.put(c); }
void FirstTurn::spaces(unsigned count) { while (count--) screen_.put(29); }
void FirstTurn::number(int value) {
    text((value >= 0 ? " " : "") + std::to_string(value));
    screen_.put(29); // BASIC's numeric PRINT suffix advances without coloring a blank.
}

void FirstTurn::begin() {
    using setup_data::line_15_string_0;
    using setup_data::line_16_string_0;
    using setup_data::line_17_string_0;
    computer_decision_.reset();
    result_.reset();
    scene_events_.clear();
    scene_final_screen_.reset();
    // BASIC 113–117 counts screen characters, not sprites or invented hitpoints.
    screen_.print(line_15_string_0); screen_.print(line_113_string_0);
    screen_.print(line_16_string_0); screen_.newline();
    screen_.print(line_16_string_0); screen_.newline();
    auto count = [&](int h) {
        int result = 0;
        for (int y = 1; y <= 5; ++y) {
            for (int x = 1; x <= 9; ++x) if (screen_.frame.screen.at(++h - 1024) != 32) ++result;
            h -= 49;
        }
        return result;
    };
    left_ = count(1827); right_ = count(1850);
    if (b_ == 0 && (left_ < 1 || right_ < 1)) {
        result_ = prepare_match_result(screen_, left_, right_, first_, second_);
        phase_ = FirstTurnPhase::result_effect;
        return;
    }
    do {
        if (++m_ > 3) {
            m_ = 1; ++mm_; b_ = 0;
            screen_.print(line_38_string_0); spaces(15); screen_.print(line_38_string_1); screen_.newline();
        }
        // BASIC AND is eager: every visit to line 39 consumes a random value.
        const auto choice = random_.next();
        if (m_ != 3 || choice <= BasicRandom::round_to_basic(0.1)) break;
    } while (true);
    phase_ = FirstTurnPhase::round_tone;
    screen_.print(line_40_string_0);
    for (int v = 1; v <= 3; ++v) {
        screen_.print(line_40_string_1); screen_.print(line_16_string_0); screen_.print(line_40_string_2);
    }
    screen_.newline();
    cloud_ = random_.next_scaled(26);
    double a = BasicRandom::round_to_basic(1068 + cloud_);
    auto poke = [&](unsigned char value) { screen_.frame.screen.at(static_cast<unsigned>(a) - 1024) = value; };
    poke(104); for (int v = 1; v <= 4; ++v) { a += 1; poke(102); }
    for (int v = 1; v <= 5; ++v) { a += 1; poke(104); }
    a += 27; poke(104);
    for (int v = 1; v <= 11; ++v) { a += 1; poke(102); }
    poke(104); a += 33; poke(104);
    for (int v = 1; v <= 6; ++v) { a += 1; poke(102); }
    poke(104);
    screen_.print(line_15_string_0); screen_.print(line_45_string_0);
    screen_.print(line_17_string_0); screen_.print(line_45_string_1); screen_.newline();
    screen_.print(line_45_string_2);
    b_ = 1 - b_; nn_ = 0;
    name_ = b_ ? first_ : second_;
    if (m_ == 3) {
        screen_.print(line_46_string_0); spaces(18); screen_.print(line_46_string_1); screen_.newline();
    } else if (b_) {
        screen_.print(line_48_string_0); spaces(17); screen_.print(line_48_string_1); screen_.newline();
    } else {
        screen_.print(line_47_string_0); spaces(17); screen_.print(line_47_string_1); screen_.newline();
    }
    const int raw_wind = static_cast<int>(std::floor(random_.next_scaled(100)));
    wind_ = raw_wind > 50 ? static_cast<int>(std::floor(-raw_wind / 2.0)) : raw_wind;
    screen_.print(line_15_string_0); spaces(14);
    if (raw_wind > 50) {
        screen_.print(line_50_string_0); number(static_cast<int>(-wind_)); screen_.print(line_50_string_1);
    } else {
        screen_.print(line_51_string_0); number(static_cast<int>(wind_)); screen_.print(line_51_string_1);
    }
    screen_.newline();
    screen_.print(line_52_string_0); spaces(13); screen_.print(line_52_string_1);
    number(mm_); screen_.print(line_52_string_2); screen_.newline();
    // Platform plays the measured routine 222 call before line 53 continues.
}
void FirstTurn::finish_tone() {
    if (phase_ != FirstTurnPhase::round_tone) return;
    // BASIC 46 skips 180/182 during nature. Any previous AI selection was
    // consumed and CT reset by 75/76 before its attack, so nature is not AI.
    if (m_ != 3 && name_.starts_with("COMP")) {
        // BASIC 113-117 leaves P equal to G (the right structure count).
        ai_memory_.p = BasicRandom::round_to_basic(static_cast<double>(right_));
        computer_decision_ = choose_computer_attack(
            b_ ? ComputerSide::left : ComputerSide::right, mm_, cloud_, wind_,
            screen_, random_, ai_memory_);
        show_weapon(computer_decision_->weapon);
        start_attack(computer_decision_->charge, computer_decision_->wind);
        return;
    }
    if (m_ == 3) {
        attack_ = simulate_nature_intro(screen_);
        phase_ = FirstTurnPhase::nature_intro;
        return;
    }
    if (m_ == 1) {
        screen_.print(line_61_string_0); spaces(15); screen_.print(line_61_string_1);
        number(mm_); screen_.print(line_61_string_2); screen_.newline();
    }
    phase_ = FirstTurnPhase::weapon; weapon_prompt();
    drain_buffered_input();
}
void FirstTurn::weapon_prompt() {
    input_.clear();
    screen_.print(setup_data::line_15_string_0); screen_.print(line_63_string_0);
    screen_.print(setup_data::line_16_string_0); screen_.print(line_63_string_1);
    text(name_); screen_.print(line_63_string_2); screen_.newline();
    screen_.print(line_63_string_3); text("? ");
    prompt_ = screen_;
}
void FirstTurn::redraw() {
    screen_ = prompt_;
    const auto visible = static_cast<std::size_t>(39 - screen_.column);
    text(input_.substr(input_.size() > visible ? input_.size() - visible : 0));
}
bool FirstTurn::buffer_input_phase() const {
    return phase_ == FirstTurnPhase::round_tone || phase_ == FirstTurnPhase::statistics ||
           playing_scene();
}
void FirstTurn::buffer_key(char value) {
    if (buffered_input_.size() < 10) buffered_input_.push_back(value);
}
void FirstTurn::drain_buffered_input() {
    while (waiting_for_input() && !buffered_input_.empty()) {
        const char value = buffered_input_.front();
        buffered_input_.pop_front();
        if (value == '\r') confirm();
        else if (value == '\b') backspace();
        else type(value);
    }
}
std::string FirstTurn::take_buffered_input() {
    std::string result;
    result.reserve(buffered_input_.size());
    while (!buffered_input_.empty()) {
        result.push_back(buffered_input_.front());
        buffered_input_.pop_front();
    }
    return result;
}
void FirstTurn::type(char value) {
    const auto c = static_cast<unsigned char>(std::toupper(static_cast<unsigned char>(value)));
    if (!waiting_for_input()) {
        if (buffer_input_phase() && c >= 32 && c <= 90)
            buffer_key(static_cast<char>(c));
        return;
    }
    if (c < 32 || c > 90 || input_.size() >= 79) return;
    input_.push_back(static_cast<char>(c)); redraw();
}
void FirstTurn::backspace() {
    if (!waiting_for_input()) {
        if (buffer_input_phase()) buffer_key('\b');
        return;
    }
    if (input_.empty()) return;
    input_.pop_back(); redraw();
}
void FirstTurn::confirm() {
    if (phase_ == FirstTurnPhase::result_prompt) {
        const auto field = parse_basic_input_field(input_);
        const char choice = field.value.empty() ? '\0' : field.value.front();
        screen_.newline(); // INPUT consumes RETURN before BASIC 155-160.
        if (field.extra_ignored) text("?EXTRA IGNORED\r");
        input_.clear();
        if (choice == 'S') {
            statistics_ = render_statistics(
                screen_, first_, second_, left_score_, right_score_);
            screen_ = statistics_->shown;
            statistics_from_result_ = true;
            phase_ = FirstTurnPhase::statistics;
            return;
        }
        if (choice == 'N') {
            screen_ = render_final_score(
                screen_, first_, second_, left_score_, right_score_);
            phase_ = FirstTurnPhase::ended;
            return;
        }
        if (choice == 'Y') {
            std::swap(first_, second_);
            std::swap(left_score_, right_score_);
            scene_events_ = replay_music_events();
            scene_final_screen_ = screen_;
            phase_ = FirstTurnPhase::replay_music;
            return;
        }
        result_prompt();
        return;
    }
    if (phase_ == FirstTurnPhase::charge_preview && waiting_for_input()) { confirm_charge(); return; }
    if (!waiting_for_weapon()) {
        if (buffer_input_phase()) buffer_key('\r');
        return;
    }
    const auto field = parse_basic_input_field(input_);
    const char choice = field.value.empty() ? '\0' : field.value.front();
    screen_.newline(); // INPUT consumes RETURN before BASIC 64–66 branches.
    if (field.extra_ignored) text("?EXTRA IGNORED\r");
    if (choice == 'S') {
        statistics_ = render_statistics(screen_, first_, second_, left_score_, right_score_);
        screen_ = statistics_->shown;
        statistics_from_result_ = false;
        phase_ = FirstTurnPhase::statistics;
        input_.clear();
        return;
    }
    if (choice == 'Q') {
        screen_ = render_final_score(screen_, first_, second_, left_score_, right_score_);
        phase_ = FirstTurnPhase::ended;
        return;
    }
    WeatherWeapon selected = WeatherWeapon::hail;
    switch (choice) {
    case 'H': selected = WeatherWeapon::hail; break;
    case 'L': selected = WeatherWeapon::lightning; break;
    case 'R': selected = WeatherWeapon::rain; break;
    case 'T': selected = WeatherWeapon::tornado; break;
    default:
        screen_.print(setup_data::line_15_string_0); screen_.print(line_67_string_0);
        screen_.print(setup_data::line_16_string_0); screen_.print(setup_data::line_16_string_0);
        screen_.newline(); weapon_prompt(); return;
    }
    show_weapon(selected);
    screen_.print(setup_data::line_15_string_0); screen_.print(line_79_string_0);
    charge_prompt();
}

void FirstTurn::show_weapon(WeatherWeapon weapon) {
    switch (weapon) {
    case WeatherWeapon::hail: weapon_ = "HAIL"; break;
    case WeatherWeapon::lightning: weapon_ = "LIGHTNING"; break;
    case WeatherWeapon::rain: weapon_ = "RAIN"; break;
    case WeatherWeapon::tornado: weapon_ = "TORNADO"; break;
    }
    screen_.print(setup_data::line_15_string_0); screen_.print(line_73_string_0);
    screen_.print(setup_data::line_16_string_0); screen_.newline();
    screen_.print(setup_data::line_16_string_0); screen_.newline();
    screen_.print(line_73_string_1); text(weapon_); screen_.newline();
    if (weapon == WeatherWeapon::tornado) {
        screen_.print(line_74_string_0);
        for (int v = 1; v <= 40; ++v) screen_.print(line_74_string_1);
        screen_.print(setup_data::line_15_string_0); screen_.print(line_74_string_2); screen_.newline();
    }
}

void FirstTurn::charge_prompt() {
    screen_.print(line_79_string_1); text("? ");
    input_.clear(); prompt_ = screen_;
    phase_ = FirstTurnPhase::charge_preview;
}

void FirstTurn::retry_charge_prompt() {
    screen_ = prompt_;
    std::fill(screen_.frame.screen.begin() + 21 * 40,
              screen_.frame.screen.end(), 32);
    std::fill(screen_.frame.colors.begin() + 21 * 40,
              screen_.frame.colors.end(), screen_.color);
    screen_.row = 21; screen_.column = 0; screen_.reverse = false;
    text("?REDO FROM START");
    screen_.row = 22; screen_.column = 0;
    charge_prompt();
}

void FirstTurn::finish_statistics() {
    if (phase_ != FirstTurnPhase::statistics || !statistics_) return;
    screen_ = statistics_->restored;
    statistics_.reset();
    if (statistics_from_result_) {
        statistics_from_result_ = false;
        result_prompt();
        drain_buffered_input();
        return;
    }
    // BASIC 65 returns to 66/67, then repeats line 63 without a new round tone.
    screen_.print(setup_data::line_15_string_0); screen_.print(line_67_string_0);
    screen_.print(setup_data::line_16_string_0); screen_.print(setup_data::line_16_string_0);
    screen_.newline();
    statistics_cleanup_ = screen_; // After PRINT67, before its GOTO63.
    phase_ = FirstTurnPhase::weapon;
    weapon_prompt();
    // The IRQ keyboard buffer retains keys during FOR/NEXT, so input queued
    // during statistics can reach line 79.
    drain_buffered_input();
}

const StatisticsScreens& FirstTurn::statistics_result() const {
    if (!statistics_) throw std::logic_error("No statistics display prepared");
    return *statistics_;
}

const TextScreen& FirstTurn::statistics_cleanup_screen() const {
    if (!statistics_cleanup_) throw std::logic_error("Statistics has not returned yet");
    return *statistics_cleanup_;
}

void FirstTurn::confirm_charge() {
    screen_.newline();
    // BASIC 79: INPUT BB$ assigns the first comma-separated field (quotes
    // stripped, surrounding spaces trimmed); further data prints the ROM
    // `?EXTRA IGNORED` message before VAL/charge handling continues.
    const auto field = parse_basic_input_field(input_);
    // Empty input retains the editor line's prompt byte: BB$=$BF and VAL=0.
    charge_bb_ = field.value.empty() ? std::string(1, '\xBF') : field.value;
    charge_extra_ignored_ = field.extra_ignored;
    if (field.extra_ignored) {
        text("?EXTRA IGNORED\r");
    }
    double charge;
    try { charge = basic_val(field.value); }
    catch (const std::overflow_error&) {
        // Recover from the source's terminal OVERFLOW error at line 79.
        retry_charge_prompt();
        return;
    }
    start_attack(charge, wind_);
}

void FirstTurn::start_attack(double charge, double wind) {
    charge_ = std::clamp(charge, -150.0, 150.0); // BASIC 80-81.
    wind_ = wind;
    screen_.print(setup_data::line_15_string_0); screen_.print(line_82_string_0);
    screen_.print(setup_data::line_16_string_0); screen_.newline();
    screen_.print(line_82_string_1); number(static_cast<int>(std::floor(charge_))); screen_.newline();
    const auto weapon = weapon_ == "HAIL" ? WeatherWeapon::hail : weapon_ == "RAIN" ? WeatherWeapon::rain
        : weapon_ == "LIGHTNING" ? WeatherWeapon::lightning : WeatherWeapon::tornado;
    attack_ = simulate_weather_attack({screen_, weapon,
                                      cloud_, wind_, charge_, mm_, ww_, nn_});
    phase_ = FirstTurnPhase::attack;
}
const WeatherAttackResult& FirstTurn::attack_result() const {
    if (!attack_) throw std::logic_error("No attack has been prepared");
    return *attack_;
}
void FirstTurn::apply_attack_event(std::size_t index) {
    if (phase_ != FirstTurnPhase::attack && phase_ != FirstTurnPhase::nature_intro)
        throw std::logic_error("Not playing an attack or nature intro");
    apply_scene_event(index);
}

bool FirstTurn::playing_scene() const {
    return phase_ == FirstTurnPhase::attack ||
           phase_ == FirstTurnPhase::nature_intro ||
           phase_ == FirstTurnPhase::result_effect ||
           phase_ == FirstTurnPhase::replay_music ||
           phase_ == FirstTurnPhase::replay_board;
}

const std::vector<AttackEvent>& FirstTurn::scene_events() const {
    if (phase_ == FirstTurnPhase::attack || phase_ == FirstTurnPhase::nature_intro)
        return attack_result().events;
    if (phase_ == FirstTurnPhase::result_effect) {
        if (!result_) throw std::logic_error("No result effect prepared");
        return result_->events;
    }
    if (phase_ == FirstTurnPhase::replay_music ||
        phase_ == FirstTurnPhase::replay_board)
        return scene_events_;
    throw std::logic_error("No scene is being played");
}

void FirstTurn::apply_scene_event(std::size_t index) {
    if (!playing_scene()) throw std::logic_error("No scene is being played");
    const auto& event = scene_events().at(index);
    if (event.kind == AttackEventKind::screen_write)
        screen_.frame.screen.at(event.address - 1024) = event.value;
    else if (event.kind == AttackEventKind::text_screen) screen_ = *event.text_screen;
    else if (event.kind == AttackEventKind::vic_write) {
        if (event.address == 53281) screen_.frame.background = event.value & 15;
        else if (event.address == 53272)
            screen_.frame.charset = event.value & 2 ? alternate_charset_ : normal_charset_;
        else if (event.address == 53269)
            for (auto& sprite : screen_.frame.sprites)
                sprite.enabled = (event.value & 0xff) != 0;
        else throw std::logic_error("Unsupported scene VIC register");
    }
}
void FirstTurn::finish_attack() {
    if (phase_ != FirstTurnPhase::attack && phase_ != FirstTurnPhase::nature_intro)
        return;
    finish_scene();
}

void FirstTurn::finish_scene() {
    if (phase_ == FirstTurnPhase::nature_intro) {
        screen_ = attack_result().screen;
        attack_.reset();

        // BASIC 59 consumes its sole RND only after both intro repetitions and
        // the final line-58 delay/PRINT have completed.
        constexpr std::array choices{WeatherWeapon::hail, WeatherWeapon::lightning,
                                     WeatherWeapon::rain, WeatherWeapon::tornado};
        const auto choice = static_cast<unsigned>(std::floor(random_.next_scaled(4)));
        const auto selected = choices.at(choice);
        show_weapon(selected);

        // BASIC 77-78: only LIGHTNING gets the automatic nature charge, and
        // EE=1 falls through both strict comparisons to zero.
        double nature_charge = 0.0;
        if (selected == WeatherWeapon::lightning) {
            if (wind_ < 1.0) nature_charge = -1.0;
            else if (wind_ > 1.0) nature_charge = 1.0;
        }
        start_attack(nature_charge, wind_);
        return;
    }
    if (phase_ == FirstTurnPhase::attack) {
        screen_ = attack_result().screen;
        ww_ = attack_result().ww; nn_ = attack_result().nn;
        cloud_ = attack_result().aa;
        charge_ = attack_result().charge_a1;
        attack_.reset(); begin();
        return;
    }
    if (phase_ == FirstTurnPhase::result_effect) {
        if (!result_) throw std::logic_error("No result effect prepared");
        screen_ = result_->screen;
        left_score_ += result_->left_score_delta;
        right_score_ += result_->right_score_delta;
        result_.reset();
        result_prompt();
        return;
    }
    if (phase_ == FirstTurnPhase::replay_music) {
        if (scene_final_screen_) screen_ = *scene_final_screen_;
        prepare_replay_board();
        return;
    }
    if (phase_ == FirstTurnPhase::replay_board) {
        if (scene_final_screen_) screen_ = *scene_final_screen_;
        scene_events_.clear();
        scene_final_screen_.reset();
        begin();
    }
}

void FirstTurn::result_prompt() {
    append_result_prompt(screen_);
    input_.clear();
    prompt_ = screen_;
    phase_ = FirstTurnPhase::result_prompt;
    drain_buffered_input();
}

void FirstTurn::prepare_replay_board() {
    scene_events_.clear();
    scene_final_screen_.reset();

    auto vic = [&](int line, unsigned address, unsigned value) {
        scene_events_.push_back({AttackEventKind::vic_write, line,
            static_cast<std::uint16_t>(address), static_cast<std::uint8_t>(value), 0, {}});
    };
    auto sid = [&](int line, unsigned address, unsigned value) {
        scene_events_.push_back({AttackEventKind::sid_write, line,
            static_cast<std::uint16_t>(address), static_cast<std::uint8_t>(value), 0, {}});
    };

    // BASIC 162 disables sprites after music, then 26-36 redraws the board.
    vic(162, 53269, 0);
    TextScreen board = screen_;
    for (auto& sprite : board.frame.sprites) sprite.enabled = false;
    render_match_board(board, first_, second_);
    for (auto& sprite : board.frame.sprites) sprite.enabled = false;
    scene_events_.push_back(
        {AttackEventKind::text_screen, 36, 0, 0, 0, board});
    vic(36, 53269, 255);

    // GOSUB 222 from line 36, including its source FOR X=1 TO 700 pause.
    sid(222, 54276, 0); sid(222, 54277, 0); sid(222, 54272, 0);
    sid(223, 54277, 29); sid(223, 54276, 17);
    sid(223, 54273, 17); sid(223, 54272, 37);
    scene_events_.push_back({AttackEventKind::delay, 224, 0, 0, 700, {}});
    sid(224, 54276, 0); sid(224, 54277, 0); sid(224, 54272, 0);

    for (auto& sprite : board.frame.sprites) sprite.enabled = true;
    scene_final_screen_ = board;
    m_ = 3;
    mm_ = 0;
    left_ = 28;
    right_ = 28;
    phase_ = FirstTurnPhase::replay_board;
}
}

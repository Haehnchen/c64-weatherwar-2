#pragma once
#include "basic/basic_random.hpp"
#include "game/computer_player.hpp"
#include "game/match_result.hpp"
#include "scenes/setup.hpp"
#include "scenes/statistics.hpp"
#include "weather/weather_attack.hpp"
#include <optional>
#include <deque>
#include <string>

namespace weatherwar {
enum class FirstTurnPhase {
    round_tone,
    weapon,
    charge_preview,
    attack,
    statistics,
    result_effect,
    result_prompt,
    replay_music,
    replay_board,
    ended,
    nature_intro,
};

// Human/computer turn controller for all four weather attacks.
class FirstTurn {
public:
    explicit FirstTurn(const Setup& setup, BasicRandom::Seed seed = BasicRandom::default_seed());
    void set_alternate_charset(const std::array<std::uint8_t, 2048>& charset) { alternate_charset_ = charset; }
    void finish_tone();
    void type(char value);
    void backspace();
    void confirm();
    const WeatherAttackResult& attack_result() const;
    void apply_attack_event(std::size_t index);
    void finish_attack();
    bool playing_scene() const;
    const std::vector<AttackEvent>& scene_events() const;
    void apply_scene_event(std::size_t index);
    void finish_scene();
    void finish_statistics();
    const StatisticsScreens& statistics_result() const;
    const TextScreen& statistics_cleanup_screen() const;
    unsigned statistics_delay_loops() const { return statistics_ ? statistics_->delay_loops : 0; }
    const TextScreen& screen() const { return screen_; }
    FirstTurnPhase phase() const { return phase_; }
    bool waiting_for_weapon() const { return phase_ == FirstTurnPhase::weapon; }
    bool waiting_for_input() const { return waiting_for_weapon() || phase_ == FirstTurnPhase::charge_preview || phase_ == FirstTurnPhase::result_prompt; }
    int left_structure() const { return left_; }
    int right_structure() const { return right_; }
    double wind() const { return wind_; }
    double cloud() const { return cloud_; }
    double charge() const { return charge_; }
    const std::string& charge_bb() const { return charge_bb_; }
    bool charge_extra_ignored() const { return charge_extra_ignored_; }
    int round() const { return mm_; }
    int attacker() const { return b_; }
    int turn_mode() const { return m_; }
    int weather_step() const { return ww_; }
    int out_of_bounds_flag() const { return nn_; }
    unsigned round_pause_loops() const { return phase_ == FirstTurnPhase::round_tone && m_ == 2 && !name_.starts_with("COMP") ? 250 : 0; }
    const std::string& input() const { return input_; }
    // Keys received before the next human prompt use the same small C64
    // keyboard-buffer transport as Opening and Setup.
    [[nodiscard]] std::string take_buffered_input();
    const std::string& weapon() const { return weapon_; }
    const std::string& first_name() const { return first_; }
    const std::string& second_name() const { return second_; }
    int left_score() const { return left_score_; }
    int right_score() const { return right_score_; }
    BasicRandom::Seed random_seed() const { return random_.seed(); }
    const std::optional<ComputerDecision>& computer_decision() const { return computer_decision_; }
private:
    void text(const std::string& value);
    void spaces(unsigned count);
    void number(int value);
    void weapon_prompt();
    void redraw();
    void begin();
    void show_weapon(WeatherWeapon weapon);
    void charge_prompt();
    void retry_charge_prompt();
    void start_attack(double charge, double wind);
    void confirm_charge();
    void result_prompt();
    void prepare_replay_board();
    void buffer_key(char value);
    void drain_buffered_input();
    bool buffer_input_phase() const;
    TextScreen screen_, prompt_;
    std::array<std::uint8_t, 2048> normal_charset_{}, alternate_charset_{};
    BasicRandom random_;
    FirstTurnPhase phase_ = FirstTurnPhase::round_tone;
    std::string name_, first_, second_, input_, weapon_;
    std::optional<WeatherAttackResult> attack_;
    std::optional<ResultSequence> result_;
    std::vector<AttackEvent> scene_events_;
    std::optional<TextScreen> scene_final_screen_;
    std::optional<ComputerDecision> computer_decision_;
    std::optional<StatisticsScreens> statistics_;
    std::optional<TextScreen> statistics_cleanup_;
    std::deque<char> buffered_input_;
    bool statistics_from_result_ = false;
    AiMemory ai_memory_;
    int m_ = 3, mm_ = 0, b_ = 0, ww_ = 0, nn_ = 0;
    int left_ = 0, right_ = 0;
    int left_score_ = 0, right_score_ = 0;
    double wind_ = 0, cloud_ = 0;
    double charge_ = 0;
    std::string charge_bb_;
    bool charge_extra_ignored_ = false;
};
}

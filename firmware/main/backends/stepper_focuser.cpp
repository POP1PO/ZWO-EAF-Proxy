#include "backends/stepper_focuser.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"

namespace {
const char* kTag = "eaf.stepper";
const char* kNvsNamespace = "eaf_focuser";
const char* kNvsKey = "position";
constexpr int64_t kDebounceUs = 30000;  // 30 ms, long enough to settle contacts
// Keep the final phase energised this long before releasing the coils, so the
// rotor completes the last half-step instead of being cut loose mid-way.
constexpr int64_t kHoldUs = 20000;
}  // namespace

StepperFocuser::StepperFocuser(const StepperConfig& config)
    : config_(config),
      core_(config.start_position, config.max_travel),
      // Activation order is the silkscreen order IN1, IN2, IN3, IN4, matching
      // the columns of StepperCore's table. IN1/IN3 and IN2/IN4 are opposite
      // halves of the same winding, so they must never be adjacent phases. The
      // IN1, IN3, IN2, IN4 order seen in Arduino sketches only compensates for
      // AccelStepper's interleaved internal table and does not apply here.
      pins_{config.in1_gpio, config.in2_gpio, config.in3_gpio, config.in4_gpio} {}

bool StepperFocuser::begin() {
  uint64_t out_mask = 0;
  for (int pin : pins_) out_mask |= (1ULL << pin);
  gpio_config_t out = {};
  out.pin_bit_mask = out_mask;
  out.mode = GPIO_MODE_OUTPUT;
  out.pull_up_en = GPIO_PULLUP_DISABLE;
  out.pull_down_en = GPIO_PULLDOWN_DISABLE;
  out.intr_type = GPIO_INTR_DISABLE;
  if (gpio_config(&out) != ESP_OK) {
    ESP_LOGE(kTag, "GPIO config failed for the coil pins");
    return false;
  }
  deEnergise();

  if (config_.zero_gpio >= 0) {
    gpio_config_t in = {};
    in.pin_bit_mask = (1ULL << config_.zero_gpio);
    in.mode = GPIO_MODE_INPUT;
    in.pull_up_en = GPIO_PULLUP_ENABLE;  // active-low button to ground
    in.pull_down_en = GPIO_PULLDOWN_DISABLE;
    in.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&in);
    button_down_ = (gpio_get_level(static_cast<gpio_num_t>(config_.zero_gpio)) == 0);
  }

  // NVS holds this scope's last position across power cycles. Nothing else knows
  // it: there is no encoder and no home switch, so a lost count can only be
  // restored by hand. A fresh chip has no free pages until erased once.
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    err = nvs_flash_init();
  }
  if (err == ESP_OK) {
    nvs_ready_ = true;
    nvs_handle_t handle;
    if (nvs_open(kNvsNamespace, NVS_READONLY, &handle) == ESP_OK) {
      int32_t stored = 0;
      if (nvs_get_i32(handle, kNvsKey, &stored) == ESP_OK) {
        core_.setPosition(stored);  // clamps into travel on our behalf
        ESP_LOGI(kTag, "restored position %ld from NVS", static_cast<long>(stored));
      }
      nvs_close(handle);
    }
  } else {
    ESP_LOGW(kTag, "NVS unavailable (%s); position will not survive a power cycle",
             esp_err_to_name(err));
  }

  last_us_ = esp_timer_get_time();
  ESP_LOGI(kTag, "stepper ready: position %ld, travel 0..%ld, %ld half-steps/s",
           static_cast<long>(core_.position()), static_cast<long>(core_.maxTravel()),
           static_cast<long>(config_.rate_sps));
  return true;
}

bool StepperFocuser::moveTo(int32_t target) {
  core_.setTarget(target);  // clamped to [0, max] inside the core
  return true;
}

bool StepperFocuser::halt() {
  core_.halt();
  return true;
}

bool StepperFocuser::setPosition(int32_t position) {
  core_.setPosition(position);
  deEnergise();
  persist();
  return true;
}

void StepperFocuser::tick() {
  const int64_t now = esp_timer_get_time();
  const int64_t elapsed = now - last_us_;
  last_us_ = now;

  pollZeroButton();

  if (core_.isMoving()) {
    // At most one half-step per call. If the loop ran late the backlog is
    // dropped, so the move takes longer, rather than emitted back to back:
    // the rotor cannot follow a burst and the count would silently diverge.
    if (core_.due(elapsed, config_.rate_sps) > 0) {
      writePhase(core_.stepOnce());
      last_step_us_ = now;
    }
    was_moving_ = true;
    return;
  }

  // Idle. Once the last phase has been held long enough for the rotor to settle,
  // release the coils and save once: NVS is flash-backed with finite erase
  // cycles, so never per step.
  if (was_moving_) {
    if (now - last_step_us_ < kHoldUs) return;
    deEnergise();
    persist();
    was_moving_ = false;
  }
}

void StepperFocuser::writePhase(const stepper::Coils& coils) {
  for (int i = 0; i < 4; ++i) {
    // invert remaps the coil columns onto the pins in reverse, which reverses
    // the direction the motor turns for a given count change. Use it when the
    // scope moves the wrong way and you would rather not re-wire the connector.
    const int idx = config_.invert ? (3 - i) : i;
    gpio_set_level(static_cast<gpio_num_t>(pins_[idx]), coils.level[i]);
  }
}

void StepperFocuser::deEnergise() {
  for (int pin : pins_) gpio_set_level(static_cast<gpio_num_t>(pin), 0);
}

void StepperFocuser::persist() {
  if (!nvs_ready_) return;
  nvs_handle_t handle;
  if (nvs_open(kNvsNamespace, NVS_READWRITE, &handle) != ESP_OK) return;
  nvs_set_i32(handle, kNvsKey, core_.position());
  nvs_commit(handle);
  nvs_close(handle);
}

void StepperFocuser::pollZeroButton() {
  if (config_.zero_gpio < 0) return;
  const bool down = (gpio_get_level(static_cast<gpio_num_t>(config_.zero_gpio)) == 0);
  const int64_t now = esp_timer_get_time();
  if (down != button_down_) {
    // Edge: start the debounce window and act only on a press that settles.
    if (now - button_changed_us_ < kDebounceUs) return;
    button_changed_us_ = now;
    button_down_ = down;
    if (down && !core_.isMoving()) {
      // Zeroing mid-move would redefine the coordinates the host is targeting,
      // so it is refused while moving; press again once stopped.
      core_.setPosition(0);
      deEnergise();
      persist();
      ESP_LOGI(kTag, "zero button: position set to 0");
    }
  }
}

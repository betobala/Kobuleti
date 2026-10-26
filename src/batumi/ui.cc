// Copyright 2015 Matthias Puech.
//
// Author: Matthias Puech (matthias.puech@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
// User interface.

#include "ui.h"

#include <cstdlib>

namespace batumi {

const int32_t kLongPressDuration = 500;
const int32_t kVeryLongPressDuration = 2000;
const int32_t kPotMoveThreshold = 1 << (16 - 10);  // 10 bits
const uint16_t kCatchupThreshold = 1 << 10;

void Ui::Init(const PanelState& panel) {
  mode_ = UI_MODE_SPLASH;
  animation_counter_ = 0;
  // synchronize pots at startup
  for (uint8_t i = 0; i < 4; i++) {
    pot_value_[i] = pot_filtered_value_[i] = pot_coarse_value_[i] = panel.pot[i];
    catchup_state_[i] = false;
  }
}

void Ui::Poll(const PanelState& panel) {
  milliseconds_++;
  sync_ = panel.sync;
  wav1_ = panel.wav1;
  wav2_ = panel.wav2;

  // Select button: short press, long press (zoom), very long press (nothing)
  if (panel.select && !select_was_pressed_) {
    press_time_ = milliseconds_;
    detect_very_long_press_ = false;
  }
  if (panel.select && press_time_) {
    int32_t pressed_time = milliseconds_ - press_time_;
    if (!detect_very_long_press_) {
      if (pressed_time > kLongPressDuration) {
        OnSelectReleased(pressed_time);
        detect_very_long_press_ = true;
      }
    } else if (pressed_time > kVeryLongPressDuration) {
      OnSelectReleased(pressed_time);
      detect_very_long_press_ = false;
      press_time_ = 0;
    }
  }
  if (!panel.select && select_was_pressed_ && press_time_ != 0 && !detect_very_long_press_) {
    OnSelectReleased(milliseconds_ - press_time_ + 1);
    press_time_ = 0;
    detect_very_long_press_ = false;
  }
  select_was_pressed_ = panel.select;

  // filter the pot values and emit events when changed
  for (uint8_t i = 0; i < 4; ++i) {
    uint16_t adc_value = panel.pot[i];
    int32_t value = (31 * pot_filtered_value_[i] + adc_value) >> 5;
    pot_filtered_value_[i] = value;
    int32_t current_value = static_cast<int32_t>(pot_value_[i]);
    if (value >= current_value + kPotMoveThreshold ||
        value <= current_value - kPotMoveThreshold) {
      OnPotChanged(i, value);
      pot_value_[i] = value;
    }
  }

  // paint the interface
  switch (mode_) {
    case UI_MODE_SPLASH:
      if (animation_counter_ % 64 == 0) {
        for (int i = 0; i < kNumLeds; i++)
          leds_[i] = ((animation_counter_ / 64) % 4) == i;
        if (animation_counter_ / 64 > 3)
          mode_ = UI_MODE_NORMAL;
      }
      animation_counter_++;
      break;

    case UI_MODE_ZOOM:
      animation_counter_++;
      for (uint8_t i = 0; i < kNumLeds; i++)
        leds_[i] = false;
      leds_[feat_mode_] = animation_counter_ & 128;
      break;

    case UI_MODE_NORMAL:
      animation_counter_++;
      bool flash = (animation_counter_ & 64) && (animation_counter_ & 32) && (animation_counter_ & 16);
      for (uint8_t i = 0; i < kNumLeds; i++) {
        if (catchup_state_[i])
          leds_[i] = i == feat_mode_ ? !flash : flash;
        else
          leds_[i] = i == feat_mode_;
      }
      break;
  }
}

void Ui::OnSelectReleased(uint32_t duration) {
  if (duration > (uint32_t) kVeryLongPressDuration) {
  }
  else if (duration > (uint32_t) kLongPressDuration) {
    if (mode_ == UI_MODE_NORMAL)
      mode_ = UI_MODE_ZOOM;
    else if (mode_ == UI_MODE_ZOOM)
      mode_ = UI_MODE_NORMAL;
  }
  else {
    switch (mode_) {
      case UI_MODE_SPLASH:
        break;
      case UI_MODE_ZOOM:
        // detect if pots have moved during zoom
        for (int i = 0; i < 4; i++)
          if (abs(pot_value_[i] - pot_coarse_value_[i]) > kCatchupThreshold)
            catchup_state_[i] = true;
        mode_ = UI_MODE_NORMAL;
        break;
      case UI_MODE_NORMAL:
        feat_mode_ = static_cast<FeatureMode>((feat_mode_ + 1) % FEAT_MODE_LAST);
        // reset pots fine value
        for (int i = 0; i < 4; i++)
          pot_fine_value_[i] = 1 << 15;
        break;
    }
  }
}

void Ui::OnPotChanged(uint8_t pot, uint16_t value) {
  switch (mode_) {
    case UI_MODE_SPLASH:
      break;
    case UI_MODE_ZOOM:
      pot_fine_value_[pot] = value;
      break;
    case UI_MODE_NORMAL:
      if (!catchup_state_[pot]) {
        pot_coarse_value_[pot] = value;
      }
      else if (abs(value - pot_coarse_value_[pot]) < kCatchupThreshold) {
        pot_coarse_value_[pot] = value;
        catchup_state_[pot] = false;
      }
      break;
  }
}

}  // namespace batumi

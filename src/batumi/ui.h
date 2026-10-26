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
// User interface. Port of the Batumi firmware UI for VCV Rack: the event queue,
// switch debouncing and flash storage are replaced by direct polling of the
// panel state, the behavior of buttons, pots and LEDs is unchanged.

#ifndef BATUMI_UI_H_
#define BATUMI_UI_H_

#include "stmlib/stmlib.h"

namespace batumi {

enum FeatureMode {
  FEAT_MODE_FREE,
  FEAT_MODE_QUAD,
  FEAT_MODE_PHASE,
  FEAT_MODE_DIVIDE,
  FEAT_MODE_LAST
};

enum UiMode {
  UI_MODE_SPLASH,
  UI_MODE_NORMAL,
  UI_MODE_ZOOM,
};

const uint8_t kNumLeds = 4;

/** Panel state, sampled once per millisecond */
struct PanelState {
  uint16_t pot[4];
  bool sync, wav1, wav2, select;
};

class Ui {
 public:
  void Init(const PanelState& panel);
  // Called once per millisecond (SysTick in the hardware)
  void Poll(const PanelState& panel);

  uint16_t coarse(uint8_t channel) const { return pot_coarse_value_[channel]; }
  int16_t fine(uint8_t channel) const { return pot_fine_value_[channel] - 32768; }

  inline FeatureMode feat_mode() const { return feat_mode_; }
  inline UiMode mode() const { return mode_; }
  inline uint8_t shape() const { return (wav2_ << 1) | wav1_; }
  inline bool sync_mode() const { return sync_; }
  inline bool led(uint8_t i) const { return leds_[i]; }

  // Persistent settings (flash storage in the hardware)
  FeatureMode feat_mode_ = FEAT_MODE_FREE;
  uint16_t pot_fine_value_[4] = {1 << 15, 1 << 15, 1 << 15, 1 << 15};

 private:
  void OnSelectReleased(uint32_t duration);
  void OnPotChanged(uint8_t pot, uint16_t value);

  uint16_t pot_value_[4];
  uint16_t pot_filtered_value_[4];
  uint16_t pot_coarse_value_[4];
  bool catchup_state_[4];
  uint32_t milliseconds_ = 0;
  uint32_t press_time_ = 0;
  bool detect_very_long_press_ = false;
  bool select_was_pressed_ = false;
  int32_t animation_counter_ = 0;
  UiMode mode_ = UI_MODE_SPLASH;
  bool sync_ = false, wav1_ = false, wav2_ = false;
  bool leds_[kNumLeds] = {};
};

}  // namespace batumi

#endif  // BATUMI_UI_H_

// Replacement for the Batumi PWM DAC driver: values are read by the VCV module.

#ifndef BATUMI_DRIVERS_DAC_H_
#define BATUMI_DRIVERS_DAC_H_

#include "stmlib/stmlib.h"

namespace batumi {

class Dac {
 public:
  int16_t sine_[4] = {};
  int16_t asgn_[4] = {};

  inline void set_sine(uint8_t channel, int16_t value) { sine_[channel] = value; }
  inline void set_asgn(uint8_t channel, int16_t value) { asgn_[channel] = value; }
};

}  // namespace batumi

#endif  // BATUMI_DRIVERS_DAC_H_

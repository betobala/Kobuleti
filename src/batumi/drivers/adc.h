// Replacement for the Batumi ADC driver: values are written by the VCV module.

#ifndef BATUMI_DRIVERS_ADC_H_
#define BATUMI_DRIVERS_ADC_H_

#include "stmlib/stmlib.h"
#include <stdlib.h>  // abs(), implicitly available in the ARM toolchain

namespace batumi {

class Adc {
 public:
  // Signed 16 bit readings, full scale = +/-5 V
  int16_t cv_[4] = {};
  int16_t reset_[4] = {};

  inline int16_t cv(uint8_t i) const { return cv_[i]; }
  inline int16_t reset(uint8_t i) const { return reset_[i]; }
};

}  // namespace batumi

#endif  // BATUMI_DRIVERS_ADC_H_

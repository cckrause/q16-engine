#include "test_harness.h"
#include "types/fixed16.h"

void test_fixed16(void) {
  TEST_SUITE_BEGIN("fixed16");

  TEST_CHECK("FIXED(5)", FIXED(5) == 0x50000);
  TEST_CHECK("mul16(3,4)=12", mul16(FIXED(3), FIXED(4)) == FIXED(12));
  TEST_CHECK("div16(10,3)~3.333", div16(FIXED(10), FIXED(3)) == 0x35555);
  TEST_CHECK("div16(1,2)=0.5", div16(FIXED16_ONE, FIXED(2)) == FIXED16_HALF);
  TEST_CHECK("float_to_fixed16(1.5)=0x18000", float_to_fixed16(1.5f) == 0x18000);
  TEST_CHECK("fixedSqrt(4)=2", fixed16_sqrt(FIXED(4)) == FIXED(2));

  // fixedSqrt(2) ~ 1.4142 = 0x16A0A (allow +-2 for rounding)
  Fixed16 sqrt2 = fixed16_sqrt(FIXED(2));
  TEST_CHECK("fixedSqrt(2)~1.4142", sqrt2 >= 0x16A08 && sqrt2 <= 0x16A0C);

  TEST_CHECK("fract16(0x18000)=0x8000", fixed16_fract(0x18000) == 0x8000);
  TEST_CHECK("round16(0x18000)=2", fixed16_round(0x18000) == 2);
  TEST_CHECK("clamp(5,0,3)=3", fixed16_clamp(FIXED(5), FIXED(0), FIXED(3)) == FIXED(3));

  TEST_SUITE_END();
}

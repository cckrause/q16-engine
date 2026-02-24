#include "math/trig_table.h"
#include "test_harness.h"
#include "types/fixed16.h"
#include "types/types.h"

void test_trig(void) {
  TEST_SUITE_BEGIN("trig");

  Fixed16 s, c;

  sin_cos_fixed(0, &s, &c);
  TEST_CHECK("cos(0)=1.0", c == FIXED16_ONE);
  TEST_CHECK("sin(0)=0", s == 0);

  sin_cos_fixed(ANGLE14_QUARTER, &s, &c);
  TEST_CHECK("cos(90)~0", c >= -2 && c <= 2);
  TEST_CHECK("sin(90)~1.0", s >= FIXED16_ONE - 2 && s <= FIXED16_ONE);

  sin_cos_fixed(ANGLE14_HALF_CIRCLE, &s, &c);
  TEST_CHECK("cos(180)~-1.0", c <= -FIXED16_ONE + 2 && c >= -FIXED16_ONE);
  TEST_CHECK("sin(180)~0", s >= -2 && s <= 2);

  TEST_SUITE_END();
}

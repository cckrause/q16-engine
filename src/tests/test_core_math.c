#include "math/core_math.h"
#include "test_harness.h"
#include "types/fixed16.h"

void test_core_math(void) {
  TEST_SUITE_BEGIN("core_math");

  // dist_approx

  // Axis-aligned: exact
  TEST_CHECK("dist(0,0,10,0)=10", dist_approx(0, 0, FIXED(10), 0) == FIXED(10));
  TEST_CHECK("dist(0,0,0,10)=10", dist_approx(0, 0, 0, FIXED(10)) == FIXED(10));
  // Diagonal: 10+10/2 = 15 (true: 14.14)
  TEST_CHECK("dist(0,0,10,10)=15", dist_approx(0, 0, FIXED(10), FIXED(10)) == FIXED(15));
  // 3-4-5: FIXED(4)+(FIXED(3)>>1)
  Fixed16 d345 = dist_approx(0, 0, FIXED(3), FIXED(4));
  TEST_CHECK("dist(0,0,3,4)~5", d345 == FIXED(4) + (FIXED(3) >> 1));

  // vec2_to_angle

  // Forward (dz>0) = 0
  TEST_CHECK("angle(0,+1)=0", vec2_to_angle(0, FIXED(1)) == 0);
  // Right (dx>0) = 4096 (90 deg)
  TEST_CHECK("angle(+1,0)=4096", vec2_to_angle(FIXED(1), 0) == 4096);
  // Backward = 8192 (180 deg)
  TEST_CHECK("angle(0,-1)=8192", vec2_to_angle(0, FIXED(-1)) == 8192);
  // Left = 12288 (270 deg)
  TEST_CHECK("angle(-1,0)=12288", vec2_to_angle(FIXED(-1), 0) == 12288);
  // 45 deg ~ 2048
  Angle14 a45 = vec2_to_angle(FIXED(1), FIXED(1));
  TEST_CHECK("angle(+1,+1)~2048", a45 >= 2040 && a45 <= 2056);

  // get_angle_difference

  TEST_CHECK("diff(0,4096)=+4096", get_angle_difference(0, 4096) == 4096);
  TEST_CHECK("diff(4096,0)=-4096", get_angle_difference(4096, 0) == -4096);
  // 100 to 16283: forward 16183 (>180), wrap to -201
  TEST_CHECK("diff(100,16283)=-201", get_angle_difference(100, 16283) == -201);
  // 15000 to 1000: wraps around forward
  TEST_CHECK("diff(15000,1000)=+2384", get_angle_difference(15000, 1000) == 2384);

  TEST_SUITE_END();
}

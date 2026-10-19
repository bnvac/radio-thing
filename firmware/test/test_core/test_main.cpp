#include <unity.h>

#include "tests.h"

void setUp() {}
void tearDown() {}

int main() {
    UNITY_BEGIN();
    run_ld2450_tests();
    run_protocol_tests();
    run_tracker_tests();
    run_blockage_tests();
    run_head_id_tests();
    run_debounce_tests();
    run_fuel_tests();
    run_geometry_tests();
    return UNITY_END();
}

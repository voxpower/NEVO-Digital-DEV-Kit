#ifndef CRA_TESTS_H
#define CRA_TESTS_H

#include <stdbool.h>

// Individual Runtime Test Case Functions (matching CRA-TR-NEVODIG-001)
bool cra_test_case_2_parameter_sanitization(void); // TON = 6000 (out-of-bounds)
bool cra_test_case_3_overread_mitigation(void);    // Block Read command 0x99
bool cra_test_case_4a_corrupt_payload(void);       // CRC16 Bit-Flip
bool cra_test_case_4b_truncated_frame(void);       // Ingestion Buffer Reset
bool cra_test_case_5_attack_surface_control(void); // General Call (0x00) & Reserved Reg (0xFF)
bool cra_test_case_6_scl_timeout_recovery(void);   // 30ms SCL Hold DoS Test

// Master Runner to execute all tests sequentially
bool cra_run_full_verification_suite(void);

#define WAIT_TIME 2000

#endif // CRA_TESTS_H
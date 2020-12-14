/* Copyright 2020 SiFive, Inc */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <metal/shutdown.h>
#include <metal/csr.h>

static bool unmi_int_trigger = false;
static bool unmi_excp_trigger = false;
static bool rnmi_int_trigger = false;
static bool rnmi_excp_trigger = false;

static void (*unmi_int_sub_handler)(void);
static void (*unmi_excp_sub_handler)(void);
static void (*rnmi_int_sub_handler)(void);
static void (*rnmi_excp_sub_handler)(void);

#define RESET()                     \
    unmi_int_trigger  = false;      \
    unmi_excp_trigger = false;      \
    rnmi_int_trigger  = false;      \
    rnmi_excp_trigger = false;      \
    unmi_int_sub_handler  = NULL;   \
    unmi_excp_sub_handler = NULL;   \
    rnmi_int_sub_handler  = NULL;   \
    rnmi_excp_sub_handler = NULL;

#define MNRET_RESTORE() {                   \
    __asm__ volatile("lw    ra,76(sp)");    \
    __asm__ volatile("lw    t0,72(sp)");    \
    __asm__ volatile("lw    t1,68(sp)");    \
    __asm__ volatile("lw    t2,64(sp)");    \
    __asm__ volatile("lw    s0,60(sp)");    \
    __asm__ volatile("lw    a0,56(sp)");    \
    __asm__ volatile("lw    a1,52(sp)");    \
    __asm__ volatile("lw    a2,48(sp)");    \
    __asm__ volatile("lw    a3,44(sp)");    \
    __asm__ volatile("lw    a4,40(sp)");    \
    __asm__ volatile("lw    a5,36(sp)");    \
    __asm__ volatile("lw    a6,32(sp)");    \
    __asm__ volatile("lw    a7,28(sp)");    \
    __asm__ volatile("lw    t3,24(sp)");    \
    __asm__ volatile("lw    t4,20(sp)");    \
    __asm__ volatile("lw    t5,16(sp)");    \
    __asm__ volatile("lw    t6,12(sp)");    \
    __asm__ volatile("addi  sp,sp,80");     \
    __asm__ volatile(".word 0x70200073");   \
}

__attribute__((section(".unmi_intvec"), interrupt)) void unmi_int_handler(void) {
    printf("UNMI interrupt handler: enter\n");
    unmi_int_trigger = true;

    if (unmi_int_sub_handler) {
        unmi_int_sub_handler();
    };

    // Disable pending interrupt bit.
    metal_set_nmi(UNMI, 0);

    // Restore with mnret.
    printf("UNMI interrupt handler: return\n");
    MNRET_RESTORE();
}

__attribute__((section(".unmi_excpvec"), interrupt)) void unmi_excp_handler(void) {
    printf("UNMI exception handler: enter\n");
    unmi_excp_trigger = true;

    if (unmi_excp_sub_handler) {
        unmi_int_sub_handler();
    }

    // Restore with mnret.
    printf("UNMI exception handler: return\n");
    MNRET_RESTORE();
}

__attribute__((section(".rnmi_intvec"), interrupt)) void rnmi_int_handler(void) {
    printf("RNMI interrupt handler: enter\n");
    rnmi_int_trigger = true;

    if (rnmi_int_sub_handler) {
        rnmi_int_sub_handler();
    };

    // Disable pending interrupt bit.
    metal_set_nmi(RNMI, 0);

    // Restore with mnret.
    printf("RNMI interrupt handler: return\n");
    MNRET_RESTORE();
}

__attribute__((section(".rnmi_excpvec"), interrupt)) void rnmi_excp_handler(void) {
    printf("RNMI exception handler: enter\n");
    rnmi_excp_trigger = true;

    if (rnmi_excp_sub_handler) {
        rnmi_excp_sub_handler();
    }

    // Restore with mnret.
    printf("RNMI exception handler: return\n");
    MNRET_RESTORE();
}

static void trigger_unmi(void) {
    printf("Try to trigger UNMI...\n");
    metal_set_nmi(UNMI, 1);
}

static void trigger_rnmi(void) {
    printf("Try to trigger RNMI...\n");
    metal_set_nmi(RNMI, 1);
}

static void trigger_illegal_insn_excp(void) {
    printf("Try to trigger illegal instruction exception...\n");
    __asm__ volatile("csrr x0, 0x999");
}

static bool test_case_1(void) {
    RESET();

    printf("=======================================\n");
    printf("Test case 1: UNMI interrupt is taken from");
    printf("all privilege modes whether or not interrupts are enabled.\n");
    printf("=======================================\n");

    // Disable mstatus.MIE and mie.MEIE.
    __asm__ volatile("csrc  mstatus, %0" :: "r" (1 << 3));
    __asm__ volatile("csrc  mie, %0" :: "r" (1 << 11));

    // Trigger UNMI.
    metal_set_nmi(UNMI, 1);

    // Check if UNMI is triggered even mie is disabled.
    return unmi_int_trigger;
}

static bool test_case_2(void) {
    RESET();

    printf("=======================================\n");
    printf("Test case 2: RNMI interrupt is taken from");
    printf("all privilege modes whether or not interrupts are enabled.\n");
    printf("=======================================\n");

    // Disable mstatus.MIE and mie.MEIE.
    __asm__ volatile("csrc  mstatus, %0" :: "r" (1 << 3));
    __asm__ volatile("csrc  mie, %0" :: "r" (1 << 11));

    // Trigger RNMI.
    metal_set_nmi(RNMI, 1);

    // Check if UNMI is triggered even mie is disabled.
    return rnmi_int_trigger;
}

static bool test_case_3(void) {
    RESET();

    printf("=======================================\n");
    printf("Test case 3: UNMI interrupt is taken within an RNMI handler.\n");
    printf("=======================================\n");

    rnmi_int_sub_handler = trigger_unmi;
    // Trigger RNMI.
    metal_set_nmi(RNMI, 1);

    // Check if both RNMI & UNMI are executed.
    return rnmi_int_trigger & unmi_int_trigger;
}

static bool test_case_4(void) {
    RESET();

    printf("=======================================\n");
    printf("Test case 4: RNMI interrupt is not taken within a UNMI handler.\n");
    printf("=======================================\n");

    unmi_int_sub_handler = trigger_rnmi;
    // Trigger UNMI.
    metal_set_nmi(UNMI, 1);

    // Check if only & UNMI are executed.
    return unmi_int_trigger && !rnmi_int_trigger;
}

static bool test_case_5(void) {
    RESET();

    printf("=======================================\n");
    printf("Test case 5: mnret executed in an UNMI handler re-enables ");
    printf("other interrupts including UNMI and RNMI.\n");
    printf("=======================================\n");

    // Trigger UNMI.
    metal_set_nmi(UNMI, 1);

    if (!unmi_int_trigger) {
        return false;
    }

    // Reset flag.
    unmi_int_trigger = false;

    // Trigger RNMI.
    metal_set_nmi(RNMI, 1);

    // Check if RNMI can be executed after mnret is executed in UNMI handler.
    return rnmi_int_trigger;
}

static bool test_case_6(void) {
    RESET();

    printf("=======================================\n");
    printf("Test case 6: mnret executed in an RNMI handler re-enables ");
    printf("other interrupts including UNMI and RNMI.\n");
    printf("=======================================\n");

    // Trigger RNMI.
    metal_set_nmi(RNMI, 1);

    if (!rnmi_int_trigger) {
        return false;
    }

    // Reset flag.
    rnmi_int_trigger = false;

    // Trigger RNMI once again.
    metal_set_nmi(RNMI, 1);

    // Check if RNMI can be executed after mnret is executed in RNMI handler.
    return rnmi_int_trigger;
}

static bool test_case_7(void) {
    RESET();

    printf("=======================================\n");
    printf("Test case 7: Exceptions in UNMI handler should be taken.\n");
    printf("=======================================\n");

    // Trigger UNMI.
    unmi_int_sub_handler = trigger_illegal_insn_excp;
    metal_set_nmi(UNMI, 1);

    // Check if both UNMI interrupt handler and exception handler are executed.
    return unmi_int_trigger && unmi_excp_trigger;
}

int main() {
    bool (*test_cases[])(void) = {
        test_case_1, test_case_2, test_case_3,
        test_case_4, test_case_5, test_case_6,
        //test_case_7,
    };

    printf("NMI test program\n");

    for (int i = 0; i < sizeof(test_cases) / sizeof(void *); i++) {
        if (!test_cases[i]()) {
            printf("FAIL!!\n");
            exit(1);
        }
    }

    printf("All test cases PASS!!\n");
}


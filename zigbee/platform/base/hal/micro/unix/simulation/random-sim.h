/***************************************************************************//**
 * @file
 * @brief Interface definitions for simulated random number generator.
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/
#ifndef __RANDOM_SIM_H__
#define __RANDOM_SIM_H__

// We offer two different stubs for random numbers, Clibs rand() and
// our own more predictable (and less random) ones.
extern bool stubsUseRand;

// Setting this to true causes the file and line numbers of the calls to
// the random number generator to be printed out as the program runs.
extern bool firmTraceRandomNumbers;

// If not using rand() this will return the value of the i'th random number.
uint16_t getRandomNumber(int i);

#endif //__RANDOM_SIM_H__

/***************************************************************************//**
 * @file
 * @brief random number generator implementation for simulator.
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
#include STACK_CORE_HEADER
#include "hal/hal.h"

#include <stdio.h>
#include <stdlib.h>

// Forward declarations
static unsigned long mersenneTwisterRand(void);
static void mersenneTwisterSeed(uint32_t seed);

//----------------------------------------------------------------
// To make it simpler for scripts to have access to the random numbers that
// the stack sees we generate all of them up front and then use them as needed.
// The scripts can index into the array of random numbers as needed.

#define randomVectorSize 4096
static bool randomInitialized = false;
static uint16_t randomNumbers[randomVectorSize];
static uint16_t nextRandom;

void initializeRandomNumbers(void)
{
  if (randomInitialized) {
    return;
  }
  randomInitialized = true;

  {
    int i;

    for (i = 0; i < randomVectorSize; i++) {
      randomNumbers[i] = mersenneTwisterRand();
    }
  }

  nextRandom = 0;
}

bool stubsUseRand = true;

uint16_t getRandomNumber(int i)
{
  assert(!stubsUseRand);
  initializeRandomNumbers();
  return randomNumbers[i % randomVectorSize];
}

// For the 'predictable' random numbers we don't allow re-seeding because it
// makes it harder to keep track of which random numbers go where.

void halStackSeedRandom(uint32_t seed)
{
  if (stubsUseRand) {
    mersenneTwisterSeed(seed);
  }
}

bool firmTraceRandomNumbers = false;

uint16_t halCommonGetRandomTraced(char *file, int line)
{
  if (stubsUseRand) {
    return mersenneTwisterRand() & 0xFFFF;
  } else {
    uint16_t result;
    initializeRandomNumbers();
    if (firmTraceRandomNumbers) {
      fprintf(stderr, "[random %d: %s line %d]\n", nextRandom, file, line);
    }
    result = randomNumbers[nextRandom];
    nextRandom = (nextRandom + 1) % randomVectorSize;
    return result;
  }
}

//----------------------------------------------------------------
// An implementation of the "Mersenne Twister" psuedorandom number generator.
// See http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/emt.html for more info.

#define REGISTER_SIZE 624
#define HIGH_BIT 0x80000000

static uint32_t shiftRegister[REGISTER_SIZE];
static int valuesUsed = REGISTER_SIZE + 1; // indicates no initialization

static void mersenneTwisterSeed(uint32_t seed)
{
  int i;
  shiftRegister[0] = seed;
  for (i = 1; i < REGISTER_SIZE; i++) {
    shiftRegister[i] =
      1812433253UL * (shiftRegister[i - 1] ^ (shiftRegister[i - 1] >> 30)) + i;
  }
  valuesUsed = REGISTER_SIZE;
}

#define MOD_REG_SIZE(i) (REGISTER_SIZE <= (i) ? (i) - REGISTER_SIZE : i)

static unsigned long mersenneTwisterRand(void)
{
  if (REGISTER_SIZE <= valuesUsed) {
    int i;

    if (valuesUsed == REGISTER_SIZE + 1) { // If uninitialized ...
      mersenneTwisterSeed(5489);           //   ... use a default seed
    }
    for (i = 0; i < REGISTER_SIZE; i++) {
      uint32_t temp = ((shiftRegister[i] & HIGH_BIT)
                       | (shiftRegister[MOD_REG_SIZE(i + 1)] & ~HIGH_BIT));
      shiftRegister[i] = (shiftRegister[MOD_REG_SIZE(i + 397)]
                          ^ (temp >> 1)
                          ^ ((temp & 1) ? 0x9908B0DFUL : 0));
    }
    valuesUsed = 0;
  }

  {
    uint32_t result = shiftRegister[valuesUsed++];

    result ^= (result >> 11);
    result ^= (result << 7) & 0x9D2C5680UL;
    result ^= (result << 15) & 0xEFC60000UL;
    result ^= (result >> 18);

    return result;
  }
}

// For testing.
//int main(void)
//{
//  int i;
//  for (i = 0; i < 1000; i++) {
//    printf("%10lu ", mersenneTwisterRand());
//    if (i % 5 ==4 )
//      printf("\n");
//  }
//  return 0;
//}

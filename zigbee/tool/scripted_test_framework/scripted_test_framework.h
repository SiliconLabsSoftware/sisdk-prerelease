/*
 * File: scripted_test_framework.h
 *
 * Description: generic unit test framework.
 *
 * Author(s): Bryan Murawsky, Maurizio Nanni
 *
 * Built on the original scripted test framrwork from Richard Kelsey.
 *
 * Copyright 2018 by Silicon Laboratories. All rights reserved.
 */

#ifndef SCRIPTED_TEST_FRAMEWORK_H
#define SCRIPTED_TEST_FRAMEWORK_H

typedef struct {
  uint32_t tick; // What time this event occurred
  char * id;
  char * caller;
  char * format;
  uint32_t length;
  uint8_t *data;
  void *next;
} ScriptCheckItem_t;

// We mark each script event with the file and line number of the source
// of the event.  The following three macros are used for the original
// source marker, for variables bound to a source marker, and for the
// values of those variables.

#define CODE_POINT       __FILE__, __LINE__
#define CODE_POINT_VARS  char *codePointFile, int codePointLine
#define CODE_POINT_VALUE codePointFile, codePointLine

// For keeping track of code points (locations in source files).
// A code point can also have a note, which helps keep track of things
// like loop iterations.  See the scriptNote() example of below.

typedef struct CodePointS {
  char                  *filename;
  int                   lineNumber;
  char                  *note;
  struct CodePointS     *next;
} CodePoint;

void pushCodePoint(char *filename, int lineNumber, const char *format, ...);
void popCodePoint(void);

#define scriptTrace(blob)                       \
  do { pushCodePoint(__FILE__, __LINE__, NULL); \
       blob;                                    \
       popCodePoint();                          \
  } while (0)

#define scriptNote(blob, format, ...)          \
  do { pushCodePoint(__FILE__, __LINE__,       \
                     (format), ##__VA_ARGS__); \
       blob;                                   \
       popCodePoint();                         \
  } while (0)

void pushScriptNote(const char *format, ...);
void popScriptNote(void);

// A version of assert() whose message includes the file, line number and an
// optional trace point. Use this instead of the regular assert() in action
// performers.

void scriptAssertInternal(CODE_POINT_VARS,
                          bool expressionBool,
                          char *expression);

#define scriptAssert(expression) \
  scriptAssertInternal(CODE_POINT, (expression), (#expression))

#define scriptAssertEqual(a, b, len) \
  scriptAssertInternal(CODE_POINT, memcmp((a), (b), (len)) == 0, "compare " #a " == " #b)

void passTimeInternal(CODE_POINT_VARS, uint32_t ticks, bool absolute);

#define passTime(ticks) \
  passTimeInternal(CODE_POINT, (ticks), false)

#define passTimeAbs(ticks) \
  passTimeInternal(CODE_POINT, (ticks), true)

// The meaning of a clock tick is determined by the test script,
// which must supply functions that advance the clock one tick and
// report the number of ticks so far.

void scriptTick(void);
uint32_t scriptTime(void);

void expectTestCheckInternal(CODE_POINT_VARS,
                             char *id,
                             char* format,
                             ...);

#define expectTestCheck(id, format, ...) \
  expectTestCheckInternal(CODE_POINT, (id), (format), ##__VA_ARGS__)

void expectNoTestCheckInternal(CODE_POINT_VARS);

#define expectNoTestCheck() \
  expectNoTestCheckInternal(CODE_POINT)

void internalPostTestCheck(char *caller, char *id, const char *format, ...);

#define postTestCheck(caller, id, format, ...) \
  internalPostTestCheck((caller), (id), (format), ##__VA_ARGS__)

//----------------------------------------------------------------
// Utility for naming a test on the command line.

typedef void (*Thunk)(void);
typedef struct {
  char *name;
  Thunk test;
} Test;

// users can define a suite of tests to be run together, with setup and
// teardown routines that are run between each entry
// passing NULL for either of the routines will instead NO_OP
void scriptedTestRunner(Test *suite, Thunk setupFunc, Thunk teardownFunc);

// 'argc' and 'argv' are as passed to main().  The command line should be
//    [--debug] test-name
// This searches function the array of tests looking for one named on the
// command line, whose Thunk is then returned.  If none is found an error
// message is printed and the program exits.
//
// 'scriptDebug' is set to TRUE if the optional '--debug' flag is present.

Thunk parseTestArgument(int argc, char **argv, Test *tests);

#endif // SCRIPTED_TEST_FRAMEWORK_H

/*
 * File: scripted_test_framework.c
 *
 * Description: generic unit test framework.
 *
 * Author(s): Bryan Murawsky, Maurizio Nanni
 *
 * Built on the original scripted test framework from Richard Kelsey.
 *
 * Copyright 2018 by Silicon Laboratories. All rights reserved.
 */

#include <stdarg.h>
#include <stdlib.h>     // for malloc()
#include <string.h>     // for memcpy()

#include PLATFORM_HEADER

#include "scripted_test_framework.h"

//------------------------------------------------------------------------------
// Static globals

static ScriptCheckItem_t* scriptItemList = NULL;
static uint32_t scriptTimingSlop = 5;
static bool scriptDebug = false;

//------------------------------------------------------------------------------
// Forward declarations

static ScriptCheckItem_t * createScriptCheckItem(char *caller,
                                                 char *id,
                                                 const char *format,
                                                 va_list argumentList);

static void freeScriptCheckItem(ScriptCheckItem_t *scriptItem);

static uint16_t writeScriptCheckItemData(ScriptCheckItem_t *scriptCheckItem,
                                         const char *format,
                                         va_list argumentList);

static void printErrorMessage(CODE_POINT_VARS,
                              ScriptCheckItem_t *want,
                              ScriptCheckItem_t *have);

static void printScriptCheckItem(ScriptCheckItem_t *scriptItem);

static void printCodePointStack(void);

void debugSimPrint(char* format, ...);
//------------------------------------------------------------------------------
// Test APIs

// users can define a suite of tests with setup and teardown routines
void scriptedTestRunner(Test *suite, Thunk setupFunc, Thunk teardownFunc)
{
  Test *runner = suite;
  while (runner) {
    debugSimPrint("\nrunning:%s", runner->name);
    Thunk run = runner->test;
    if (setupFunc != NULL) {
      setupFunc();
    }
    run();
    if (teardownFunc != NULL) {
      teardownFunc();
    }
    runner++;
    if (runner->test == NULL) {
      runner = NULL;
    }
  }
}

void passTimeInternal(CODE_POINT_VARS,
                      uint32_t ticks,
                      bool absolute)
{
  fprintf(stderr, ".");

  // Convert an absolute time into the relative number of ticks from now
  if (absolute) {
    if (ticks <= scriptTime()) {
      ticks = 0;
    } else {
      ticks = ticks - scriptTime();
    }
  }

  for (uint32_t i = 0; i < ticks; i++) {
    scriptTick();

    // Add a check function
    if (scriptItemList != NULL) {
      if ((scriptTime() - scriptItemList->tick) > scriptTimingSlop) {
        pushCodePoint(CODE_POINT_VALUE, NULL);
        printCodePointStack();
        fprintf(stderr, "\n   Unexpected event while passing time: ");
        printScriptCheckItem(scriptItemList);
        fprintf(stderr, "\n\n");
        exit(1);
      }
    }
  }
}

void expectTestCheckInternal(CODE_POINT_VARS,
                             char *id,
                             char* format,
                             ...)
{
  ScriptCheckItem_t *ptr;
  char *formatFinger;
  uint8_t *dataFinger;
  va_list argumentList;
  ScriptCheckItem_t *wanted;
  uint32_t currentSlop = scriptTimingSlop;

  fprintf(stderr, ".");

  va_start(argumentList, format);
  wanted = createScriptCheckItem("", id, format, argumentList);
  va_end(argumentList);
  va_start(argumentList, format);
  writeScriptCheckItemData(wanted, format, argumentList);
  va_end(argumentList);

  while (scriptItemList == NULL) {
    if (currentSlop == 0) {
      pushCodePoint(CODE_POINT_VALUE, NULL);
      printCodePointStack();
      fprintf(stderr, "\n\tran out of time at %u waiting for ", scriptTime());
      printScriptCheckItem(wanted);
      fprintf(stderr, "\n\n");
      exit(1);
    }

    scriptTick();
    currentSlop--;
  }

  if (strcmp(scriptItemList->id, id) != 0
      || strcmp(scriptItemList->format, format) != 0
      || (scriptTime() - scriptItemList->tick) > scriptTimingSlop) {
    printErrorMessage(CODE_POINT_VALUE,
                      wanted,
                      scriptItemList);
    exit(1);
  }

  // Remove the head from the list for processing
  ptr = scriptItemList;
  scriptItemList = ptr->next;

  // Check that the parameters of the event match
  dataFinger = ptr->data;

  va_start(argumentList, format);
  for (formatFinger = (char *) format; *formatFinger != 0; formatFinger++) {
    switch (*formatFinger) {
      case 's':
      case 'u':
      {
        uint8_t have = *((uint8_t *)dataFinger);
        uint8_t expected = (uint8_t)va_arg(argumentList, unsigned int);
        if (expected != have) {
          printErrorMessage(CODE_POINT_VALUE,
                            wanted,
                            ptr);
          exit(1);
        }
        dataFinger++;
        break;
      }
      case 'v':
      {
        uint16_t have = *((uint16_t *)dataFinger);
        uint16_t expected = (uint16_t)va_arg(argumentList, unsigned int);
        if (expected != have) {
          printErrorMessage(CODE_POINT_VALUE,
                            wanted,
                            ptr);
          exit(1);
        }
        dataFinger += sizeof(uint16_t);
        break;
      }
      case 'i':
      case 'w':
      {
        uint32_t have = *((uint32_t *)dataFinger);
        uint32_t expected = (uint32_t)va_arg(argumentList, uint32_t);
        if (expected != have) {
          printErrorMessage(CODE_POINT_VALUE,
                            wanted,
                            ptr);
          exit(1);
        }
        dataFinger += sizeof(uint32_t);
        break;
      }
      case 'b':
      {
        const uint8_t *expectedPtr = va_arg(argumentList, const uint8_t*);
        uint32_t expectedLength = va_arg(argumentList, uint32_t);

        uint32_t haveLength = *((uint32_t *)dataFinger);
        dataFinger += sizeof(uint32_t);
        uint8_t *havePtr = dataFinger;
        dataFinger += haveLength;

        if (expectedLength != haveLength
            || memcmp(havePtr, expectedPtr, haveLength) != 0) {
          printErrorMessage(CODE_POINT_VALUE,
                            wanted,
                            ptr);
          exit(1);
        }
        break;
      }
      case 'p':
      {
        void *expectedPtr = va_arg(argumentList, void*);
        void *havePtr = (*(void**)dataFinger);
        dataFinger += sizeof(void*);

        if (expectedPtr != havePtr) {
          printErrorMessage(CODE_POINT_VALUE,
                            wanted,
                            ptr);
          exit(1);
        }

        break;
      }
      default:
        // confused!
        assert(false);
        break;
    }
  }
  va_end(argumentList);

  // Free script item memory
  freeScriptCheckItem(ptr);
  freeScriptCheckItem(wanted);

  if (scriptDebug) {
    fprintf(stderr, "expectTestCheck():");
    pushCodePoint(CODE_POINT_VALUE, NULL);
    printCodePointStack();
    popCodePoint();
    fprintf(stderr, " Pass\n");
  }
}

void expectNoTestCheckInternal(CODE_POINT_VARS)
{
  if (scriptItemList != NULL) {
    printErrorMessage(CODE_POINT_VALUE,
                      NULL,
                      scriptItemList);
    exit(1);
  }
}

void scriptAssertInternal(CODE_POINT_VARS,
                          bool expressionBool,
                          char *expression)
{
  // If there was no error just print a '.' and finish
  if (expressionBool) {
    if (scriptDebug) {
      fprintf(stderr, "scriptAssert(): ");
      pushCodePoint(CODE_POINT_VALUE, NULL);
      printCodePointStack();
      popCodePoint();
      fprintf(stderr, " Pass\n");
    }
    fprintf(stderr, ".");
    return;
  }

  // Assertion has failed at this point
  pushCodePoint(CODE_POINT_VALUE, NULL);
  printCodePointStack();
  fprintf(stderr, "\nAssertion `%s' failed\n\n", expression);
  exit(1);
}

void internalPostTestCheck(char *caller, char *id, const char *format, ...)
{
  va_list argumentList;
  ScriptCheckItem_t *scriptItem;

  // Create the script check item
  va_start(argumentList, format);
  scriptItem = createScriptCheckItem(caller, id, format, argumentList);
  va_end(argumentList);

  // Write the script check item data
  va_start(argumentList, format);
  writeScriptCheckItemData(scriptItem, format, argumentList);
  va_end(argumentList);

  // Add this to the script item list
  ScriptCheckItem_t** ptr = &scriptItemList;
  while (*ptr != NULL) {
    ptr = (ScriptCheckItem_t**)&((*ptr)->next);
  }
  *ptr = scriptItem;
  if (scriptDebug) {
    fprintf(stderr, "postTestCheck() %s\n  ID: %s\n  Format:%s\n", caller, id, format);
  }
}

// Utility for naming a test on the command line.  This searches 'tests'
// for the one named on the command line.
Thunk parseTestArgument(int argc, char **argv, Test *tests)
{
  int i;
  int errors = 0;
  bool disabled = false;
  char *testName = NULL;
  char *me = *argv;   // Save program name.
  argv++; argc--;   // Skip program name.

  for (; argc > 0; argc--, argv++) {
    if (argv[0][0] == '-') {
      switch (argv[0][1]) {
        case '-': {
          if (strcmp(argv[0], "--debug") == 0) {
            scriptDebug = true;
          } else if (strcmp(argv[0], "--disable") == 0) {
            disabled = true;
          } else if (strcmp(argv[0], "--help") == 0) {
            errors++;
            break;
          } else {
            fprintf(stderr, "Invalid argument: %s\n", *argv);
            errors++;
          }
          break;
        }
        default:
          fprintf(stderr, "Invalid argument: %s\n", *argv);
          errors++;
      }
    } else if (testName == NULL) {
      testName = argv[0];
    } else {
      fprintf(stderr, "Too many test names: %s and %s\n", testName, *argv);
      errors++;
      break;
    }
  }

  if (testName == NULL) {
    errors += 1;
  }

  if (errors == 0) {
    for (i = 0;; i++) {
      if (tests[i].name == NULL) {
        fprintf(stderr, "Unrecognized test: '%s'\n", testName);
        errors += 1;
        break;
      } else if (strcmp(testName, tests[i].name) == 0) {
        if (disabled) {
          fprintf(stderr, "[***Test %s is disabled***]\n", testName);
          exit(0);
        } else {
          fprintf(stderr, "[Testing %s ", testName);
          return tests[i].test;
        }
      }
    }
  }

  if (errors != 0) {
    fprintf(stderr, "Usage: %s [--debug] [--disable] test\n", me);
    for (i = 0; tests[i].name != NULL; i++) {
      fprintf(stderr, "%s %s\n",
              i == 0 ? "Tests:" : "      ",
              tests[i].name);
    }
    exit(1);
  }

  return NULL;
}

static CodePoint *codePointStack = NULL;
static CodePoint *addCodePoint(char *filename, int lineNumber, char *note);

void pushCodePoint(char *filename, int lineNumber, const char *format, ...)
{
  char *note = NULL;
  va_list argList;

  if (format != NULL) {
    char buffer[4096];
    int length;

    va_start(argList, format);
    length = vsnprintf(buffer, sizeof(buffer), format, argList);
    va_end(argList);

    assert(0 < length);
    note = (char *) malloc(length + 1);
    assert(note != NULL);
    memcpy(note, buffer, length + 1);
  }

  codePointStack = addCodePoint(filename, lineNumber, note);
}

void popCodePoint(void)
{
  codePointStack = codePointStack->next;
}

static char *globalScriptNote = NULL;
void pushScriptNote(const char *format, ...)
{
  if (globalScriptNote != NULL) {
    fprintf(stderr, "\nError: script note already pushed\n\n");
    exit(1);
  }

  va_list argList;

  if (format != NULL) {
    char buffer[4096];
    int length;

    va_start(argList, format);
    length = vsnprintf(buffer, sizeof(buffer), format, argList);
    va_end(argList);

    assert(0 < length);
    globalScriptNote = (char *) malloc(length + 1);
    assert(globalScriptNote != NULL);
    memcpy(globalScriptNote, buffer, length + 1);
  }
}

void popScriptNote(void)
{
  free(globalScriptNote);
  globalScriptNote = NULL;
}

//------------------------------------------------------------------------------
// Static functions

static ScriptCheckItem_t * createScriptCheckItem(char *caller,
                                                 char *id,
                                                 const char *format,
                                                 va_list argumentList)
{
  const char *formatFinger;
  ScriptCheckItem_t *scriptItem =
    (ScriptCheckItem_t *)malloc(sizeof(ScriptCheckItem_t));
  scriptItem->length = 0;

  // Compute the amount of memory we need to allocate.
  for (formatFinger = (char *) format; *formatFinger != 0; formatFinger++) {
    switch (*formatFinger) {
      case 'u':
        va_arg(argumentList, unsigned int);
        scriptItem->length++;
        break;
      case 's':
        va_arg(argumentList, int);
        scriptItem->length++;
        break;
      case 'v':
        va_arg(argumentList, unsigned int);
        scriptItem->length += sizeof(uint16_t);
        break;
      case 'i':
      case 'w':
        va_arg(argumentList, uint32_t);
        scriptItem->length += sizeof(uint32_t);
        break;
      case 'b': {
        va_arg(argumentList, const uint8_t*);
        scriptItem->length += va_arg(argumentList, uint32_t) + sizeof(uint32_t);
        break;
      }
      case 'p': {
        va_arg(argumentList, void*);
        scriptItem->length += sizeof(void*);
        break;
      }
      default:
        assert(false);
        break;
    }
  }

  scriptItem->tick = scriptTime();
  if (id) {
    scriptItem->id = malloc(strlen(id) + 1);
    scriptItem->id = memcpy(scriptItem->id, id, strlen(id) + 1);
  } else {
    scriptItem->id = NULL;
  }
  if (caller) {
    scriptItem->caller = malloc(strlen(caller) + 1);
    scriptItem->caller = memcpy(scriptItem->caller, caller, strlen(caller) + 1);
  } else {
    scriptItem->caller = NULL;
  }
  scriptItem->format = malloc(strlen(format) + 1);
  scriptItem->format = memcpy(scriptItem->format, format, strlen(format) + 1);
  scriptItem->data = (uint8_t *)malloc(scriptItem->length);
  scriptItem->next = NULL;

  return scriptItem;
}

static uint16_t writeScriptCheckItemData(ScriptCheckItem_t *scriptCheckItem,
                                         const char *format,
                                         va_list argumentList)
{
  uint8_t *dataFinger = scriptCheckItem->data;
  const char *formatFinger;

  memset(scriptCheckItem->data, 0, scriptCheckItem->length);

  for (formatFinger = (char *) format; *formatFinger != 0; formatFinger++) {
    switch (*formatFinger) {
      case 'u':
        *dataFinger++ = va_arg(argumentList, unsigned int);
        break;
      case 's':
        *dataFinger++ = va_arg(argumentList, int);
        break;
      case 'v':
        *((uint16_t*)dataFinger) = va_arg(argumentList, unsigned int);
        dataFinger += sizeof(uint16_t);
        break;
      case 'i':
      case 'w':
        if (sizeof(unsigned int) < sizeof(uint32_t)) {
          *((uint32_t*)dataFinger) = va_arg(argumentList, uint32_t);
        } else {
          *((unsigned int*)dataFinger) = va_arg(argumentList, unsigned int);
        }
        dataFinger += sizeof(uint32_t);
        break;
      case 'b': {
        const uint8_t *data = va_arg(argumentList, const uint8_t*);
        uint32_t dataSize = va_arg(argumentList, uint32_t);

        *((uint32_t*)dataFinger) = dataSize;
        dataFinger += sizeof(uint32_t);

        if (dataSize > 0) {
          if (data != NULL) {
            memcpy(dataFinger, data, dataSize);
          } else {
            memset(dataFinger, 0, dataSize);
          }
        }

        dataFinger += dataSize;
        break;
      }
      case 'p': {
        void *ptr = va_arg(argumentList, void*);
        *((void**)dataFinger) = ptr;
        dataFinger += sizeof(void*);
        break;
      }
      default:
        // confused!
        assert(false);
        break;
    }
  }

  uint32_t length = dataFinger - scriptCheckItem->data;
  // sanity check
  assert(length <= scriptCheckItem->length);

  return length;
}

static void freeScriptCheckItem(ScriptCheckItem_t *scriptItem)
{
  // Free script item memory
  free(scriptItem->id);
  free(scriptItem->caller);
  free(scriptItem->format);
  free(scriptItem->data);
  free(scriptItem);
}

static void printScriptCheckItem(ScriptCheckItem_t *scriptItem)
{
  uint8_t *dataFinger = scriptItem->data;
  const char *formatFinger;

  fprintf(stderr, "[%s", scriptItem->id);

  for (formatFinger = scriptItem->format; *formatFinger != 0; formatFinger++) {
    switch (*formatFinger) {
      case 's':
      case 'u':
      {
        uint8_t printValue = *((uint8_t *)dataFinger);
        fprintf(stderr, " %d", printValue);
        dataFinger++;
        break;
      }
      case 'v':
      {
        uint16_t printValue = *((uint16_t *)dataFinger);
        fprintf(stderr, " %d", printValue);
        dataFinger += sizeof(uint16_t);
        break;
      }
      case 'i':
      case 'w':
      {
        uint32_t printValue = *((uint32_t *)dataFinger);
        fprintf(stderr, " %d", printValue);
        dataFinger += sizeof(uint32_t);
        break;
      }
      case 'b':
      {
        uint32_t printLength = *((uint32_t *)dataFinger);
        dataFinger += sizeof(uint32_t);
        uint8_t *printPtr = dataFinger;
        dataFinger += printLength;
        uint32_t i;

        fprintf(stderr, " [ ");
        for (i = 0; i < printLength; i++) {
          fprintf(stderr, "%.2x ", printPtr[i]);
        }
        fprintf(stderr, "]");
        break;
      }
      case 'p':
      {
        void* printValue = (*(void**)dataFinger);
        fprintf(stderr, " %p", printValue);
        dataFinger += sizeof(void*);
        break;
      }
      default:
        // confused!
        assert(false);
        break;
    }
  }
  fprintf(stderr, "]");
}

static void printErrorMessage(CODE_POINT_VARS,
                              ScriptCheckItem_t *want,
                              ScriptCheckItem_t *have)
{
  pushCodePoint(CODE_POINT_VALUE, NULL);
  printCodePointStack();

  fprintf(stderr, "\nTest failed:\n");
  fprintf(stderr, "            got: ");
  if (have) {
    printScriptCheckItem(have);
  } else {
    fprintf(stderr, "none");
  }
  fprintf(stderr, "\n");
  fprintf(stderr, "     but wanted: ");
  if (want) {
    printScriptCheckItem(want);
  } else {
    fprintf(stderr, "none");
  }

  fprintf(stderr, "\n\n");
}

static CodePoint *addCodePoint(char *filename, int lineNumber, char *note)
{
  CodePoint *codePoint = (CodePoint *) malloc(sizeof(CodePoint));
  assert(codePoint != NULL);
  codePoint->filename = filename;
  codePoint->lineNumber = lineNumber;
  codePoint->note = note;
  codePoint->next = codePointStack;
  return codePoint;
}

static bool sameCodePoint(CodePoint *x, CodePoint *y)
{
  return (strcmp(x->filename, y->filename) == 0
          && x->lineNumber == y->lineNumber);
}

static void printCodePointStack(void)
{
  int cnt = 0;
  CodePoint *codePoint = codePointStack;
  CodePoint *last = NULL;
  while (codePoint != NULL) {
    CodePoint *next = codePoint->next;

    if (last != NULL
        && sameCodePoint(codePoint, last)
        && codePoint->note == NULL) {
      // do nothing
    } else if (next != NULL
               && sameCodePoint(codePoint, next)
               && codePoint->note == NULL) {
      // do nothing
    } else {
      fprintf(stderr, "\n  %s:%d", codePoint->filename, codePoint->lineNumber);
      if (codePoint->note != NULL) {
        fprintf(stderr, " - %s", codePoint->note);
      }
      if (cnt == 0 && globalScriptNote) {
        fprintf(stderr, " - %s", globalScriptNote);
      }
      last = codePoint;
    }
    codePoint = next;
    cnt++;
  }
}

//------------------------------------------------------------------------------
// Handy debugging functions

void simPrintStartLine(void)
{
  fprintf(stderr, "[");
}

void vSimPrint(char *format, va_list argPointer)
{
  vfprintf(stderr, format, argPointer);
  putc('\n', stderr);
}

void debugSimPrint(char* format, ...)
{
  if (scriptDebug) {
    va_list argPointer;
    va_start(argPointer, format);
    vSimPrint(format, argPointer);
    va_end(argPointer);
  }
}

void debugPrintTextAndHex(const char* text,
                          const uint8_t* hexData,
                          uint8_t length,
                          uint8_t spaceEveryXChars,
                          bool finalCr)
{
  if (scriptDebug) {
    uint8_t i;
    fprintf(stderr, "%s", text);
    for (i = 0; i < length; i++) {
      if (i != 0 && (i % spaceEveryXChars == 0)) {
        fprintf(stderr, " ");
      }
      fprintf(stderr, "%02X", hexData[i]);
    }
    if (finalCr) {
      fprintf(stderr, "\n");
    }
  }
}

void simPrint(char* format, ...)
{
  va_list argPointer;
  va_start(argPointer, format);
  vSimPrint(format, argPointer);
  va_end(argPointer);
}

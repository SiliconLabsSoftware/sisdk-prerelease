/***************************************************************************//**
 * @file
 * @brief
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
// Automatic Prototypes
// This block pulls in the file #define as CMDHEADER and is responsible for
// generating the function prototypes for all functions defined in CMD().
#define CMDLIST(listName)
#define CMD(func, cmdName, args, helpStr)  void func(uint16_t param1, uint16_t param2);
#define CMD2(func, cmdName, args, helpStr)
#define CMDX(x, func, cmdName, args, helpStr)
#include CMDHEADER
#undef CMDX
#undef CMD2
#undef CMD
#undef CMDLIST

// Flash strings
// This block pulls in the file #define as CMDHEADER and is responsible for
// generating the set of character arrays needed to store the arguement
// strings and the help strings.
#define CMDLIST(listName)
  #define CMD(func, cmdName, args, helpStr)  char const func##Args[] = args; \
  char const func##Help[] = helpStr;
  #define CMD2(func, cmdName, args, helpStr) char const func##Args2[] = args; \
  char const func##Help2[] = helpStr;
  #define CMDX(x, func, cmdName, args, helpStr) char const func##x##Args[] = args; \
  char const func##x##Help[] = helpStr;

#include CMDHEADER
#undef CMDX
#undef CMD2
#undef CMD
#undef CMDLIST

// Actual definition of the list
// This block pulls in the file #define as CMDHEADER and is responsible for
// generating the data structure, of type commandType and named by the
// CMDLIST macro, that holds all command information.  The commandType is
// defined in haltest.h and this structure is used by the runCommand() and
// printCommands() functions.
#define CMDLIST(listName)               commandType const listName[] = {
  #define CMD(func, cmdName, args, helpStr)  { func, cmdName, func##Args, func##Help },
  #define CMD2(func, cmdName, args, helpStr) { func, cmdName, func##Args2, func##Help2 },
  #define CMDX(x, func, cmdName, args, helpStr) { func, cmdName, func##x##Args, func##x##Help },
  #include CMDHEADER
{ NULL, NULL, NULL, NULL }
};

#undef CMDX
#undef CMD2
#undef CMD
#undef CMDLIST

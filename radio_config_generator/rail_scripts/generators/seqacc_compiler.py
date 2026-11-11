#!/usr/bin/python

import yaml
import argparse
import os
import re
import sys
from jinja2 import Environment, FileSystemLoader
import subprocess
import inspect
from host_py_rm_studio_internal import RM_Factory, factory

sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)), '../..'))
TEMPLATE_PATH = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'templates')

MACROS = {'SET': {'Action': 'OR'},
          'CLEAR': {'Action': 'AND', 'isNotArg': True},
          'REGALL': {'Action': 'SKIPCOND_REGALL'},
          'REGANY': {'Action': 'SKIPCOND_REGANY'},
          'REGNEGALL': {'Action': 'SKIPCOND_REGNEGALL'},
          'REGNEGANY': {'Action': 'SKIPCOND_REGNEGANY'},
          'REGEQ0': {'Action': 'SKIPCOND_REGEQ0'},
          'REGNEQ0': {'Action': 'SKIPCOND_REGNEQ0'},
          'REGEQ1': {'Action': 'SKIPCOND_REGEQ1'},
          'REGNEQ1': {'Action': 'SKIPCOND_REGNEQ1'},
          'SIGALL': {'Action': 'SKIPCOND_SIGALL'},
          'SIGANY': {'Action': 'SKIPCOND_SIGANY'},
          'SIGNEGALL': {'Action': 'SKIPCOND_SIGNEGALL'},
          'SIGNEGANY': {'Action': 'SKIPCOND_SIGNEGANY'},
          'SIGEQ0': {'Action': 'SKIPCOND_SIGEQ0'},
          'SIGNEQ0': {'Action': 'SKIPCOND_SIGNEQ0'},
          'SIGEQ1': {'Action': 'SKIPCOND_SIGEQ1'},
          'SIGNEQ1': {'Action': 'SKIPCOND_SIGNEQ1'},
          'MACRO_WRITE': {'Action': 'WRITE', 'arg': [{'RAC_SCRATCH0': 0xABADCAFE}]}  # example of macro
          }

# Depending on the action, the synthax can change. For example, a simple WRITE can be seen as writing that <value> to
# <destination>, where <value> can be a register, mask, shift, etc. or an integer.
# But for 'MOV' action, it is moving a <value at a given address> to a <destination>.
ACTION_USING_REG_ADDR = [
    'MOVBLOCK', 'MOV', 'SAVE_AND_RESTORE'
]

# SEQACC argument fields that are hard-coded
ARG_OFFSET_POS = 0
ARG_OPCODE_POS = 28

# Runtime / reserved SEQACC BASE[] slots. Indices come from RuntimeBases in
# seqacc_regbases.yml. Families opt in per alias with "<alias>: runtime".
# DYNAMIC is always reserved; other entries (e.g. REPAINT_MEM) only reserve
# their index on families that opted in.
RUNTIME_ALIAS_VALUE = 'runtime'
REQUIRED_RUNTIME_BASES = ('REPAINT_MEM', 'DYNAMIC')
ALWAYS_ACTIVE_RUNTIME_BASES = ('DYNAMIC',)


def loadRuntimeBases(regBaseYml):
    """Load and validate the top-level RuntimeBases map from seqacc_regbases.yml."""
    runtimeBases = regBaseYml.get('RuntimeBases')
    if not isinstance(runtimeBases, dict) or not runtimeBases:
        raise Exception('seqacc_regbases.yml must define a top-level RuntimeBases map '
                        '(e.g. REPAINT_MEM: 14, DYNAMIC: 15).')

    loaded = {}
    for name, index in runtimeBases.items():
        if isinstance(index, bool) or not isinstance(index, int):
            raise Exception('RuntimeBases.{} must be an integer BASE index, not {}.'.format(name, index))
        loaded[str(name)] = index

    for required in REQUIRED_RUNTIME_BASES:
        if required not in loaded:
            raise Exception('RuntimeBases is missing required entry {}.'.format(required))

    if len(set(loaded.values())) != len(loaded):
        raise Exception('RuntimeBases indices must be unique.')

    return loaded


def runtimeBaseMacroName(alias):
    """C macro for a runtime base. REPAINT_MEM -> SEQACC_REGBASES_REPAINT_BASE_ENTRY."""
    stem = alias[:-4] if alias.endswith('_MEM') else alias
    return 'SEQACC_REGBASES_{}_BASE_ENTRY'.format(stem)


def isRuntimeBaseActive(name, aliases, runtimeBases):
    """True if this RuntimeBases entry is reserved for the current family."""
    if name not in runtimeBases:
        return False
    if name in ALWAYS_ACTIVE_RUNTIME_BASES:
        return True
    return (aliases or {}).get(name) == RUNTIME_ALIAS_VALUE


def isRuntimeDest(dest, alias, runtimeBases, seqName):
    """True if dest is a RuntimeBases alias. Raises if the family did not opt in."""
    if dest not in runtimeBases:
        return False
    target = (alias or {}).get(dest)
    if target == RUNTIME_ALIAS_VALUE:
        return True
    if dest in (alias or {}):
        raise KeyError('{} used in sequence {} but Alias {} is {}, not "{}". '
                       'Runtime bases are not addresses; see RuntimeBases in seqacc_regbases.yml.'.format(
                           dest, seqName, dest, target, RUNTIME_ALIAS_VALUE))
    raise KeyError('{} used in sequence {} but this part did not opt in. '
                   'Add "{}: {}" under Alias in seqacc_regbases.yml.'.format(
                       dest, seqName, dest, RUNTIME_ALIAS_VALUE))


def runtimeNameForIndex(runtimeBases, index, aliases=None):
    """Return the active RuntimeBases alias for a reserved index, or None."""
    for name, reservedIndex in runtimeBases.items():
        if reservedIndex == index and isRuntimeBaseActive(name, aliases, runtimeBases):
            return name
    return None


class CompilerContext(object):
    def __init__(self, chip, reg_base_yml, sequences_yml):
        self.sequences_yml = sequences_yml
        self.fileName = sequences_yml['Name']
        self.fileInclude = sequences_yml.get('FileIncludes')
        self.sequences = {}
        self.regBases = {}
        self.runtimeBases = loadRuntimeBases(reg_base_yml)

        self.chip = chip.upper()
        self.part = self.chip
        try:
            factory.get_mod_name(self.chip)
        except KeyError:
            for code_name in factory.RM_S3_PART_FAMILY_NAMES:
                try:
                    if factory.get_mod_name(code_name).upper() == self.chip or code_name in self.chip:
                        self.part = code_name
                        break
                except KeyError:
                    pass

        self.chip_base_addr = reg_base_yml.get(chip)
        if self.chip_base_addr is None:
            self.chip_base_addr = reg_base_yml.get(self.part)
            if self.chip_base_addr is None:
                raise Exception("{} part is not defined in seqacc_regbases.yml."
                                "\nCheck if base addresses are defined for this part.".format(chip))

        rm_factory = RM_Factory(self.part)
        self.rm = rm_factory()

        rail_scripts_path = os.path.split(os.path.dirname(os.path.abspath(__file__)))[0]
        self.rail_scripts_dir, self.rail_scripts_branch, self.rail_scripts_commit = \
            self.get_git_dir_branch_and_commit(rail_scripts_path)

        rm_path = os.path.dirname(inspect.getfile(RM_Factory(self.part)))
        self.rm_dir, self.rm_branch, self.rm_commit = \
            self.get_git_dir_branch_and_commit(rm_path)

    def handler(self):
        for sequenceName, sequenceValue in self.sequences_yml['Sequences'].items():
            print("\n")
            sequence = Sequence(self.chip, self.part, self.rm, self.chip_base_addr,
                                sequenceName, sequenceValue, self.runtimeBases)
            sequence.validateRuntimeBases()
            sequence.sequenceHandler()
            self.sequences[sequenceName] = sequence

            base_addr_config_name = sequence.getBaseAddrConfigName()
            if base_addr_config_name not in self.regBases:
                self.regBases[base_addr_config_name] = {}
                self.regBases[base_addr_config_name]['BaseAddr'] = sequence.baseAddrConfig['BaseAddr']
                self.regBases[base_addr_config_name]['Define'] = sequence.baseAddrConfig.get('Define', 'SEQACC_BASE_ADDR_CFG_DFLT_ARRAY_SZ')

                # Get the positions of the fields in the SEQACC argument
                self.regBases[base_addr_config_name]['OpcodePos'] = ARG_OPCODE_POS
                self.regBases[base_addr_config_name]['BasePos'] = sequence.basePos
                self.regBases[base_addr_config_name]['BasePosRegField'] = sequence.basePos << 4
                self.regBases[base_addr_config_name]['ContWrPos'] = sequence.contWrPos
                self.regBases[base_addr_config_name]['OffsetPos'] = ARG_OFFSET_POS

                # Get the masks to each field in the SEQACC argument
                self.regBases[base_addr_config_name]['OpcodeMask'] = ((1 << ARG_OPCODE_POS) - 1) ^ 0xFFFFFFFF
                self.regBases[base_addr_config_name]['BaseMask'] = ((1 << (ARG_OPCODE_POS - sequence.basePos)) - 1) << sequence.basePos
                self.regBases[base_addr_config_name]['OffsetAddrMask'] = (2 ** (sequence.contWrPos) - 1)
                self.regBases[base_addr_config_name]['ContWrMask'] = ((self.regBases[base_addr_config_name]['OpcodeMask']
                                                                             | self.regBases[base_addr_config_name]['BaseMask']
                                                                             | self.regBases[base_addr_config_name]['OffsetAddrMask'])
                                                                            ^ 0xFFFFFFFF)
                # Mask to get the base address from a 32-bit address
                self.regBases[base_addr_config_name]['BaseAddrMask'] = self.regBases[base_addr_config_name]['OffsetAddrMask'] ^ 0xFFFFFFFF
                self.regBases[base_addr_config_name]['BitOpMask'] = 0
                for i, addr in enumerate(list(sequence.baseAddrConfig['BaseAddr'].keys())[:-1]):
                    if addr != 0xB0000000 and addr != 0xA0000000:
                        self.regBases[base_addr_config_name]['BitOpMask'] |= (1 << i)

            print("{}: {} bytes".format(sequenceName, sequence.lengthWord * 4))

    def getRender(self):
        aliases = {}
        if self.sequences:
            aliases = next(iter(self.sequences.values())).alias or {}
        runtimeBases = [{'alias': alias,
                         'index': index,
                         'macro': runtimeBaseMacroName(alias)}
                        for alias, index in self.runtimeBases.items()
                        if isRuntimeBaseActive(alias, aliases, self.runtimeBases)]
        return {'RegBases': self.regBases, 'Sequences': self.sequences,
                'FileName': self.fileName, 'FileIncludes': self.fileInclude,
                'RuntimeBases': runtimeBases,
                'RailScriptsDir': self.rail_scripts_dir, 'RailScriptsCommit': self.rail_scripts_commit,
                'RmDir': self.rm_dir, 'RmCommit': self.rm_commit}

    @staticmethod
    def get_git_dir_branch_and_commit(path):
        if 'rail_scripts' in path:
            root_dir = 'rail_scripts'
        elif 'host_py_rm_studio_internal' in path:
            root_dir = 'host_py_rm_studio_internal'
        else:
            raise Exception("Path {} cannot be handled".format(path))

        git_dir = path.split(os.sep)
        git_dir = git_dir[git_dir.index(root_dir):]
        git_dir = os.path.join(*git_dir)

        git_branch = None
        git_commit = None
        try:
            git_commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=path)
            git_commit = git_commit.strip()
            git_commit = str(git_commit.decode('utf-8')).replace('\n', '')

            git_branch = subprocess.check_output(['git', 'rev-parse', '--abbrev-ref', 'HEAD'], cwd=path)
            git_branch = git_branch.strip()
            git_branch = str(git_branch.decode('utf-8')).replace('\n', '')
        except Exception as ex:
            pass

        return git_dir, git_branch, git_commit


class Sequence(object):
    def __init__(self, chip, part, rm, chip_base_addr, name, data, runtimeBases):
        self.chip = chip
        self.part = part
        self.rm = rm
        self.chip_base_addr = chip_base_addr
        self.name = name
        self.data = data
        self.runtimeBases = runtimeBases

        base_addr_config_name = self.getBaseAddrConfigName()
        self.baseAddrConfig = self.chip_base_addr[base_addr_config_name]

        self.basePos = self.baseAddrConfig.get('BasePos')
        if self.basePos is None:
            raise Exception("BasePos field not found in {}".format(base_addr_config_name))
        self.basePos &= 0x1F

        self.alias = self.baseAddrConfig.get('Alias')

        self.contWrPos = self.baseAddrConfig.get('ContWrPos')
        if self.contWrPos is None:
            raise Exception("ContWrPos field not found in {}".format(base_addr_config_name))

        self.sequenceCfg = self._encodeCfg()
        self.value = []
        self.lengthWord = 0
        self.condition = data.get('Condition')  # used in template
        self.defineProhibited = False  # Used to prevent using DEFINE once ifdef is called

    def validateRuntimeBases(self):
        """Reject reserved indices in BaseAddr and invalid runtime Alias entries."""
        reserved = {idx for name, idx in self.runtimeBases.items()
                    if isRuntimeBaseActive(name, self.alias, self.runtimeBases)}
        for addr, idx in self.baseAddrConfig['BaseAddr'].items():
            if addr == 0xFFFFFFFF:
                continue
            if idx in reserved:
                raise Exception('BaseAddr {} is assigned reserved RuntimeBases index {}. '
                                'Do not put runtime sentinels in BaseAddr; opt in with '
                                '"<alias>: {}" under Alias.'.format(hex(addr), idx, RUNTIME_ALIAS_VALUE))

        for name, target in (self.alias or {}).items():
            if name in self.runtimeBases:
                if target != RUNTIME_ALIAS_VALUE:
                    raise Exception('Alias {} must be set to "{}", not {}. Runtime bases are not '
                                    'addresses and must not appear in BaseAddr.'.format(
                                        name, RUNTIME_ALIAS_VALUE, target))
            elif target == RUNTIME_ALIAS_VALUE:
                raise Exception('Alias {} is marked {} but is not a known RuntimeBases entry '
                                '({}).'.format(name, RUNTIME_ALIAS_VALUE, ', '.join(self.runtimeBases)))

    @staticmethod
    def getConfig(baseDict, *argv, default=None):
        returnDict = baseDict
        for arg in argv:
            try:
                returnDict = returnDict[arg]
            except KeyError:
                return default
        return returnDict

    def getBaseAddrConfigName(self):
        return self.data.get('BaseAddrCfg', {}).get('Name', 'BaseAddrCfgDefault')

    def getBaseAddrConfigAccess(self):
        return self.data.get('BaseAddrCfg', {}).get('Access', 'BaseAddrCfgDefault')

    def conditionLength(self, conditionValue):
        # Need to know how much bytes to jump over, then compute the length of the condition, in bytes
        length = 0
        for subcmd in conditionValue:
            for action, value in subcmd.items():
                if action in ['NOP', 'ENDSEQ']:
                    length += 4  # instruction, no data
                elif type(value) is list:
                    isDefine = False

                    # Check if there is a DEFINE hidden inside this list
                    for el in value:
                        subVal = next(iter(el.values()))
                        if type(subVal) is dict and subVal.get('DEFINE'):
                            # DEFINE found, then the sequence length is instruction + data
                            isDefine = True
                            break
                    if isDefine:
                        length += 8
                    else:
                        # Inside a single write, there can be multiple actions but they may not be all continuous !
                        # First, compute length as if there was no optimization
                        length += (4 + 4) * len(value)  # ( instruction + value ) * number of instruction in the action

                        # Get register addresses and store them in a list
                        optimized_write = []
                        optimized_write_cnt = 0 # in words
                        for instr in value:
                            for block_register in instr.keys():
                                if isRuntimeDest(block_register, self.alias, self.runtimeBases, self.name):
                                    # Runtime dests are not packed into continuous writes.
                                    continue
                                # Check if an alias is used
                                alias = self.alias.get(block_register, block_register)
                                # Store addr
                                address = Action.findRegAddress(self.rm, alias, '_S')

                                baseAddrDict = self.baseAddrConfig['BaseAddr']
                                try:
                                    regBaseId = baseAddrDict[int(address) & (0xFFFF << self.contWrPos)]
                                except KeyError:
                                    address = Action.findRegAddress(self.rm, block_register, '_NS')
                                    try:
                                        regBaseId = baseAddrDict[int(address) & (0xFFFF << self.contWrPos)]
                                    except:
                                        baseAddress = int(address) & (0xFFFF << self.contWrPos)
                                        # If we end up here, it means an address is hardcoded in the sequence !
                                        # In that case, the address must be changed within the sequence.
                                        raise KeyError('Base address {} missing for {} address '
                                                       'in seqacc_regbases.yml.'.format(hex(baseAddress), hex(address)))
                                optimized_write.append(address)

                        # Then, sort instructions by registers addresses
                        optimized_write = sorted(optimized_write)

                        # Check if there will be optimization and count the number of instruction saved if any
                        previous_addr = 0
                        for addr in optimized_write:
                            if (addr - previous_addr) == 4:
                                optimized_write_cnt += 1
                            previous_addr = addr

                        # remove optimized writes, converted to bytes
                        length -= optimized_write_cnt * 4
                else:
                    length += 8  # instruction + data
        return length

    def _encodeCfg(self):
        config = self.data.get('SequenceCfg', {})
        return (config.get('MOVSWAP', 0) << 14) | (config.get('DISABSRST', 0) << 13) | (config.get('HWSTTRIG', 0) << 10)\
                | (config.get('HWSTSEL', 0)) << 5 | self.contWrPos

    def getLength(self):
        length = 0
        for obj in self.value:
            if type(obj) is not str:
                length += obj.getLength()
        self.lengthWord = length
        return length

    def branch(self, instructionList):
        """
        Find and develop branch syntax in the sequence.
        This function translate an If/Else condition using SKIPCOND_ and JUMP instructions.
        """
        for index, command in enumerate(instructionList):
            try:
                if command.get('IF'):
                    # Recursive search for nested conditions
                    indexIf = index
                    indexEndIf = indexIf + 1
                    self.branch(command['IF'])

                    # IF
                    conditionInst = instructionList[indexIf]['IF'][0]
                    valueIf = instructionList[indexIf]['IF'][1:]
                    lenIf = self.conditionLength(valueIf)

                    # Replace If instruction by SKIPCOND instruction
                    # and extract values from If and copy them into the sequence
                    instructionList[indexIf] = conditionInst
                    for actionIf in valueIf:
                        instructionList.insert(indexEndIf, actionIf)
                        indexEndIf += 1

                    try:
                        if instructionList[indexEndIf].get('ELSE'):
                            valueElse = instructionList[indexEndIf]['ELSE']
                            self.branch(valueElse)
                            lenElse = self.conditionLength(valueElse)

                            # Add JUMP to reach end of Else condition
                            instructionList.insert(indexEndIf, {'JUMP': lenElse})
                            indexEndIf += 1
                            lenIf += 8  # Add JUMP instruction length

                            # ELSE
                            # Insert JUMP right after SKIPCOND instruction to reach end of If condition
                            instructionList.insert(indexIf + 1, {'JUMP': lenIf})
                            indexEndIf += 1
                            # Extract values from Else and copy them into the sequence
                            for actionElse in valueElse:
                                instructionList.insert(indexEndIf, actionElse)
                                indexEndIf += 1
                            # Finally, erase ELSE instruction now that its instructions have been copied
                            del instructionList[indexEndIf]
                        else:
                            # Insert JUMP right after SKIPCOND instruction to reach end of If condition
                            instructionList.insert(indexIf + 1, {'JUMP': lenIf})
                            indexEndIf += 1
                    except (IndexError, AttributeError):
                        # Ending up here means the IF condition has no ELSE AND there is no other instruction afterward,
                        # or, in the case of AttributeError, the instruction afterward is a string (probably #if/#endif).
                        # So we can just execute the "else" condition just above:
                        # Insert JUMP right after SKIPCOND instruction to reach end of If condition
                        instructionList.insert(indexIf + 1, {'JUMP': lenIf})
                        indexEndIf += 1
            except AttributeError:
                # If we end up here, it means we're trying to read a string as a dict (probably because of ifdef string)
                pass

    def getInstructionIndex(self, seqIdx):
        actionIdx = 0
        instIdx = 0
        lastAction = self.value[-1].inst
        for info in lastAction.values():
            for instruction in info:
                if type(instruction[1]) is dict:
                    define = instruction[1].get('DEFINE')
                    if define is not None:
                        assert self.defineProhibited is False, "DEFINE keyword in sequence {} is used after #ifdef or " \
                                                               "#if statement. Impossible to compute DEFINE value, " \
                                                               "please rearrange the sequence to avoid having DEFINE " \
                                                               "used after #ifdef or #if.".format(self.name)
                        define += ' ' + str(1 + seqIdx + actionIdx + instIdx)
                        # update value
                        instruction[1]['DEFINE'] = define
                instIdx += 1
            actionIdx += 1

    def sequenceHandler(self):
        seqBaseIdxCount = 0  # Holds the count of the sequences traversed
        self.branch(self.data['Sequence'])

        for seqInstr in self.data['Sequence']:
            if type(seqInstr) is dict:
                for command, value in seqInstr.items():
                    # Encode actions and macros
                    if command in MACROS:
                        value = self.getConfig(MACROS, command, 'arg', default=value)
                        isNotArg = self.getConfig(MACROS, command, 'isNotArg', default=False)
                        action = self.getConfig(MACROS, command, 'Action')
                    else:
                        action = command
                        isNotArg = False

                    # Store action into instruction list
                    self.value.append(Action(self.rm, action, value, self.name, self.baseAddrConfig, self.contWrPos,
                                             self.alias, self.runtimeBases, isNotArg=isNotArg))

                    # Manage defines and indexes
                    self.getInstructionIndex(seqBaseIdxCount)
                    seqBaseIdxCount = self.getLength()
            elif type(seqInstr) is str:
                # String detected: ifdef possible then following action cannot use DEFINE keyword
                if "#endif" in seqInstr:
                    self.defineProhibited = True
                self.value.append(seqInstr)
            else:
                print("Unknown type for class Action")
        self.getLength()


class Action(object):
    def __init__(self, rm, actionName, arg, seqName, baseAddrConfig, contWrPos, alias, runtimeBases, isNotArg=False):
        """
        :opCode: The action ID
        :data: A dictionary containing the offset and the value to write for each baseAddrId.
            The dict should be defined as:
                data = {baseAddrId: {offset: value, offset1: value, etc.}}
            offset: Register address LSB depending on CNTWRPOS in sequence YAML file.
            value: The argument unchanged (str or integer depending on the input)
        :inst: A dictionary containing the instruction and the list of continuous <data>
            The dict should be defined as:
                dict = {instruction: [self.data]}
        """
        self.rm = rm
        self.name = actionName
        self.seqName = seqName
        self.baseAddrDict = baseAddrConfig['BaseAddr']
        self.basePos = baseAddrConfig['BasePos']
        self.contWrPos = contWrPos
        self.alias = alias
        self.runtimeBases = runtimeBases
        # isContWrReserved indicates whether an opcode uses LENGTH field as an indicator for a special action
        # that is not compliant with continuous writes (such as DELAY, SKIPCOND, etc.)
        self._opCode, self.isContWrReserved = self.getOpCode()
        self.data = {}
        self.inst = {}
        self.isNotArg = isNotArg
        self.isSimpleArg = False
        self.addOffset = None
        self.wordOffset = 0
        self.lineNumber = 0
        self.encodeAction(arg)

    def getOpCode(self):
        WRITE = 0
        AND = 1 << 28
        XOR = 2 << 28
        OR = 3 << 28
        DELAY = 4 << 28
        DELAYABS = (4 << 28) | (1 << self.contWrPos)
        MOV = 5 << 28
        MOVNOINC = 6 << 28
        WAITFORREG_ALL = 7 << 28
        WAITFORREG_ANY = (7 << 28) | (1 << self.contWrPos)
        WAITFORREG_NEGALL = (7 << 28) | (2 << self.contWrPos)
        WAITFORREG_NEGANY = (7 << 28) | (3 << self.contWrPos)
        WAITFORREG_EQ0 = (7 << 28) | (4 << self.contWrPos)
        WAITFORREG_NEQ0 = (7 << 28) | (5 << self.contWrPos)
        WAITFORREG_EQ1 = (7 << 28) | (6 << self.contWrPos)
        WAITFORREG_NEQ1 = (7 << 28) | (7 << self.contWrPos)
        WAITFORSIG_ALL = 8 << 28
        WAITFORSIG_ANY = (8 << 28) | (1 << self.contWrPos)
        WAITFORSIG_NEGALL = (8 << 28) | (2 << self.contWrPos)
        WAITFORSIG_NEGANY = (8 << 28) | (3 << self.contWrPos)
        WAITFORSIG_EQ0 = (8 << 28) | (4 << self.contWrPos)
        WAITFORSIG_NEQ0 = (8 << 28) | (5 << self.contWrPos)
        WAITFORSIG_EQ1 = (8 << 28) | (6 << self.contWrPos)
        WAITFORSIG_NEQ1 = (8 << 28) | (7 << self.contWrPos)
        WRITENOINC = 9 << 28
        JUMP = 10 << 28
        JUMPABS = (10 << 28) | (1 << self.contWrPos)
        SETTRIG = 11 << 28
        SETTRIG_SET = (11 << 28) | (1 << self.contWrPos)
        SETTRIG_CLR = (11 << 28) | (2 << self.contWrPos)
        SETTRIG_TOG = (11 << 28) | (3 << self.contWrPos)
        MOVBLOCK = (12 << 28)
        SKIPCOND_REGALL = 13 << 28
        SKIPCOND_REGANY = (13 << 28) | (1 << self.contWrPos)
        SKIPCOND_REGNEGALL = (13 << 28) | (2 << self.contWrPos)
        SKIPCOND_REGNEGANY = (13 << 28) | (3 << self.contWrPos)
        SKIPCOND_REGEQ0 = (13 << 28) | (4 << self.contWrPos)
        SKIPCOND_REGNEQ0 = (13 << 28) | (5 << self.contWrPos)
        SKIPCOND_REGEQ1 = (13 << 28) | (6 << self.contWrPos)
        SKIPCOND_REGNEQ1 = (13 << 28) | (7 << self.contWrPos)
        SKIPCOND_SIGALL = (13 << 28) | (8 << self.contWrPos)
        SKIPCOND_SIGANY = (13 << 28) | (9 << self.contWrPos)
        SKIPCOND_SIGNEGALL = (13 << 28) | (10 << self.contWrPos)
        SKIPCOND_SIGNEGANY = (13 << 28) | (11 << self.contWrPos)
        SKIPCOND_SIGEQ0 = (13 << 28) | (12 << self.contWrPos)
        SKIPCOND_SIGNEQ0 = (13 << 28) | (13 << self.contWrPos)
        SKIPCOND_SIGEQ1 = (13 << 28) | (14 << self.contWrPos)
        SKIPCOND_SIGNEQ1 = (13 << 28) | (15 << self.contWrPos)
        NOP = 15 << 28
        ENDSEQ = 0xFFFFFFFF

        # Actions that do use continuous write field for such purpose and not as an action indicator (like difference
        # between DELAY and DELAYABS).
        if self.name in ['WRITE', 'AND', 'XOR', 'OR', 'MOV', 'MOVNOINC', 'WRITENOINC', 'MOVBLOCK']:
            isContWrReserved = False
        else:
            isContWrReserved = True

        return eval(self.name), isContWrReserved

    @staticmethod
    def parser(register):
        regInfo = re.split("[(_ )]+", register)
        regInfo = list(filter(lambda x: (x != '' and x != ' '), regInfo))  # remove empty str ''
        #      block,      reg
        return regInfo[0], regInfo[1]

    def getTrace(self, seqName, fileName, lineNumber, wordOffset):
        traceStr = ""
        opCode = self.name
        # compute target addr, first get the base address from the index

        for addr, data in self.inst.items():
            runtimeName = None
            if addr == 0xFFFFFFFF:
                baseAddr = 0
            else:
                baseAddrIdx = (addr & ((2**(28-self.basePos) - 1) << self.basePos)) >> self.basePos
                runtimeName = runtimeNameForIndex(self.runtimeBases, baseAddrIdx, self.alias)
                if runtimeName is not None:
                    baseAddr = 0
                else:
                    baseAddr = list(self.baseAddrDict.keys())[baseAddrIdx] if baseAddrIdx != 0xFFFFFFFF else 0

            if self.isSimpleArg:
                # This case handles a one line instruction, like JUMP, NOP, ENDSEQ etc.
                # Some have a single value (JUMP, JUMPABS) and some not (ENDSEQ)
                register = 0
                numData = 1 if data[0][1] is not None else 0
            else:
                numData = len(data)
                if runtimeName is not None:
                    register = runtimeName
                else:
                    firstAddr = baseAddr + data[0][0] # base address + offset
                    try:
                        # Retrieve the register name by its address and filter the _S and _NS in name
                        register = self.rm.addressToNames(firstAddr)[0].replace('.', '_')
                    except Exception as e:
                        if type(e).__name__ == 'RegMapAddressError':
                            # Otherwise just get the raw address
                            register = firstAddr

            # Print the start of the string except values that are dynamic
            traceStr += "{},{}.c,{},{},{},{},{}".format(seqName, fileName, lineNumber, wordOffset, opCode,
                                                        hex(register) if (type(register) is int and register != 0) else register,
                                                        numData)
            # Print values now according to their type
            for offset, val in data:
                if val is None:
                    break

                if type(val) is dict:
                    val = val['VALUE']

                traceStr = traceStr + "," + str(hex(val)) if type(val) is int else traceStr + "," + str(val)
            traceStr += ",\n"

            # Update word offset count
            wordOffset = wordOffset + 1 if data[0][1] is None else wordOffset + 1 + len(data)
            self.wordOffset = wordOffset

            # Update line number count
            lineNumber = lineNumber + len(data) if len(data) > 1 else lineNumber + 1
            self.lineNumber = lineNumber

        return traceStr

    def getWordOffset(self):
        return self.wordOffset

    def getLineNumber(self):
        return self.lineNumber

    def getLength(self):
        """ Return the number of words contained in the action """
        length = 0
        for inst, val in self.inst.items():
            # When there is no argument, val is None: remove those since they're not printed
            filteredVal = list(filter(lambda x: x[1] is not None, val))
            length += 1 + len(filteredVal)
        return length

    def reverseMask(self):
        for baseAddrId, value in self.data.items():
            for offset in value:
                if type(value[offset]) is dict:
                    valueToConvert = value[offset]['VALUE']
                else:
                    valueToConvert = value[offset]

                if type(valueToConvert) is str:
                    valueToConvert = '~(' + valueToConvert + ')'
                else:
                    valueToConvert = (~valueToConvert) & 0xFFFFFFFF

                if type(value[offset]) is dict:
                    value[offset]['VALUE'] = valueToConvert
                else:
                    value[offset] = valueToConvert

    @staticmethod
    def findRegAddress(rm, block_register, suffix):
        if type(block_register) is str:
            regInfo = re.split("[(_\-> )]+", block_register)
            regInfo = list(filter(lambda x: (x != '' and x != ' '), regInfo))  # remove empty str ''

            reg = regInfo[0]

            # Some register fields may have underscores in their names (e.g. BTCLMAC->ENC_CTRL).
            # The register field name is all the underscore-separated strings between
            # the register name and the register type ('SET', 'CLR', 'TGL').
            if regInfo[-1] in ('SET', 'CLR', 'TGL'):
                regField = '_'.join(regInfo[1:-1])
                offset = f"{regInfo[-1].lower()}Offset"
            else:
                regField = '_'.join(regInfo[1:])
                offset = None

            address = eval('rm.' + reg + suffix + '.' + regField + '.address')

            if offset is not None:
                address += eval('rm.' + reg + suffix + '.' + regField + '.' + offset)

            return address
        else:
            return block_register

    def findAlias(self, dest, val):
        alias = self.alias.get(dest, dest)
        if alias != dest:
            # Alias found
            try:
                offset = val.get('OFFSET')
                if offset is not None:
                    self.addOffset = '(' + str(offset) + ') & 0x{0:04X}'.format((2 ** self.contWrPos) - 1)
            except AttributeError:
                # No offset keyword found, let's assume it is wanted
                pass

        return alias

    def store(self, baseAddrId, offset, value):
        if type(value) is list:
            for i, el in enumerate(value):
                self.data.setdefault(baseAddrId, {})[offset + i * 4] = el
        else:
            if offset in self.data.setdefault(baseAddrId, {}):
                if self.name not in ['MOV', 'MOVBLOCK']:
                    if type(self.data[baseAddrId][offset]) is dict:
                        regValue = self.data[baseAddrId][offset]['VALUE']
                    else:
                        regValue = self.data[baseAddrId][offset]

                    if type(value) is dict:
                        value = value['VALUE']

                    if type(value) is str:
                        regValue += ' | ' + value
                    else:
                        regValue |= value

                    # store result
                    if type(self.data[baseAddrId][offset]) is dict:
                        self.data[baseAddrId][offset]['VALUE'] = regValue
                    else:
                        self.data[baseAddrId][offset] = regValue
                else:
                    if self.name == 'MOV':
                        self.data[baseAddrId][offset + 4 * len(self.data[baseAddrId])] = value
                    elif self.name == 'MOVBLOCK':
                        previous_offset = list(self.data[baseAddrId])[-1]
                        self.data[baseAddrId][previous_offset + 4 * self.data[baseAddrId][previous_offset]['LENGTH']] = value
                    else:
                        raise (AttributeError, "Undefined parameter {}".format(self.name))
            else:
                self.data[baseAddrId][offset] = value

    def encodeAction(self, arg):
        """
        For every action, get the register base address and offset and store it into data.
        Values defined at same offset are ORed
        """
        if type(arg) is list:
            for regInstr in arg:
                if type(regInstr) is dict:
                    for block_register, val in regInstr.items():
                        if isRuntimeDest(block_register, self.alias, self.runtimeBases, self.seqName):
                            offset = 0
                            if isinstance(val, dict) and val.get('OFFSET') is not None:
                                try:
                                    offset = int(val['OFFSET']) & ((1 << self.contWrPos) - 1)
                                except (TypeError, ValueError):
                                    # Non-integer OFFSET is applied at print time, matching other aliases.
                                    self.findAlias(block_register, val)
                                    offset = 0
                            self.store(self.runtimeBases[block_register], offset, val)
                            continue

                        # Get the destination address: it can either be a register or a direct address
                        block_register = self.findAlias(block_register, val)

                        # Extract correct base address from register. Try to get _S polarity then _NS polarity if
                        # the first one didn't succeed. This is done to allow agility between _S and _NS addresses and
                        # to avoid constantly changing base addresses between 0xA000 and 0xB000.
                        address = self.findRegAddress(self.rm, block_register, '_S')
                        try:
                            regBaseId = self.baseAddrDict[int(address) & (0xFFFF << self.contWrPos)]
                        except KeyError:
                            address = self.findRegAddress(self.rm, block_register, '_NS')
                            try:
                                regBaseId = self.baseAddrDict[int(address) & (0xFFFF << self.contWrPos)]
                            except:
                                baseAddress = int(address) & (0xFFFF << self.contWrPos)
                                # If we end up here, it means an address is hardcoded in the sequence !
                                # In that case, the address must be changed within the sequence.
                                raise KeyError('Base address {} missing for {} address '
                                               'in seqacc_regbases.yml.'.format(hex(baseAddress), hex(address)))

                        regOffset = address & ((2 ** self.contWrPos) - 1)

                        # Check offset width
                        if regOffset > (2 ** self.contWrPos) - 1:
                            raise Exception("Base address not found for register address 0x{0:08X}."
                                            " Check base addresses in seqacc_regbases.yml".format(address))

                        self.store(regBaseId, regOffset, val)
                else:
                    # Arg is a simple list
                    for i, val in enumerate(arg):
                        self.data.setdefault(0, {})[i*4] = val
        else:
            # Actions having no or one argument that is not a dict end here
            self.isSimpleArg = True  # Mostly used for correct printing
            self.data.setdefault(0xFFFFFFFF, {})[0] = arg

        if self.isNotArg is True:
            self.reverseMask()

        self._encode()

    def _encode(self):
        """
         Fill inst list that is used for printing actions with encoded instructions and values.
         Manages the continuous write operation.
         First sort registers in data by offset, then parse these offsets and verify if they are continuous.
         If not, break the data list into continuous sublists.
        """

        # First, sort instructions by registers offsets
        self.data = dict(sorted(self.data.items()))
        for baseAddrId in self.data.keys():
            self.data[baseAddrId] = dict(sorted(self.data[baseAddrId].items()))
            # Convert dict to list for simplicity in next step
            self.data[baseAddrId] = list(self.data[baseAddrId].items())

        # Then check if addresses are continuous
        maxContinuousWr = 2**(self.basePos - self.contWrPos) - 1
        for baseAddrId, value in self.data.items():
            prevOffset = 0xFFFFFFFF
            continuousIndex = 0
            for i, element in enumerate(value):
                offsetContinuous = value[continuousIndex][0]
                # Break the chain of action into subchains if either address are not continuous
                # or the limit of continuous writes has been reached
                offset = element[0]
                if ((offset - prevOffset) > 4 and prevOffset != 0xFFFFFFFF) or (i - continuousIndex) == maxContinuousWr:
                    try:
                        continuousWrField = value[continuousIndex][1]['LENGTH']
                    except (TypeError, KeyError):
                        continuousWrField = i - continuousIndex

                    instruction = (self._opCode
                                   | (baseAddrId << self.basePos)
                                   | (continuousWrField << self.contWrPos)
                                   | offsetContinuous)
                    self.inst[instruction] = [el for el in value[continuousIndex:i]]
                    continuousIndex = i
                prevOffset = offset

            if not self.isContWrReserved:
                try:
                    length = value[continuousIndex][1]['LENGTH']
                except (TypeError, KeyError):
                    length = len(value) - continuousIndex

                continuousWrField = length
                offset = value[continuousIndex][0]
            else:
                baseAddrId = 0 if baseAddrId == 0xFFFFFFFF else baseAddrId
                continuousWrField = 0
                offset = prevOffset

            # Store last element
            instruction = self._opCode | (baseAddrId << self.basePos) | (continuousWrField << self.contWrPos) | offset
            self.inst[instruction] = [el for el in value[continuousIndex:]]


class SequenceGenerator(object):
    REG_TYPE_OFFSETS = {'': 0x0000,
                        '_SET': 0x1000,
                        '_CLR': 0x2000,
                        '_TGL': 0x4000 }

    def __init__(self, outputDir, regbaseOutputDir, fileName, rm, template=None):
        self.rm = rm
        self.source = open(os.path.join(outputDir, fileName + '.c'), "w+", newline='\n')
        self.header = open(os.path.join(outputDir, fileName + '.h'), "w+", newline='\n')
        self.env = Environment(loader=FileSystemLoader(TEMPLATE_PATH), keep_trailing_newline=True)
        print("Output code files: {}, \n{}".format(os.path.join(outputDir, fileName + ".c"),
                                                   os.path.join(outputDir, fileName + ".h")))

        self.env.filters['printStatement'] = self.printStatement
        self.env.filters['printHeader'] = self.printHeader
        self.env.filters['printBaseAddr'] = self.printBaseAddr
        self.env.filters['getAccess'] = self.getAccess

        self.regBaseSource = open(os.path.join(regbaseOutputDir, 'seqacc_regbases.c'), "w+", newline='\n')
        self.regBaseHeader = open(os.path.join(regbaseOutputDir, 'seqacc_regbases.h'), "w+", newline='\n')
        self.template_regbases_src = self.env.get_template("seqacc_regbases.c.j2")
        self.template_regbases_hdr = self.env.get_template("seqacc_regbases.h.j2")

        if template:
            self.template_source = self.env.get_template("{}.c.j2".format(template))
            self.template_header = self.env.get_template("{}.h.j2".format(template))
        else:
            self.template_source = self.env.get_template("seqacc_default.c.j2")
            self.template_header = self.env.get_template("seqacc_default.h.j2")

    def render(self, context):
        source_output = self.template_source.render(context)
        header_output = self.template_header.render(context)
        regbases_src = self.template_regbases_src.render(context)
        regbases_hdr = self.template_regbases_hdr.render(context)

        self.source.write(source_output)
        self.header.write(header_output)
        self.regBaseSource.write(regbases_src)
        self.regBaseHeader.write(regbases_hdr)

        self.source.close()
        self.header.close()
        self.regBaseSource.close()
        self.regBaseHeader.close()

    def generateTrace(self, sequencesObj, outputDir, outputFile):
        self.source = {sequencesObj.fileName: {}}

        # 1) Get sequence name and their starting line in C file
        with open(os.path.join(outputDir, sequencesObj.fileName + ".c"), 'r') as cFile:
            for number, line in enumerate(cFile, 1):
                for sequence in sequencesObj.sequences.keys():
                    if 'uint32_t ' + sequence in line:
                        # Add 1 to the line number because the sequence starts at beginning of next line
                        self.source[sequencesObj.fileName][sequence] = number + 1
                    elif '#if' in line:
                        raise TypeError('#if/#ifdef detected in C file {}.c::{} while generating Trace file. '
                                        'Feature not available yet, sequences will still be generated but Trace '
                                        'file will not.'.format(sequencesObj.fileName, sequence))
                    else:
                        pass
        cFile.close()

        # 2) Now compute the word offset, the line number based on the optimized instructions and print them
        traceStr = ""
        for seqName, sequence in sequencesObj.sequences.items():
            lineNumber = self.source[sequencesObj.fileName][seqName]
            wordOffset = 0
            for actionObj in sequence.value:
                if type(actionObj) is not str:
                    traceStr += actionObj.getTrace(seqName, sequencesObj.fileName, lineNumber, wordOffset)
                    wordOffset = actionObj.getWordOffset()
                    lineNumber = actionObj.getLineNumber()
                else:
                    pass

        outputFile.write(traceStr)

    # Filter definition
    def printStatement(self, action):
        returnStr = ''
        if type(action) is Action:
            for instruction, value in action.inst.items():
                returnStr += '\n  '
                # Print instruction
                returnStr += '0x{0:08X}UL'.format(instruction)

                if action.addOffset is not None:
                    returnStr += ' | ({})'.format(action.addOffset)
                returnStr += ','

                # Print action
                returnStr += self.printAction(action, value[0][1])

                # Print comment
                returnStr += self.getAddrFromInst(action, instruction, value[0][1])

                # Print continuous write if any
                for offsetAndValue in value[1:]:
                    # print offset
                    returnStr += '\n  /*  0x{0:04X} */'.format(offsetAndValue[0])
                    # print value
                    returnStr += self.printAction(action, offsetAndValue[1])

        elif type(action) is str:
            # print ifdef
            returnStr += '\n  ' + action
        else:
            # Unknown type of printing
            pass

        return returnStr

    def getAddrFromInst(self, action, instruction, value):
        offsetMask = 2**action.contWrPos - 1
        offset = instruction & offsetMask
        baseAddrMask = 2**(28 - action.basePos) - 1  # 28: opCode bit position
        baseAddrId = (instruction >> action.basePos) & baseAddrMask

        if action.isSimpleArg:
            return " /* {} */".format(action.name)
        elif action.addOffset != None:
            return " /* {} at {}, regBaseIdx: {} */".format(action.name, value['OFFSET'], baseAddrId)
        else:
            runtimeName = runtimeNameForIndex(action.runtimeBases, baseAddrId, action.alias)
            if runtimeName is not None:
                return " /* {} at {}, regBaseIdx: {} */".format(action.name, runtimeName, baseAddrId)
            baseAddr = [addr for addr, Id in action.baseAddrDict.items() if Id == baseAddrId][0]
            regAddr = baseAddr + offset
            for regType, regOffset in self.REG_TYPE_OFFSETS.items():
                baseRegAddr = regAddr & ~regOffset
                try:
                    register = self.rm.addressToNames(baseRegAddr)[0] + regType
                except Exception as e:
                    if type(e).__name__ == 'RegMapAddressError':
                        pass
                else:
                    strPattern = " /* {} at {}, regBaseIdx: {} */".format(action.name, register, baseAddrId)
                    break
            else:
                register = regAddr
                strPattern = " /* {0} at 0x{1:08X}, regBaseIdx: {2} */".format(action.name, register, baseAddrId)
            return strPattern

    @staticmethod
    def printAction(action, value):
        if type(value) is dict:
            value = value['VALUE']

        if type(value) is int:
            return " 0x{0:08X}UL,".format(value)
        elif value is not None:
            printValue = ' {},'.format(value)
            if action.isSimpleArg and action.name == 'JUMPABS':
                printValue = " (uint32_t)" + printValue
            return printValue
        else:
            return ''

    @staticmethod
    def printHeader(action):
        returnStr = ''
        if type(action) is Action:
            for baseAddressId, data in action.data.items():
                for offsetValue in data:
                    if type(offsetValue[1]) is dict:
                        if 'COMMENT' in offsetValue[1]:
                            returnStr += '// {}\n'.format(offsetValue[1]['COMMENT'])

                        if 'DEFINE' in offsetValue[1]:
                            returnStr += '#define {}\n'.format(offsetValue[1]['DEFINE'].upper())
        else:
            pass
        return returnStr

    @staticmethod
    def printBaseAddr(baseAddr):
        return '0x{0:04X}U'.format(baseAddr >> 16)

    @staticmethod
    def getAccess(sequence_obj):
        return sequence_obj.getBaseAddrConfigAccess()

def main(argv=None):
    """
        For more details on how to use this tool, please read the documentation
        at https://confluence.silabs.com/display/RFSW/Sequence+Compiler
        """

    parser = argparse.ArgumentParser(description='Run SEQACC sequence compiler')
    parser.add_argument('--source',
                        help='YAML source script to process. Usually stored in rail_lib/chip/efr32/seq_s3/.',
                        type=str,
                        required=True)
    parser.add_argument('--destination',
                        help='Destination folder for the output C header file. Default points to same source directory',
                        type=str)
    parser.add_argument('--regbase',
                        help='YAML regBases source to process.'
                             'Default is seqacc_regbases.yml located alongside the compiler.',
                        type=str,
                        default=os.path.join(os.path.dirname(os.path.realpath(__file__)), 'seqacc_regbases.yml'))
    parser.add_argument('--regbase_output',
                        help='YAML regBases output location.'
                             'Default points to same source directory.',
                        type=str)
    parser.add_argument('--part',
                        help='The part\'s code name that will execute the sequence.',
                        type=str,
                        default='rainier')
    parser.add_argument('--template',
                        help='Override default template file',
                        type=str,
                        required=False)
    parser.add_argument('--trace',
                        help='Output the Trace file',
                        action='store_true')
    args = parser.parse_args(argv)

    if args.destination is None:
        destination = os.path.dirname(args.source)
    else:
        destination = args.destination
    if not os.path.exists(destination):
        os.makedirs(destination)

    if args.regbase_output is None:
        regbaseOutput = destination
    else:
        regbaseOutput = args.regbase_output

    print('YAML regBase source path: {}'.format(args.regbase))
    ymlRegBase = yaml.safe_load(open(args.regbase))

    sources = []
    if os.path.isdir(args.source):
        for file in os.listdir(args.source):
            if file.endswith('.yml'):
                sources.append(file)
    else:
        sources.append(args.source)

    if args.trace:
        traceFileName = os.path.join(destination, "sequences.trace")
        traceFile = open(traceFileName, 'w+')

    for source in sources:
        print('YAML sequence source path: {}'.format(os.path.join(os.path.dirname(args.source), source)))
        ymlData = yaml.safe_load(open(os.path.join(os.path.dirname(args.source), source)))

        sequences = CompilerContext(args.part, ymlRegBase, ymlData)
        try:
            sequences.handler()
        except KeyError as ex:
            if os.path.isdir(args.source) and 'did not opt in' in str(ex):
                print('Skipping {}: {}'.format(source, ex))
                continue
            raise

        # Generate C files
        generator = SequenceGenerator(destination, regbaseOutput, sequences.fileName, sequences.rm)
        generator.render(sequences.getRender())

        if args.trace:
            # Generate Trace file
            try:
                generator.generateTrace(sequences, destination, traceFile)
            except Exception as e:
                traceFile.close()
                os.remove(traceFileName)
                args.trace = False
                print(e)
                pass

    if args.trace:
        traceFile.close()

if __name__ == "__main__":
    main()

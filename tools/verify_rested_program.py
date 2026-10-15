"""Check a privately exported authored Rested program, without distributing it.

This checks the parser's straight-line operand model, not native execution.
"""
import json
import sys
from pathlib import Path

ARITIES = {3210400059: 1, 3677738953: 3, 1743706366: 4,
           1408761274: 4, 3247530919: 4, 2038663391: 7, 3826668199: 7}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def verify(program):
    layout = program['dataLayout']
    require(len(layout) == 69, 'expected 69 layout entries')
    intervals = sorted((x['offsetInBytes'], x['offsetInBytes'] + x['size']) for x in layout)
    require(all(0 <= start < end <= len(program['data']) for start, end in intervals), 'data bounds')
    require(all(a[1] <= b[0] for a, b in zip(intervals, intervals[1:])), 'overlapping storage')
    require(len({x['name']['value'] for x in layout}) == len(layout), 'duplicate variable identity')
    pairs = []
    high_water = 0
    for section, effect in [('code', 2038663391), ('codeShutdown', 3826668199)]:
        words = program[section]
        pc = 0
        operands = []
        pending_result = False
        halted = False
        effects = {}
        while pc < len(words):
            op = words[pc]
            pc += 1
            require(not halted or op == 30, 'instructions after halt')
            if op == 11:
                require(not pending_result and pc < len(words), 'invalid operand boundary')
                word = words[pc]
                pc += 1
                require(word >> 16 == 0xffff and (word & 0xffff) < len(layout), 'layout index')
                operands.append(word & 0xffff)
            elif op == 12:
                require(not pending_result, 'unconsumed call result')
                operands.append(None)
            elif op == 19:
                require(not pending_result and pc + 1 < len(words), 'invalid call boundary')
                identity, function = words[pc:pc + 2]
                pc += 2
                require(function in ARITIES and len(operands) == ARITIES[function], 'call arity')
                if function == effect:
                    require(identity not in effects, 'duplicate effect identity')
                    effects[identity] = tuple(operands)
                elif section == 'codeShutdown' or function == 3826668199:
                    raise ValueError('unexpected effect phase')
                high_water = max(high_water, len(operands))
                operands = []
                pending_result = True
            elif op == 27:
                require(pending_result and not operands, 'pop without call result')
                pending_result = False
            elif op == 30:
                require(not operands and not pending_result, 'nonempty stack at halt')
                halted = True
            else:
                raise ValueError('unsupported control flow')
        require(halted, 'missing halt')
        pairs.append(effects)
    require(len(pairs[0]) == 11 and pairs[0] == pairs[1], 'apply/revert pairing')
    # Each effect owns distinct stored value/sign slots, shared entity inputs are allowed.
    storage = [slot for args in pairs[0].values() for slot in args[-2:]]
    require(None not in storage and len(storage) == len(set(storage)), 'effect storage alias')
    require(high_water * 8 <= program['stackSize'], 'operand stack capacity')
    return {'paired_effects': len(pairs[0]), 'layout_entries': len(layout), 'operand_high_water': high_water}


if __name__ == '__main__':
    print(json.dumps(verify(json.loads(Path(sys.argv[1]).read_text(encoding='utf-8')))))

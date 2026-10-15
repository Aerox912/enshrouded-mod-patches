import copy
import json
import os
import unittest
from pathlib import Path
from tools.verify_rested_program import verify


@unittest.skipUnless(os.environ.get('ESM_RESTED_PROGRAM'), 'private authored program required')
class RestedProgramTests(unittest.TestCase):
    def setUp(self):
        self.program = json.loads(Path(os.environ['ESM_RESTED_PROGRAM']).read_text(encoding='utf-8'))

    def test_authored_program(self):
        self.assertEqual(verify(self.program), {'paired_effects': 11, 'layout_entries': 69, 'operand_high_water': 7})

    def test_mutations_are_rejected(self):
        for kind in ['overlap', 'bounds', 'branch', 'revert', 'stack', 'identity', 'arity']:
            p = copy.deepcopy(self.program)
            if kind == 'overlap': p['dataLayout'][1]['offsetInBytes'] = p['dataLayout'][0]['offsetInBytes']
            elif kind == 'bounds': p['dataLayout'][0]['offsetInBytes'] = len(p['data'])
            elif kind == 'branch': p['code'][0] = 1
            elif kind == 'revert': p['codeShutdown'][1] ^= 1
            elif kind == 'stack': p['stackSize'] = 1
            elif kind == 'identity': p['dataLayout'][1]['name'] = p['dataLayout'][0]['name']
            elif kind == 'arity': p['code'].insert(0, 12)
            with self.subTest(kind=kind), self.assertRaises(ValueError):
                verify(p)

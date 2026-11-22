# SPDX-License-Identifier: GPL-2.0-or-later
from contextlib import redirect_stderr
import io
import unittest
from unittest.mock import patch

import check_hook_sites as hooks


class HookSiteChecks(unittest.TestCase):
    def test_original_kakariko_collision(self):
        original_source = hooks.source

        def old_capture(path):
            text = original_source(path)
            if path.name == 'area_load_trace.h':
                self.assertIn('0x4352f8', text)
                return text.replace('0x4352f8', '0x4352f4')
            return text

        with patch.object(hooks, 'source', old_capture):
            diagnostic = io.StringIO()
            with redirect_stderr(diagnostic):
                self.assertEqual(hooks.main(['--mode', 'dev']), 1)
            player = hooks.collisions(hooks.audit(hooks.ROOT, 'player'))
        self.assertIn('0x004352f4', diagnostic.getvalue())
        self.assertIn('Randomizer::PlayInit', diagnostic.getvalue())
        self.assertIn('AreaLoadTrace::Sites', diagnostic.getvalue())
        self.assertEqual(player, {})

    def test_all_instruction_words_in_guest_patch(self):
        guest = {'patches': [{'address': 0x100000, 'name': 'BranchPair', 'data': 'AAAAAAAAAAA='}]}
        words = dict(hooks.randomizer_patches('if (patch.address < 0x4cc000)', guest))
        self.assertEqual(words, {0x100000: 'BranchPair', 0x100004: 'BranchPair'})

    def test_merchant_requires_exact_owners_and_dispatch_order(self):
        pc = 0x14eda8
        owners = ['Randomizer::BeanDaddyPurchase', 'Prices::Sites[68]']
        self.assertFalse(hooks.collisions({pc: owners}))
        for conflict in (owners[::-1], owners + ['AreaLoadTrace'], [owners[0], owners[0]]):
            self.assertEqual(hooks.collisions({pc: conflict}), {pc: conflict})
        self.assertTrue(hooks.collisions({pc + 4: owners}))

    def test_unknown_address_selector_fails(self):
        header = 'inline U32 Instruction(U32 pc,U32 word) {\n return patch_table.at(pc);\n}'
        with self.assertRaisesRegex(ValueError, 'address selector'):
            list(hooks.instruction_sites('NewHook', None, {'NewHook': header}, {}, {}))

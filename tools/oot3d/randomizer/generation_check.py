#!/usr/bin/env python3
"""Check real generator output for shop/token scopes, prices and reproducibility."""
import base64
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

helper = Path(sys.argv[1]).resolve()
metadata = json.loads((Path(__file__).resolve().parents[3] /
                       'src/oot3d/randomizer/assets/guest.json').read_text())
message_base = metadata['symbols']['rCustomMessages']['address']


def check(package):
    config = package['config']
    fields = package['fields']
    prices = struct.unpack('<32i', base64.b64decode(fields['rShopsanityPrices']))
    shops = config['shopsanity']
    if shops < 2:
        assert prices == (0,) * 32
    else:
        for shop in range(8):
            count = sum(p >= 0 for p in prices[shop*4:shop*4+4])
            assert 1 <= count <= 4 if shops == 6 else count == shops - 1
        cap = (295, 10, 99, 200, 500, 999)[config['shop_prices']]
        for price in prices:
            assert -1 <= price <= cap
            if price >= 0 and config['shop_prices'] == 1:
                assert price == 10
        entries = base64.b64decode(package['message_entries'])
        messages = base64.b64decode(package['message_data'])
        by_id = {struct.unpack_from('<I', entries, at)[0]: at for at in range(0, len(entries), 96)}
        difficulty = package['difficulty']
        factor = difficulty['price_multiplier'] if difficulty['prices'] else 1
        for slot, price in enumerate(prices):
            if price < 0:
                continue  # Unused slot: the normal shop description is used.
            cost = min(500, price * factor) if factor > 1 else price
            for text_id in (0x9200 + 2*slot, 0x9201 + 2*slot):
                for language in (1, 5, 7):  # English, French, Spanish.
                    offset, length = struct.unpack_from('<II', entries, by_id[text_id]+16+language*8)
                    text = messages[offset-message_base:offset-message_base+length]
                    assert length and str(cost).encode() in text, (text_id, language, cost, text)
    tokens = config['tokensanity']
    skulls = [(key, value & 65535) for key, value in
              struct.iter_unpack('<II', base64.b64decode(fields['rItemOverrides']))
              if (key >> 16) & 255 == 3]
    shuffled = [(key, item) for key, item in skulls if item != 0x5b]
    if tokens == 0:
        assert not shuffled
    else:
        assert shuffled
        if tokens == 1:
            assert all(((key >> 8) & 255) < 10 for key, _ in shuffled)
        elif tokens == 2:
            assert all(((key >> 8) & 255) >= 10 for key, _ in shuffled)
        else:
            assert len(skulls) == 100


with tempfile.TemporaryDirectory(prefix='oot-rando-generation-') as temp:
    temp = Path(temp)
    cases = [({'shopsanity': n, 'tokensanity': n % 4}, 'original') for n in range(7)]
    cases += [({'shopsanity': 5, 'shop_prices': price, 'tokensanity': 3}, 'mq') for price in range(6)]
    # Retain the other difficulty defaults while enabling the 2x/3x price rule.
    default_difficulty = None
    for index, (options, mode) in enumerate(cases + [({}, 'original'), ({}, 'mq')]):
        if index >= len(cases):
            factor = index-len(cases)+2
            options = {'shopsanity': 5, 'shop_prices': 5, 'tokensanity': 3,
                       'difficulty': dict(default_difficulty, prices=True, price_multiplier=factor)}
        config = {'enabled': True, 'seed': 'shop-token-check', **options}
        input_path, output_path = temp/'config.json', temp/'seed.json'
        input_path.write_text(json.dumps(config))
        subprocess.run([helper, input_path, mode, output_path], check=True, capture_output=True)
        output = output_path.read_bytes()
        package = json.loads(output)
        default_difficulty = package['difficulty']
        check(package)
        if index in (0, 7):  # Replay one Original and one Master Quest report.
            report = temp/'last-generation.json'
            report.write_text(json.dumps(dict(config, difficulty=package['difficulty'], mq=mode == 'mq')))
            subprocess.run([helper, '--replay', report, output_path], check=True, capture_output=True)
            assert output_path.read_bytes() == output
        print(f'Passed generation case {index+1}: {mode}, shops={package["config"]["shopsanity"]}, '
              f'tokens={package["config"]["tokensanity"]}', flush=True)
print('Shop/token generation, translated prices, difficulty prices and reproducibility passed')

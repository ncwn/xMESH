"""
 Author: Raunak Mukhia @rmukhia
 Date:   26/01/22

 File:  generate_hazemon_type.py
 Descr: Generate can5_hazemon_types.h/c from json data downloaded form hazemon site.
"""

import json
from string import Template

TAB = '    '
db_data = []
timestamp_id = 234  # change this to be more than the largest value in id in the json file.


def conv_tok(token: str) -> str:
    return token.upper().replace('.', '_').replace(' ', '_')


def get_attr(elem: dict) -> list:
    return elem['id'], elem['datatype'], conv_tok(elem['notes']), elem['multiplier']


'''
    Header Section
'''


def get_enum() -> str:
    content = ''
    for elem in db_data:
        id, _, token, _ = get_attr(elem)
        content += TAB + ('HAZEMON_' + token).ljust(24) + ' = ' + str(id).ljust(2) + ',\n'
    content += TAB + 'HAZEMON_TIMESTAMP'.ljust(24) + ' = ' + str(timestamp_id).ljust(2) + ',\n'

    return content


def gen_header():
    with open("templates/can5_hazemon_types.h.tpl") as f:
        header_tpl_txt = f.read()
        header_tpl = Template(header_tpl_txt)

    hazemon_type_enum = get_enum()

    header_h = header_tpl.substitute(hazemon_type_enum=hazemon_type_enum)

    header_path = '../../components/can5_net/can5_protocols/can5_hazemon/can5_hazemon_types.h'
    with open(header_path, 'w') as f:
        f.write(header_h)
        print(header_path, 'written.')


'''
    Code Section
'''


def get_hazemon_type_token() -> str:
    content = ''
    for elem in db_data:
        id, datatype, token, multiplier = get_attr(elem)
        content += TAB + '{ "' + (token + '"').ljust(24) + ', ' + ('HAZEMON_' + token).ljust(24) + ', "' + (
                datatype + '"').ljust(
            8) + ', ' + str(
            multiplier).ljust(8) + '},\n'
    content += TAB + '{ "' + 'TIMESTAMP"'.ljust(24) + ', ' + 'HAZEMON_TIMESTAMP'.ljust(24) + ', "' + 'Q"'.ljust(
        8) + ', ' + str(
        1).ljust(8) + '},\n'
    return content


def get_tag_tab() -> str:
    content = ''
    for elem in db_data:
        _, _, token, _ = get_attr(elem)
        content += TAB + 'TAG_TAB_ITEM(' + ('HAZEMON_' + token).ljust(24) + '),\n'
    content += TAB + 'TAG_TAB_ITEM(' + 'HAZEMON_TIMESTAMP'.ljust(24) + '),\n'
    return content


def gen_code():
    with open("templates/can5_hazemon_types.c.tpl") as f:
        code_tpl_txt = f.read()
        code_tpl = Template(code_tpl_txt)

    hazemon_type_tokens = get_hazemon_type_token()
    tag_tab = get_tag_tab()

    code_c = code_tpl.substitute(hazemon_type_tokens=hazemon_type_tokens, tag_tab=tag_tab)

    code_path = '../../components/can5_net/can5_protocols/can5_hazemon/can5_hazemon_types.c'
    with open(code_path, 'w') as f:
        f.write(code_c)
        print(code_path, 'written.')


if __name__ == '__main__':
    with open("unified_type.json") as f:
        db_data = json.load(f)

    gen_header()

    gen_code()

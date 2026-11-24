"""Inspect the public operator timetable. Source is the AREX official download."""
import argparse
from pathlib import Path
import json
import zipfile
import xml.etree.ElementTree as ET

NS = {'s': 'http://schemas.openxmlformats.org/spreadsheetml/2006/main'}


def sheets(path):
    with zipfile.ZipFile(path) as archive:
        shared = [str(''.join(node.itertext())) for node in
                  ET.fromstring(archive.read('xl/sharedStrings.xml')).findall('s:si', NS)]
        workbook = ET.fromstring(archive.read('xl/workbook.xml'))
        names = [sheet.attrib['name'] for sheet in workbook.findall('s:sheets/s:sheet', NS)]
        for number, name in enumerate(names, 1):
            xml = ET.fromstring(archive.read(f'xl/worksheets/sheet{number}.xml'))
            rows = []
            for row in xml.findall('s:sheetData/s:row', NS):
                cells = {}
                for cell in row.findall('s:c', NS):
                    value = cell.find('s:v', NS)
                    if value is not None:
                        text = value.text or ''
                        if cell.attrib.get('t') == 's': text = shared[int(text)]
                        cells[cell.attrib['r']] = text
                if cells: rows.append(cells)
            yield name, rows


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("workbook", type=Path)
    for name, rows in sheets(parser.parse_args().workbook):
        print(json.dumps({'sheet': name, 'rows': [{k: v for k, v in list(row.items())[:16]} for row in rows[:22]]}, ensure_ascii=True))

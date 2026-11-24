"""Build the station picker from Seoul's official realtime API station workbook."""
import argparse
import gzip, json, zipfile, re, xml.etree.ElementTree as E
from pathlib import Path
root=Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('stations', type=Path, help='Seoul OA-12764 XLSX download')
parser.add_argument('arex_page', type=Path, help='Saved operator station page HTML')
parser.add_argument('--date', required=True, help='Publication date of the station spreadsheet')
parser.add_argument('--output', type=Path, default=root/'data/metro-network.json.gz')
args = parser.parse_args()
n={'s':'http://schemas.openxmlformats.org/spreadsheetml/2006/main'}
with zipfile.ZipFile(args.stations) as z:
    shared=[''.join(t.itertext()) for t in E.fromstring(z.read('xl/sharedStrings.xml')).findall('s:si',n)]
    rows=[]
    for row in E.fromstring(z.read('xl/worksheets/sheet1.xml')).findall('s:sheetData/s:row',n)[1:]:
        cells=[]
        for c in row:
            v=c.find('s:v',n)
            if v is not None: cells.append(shared[int(v.text)] if c.get('t')=='s' else v.text)
        if len(cells)>=4: rows.append(cells[:4])
lines={}
for line,sid,name,label in rows:
    lines.setdefault(line,{'id':line,'name':label,'stations':[]})['stations'].append([sid,name])
options=re.findall(r'<option value="(\d{3})">([^<]+)</option>',args.arex_page.read_text(encoding='utf-8'))
if not options:
    parser.error('No Airport Railroad station options were found in the supplied HTML')
arex=lines['1065']
arex['stations']=[[next((sid for sid,nm in arex['stations'] if nm==name or nm+'역'==name),code),name] for code,name in options]
arex['codes']=[code for code,name in options]
catalog={'date':args.date,'source':'https://data.seoul.go.kr/dataList/OA-12764/A/1/datasetView.do','lines':list(lines.values())}
raw=json.dumps(catalog,ensure_ascii=False,separators=(',',':')).encode()
args.output.write_bytes(gzip.compress(raw,mtime=0))
print(json.dumps({'lines':[(k,v['name'],len(v['stations'])) for k,v in lines.items()],'bytes':len(raw)},ensure_ascii=True))

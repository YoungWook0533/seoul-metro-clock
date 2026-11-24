const fs=require('fs'),z=require('zlib'),vm=require('vm'),assert=require('assert');
const network=JSON.parse(z.gunzipSync(fs.readFileSync('data/metro-network.json.gz')));
const source=fs.readFileSync('web/metro.html','utf8');
const code=source.slice(source.indexOf('const normalize='),source.indexOf('async function records('));
assert.equal(network.lines.length,19);
for(const line of network.lines){assert(line.stations.length>2);assert.equal(new Set(line.stations.map(s=>s[0])).size,line.stations.length)}
function check(lineId,r,next,prev){const context={line:network.lines.find(l=>l.id===lineId)};vm.createContext(context);vm.runInContext(code,context);context.r=r;const result=vm.runInContext('adjacency(r)',context);assert(result);const name=id=>context.line.stations.find(s=>s[0]===id)[1];assert.equal(name(result.next),next);assert.equal(name(result.prev),prev)}
check('1009',{updnLine:'하행',statnFid:'1009000906',statnTid:'1009000904',trainLineNm:'개화행 - 신방화방면'},'신방화','양천향교');
check('1009',{updnLine:'상행',statnFid:'1009000904',statnTid:'1009000906',trainLineNm:'중앙보훈병원행 - 양천향교방면 (급행)'},'양천향교','신방화');
check('1002',{updnLine:'외선',statnFid:'1002000202',statnTid:'1002000243',trainLineNm:'성수행 - 충정로방면'},'충정로','을지로입구');
check('1002',{updnLine:'내선',statnFid:'1002000243',statnTid:'1002000202',trainLineNm:'성수행 - 을지로입구방면'},'을지로입구','충정로');
console.log('PASS: official 19-line catalog; opposite directions and inner/outer-loop adjacency');

const locale=source.slice(source.indexOf("let language="),source.indexOf('async function initLanguage()'));
const languageContext=vm.createContext({});
vm.runInContext(locale,languageContext);
assert.equal(vm.runInContext("stationLabel('마곡나루')",languageContext),'마곡나루');
assert.equal(vm.runInContext("language='en'; lineLabel('공항철도','1065')",languageContext),'Airport Railroad');
assert.equal(vm.runInContext("stationLabel('마곡나루')",languageContext),'Magoknaru');
assert.equal(vm.runInContext("language='ko'; stationLabel('마곡나루')",languageContext),'마곡나루');
console.log('PASS: English and Korean station display strings');

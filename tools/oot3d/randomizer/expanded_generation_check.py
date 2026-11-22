# SPDX-License-Identifier: GPL-2.0-or-later
# Real upstream generator coverage; writes only to the supplied test directory.
import json,subprocess,pathlib,concurrent.futures,sys,base64,struct
root=pathlib.Path(__file__).resolve().parents[3]
exe=pathlib.Path(sys.argv[1]).resolve()
out=pathlib.Path(sys.argv[2]);out.mkdir(exist_ok=True)
checks={
'base':{},'mq':{'mode':'mq'},
'mq-start-stones':{'mode':'mq','advanced':dict(StartingKokiriEmerald=1,StartingGoronRuby=1,StartingZoraSapphire=1)},
'all-start-rewards':{'advanced':{'Starting'+reward:1 for reward in (
 'KokiriEmerald','GoronRuby','ZoraSapphire','ForestMedallion','FireMedallion',
 'WaterMedallion','SpiritMedallion','ShadowMedallion','LightMedallion')}},
'cosmetic':{'advanced':dict(ShuffleMusic=1,ShuffleSFX=1)},
'logic':{'advanced':dict(Logic=1,LogicGrottosWithoutAgony=1,RestoreISG=1,GlitchISG=1)},
'exclude-inventory':{'advanced':{'Exclude/292':1,'ShuffleKokiriSword':1,'StartingFireArrows':1,'StartingZeldasLullaby':1,'StartingKokiriEmerald':1,'HintSpecificity':1}},
'start-keys':{'maps':5,'small_keys':5,'boss_keys':5,'ganon_key':5,'wallet':3},
'entrances':{'advanced':dict(ShuffleEntrances=1,ShuffleDungeonEntrances=2,ShuffleBossEntrances=2,ShuffleOverworldEntrances=1,ShuffleInteriorEntrances=2,ShuffleGrottoEntrances=1,ShuffleOwlDrops=1,ShuffleWarpSongs=1,ShuffleOverworldSpawns=1)},
'mixed-entrances':{'advanced':dict(ShuffleEntrances=1,ShuffleDungeonEntrances=1,ShuffleInteriorEntrances=1,ShuffleGrottoEntrances=1,MixedEntrancePools=1,MixDungeons=1,MixInteriors=1,MixGrottos=1,DecoupleEntrances=1)},
'items':{'tokensanity':3,'shopsanity':6,'advanced':dict(Scrubsanity=3,ShuffleCows=1,ShuffleKokiriSword=1,ShuffleMasterSword=1,ShuffleOcarinas=1,ShuffleWeirdEgg=1,ShuffleZeldasLetter=1,ShuffleGerudoToken=1,ShuffleMagicBeans=1,ShuffleMerchants=2,ShuffleAdultTradeQuest=1,ShuffleChestMinigame=2,ShuffleFrogSongRupees=1,ShuffleOcarinaButtons=1,ShuffleRupees=1,ShuffleRecoveryHearts=1,ShuffleBigPoes=1)},
'keys':{'maps':4,'small_keys':4,'boss_keys':4,'ganon_key':4,'rewards':1,'songs':2,'advanced':dict(KeyRings=1,GerudoKeys=3)},
'goals':{'ganon_key':9,'advanced':dict(Bridge=2,BridgeStoneCount=2,GanonsTrialsCount=2,LACSStoneCount=2)},
'hunt':{'advanced':dict(TriforceHunt=1,TriforcePiecesTotal=29,TriforcePiecesRequired=19)},
'enemies':{'advanced':dict(Enemizer=1,ShuffleEnemySouls=1)},
'skulltulas':{'tokensanity':3,'advanced':dict(RandomGsLocations=1,GsLocGuaranteeNew=1)},
'audio':{'advanced':dict(ShuffleMusic=1,ShuffleSFX=1,RandomSongNotes=1)},
'multiplayer':{'advanced':dict(MP_Enabled=1,MP_SharedProgress=1,MP_SharedHealth=1,MP_SharedRupees=1,MP_SharedAmmo=1)},
'mq-extras':{'mode':'mq','maps':4,'small_keys':4,'boss_keys':4,'advanced':dict(ShuffleEntrances=1,ShuffleDungeonEntrances=1,ShuffleEnemySouls=2,ShuffleCows=1,Scrubsanity=1,ShuffleMusic=1)},
}
def run(pair):
 name,c=pair;c=c.copy();mode=c.pop('mode','original');c.update(enabled=True,seed='expanded-regression')
 p=out/(name+'-config.json');p.write_text(json.dumps(c))
 with (out/(name+'.log')).open('w') as log:
  try:r=subprocess.run([str(exe),str(p),mode,str(out/(name+'.json'))],stdout=log,stderr=subprocess.PIPE,timeout=240)
  except subprocess.TimeoutExpired as error:raise AssertionError(f'{name} timed out') from error
 if r.returncode:raise AssertionError((name,r.returncode,r.stderr.decode(errors='replace')[-1000:]))
 package=json.loads((out/(name+'.json')).read_bytes())
 assert package['format']==2
 lengths={'rEntranceOverrides':2590,'rEnemyOverrides':6400,'rGsLocOverrides':2800,'rBGMOverrides':512,'rSfxData':23352,'rScrubRandomItemPrices':22}
 for key,length in lengths.items():assert len(base64.b64decode(package['fields'][key]))==length,(name,key)
 for key in ('rEntranceOverrides','rEnemyOverrides','rGsLocOverrides'):
  if (name,key) in [('entrances','rEntranceOverrides'),('enemies','rEnemyOverrides'),('skulltulas','rGsLocOverrides')]:
   assert any(base64.b64decode(package['fields'][key])),(name,key)
 if name=='items':assert len(package['native_patches'])==1
 if name=='audio':
  assert len(package['native_patches'])==2
  music=struct.unpack('<128I',base64.b64decode(package['fields']['rBGMOverrides']))
  assert len(set(music[:85]))==85 and any(a>b for a,b in zip(music,music[1:]))
 if name=='exclude-inventory':
  assert package['config']['advanced']['Exclude/292']==1
 # These cases start with the first N rewards in the game's stone/medallion order.
 starting_counts={'base':0,'mq':0,'exclude-inventory':1,'mq-start-stones':3,'all-start-rewards':9}
 if name in starting_counts:
  count=starting_counts[name]
  rewards=struct.unpack('<9i',base64.b64decode(package['fields']['rDungeonRewardOverrides']))
  assert sorted(rewards)==[-1]*count+list(range(count,9)),(name,rewards)
  if count:
   assert rewards[-1]==-1,(name,"Link's Pocket",rewards[-1])
 return name,'passed' 
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
 for result in pool.map(run,checks.items()):print(result,flush=True)

# Cosmetic audio choices must not change gameplay placement or the shared seed hash.
base=json.loads((out/'base.json').read_bytes());cosmetic=json.loads((out/'cosmetic.json').read_bytes())
assert base['fields']['rItemOverrides']==cosmetic['fields']['rItemOverrides']
assert base['hash']==cosmetic['hash']
# Deterministic placements, prices, music, SFX, native tables and messages.
for name in ('items','entrances','audio','enemies'):
 previous=(out/(name+'.json')).read_bytes()
 run((name,checks[name]))
 assert previous==(out/(name+'.json')).read_bytes(),name
for name,mode,options,reason in [
 ('mixed-layout','original',{'MQDungeonCount':6},'Invalid randomizer option: MQDungeonCount'),
 ('unsupported-mq','mq',{'Enemizer':1},'Master Quest logic does not support Enemizer'),
 ('bad-triforce','original',{'TriforceHunt':1,'TriforcePiecesTotal':1,'TriforcePiecesRequired':10},'Triforce pieces required cannot exceed'),
 ('bad-option','original',{'Scrubsanity':99},'Invalid randomizer option: Scrubsanity'),
 ('bad-exclusion','original',{'Exclude/99999999':1},'Invalid randomizer option: Exclude/99999999')]:
 p=out/(name+'-config.json');p.write_text(json.dumps({'enabled':True,'seed':'reject','advanced':options}))
 r=subprocess.run([exe,p,mode,out/(name+'.json')],capture_output=True,timeout=30)
 assert r.returncode==1 and reason.encode() in r.stderr,(name,r.returncode,r.stderr[-1000:])
 print(name,'correctly rejected',flush=True)
print('Expanded generation, deterministic tables and settings validation passed',flush=True)

#!/usr/bin/env python3
"""Read-only ocarina capture audit against the user's extracted Link animation.
Usage: ocarina-capture-audit.py CAPTURE.json zelda_link_child_new.zar
Joint matching identifies the action independently of capture filenames; the
capture does not store action IDs. This is an approximate animation comparison,
not a visual smoothness test. No game assets are exported.
"""
import json,struct,math,itertools,sys,collections
from pathlib import Path
D=json.loads(Path(sys.argv[1]).read_text())
archive=Path(sys.argv[2]).read_bytes()
assert archive[:4]==b'ZAR\x01'
count=struct.unpack_from('<H',archive,10)[0]
files,offsets=struct.unpack_from('<II',archive,16)
b=None
for i in range(count):
 size,name_at=struct.unpack_from('<II',archive,files+8*i)
 name=archive[name_at:archive.index(0,name_at)].decode()
 if name=='child/anim/nml_okarina_start.csab':
  start=struct.unpack_from('<I',archive,offsets+4*i)[0]
  b=archive[start:start+size]
assert b and b[:4]==b'csab' and struct.unpack_from('<I',b,8)[0]==3
for key in ['poses','draws']:
 values=D[key]
 print(key,len(values),'frames',min(x['frame'] for x in values),max(x['frame'] for x in values))
frames=collections.defaultdict(list)
for pose in D['poses']:
 if pose['model']=='childlink_v2':frames[pose['frame']+1].append(pose)
counts=collections.Counter();fallback=collections.Counter()
for draw in D['draws']:
 if not draw['perspective']:continue
 for pose in frames[draw['frame']]:
  if all(all(abs(a-b)<(.03 if i%4==3 else 1e-5) for i,(a,b) in enumerate(zip(u,v)))
         for u,v in zip(pose['native'],draw['palette'])):
   counts[(draw['pose_midpoint'],draw['camera_midpoint'])]+=1
   if not draw['pose_midpoint']:fallback[draw['frame']]+=1
   break
print('Link draws (pose midpoint, camera midpoint):',dict(counts))
print('Native-pose fallback frames:',dict(fallback))
def mul(a,b):return [[sum(a[i][k]*b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]
def trans(a):return list(map(list,zip(*a)))
def rot(axis,t):
 c,s=math.cos(t),math.sin(t);return [[[1,0,0],[0,c,-s],[0,s,c]],[[c,0,s],[0,1,0],[-s,0,c]],[[c,-s,0],[s,c,0],[0,0,1]]][axis]
def track(at,t):
 typ,n,_,end=struct.unpack_from('<4I',b,at);assert typ==2
 ks=[struct.unpack_from('<Hhhh',b,at+16+i*8) for i in range(n)]
 if t<=ks[0][0]:return ks[0][1]*math.pi/32768
 if t>=ks[-1][0]:return ks[-1][1]*math.pi/32768
 for a,z in zip(ks,ks[1:]):
  if a[0]<=t<=z[0]:
   dt=z[0]-a[0];u=(t-a[0])/dt
   return ((2*u**3-3*u*u+1)*a[1]+(u**3-2*u*u+u)*a[3]*dt+(-2*u**3+3*u*u)*z[1]+(u**3-u*u)*z[2]*dt)*math.pi/32768
nodes={}
for i in range(21):
 at=struct.unpack_from('<I',b,0x6c+i*4)[0]+0x18
 bone=struct.unpack_from('<H',b,at+4)[0];offs=struct.unpack_from('<9H',b,at+8)
 if all(offs[3:6]):nodes[bone]=[at+x for x in offs[3:6]]
snapshots={p['skeleton_snapshot']:p['frame'] for p in D['poses']
           if p['model']=='childlink_v2' and p['skeleton_snapshot']>=0}
for snap,frame in snapshots.items():
 bones=D['skeleton_snapshots'][snap]['bones'];local={}
 for i in [10,11,14,15,16,18,19,20]:
  w=bones[i]['world'];p=bones[bones[i]['parent']]['world'];R=[[w[r*4+c]*100 for c in range(3)] for r in range(3)];P=[[p[r*4+c]*100 for c in range(3)] for r in range(3)];local[i]=mul(trans(P),R)
 best=[]
 for order in itertools.permutations(range(3)):
  for ti in range(111):
   t=ti/10;err=0
   for i,L in local.items():
    rs=[rot(j,track(nodes[i][j],t)) for j in range(3)];R=mul(mul(rs[order[0]],rs[order[1]]),rs[order[2]])
    err+=sum((L[r][c]-R[r][c])**2 for r in range(3) for c in range(3))
   best.append((err,t,order))
 print('frame',frame,'best start-animation fit (squared matrix error, animation frame, Euler order)',min(best))

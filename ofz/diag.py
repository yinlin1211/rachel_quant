import numpy as np, glob
BASE='/home/intern2/rachel/ofz'; DB='/opt/bar_data'
NDAY,NSTK=1211,5213
cl=np.fromfile(f'{DB}/DailyBar/clse.dat',dtype=np.float32).reshape(NDAY,NSTK).astype(float)
ad=np.fromfile(f'{DB}/DailyBar/adj.dat',dtype=np.float32).reshape(NDAY,NSTK).astype(float)
days=[l.strip()[:8] for l in open(f'{DB}/Meta/daily_time_list.txt')]
di_map={d:i for i,d in enumerate(days)}
cl[cl<=0]=np.nan; ad[~np.isfinite(ad)|(ad<=0)]=1.0

rows=[]
for f in sorted(x for x in glob.glob(f'{BASE}/raw_*.csv') if not x.endswith('raw_all.csv')):
    for ln in open(f):
        p=ln.rstrip().split(',')
        if len(p)>=15: rows.append(p[:15])
a=np.array(rows,dtype=np.float64)
d=a[:,0].astype(np.int64); c=a[:,1].astype(np.int64)
nB,nS,vB,vS=a[:,2],a[:,3],a[:,4],a[:,5]
tn,tv=nB+nS,vB+vS
keep=(tn>=50)&(tv>0)&(c<NSTK)
d,c,vB,vS,tn,tv=[x[keep] for x in (d,c,vB,vS,tn,tv)]
di=np.array([di_map.get(str(x),-1) for x in d])
ok=(di>=0)&(di+5<NDAY)
di,c,vB,vS,tn,tv=[x[ok] for x in (di,c,vB,vS,tn,tv)]
p0=cl[di,c]*ad[di,c]; r1=cl[di+1,c]*ad[di+1,c]/p0-1
F={'ofi_vol':(vB-vS)/tv,'ofi_cnt':(nB[keep][ok]-nS[keep][ok])/(nB[keep][ok]+nS[keep][ok])}

order=np.argsort(di,kind='mergesort'); di_s=di[order]
b=np.flatnonzero(np.r_[True,di_s[1:]!=di_s[:-1]]); se=list(zip(b,np.r_[b[1:],len(di_s)]))
def rk(x):
    o=np.argsort(x,kind='mergesort'); r=np.empty(len(x)); r[o]=np.arange(len(x)); return r/max(len(x)-1,1)
for name,x in F.items():
    x=x[order]; y=r1[order]
    ic,sp_m,sp_med,nhi=[],[],[],0
    for s,e in se:
        xs,ys=x[s:e],y[s:e]
        m=np.isfinite(xs)&np.isfinite(ys)
        if m.sum()<100: continue
        xr=rk(xs[m]); yr=rk(ys[m])
        ic.append(np.corrcoef(xr,yr)[0,1])
        hi=ys[m][xr>=0.8]; lo=ys[m][xr<0.2]
        sp_m.append(hi.mean()-lo.mean()); sp_med.append(np.median(hi)-np.median(lo))
        nhi+=1
    ic=np.array(ic); sp_m=np.array(sp_m)*1e4; sp_med=np.array(sp_med)*1e4
    print(f'{name}: 天 {len(ic)}  IC={ic.mean():.4f}  均值价差={sp_m.mean():.2f}bp  中位价差={sp_med.mean():.2f}bp  价差std={sp_m.std():.1f}bp')
    print('   前5天 (IC, 均值价差, 中位价差):',[(round(ic[i],4),round(sp_m[i],1),round(sp_med[i],1)) for i in range(5)])

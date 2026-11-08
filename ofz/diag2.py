"""对 8 个因子做：中位数分层、市值中性化后的 IC"""
import numpy as np, glob
BASE='/home/intern2/rachel/ofz'; DB='/opt/bar_data'
NDAY,NSTK=1211,5213
def ld(n,d=np.float32):
    return np.fromfile(f'{DB}/DailyBar/{n}.dat',dtype=d).reshape(NDAY,NSTK).astype(float)
cl,ad,cap=ld('clse'),ld('adj'),ld('CAP')
days=[l.strip()[:8] for l in open(f'{DB}/Meta/daily_time_list.txt')]
dmap={d:i for i,d in enumerate(days)}
cl[cl<=0]=np.nan; ad[~np.isfinite(ad)|(ad<=0)]=1.0; cap[cap<=0]=np.nan
tkr=[l.strip() for l in open(f'{DB}/Meta/tkr.txt')]
is_sh=np.array([t.endswith('.SH') for t in tkr],bool)

rows=[]
for f in sorted(x for x in glob.glob(f'{BASE}/raw_*.csv') if not x.endswith('raw_all.csv')):
    for ln in open(f):
        p=ln.rstrip().split(',')
        if len(p)>=15: rows.append(p[:15])
a=np.array(rows,dtype=np.float64); del rows
dd=a[:,0].astype(np.int64); c=a[:,1].astype(np.int64)
nB,nS,vB,vS,nD,nDB,nDS,vD,nBig,vBig,vLate,amtB,amtAll=a[:,2:].T
tn,tv=nB+nS,vB+vS
keep=(tn>=50)&(tv>0)&(c<NSTK)
dd,c,nB,nS,vB,vS,nD,nDB,nDS,vBig,vLate,amtB,amtAll,tn,tv=[x[keep] for x in (dd,c,nB,nS,vB,vS,nD,nDB,nDS,vBig,vLate,amtB,amtAll,tn,tv)]
di=np.array([dmap.get(str(x),-1) for x in dd]); ok=(di>=0)&(di+5<NDAY)
di,c,vB,vS,nD,nDB,nDS,vBig,vLate,amtB,amtAll,tn,tv,nB,nS=[x[ok] for x in (di,c,vB,vS,nD,nDB,nDS,vBig,vLate,amtB,amtAll,tn,tv,nB,nS)]
p0=cl[di,c]*ad[di,c]; r1=cl[di+1,c]*ad[di+1,c]/p0-1
mcap=cap[di,c]
sh=is_sh[c]
with np.errstate(divide='ignore',invalid='ignore'):
    F={'ofi_cnt':(nB-nS)/tn,'ofi_vol':(vB-vS)/tv,'avg_size':np.log1p(tv/tn),
       'big_ratio':vBig/tv,'late_ratio':vLate/tv,
       'px_pos':(amtB/vB)/(amtAll/tv)-1.0,
       'cancel_rate':np.where(sh,nD/(tn+nD),np.nan),
       'cancel_imb':np.where(sh,(nDB-nDS)/(nDB+nDS),np.nan)}
order=np.argsort(di,kind='mergesort'); di_s=di[order]
b=np.flatnonzero(np.r_[True,di_s[1:]!=di_s[:-1]]); se=list(zip(b,np.r_[b[1:],len(di_s)]))
def rk(x):
    o=np.argsort(x,kind='mergesort'); r=np.empty(len(x)); r[o]=np.arange(len(x)); return r/max(len(x)-1,1)
y=r1[order]; mc=np.log(mcap[order])
print(f"{'因子':<12}{'IC':>8}{'中性化IC':>9}{'均值价差':>9}{'中位价差':>9}{'中位Sharpe':>10}")
for name,x0 in F.items():
    x=x0[order]
    ic,nic,spm,spd=[],[],[],[]
    for s,e in se:
        xs,ys,ms=x[s:e],y[s:e],mc[s:e]
        m=np.isfinite(xs)&np.isfinite(ys)
        if m.sum()<100: continue
        xr=rk(xs[m]); yr=rk(ys[m])
        ic.append(np.corrcoef(xr,yr)[0,1])
        hi=ys[m][xr>=0.8]; lo=ys[m][xr<0.2]
        spm.append(hi.mean()-lo.mean()); spd.append(np.median(hi)-np.median(lo))
        mm=m&np.isfinite(ms)
        if mm.sum()>200:
            xx=rk(xs[mm]); cc=rk(ms[mm]); yy=rk(ys[mm])
            A=np.c_[np.ones_like(cc),cc]
            beta=np.linalg.lstsq(A,xx,rcond=None)[0]
            nic.append(np.corrcoef(xx-A@beta,yy)[0,1])
    ic=np.array(ic); spm=np.array(spm)*1e4; spd=np.array(spd)*1e4
    nics=np.array([v for v in nic if np.isfinite(v)])
    shd=spd.mean()/spd.std(ddof=1)*np.sqrt(244)
    print(f"{name:<12}{ic.mean():>8.4f}{nics.mean():>9.4f}{spm.mean():>9.2f}{spd.mean():>9.2f}{shd:>10.2f}")

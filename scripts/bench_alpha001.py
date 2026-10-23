import os
import sys
import time

sys.path.insert(0, '/home/intern2/pyTickAlpha')
from utils_alphas import cal_alpha_datas

date = sys.argv[1]
out = sys.argv[2]
os.makedirs(out, exist_ok=True)
t0 = time.perf_counter()
cal_alpha_datas('/opt/bar_data', date, out, (('aID_001', 'Alpha_001'),))
print(f'{date} elapsed={time.perf_counter() - t0:.1f}s', flush=True)

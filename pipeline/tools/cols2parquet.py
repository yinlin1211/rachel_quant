# -*- coding: utf-8 -*-
"""把 tick2cols 产出的 .cols 列文件打包成 Parquet（列式 + zstd）。

用法：python3 cols2parquet.py <day_dir> [--delete]
输出：<day_dir>/chunk-XX.parquet
"""
import sys, os, struct, glob, time
import numpy as np
import pyarrow as pa
import pyarrow.parquet as pq

DT = {0: np.int32, 1: np.uint32, 2: np.float32, 3: np.int8}


def read_cols(path):
    with open(path, 'rb') as f:
        assert f.read(5) == b'COLS1'
        f.read(1)
        nrec, ncol = struct.unpack('<ii', f.read(8))
        cols = []
        for _ in range(ncol):
            (ln,) = struct.unpack('<i', f.read(4))
            name = f.read(ln).decode()
            (dt,) = struct.unpack('<i', f.read(4))
            cols.append((name, dt))
        out = [(name, np.fromfile(f, dtype=DT[dt], count=nrec)) for name, dt in cols]
    return nrec, out


def main():
    d = sys.argv[1]
    delete = '--delete' in sys.argv
    files = sorted(glob.glob(os.path.join(d, 'chunk_*.cols')))
    raw = sum(os.path.getsize(p) for p in files)
    total = 0
    t0 = time.time()
    for p in files:
        nrec, cols = read_cols(p)
        total += nrec
        tbl = pa.Table.from_arrays([pa.array(v) for _, v in cols],
                                   names=[n for n, _ in cols])
        out = p.replace('.cols', '.parquet')
        pq.write_table(tbl, out, compression='zstd', compression_level=3,
                       use_dictionary=True)
        if delete:
            os.remove(p)
    parq = sum(os.path.getsize(p.replace('.cols', '.parquet')) for p in files)
    dt = time.time() - t0
    print(f"chunks={len(files)} records={total:,}")
    print(f"cols={raw/1e6:.1f} MB -> parquet={parq/1e6:.1f} MB "
          f"(ratio {raw/max(parq,1):.1f}x)  pack_time={dt:.1f}s")


if __name__ == '__main__':
    main()

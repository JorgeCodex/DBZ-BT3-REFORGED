import argparse, math, struct
from pathlib import Path
from convert_earth import geometry_chains, vertex_batches

def main():
    p=argparse.ArgumentParser(description='Build Adventure collision from the visible BT2 Earth geometry (group 2).')
    p.add_argument('map',type=Path); p.add_argument('output',type=Path); args=p.parse_args()
    raw=args.map.read_bytes(); start,end=struct.unpack_from('<II',raw,4); model=raw[start:end]
    triangles=[]
    for _,chain in geometry_chains(model,2):
        for off,n in vertex_batches(model,chain):
            vertices=[struct.unpack_from('<3f',model,off+i*48) for i in range(n)]
            for i in range(2,n):
                a,b,c=vertices[i-2:i+1]
                u=[b[j]-a[j] for j in range(3)]; v=[c[j]-a[j] for j in range(3)]
                cross=(u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
                if all(math.isfinite(t) for point in (a,b,c) for t in point) and sum(t*t for t in cross)>1e-6:
                    triangles.append((*a,*b,*c))
    if not triangles: raise ValueError('No collision triangles')
    size=250.0; x0=math.floor(min(min(t[0],t[3],t[6]) for t in triangles)/size)*size; z0=math.floor(min(min(t[2],t[5],t[8]) for t in triangles)/size)*size
    nx=math.floor((max(max(t[0],t[3],t[6]) for t in triangles)-x0)/size)+1; nz=math.floor((max(max(t[2],t[5],t[8]) for t in triangles)-z0)/size)+1
    cells=[[] for _ in range(nx*nz)]
    for i,t in enumerate(triangles):
        xa=int((min(t[0],t[3],t[6])-x0)//size); xb=int((max(t[0],t[3],t[6])-x0)//size)
        za=int((min(t[2],t[5],t[8])-z0)//size); zb=int((max(t[2],t[5],t[8])-z0)//size)
        for z in range(za,zb+1):
            for x in range(xa,xb+1): cells[z*nx+x].append(i)
    offsets=[0]; indices=[]
    for c in cells: indices.extend(c); offsets.append(len(indices))
    out=bytearray(struct.pack('<4sIIIfffI',b'ACOL',1,nx,nz,x0,z0,size,len(triangles)))
    out.extend(struct.pack(f'<{len(offsets)}I',*offsets)); out.extend(struct.pack(f'<{len(indices)}I',*indices))
    for t in triangles: out.extend(struct.pack('<9f',*t))
    args.output.write_bytes(out); print(f'{len(triangles)} triangles, {nx} x {nz} cells, {len(indices)} references')
if __name__=='__main__': main()

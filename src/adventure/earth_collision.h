#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

// Collision data is generated from Earth draw group 2, in the same coordinates as the map.
struct EarthCollision {
    struct Point { float x,y,z; };
    struct Triangle { Point a,b,c; };
    unsigned nx=0,nz=0; float x0=0,z0=0,cell=0;
    std::vector<std::uint32_t> offsets, indices;
    std::vector<Triangle> triangles;
    bool load(const char* path) {
        FILE* f=nullptr; if (fopen_s(&f,path,"rb")) return false;
        char magic[4]; std::uint32_t version=0,count=0;
        auto read=[&](void* p,size_t n) { return std::fread(p,1,n,f)==n; };
        bool ok=read(magic,4) && std::memcmp(magic,"ACOL",4)==0 && read(&version,4) && version==1 &&
                read(&nx,4) && read(&nz,4) && read(&x0,4) && read(&z0,4) && read(&cell,4) && read(&count,4);
        if (!ok || nx==0 || nz==0 || nx>512 || nz>512 || count>1000000 || !(cell>0)) { std::fclose(f); return false; }
        offsets.resize(static_cast<size_t>(nx)*nz+1);
        ok=read(offsets.data(),offsets.size()*4) && offsets[0]==0 && std::is_sorted(offsets.begin(),offsets.end()) && offsets.back()<10000000;
        if (ok) {
            indices.resize(offsets.back()); triangles.resize(count);
            ok=read(indices.data(),indices.size()*4) && read(triangles.data(),triangles.size()*sizeof(Triangle));
            for (auto i:indices) if (i>=count) { ok=false; break; }
        }
        std::fclose(f); if (!ok) { offsets.clear(); indices.clear(); triangles.clear(); } return ok;
    }
    static Point sub(Point a,Point b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
    static Point cross(Point a,Point b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
    static float dot(Point a,Point b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
    static bool hit(const Triangle& t,Point start,Point delta,float& distance) {
        Point e1=sub(t.b,t.a),e2=sub(t.c,t.a),p=cross(delta,e2);
        float det=dot(e1,p); if (std::abs(det)<1e-8f) return false;
        float inv=1.0f/det; Point q=sub(start,t.a); float u=dot(q,p)*inv;
        if (u<0 || u>1) return false;
        Point r=cross(q,e1); float v=dot(delta,r)*inv; if (v<0 || u+v>1) return false;
        float d=dot(e2,r)*inv; if (d<0 || d>1) return false;
        distance=d; return true;
    }
    bool trace(Point from,Point to,float& nearest) const {
        if (offsets.empty()) return false;
        auto grid=[](float v,float origin,float width,int n) { return std::clamp(static_cast<int>(std::floor((v-origin)/width)),0,n-1); };
        if (std::max(from.x,to.x)<x0 || std::min(from.x,to.x)>x0+nx*cell || std::max(from.z,to.z)<z0 || std::min(from.z,to.z)>z0+nz*cell) return false;
        int xa=grid(std::min(from.x,to.x),x0,cell,nx),xb=grid(std::max(from.x,to.x),x0,cell,nx);
        int za=grid(std::min(from.z,to.z),z0,cell,nz),zb=grid(std::max(from.z,to.z),z0,cell,nz);
        bool found=false; nearest=1.0f; Point dir=sub(to,from);
        for(int z=za;z<=zb;++z) for(int x=xa;x<=xb;++x) {
            size_t k=static_cast<size_t>(z)*nx+x;
            for(auto i=offsets[k];i<offsets[k+1];++i) { float d; if(hit(triangles[indices[i]],from,dir,d) && d<nearest) { nearest=d; found=true; } }
        }
        return found;
    }
};

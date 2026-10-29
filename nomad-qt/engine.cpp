// Nomad Animator - editor/animador 3D em C++ + OpenGL ES 3.0 (NativeActivity)
#include "engine_shim.h"
#include <android/log.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <GLES3/gl31.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>
#include <ctime>
#include <cstdio>
#include <cstdarg>
#include <cstdint>
#include <string>
#include <dirent.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <sys/stat.h>
#include <zlib.h>
#include <cctype>
#include <unordered_map>
#include <climits>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <android/window.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <media/NdkMediaMuxer.h>
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "ImGuizmo.h"
namespace eng {
#define PI 3.14159265f

// ---------- matematica ----------
struct V{float x=0,y=0,z=0;};
V operator+(V a,V b){return{a.x+b.x,a.y+b.y,a.z+b.z};}
V operator-(V a,V b){return{a.x-b.x,a.y-b.y,a.z-b.z};}
V operator*(V a,float k){return{a.x*k,a.y*k,a.z*k};}
float dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
V cross(V a,V b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
V norm(V a){return a*(1/sqrtf(dot(a,a)));}
V lerp(V a,V b,float t){return a+(b-a)*t;}
struct M{float m[16];};
M id(){M r={};r.m[0]=r.m[5]=r.m[10]=r.m[15]=1;return r;}
M mul(const M&a,const M&b){M r;for(int c=0;c<4;c++)for(int i=0;i<4;i++){float s=0;for(int k=0;k<4;k++)s+=a.m[k*4+i]*b.m[c*4+k];r.m[c*4+i]=s;}return r;}
M persp(float fy,float as,float n,float f){M r={};float t=1/tanf(fy/2);r.m[0]=t/as;r.m[5]=t;r.m[10]=(f+n)/(n-f);r.m[11]=-1;r.m[14]=2*f*n/(n-f);return r;}
M lookAt(V e,V c,V up){V f=norm(c-e),s=norm(cross(f,up)),u=cross(s,f);M r=id();
 r.m[0]=s.x;r.m[4]=s.y;r.m[8]=s.z;r.m[1]=u.x;r.m[5]=u.y;r.m[9]=u.z;r.m[2]=-f.x;r.m[6]=-f.y;r.m[10]=-f.z;
 r.m[12]=-dot(s,e);r.m[13]=-dot(u,e);r.m[14]=dot(f,e);return r;}
struct T{V p,r,s{1,1,1};};
M model(const T&t){
 float cx=cosf(t.r.x),sx=sinf(t.r.x),cy=cosf(t.r.y),sy=sinf(t.r.y),cz=cosf(t.r.z),sz=sinf(t.r.z);
 M a=id(),b=id(),c=id();
 a.m[5]=cx;a.m[6]=sx;a.m[9]=-sx;a.m[10]=cx;
 b.m[0]=cy;b.m[2]=-sy;b.m[8]=sy;b.m[10]=cy;
 c.m[0]=cz;c.m[1]=sz;c.m[4]=-sz;c.m[5]=cz;
 M r=mul(c,mul(b,a));
 for(int i=0;i<4;i++){r.m[i]*=t.s.x;r.m[4+i]*=t.s.y;r.m[8+i]*=t.s.z;}
 r.m[12]=t.p.x;r.m[13]=t.p.y;r.m[14]=t.p.z;return r;}

// ---------- cena + animacao ----------
struct Key{float t;T x;};
struct Obj{int mesh;V col;T cur;std::vector<Key> k;char nm[24]="";int parent=-1;float len=1;float met=0,rou=.5f;int skin=-1;int tb=-1,tm=-1,tn=-1;float ac=0;int kind=0,shape=0;float fov=.8727f;bool hide=false,nor=false;};
std::vector<Obj> objs;
int sel=-1,tool=3,W=1,H=1;      // tool: 0 mover,1 girar,2 escalar,3 camera
bool gAutoKey=false;
bool playing=false;float tm=1/24.f;float DUR=10;int gFS=1,gFE=240,gVS=1,gVE=120,gPlayDir=1;
void syncRange(){DUR=gFE/24.f;if(gVE>gFE)gVE=gFE;if(gVS<gFS)gVS=gFS;if(gVE-gVS<5)gVE=std::min(gFE,gVS+5);}
float yaw=.9f,pitch=.5f,cd=16;V tgt{0,.5f,0};float FOV=1.047f;
V camEye(){return tgt+V{cosf(pitch)*sinf(yaw),sinf(pitch),cosf(pitch)*cosf(yaw)}*cd;}
static void qmulE(const float*a,const float*b,float*o){o[0]=a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1];o[1]=a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0];
 o[2]=a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3];o[3]=a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2];}
static void e2q(V r,float*q){float a[4]={sinf(r.x/2),0,0,cosf(r.x/2)},b[4]={0,sinf(r.y/2),0,cosf(r.y/2)},c[4]={0,0,sinf(r.z/2),cosf(r.z/2)},t[4];qmulE(b,a,t);qmulE(c,t,q);}
static V q2e(const float*q){float x=q[0],y=q[1],z=q[2],w=q[3];return V{atan2f(2*(w*x+y*z),1-2*(x*x+y*y)),asinf(std::clamp(2*(w*y-z*x),-1.f,1.f)),atan2f(2*(w*z+x*y),1-2*(y*y+z*z))};}
static V slerpE(V a,V b,float u){float qa[4],qb[4],q[4];e2q(a,qa);e2q(b,qb);float d=qa[0]*qb[0]+qa[1]*qb[1]+qa[2]*qb[2]+qa[3]*qb[3];
 if(d<0){for(int i=0;i<4;i++)qb[i]=-qb[i];d=-d;}float wa,wb;
 if(d>.9995f){wa=1-u;wb=u;}else{float th=acosf(d),sn=sinf(th);wa=sinf((1-u)*th)/sn;wb=sinf(u*th)/sn;}
 for(int i=0;i<4;i++)q[i]=qa[i]*wa+qb[i]*wb;float l=sqrtf(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]);for(int i=0;i<4;i++)q[i]/=l;return q2e(q);}
T eval(Obj&o,float t){
 auto&k=o.k;if(k.empty())return o.cur;
 if(t<=k.front().t)return k.front().x;if(t>=k.back().t)return k.back().x;
 size_t i=0;while(k[i+1].t<t)i++;
 float u=(t-k[i].t)/(k[i+1].t-k[i].t);T r;
 r.p=lerp(k[i].x.p,k[i+1].x.p,u);r.r=slerpE(k[i].x.r,k[i+1].x.r,u);r.s=lerp(k[i].x.s,k[i+1].x.s,u);return r;}
void applyAnim(){for(auto&o:objs)if(!o.k.empty())o.cur=eval(o,tm);}
struct Skin{std::vector<int> j;std::vector<M> ibm,fz;};std::vector<Skin> skins;
M worldM(int i,int d=0){M l=model(objs[i].cur);int p=objs[i].parent;return(p>=0&&d<32)?mul(worldM(p,d+1),l):l;}
V wp(int i){M m=worldM(i);return V{m.m[12],m.m[13],m.m[14]};}
V invPt(const M&m,V p){V c0{m.m[0],m.m[1],m.m[2]},c1{m.m[4],m.m[5],m.m[6]},c2{m.m[8],m.m[9],m.m[10]},d=p-V{m.m[12],m.m[13],m.m[14]};
 float det=dot(c0,cross(c1,c2));if(fabsf(det)<1e-8f)return d;return V{dot(cross(c1,c2),d),dot(cross(c2,c0),d),dot(cross(c0,c1),d)}*(1/det);}
M invAff(const M&m){V c0{m.m[0],m.m[1],m.m[2]},c1{m.m[4],m.m[5],m.m[6]},c2{m.m[8],m.m[9],m.m[10]};float det=dot(c0,cross(c1,c2));M r=id();if(fabsf(det)<1e-12f)return r;
 V r0=cross(c1,c2)*(1/det),r1=cross(c2,c0)*(1/det),r2=cross(c0,c1)*(1/det);
 r.m[0]=r0.x;r.m[4]=r0.y;r.m[8]=r0.z;r.m[1]=r1.x;r.m[5]=r1.y;r.m[9]=r1.z;r.m[2]=r2.x;r.m[6]=r2.y;r.m[10]=r2.z;
 V t{m.m[12],m.m[13],m.m[14]};r.m[12]=-(r0.x*t.x+r0.y*t.y+r0.z*t.z);r.m[13]=-(r1.x*t.x+r1.y*t.y+r1.z*t.z);r.m[14]=-(r2.x*t.x+r2.y*t.y+r2.z*t.z);return r;}
T decomp(const M&m){T t;t.p=V{m.m[12],m.m[13],m.m[14]};
 float sx=sqrtf(m.m[0]*m.m[0]+m.m[1]*m.m[1]+m.m[2]*m.m[2]),sy=sqrtf(m.m[4]*m.m[4]+m.m[5]*m.m[5]+m.m[6]*m.m[6]),sz=sqrtf(m.m[8]*m.m[8]+m.m[9]*m.m[9]+m.m[10]*m.m[10]);
 if(sx>1e-8f&&sy>1e-8f&&sz>1e-8f){t.s=V{sx,sy,sz};t.r.y=asinf(std::clamp(-m.m[2]/sx,-1.f,1.f));t.r.x=atan2f(m.m[6]/sy,m.m[10]/sz);t.r.z=atan2f(m.m[1]/sx,m.m[0]/sx);}
 return t;}
void rotateWorld(Obj&ob,int a,float ang){
 M Pr=id();if(ob.parent>=0){M Pw=worldM(ob.parent);for(int c=0;c<3;c++){float L=sqrtf(Pw.m[c*4]*Pw.m[c*4]+Pw.m[c*4+1]*Pw.m[c*4+1]+Pw.m[c*4+2]*Pw.m[c*4+2]);
  if(L>1e-8f)for(int r=0;r<3;r++)Pr.m[c*4+r]=Pw.m[c*4+r]/L;}}
 M Pi=id();for(int c=0;c<3;c++)for(int r=0;r<3;r++)Pi.m[c*4+r]=Pr.m[r*4+c];
 T tt=ob.cur;tt.p=V{0,0,0};tt.s=V{1,1,1};M Lr=model(tt);
 M R=id();float c_=cosf(ang),s_=sinf(ang);
 if(a==0){R.m[5]=c_;R.m[6]=s_;R.m[9]=-s_;R.m[10]=c_;}else if(a==1){R.m[0]=c_;R.m[2]=-s_;R.m[8]=s_;R.m[10]=c_;}else{R.m[0]=c_;R.m[1]=s_;R.m[4]=-s_;R.m[5]=c_;}
 M Ln=mul(Pi,mul(R,mul(Pr,Lr)));ob.cur.r=decomp(Ln).r;}
// ---------- control rig: IK de 2 ossos + mistura IK/FK ----------
struct Rig{int a=-1,b=-1,c=-1,tgt=-1,pole=-1;float w=1;bool rotc=true;std::vector<std::pair<float,float>> wk;V base[3];bool hasBase=false;};
std::vector<Rig> rigs;
float rigW(const Rig&g){auto&k=g.wk;if(k.empty())return g.w;if(tm<=k.front().first)return k.front().second;if(tm>=k.back().first)return k.back().second;
 size_t i=0;while(k[i+1].first<tm)i++;float u=(tm-k[i].first)/(k[i+1].first-k[i].first);return k[i].second+(k[i+1].second-k[i].second)*u;}
int rigOf(int o){for(size_t i=0;i<rigs.size();i++){Rig&g=rigs[i];if(g.a==o||g.b==o||g.c==o||g.tgt==o||g.pole==o)return(int)i;}return -1;}
static float vlen(V v){return sqrtf(dot(v,v));}
static V rotV(V v,V ax,float c,float s){return v*c+cross(ax,v)*s+ax*(dot(ax,v)*(1-c));}
static bool nmHas(const char*n,const char*k){std::string s=n;for(auto&ch:s)ch=(char)tolower((unsigned char)ch);return s.find(k)!=std::string::npos;}
V rigPoleDir(int a,int b,int c){V A=wp(a),B=wp(b),C=wp(c);float L=vlen(B-A)+vlen(C-B);V u=C-A;float lu=vlen(u);u=lu>1e-6f?u*(1/lu):V{0,1,0};
 V pv=(B-A)-u*dot(B-A,u);float lp=vlen(pv);if(lp>.08f*L)return pv*(1/lp); // membro ja dobrado: usa a dobra real
 const char*na=objs[a].nm,*nb=objs[b].nm,*nc=objs[c].nm;
 bool leg=nmHas(na,"leg")||nmHas(na,"thigh")||nmHas(nb,"knee")||nmHas(nb,"calf")||nmHas(nb,"shin")||nmHas(nc,"foot")||nmHas(nc,"leg")||nmHas(nc,"ankle")||nmHas(nc,"toe");
 V dir=leg?V{0,0,1}:V{0,0,-1}; // frente do personagem = +Z (glTF): joelho vai para frente, cotovelo para tras
 V d2=dir-u*dot(dir,u);float l2=vlen(d2);if(l2>1e-5f)return d2*(1/l2);
 return norm(cross(u,fabsf(u.y)<.9f?V{0,1,0}:V{1,0,0}));}
void setKeyAt(Obj&o,float t,const T&x){bool f=false;for(auto&k:o.k)if(fabsf(k.t-t)<.02f){k.x=x;f=true;}
 if(!f){o.k.push_back({t,x});std::sort(o.k.begin(),o.k.end(),[](const Key&a,const Key&b){return a.t<b.t;});}}
void rigKeyW(Rig&g,float v){bool f=false;for(auto&k:g.wk)if(fabsf(k.first-tm)<.02f){k.second=v;f=true;}
 if(!f){g.wk.push_back({tm,v});std::sort(g.wk.begin(),g.wk.end());}}
void aimBone(int i,int ch,V want){ // gira o osso i (menor rotacao) para que a direcao i->ch aponte para 'want'
 V p=wp(i),cur=wp(ch)-p,des=want-p;float lc=vlen(cur),ld=vlen(des);if(lc<1e-6f||ld<1e-6f)return;cur=cur*(1/lc);des=des*(1/ld);
 V ax=cross(cur,des);float s=vlen(ax),c=dot(cur,des);
 if(s<1e-6f){if(c>0)return;ax=norm(cross(cur,fabsf(cur.y)<.9f?V{0,1,0}:V{1,0,0}));s=0;c=-1;}else ax=ax*(1/s);
 M W=worldM(i);for(int col=0;col<3;col++){V v{W.m[col*4],W.m[col*4+1],W.m[col*4+2]};v=rotV(v,ax,c,s);W.m[col*4]=v.x;W.m[col*4+1]=v.y;W.m[col*4+2]=v.z;}
 M P=objs[i].parent>=0?worldM(objs[i].parent):id();objs[i].cur.r=decomp(mul(invAff(P),W)).r;}
void setWorldRot(int i,const M&Rt){ // o objeto i passa a ter a rotacao mundial de Rt (mantem escala e posicao)
 M W=worldM(i);for(int col=0;col<3;col++){V w{W.m[col*4],W.m[col*4+1],W.m[col*4+2]},t{Rt.m[col*4],Rt.m[col*4+1],Rt.m[col*4+2]};float lt=vlen(t),lw=vlen(w);if(lt<1e-8f)return;t=t*(lw/lt);W.m[col*4]=t.x;W.m[col*4+1]=t.y;W.m[col*4+2]=t.z;}
 M P=objs[i].parent>=0?worldM(objs[i].parent):id();objs[i].cur.r=decomp(mul(invAff(P),W)).r;}
void solveRigs(){
 int n=(int)objs.size();
 for(Rig&g:rigs){if(g.a<0||g.b<0||g.c<0||g.tgt<0||g.a>=n||g.b>=n||g.c>=n||g.tgt>=n)continue;
  int ids[3]={g.a,g.b,g.c};float w=rigW(g);
  for(int k=0;k<3;k++){Obj&o=objs[ids[k]];if(!o.k.empty())g.base[k]=eval(o,tm).r;else if(w<=.001f||!g.hasBase)g.base[k]=o.cur.r;}
  g.hasBase=true;
  for(int k=0;k<3;k++)objs[ids[k]].cur.r=g.base[k];
  if(w<=.001f)continue;
  V A=wp(g.a),B=wp(g.b),C=wp(g.c),Tp=wp(g.tgt);
  float l1=vlen(B-A),l2=vlen(C-B);if(l1<1e-5f||l2<1e-5f)continue;
  V dT=Tp-A;float d=vlen(dT);if(d<1e-6f)continue;V u=dT*(1/d);
  float dmax=(l1+l2)*(1-1e-4f),dmin=fabsf(l1-l2)+(l1+l2)*1e-4f;float dc=std::clamp(d,dmin,dmax);
  V hint=(g.pole>=0&&g.pole<n)?wp(g.pole)-A:B-A;V pv=hint-u*dot(hint,u);float lp=vlen(pv);
  if(lp<1e-5f){pv=cross(u,fabsf(u.y)<.9f?V{0,1,0}:V{1,0,0});lp=vlen(pv);}pv=pv*(1/lp);
  float x=(l1*l1-l2*l2+dc*dc)/(2*dc),h=sqrtf(std::max(l1*l1-x*x,0.f));
  V E=A+u*x+pv*h,Te=A+u*dc;
  aimBone(g.a,g.b,E);aimBone(g.b,g.c,Te);
  if(g.rotc)setWorldRot(g.c,worldM(g.tgt));
  if(w<.999f)for(int k=0;k<3;k++){V rs=objs[ids[k]].cur.r;objs[ids[k]].cur.r=slerpE(g.base[k],rs,w);}}}
// ---------- camera como objeto (kind 3): POV e camera de render ----------
int gCamView=-1,gCamAct=-1;float gSaveYaw=0,gSavePitch=0,gSaveCd=8,gSaveFov=1.047f;V gSaveTgt;
bool camBasis(int i,V&e,V&f,V&r,V&u){if(i<0||i>=(int)objs.size())return false;M W=worldM(i);e=V{W.m[12],W.m[13],W.m[14]};
 V c0{W.m[0],W.m[1],W.m[2]},c1{W.m[4],W.m[5],W.m[6]},c2{W.m[8],W.m[9],W.m[10]};if(vlen(c0)<1e-8f||vlen(c1)<1e-8f||vlen(c2)<1e-8f)return false;
 r=norm(c0);u=norm(c1);f=norm(c2)*-1.f;return true;} // a camera olha para -Z local (como no glTF/Blender)
int activeCam(){if(gCamAct>=0&&gCamAct<(int)objs.size()&&objs[gCamAct].kind==3)return gCamAct;for(int i=0;i<(int)objs.size();i++)if(objs[i].kind==3)return i;return -1;}
bool camRender(V&e,V&f,V&r,V&u,float&fov){int i=activeCam();if(i<0||!camBasis(i,e,f,r,u))return false;fov=objs[i].fov;return true;}
void camViewOff(){if(gCamView<0)return;gCamView=-1;yaw=gSaveYaw;pitch=gSavePitch;cd=gSaveCd;tgt=gSaveTgt;FOV=gSaveFov;}
void viewFromCam(){int i=gCamView;V e,f,r,u;if(i<0||i>=(int)objs.size()||objs[i].kind!=3||!camBasis(i,e,f,r,u)){camViewOff();return;}
 V dir=f*-1.f;pitch=std::clamp(asinf(std::clamp(dir.y,-1.f,1.f)),-1.5f,1.5f);yaw=atan2f(dir.x,dir.z);cd=5;tgt=e+f*cd;FOV=objs[i].fov;}
void camViewOn(int i){if(gCamView<0){gSaveYaw=yaw;gSavePitch=pitch;gSaveCd=cd;gSaveTgt=tgt;gSaveFov=FOV;}gCamView=i;gCamAct=i;viewFromCam();}
void camFromView(){int i=gCamView;if(i<0||i>=(int)objs.size())return;V e=camEye(),f=norm(tgt-e),r=norm(cross(f,V{0,1,0})),u=cross(r,f);
 M m=id();m.m[0]=r.x;m.m[1]=r.y;m.m[2]=r.z;m.m[4]=u.x;m.m[5]=u.y;m.m[6]=u.z;m.m[8]=-f.x;m.m[9]=-f.y;m.m[10]=-f.z;m.m[12]=e.x;m.m[13]=e.y;m.m[14]=e.z;
 M P=objs[i].parent>=0?worldM(objs[i].parent):id();T t=decomp(mul(invAff(P),m));objs[i].cur.p=t.p;objs[i].cur.r=t.r;}
void addObj(int mesh){
 static const V pal[]={{.9f,.35f,.3f},{.3f,.7f,.9f},{.5f,.85f,.4f},{.95f,.8f,.3f},{.7f,.45f,.9f}};
 Obj o;o.mesh=mesh;o.col=pal[objs.size()%5];static int cnt[6]={0,0,0,0,0,0};static const char*NM[6]={"Cube","Sphere","Plane","","","Bone"};snprintf(o.nm,24,"%s.%03d",NM[mesh],++cnt[mesh]);
 if(mesh==5){o.col={.85f,.85f,.65f};o.cur.p={0,0,0};if(sel>=0&&objs[sel].mesh==5){o.parent=sel;o.cur.p={0,objs[sel].len,0};}}
 else{o.cur.p={(objs.size()%4)*1.4f-2.1f,mesh==2?0.f:.5f,0};if(mesh==2)o.cur.s={2,2,2};}
 objs.push_back(o);sel=(int)objs.size()-1;}
static const V LCOL[6]={{1.f,1.f,1.f},{1.f,.85f,.62f},{.68f,.84f,1.f},{1.f,.32f,.28f},{.42f,1.f,.48f},{.38f,.52f,1.f}};
void addLight(int ty){ // 0 sol (direcional), 1 ponto, 2 spot. Aponta para -Y local; len = intensidade, fov = meio-angulo do spot
 ty=std::clamp(ty,0,2);Obj o;o.mesh=1;o.kind=4;o.shape=ty;static int cnt[3]={0,0,0};static const char*NM[3]={"Sun","Point","Spot"};
 snprintf(o.nm,24,"%s.%03d",NM[ty],++cnt[ty]);o.col=LCOL[ty==1?1:0];o.len=ty==0?3.f:(ty==1?40.f:60.f);o.fov=.6f;
 o.cur.p={(objs.size()%4)*1.4f-2.1f,ty==0?5.f:3.f,0};objs.push_back(o);sel=(int)objs.size()-1;}
void clearParent(int c){M w=worldM(c);objs[c].parent=-1;objs[c].cur=decomp(w);}
void delObj(int i){
 if(gCamView==i)camViewOff();if(gCamAct==i)gCamAct=-1;
 for(int ri=(int)rigs.size()-1;ri>=0;ri--){Rig&g=rigs[ri];if(g.a==i||g.b==i||g.c==i||g.tgt==i||g.pole==i)rigs.erase(rigs.begin()+ri);}
 for(auto&sk:skins){if(sk.fz.size()<sk.j.size())sk.fz.assign(sk.j.size(),id());for(size_t k=0;k<sk.j.size();k++)if(sk.j[k]==i){sk.fz[k]=worldM(i);sk.j[k]=-1;}}
 int gp=objs[i].parent;
 for(int j=0;j<(int)objs.size();j++)if(objs[j].parent==i){M w=worldM(j);objs[j].parent=gp;objs[j].cur=decomp(gp>=0?mul(invAff(worldM(gp)),w):w);}
 objs.erase(objs.begin()+i);for(auto&o:objs)if(o.parent>i)o.parent--;sel=-1;
 for(Rig&g:rigs){int*qq[5]={&g.a,&g.b,&g.c,&g.tgt,&g.pole};for(int*pp:qq)if(*pp>i)(*pp)--;}
 if(gCamView>i)gCamView--;if(gCamAct>i)gCamAct--;
 for(auto&sk:skins)for(auto&jj:sk.j)if(jj>i)jj--;}

// ---------- GL ----------
const char*VS=R"(#version 300 es
layout(location=0) in vec3 aP; layout(location=1) in vec3 aN; layout(location=2) in vec4 aJ; layout(location=3) in vec4 aW; layout(location=4) in vec2 aUV;
uniform mat4 uMVP; uniform mat4 uM; uniform mat4 uJ[64]; uniform float uSk; out vec3 vN; out vec3 vP; out vec3 vW; out vec2 vUV;
void main(){vec4 p=vec4(aP,1.0);vec3 n=aN;if(uSk>0.5){mat4 S=aW.x*uJ[int(aJ.x)]+aW.y*uJ[int(aJ.y)]+aW.z*uJ[int(aJ.z)]+aW.w*uJ[int(aJ.w)];p=S*p;n=mat3(S)*n;}
 gl_Position=uMVP*p; vN=mat3(uM)*n; vP=aP; vW=(uM*p).xyz; vUV=aUV;})";
const char*FS=R"(#version 300 es
precision highp float; in vec3 vN; in vec3 vP; in vec3 vW; in vec2 vUV; uniform highp sampler2DArray uTex; uniform vec4 uTL; uniform vec4 uC; uniform float uL; uniform vec3 uS; uniform vec3 uCam; uniform float uMc; uniform vec3 uMcR,uMcU,uMcB; uniform vec2 uMR; out vec4 o;
vec3 shade(vec3 L,vec3 Lc,vec3 N,vec3 V,vec3 alb,vec3 F0,float met,float rou){
 vec3 H=normalize(L+V);float a=rou*rou,a2=a*a;float NL=max(dot(N,L),0.0),NV=max(dot(N,V),1e-3),NH=max(dot(N,H),0.0),VH=max(dot(V,H),0.0);
 float dd=NH*NH*(a2-1.0)+1.0;float D=a2/(3.14159*dd*dd+1e-6);float k=(rou+1.0)*(rou+1.0)/8.0;
 float G=(NL/(NL*(1.0-k)+k))*(NV/(NV*(1.0-k)+k));vec3 F=F0+(1.0-F0)*pow(max(1.0-VH,0.0),5.0);
 vec3 spec=D*G*F/(4.0*NV*max(NL,1e-3));vec3 kd=(1.0-F)*(1.0-met);
 return (kd*alb/3.14159+spec)*Lc*NL;}
vec3 pbr(){vec3 N=normalize(vN),V=normalize(uCam-vW);if(dot(N,V)<0.0)N=-N;
 vec3 base=uC.rgb;float met=uMR.x,rou=uMR.y;
 if(uTL.x>=0.0)base*=texture(uTex,vec3(vUV,uTL.x)).rgb;
 if(uTL.y>=0.0){vec4 mrs=texture(uTex,vec3(vUV,uTL.y));rou*=mrs.g;met*=mrs.b;}
 if(uTL.z>=0.0){vec3 tn=texture(uTex,vec3(vUV,uTL.z)).xyz*2.0-1.0;vec3 dp1=dFdx(vW),dp2=dFdy(vW);vec2 du1=dFdx(vUV),du2=dFdy(vUV);
  vec3 dp2p=cross(dp2,N),dp1p=cross(N,dp1);vec3 Tt=dp2p*du1.x+dp1p*du2.x,Bt=-(dp2p*du1.y+dp1p*du2.y);float iv=inversesqrt(max(dot(Tt,Tt),dot(Bt,Bt))+1e-20);N=normalize(mat3(Tt*iv,Bt*iv,N)*tn);}
 vec3 alb=pow(base,vec3(2.2));rou=clamp(rou,0.05,1.0);vec3 F0=mix(vec3(0.04),alb,met);
 vec3 col=shade(normalize(vec3(.4,.8,.5)),vec3(3.2,3.0,2.8),N,V,alb,F0,met,rou)+shade(normalize(vec3(-.6,.3,-.4)),vec3(.6,.7,1.0),N,V,alb,F0,met,rou);
 vec3 amb=mix(vec3(.12,.11,.10),vec3(.22,.26,.34),N.y*.5+.5);float NV=max(dot(N,V),0.0);
 vec3 Fa=F0+(max(vec3(1.0-rou),F0)-F0)*pow(1.0-NV,5.0);
 col+=amb*(alb*(1.0-met)+Fa*0.6);
 col=col*(2.51*col+0.03)/(col*(2.43*col+0.59)+0.14);return pow(clamp(col,0.0,1.0),vec3(1.0/2.2));}
vec3 matcap(){vec3 N=normalize(vN),V=normalize(uCam-vW);if(dot(N,V)<0.0)N=-N;
  vec3 n=vec3(dot(N,uMcR),dot(N,uMcU),dot(N,uMcB));
  vec3 L1=normalize(vec3(.10,.50,.85)),L2=normalize(vec3(-.60,-.30,.50));
  float d1=max(dot(n,L1),0.0),d2=max(dot(n,L2),0.0);
  float amb=.46+.10*n.y;
  vec3 c=vec3(.64)*(amb+.50*d1+.22*d2);
  vec3 H=normalize(L1+vec3(0.0,0.0,1.0));c+=vec3(.04)*pow(max(dot(n,H),0.0),24.0);
  return clamp(c,0.0,1.0);}
 void main(){if(uL<-1.5){vec2 q=(vP.xy-.5)*uS.xy;float dd=(abs(q.x)+abs(q.y)-uS.x*.5)*.7071;o=vec4(uC.rgb,uC.a*clamp(.5-dd,0.0,1.0));return;} if(uL<0.0){vec2 q=(vP.xy-.5)*uS.xy;vec2 e=abs(q)-uS.xy*.5+uS.z;float dd=length(max(e,0.0))+min(max(e.x,e.y),0.0)-uS.z;o=vec4(uC.rgb,uC.a*clamp(.5-dd,0.0,1.0));return;} if(uL>0.5){if(uTL.w>0.5&&uTL.x>=0.0&&texture(uTex,vec3(vUV,uTL.x)).a<0.5)discard;o=vec4(uMc>0.5?matcap():pbr(),uC.a);return;} o=vec4(uC.rgb,uC.a);})";
GLint uMcL=-1,uMcRL=-1,uMcUL=-1,uMcBL=-1;GLuint uMVP,uM,uC,uL,uS,uCamL,uMRL,uJL,uSkL,uTLL,uTexL,gProg=0;float gS[3]={1,1,0},gMR[2]={0,.5f},gSk=0,gTL[4]={-1,-1,-1,0};void initText();void initPost();void initMC();void initTexArr();
struct Mesh{GLuint vao=0,vbo=0;int n=0;GLenum mode=GL_TRIANGLES;std::vector<float> cpu;float rad=1;GLuint svbo=0;bool skinned=false;std::vector<float> skin;GLuint uvbo=0;std::vector<float> uv;};
Mesh gBoneWire,gJointRing;
std::vector<Mesh> meshes(6); // 0 cubo,1 esfera,2 plano,3 quad2D,4 grade,5 osso
GLuint sh(GLenum t,const char*s){GLuint h=glCreateShader(t);glShaderSource(h,1,&s,0);glCompileShader(h);GLint ok;glGetShaderiv(h,GL_COMPILE_STATUS,&ok);
 if(!ok){char b[512];glGetShaderInfoLog(h,512,0,b);__android_log_print(ANDROID_LOG_ERROR,"NA","%s",b);}return h;}
void vtx(std::vector<float>&v,V p,V n){v.insert(v.end(),{p.x,p.y,p.z,n.x,n.y,n.z});}
Mesh upload(const std::vector<float>&v,GLenum mode){
 Mesh m;m.n=(int)v.size()/6;m.mode=mode;m.cpu=v;
 glGenVertexArrays(1,&m.vao);glGenBuffers(1,&m.vbo);glBindVertexArray(m.vao);glBindBuffer(GL_ARRAY_BUFFER,m.vbo);
 glBufferData(GL_ARRAY_BUFFER,v.size()*4,v.data(),GL_STATIC_DRAW);
 glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,24,(void*)0);
 glEnableVertexAttribArray(1);glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,24,(void*)12);return m;}
void attachSkin(Mesh&m,const std::vector<float>&sk){
 glBindVertexArray(m.vao);glGenBuffers(1,&m.svbo);glBindBuffer(GL_ARRAY_BUFFER,m.svbo);glBufferData(GL_ARRAY_BUFFER,sk.size()*4,sk.data(),GL_STATIC_DRAW);
 glEnableVertexAttribArray(2);glVertexAttribPointer(2,4,GL_FLOAT,GL_FALSE,32,(void*)0);
 glEnableVertexAttribArray(3);glVertexAttribPointer(3,4,GL_FLOAT,GL_FALSE,32,(void*)16);m.skinned=true;m.skin=sk;}
void attachUV(Mesh&m,const std::vector<float>&uv){glBindVertexArray(m.vao);glGenBuffers(1,&m.uvbo);glBindBuffer(GL_ARRAY_BUFFER,m.uvbo);glBufferData(GL_ARRAY_BUFFER,uv.size()*4,uv.data(),GL_STATIC_DRAW);
 glEnableVertexAttribArray(4);glVertexAttribPointer(4,2,GL_FLOAT,GL_FALSE,8,(void*)0);m.uv=uv;}
void buildMeshes(){
 std::vector<float> a;
 V F[6][3]={{{1,0,0},{0,1,0},{0,0,1}},{{-1,0,0},{0,0,1},{0,1,0}},{{0,1,0},{0,0,1},{1,0,0}},
            {{0,-1,0},{1,0,0},{0,0,1}},{{0,0,1},{1,0,0},{0,1,0}},{{0,0,-1},{0,1,0},{1,0,0}}};
 int ix[6]={0,1,2,0,2,3};
 for(auto&f:F){V n=f[0],u=f[1],v=f[2],c=n*.5f;V p[4]={c-u*.5f-v*.5f,c+u*.5f-v*.5f,c+u*.5f+v*.5f,c-u*.5f+v*.5f};
  for(int i:ix)vtx(a,p[i],n);}
 meshes[0]=upload(a,GL_TRIANGLES);
 a.clear();
 auto sp=[](int i,int j){float th=i*PI/16,ph=j*2*PI/24;return V{sinf(th)*cosf(ph),cosf(th),sinf(th)*sinf(ph)};};
 for(int i=0;i<16;i++)for(int j=0;j<24;j++){V p=sp(i,j),q=sp(i+1,j),r=sp(i+1,j+1),s=sp(i,j+1);
  vtx(a,p*.5f,p);vtx(a,q*.5f,q);vtx(a,r*.5f,r);vtx(a,p*.5f,p);vtx(a,r*.5f,r);vtx(a,s*.5f,s);}
 meshes[1]=upload(a,GL_TRIANGLES);
 a.clear();V up{0,1,0};
 V pl[6]={{-1,0,-1},{-1,0,1},{1,0,1},{-1,0,-1},{1,0,1},{1,0,-1}};for(V p:pl)vtx(a,p,up);
 meshes[2]=upload(a,GL_TRIANGLES);
 a.clear();
 V qd[6]={{0,0,0},{1,0,0},{1,1,0},{0,0,0},{1,1,0},{0,1,0}};for(V p:qd)vtx(a,p,{0,0,1});
 meshes[3]=upload(a,GL_TRIANGLES);
 a.clear();
 for(int i=-10;i<=10;i++){vtx(a,{(float)i,0,-10},up);vtx(a,{(float)i,0,10},up);vtx(a,{-10,0,(float)i},up);vtx(a,{10,0,(float)i},up);}
 meshes[4]=upload(a,GL_LINES);
 a.clear();{V h{0,0,0},t{0,1,0};float w=.12f,y=.15f;V r[4]={{w,y,0},{0,y,w},{-w,y,0},{0,y,-w}};
  for(int i=0;i<4;i++){V p=r[i],q=r[(i+1)%4];V n1=norm(cross(p-h,q-h)),n2=norm(cross(q-t,p-t));
   vtx(a,h,n1);vtx(a,p,n1);vtx(a,q,n1);vtx(a,t,n2);vtx(a,q,n2);vtx(a,p,n2);}}
 meshes[5]=upload(a,GL_TRIANGLES);
 a.clear();{V h{0,0,0},t{0,1,0};float w=.12f,y=.15f;V r[4]={{w,y,0},{0,y,w},{-w,y,0},{0,y,-w}};
  for(int i=0;i<4;i++){V p=r[i],q=r[(i+1)%4];vtx(a,h,up);vtx(a,p,up);vtx(a,p,up);vtx(a,q,up);vtx(a,t,up);vtx(a,p,up);}}
 gBoneWire=upload(a,GL_LINES);
 a.clear();for(int ax=0;ax<3;ax++)for(int k=0;k<24;k++){float a0=k*2*PI/24,a1=(k+1)*2*PI/24;V p0,p1;
  if(ax==0){p0={cosf(a0),sinf(a0),0};p1={cosf(a1),sinf(a1),0};}else if(ax==1){p0={cosf(a0),0,sinf(a0)};p1={cosf(a1),0,sinf(a1)};}else{p0={0,cosf(a0),sinf(a0)};p1={0,cosf(a1),sinf(a1)};}
  vtx(a,p0,up);vtx(a,p1,up);}
 gJointRing=upload(a,GL_LINES);}
void initGL(){
 GLuint p=gProg=glCreateProgram();glAttachShader(p,sh(GL_VERTEX_SHADER,VS));glAttachShader(p,sh(GL_FRAGMENT_SHADER,FS));
 glLinkProgram(p);glUseProgram(p);
 uMVP=glGetUniformLocation(p,"uMVP");uM=glGetUniformLocation(p,"uM");uC=glGetUniformLocation(p,"uC");uL=glGetUniformLocation(p,"uL");uS=glGetUniformLocation(p,"uS");uCamL=glGetUniformLocation(p,"uCam");uMcL=glGetUniformLocation(p,"uMc");uMcRL=glGetUniformLocation(p,"uMcR");uMcUL=glGetUniformLocation(p,"uMcU");uMcBL=glGetUniformLocation(p,"uMcB");uJL=glGetUniformLocation(p,"uJ");uSkL=glGetUniformLocation(p,"uSk");uTLL=glGetUniformLocation(p,"uTL");uTexL=glGetUniformLocation(p,"uTex");glUniform1i(uTexL,1);uMRL=glGetUniformLocation(p,"uMR");
 glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);buildMeshes();for(size_t i=6;i<meshes.size();i++){std::vector<float> c=meshes[i].cpu,sk=meshes[i].skin,uvv=meshes[i].uv;float r=meshes[i].rad;meshes[i]=upload(c,GL_TRIANGLES);meshes[i].rad=r;if(!sk.empty())attachSkin(meshes[i],sk);if(!uvv.empty())attachUV(meshes[i],uvv);}initText();initPost();initMC();initTexArr();}
void draw(Mesh&m,const M&mvp,const M&mod,float r,float g,float b,float a,float lit){
 glUniformMatrix4fv(uMVP,1,GL_FALSE,mvp.m);glUniformMatrix4fv(uM,1,GL_FALSE,mod.m);
 glUniform4f(uC,r,g,b,a);glUniform1f(uL,lit);glUniform3f(uS,gS[0],gS[1],gS[2]);glUniform2f(uMRL,gMR[0],gMR[1]);glUniform1f(uSkL,gSk);glUniform4f(uTLL,gTL[0],gTL[1],gTL[2],gTL[3]);glBindVertexArray(m.vao);glDrawArrays(m.mode,0,m.n);}

// ---------- UI 2D ----------
void rect(float x,float y,float w,float h,float r,float g,float b,float a=1,float rad=0){
 M m={};m.m[0]=2*w/W;m.m[5]=-2*h/H;m.m[10]=1;m.m[12]=2*x/W-1;m.m[13]=1-2*y/H;m.m[15]=1;
 gS[0]=w;gS[1]=h;gS[2]=std::min(rad,std::min(w,h)/2);draw(meshes[3],m,id(),r,g,b,a,-1);}
const char*GN="ABCDEHIKLMNOPRSTUVYXZ0123456789";
const char*GG[]={"010101111101101","110101110101110","011100100100011","110101101101110","111100110100111",
 "101101111101101","111010010010111","101101110101101","100100100100111","101111111101101","110101101101101",
 "010101101101010","110101110100100","110101110101101","011100010001110","111010010010010","101101101101111",
 "101101101101010","101101010010010","101101010101101","111001010100111","111101101101111","010110010010111","111001111100111","111001111001111","101101111001001","111100111001111","111100111101111","111001001010010","111101111101111","111101111001111"};
void btext(const char*s,float x,float y,float ps,float r,float g,float b){
 for(;*s;s++){const char*p=strchr(GN,*s);
  if(p){const char*gl=GG[p-GN];for(int k=0;k<15;k++)if(gl[k]=='1')rect(x+(k%3)*ps,y+(k/3)*ps,ps,ps,r,g,b);}
  x+=4*ps;}}
// ---- fonte TTF do sistema (stb_truetype) ----
std::vector<unsigned char> gAtlas;stbtt_bakedchar gBC[224];bool gHi=false;GLuint gTex=0,gTP=0,gTVao=0,gTVbo=0,uTS,uTC;
const float FPX=44;
bool loadFont(){
 FILE*f=nullptr;
 static const char*P[]={"/system/fonts/Roboto-Regular.ttf","/system/fonts/RobotoStatic-Regular.ttf","/system/fonts/Roboto[wdth,wght].ttf","/system/fonts/NotoSans-Regular.ttf","/system/fonts/DroidSans.ttf"};
 for(auto q:P){f=fopen(q,"rb");if(f)break;}
 for(int pass=0;pass<2&&!f;pass++){if(DIR*dr=opendir("/system/fonts")){while(dirent*en=readdir(dr)){std::string n=en->d_name;
  bool ttf=n.size()>8&&n.compare(n.size()-4,4,".ttf")==0,bad=false;
  for(const char*w:{"Italic","Bold","Light","Thin","Medium","Black","Mono","Slab","Serif","Emoji","Symbols","Cjk","CJK","Condensed"})if(n.find(w)!=std::string::npos)bad=true;
  bool ok=pass==0?n.find("Roboto")!=std::string::npos:n.find("Regular")!=std::string::npos;
  if(ttf&&ok&&!bad){f=fopen(("/system/fonts/"+n).c_str(),"rb");if(f)break;}}closedir(dr);}}
 if(!f)return false;
 fseek(f,0,SEEK_END);long sz=ftell(f);fseek(f,0,SEEK_SET);std::vector<unsigned char> d(sz>0?sz:1);
 size_t rd=fread(d.data(),1,sz,f);fclose(f);if(sz<=0||(long)rd!=sz)return false;
 gAtlas.assign(512*512,0);
 if(stbtt_BakeFontBitmap(d.data(),0,FPX,gAtlas.data(),512,512,32,224,gBC)>0)gHi=true;
 else{std::fill(gAtlas.begin(),gAtlas.end(),0);if(stbtt_BakeFontBitmap(d.data(),0,FPX,gAtlas.data(),512,512,32,96,gBC)<=0){gAtlas.clear();return false;}}
 return true;}
void initText(){
 static bool tried=false;if(!tried){tried=true;loadFont();}
 gTex=0;if(gAtlas.empty())return;
 const char*vs="#version 300 es\nlayout(location=0) in vec4 aV; uniform vec2 uScr; out vec2 vT;\nvoid main(){gl_Position=vec4(aV.x/uScr.x*2.0-1.0,1.0-aV.y/uScr.y*2.0,0.0,1.0); vT=aV.zw;}";
 const char*fs="#version 300 es\nprecision mediump float; in vec2 vT; uniform sampler2D uTex; uniform vec4 uC; out vec4 o;\nvoid main(){o=vec4(uC.rgb,uC.a*texture(uTex,vT).r);}";
 GLuint p=glCreateProgram();glAttachShader(p,sh(GL_VERTEX_SHADER,vs));glAttachShader(p,sh(GL_FRAGMENT_SHADER,fs));glLinkProgram(p);gTP=p;
 uTS=glGetUniformLocation(p,"uScr");uTC=glGetUniformLocation(p,"uC");glUseProgram(p);glUniform1i(glGetUniformLocation(p,"uTex"),0);glUseProgram(gProg);
 glGenTextures(1,&gTex);glBindTexture(GL_TEXTURE_2D,gTex);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
 glTexImage2D(GL_TEXTURE_2D,0,GL_R8,512,512,0,GL_RED,GL_UNSIGNED_BYTE,gAtlas.data());
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
 glGenVertexArrays(1,&gTVao);glGenBuffers(1,&gTVbo);glBindVertexArray(gTVao);glBindBuffer(GL_ARRAY_BUFFER,gTVbo);
 glEnableVertexAttribArray(0);glVertexAttribPointer(0,4,GL_FLOAT,GL_FALSE,16,(void*)0);}
static int u8next(const char*&s){int c=(unsigned char)*s++;if(c<0x80)return c;if(c>=0xC0&&c<0xE0&&(((unsigned char)*s)&0xC0)==0x80){int c2=(unsigned char)*s++;return((c&0x1F)<<6)|(c2&0x3F);}return 0;}
static int u8fold(int c){if(c<128||gHi)return c; // sem glifos acentuados: usa a letra base
 static const int F[36]={0xE1,0xE0,0xE2,0xE3,0xE4,0xC1,0xC0,0xC2,0xC3,0xC4,0xE9,0xE8,0xEA,0xEB,0xC9,0xC8,0xCA,0xED,0xEC,0xEE,0xCD,0xF3,0xF2,0xF4,0xF5,0xF6,0xD3,0xD4,0xD5,0xFA,0xF9,0xFB,0xFC,0xDA,0xE7,0xC7};
 static const char*T="aaaaaAAAAAeeeeEEEiiiIooooOOOuuuuUcC";for(int i=0;i<36;i++)if(F[i]==c)return T[i];return 0;}
float tw(const char*s,float ps){if(!gTex)return strlen(s)*4*ps-ps;float w=0;while(*s){int c=u8fold(u8next(s));if(c>=32&&c<(gHi?256:128))w+=gBC[c-32].xadvance;}return w*ps*7/FPX;}
void text(const char*s,float x,float y,float ps,float r,float g,float b){
 if(!gTex){btext(s,x,y,ps,r,g,b);return;}
 float sc=ps*7/FPX,by=y+5*ps;std::vector<float> v;
 while(*s){int c=u8fold(u8next(s));if(c<32||c>=(gHi?256:128))continue;stbtt_aligned_quad q;float xp=0,yp=0;stbtt_GetBakedQuad(gBC,512,512,c-32,&xp,&yp,&q,1);
  float x0=x+q.x0*sc,x1=x+q.x1*sc,y0=by+q.y0*sc,y1=by+q.y1*sc;
  v.insert(v.end(),{x0,y0,q.s0,q.t0,x1,y0,q.s1,q.t0,x1,y1,q.s1,q.t1,x0,y0,q.s0,q.t0,x1,y1,q.s1,q.t1,x0,y1,q.s0,q.t1});x+=xp*sc;}
 if(v.empty())return;
 glUseProgram(gTP);glUniform2f(uTS,(float)W,(float)H);glUniform4f(uTC,r,g,b,1);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,gTex);
 glBindVertexArray(gTVao);glBindBuffer(GL_ARRAY_BUFFER,gTVbo);glBufferData(GL_ARRAY_BUFFER,v.size()*4,v.data(),GL_STREAM_DRAW);
 glDrawArrays(GL_TRIANGLES,0,(int)v.size()/4);glUseProgram(gProg);}
struct Rc{float x,y,w,h;};struct Btn{Rc r;int id;};
std::vector<Btn> gBtn;std::vector<Rc> gBlock;Rc gTrack={0,0,0,0};
float U(){return H*.01f;}
bool inR(const Rc&r,float x,float y){return x>=r.x&&x<=r.x+r.w&&y>=r.y&&y<=r.y+r.h;}
void dia(float cx,float cy,float sz,float r,float g,float b){M m={};m.m[0]=2*sz/W;m.m[5]=-2*sz/H;m.m[10]=1;m.m[12]=2*(cx-sz/2)/W-1;m.m[13]=1-2*(cy-sz/2)/H;m.m[15]=1;
 gS[0]=sz;gS[1]=sz;gS[2]=0;draw(meshes[3],m,id(),r,g,b,1,-2);}
void tcen(const char*s,float cx,float cy,float ps,float r,float g,float b){text(s,cx-tw(s,ps)/2,cy-2.5f*ps,ps,r,g,b);}
void panel(float x,float y,float w,float h,float rad){rect(x,y,w,h,.13f,.13f,.14f,.9f,rad);gBlock.push_back({x,y,w,h});}
void pill(float x,float y,float w,float h,int bid,const char*lb,bool on,float cr,float cg,float cb){
 if(on){cr=.28f;cg=.45f;cb=.70f;}
 rect(x,y,w,h,cr,cg,cb,1,h*.28f);tcen(lb,x+w/2,y+h/2,h*.072f,.94f,.94f,.94f);gBtn.push_back({{x,y,w,h},bid});}
void icon(int bid,float cx,float cy,float s,bool on){
 float br=on?.28f:.33f,bg=on?.45f:.33f,bb=on?.70f:.35f;
 if(bid==6){rect(cx-s,cy-s,2*s,2*s,.92f,.92f,.92f,1,s);rect(cx-s*.7f,cy-s*.7f,1.4f*s,1.4f*s,br,bg,bb,1,s*.7f);rect(cx-s*.22f,cy-s*.22f,s*.44f,s*.44f,.92f,.92f,.92f,1,s*.22f);}
 else if(bid==3){rect(cx-s,cy-s*.09f,2*s,s*.18f,.92f,.92f,.92f);rect(cx-s*.09f,cy-s,s*.18f,2*s,.92f,.92f,.92f);
  dia(cx-s,cy,s*.7f,.92f,.92f,.92f);dia(cx+s,cy,s*.7f,.92f,.92f,.92f);dia(cx,cy-s,s*.7f,.92f,.92f,.92f);dia(cx,cy+s,s*.7f,.92f,.92f,.92f);}
 else if(bid==4){rect(cx-s,cy-s,2*s,2*s,.92f,.92f,.92f,1,s);rect(cx-s*.66f,cy-s*.66f,1.32f*s,1.32f*s,br,bg,bb,1,s*.66f);rect(cx+s*.42f,cy-s*.9f,s*.45f,s*.45f,1,.8f,.2f,1,s*.22f);}
 else{rect(cx-s,cy-s,2*s,2*s,.92f,.92f,.92f,1,s*.18f);rect(cx-s*.78f,cy-s*.78f,1.56f*s,1.56f*s,br,bg,bb,1,s*.1f);rect(cx-s*.55f,cy-s*.55f,s*.7f,s*.7f,.92f,.92f,.92f,1,s*.08f);}}
const char*BL[10]={"CUBE","SPH","PLN","MOVE","ROT","SCL","CAM","KEY","PLAY","DEL"};
float BC[10][3]={{.2f,.6f,.3f},{.2f,.6f,.3f},{.2f,.6f,.3f},{.2f,.4f,.8f},{.2f,.4f,.8f},{.2f,.4f,.8f},{.2f,.4f,.8f},{.9f,.75f,.1f},{.9f,.5f,.1f},{.8f,.2f,.2f}};
float ML(){return W*.04f;} float bw(){return (W-2*ML())/10.f;} float bh(){return H*.1f;} float tlh(){return H*.09f;} float tly(){return H-H*.045f-tlh();}
float gTX0=0,gTW=1;float tx(float t){return gTX0+(t*24-gVS)/(float)std::max(gVE-gVS,1)*gTW;}

bool ready=false;

// ---------- render ----------
M gVP;int gAxis=-1,gIdx=0;bool gDrag=false;
const V AX[3]={{1,0,0},{0,1,0},{0,0,1}};
const float AC[3][3]={{.9f,.25f,.25f},{.4f,.8f,.3f},{.3f,.5f,1.f}};
float gl(){return cd*.2f;}
float nearP(){return std::clamp(cd*.05f,.005f,.1f);} // plano proximo diminui ao dar zoom perto
bool prj(V p,float&x,float&y){const float*m=gVP.m;
 float cx=m[0]*p.x+m[4]*p.y+m[8]*p.z+m[12],cy=m[1]*p.x+m[5]*p.y+m[9]*p.z+m[13],cw=m[3]*p.x+m[7]*p.y+m[11]*p.z+m[15];
 if(cw<.01f)return false;x=(cx/cw*.5f+.5f)*W;y=(1-(cy/cw*.5f+.5f))*H;return true;}
V ringPt(int a,int i){float th=i*2*PI/32,c=cosf(th)*gl()*.8f,s=sinf(th)*gl()*.8f;V o=wp(sel);
 return a==0?o+V{0,c,s}:a==1?o+V{s,0,c}:o+V{c,s,0};}
int hitGizmo(float x,float y){
 if(sel<0||tool>2)return -1;
 V o=wp(sel);float ox,oy;if(!prj(o,ox,oy))return -1;
 int best=-1;float bd=48;
 for(int a=0;a<3;a++){float d=1e9f;int bi=0;
  if(tool==1){for(int i=0;i<32;i++){float px,py;if(prj(ringPt(a,i),px,py)){float dd=hypotf(px-x,py-y);if(dd<d){d=dd;bi=i;}}}}
  else{float ex,ey;if(prj(o+AX[a]*gl(),ex,ey)){float vx=ex-ox,vy=ey-oy,t=std::clamp(((x-ox)*vx+(y-oy)*vy)/(vx*vx+vy*vy+1e-3f),0.f,1.f);d=hypotf(x-ox-vx*t,y-oy-vy*t);}}
  if(d<bd){bd=d;best=a;gIdx=bi;}}
 return best;}
void drawGizmo(){
 V p=wp(sel);float L=gl(),th=L*.025f;
 auto box=[&](V c,V sc,const float*k,int ms=0){T t;t.p=c;t.s=sc;M m=model(t);draw(meshes[ms],mul(gVP,m),m,k[0],k[1],k[2],1,0);};
 static const float Y[3]={1,.9f,.2f};
 for(int a=0;a<3;a++){const float*k=(gDrag&&a==gAxis)?Y:AC[a];
  if(tool==1){const int NS=72;float R=L*.8f,tk=th*2.f;V pts[NS+1];V ce=camEye();
   for(int i=0;i<=NS;i++){float an=i*2*PI/NS,c=cosf(an)*R,s=sinf(an)*R;pts[i]=a==0?p+V{0,c,s}:a==1?p+V{s,0,c}:p+V{c,s,0};}
   for(int i=0;i<NS;i++){V d=pts[i+1]-pts[i];float sl=sqrtf(dot(d,d));V e1=d*(1/sl),mid=(pts[i]+pts[i+1])*.5f,rd=norm(mid-p),e3=norm(cross(e1,rd));
    M m={};float*q=m.m;q[0]=e1.x*sl*1.2f;q[1]=e1.y*sl*1.2f;q[2]=e1.z*sl*1.2f;q[4]=rd.x*tk;q[5]=rd.y*tk;q[6]=rd.z*tk;q[8]=e3.x*tk;q[9]=e3.y*tk;q[10]=e3.z*tk;q[12]=mid.x;q[13]=mid.y;q[14]=mid.z;q[15]=1;
    float al=dot(mid-p,ce-p)<0?.3f:1.f;draw(meshes[0],mul(gVP,m),m,k[0],k[1],k[2],al,0);}}
  else{V sc{th,th,th};(&sc.x)[a]=L;box(p+AX[a]*(L*.5f),sc,k);float e=th*(tool==2?7.f:4.f);box(p+AX[a]*L,V{e,e,e},k,1);}}}
bool gUiQml=true;
struct ND{float x,y,z;int a;bool pos;};
float navR(){return H*.1f;} float navX(){return W-ML()-navR()-H*.01f;} float navY(){return H*(gUiQml?.105f:.072f)+navR()+H*.02f;}
void navPts(ND*d){V f=norm(tgt-camEye()),s=norm(cross(f,V{0,1,0})),u=cross(s,f);float R=navR();
 for(int i=0;i<6;i++){int a=i%3;V v=AX[a]*(i<3?1.f:-1.f);d[i]={navX()+dot(v,s)*R*.72f,navY()-dot(v,u)*R*.72f,dot(v,f),a,i<3};}}
void drawNav(){ND d[6];navPts(d);float R=navR(),cx=navX(),cy=navY();
 rect(cx-R-6,cy-R-6,2*R+12,2*R+12,0,0,0,.3f,R+6);
 std::sort(d,d+6,[](const ND&p,const ND&q){return p.z>q.z;});
 static const char*LB[3]={"X","Y","Z"};
 for(auto&e:d){const float*c=AC[e.a];float k=e.pos?1.f:.5f,r=e.pos?H*.024f:H*.017f;
  if(e.pos)for(int j=1;j<6;j++)rect(cx+(e.x-cx)*j/6-2,cy+(e.y-cy)*j/6-2,4,4,c[0],c[1],c[2],1,2);
  rect(e.x-r,e.y-r,2*r,2*r,c[0]*k,c[1]*k,c[2]*k,1,r);
  if(e.pos){float ps=H*.0055f;text(LB[e.a],e.x-tw(LB[e.a],ps)/2,e.y-2.5f*ps,ps,1,1,1);}}}
// ---------- exportacao (MAD / glTF) + rigging ----------
bool parentMode=false;std::string gToast;float gToastT=0;const char*gDir=".";std::string gProj="projeto";bool gNameOpen=false,gKbShift=false;int gNameMode=0,gMode=0;std::string gNameBuf;bool gCanUndo=false,gCanRedo=false,gSceneCU=false,gSceneCR=false,gSizeOpen=false;int gSRad=70;float gSStr=.5f;int gSizeField=0,gMcCW=0,gMcCH=0;std::string gSizeBuf[2];
void toast(const std::string&t){gToast=t;gToastT=4;}
std::string fm(const char*f,...){char b[1024];va_list a;va_start(a,f);vsnprintf(b,1024,f,a);va_end(a);return b;}
// ---------- control rig: criar / alternar IK-FK / bake / remover ----------
void rigSnapIK(Rig&g){ // controles IK vao para onde a pose FK esta (evita salto ao trocar FK->IK)
 M Wc=worldM(g.c);Obj&t=objs[g.tgt];t.cur.p=V{Wc.m[12],Wc.m[13],Wc.m[14]};t.cur.r=decomp(Wc).r;
 if(g.pole>=0&&g.pole<(int)objs.size()){V A=wp(g.a),B=wp(g.b),C=wp(g.c);V u=norm(C-A);V pv=rigPoleDir(g.a,g.b,g.c);
  objs[g.pole].cur.p=B+pv*((vlen(B-A)+vlen(C-B))*.7f);}}
void rigCreate(){
 if(sel<0){toast("Selecione o osso final (mao ou pe)");return;}
 int c=sel;if(rigOf(c)>=0){toast("Esse osso ja faz parte de um IK");return;}
 int b=objs[c].parent,a=b>=0?objs[b].parent:-1;
 if(b<0||a<0){toast("Precisa de 2 ossos acima (ex.: braco > antebraco > mao)");return;}
 V A=wp(a),B=wp(b),C=wp(c);float l2=vlen(C-B),l1=vlen(B-A);float r=std::max(.04f,l2*.28f);
 Obj t;t.mesh=1;t.kind=1;t.col={.25f,.6f,1.f};snprintf(t.nm,24,"IK.%.18s",objs[c].nm);t.cur.p=C;t.cur.r=decomp(worldM(c)).r;t.cur.s={2*r,2*r,2*r};
 Obj p;p.mesh=1;p.kind=2;p.col={.3f,.9f,.4f};snprintf(p.nm,24,"Pole.%.16s",objs[c].nm);p.cur.s={1.4f*r,1.4f*r,1.4f*r};
 V pv=rigPoleDir(a,b,c);p.cur.p=B+pv*((l1+l2)*.7f);p.shape=3;
 objs.push_back(t);int ti=(int)objs.size()-1;objs.push_back(p);int pi=(int)objs.size()-1;
 Rig g;g.a=a;g.b=b;g.c=c;g.tgt=ti;g.pole=pi;rigs.push_back(g);sel=ti;toast("IK criado: mova o controle azul");}
void rigRemove(int ri){int t=rigs[ri].tgt,p=rigs[ri].pole;rigs.erase(rigs.begin()+ri);
 int hi=std::max(t,p),lo=std::min(t,p);if(hi>=0&&hi<(int)objs.size())delObj(hi);if(lo>=0&&lo<(int)objs.size()&&lo!=hi)delObj(lo);sel=-1;toast("IK removido");}
void rigToggle(int ri){Rig&g=rigs[ri];int ids[3]={g.a,g.b,g.c};float w=rigW(g);
 if(w>=.5f){for(int k=0;k<3;k++){Obj&o=objs[ids[k]];g.base[k]=o.cur.r;if(!o.k.empty())setKeyAt(o,tm,o.cur);}g.w=0;if(!g.wk.empty())rigKeyW(g,0);toast("Modo FK");}
 else{rigSnapIK(g);g.w=1;if(!g.wk.empty())rigKeyW(g,1);toast("Modo IK");}}
void rigBake(int ri){Rig&g=rigs[ri];int ids[3]={g.a,g.b,g.c};int f0=std::min(gFS,gFE),f1=std::max(gFS,gFE);float t0=tm;
 std::vector<float> ts;std::vector<T> ps;
 for(int f=f0;f<=f1;f++){tm=f/24.f;applyAnim();solveRigs();ts.push_back(tm);for(int k=0;k<3;k++)ps.push_back(objs[ids[k]].cur);}
 for(size_t i=0;i<ts.size();i++)for(int k=0;k<3;k++)setKeyAt(objs[ids[k]],ts[i],ps[i*3+k]);
 g.w=0;g.wk.clear();tm=t0;applyAnim();toast(fm("IK convertido em chaves FK (%d frames)",(int)ts.size()));}
// ---------- camera: criar / ver pela camera ----------
void addCamera(){V e=camEye(),f=norm(tgt-e),r=norm(cross(f,V{0,1,0})),u=cross(r,f);
 M m=id();m.m[0]=r.x;m.m[1]=r.y;m.m[2]=r.z;m.m[4]=u.x;m.m[5]=u.y;m.m[6]=u.z;m.m[8]=-f.x;m.m[9]=-f.y;m.m[10]=-f.z;m.m[12]=e.x;m.m[13]=e.y;m.m[14]=e.z;
 T t=decomp(m);int n=0;for(auto&q:objs)if(q.kind==3)n++;
 Obj o;o.mesh=1;o.kind=3;o.col={.85f,.85f,.9f};if(n)snprintf(o.nm,24,"Camera.%03d",n);else snprintf(o.nm,24,"Camera");
 o.cur.p=t.p;o.cur.r=t.r;o.cur.s={.5f,.5f,.5f};o.fov=FOV;
 objs.push_back(o);sel=(int)objs.size()-1;if(gCamAct<0||activeCam()<0)gCamAct=sel;toast("Camera criada na vista atual. Toque nela de novo para ver pela camera");}
void addCameraAt(V e,V tg){V f=norm(tg-e),r=norm(cross(f,V{0,1,0})),u=cross(r,f);
 M m=id();m.m[0]=r.x;m.m[1]=r.y;m.m[2]=r.z;m.m[4]=u.x;m.m[5]=u.y;m.m[6]=u.z;m.m[8]=-f.x;m.m[9]=-f.y;m.m[10]=-f.z;m.m[12]=e.x;m.m[13]=e.y;m.m[14]=e.z;
 T t=decomp(m);Obj o;o.mesh=1;o.kind=3;o.col={.85f,.85f,.9f};snprintf(o.nm,24,"Camera");
 o.cur.p=t.p;o.cur.r=t.r;o.cur.s={.5f,.5f,.5f};o.fov=FOV;objs.push_back(o);sel=(int)objs.size()-1;if(gCamAct<0||activeCam()<0)gCamAct=sel;}
void camViewToggle(){if(gCamView>=0){camViewOff();toast("Vista normal");return;}
 int i=(sel>=0&&objs[sel].kind==3)?sel:activeCam();if(i<0){toast("Crie uma camera primeiro (icone de camera)");return;}
 camViewOn(i);toast(fm("Vendo pela camera: %s",objs[i].nm));}
void insertKey(){if(sel<0)return;Obj&o=objs[sel];bool f=false;for(auto&k:o.k)if(fabsf(k.t-tm)<.02f){k.x=o.cur;f=true;}
 if(!f){o.k.push_back({tm,o.cur});std::sort(o.k.begin(),o.k.end(),[](const Key&a,const Key&b){return a.t<b.t;});}}
void setParent(int c,int p){for(int a=p;a>=0;a=objs[a].parent)if(a==c){toast("Nao pode: ciclo");return;}
 M w=worldM(c);objs[c].parent=p;objs[c].cur=decomp(mul(invAff(worldM(p)),w));toast("Parent definido");}
void finishParent(int h){parentMode=false;if(sel>=0&&h>=0&&h!=sel)setParent(sel,h);else toast("Cancelado");}
FILE*openOut(const char*ext,std::string&path,bool&fb){long t=(long)time(nullptr);fb=false;
 path=fm("/storage/emulated/0/Download/nomad_%ld.%s",t,ext);FILE*f=fopen(path.c_str(),"wb");
 if(!f){path=fm("%s/nomad_%ld.%s",gDir,t,ext);f=fopen(path.c_str(),"wb");fb=true;}return f;}
void saved(const std::string&path,bool fb){toast("Salvo: "+path+(fb?"  (ative Acesso a todos os arquivos para salvar em Downloads)":""));}
void writeMAD(FILE*f){
 auto U_=[&](uint32_t v){fwrite(&v,4,1,f);};auto Fl=[&](float v){fwrite(&v,4,1,f);};
 fwrite("MAD2",1,4,f);U_(2);Fl(24);Fl(DUR);U_((uint32_t)(meshes.size()-6));
 for(size_t i=6;i<meshes.size();i++){Mesh&m=meshes[i];U_((uint32_t)m.cpu.size());fwrite(m.cpu.data(),4,m.cpu.size(),f);Fl(m.rad);U_((uint32_t)m.skin.size());if(!m.skin.empty())fwrite(m.skin.data(),4,m.skin.size(),f);}
 U_((uint32_t)skins.size());
 for(auto&sk:skins){U_((uint32_t)sk.j.size());for(size_t k=0;k<sk.j.size();k++){U_((uint32_t)sk.j[k]);fwrite(sk.ibm[k].m,4,16,f);const M fz=k<sk.fz.size()?sk.fz[k]:id();fwrite(fz.m,4,16,f);}}
 U_((uint32_t)objs.size());
 for(auto&o:objs){U_((uint32_t)o.mesh);U_((uint32_t)o.parent);U_((uint32_t)o.skin);fwrite(o.nm,1,24,f);Fl(o.len);Fl(o.met);Fl(o.rou);Fl(o.col.x);Fl(o.col.y);Fl(o.col.z);
  const float*c=&o.cur.p.x;for(int i=0;i<9;i++)Fl(c[i]);U_((uint32_t)o.k.size());for(auto&k:o.k){Fl(k.t);const float*q=&k.x.p.x;for(int i=0;i<9;i++)Fl(q[i]);}}
 Fl(yaw);Fl(pitch);Fl(cd);Fl(tgt.x);Fl(tgt.y);Fl(tgt.z);Fl(tm);
 fwrite("RIG3",1,4,f);U_((uint32_t)objs.size());for(auto&o:objs)U_((uint32_t)o.kind);for(auto&o:objs)U_((uint32_t)o.shape);for(auto&o:objs)Fl(o.fov);U_((uint32_t)rigs.size());
 for(auto&g:rigs){U_((uint32_t)g.a);U_((uint32_t)g.b);U_((uint32_t)g.c);U_((uint32_t)g.tgt);U_((uint32_t)g.pole);Fl(g.w);U_(g.rotc?1u:0u);U_((uint32_t)g.wk.size());for(auto&k:g.wk){Fl(k.first);Fl(k.second);}}}
FILE*openOutName(const std::string&nm,const char*ext,std::string&path,bool&fb){fb=false;
 path=fm("/storage/emulated/0/Download/%s.%s",nm.c_str(),ext);FILE*f=fopen(path.c_str(),"wb");
 if(!f){path=fm("%s/%s.%s",gDir,nm.c_str(),ext);f=fopen(path.c_str(),"wb");fb=true;}return f;}
void exportMAD(){std::string path;bool fb;FILE*f=openOut("mad",path,fb);if(!f){toast("Erro ao salvar");return;}writeMAD(f);fclose(f);saved(path,fb);}
void saveProject(){std::string path;bool fb;FILE*f=openOutName(gProj,"mad",path,fb);if(!f){toast("Erro ao salvar");return;}writeMAD(f);fclose(f);saved(path,fb);}
std::string b64(const std::vector<unsigned char>&d){static const char*AB="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";std::string o;
 for(size_t i=0;i<d.size();i+=3){unsigned v=d[i]<<16;if(i+1<d.size())v|=d[i+1]<<8;if(i+2<d.size())v|=d[i+2];
  o+=AB[v>>18&63];o+=AB[v>>12&63];o+=i+1<d.size()?AB[v>>6&63]:'=';o+=i+2<d.size()?AB[v&63]:'=';}return o;}
void qm(const float*a,const float*b,float*o){o[0]=a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1];o[1]=a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0];
 o[2]=a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3];o[3]=a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2];}
void eq(V r,float*q){float a[4]={sinf(r.x/2),0,0,cosf(r.x/2)},b[4]={0,sinf(r.y/2),0,cosf(r.y/2)},c[4]={0,0,sinf(r.z/2),cosf(r.z/2)},t[4];qm(b,a,t);qm(c,t,q);}
std::string mm3(const std::vector<float>&v){float lo[3]={1e30f,1e30f,1e30f},hi[3]={-1e30f,-1e30f,-1e30f};
 for(size_t i=0;i+2<v.size();i+=3)for(int c=0;c<3;c++){lo[c]=std::min(lo[c],v[i+c]);hi[c]=std::max(hi[c],v[i+c]);}
 return fm(",\"min\":[%g,%g,%g],\"max\":[%g,%g,%g]",lo[0],lo[1],lo[2],hi[0],hi[1],hi[2]);}
void exportGLTF(){
 if(objs.empty()){toast("Cena vazia");return;}
 std::string path;bool fb;FILE*f=openOut("gltf",path,fb);if(!f){toast("Erro ao salvar");return;}
 std::vector<unsigned char> bin;std::string views,accs,meshJ,mats,nodes,samp,chn,roots;int nv=0,na=0,nm=0,ns=0;
 auto view=[&](const void*d,size_t n){while(bin.size()%4)bin.push_back(0);size_t off=bin.size();const unsigned char*c=(const unsigned char*)d;bin.insert(bin.end(),c,c+n);
  views+=fm("%s{\"buffer\":0,\"byteOffset\":%zu,\"byteLength\":%zu}",nv?",":"",off,n);return nv++;};
 auto acc=[&](const std::vector<float>&v,int comps,const char*ty,const std::string&mm){int vi=view(v.data(),v.size()*4);
  accs+=fm("%s{\"bufferView\":%d,\"componentType\":5126,\"count\":%d,\"type\":\"%s\"%s}",na?",":"",vi,(int)v.size()/comps,ty,mm.c_str());return na++;};
 std::vector<int> pa(meshes.size(),-1),pn(meshes.size(),-1),pj(meshes.size(),-1),pw(meshes.size(),-1);
 for(int i=0;i<(int)objs.size();i++){Obj&o=objs[i];int mj=-1;bool bone=o.mesh==5;
  if((o.mesh<3||o.mesh>=6)&&!o.kind){int m=o.mesh;
   if(pa[m]<0){std::vector<float> P,N;auto&c=meshes[m].cpu;for(size_t j=0;j+5<c.size();j+=6){P.insert(P.end(),{c[j],c[j+1],c[j+2]});N.insert(N.end(),{c[j+3],c[j+4],c[j+5]});}
    pa[m]=acc(P,3,"VEC3",mm3(P));pn[m]=acc(N,3,"VEC3","");
    if(meshes[m].skinned){std::vector<unsigned short> Jv;std::vector<float> Wv;auto&sk=meshes[m].skin;
     for(size_t j=0;j+7<sk.size();j+=8){for(int q=0;q<4;q++)Jv.push_back((unsigned short)sk[j+q]);for(int q=0;q<4;q++)Wv.push_back(sk[j+4+q]);}
     int vi=view(Jv.data(),Jv.size()*2);accs+=fm("%s{\"bufferView\":%d,\"componentType\":5123,\"count\":%d,\"type\":\"VEC4\"}",na?",":"",vi,(int)Jv.size()/4);pj[m]=na++;pw[m]=acc(Wv,4,"VEC4","");}}
   mats+=fm("%s{\"pbrMetallicRoughness\":{\"baseColorFactor\":[%g,%g,%g,1],\"metallicFactor\":%g,\"roughnessFactor\":%g}}",nm?",":"",o.col.x,o.col.y,o.col.z,o.met,o.rou);
   std::string at=fm("\"POSITION\":%d,\"NORMAL\":%d",pa[m],pn[m]);if(pj[m]>=0)at+=fm(",\"JOINTS_0\":%d,\"WEIGHTS_0\":%d",pj[m],pw[m]);
   meshJ+=fm("%s{\"primitives\":[{\"attributes\":{%s},\"material\":%d}]}",nm?",":"",at.c_str(),nm);mj=nm++;}
  float q[4];eq(o.cur.r,q);std::string ch;
  for(int j=0;j<(int)objs.size();j++)if(objs[j].parent==i)ch+=fm("%s%d",ch.empty()?"":",",j);
  std::string n=fm("{\"name\":\"%s\",\"translation\":[%g,%g,%g],\"rotation\":[%g,%g,%g,%g],\"scale\":[%g,%g,%g]",o.nm,
   o.cur.p.x,o.cur.p.y,o.cur.p.z,q[0],q[1],q[2],q[3],o.cur.s.x,o.cur.s.y,o.cur.s.z);
  if(mj>=0)n+=fm(",\"mesh\":%d",mj);if(mj>=0&&o.skin>=0&&meshes[o.mesh].skinned)n+=fm(",\"skin\":%d",o.skin);if(bone)n+=fm(",\"extras\":{\"bone\":true,\"length\":%g}",o.len);
  if(!ch.empty())n+=",\"children\":["+ch+"]";n+="}";nodes+=(i?",":"")+n;
  if(o.parent<0)roots+=fm("%s%d",roots.empty()?"":",",i);}
 for(int i=0;i<(int)objs.size();i++){Obj&o=objs[i];if(o.k.empty())continue;std::vector<float> ti,tp,rq,sc;
  for(auto&k:o.k){ti.push_back(k.t);tp.insert(tp.end(),{k.x.p.x,k.x.p.y,k.x.p.z});float q[4];eq(k.x.r,q);rq.insert(rq.end(),{q[0],q[1],q[2],q[3]});sc.insert(sc.end(),{k.x.s.x,k.x.s.y,k.x.s.z});}
  int ai=acc(ti,1,"SCALAR",fm(",\"min\":[%g],\"max\":[%g]",ti.front(),ti.back()));
  int ao[3]={acc(tp,3,"VEC3",""),acc(rq,4,"VEC4",""),acc(sc,3,"VEC3","")};static const char*PN[3]={"translation","rotation","scale"};
  for(int c=0;c<3;c++){samp+=fm("%s{\"input\":%d,\"output\":%d,\"interpolation\":\"LINEAR\"}",ns?",":"",ai,ao[c]);
   chn+=fm("%s{\"sampler\":%d,\"target\":{\"node\":%d,\"path\":\"%s\"}}",ns?",":"",ns,i,PN[c]);ns++;}}
 std::string js="{\"asset\":{\"version\":\"2.0\",\"generator\":\"Nomad Animator\"},\"scene\":0,\"scenes\":[{\"nodes\":["+roots+"]}],\"nodes\":["+nodes+"],";
 std::string skinJ;
 for(size_t si=0;si<skins.size();si++){Skin&sk=skins[si];std::vector<float> ib;std::string jl;
  for(size_t k=0;k<sk.j.size();k++){const float*mp=sk.ibm[k].m;ib.insert(ib.end(),mp,mp+16);jl+=(k?",":"")+std::to_string(std::max(sk.j[k],0));}
  int ai=acc(ib,16,"MAT4","");skinJ+=(si?",":"")+std::string("{\"inverseBindMatrices\":")+std::to_string(ai)+",\"joints\":["+jl+"]}";}
 if(!skinJ.empty())js+="\"skins\":["+skinJ+"],";
 if(nm)js+="\"meshes\":["+meshJ+"],\"materials\":["+mats+"],";
 if(ns)js+="\"animations\":[{\"name\":\"Anim\",\"samplers\":["+samp+"],\"channels\":["+chn+"]}],";
 js+="\"accessors\":["+accs+"],\"bufferViews\":["+views+"],\"buffers\":[{\"byteLength\":"+std::to_string(bin.size())+",\"uri\":\"data:application/octet-stream;base64,"+b64(bin)+"\"}]}";
 fwrite(js.data(),1,js.size(),f);fclose(f);saved(path,fb);}
// ---------- icones, importacao glTF ----------
Rc gListR={0,0,0,0};float gScrollPx=0,gSY0=0,gSS0=0,gRowH=1;
float gFlash[64]={};bool gTlQml=true;bool gFileMenu=false,gRenderOpen=false,gRigOpen=false,gTlOpen=false,gToolFan=false,gAddFan=false;bool gImportOpen=false;int gPick=0;std::vector<std::string> gFiles;
void icon2(int id,float cx,float cy,float s,bool on){
 if(id>=3&&id<=6){icon(id,cx,cy,s,on);return;}
 const float c=.93f;
 if(id==0){rect(cx-s*.35f,cy-s,s*1.4f,s*1.4f,.55f,.55f,.6f,1,s*.12f);rect(cx-s,cy-s*.4f,s*1.4f,s*1.4f,c,c,c,1,s*.12f);}
 else if(id==1){rect(cx-s,cy-s,2*s,2*s,c,c,c,1,s);rect(cx-s*.5f,cy-s*.6f,s*.5f,s*.5f,.6f,.6f,.65f,1,s*.25f);}
 else if(id==2){rect(cx-s*1.1f,cy-s*.3f,s*2.2f,s*.6f,c,c,c,1,s*.12f);}
 else if(id==10){rect(cx-s*.12f,cy-s*.7f,s*.24f,s*1.4f,c,c,c);rect(cx-s*.4f,cy+s*.3f,s*.8f,s*.8f,c,c,c,1,s*.4f);rect(cx-s*.25f,cy-s,s*.5f,s*.5f,c,c,c,1,s*.25f);}
 else if(id==11||id==12){rect(cx-s*.35f,cy-s,s*.7f,s*.7f,c,c,c,1,s*.35f);rect(cx-s*.12f,cy-s*.35f,s*.24f,s*.6f,c,c,c);rect(cx-s*.35f,cy+s*.3f,s*.7f,s*.7f,c,c,c,1,s*.35f);
  if(id==12)rect(cx-s*.9f,cy-s*.12f,s*1.8f,s*.24f,1,.45f,.4f);}
 else if(id==8){if(playing){rect(cx-s*.7f,cy-s,s*.55f,2*s,c,c,c,1,s*.1f);rect(cx+s*.15f,cy-s,s*.55f,2*s,c,c,c,1,s*.1f);}
  else for(int i=0;i<8;i++){float h=2*s*(1-i/8.f);rect(cx-s*.7f+i*s*.2f,cy-h/2,s*.22f,h,c,c,c);}}
 else if(id==40){rect(cx-s*.7f,cy-s,s*1.4f,s*2.f,c,c,c,1,s*.15f);rect(cx-s*.45f,cy-s*.1f,s*.9f,s*.2f,.3f,.3f,.33f);rect(cx-s*.1f,cy-s*.45f,s*.2f,s*.9f,.3f,.3f,.33f);}
 else if(id==41){rect(cx-s,cy-s*.75f,s*.8f,s*.4f,c,c,c,1,s*.1f);rect(cx-s,cy-s*.4f,s*2.f,s*1.3f,c,c,c,1,s*.12f);}
 else if(id==42){rect(cx-s*.9f,cy-s*.9f,s*1.8f,s*1.8f,c,c,c,1,s*.15f);rect(cx-s*.55f,cy-s*.9f,s*1.1f,s*.6f,.3f,.3f,.33f,1,s*.06f);rect(cx-s*.5f,cy+s*.2f,s,s*.7f,.3f,.3f,.33f,1,s*.06f);}
 else if(id==9){rect(cx-s*.8f,cy-s,s*1.6f,s*.25f,c,c,c);rect(cx-s*.3f,cy-s*1.2f,s*.6f,s*.25f,c,c,c);rect(cx-s*.65f,cy-s*.65f,s*1.3f,s*1.7f,c,c,c,1,s*.15f);}
 else if(id==91||id==92){bool en=id==91?gCanUndo:gCanRedo;float cc=en?c:.42f,sg=id==92?-1.f:1.f;const float R=s*.78f,rd=s*.17f;
  for(int k=0;k<=10;k++){float a=(20-k*22)*PI/180.f;float qx=cx+sg*R*cosf(a),qy=cy+s*.15f+R*sinf(a);rect(qx-rd,qy-rd,2*rd,2*rd,cc,cc,cc,1,rd);}
  float hx=cx+sg*R*cosf(-200*PI/180.f),hy=cy+s*.15f+R*sinf(-200*PI/180.f);
  for(int q=0;q<3;q++){float w=s*(1.5f-q*.5f);rect(hx-w/2,hy+s*.05f+q*s*.3f,w,s*.3f,cc,cc,cc);}}
 else if(id==63){rect(cx-s*.28f,cy-s*1.05f,s*.56f,s*.56f,c,c,c,1,s*.28f);rect(cx-s*.1f,cy-s*.4f,s*.2f,s*.95f,c,c,c);rect(cx-s*.75f,cy-s*.3f,s*1.5f,s*.18f,c,c,c);rect(cx-s*.5f,cy+s*.5f,s*.2f,s*.9f,c,c,c);rect(cx+s*.3f,cy+s*.5f,s*.2f,s*.9f,c,c,c);}
 else if(id==18){rect(cx-s*.95f,cy-s*.5f,s*1.5f,s*1.1f,c,c,c,1,s*.15f);rect(cx-s*.65f,cy-s*.8f,s*.6f,s*.35f,c,c,c,1,s*.1f);rect(cx+s*.55f,cy-s*.3f,s*.55f,s*.7f,c,c,c,1,s*.1f);rect(cx-s*.55f,cy-s*.2f,s*.5f,s*.5f,.3f,.3f,.33f,1,s*.25f);}
 else if(id==19){rect(cx-s,cy-s*.75f,s*2.f,s*1.5f,c,c,c,1,s*.15f);rect(cx-s*.8f,cy-s*.55f,s*1.6f,s*1.1f,.3f,.3f,.33f,1,s*.1f);rect(cx-s*.3f,cy-s*.3f,s*.6f,s*.6f,c,c,c,1,s*.3f);}}
// ---------- permissao "Acesso a todos os arquivos" (via JNI) ----------
android_app*gApp=nullptr;
bool filesGranted(){
 if(!gApp||!gApp->activity)return true;ShimVM*vm=gApp->activity->vm;JNIEnv*env=nullptr;
 if(vm->AttachCurrentThread(&env,nullptr)!=JNI_OK||!env)return true;
 bool ok=true;jclass ec=env->FindClass("android/os/Environment");
 if(ec){jmethodID m=env->GetStaticMethodID(ec,"isExternalStorageManager","()Z");
  if(m)ok=env->CallStaticBooleanMethod(ec,m)!=JNI_FALSE;}
 if(env->ExceptionCheck())env->ExceptionClear();
 vm->DetachCurrentThread();return ok;}
void openFilesSettings(){
 if(!gApp||!gApp->activity)return;ShimVM*vm=gApp->activity->vm;JNIEnv*env=nullptr;
 if(vm->AttachCurrentThread(&env,nullptr)!=JNI_OK||!env)return;
 jobject act=gApp->activity->clazz;
 auto go=[&](const char*action,bool withPkg)->bool{
  jclass ic=env->FindClass("android/content/Intent");if(!ic){env->ExceptionClear();return false;}
  jmethodID ct=env->GetMethodID(ic,"<init>","(Ljava/lang/String;)V");
  jobject it=env->NewObject(ic,ct,env->NewStringUTF(action));
  if(withPkg){jclass uc=env->FindClass("android/net/Uri");
   jmethodID pr=env->GetStaticMethodID(uc,"parse","(Ljava/lang/String;)Landroid/net/Uri;");
   jobject uri=env->CallStaticObjectMethod(uc,pr,env->NewStringUTF("package:com.nomad.animator"));
   jmethodID sd=env->GetMethodID(ic,"setData","(Landroid/net/Uri;)Landroid/content/Intent;");env->CallObjectMethod(it,sd,uri);}
  jclass ac=env->GetObjectClass(act);jmethodID sa=env->GetMethodID(ac,"startActivity","(Landroid/content/Intent;)V");
  env->CallVoidMethod(act,sa,it);
  if(env->ExceptionCheck()){env->ExceptionClear();return false;}return true;};
 if(!go("android.settings.MANAGE_APP_ALL_FILES_ACCESS_PERMISSION",true))go("android.settings.MANAGE_ALL_FILES_ACCESS_PERMISSION",false);
 vm->DetachCurrentThread();}
void needFiles(){toast("Ative Permitir gerenciar todos os arquivos e volte ao app");openFilesSettings();}
void importGLTF(const std::string&path);
// ZIPBEGIN
static uint32_t rd32(const unsigned char*q){return q[0]|(q[1]<<8)|(q[2]<<16)|((uint32_t)q[3]<<24);}
static uint32_t rd16(const unsigned char*q){return q[0]|(q[1]<<8);}
static void mkdirs(const std::string&path){for(size_t i=1;i<path.size();i++)if(path[i]=='/')mkdir(path.substr(0,i).c_str(),0755);}
std::string unzipModel(const std::string&zp){
 FILE*f=fopen(zp.c_str(),"rb");if(!f)return "";
 fseek(f,0,SEEK_END);long sz=ftell(f);fseek(f,0,SEEK_SET);if(sz<22){fclose(f);return "";}
 std::vector<unsigned char> z(sz);size_t rd=fread(z.data(),1,sz,f);fclose(f);if((long)rd!=sz)return "";
 long e=-1;for(long i=sz-22;i>=0&&i>=sz-22-65535;i--)if(rd32(&z[i])==0x06054b50){e=i;break;}
 if(e<0)return "";
 int n=(int)rd16(&z[e+10]);uint32_t p=rd32(&z[e+16]);
 std::string nm=zp;size_t sl=nm.rfind('/');if(sl!=std::string::npos)nm=nm.substr(sl+1);size_t dt=nm.rfind('.');if(dt!=std::string::npos)nm=nm.substr(0,dt);
 std::string out=std::string(gDir)+"/import_"+nm;mkdir(out.c_str(),0755);std::string found;
 for(int i=0;i<n;i++){if((long)p+46>sz||rd32(&z[p])!=0x02014b50)break;
  int method=(int)rd16(&z[p+10]);uint32_t csz=rd32(&z[p+20]),usz=rd32(&z[p+24]);uint32_t nl=rd16(&z[p+28]),el=rd16(&z[p+30]),cl=rd16(&z[p+32]),lo=rd32(&z[p+42]);
  if((long)(p+46+nl)>sz)break;std::string name((const char*)&z[p+46],nl);p+=46+nl+el+cl;
  std::string l=name;for(auto&c:l)c=(char)tolower((unsigned char)c);
  auto ends=[&](const char*x){size_t k=strlen(x);return l.size()>k&&l.compare(l.size()-k,k,x)==0;};
  bool isg=ends(".gltf")||ends(".glb");
  if(!(isg||ends(".bin")||ends(".png")||ends(".jpg")||ends(".jpeg"))||name.find("..")!=std::string::npos||name[0]=='/')continue;
  if((long)lo+30>sz||rd32(&z[lo])!=0x04034b50)continue;
  uint32_t ds=lo+30+rd16(&z[lo+26])+rd16(&z[lo+28]);if((long)ds+(long)csz>sz)continue;
  std::vector<unsigned char> o(usz?usz:1);
  if(method==0){if(csz!=usz)continue;memcpy(o.data(),&z[ds],usz);}
  else if(method==8){z_stream zs;memset(&zs,0,sizeof zs);if(inflateInit2(&zs,-MAX_WBITS)!=Z_OK)continue;
   zs.next_in=&z[ds];zs.avail_in=csz;zs.next_out=o.data();zs.avail_out=usz;int r=inflate(&zs,Z_FINISH);inflateEnd(&zs);if(r!=Z_STREAM_END)continue;}
  else continue;
  std::string fp=out+"/"+name;mkdirs(fp);FILE*w=fopen(fp.c_str(),"wb");if(!w)continue;fwrite(o.data(),1,usz,w);fclose(w);
  if(isg&&found.empty())found=fp;}
 return found;}
// ZIPEND
void importPath(const std::string&p){std::string l=p;for(auto&c:l)c=(char)tolower((unsigned char)c);
 if(l.size()>4&&l.compare(l.size()-4,4,".zip")==0){std::string g=unzipModel(p);if(g.empty()){toast("O zip nao tem .gltf/.glb");return;}importGLTF(g);}else importGLTF(p);}
void scanDir(const std::string&dp,int depth,std::vector<std::pair<time_t,std::string>>&v){DIR*d=opendir(dp.c_str());if(!d)return;
 while(dirent*e=readdir(d)){std::string n=e->d_name;if(n=="."||n=="..")continue;std::string pth=dp+"/"+n;struct stat st;if(stat(pth.c_str(),&st)!=0)continue;
  if(S_ISDIR(st.st_mode)){if(depth>0&&n.compare(0,7,"import_")!=0)scanDir(pth,depth-1,v);continue;}
  std::string l=n;for(auto&ch:l)ch=(char)tolower((unsigned char)ch);
  auto ends=[&](const char*x){size_t k=strlen(x);return l.size()>k&&l.compare(l.size()-k,k,x)==0;};
  if(gPick==0?(ends(".gltf")||ends(".glb")||ends(".zip")):ends(".mad"))v.push_back({st.st_mtime,pth});}
 closedir(d);}
void scanFiles(){gFiles.clear();std::vector<std::pair<time_t,std::string>> v;
 scanDir("/storage/emulated/0/Download",1,v);scanDir(gDir,1,v);
 std::sort(v.rbegin(),v.rend());for(size_t i=0;i<v.size()&&i<200;i++)gFiles.push_back(v[i].second);}
// ---------- texturas (array 1024x1024, ate 16 imagens) ----------
const int TEXN=1024,TEXMAX=16;GLuint gTexArr=0;std::vector<std::vector<unsigned char>> gTexCpu;
void initTexArr(){gTexArr=0;if(gTexCpu.empty())return;glActiveTexture(GL_TEXTURE0);glGenTextures(1,&gTexArr);glBindTexture(GL_TEXTURE_2D_ARRAY,gTexArr);
 glTexStorage3D(GL_TEXTURE_2D_ARRAY,11,GL_RGBA8,TEXN,TEXN,TEXMAX);
 glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_T,GL_REPEAT);
 for(size_t i=0;i<gTexCpu.size();i++)glTexSubImage3D(GL_TEXTURE_2D_ARRAY,0,0,0,(GLint)i,TEXN,TEXN,1,GL_RGBA,GL_UNSIGNED_BYTE,gTexCpu[i].data());
 glGenerateMipmap(GL_TEXTURE_2D_ARRAY);}
int addTexture(const unsigned char*data,int len){
 if(!data||len<8||(int)gTexCpu.size()>=TEXMAX)return -1;int w=0,h=0,c=0;unsigned char*px=stbi_load_from_memory(data,len,&w,&h,&c,4);
 if(!px||w<1||h<1){if(px)stbi_image_free(px);return -1;}
 std::vector<unsigned char> out((size_t)TEXN*TEXN*4);
 for(int y=0;y<TEXN;y++){float fy=(y+.5f)*h/TEXN-.5f;int y0=(int)floorf(fy);float ty=fy-y0;int ya=std::clamp(y0,0,h-1),yb=std::clamp(y0+1,0,h-1);
  for(int x=0;x<TEXN;x++){float fx=(x+.5f)*w/TEXN-.5f;int x0=(int)floorf(fx);float tx_=fx-x0;int xa=std::clamp(x0,0,w-1),xb=std::clamp(x0+1,0,w-1);
   const unsigned char*p00=px+((size_t)ya*w+xa)*4,*p10=px+((size_t)ya*w+xb)*4,*p01=px+((size_t)yb*w+xa)*4,*p11=px+((size_t)yb*w+xb)*4;
   for(int k=0;k<4;k++){float a=p00[k]+(p10[k]-p00[k])*tx_,b=p01[k]+(p11[k]-p01[k])*tx_;out[((size_t)y*TEXN+x)*4+k]=(unsigned char)std::clamp(a+(b-a)*ty+.5f,0.f,255.f);}}}
 stbi_image_free(px);gTexCpu.push_back(std::move(out));int layer=(int)gTexCpu.size()-1;
 if(!gTexArr)initTexArr();else{glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D_ARRAY,gTexArr);
  glTexSubImage3D(GL_TEXTURE_2D_ARRAY,0,0,0,layer,TEXN,TEXN,1,GL_RGBA,GL_UNSIGNED_BYTE,gTexCpu[layer].data());glGenerateMipmap(GL_TEXTURE_2D_ARRAY);}
 return layer;}
std::vector<unsigned char> b64dec(const std::string&s2){std::vector<unsigned char> o;int val=0,bits=-8;
 for(unsigned char ch:s2){int d;if(ch>='A'&&ch<='Z')d=ch-'A';else if(ch>='a'&&ch<='z')d=ch-'a'+26;else if(ch>='0'&&ch<='9')d=ch-'0'+52;else if(ch=='+'||ch=='-')d=62;else if(ch=='/'||ch=='_')d=63;else continue;
  val=(val<<6)+d;bits+=6;if(bits>=0){o.push_back((unsigned char)((val>>bits)&0xFF));bits-=8;}}return o;}
int gltfImage(cgltf_data*d,cgltf_image*im,const std::string&dir,std::vector<int>&cache){
 if(!im)return -1;size_t ix=(size_t)(im-d->images);if(ix>=cache.size())return -1;if(cache[ix]!=-2)return cache[ix];int L=-1;
 if(im->buffer_view&&im->buffer_view->buffer&&im->buffer_view->buffer->data){const unsigned char*bd=(const unsigned char*)im->buffer_view->buffer->data+im->buffer_view->offset;L=addTexture(bd,(int)im->buffer_view->size);}
 else if(im->uri){std::string u=im->uri;
  if(u.compare(0,5,"data:")==0){size_t c=u.find(',');if(c!=std::string::npos){std::vector<unsigned char> bin=b64dec(u.substr(c+1));L=addTexture(bin.data(),(int)bin.size());}}
  else{std::string dec;for(size_t i=0;i<u.size();i++){if(u[i]=='%'&&i+2<u.size()){dec+=(char)strtol(u.substr(i+1,2).c_str(),nullptr,16);i+=2;}else dec+=u[i];}
   FILE*f=fopen((dir+dec).c_str(),"rb");if(f){fseek(f,0,SEEK_END);long sz=ftell(f);fseek(f,0,SEEK_SET);
    if(sz>0&&sz<80000000){std::vector<unsigned char> b(sz);if(fread(b.data(),1,sz,f)==(size_t)sz)L=addTexture(b.data(),(int)sz);}fclose(f);}}}
 cache[ix]=L;return L;}
void importGLTF(const std::string&path){
 cgltf_options op;memset(&op,0,sizeof op);cgltf_data*d=nullptr;
 if(cgltf_parse_file(&op,path.c_str(),&d)!=cgltf_result_success){toast("Falha ao ler o arquivo");return;}
 if(cgltf_load_buffers(&op,d,path.c_str())!=cgltf_result_success){cgltf_free(d);toast("Falta o .bin: ele precisa estar na mesma pasta do .gltf");return;}
 std::string dir=path.substr(0,path.rfind('/')+1);std::vector<int> imgc(d->images_count,-2);
 struct PR{int mesh=5;V col{.8f,.8f,.8f};float met=0,rou=.6f;int tb=-1,tm=-1,tn=-1;float ac=0;};
 std::vector<std::vector<PR>> prs(d->meshes_count);
 for(size_t m=0;m<d->meshes_count;m++)for(size_t pi=0;pi<d->meshes[m].primitives_count;pi++){
  cgltf_primitive&pr=d->meshes[m].primitives[pi];if(pr.type!=cgltf_primitive_type_triangles)continue;
  cgltf_accessor*pos=nullptr,*nor=nullptr,*jn=nullptr,*wt=nullptr,*tc=nullptr;
  for(size_t a=0;a<pr.attributes_count;a++){cgltf_attribute&at=pr.attributes[a];
   if(at.type==cgltf_attribute_type_position)pos=at.data;else if(at.type==cgltf_attribute_type_normal)nor=at.data;
   else if(at.type==cgltf_attribute_type_joints&&at.index==0)jn=at.data;else if(at.type==cgltf_attribute_type_weights&&at.index==0)wt=at.data;
   else if(at.type==cgltf_attribute_type_texcoord&&at.index==0)tc=at.data;}
  if(!pos)continue;
  PR res;
  if(pr.material){cgltf_material*mt=pr.material;
   if(mt->has_pbr_metallic_roughness){const float*c=mt->pbr_metallic_roughness.base_color_factor;res.col=V{c[0],c[1],c[2]};
    res.met=mt->pbr_metallic_roughness.metallic_factor;res.rou=mt->pbr_metallic_roughness.roughness_factor;
    if(tc&&mt->pbr_metallic_roughness.base_color_texture.texture)res.tb=gltfImage(d,mt->pbr_metallic_roughness.base_color_texture.texture->image,dir,imgc);
    if(tc&&mt->pbr_metallic_roughness.metallic_roughness_texture.texture)res.tm=gltfImage(d,mt->pbr_metallic_roughness.metallic_roughness_texture.texture->image,dir,imgc);}
   if(tc&&mt->normal_texture.texture)res.tn=gltfImage(d,mt->normal_texture.texture->image,dir,imgc);
   if(mt->alpha_mode!=cgltf_alpha_mode_opaque&&res.tb>=0)res.ac=1;}
  std::vector<float> v,sk,uvs;size_t cnt=pr.indices?pr.indices->count:pos->count;
  for(size_t i=0;i+2<cnt;i+=3){V P[3],N[3];float SK[3][8];float UVk[3][2];
   for(int k=0;k<3;k++){size_t ix=pr.indices?cgltf_accessor_read_index(pr.indices,i+k):i+k;float f[4]={0,0,0,0};
    cgltf_accessor_read_float(pos,ix,f,3);P[k]=V{f[0],f[1],f[2]};
    if(nor){cgltf_accessor_read_float(nor,ix,f,3);N[k]=V{f[0],f[1],f[2]};}
    float jf[4]={0,0,0,0},wf[4]={1,0,0,0};
    if(jn&&wt){cgltf_accessor_read_float(jn,ix,jf,4);cgltf_accessor_read_float(wt,ix,wf,4);float sm=wf[0]+wf[1]+wf[2]+wf[3];
     if(sm<1e-6f){wf[0]=1;wf[1]=wf[2]=wf[3]=0;jf[0]=0;}else for(int q=0;q<4;q++){if(jf[q]>=64){wf[q]=0;jf[q]=0;}wf[q]/=sm;}}
    for(int q=0;q<4;q++){SK[k][q]=jf[q];SK[k][4+q]=wf[q];}
    float tv2[2]={0,0};if(tc)cgltf_accessor_read_float(tc,ix,tv2,2);UVk[k][0]=tv2[0];UVk[k][1]=tv2[1];}
   if(!nor){V fn=cross(P[1]-P[0],P[2]-P[0]);float l=sqrtf(dot(fn,fn));fn=l>1e-12f?fn*(1/l):V{0,1,0};N[0]=N[1]=N[2]=fn;}
   for(int k=0;k<3;k++){vtx(v,P[k],N[k]);sk.insert(sk.end(),SK[k],SK[k]+8);uvs.push_back(UVk[k][0]);uvs.push_back(UVk[k][1]);}}
  if(v.empty())continue;
  Mesh mm=upload(v,GL_TRIANGLES);float r=0;for(size_t i=0;i+2<v.size();i+=6)r=std::max(r,sqrtf(v[i]*v[i]+v[i+1]*v[i+1]+v[i+2]*v[i+2]));
  mm.rad=r>0?r:1;if(jn&&wt)attachSkin(mm,sk);if(tc)attachUV(mm,uvs);meshes.push_back(mm);res.mesh=(int)meshes.size()-1;prs[m].push_back(res);}
 int base=(int)objs.size();size_t nn=d->nodes_count;int sb=(int)skins.size();
 for(size_t i=0;i<nn;i++){cgltf_node*nd=&d->nodes[i];Obj o;int mid=nd->mesh?(int)(nd->mesh-d->meshes):-1;
  if(mid>=0&&!prs[mid].empty()){PR&q=prs[mid][0];o.mesh=q.mesh;o.col=q.col;o.met=q.met;o.rou=q.rou;o.tb=q.tb;o.tm=q.tm;o.tn=q.tn;o.ac=q.ac;}else{o.mesh=5;o.col=V{.85f,.85f,.65f};}
  snprintf(o.nm,24,"%s",nd->name?nd->name:"Node");o.skin=nd->skin?sb+(int)(nd->skin-d->skins):-1;o.parent=nd->parent?base+(int)(nd->parent-d->nodes):-1;T&t=o.cur;
  if(nd->has_matrix){float m[16];cgltf_node_transform_local(nd,m);t.p=V{m[12],m[13],m[14]};
   float sx=sqrtf(m[0]*m[0]+m[1]*m[1]+m[2]*m[2]),sy=sqrtf(m[4]*m[4]+m[5]*m[5]+m[6]*m[6]),sz=sqrtf(m[8]*m[8]+m[9]*m[9]+m[10]*m[10]);
   if(sx>0&&sy>0&&sz>0){t.s=V{sx,sy,sz};t.r.y=asinf(std::clamp(-m[2]/sx,-1.f,1.f));t.r.x=atan2f(m[6]/sy,m[10]/sz);t.r.z=atan2f(m[1]/sx,m[0]/sx);}}
  else{if(nd->has_translation)t.p=V{nd->translation[0],nd->translation[1],nd->translation[2]};
   if(nd->has_scale)t.s=V{nd->scale[0],nd->scale[1],nd->scale[2]};
   if(nd->has_rotation){float x=nd->rotation[0],y=nd->rotation[1],z=nd->rotation[2],w=nd->rotation[3];
    t.r.y=asinf(std::clamp(2*(w*y-z*x),-1.f,1.f));t.r.x=atan2f(2*(w*x+y*z),1-2*(x*x+y*y));t.r.z=atan2f(2*(w*z+x*y),1-2*(y*y+z*z));}}
  if(o.mesh==5){o.len=.25f;for(size_t c=0;c<nd->children_count;c++){cgltf_node*ch=nd->children[c];
   if(ch->has_translation){V tv{ch->translation[0],ch->translation[1],ch->translation[2]};float l=sqrtf(dot(tv,tv));if(l>.01f){o.len=l;break;}}}}
  objs.push_back(o);}
 for(size_t i=0;i<nn;i++){cgltf_node*nd=&d->nodes[i];if(!nd->mesh)continue;int mid=(int)(nd->mesh-d->meshes);
  for(size_t q=1;q<prs[mid].size();q++){PR&r=prs[mid][q];Obj o;o.mesh=r.mesh;o.col=r.col;o.met=r.met;o.rou=r.rou;o.tb=r.tb;o.tm=r.tm;o.tn=r.tn;o.ac=r.ac;
   snprintf(o.nm,24,"%.20s.%d",objs[base+i].nm,(int)q);o.parent=base+(int)i;o.skin=objs[base+i].skin;objs.push_back(o);}}
 for(size_t si=0;si<d->skins_count;si++){cgltf_skin&cs=d->skins[si];Skin sk;
  for(size_t k=0;k<cs.joints_count;k++){sk.j.push_back(base+(int)(cs.joints[k]-d->nodes));M im=id();
   if(cs.inverse_bind_matrices){float f16[16];cgltf_accessor_read_float(cs.inverse_bind_matrices,k,f16,16);memcpy(im.m,f16,sizeof im.m);}sk.ibm.push_back(im);}
  sk.fz.assign(sk.j.size(),id());skins.push_back(sk);}
 if(nn){float R=0;for(size_t i=0;i<nn;i++){Obj&o=objs[base+i];if(o.mesh==5)continue;M w=worldM(base+(int)i);V c{w.m[12],w.m[13],w.m[14]};
   float sc=std::max({sqrtf(w.m[0]*w.m[0]+w.m[1]*w.m[1]+w.m[2]*w.m[2]),sqrtf(w.m[4]*w.m[4]+w.m[5]*w.m[5]+w.m[6]*w.m[6]),sqrtf(w.m[8]*w.m[8]+w.m[9]*w.m[9]+w.m[10]*w.m[10])});
   R=std::max(R,sqrtf(dot(c,c))+meshes[o.mesh].rad*sc);}
  if(R>1e-4f){float f=(R>6||R<1.5f)?3.f/R:1.f;if(f!=1.f)for(size_t i=0;i<nn;i++){Obj&o=objs[base+i];if(o.parent<0){o.cur.s=o.cur.s*f;o.cur.p=o.cur.p*f;}}}
  sel=base;}
 cgltf_free(d);toast(fm("Importado: %d objetos",(int)nn));}
// ---------- SSAO (meia resolucao) + render pbr ----------
bool gMatcap=true,gSSAO=false,gPostOK=false,gMsOK=false,gBones=true;float gAOs=.85f,gFps=0,gFAcc=0;int gFN=0,gHW=1,gHH=1;
GLuint gMsFbo=0,gMsC=0,gMsD=0,gFbo=0,gCT=0,gDT=0,gAoFbo=0,gAoT=0,gAoP=0,gCoP=0;
GLint uAD,uATH,uANF,uAR,uASZ,uCC,uCA,uCT,uCS;
float gSlX[10]={0,0,1,0,0,0,0,0,0,0},gSlW[10]={1,1,1,1,1,1,1,1,1,1};
void setSlider(int i,float x){float v=std::clamp((x-gSlX[i])/gSlW[i],0.f,1.f);if(i==2){gAOs=v;return;}if(i==5){gSRad=20+(int)(v*180+.5f);return;}if(i==6){gSStr=.05f+v*.95f;return;}if(i==4){if(sel>=0&&objs[sel].kind==3)objs[sel].fov=.17f+v*2.23f;return;}if(i==7){if(sel>=0&&objs[sel].kind==4){float lo=objs[sel].shape==0?.3f:3.f,hi=objs[sel].shape==0?30.f:1000.f;objs[sel].len=lo*powf(hi/lo,v);}return;}if(i==8){if(sel>=0&&objs[sel].kind==4)objs[sel].fov=.1f+v*1.3f;return;}if(i==3){int ri=sel>=0?rigOf(sel):-1;if(ri>=0){rigs[ri].w=v;if(!rigs[ri].wk.empty())rigKeyW(rigs[ri],v);}return;}if(sel<0)return;if(i==0)objs[sel].met=v;else objs[sel].rou=v;}
GLuint mkProg(const char*vs,const char*fs){GLuint p=glCreateProgram();glAttachShader(p,sh(GL_VERTEX_SHADER,vs));glAttachShader(p,sh(GL_FRAGMENT_SHADER,fs));glLinkProgram(p);return p;}
void initPost(){
 gHW=std::max(W/2,1);gHH=std::max(H/2,1);
 auto tex=[&](GLuint&t,GLint ifmt,int w,int h,GLenum fmt,GLenum ty,GLint filt){glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);glTexImage2D(GL_TEXTURE_2D,0,ifmt,w,h,0,fmt,ty,nullptr);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,filt);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,filt);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);};
 tex(gCT,GL_RGBA8,W,H,GL_RGBA,GL_UNSIGNED_BYTE,GL_LINEAR);tex(gDT,GL_DEPTH_COMPONENT24,W,H,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,GL_NEAREST);tex(gAoT,GL_R8,gHW,gHH,GL_RED,GL_UNSIGNED_BYTE,GL_LINEAR);
 glGenFramebuffers(1,&gFbo);glBindFramebuffer(GL_FRAMEBUFFER,gFbo);
 glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,gCT,0);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,gDT,0);
 bool ok=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
 glGenFramebuffers(1,&gAoFbo);glBindFramebuffer(GL_FRAMEBUFFER,gAoFbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,gAoT,0);
 ok=ok&&glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
 GLint ms=0;glGetIntegerv(GL_MAX_SAMPLES,&ms);ms=std::min(ms,4);gMsOK=false;
 if(ms>=2){glGenRenderbuffers(1,&gMsC);glBindRenderbuffer(GL_RENDERBUFFER,gMsC);glRenderbufferStorageMultisample(GL_RENDERBUFFER,ms,GL_RGBA8,W,H);
  glGenRenderbuffers(1,&gMsD);glBindRenderbuffer(GL_RENDERBUFFER,gMsD);glRenderbufferStorageMultisample(GL_RENDERBUFFER,ms,GL_DEPTH_COMPONENT24,W,H);
  glGenFramebuffers(1,&gMsFbo);glBindFramebuffer(GL_FRAMEBUFFER,gMsFbo);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,gMsC);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,gMsD);
  gMsOK=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;}
 glBindFramebuffer(GL_FRAMEBUFFER,0);gPostOK=ok;
 const char*qv="#version 300 es\nlayout(location=0) in vec3 aP; out vec2 vUV; void main(){vUV=aP.xy;gl_Position=vec4(aP.xy*2.0-1.0,0.0,1.0);}";
 const char*af=R"(#version 300 es
precision highp float; in vec2 vUV; uniform sampler2D uD; uniform vec2 uTH; uniform vec2 uNF; uniform vec2 uSz; uniform float uR; out vec4 o;
const float bay[16]=float[16](0.,8.,2.,10.,12.,4.,14.,6.,3.,11.,1.,9.,15.,7.,13.,5.);
float lin(float d){float z=d*2.0-1.0;return 2.0*uNF.x*uNF.y/(uNF.y+uNF.x-z*(uNF.y-uNF.x));}
vec3 vp(ivec2 p){float e=lin(texelFetch(uD,p,0).r);vec2 uv=(vec2(p)+0.5)/uSz;return vec3((uv*2.0-1.0)*uTH*e,-e);}
void main(){ivec2 p=ivec2(gl_FragCoord.xy*2.0);if(texelFetch(uD,p,0).r>=0.99999){o=vec4(1.0);return;}
 vec3 P=vp(p),Pl=vp(p-ivec2(1,0)),Pr=vp(p+ivec2(1,0)),Pd=vp(p-ivec2(0,1)),Pu=vp(p+ivec2(0,1));
 vec3 dx=abs(Pr.z-P.z)<abs(P.z-Pl.z)?Pr-P:P-Pl;vec3 dy=abs(Pu.z-P.z)<abs(P.z-Pd.z)?Pu-P:P-Pd;
 vec3 N=normalize(cross(dx,dy));if(dot(N,-P)<0.0)N=-N;
 int bi=int(mod(gl_FragCoord.x,4.0))+4*int(mod(gl_FragCoord.y,4.0));float rot=(bay[bi]+0.5)/16.0*6.2831853;
 vec3 T=normalize(cross(N,abs(N.y)<0.9?vec3(0.0,1.0,0.0):vec3(1.0,0.0,0.0)));vec3 B=cross(N,T);
 vec3 P0=P+N*0.03*uR;float occ=0.0;
 for(int i=0;i<12;i++){float t=(float(i)+0.5)/12.0;float ph=float(i)*2.39996+rot;float s=sqrt(t);
  vec3 k=vec3(cos(ph)*s,sin(ph)*s,sqrt(1.0-t));k*=mix(0.2,1.0,t*t);
  vec3 sp=P0+(T*k.x+B*k.y+N*k.z)*uR;if(sp.z>-0.01)continue;
  vec2 uv=sp.xy/(-sp.z*uTH)*0.5+0.5;if(uv.x<0.0||uv.y<0.0||uv.x>1.0||uv.y>1.0)continue;
  float sd=lin(texelFetch(uD,min(ivec2(uv*uSz),ivec2(uSz)-1),0).r);
  float rc=smoothstep(0.0,1.0,uR/abs(-P.z-sd));occ+=(sd<-sp.z-0.06*uR?1.0:0.0)*rc;}
 o=vec4(1.0-occ/12.0);})";
 const char*cf=R"(#version 300 es
precision highp float; in vec2 vUV; uniform sampler2D uCol; uniform sampler2D uAO; uniform vec2 uTx; uniform float uStr; out vec4 o;
void main(){vec4 c=texture(uCol,vUV);vec2 q0=floor(vUV/uTx+0.5);
 float a=(texture(uAO,(q0+vec2(-1.0,-1.0))*uTx).r+texture(uAO,(q0+vec2(1.0,-1.0))*uTx).r+texture(uAO,(q0+vec2(-1.0,1.0))*uTx).r+texture(uAO,(q0+vec2(1.0,1.0))*uTx).r)*0.25;
 o=vec4(c.rgb*mix(1.0,a,uStr),1.0);})";
 gAoP=mkProg(qv,af);gCoP=mkProg(qv,cf);
 uAD=glGetUniformLocation(gAoP,"uD");uATH=glGetUniformLocation(gAoP,"uTH");uANF=glGetUniformLocation(gAoP,"uNF");uAR=glGetUniformLocation(gAoP,"uR");uASZ=glGetUniformLocation(gAoP,"uSz");
 uCC=glGetUniformLocation(gCoP,"uCol");uCA=glGetUniformLocation(gCoP,"uAO");uCT=glGetUniformLocation(gCoP,"uTx");uCS=glGetUniformLocation(gCoP,"uStr");
 glUseProgram(gProg);}
void runPost(){
 if(gMsOK){glBindFramebuffer(GL_READ_FRAMEBUFFER,gMsFbo);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,gFbo);
  glBlitFramebuffer(0,0,W,H,0,0,W,H,GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT,GL_NEAREST);}
 glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glBindVertexArray(meshes[3].vao);
 glBindFramebuffer(GL_FRAMEBUFFER,gAoFbo);glViewport(0,0,gHW,gHH);glUseProgram(gAoP);
 glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,gDT);glUniform1i(uAD,0);
 float th=tanf(FOV/2);glUniform2f(uATH,th*(float)W/H,th);glUniform2f(uANF,nearP(),100.f);glUniform1f(uAR,.55f);glUniform2f(uASZ,(float)W,(float)H);glDrawArrays(GL_TRIANGLES,0,6);
 glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,W,H);glUseProgram(gCoP);
 glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,gCT);glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,gAoT);
 glUniform1i(uCC,0);glUniform1i(uCA,1);glUniform2f(uCT,1.f/gHW,1.f/gHH);glUniform1f(uCS,gAOs);glDrawArrays(GL_TRIANGLES,0,6);
 glActiveTexture(GL_TEXTURE0);glEnable(GL_BLEND);glUseProgram(gProg);}
const char*MCCS=R"MC(#version 310 es
precision highp float;precision highp int;
layout(local_size_x=8,local_size_y=8,local_size_z=1) in;
layout(rgba32f,binding=0) writeonly uniform highp image2D uOut;
uniform highp sampler2D uPrev;
layout(rgba16f,binding=1) writeonly uniform highp image2D uAovOut;
uniform highp sampler2D uAovPrev;
layout(rgba16f,binding=2) writeonly uniform highp image2D uAlbOut;
uniform highp sampler2D uAlbPrev;
uniform highp sampler2DArray uTexA;
uniform int uN;
layout(std430,binding=0) readonly buffer NB{vec4 nd[];};
layout(std430,binding=1) readonly buffer TB{vec4 tr[];};
uniform ivec2 uSize;uniform int uOffY,uFirst,uFrame,uBounces,uLeak;
uniform vec3 uCam,uF,uR,uU;uniform vec2 uTH;uniform vec3 uSunD,uSunC;uniform float uSunCos;uniform int uNL;uniform vec4 uLP[8],uLD[8],uLC[8];
uint rs;
uint pcg(uint v){uint s=v*747796405u+2891336453u;uint w=((s>>((s>>28u)+4u))^s)*277803737u;return (w>>22u)^w;}
float rnd(){rs=pcg(rs);return float(rs)*(1.0/4294967296.0);}
const float PI=3.14159265;const float SHTOL=0.012;
uniform float uSky;
vec3 sky(vec3 d){return mix(vec3(.30,.28,.26),vec3(.42,.52,.78),clamp(d.y*.5+.5,0.,1.))*.9*uSky;}
bool hit(vec3 o,vec3 d,bool any,float tm,bool cull,out float th,out int ti,out vec2 buv){
 th=1e30;ti=-1;buv=vec2(0.0);vec3 id=1.0/mix(d,vec3(1e-9),lessThan(abs(d),vec3(1e-9)));
 int st[32];int sp=0;st[sp++]=0;
 while(sp>0){int n=st[--sp];vec4 a=nd[2*n],b=nd[2*n+1];
  vec3 t0=(a.xyz-o)*id,t1=(b.xyz-o)*id;vec3 tn=min(t0,t1),tf=max(t0,t1);
  float tmin=max(max(tn.x,tn.y),max(tn.z,0.0)),tmx=min(min(tf.x,tf.y),min(tf.z,th));
  if(tmin>tmx)continue;
  int cnt=floatBitsToInt(b.w),lf=floatBitsToInt(a.w);
  if(cnt>0){for(int i=0;i<cnt;i++){int t=lf+i;vec3 p0=tr[8*t].xyz,p1=tr[8*t+1].xyz,p2=tr[8*t+2].xyz;
    vec3 e1=p1-p0,e2=p2-p0,pv=cross(d,e2);float det=dot(e1,pv);if(abs(det)<1e-12)continue;
    if(cull){vec3 Ng=cross(e1,e2);if(-dot(d,Ng)*dot(Ng,tr[8*t+3].xyz)<0.0)continue;}
    float inv=1.0/det;
    vec3 tv=o-p0;float u=dot(tv,pv)*inv;if(u<0.0||u>1.0)continue;vec3 qv=cross(tv,e1);float v=dot(d,qv)*inv;if(v<0.0||u+v>1.0)continue;
    float tt=dot(e2,qv)*inv;if(tt>tm&&tt<th){
     float ya=tr[8*t+7].y;if(fract(ya+0.001)>0.25){int lb=int(floor(ya+0.001));if(lb>=0){float w0=1.0-u-v;vec2 uq=vec2(tr[8*t].w*w0+tr[8*t+1].w*u+tr[8*t+2].w*v,tr[8*t+3].w*w0+tr[8*t+4].w*u+tr[8*t+5].w*v);if(textureLod(uTexA,vec3(uq,float(lb)),0.0).a<0.5)continue;}}
     th=tt;ti=t;buv=vec2(u,v);if(any)return true;}}}
  else{st[sp++]=lf;st[sp++]=n+1;}}
 return ti>=0;}
vec3 brdf(vec3 L,vec3 N,vec3 V,vec3 alb,float met,float rou,vec3 F0){
 vec3 H=normalize(L+V);float a=rou*rou,a2=a*a;float NL=max(dot(N,L),1e-4),NV=max(dot(N,V),1e-3),NH=max(dot(N,H),0.0),VH=max(dot(V,H),0.0);
 float dd=NH*NH*(a2-1.0)+1.0;float D=a2/(PI*dd*dd+1e-7);float k=(rou+1.0)*(rou+1.0)/8.0;
 float G=(NL/(NL*(1.0-k)+k))*(NV/(NV*(1.0-k)+k));vec3 F=F0+(1.0-F0)*pow(max(1.0-VH,0.0),5.0);
 return (1.0-F)*(1.0-met)*alb/PI+D*G*F/(4.0*NV*NL);}
void basis(vec3 N,out vec3 T,out vec3 B){T=normalize(cross(N,abs(N.y)<0.99?vec3(0.0,1.0,0.0):vec3(1.0,0.0,0.0)));B=cross(N,T);}
void main(){
 ivec2 px=ivec2(gl_GlobalInvocationID.xy)+ivec2(0,uOffY);if(px.x>=uSize.x||px.y>=uSize.y)return;
 rs=pcg(uint(px.x)*1973u+uint(px.y)*9277u+uint(uFrame)*26699u+1u);
 vec2 uv=(vec2(px)+vec2(rnd(),rnd()))/vec2(uSize)*2.0-1.0;
 vec3 rd=normalize(uF+uR*(uv.x*uTH.x)+uU*(uv.y*uTH.y));vec3 ro=uCam;
 vec3 Lo=vec3(0.0),T=vec3(1.0);vec3 aN=vec3(0.0);float aD=1e4;vec3 aA=vec3(1.0);
 for(int b=0;b<uBounces;b++){
  float th;int ti;vec2 bu;
  if(!hit(ro,rd,false,1e-4,b>0&&uLeak>0,th,ti,bu)){Lo+=T*sky(rd);break;}
  vec3 n0=tr[8*ti+3].xyz,n1=tr[8*ti+4].xyz,n2=tr[8*ti+5].xyz;
  vec3 N=normalize(n0*(1.0-bu.x-bu.y)+n1*bu.x+n2*bu.y);
  vec4 m1=tr[8*ti+6],m2=tr[8*ti+7];
  float w0=1.0-bu.x-bu.y;vec2 tuv=vec2(tr[8*ti].w*w0+tr[8*ti+1].w*bu.x+tr[8*ti+2].w*bu.y,tr[8*ti+3].w*w0+tr[8*ti+4].w*bu.x+tr[8*ti+5].w*bu.y);
  int lb=int(floor(m2.y+0.001)),lm=int(floor(m2.z+0.001)),ln=int(floor(m2.w+0.001));
  vec3 alb=m1.rgb;float met=m1.a,rou=m2.x;
  if(lb>=0)alb*=pow(textureLod(uTexA,vec3(tuv,float(lb)),0.0).rgb,vec3(2.2));
  if(lm>=0){vec4 mrs=textureLod(uTexA,vec3(tuv,float(lm)),0.0);rou*=mrs.g;met*=mrs.b;}
  rou=clamp(rou,0.05,1.0);
  if(ln>=0){vec3 tn=textureLod(uTexA,vec3(tuv,float(ln)),0.0).xyz*2.0-1.0;
   vec3 qa=tr[8*ti].xyz,qb=tr[8*ti+1].xyz,qc=tr[8*ti+2].xyz;vec3 e1=qb-qa,e2=qc-qa;
   vec2 ua=vec2(tr[8*ti].w,tr[8*ti+3].w),ub=vec2(tr[8*ti+1].w,tr[8*ti+4].w),uc=vec2(tr[8*ti+2].w,tr[8*ti+5].w);vec2 d1=ub-ua,d2=uc-ua;float rr=d1.x*d2.y-d2.x*d1.y;
   if(abs(rr)>1e-12){vec3 Tt=(e1*d2.y-e2*d1.y)/rr,Bt=-(e2*d1.x-e1*d2.x)/rr;Tt-=N*dot(N,Tt);float lt=length(Tt);
   if(lt>1e-6){Tt/=lt;Bt-=N*dot(N,Bt)+Tt*dot(Tt,Bt);float lq=length(Bt);if(lq>1e-6){Bt/=lq;vec3 N2=Tt*tn.x+Bt*tn.y+N*tn.z;float l2=dot(N2,N2);if(l2>1e-6)N=N2*inversesqrt(l2);}}}}
  vec3 F0=mix(vec3(0.04),alb,met);
  vec3 V=-rd;if(dot(N,V)<0.0)N=-N;if(b==0){aN=N;aD=th;aA=alb;}vec3 P=ro+rd*th+N*0.002;
  vec3 TT,BB;basis(uSunD,TT,BB);float cz=1.0-rnd()*(1.0-uSunCos),sz=sqrt(max(1.0-cz*cz,0.0)),ph=2.0*PI*rnd();
  vec3 sd=normalize(TT*(sz*cos(ph))+BB*(sz*sin(ph))+uSunD*cz);float NLs=dot(N,sd);
  if(NLs>0.0){float t2;int i2;vec2 b2;if(!hit(P,sd,true,SHTOL,uLeak>0,t2,i2,b2))Lo+=T*brdf(sd,N,V,alb,met,rou,F0)*uSunC*NLs;}
  for(int li=0;li<uNL;li++){
   vec4 lp=uLP[li],ld=uLD[li],lc=uLC[li];vec3 Lv;float att=1.0,dist=1e30;
   if(lp.w<0.5){Lv=-ld.xyz;}
   else{vec3 dv=lp.xyz-P;float d2=max(dot(dv,dv),1e-4);dist=sqrt(d2);Lv=dv/dist;att=1.0/d2;
    if(lp.w>1.5){float cs=dot(-Lv,ld.xyz);att*=smoothstep(ld.w,mix(ld.w,1.0,0.3),cs);}}
   float NLl=dot(N,Lv);
   if(NLl>0.0&&att>0.0){float t3;int i3;vec2 b3;bool occ;
    if(lp.w<0.5)occ=hit(P,Lv,true,SHTOL,uLeak>0,t3,i3,b3);
    else occ=hit(P,Lv,false,SHTOL,uLeak>0,t3,i3,b3)&&t3<dist-0.02;
    if(!occ)Lo+=T*brdf(Lv,N,V,alb,met,rou,F0)*lc.rgb*att*NLl;}}
  float a=rou*rou,a2=a*a,ps=mix(0.5,1.0,met);vec3 Ld;vec3 T0,B0;basis(N,T0,B0);
  if(rnd()<ps){float u1=rnd(),u2=rnd();float ct=sqrt((1.0-u2)/(1.0+(a2-1.0)*u2)),st=sqrt(max(1.0-ct*ct,0.0)),p2=2.0*PI*u1;
   vec3 H=normalize(T0*(st*cos(p2))+B0*(st*sin(p2))+N*ct);Ld=reflect(-V,H);}
  else{float u1=rnd(),u2=rnd();float r=sqrt(u1),p2=2.0*PI*u2;Ld=normalize(T0*(r*cos(p2))+B0*(r*sin(p2))+N*sqrt(max(1.0-u1,0.0)));}
  float NL=dot(N,Ld);if(NL<=0.0)break;
  vec3 H2=normalize(Ld+V);float NH=max(dot(N,H2),0.0),VH=max(dot(V,H2),1e-4);
  float dd=NH*NH*(a2-1.0)+1.0,D=a2/(PI*dd*dd+1e-7);
  float pdf=(1.0-ps)*NL/PI+ps*D*NH/(4.0*VH);
  T*=brdf(Ld,N,V,alb,met,rou,F0)*NL/max(pdf,1e-5);
  if(b>=2){float q=clamp(max(T.r,max(T.g,T.b)),0.05,0.95);if(rnd()>q)break;T/=q;}
  ro=P;rd=Ld;}
 vec4 prev=uFirst==1?vec4(0.0):texelFetch(uPrev,px,0);
 if(any(isnan(Lo))||any(isinf(Lo)))Lo=vec3(0.0);
 imageStore(uOut,px,prev+vec4(min(Lo,vec3(20.0)),1.0));
 vec4 av=vec4(aN,aD);vec4 pa=uFirst==1?av:texelFetch(uAovPrev,px,0);
 imageStore(uAovOut,px,vec4(pa.rgb+(av.rgb-pa.rgb)/float(uN),pa.a+(av.a-pa.a)/float(uN)));
 vec4 al=vec4(aA,1.0);vec4 pl=uFirst==1?al:texelFetch(uAlbPrev,px,0);imageStore(uAlbOut,px,vec4(pl.rgb+(al.rgb-pl.rgb)/float(uN),1.0));}
)MC";
const char*MCCS0=R"MC(#version 310 es
precision highp float;precision highp int;
layout(local_size_x=8,local_size_y=8,local_size_z=1) in;
layout(rgba32f,binding=0) writeonly uniform highp image2D uOut;
uniform highp sampler2D uPrev;
layout(rgba16f,binding=1) writeonly uniform highp image2D uAovOut;
uniform highp sampler2D uAovPrev;
layout(rgba16f,binding=2) writeonly uniform highp image2D uAlbOut;
uniform highp sampler2D uAlbPrev;
uniform highp sampler2DArray uTexA;
uniform int uN;
layout(std430,binding=0) readonly buffer NB{vec4 nd[];};
layout(std430,binding=1) readonly buffer TB{vec4 tr[];};
uniform ivec2 uSize;uniform int uOffY,uFirst,uFrame,uBounces,uLeak;
uniform vec3 uCam,uF,uR,uU;uniform vec2 uTH;uniform vec3 uSunD,uSunC;uniform float uSunCos;
uint rs;
uint pcg(uint v){uint s=v*747796405u+2891336453u;uint w=((s>>((s>>28u)+4u))^s)*277803737u;return (w>>22u)^w;}
float rnd(){rs=pcg(rs);return float(rs)*(1.0/4294967296.0);}
const float PI=3.14159265;const float SHTOL=0.012;
vec3 sky(vec3 d){return mix(vec3(.30,.28,.26),vec3(.42,.52,.78),clamp(d.y*.5+.5,0.,1.))*.9;}
bool hit(vec3 o,vec3 d,bool any,float tm,bool cull,out float th,out int ti,out vec2 buv){
 th=1e30;ti=-1;buv=vec2(0.0);vec3 id=1.0/mix(d,vec3(1e-9),lessThan(abs(d),vec3(1e-9)));
 int st[32];int sp=0;st[sp++]=0;
 while(sp>0){int n=st[--sp];vec4 a=nd[2*n],b=nd[2*n+1];
  vec3 t0=(a.xyz-o)*id,t1=(b.xyz-o)*id;vec3 tn=min(t0,t1),tf=max(t0,t1);
  float tmin=max(max(tn.x,tn.y),max(tn.z,0.0)),tmx=min(min(tf.x,tf.y),min(tf.z,th));
  if(tmin>tmx)continue;
  int cnt=floatBitsToInt(b.w),lf=floatBitsToInt(a.w);
  if(cnt>0){for(int i=0;i<cnt;i++){int t=lf+i;vec3 p0=tr[8*t].xyz,p1=tr[8*t+1].xyz,p2=tr[8*t+2].xyz;
    vec3 e1=p1-p0,e2=p2-p0,pv=cross(d,e2);float det=dot(e1,pv);if(abs(det)<1e-12)continue;
    if(cull){vec3 Ng=cross(e1,e2);if(-dot(d,Ng)*dot(Ng,tr[8*t+3].xyz)<0.0)continue;}
    float inv=1.0/det;
    vec3 tv=o-p0;float u=dot(tv,pv)*inv;if(u<0.0||u>1.0)continue;vec3 qv=cross(tv,e1);float v=dot(d,qv)*inv;if(v<0.0||u+v>1.0)continue;
    float tt=dot(e2,qv)*inv;if(tt>tm&&tt<th){
     float ya=tr[8*t+7].y;if(fract(ya+0.001)>0.25){int lb=int(floor(ya+0.001));if(lb>=0){float w0=1.0-u-v;vec2 uq=vec2(tr[8*t].w*w0+tr[8*t+1].w*u+tr[8*t+2].w*v,tr[8*t+3].w*w0+tr[8*t+4].w*u+tr[8*t+5].w*v);if(textureLod(uTexA,vec3(uq,float(lb)),0.0).a<0.5)continue;}}
     th=tt;ti=t;buv=vec2(u,v);if(any)return true;}}}
  else{st[sp++]=lf;st[sp++]=n+1;}}
 return ti>=0;}
vec3 brdf(vec3 L,vec3 N,vec3 V,vec3 alb,float met,float rou,vec3 F0){
 vec3 H=normalize(L+V);float a=rou*rou,a2=a*a;float NL=max(dot(N,L),1e-4),NV=max(dot(N,V),1e-3),NH=max(dot(N,H),0.0),VH=max(dot(V,H),0.0);
 float dd=NH*NH*(a2-1.0)+1.0;float D=a2/(PI*dd*dd+1e-7);float k=(rou+1.0)*(rou+1.0)/8.0;
 float G=(NL/(NL*(1.0-k)+k))*(NV/(NV*(1.0-k)+k));vec3 F=F0+(1.0-F0)*pow(max(1.0-VH,0.0),5.0);
 return (1.0-F)*(1.0-met)*alb/PI+D*G*F/(4.0*NV*NL);}
void basis(vec3 N,out vec3 T,out vec3 B){T=normalize(cross(N,abs(N.y)<0.99?vec3(0.0,1.0,0.0):vec3(1.0,0.0,0.0)));B=cross(N,T);}
void main(){
 ivec2 px=ivec2(gl_GlobalInvocationID.xy)+ivec2(0,uOffY);if(px.x>=uSize.x||px.y>=uSize.y)return;
 rs=pcg(uint(px.x)*1973u+uint(px.y)*9277u+uint(uFrame)*26699u+1u);
 vec2 uv=(vec2(px)+vec2(rnd(),rnd()))/vec2(uSize)*2.0-1.0;
 vec3 rd=normalize(uF+uR*(uv.x*uTH.x)+uU*(uv.y*uTH.y));vec3 ro=uCam;
 vec3 Lo=vec3(0.0),T=vec3(1.0);vec3 aN=vec3(0.0);float aD=1e4;vec3 aA=vec3(1.0);
 for(int b=0;b<uBounces;b++){
  float th;int ti;vec2 bu;
  if(!hit(ro,rd,false,1e-4,b>0&&uLeak>0,th,ti,bu)){Lo+=T*sky(rd);break;}
  vec3 n0=tr[8*ti+3].xyz,n1=tr[8*ti+4].xyz,n2=tr[8*ti+5].xyz;
  vec3 N=normalize(n0*(1.0-bu.x-bu.y)+n1*bu.x+n2*bu.y);
  vec4 m1=tr[8*ti+6],m2=tr[8*ti+7];
  float w0=1.0-bu.x-bu.y;vec2 tuv=vec2(tr[8*ti].w*w0+tr[8*ti+1].w*bu.x+tr[8*ti+2].w*bu.y,tr[8*ti+3].w*w0+tr[8*ti+4].w*bu.x+tr[8*ti+5].w*bu.y);
  int lb=int(floor(m2.y+0.001)),lm=int(floor(m2.z+0.001)),ln=int(floor(m2.w+0.001));
  vec3 alb=m1.rgb;float met=m1.a,rou=m2.x;
  if(lb>=0)alb*=pow(textureLod(uTexA,vec3(tuv,float(lb)),0.0).rgb,vec3(2.2));
  if(lm>=0){vec4 mrs=textureLod(uTexA,vec3(tuv,float(lm)),0.0);rou*=mrs.g;met*=mrs.b;}
  rou=clamp(rou,0.05,1.0);
  if(ln>=0){vec3 tn=textureLod(uTexA,vec3(tuv,float(ln)),0.0).xyz*2.0-1.0;
   vec3 qa=tr[8*ti].xyz,qb=tr[8*ti+1].xyz,qc=tr[8*ti+2].xyz;vec3 e1=qb-qa,e2=qc-qa;
   vec2 ua=vec2(tr[8*ti].w,tr[8*ti+3].w),ub=vec2(tr[8*ti+1].w,tr[8*ti+4].w),uc=vec2(tr[8*ti+2].w,tr[8*ti+5].w);vec2 d1=ub-ua,d2=uc-ua;float rr=d1.x*d2.y-d2.x*d1.y;
   if(abs(rr)>1e-12){vec3 Tt=(e1*d2.y-e2*d1.y)/rr,Bt=-(e2*d1.x-e1*d2.x)/rr;Tt-=N*dot(N,Tt);float lt=length(Tt);
   if(lt>1e-6){Tt/=lt;Bt-=N*dot(N,Bt)+Tt*dot(Tt,Bt);float lq=length(Bt);if(lq>1e-6){Bt/=lq;vec3 N2=Tt*tn.x+Bt*tn.y+N*tn.z;float l2=dot(N2,N2);if(l2>1e-6)N=N2*inversesqrt(l2);}}}}
  vec3 F0=mix(vec3(0.04),alb,met);
  vec3 V=-rd;if(dot(N,V)<0.0)N=-N;if(b==0){aN=N;aD=th;aA=alb;}vec3 P=ro+rd*th+N*0.002;
  vec3 TT,BB;basis(uSunD,TT,BB);float cz=1.0-rnd()*(1.0-uSunCos),sz=sqrt(max(1.0-cz*cz,0.0)),ph=2.0*PI*rnd();
  vec3 sd=normalize(TT*(sz*cos(ph))+BB*(sz*sin(ph))+uSunD*cz);float NLs=dot(N,sd);
  if(NLs>0.0){float t2;int i2;vec2 b2;if(!hit(P,sd,true,SHTOL,uLeak>0,t2,i2,b2))Lo+=T*brdf(sd,N,V,alb,met,rou,F0)*uSunC*NLs;}
  float a=rou*rou,a2=a*a,ps=mix(0.5,1.0,met);vec3 Ld;vec3 T0,B0;basis(N,T0,B0);
  if(rnd()<ps){float u1=rnd(),u2=rnd();float ct=sqrt((1.0-u2)/(1.0+(a2-1.0)*u2)),st=sqrt(max(1.0-ct*ct,0.0)),p2=2.0*PI*u1;
   vec3 H=normalize(T0*(st*cos(p2))+B0*(st*sin(p2))+N*ct);Ld=reflect(-V,H);}
  else{float u1=rnd(),u2=rnd();float r=sqrt(u1),p2=2.0*PI*u2;Ld=normalize(T0*(r*cos(p2))+B0*(r*sin(p2))+N*sqrt(max(1.0-u1,0.0)));}
  float NL=dot(N,Ld);if(NL<=0.0)break;
  vec3 H2=normalize(Ld+V);float NH=max(dot(N,H2),0.0),VH=max(dot(V,H2),1e-4);
  float dd=NH*NH*(a2-1.0)+1.0,D=a2/(PI*dd*dd+1e-7);
  float pdf=(1.0-ps)*NL/PI+ps*D*NH/(4.0*VH);
  T*=brdf(Ld,N,V,alb,met,rou,F0)*NL/max(pdf,1e-5);
  if(b>=2){float q=clamp(max(T.r,max(T.g,T.b)),0.05,0.95);if(rnd()>q)break;T/=q;}
  ro=P;rd=Ld;}
 vec4 prev=uFirst==1?vec4(0.0):texelFetch(uPrev,px,0);
 if(any(isnan(Lo))||any(isinf(Lo)))Lo=vec3(0.0);
 imageStore(uOut,px,prev+vec4(min(Lo,vec3(20.0)),1.0));
 vec4 av=vec4(aN,aD);vec4 pa=uFirst==1?av:texelFetch(uAovPrev,px,0);
 imageStore(uAovOut,px,vec4(pa.rgb+(av.rgb-pa.rgb)/float(uN),pa.a+(av.a-pa.a)/float(uN)));
 vec4 al=vec4(aA,1.0);vec4 pl=uFirst==1?al:texelFetch(uAlbPrev,px,0);imageStore(uAlbOut,px,vec4(pl.rgb+(al.rgb-pl.rgb)/float(uN),1.0));}
)MC";
const char*DNCS=R"MC(#version 310 es
precision highp float;precision highp int;
layout(local_size_x=8,local_size_y=8,local_size_z=1) in;
layout(rgba16f,binding=0) writeonly uniform highp image2D uOut;
uniform highp sampler2D uSrc;uniform highp sampler2D uAov;uniform highp sampler2D uAlb;
uniform ivec2 uSize;uniform int uStep,uIter0,uLast;uniform float uPhiC;
vec3 col(ivec2 p){vec4 c=texelFetch(uSrc,p,0);return uIter0==1?(c.a>0.0?c.rgb/c.a:vec3(0.0))/max(texelFetch(uAlb,p,0).rgb,vec3(.03)):c.rgb;}
float lum(vec3 c){return dot(c,vec3(0.2126,0.7152,0.0722));}
void main(){ivec2 p=ivec2(gl_GlobalInvocationID.xy);if(p.x>=uSize.x||p.y>=uSize.y)return;
 vec4 ap=texelFetch(uAov,p,0);vec3 cp=col(p);float lp=lum(cp);vec3 sum=vec3(0.0);float ws=0.0;
 for(int j=-1;j<=1;j++)for(int i=-1;i<=1;i++){
  ivec2 q=clamp(p+ivec2(i,j)*uStep,ivec2(0),uSize-1);float k=(i==0?2.0:1.0)*(j==0?2.0:1.0);
  vec4 aq=texelFetch(uAov,q,0);vec3 cq=col(q);
  float wn=pow(max(dot(ap.xyz,aq.xyz),0.0),24.0);
  float wd=exp(-abs(ap.w-aq.w)/(0.03*ap.w+0.02*float(uStep)*ap.w+1e-3));
  float wc=exp(-abs(lp-lum(cq))/uPhiC);
  float w=(i==0&&j==0)?k:k*wn*wd*wc;sum+=cq*w;ws+=w;}
 vec3 res=sum/max(ws,1e-6);if(uLast==1)res*=max(texelFetch(uAlb,p,0).rgb,vec3(.03));imageStore(uOut,p,vec4(res,1.0));}
)MC";
const char*MCTM=R"MC(#version 300 es
precision highp float; in vec2 vUV; uniform sampler2D uAcc; uniform ivec2 uSz; out vec4 o;
vec3 fetchC(ivec2 p){p=clamp(p,ivec2(0),uSz-1);vec4 a=texelFetch(uAcc,p,0);return a.a>0.0?a.rgb/a.a:vec3(0.0);}
void main(){vec2 p=vUV*vec2(uSz)-0.5;ivec2 i0=ivec2(floor(p));vec2 f=fract(p);
 vec3 c=mix(mix(fetchC(i0),fetchC(i0+ivec2(1,0)),f.x),mix(fetchC(i0+ivec2(0,1)),fetchC(i0+ivec2(1,1)),f.x),f.y);
 c=c*(2.51*c+0.03)/(c*(2.43*c+0.59)+0.14);o=vec4(pow(clamp(c,0.0,1.0),vec3(1.0/2.2)),1.0);}
)MC";
// ---------- Monte Carlo (preview) e path tracing (render final) ----------
struct TriD{V p[3],n[3];float uv[3][2]={};V col;float met=0,rou=.5f;float tb=-1,tm=-1,tn=-1,ac=0;};struct BN{V mn,mx;int lf=0,cnt=0;};
std::vector<TriD> gTD;std::vector<int> gOrd;std::vector<V> gCen;std::vector<BN> gBN;
bool gMcOK=false,gMcPrev=false,gMcFinal=false,gMcGround=true,gMcHaveCam=false;
int gMcSpp=64,gMcSamples=0,gMcRow=0,gMcRows=24,gMcW=0,gMcH=0,gMcCur=0,gMcFrame=0;
bool gMcLeak=true,gVidBusy=false;int gMcFinW=1920,gMcBn=12,gVidQ=1;const char*gVidQN[3]={"Rapido 1024","Medio 1280","Alto 1920"};
GLuint gMcTex[2]={0,0},gMcProg=0,gMcTm=0,gMcNB=0,gMcTB=0,gMcOutF=0,gMcOutT=0;
GLuint gMcAov[2]={0,0},gMcAlb[2]={0,0},gMcDnT[2]={0,0},gDnProg=0;bool gMcDn=true,gMcDnOK=false,gDnDirty=false;int gMcDnRes=0;GLint dnSrc,dnAov,dnSz,dnStep,dnI0,dnPhi,mcAovPrev,mcN,dnAlb,dnLast,mcAlbPrev,mcTexA;
GLint mcLeak,mcSz,mcOff,mcFirst,mcFr,mcBn,mcCam,mcF,mcR,mcU,mcTH,mcSD,mcSC,mcSCos,mcPrv,tmAcc,tmSz;
GLint mcNL=-1,mcLP=-1,mcLD=-1,mcLC=-1,mcSky=-1;
bool gCapReq=false,gCapNow=false;int gCapW=1280,gCapH=720;
int gVidFps=30,gRA=-1,gRB=-1,gAutoPng=0,gPrevSeq=0;bool gMcNight=false,gRenderReq=false;std::string gPrevPath;
double gSigS=-1,gSigC=-1,gMcLast=0;float gDt=.016f;V gCE,gCF,gCR,gCU;float gCFov=1.f;
double nowSec(){timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
V vmin(V a,V b){return V{std::min(a.x,b.x),std::min(a.y,b.y),std::min(a.z,b.z)};}
V vmax(V a,V b){return V{std::max(a.x,b.x),std::max(a.y,b.y),std::max(a.z,b.z)};}
V xf(const M&m,V p){return V{m.m[0]*p.x+m.m[4]*p.y+m.m[8]*p.z+m.m[12],m.m[1]*p.x+m.m[5]*p.y+m.m[9]*p.z+m.m[13],m.m[2]*p.x+m.m[6]*p.y+m.m[10]*p.z+m.m[14]};}
V xn(const M&m,V p){return V{m.m[0]*p.x+m.m[4]*p.y+m.m[8]*p.z,m.m[1]*p.x+m.m[5]*p.y+m.m[9]*p.z,m.m[2]*p.x+m.m[6]*p.y+m.m[10]*p.z};}
void mcReset(){gMcSamples=0;gMcRow=0;gMcFrame=0;gMcDnOK=false;}
// ---------- escultura: pincel sobre a malha (malha soldada por posicao = sem rachaduras) ----------
struct KeyI{int x,y,z;bool operator==(const KeyI&o)const{return x==o.x&&y==o.y&&z==o.z;}};
struct KeyH{size_t operator()(const KeyI&k)const{return((size_t)(unsigned)k.x*73856093u)^((size_t)(unsigned)k.y*19349663u)^((size_t)(unsigned)k.z*83492791u);}};
struct SM{int mesh=-1;size_t nsv=0;std::vector<int> wid,ws,wl,ns,nl;std::vector<V> wp,wn;};
SM gSM;bool gSculpt=false,gSInv=false,gSSym=true;int gSBrush=0;float gSCx=0,gSCy=0;
const char*gSBN[5]={"Desenhar","Inflar","Suavizar","Achatar","Agarrar"};
std::vector<int> gSDirty;std::vector<char> gSFlag,gSNF;
struct M3{V a,b,c;};
static M3 inv3(const M3&m,bool&ok){V r0=cross(m.b,m.c),r1=cross(m.c,m.a),r2=cross(m.a,m.b);float det=dot(m.a,r0);if(fabsf(det)<1e-20f){ok=false;return m;}float iv=1/det;ok=true;
 return M3{V{r0.x,r1.x,r2.x}*iv,V{r0.y,r1.y,r2.y}*iv,V{r0.z,r1.z,r2.z}*iv};}
static V mul3(const M3&m,V v){return m.a*v.x+m.b*v.y+m.c*v.z;}
static float sFall(float d,float R){if(d>=R)return 0;float x=1-d*d/(R*R);return x*x;}
void smNormalFor(int w){SM&s=gSM;V n{};for(int k=s.ws[w];k<s.ws[w+1];k++){int t=s.wl[k]/3;V a=s.wp[s.wid[3*t]],b=s.wp[s.wid[3*t+1]],c=s.wp[s.wid[3*t+2]];n=n+cross(b-a,c-a);}
 float l=vlen(n);s.wn[w]=l>1e-12f?n*(1/l):V{0,1,0};}
void smBuild(int mi){Mesh&m=meshes[mi];SM&s=gSM;s=SM();s.mesh=mi;size_t nv=m.cpu.size()/6;s.nsv=nv;
 float eps=std::max(m.rad,1e-3f)*1e-5f;std::unordered_map<KeyI,int,KeyH> mp;mp.reserve(nv);s.wid.resize(nv);
 for(size_t i=0;i<nv;i++){const float*c=&m.cpu[i*6];KeyI k{(int)lroundf(c[0]/eps),(int)lroundf(c[1]/eps),(int)lroundf(c[2]/eps)};auto it=mp.find(k);
  if(it==mp.end()){int id2=(int)s.wp.size();mp[k]=id2;s.wp.push_back(V{c[0],c[1],c[2]});s.wid[i]=id2;}else s.wid[i]=it->second;}
 size_t nw=s.wp.size();s.ws.assign(nw+1,0);for(size_t i=0;i<nv;i++)s.ws[s.wid[i]+1]++;for(size_t w=0;w<nw;w++)s.ws[w+1]+=s.ws[w];
 s.wl.resize(nv);{std::vector<int> fl(s.ws.begin(),s.ws.end()-1);for(size_t i=0;i<nv;i++)s.wl[fl[s.wid[i]]++]=(int)i;}
 {std::vector<std::vector<int>> nb(nw);for(size_t t=0;t+2<nv;t+=3){int a=s.wid[t],b=s.wid[t+1],c=s.wid[t+2];nb[a].push_back(b);nb[a].push_back(c);nb[b].push_back(a);nb[b].push_back(c);nb[c].push_back(a);nb[c].push_back(b);}
  s.ns.assign(nw+1,0);for(size_t w=0;w<nw;w++){auto&v=nb[w];std::sort(v.begin(),v.end());v.erase(std::unique(v.begin(),v.end()),v.end());s.ns[w+1]=s.ns[w]+(int)v.size();}
  s.nl.resize(s.ns[nw]);for(size_t w=0;w<nw;w++)std::copy(nb[w].begin(),nb[w].end(),s.nl.begin()+s.ns[w]);}
 s.wn.assign(nw,V{0,1,0});for(size_t w=0;w<nw;w++)smNormalFor((int)w);
 gSFlag.assign(nw,0);gSNF.assign(nw,0);gSDirty.clear();}
void smPose(int oi,std::vector<V>&pp){Obj&o=objs[oi];Mesh&m=meshes[o.mesh];size_t nv=m.cpu.size()/6;pp.resize(nv);
 bool sk=o.skin>=0&&o.skin<(int)skins.size()&&m.skinned&&m.skin.size()>=nv*8;
 if(sk){Skin&s=skins[o.skin];std::vector<M> J(s.j.size());for(size_t k=0;k<s.j.size();k++){M jw=s.j[k]>=0?worldM(s.j[k]):(k<s.fz.size()?s.fz[k]:id());J[k]=mul(jw,s.ibm[k]);}
  for(size_t i=0;i<nv;i++){const float*c=&m.cpu[i*6];V p{c[0],c[1],c[2]};const float*sw=&m.skin[i*8];V a{};
   for(int q=0;q<4;q++){float wq=sw[4+q];if(wq<=0)continue;int ji=(int)sw[q];if(ji>=(int)J.size())continue;a=a+xf(J[ji],p)*wq;}pp[i]=a;}}
 else{M w=worldM(oi);for(size_t i=0;i<nv;i++){const float*c=&m.cpu[i*6];pp[i]=xf(w,V{c[0],c[1],c[2]});}}}
struct SHit{bool ok=false;int tri=-1;float t=0,u=0,v=0;};
bool smRay(const std::vector<V>&pp,V ro,V rd,SHit&h){float best=1e30f;int bt=-1;float bu=0,bv=0;
 for(size_t t=0;t+2<pp.size();t+=3){V e1=pp[t+1]-pp[t],e2=pp[t+2]-pp[t],pv=cross(rd,e2);float det=dot(e1,pv);if(fabsf(det)<1e-14f)continue;float iv=1/det;V tv=ro-pp[t];
  float u=dot(tv,pv)*iv;if(u<0||u>1)continue;V qv=cross(tv,e1);float v=dot(rd,qv)*iv;if(v<0||u+v>1)continue;float tt=dot(e2,qv)*iv;if(tt>1e-5f&&tt<best){best=tt;bt=(int)(t/3);bu=u;bv=v;}}
 h.ok=bt>=0;h.tri=bt;h.t=best;h.u=bu;h.v=bv;return h.ok;}
void sRay(float sx,float sy,V&ro,V&rd){V e=camEye(),f=norm(tgt-e),r=norm(cross(f,V{0,1,0})),u=cross(r,f);float th=tanf(FOV/2),as=(float)W/std::max(H,1);
 ro=e;rd=norm(f+r*((2*sx/W-1)*th*as)+u*((1-2*sy/H)*th));}
struct Stk{bool on=false;int obj=-1;float sx0=0,sy0=0,pxw=1,s=1,Rb=1;V r,u;M3 B,Qi;bool okA=false;std::vector<int> g1,g2;std::vector<float> k1,k2;std::vector<V> p1,p2;std::unordered_map<int,V> before;};
Stk gSt;
struct SUndo{int mesh=-1;std::vector<int> w;std::vector<V> oldp,newp;};
std::vector<SUndo> gSU,gSR;
void smRaw(int w,V np){gSM.wp[w]=np;if(!gSFlag[w]){gSFlag[w]=1;gSDirty.push_back(w);}}
void sSet(int w,V np){if(!gSt.before.count(w))gSt.before[w]=gSM.wp[w];smRaw(w,np);}
void sFlush(){SM&s=gSM;if(gSDirty.empty())return;std::vector<int> aff;aff.reserve(gSDirty.size()*4);
 for(int w:gSDirty){if(!gSNF[w]){gSNF[w]=1;aff.push_back(w);}for(int k=s.ns[w];k<s.ns[w+1];k++){int q=s.nl[k];if(!gSNF[q]){gSNF[q]=1;aff.push_back(q);}}}
 for(int w:aff)smNormalFor(w);
 Mesh&m=meshes[s.mesh];int mn=INT_MAX,mx=-1;
 for(int w:aff){gSNF[w]=0;for(int k=s.ws[w];k<s.ws[w+1];k++){int si=s.wl[k];float*c=&m.cpu[(size_t)si*6];c[0]=s.wp[w].x;c[1]=s.wp[w].y;c[2]=s.wp[w].z;c[3]=s.wn[w].x;c[4]=s.wn[w].y;c[5]=s.wn[w].z;mn=std::min(mn,si);mx=std::max(mx,si);}}
 for(int w:gSDirty)gSFlag[w]=0;gSDirty.clear();
 if(mx>=mn&&m.vbo){glBindBuffer(GL_ARRAY_BUFFER,m.vbo);glBufferSubData(GL_ARRAY_BUFFER,(GLintptr)mn*24,(GLsizeiptr)(mx-mn+1)*24,&m.cpu[(size_t)mn*6]);}}
V sAvgN(V c,float Rb){SM&s=gSM;V n{};float R=Rb*.6f,R2=R*R;for(size_t w=0;w<s.wp.size();w++){V d=s.wp[w]-c;float dd=dot(d,d);if(dd>=R2)continue;n=n+s.wn[w]*sFall(sqrtf(dd),R);}
 float l=vlen(n);return l>1e-9f?n*(1/l):V{0,1,0};}
void sApplyAt(V c,V n,float Rb,int brush,bool inv,float str){SM&s=gSM;float sg=inv?-1.f:1.f,R2=Rb*Rb;size_t nw=s.wp.size();
 if(brush==2){std::vector<std::pair<int,V>> up;
  for(size_t w=0;w<nw;w++){V d=s.wp[w]-c;float dd=dot(d,d);if(dd>=R2)continue;float f=sFall(sqrtf(dd),Rb);int a=s.ns[w],b=s.ns[w+1];if(b<=a)continue;
   V avg{};for(int k=a;k<b;k++)avg=avg+s.wp[s.nl[k]];avg=avg*(1.f/(b-a));up.push_back({(int)w,s.wp[w]+(avg-s.wp[w])*(str*.6f*f*(sg>0?1.f:-.5f))});}
  for(auto&q:up)sSet(q.first,q.second);return;}
 float amt=str*.06f*Rb;
 for(size_t w=0;w<nw;w++){V d=s.wp[w]-c;float dd=dot(d,d);if(dd>=R2)continue;float f=sFall(sqrtf(dd),Rb);V p=s.wp[w],np;
  if(brush==0)np=p+n*(amt*f*sg);else if(brush==1)np=p+s.wn[w]*(amt*f*sg);else np=p-n*(dot(p-c,n)*str*.5f*f*sg);
  sSet((int)w,np);}}
bool sProbe(float x,float y,std::vector<V>&pp,V&cB,SHit&h){if(gSt.obj<0)return false;Mesh&m=meshes[objs[gSt.obj].mesh];smPose(gSt.obj,pp);V ro,rd;sRay(x,y,ro,rd);if(!smRay(pp,ro,rd,h))return false;
 size_t t=(size_t)h.tri*3;const float*c0=&m.cpu[t*6],*c1=&m.cpu[(t+1)*6],*c2=&m.cpu[(t+2)*6];V b0{c0[0],c0[1],c0[2]},b1{c1[0],c1[1],c1[2]},b2{c2[0],c2[1],c2[2]};
 cB=b0*(1-h.u-h.v)+b1*h.u+b2*h.v;V q0=pp[t],q1=pp[t+1],q2=pp[t+2];
 float eb=vlen(b1-b0),eq=vlen(q1-q0);gSt.s=eb>1e-9f?eq/eb:1;gSt.pxw=2*h.t*tanf(FOV/2)/std::max(H,1);gSt.Rb=gSRad*gSt.pxw/std::max(gSt.s,1e-9f);
 V nb=cross(b1-b0,b2-b0),nq=cross(q1-q0,q2-q0);float lb=vlen(nb),lq=vlen(nq);
 if(lb>1e-14f&&lq>1e-14f){V nbs=nb*(sqrtf(lb)/lb),nqs=nq*(sqrtf(lq)/lq);M3 B{b1-b0,b2-b0,nbs},Q{q1-q0,q2-q0,nqs};bool ok=false;M3 Qi=inv3(Q,ok);gSt.B=B;gSt.Qi=Qi;gSt.okA=ok;}else gSt.okA=false;
 return true;}
bool sculptBegin(float x,float y){if(!gSculpt||sel<0)return false;Obj&o=objs[sel];
 if(o.kind||o.mesh==5||o.mesh<0||o.mesh>=(int)meshes.size()||meshes[o.mesh].cpu.empty()||gSM.mesh!=o.mesh)return false;
 gSt=Stk();gSt.obj=sel;std::vector<V> pp;V cB;SHit h;if(!sProbe(x,y,pp,cB,h)){gSt.obj=-1;return false;}
 gSt.on=true;gSt.sx0=x;gSt.sy0=y;gSCx=x;gSCy=y;
 {V e=camEye(),f=norm(tgt-e);gSt.r=norm(cross(f,V{0,1,0}));gSt.u=cross(gSt.r,f);}
 float Rb=gSt.Rb;SM&s=gSM;
 if(gSBrush==4){
  for(int pass=0;pass<2;pass++){V c=pass?V{-cB.x,cB.y,cB.z}:cB;if(pass&&(!gSSym||fabsf(cB.x)<=.15f*Rb))break;
   auto&gi=pass?gSt.g2:gSt.g1;auto&gk=pass?gSt.k2:gSt.k1;auto&gp=pass?gSt.p2:gSt.p1;
   for(size_t w=0;w<s.wp.size();w++){V d=s.wp[w]-c;float dd=dot(d,d);if(dd>=Rb*Rb)continue;gi.push_back((int)w);gk.push_back(sFall(sqrtf(dd),Rb));gp.push_back(s.wp[w]);}}
  return true;}
 V n=sAvgN(cB,Rb);sApplyAt(cB,n,Rb,gSBrush,gSInv,gSStr);
 if(gSSym&&fabsf(cB.x)>.15f*Rb)sApplyAt(V{-cB.x,cB.y,cB.z},V{-n.x,n.y,n.z},Rb,gSBrush,gSInv,gSStr);
 sFlush();return true;}
void sculptMove(float x,float y){if(!gSt.on)return;gSCx=x;gSCy=y;
 if(gSBrush==4){V dw=(gSt.r*(x-gSt.sx0)-gSt.u*(y-gSt.sy0))*gSt.pxw;V db=gSt.okA?mul3(gSt.B,mul3(gSt.Qi,dw)):dw*(1/std::max(gSt.s,1e-9f));
  for(size_t i=0;i<gSt.g1.size();i++)sSet(gSt.g1[i],gSt.p1[i]+db*gSt.k1[i]);
  V dm{-db.x,db.y,db.z};for(size_t i=0;i<gSt.g2.size();i++)sSet(gSt.g2[i],gSt.p2[i]+dm*gSt.k2[i]);
  sFlush();return;}
 std::vector<V> pp;V cB;SHit h;if(!sProbe(x,y,pp,cB,h))return;float Rb=gSt.Rb;V n=sAvgN(cB,Rb);
 sApplyAt(cB,n,Rb,gSBrush,gSInv,gSStr);
 if(gSSym&&fabsf(cB.x)>.15f*Rb)sApplyAt(V{-cB.x,cB.y,cB.z},V{-n.x,n.y,n.z},Rb,gSBrush,gSInv,gSStr);
 sFlush();}
void sculptEnd(){if(!gSt.on)return;gSt.on=false;
 if(!gSt.before.empty()){SUndo u;u.mesh=gSM.mesh;for(auto&kv:gSt.before){u.w.push_back(kv.first);u.oldp.push_back(kv.second);u.newp.push_back(gSM.wp[kv.first]);}
  gSU.push_back(std::move(u));if(gSU.size()>30)gSU.erase(gSU.begin());gSR.clear();}
 gSt.before.clear();gSt.g1.clear();gSt.g2.clear();gSt.k1.clear();gSt.k2.clear();gSt.p1.clear();gSt.p2.clear();gSigS=-1;}
static void sApplyUndo(const SUndo&u,bool useOld){if(u.mesh!=gSM.mesh)return;for(size_t i=0;i<u.w.size();i++)smRaw(u.w[i],useOld?u.oldp[i]:u.newp[i]);sFlush();gSigS=-1;}
void sculptUndo(){if(gSU.empty())return;SUndo u=std::move(gSU.back());gSU.pop_back();sApplyUndo(u,true);gSR.push_back(std::move(u));toast("Desfeito");}
void sculptRedo(){if(gSR.empty())return;SUndo u=std::move(gSR.back());gSR.pop_back();sApplyUndo(u,false);gSU.push_back(std::move(u));toast("Refeito");}
static bool sculptable(int i){return i>=0&&i<(int)objs.size()&&!objs[i].kind&&objs[i].mesh!=5&&objs[i].mesh>=0&&objs[i].mesh<(int)meshes.size()&&!meshes[objs[i].mesh].cpu.empty();}
int cloneMesh(int mi){Mesh src=meshes[mi];Mesh mm=upload(src.cpu,src.mode);mm.rad=src.rad;if(src.skinned&&!src.skin.empty())attachSkin(mm,src.skin);if(!src.uv.empty())attachUV(mm,src.uv);meshes.push_back(mm);return(int)meshes.size()-1;}
static void sculptPrep(int oi){ // malhas padrao (cubo, esfera, plano...) sao compartilhadas: esculpir cria uma copia propria do objeto
 if(oi<0||oi>=(int)objs.size())return;int mi=objs[oi].mesh;if(mi>=0&&mi<6&&!meshes[mi].cpu.empty())objs[oi].mesh=cloneMesh(mi);}
void sculptToggle(){if(gSculpt){gSculpt=false;gSt.on=false;gCanUndo=gSceneCU;gCanRedo=gSceneCR;toast("Escultura desligada");return;}
 if(!sculptable(sel)){toast("Selecione um objeto com malha para esculpir");return;}
 if(meshes[objs[sel].mesh].cpu.size()/6>900000){toast("Malha pesada demais para esculpir (limite: 300 mil triangulos)");return;}
 sculptPrep(sel);smBuild(objs[sel].mesh);gSU.clear();gSR.clear();gSculpt=true;gRenderOpen=false;gRigOpen=false;gFileMenu=false;gTlOpen=false;gToolFan=gAddFan=false;toast("Escultura: 1 dedo esculpe, 2 dedos movem a camera");}
void sculptSync(){if(!gSculpt)return;
 if(!sculptable(sel)){gSculpt=false;gSt.on=false;gCanUndo=gSceneCU;gCanRedo=gSceneCR;return;}
 sculptPrep(sel);int mi=objs[sel].mesh;if(gSM.mesh!=mi||gSM.nsv!=meshes[mi].cpu.size()/6){gSt.on=false;smBuild(mi);gSU.clear();gSR.clear();}
 gCanUndo=!gSU.empty();gCanRedo=!gSR.empty();}
void gatherTris(){gTD.clear();std::vector<std::vector<M>> JS(skins.size());
 for(size_t s2=0;s2<skins.size();s2++){Skin&sk=skins[s2];JS[s2].resize(sk.j.size());
  for(size_t k=0;k<sk.j.size();k++){M jw=sk.j[k]>=0?worldM(sk.j[k]):(k<sk.fz.size()?sk.fz[k]:id());JS[s2][k]=mul(jw,sk.ibm[k]);}}
 for(int i=0;i<(int)objs.size()&&gTD.size()<150000;i++){Obj&o=objs[i];if(o.mesh==5||o.kind||o.hide||o.nor)continue;Mesh&m=meshes[o.mesh];if(m.cpu.empty())continue;
  M w=worldM(i);bool sk=o.skin>=0&&o.skin<(int)skins.size()&&m.skinned;V col{powf(o.col.x,2.2f),powf(o.col.y,2.2f),powf(o.col.z,2.2f)};
  for(size_t v=0;v+17<m.cpu.size();v+=18){TriD t;t.col=col;t.met=o.met;t.rou=o.rou;t.tb=(float)o.tb;t.tm=(float)o.tm;t.tn=(float)o.tn;t.ac=o.ac;
   for(int k=0;k<3;k++){const float*c=&m.cpu[v+6*k];V pp{c[0],c[1],c[2]},nn{c[3],c[4],c[5]};
    if(sk){const float*sw=&m.skin[(v/6+k)*8];V ap,an;for(int q=0;q<4;q++){float wq=sw[4+q];if(wq<=0)continue;int ji=(int)sw[q];if(ji>=(int)JS[o.skin].size())continue;
      ap=ap+xf(JS[o.skin][ji],pp)*wq;an=an+xn(JS[o.skin][ji],nn)*wq;}pp=ap;nn=an;}
    else{pp=xf(w,pp);nn=xn(w,nn);}
    float l=sqrtf(dot(nn,nn));t.p[k]=pp;t.n[k]=l>1e-8f?nn*(1/l):V{0,1,0};size_t ui=(v/6+k)*2;if(m.uv.size()>=ui+2){t.uv[k][0]=m.uv[ui];t.uv[k][1]=m.uv[ui+1];}}
   gTD.push_back(t);}}
 if(gMcGround){TriD a;a.col=V{.25f,.25f,.25f};a.rou=.85f;V q[4]={{-60,-.002f,-60},{60,-.002f,-60},{60,-.002f,60},{-60,-.002f,60}};
  for(int k=0;k<3;k++)a.n[k]=V{0,1,0};TriD b=a;a.p[0]=q[0];a.p[1]=q[2];a.p[2]=q[1];b.p[0]=q[0];b.p[1]=q[3];b.p[2]=q[2];gTD.push_back(a);gTD.push_back(b);}
 if(gTD.empty()){TriD t;t.p[0]=V{1e6f,1e6f,1e6f};t.p[1]=V{1e6f+1,1e6f,1e6f};t.p[2]=V{1e6f,1e6f+1,1e6f};gTD.push_back(t);}}
int bvhRec(int b,int e){int id_=(int)gBN.size();gBN.push_back(BN());BN n;n.mn=V{1e30f,1e30f,1e30f};n.mx=V{-1e30f,-1e30f,-1e30f};V cmn=n.mn,cmx=n.mx;
 for(int i=b;i<e;i++){TriD&t=gTD[gOrd[i]];for(int k=0;k<3;k++){n.mn=vmin(n.mn,t.p[k]);n.mx=vmax(n.mx,t.p[k]);}cmn=vmin(cmn,gCen[gOrd[i]]);cmx=vmax(cmx,gCen[gOrd[i]]);}
 if(e-b<=4){n.lf=b;n.cnt=e-b;gBN[id_]=n;return id_;}
 V ex=cmx-cmn;int ax=(ex.x>ex.y&&ex.x>ex.z)?0:(ex.y>ex.z?1:2);int m=(b+e)/2;
 std::nth_element(gOrd.begin()+b,gOrd.begin()+m,gOrd.begin()+e,[&](int a,int c){return (&gCen[a].x)[ax]<(&gCen[c].x)[ax];});
 bvhRec(b,m);int r=bvhRec(m,e);n.lf=r;n.cnt=0;gBN[id_]=n;return id_;}
void mcBuild(){gatherTris();int n=(int)gTD.size();gOrd.resize(n);gCen.resize(n);
 for(int i=0;i<n;i++){gOrd[i]=i;gCen[i]=(gTD[i].p[0]+gTD[i].p[1]+gTD[i].p[2])*(1.f/3);}
 gBN.clear();bvhRec(0,n);std::vector<float> nd,tr;
 auto bits=[](int v){float f;memcpy(&f,&v,4);return f;};
 for(auto&b:gBN)nd.insert(nd.end(),{b.mn.x,b.mn.y,b.mn.z,bits(b.lf),b.mx.x,b.mx.y,b.mx.z,bits(b.cnt)});
 for(int i=0;i<n;i++){TriD&t=gTD[gOrd[i]];for(int k=0;k<3;k++)tr.insert(tr.end(),{t.p[k].x,t.p[k].y,t.p[k].z,t.uv[k][0]});
  for(int k=0;k<3;k++)tr.insert(tr.end(),{t.n[k].x,t.n[k].y,t.n[k].z,t.uv[k][1]});tr.insert(tr.end(),{t.col.x,t.col.y,t.col.z,t.met,t.rou,t.tb+((t.ac>.5f&&t.tb>=0)?.5f:0.f),t.tm,t.tn});}
 glBindBuffer(GL_SHADER_STORAGE_BUFFER,gMcNB);glBufferData(GL_SHADER_STORAGE_BUFFER,nd.size()*4,nd.data(),GL_DYNAMIC_DRAW);
 glBindBuffer(GL_SHADER_STORAGE_BUFFER,gMcTB);glBufferData(GL_SHADER_STORAGE_BUFFER,tr.size()*4,tr.data(),GL_DYNAMIC_DRAW);}
void initMC(){gMcOK=false;gMcW=gMcH=0;gMcTex[0]=gMcTex[1]=0;gMcOutF=gMcOutT=0;gMcAov[0]=gMcAov[1]=gMcAlb[0]=gMcAlb[1]=gMcDnT[0]=gMcDnT[1]=0;gMcDnOK=false;gMcSamples=0;gSigS=gSigC=-1;gMcHaveCam=false;
 GLint ma=0,mi=0;glGetIntegerv(GL_MAJOR_VERSION,&ma);glGetIntegerv(GL_MINOR_VERSION,&mi);if(ma<3||(ma==3&&mi<1))return;
 GLuint p=glCreateProgram();glAttachShader(p,sh(GL_COMPUTE_SHADER,MCCS));glLinkProgram(p);GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);
 if(!ok){char b0[512];glGetProgramInfoLog(p,512,0,b0);__android_log_print(ANDROID_LOG_ERROR,"NA","MC(luzes): %s",b0);glDeleteProgram(p);p=glCreateProgram();glAttachShader(p,sh(GL_COMPUTE_SHADER,MCCS0));glLinkProgram(p);glGetProgramiv(p,GL_LINK_STATUS,&ok);}
 if(!ok){char b[512];glGetProgramInfoLog(p,512,0,b);__android_log_print(ANDROID_LOG_ERROR,"NA","MC: %s",b);return;}
 gMcProg=p;mcSz=glGetUniformLocation(p,"uSize");mcOff=glGetUniformLocation(p,"uOffY");mcFirst=glGetUniformLocation(p,"uFirst");mcFr=glGetUniformLocation(p,"uFrame");
 mcBn=glGetUniformLocation(p,"uBounces");mcLeak=glGetUniformLocation(p,"uLeak");mcCam=glGetUniformLocation(p,"uCam");mcF=glGetUniformLocation(p,"uF");mcR=glGetUniformLocation(p,"uR");mcU=glGetUniformLocation(p,"uU");
 mcTH=glGetUniformLocation(p,"uTH");mcSD=glGetUniformLocation(p,"uSunD");mcSC=glGetUniformLocation(p,"uSunC");mcSCos=glGetUniformLocation(p,"uSunCos");mcNL=glGetUniformLocation(p,"uNL");mcLP=glGetUniformLocation(p,"uLP");mcLD=glGetUniformLocation(p,"uLD");mcLC=glGetUniformLocation(p,"uLC");mcSky=glGetUniformLocation(p,"uSky");mcPrv=glGetUniformLocation(p,"uPrev");
 const char*qv="#version 300 es\nlayout(location=0) in vec3 aP; out vec2 vUV; void main(){vUV=aP.xy;gl_Position=vec4(aP.xy*2.0-1.0,0.0,1.0);}";
 gMcTm=mkProg(qv,MCTM);tmAcc=glGetUniformLocation(gMcTm,"uAcc");tmSz=glGetUniformLocation(gMcTm,"uSz");mcAovPrev=glGetUniformLocation(p,"uAovPrev");mcN=glGetUniformLocation(p,"uN");mcAlbPrev=glGetUniformLocation(p,"uAlbPrev");mcTexA=glGetUniformLocation(p,"uTexA");
 {GLuint dp=glCreateProgram();glAttachShader(dp,sh(GL_COMPUTE_SHADER,DNCS));glLinkProgram(dp);GLint ok2=0;glGetProgramiv(dp,GL_LINK_STATUS,&ok2);
  if(ok2){gDnProg=dp;dnSrc=glGetUniformLocation(dp,"uSrc");dnAov=glGetUniformLocation(dp,"uAov");dnSz=glGetUniformLocation(dp,"uSize");
   dnStep=glGetUniformLocation(dp,"uStep");dnI0=glGetUniformLocation(dp,"uIter0");dnPhi=glGetUniformLocation(dp,"uPhiC");dnAlb=glGetUniformLocation(dp,"uAlb");dnLast=glGetUniformLocation(dp,"uLast");}else gDnProg=0;}
 glGenBuffers(1,&gMcNB);glGenBuffers(1,&gMcTB);glUseProgram(gProg);gMcOK=true;}
void mcResize(int w,int h){
 if(gMcTex[0]){glDeleteTextures(2,gMcTex);glDeleteTextures(2,gMcAov);glDeleteTextures(2,gMcAlb);glDeleteTextures(2,gMcDnT);glDeleteTextures(1,&gMcOutT);glDeleteFramebuffers(1,&gMcOutF);}
 glGenTextures(2,gMcTex);
 for(int i=0;i<2;i++){glBindTexture(GL_TEXTURE_2D,gMcTex[i]);glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA32F,w,h);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);}
 glGenTextures(2,gMcAov);glGenTextures(2,gMcDnT);glGenTextures(2,gMcAlb);
 for(int i=0;i<2;i++){glBindTexture(GL_TEXTURE_2D,gMcAlb[i]);glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA16F,w,h);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);}
 for(int i=0;i<4;i++){glBindTexture(GL_TEXTURE_2D,i<2?gMcAov[i]:gMcDnT[i-2]);glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA16F,w,h);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);}
 glGenTextures(1,&gMcOutT);glBindTexture(GL_TEXTURE_2D,gMcOutT);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
 glGenFramebuffers(1,&gMcOutF);glBindFramebuffer(GL_FRAMEBUFFER,gMcOutF);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,gMcOutT,0);
 glBindFramebuffer(GL_FRAMEBUFFER,0);gMcW=w;gMcH=h;gMcCur=0;mcReset();}
double sigScene(){double s2=0;for(int i=0;i<(int)objs.size();i++){Obj&o=objs[i];M w=worldM(i);for(int k=0;k<16;k++)s2+=w.m[k]*(k+1)*(i+1)*.37;
  s2+=(o.col.x+o.col.y*2+o.col.z*3+o.met*5+o.rou*7+o.len)*(i+1);s2+=(o.fov*13+o.shape*17+o.kind)*(i+1);s2+=((o.hide?3:0)+(o.nor?5:0))*(i+1)*1.7;}return s2+objs.size()*1000.0+(gMcGround?7:0);}
double sigCam(){return yaw*3.1+pitch*7.3+cd*11.7+tgt.x+tgt.y*2+tgt.z*3;}
bool mcShow(){return gMcOK&&(gMcPrev||gMcFinal)&&gMcSamples>0&&gMcTex[0];}
void mcStep(){
 if(!gMcOK||!(gMcPrev||gMcFinal))return;
 int tw_,th_;if(gMcFinal){if(gMcCW>0&&gMcCH>0){tw_=gMcCW;th_=gMcCH;}else{float sc=std::min(1.f,(float)gMcFinW/W);tw_=(int)(W*sc);th_=(int)(H*sc);}}else{tw_=std::max(W/2,64);th_=std::max(H/2,64);}
 if(tw_!=gMcW||th_!=gMcH){mcResize(tw_,th_);gSigS=-1;gMcHaveCam=false;}
 double now=nowSec();V e=camEye();V f=norm(tgt-e),r=norm(cross(f,V{0,1,0})),u=cross(r,f);
 float rfov=FOV;{V e2,f2,r2,u2;float fv=FOV;if(gMcFinal&&camRender(e2,f2,r2,u2,fv)){e=e2;f=f2;r=r2;u=u2;rfov=fv;}}
 if(gMcFinal){if(!gMcHaveCam){mcBuild();gCE=e;gCF=f;gCR=r;gCU=u;gCFov=rfov;mcReset();gMcHaveCam=true;}e=gCE;f=gCF;r=gCR;u=gCU;rfov=gCFov;}
 else{double ss=sigScene(),sc2=sigCam();bool rs=false;
  if(ss!=gSigS&&now-gMcLast>.15){mcBuild();gSigS=ss;gMcLast=now;rs=true;}
  if(sc2!=gSigC){gSigC=sc2;rs=true;}if(rs)mcReset();}
 int cap=gMcFinal?gMcSpp:128;if(gMcSamples>=cap)return;
 float hiT=gVidBusy?.2f:.045f,loT=gVidBusy?.12f:.028f;if(gDt>hiT)gMcRows=std::max(8,gMcRows*3/4);else if(gDt<loT)gMcRows=std::min(gMcH,gMcRows*5/4+1);
 int rows=std::min(gMcRows,gMcH-gMcRow);
 glUseProgram(gMcProg);glBindBufferBase(GL_SHADER_STORAGE_BUFFER,0,gMcNB);glBindBufferBase(GL_SHADER_STORAGE_BUFFER,1,gMcTB);
 glBindImageTexture(0,gMcTex[gMcCur^1],0,GL_FALSE,0,GL_WRITE_ONLY,GL_RGBA32F);
 glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,gMcTex[gMcCur]);glUniform1i(mcPrv,0);
 glBindImageTexture(1,gMcAov[gMcCur^1],0,GL_FALSE,0,GL_WRITE_ONLY,GL_RGBA16F);glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,gMcAov[gMcCur]);glUniform1i(mcAovPrev,1);glBindImageTexture(2,gMcAlb[gMcCur^1],0,GL_FALSE,0,GL_WRITE_ONLY,GL_RGBA16F);glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,gMcAlb[gMcCur]);glUniform1i(mcAlbPrev,2);glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D_ARRAY,gTexArr);glUniform1i(mcTexA,3);glActiveTexture(GL_TEXTURE0);glUniform1i(mcN,gMcSamples+1);
 glUniform2i(mcSz,gMcW,gMcH);glUniform1i(mcOff,gMcRow);glUniform1i(mcFirst,gMcSamples==0?1:0);glUniform1i(mcFr,gMcFrame);glUniform1i(mcBn,gMcFinal?gMcBn:2);glUniform1i(mcLeak,gMcLeak?1:0);
 glUniform3f(mcCam,e.x,e.y,e.z);glUniform3f(mcF,f.x,f.y,f.z);glUniform3f(mcR,r.x,r.y,r.z);glUniform3f(mcU,u.x,u.y,u.z);
 float th=tanf(rfov/2);glUniform2f(mcTH,th*(float)gMcW/gMcH,th);
 V sd=norm(V{.4f,.8f,.5f});glUniform3f(mcSD,sd.x,sd.y,sd.z);
 {float LP[32],LD[32],LC[32];int nl=0;bool hs=false;
  for(int i=0;i<(int)objs.size()&&nl<8;i++){Obj&o=objs[i];if(o.kind!=4)continue;M w=worldM(i);V dr=norm(V{-w.m[4],-w.m[5],-w.m[6]});int ty=std::clamp(o.shape,0,2);
   float*pp=&LP[nl*4],*dd=&LD[nl*4],*cc=&LC[nl*4];pp[0]=w.m[12];pp[1]=w.m[13];pp[2]=w.m[14];pp[3]=(float)ty;dd[0]=dr.x;dd[1]=dr.y;dd[2]=dr.z;dd[3]=cosf(o.fov);
   cc[0]=powf(o.col.x,2.2f)*o.len;cc[1]=powf(o.col.y,2.2f)*o.len;cc[2]=powf(o.col.z,2.2f)*o.len;cc[3]=0;if(ty==0)hs=true;nl++;}
  if(hs)glUniform3f(mcSC,0.f,0.f,0.f);else if(gMcNight)glUniform3f(mcSC,.35f,.45f,.75f);else glUniform3f(mcSC,3.2f,3.0f,2.8f);glUniform1f(mcSky,gMcNight?.06f:1.f);
  glUniform1i(mcNL,nl);if(nl>0){glUniform4fv(mcLP,nl,LP);glUniform4fv(mcLD,nl,LD);glUniform4fv(mcLC,nl,LC);}}
 glUniform1f(mcSCos,.9995f);
 glDispatchCompute((gMcW+7)/8,(rows+7)/8,1);
 glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT|GL_SHADER_STORAGE_BARRIER_BIT);
 gMcRow+=rows;if(gMcRow>=gMcH){gMcCur^=1;gMcSamples++;gMcRow=0;gMcFrame++;gDnDirty=true;}
 glUseProgram(gProg);}
void mcDenoise(){if(!gMcDn||!gDnProg||!gMcTex[0])return;int iters=gMcFinal?4:3;glUseProgram(gDnProg);
 glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,gMcAov[gMcCur]);glUniform1i(dnAov,1);glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,gMcAlb[gMcCur]);glUniform1i(dnAlb,2);
 for(int k=0;k<iters;k++){GLuint src=k==0?gMcTex[gMcCur]:gMcDnT[(k-1)&1];
  glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,src);glUniform1i(dnSrc,0);
  glBindImageTexture(0,gMcDnT[k&1],0,GL_FALSE,0,GL_WRITE_ONLY,GL_RGBA16F);
  glUniform2i(dnSz,gMcW,gMcH);glUniform1i(dnStep,1<<k);glUniform1i(dnI0,k==0?1:0);glUniform1i(dnLast,k==iters-1?1:0);glUniform1f(dnPhi,.15f+2.f/sqrtf((float)std::max(gMcSamples,1)));
  glDispatchCompute((gMcW+7)/8,(gMcH+7)/8,1);glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT|GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);}
 gMcDnRes=(iters-1)&1;gMcDnOK=true;glActiveTexture(GL_TEXTURE0);glUseProgram(gProg);}
void mcDraw(){glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glUseProgram(gMcTm);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,(gMcDn&&gMcDnOK)?gMcDnT[gMcDnRes]:gMcTex[gMcCur]);
 glUniform1i(tmAcc,0);glUniform2i(tmSz,gMcW,gMcH);glBindVertexArray(meshes[3].vao);glDrawArrays(GL_TRIANGLES,0,6);glEnable(GL_BLEND);glUseProgram(gProg);}
void mcFrame(){mcStep();if(gDnDirty){if(gMcDn&&mcShow()&&(!gVidBusy||gMcSamples>=gMcSpp))mcDenoise();gDnDirty=false;}if(!mcShow())return;{int vx=0,vy=0,vw=W,vh=H;if(gMcFinal&&gMcW>0&&gMcH>0){float ra=(float)gMcW/gMcH,sa=(float)W/H;if(ra>sa+.002f){vh=(int)(W/ra);vy=(H-vh)/2;}else if(ra<sa-.002f){vw=(int)(H*ra);vx=(W-vw)/2;}}
  if(vw!=W||vh!=H){glClearColor(.08f,.08f,.09f,1);glClear(GL_COLOR_BUFFER_BIT);}
  glViewport(vx,vy,vw,vh);mcDraw();glViewport(0,0,W,H);}}
static void mcWrite(void*c,void*d,int n){fwrite(d,1,n,(FILE*)c);}
void mcSaveTo(const std::string&path){if(!mcShow())return;
 glBindFramebuffer(GL_FRAMEBUFFER,gMcOutF);glViewport(0,0,gMcW,gMcH);mcDraw();
 std::vector<unsigned char> px((size_t)gMcW*gMcH*4);glReadPixels(0,0,gMcW,gMcH,GL_RGBA,GL_UNSIGNED_BYTE,px.data());
 glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,W,H);
 for(int y=0;y<gMcH/2;y++)for(int x=0;x<gMcW*4;x++)std::swap(px[(size_t)y*gMcW*4+x],px[(size_t)(gMcH-1-y)*gMcW*4+x]);
 FILE*f=fopen(path.c_str(),"wb");if(!f)return;stbi_write_png_to_func(mcWrite,f,gMcW,gMcH,4,px.data(),gMcW*4);fclose(f);}
void mcSave(){if(!mcShow()){toast("Nada para salvar");return;}
 glBindFramebuffer(GL_FRAMEBUFFER,gMcOutF);glViewport(0,0,gMcW,gMcH);mcDraw();
 std::vector<unsigned char> px((size_t)gMcW*gMcH*4);glReadPixels(0,0,gMcW,gMcH,GL_RGBA,GL_UNSIGNED_BYTE,px.data());
 glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,W,H);
 for(int y=0;y<gMcH/2;y++)for(int x=0;x<gMcW*4;x++)std::swap(px[(size_t)y*gMcW*4+x],px[(size_t)(gMcH-1-y)*gMcW*4+x]);
 std::string path;bool fb;FILE*f=openOut("png",path,fb);if(!f){toast("Erro ao salvar");return;}
 stbi_write_png_to_func(mcWrite,f,gMcW,gMcH,4,px.data(),gMcW*4);fclose(f);saved(path,fb);}
// ---------- render de video (MP4 via MediaCodec; plano B: sequencia de PNG) ----------
struct Vid{bool on=false,png=false,mxOn=false,fb=false;int fps=24;float t0=0;int f0=1,f1=1,f=1,done=0,total=1,vw=0,vh=0,cf=21,stage=0,fd=-1,trk=-1;float tm0=0;int spp0=64,fw0=1920,bn0=6;AMediaCodec*cd=nullptr;AMediaMuxer*mx=nullptr;std::string path,dir;};
Vid gVid;
bool vidDrain(bool eos){ // esvazia a saida do codec; true quando chegou o fim do stream
 AMediaCodecBufferInfo info;
 for(int guard=0;guard<(eos?300:64);guard++){
  ssize_t o=AMediaCodec_dequeueOutputBuffer(gVid.cd,&info,eos?10000:0);
  if(o==-1){if(!eos)return false;continue;} // sem saida ainda
  if(o==-2){AMediaFormat*of=AMediaCodec_getOutputFormat(gVid.cd);gVid.trk=(int)AMediaMuxer_addTrack(gVid.mx,of);AMediaFormat_delete(of);AMediaMuxer_start(gVid.mx);gVid.mxOn=true;continue;}
  if(o<0)continue;
  size_t sz=0;uint8_t*b=AMediaCodec_getOutputBuffer(gVid.cd,(size_t)o,&sz);
  if(info.flags&2)info.size=0; // dados de configuracao ja vao no formato
  if(gVid.mxOn&&info.size>0&&b)AMediaMuxer_writeSampleData(gVid.mx,(size_t)gVid.trk,b,&info);
  bool end=(info.flags&4)!=0;
  AMediaCodec_releaseOutputBuffer(gVid.cd,(size_t)o,false);
  if(end)return true;}
 return false;}
void vidEncClose(bool flush){
 if(gVid.cd){
  if(flush){ssize_t ii=-1;for(int k=0;k<50&&ii<0;k++){ii=AMediaCodec_dequeueInputBuffer(gVid.cd,10000);if(ii<0)vidDrain(false);}
   if(ii>=0){AMediaCodec_queueInputBuffer(gVid.cd,(size_t)ii,0,0,(uint64_t)gVid.done*1000000/gVid.fps,4);for(int k=0;k<20;k++)if(vidDrain(true))break;}}
  AMediaCodec_stop(gVid.cd);AMediaCodec_delete(gVid.cd);gVid.cd=nullptr;}
 if(gVid.mx){if(gVid.mxOn)AMediaMuxer_stop(gVid.mx);AMediaMuxer_delete(gVid.mx);gVid.mx=nullptr;gVid.mxOn=false;}
 if(gVid.fd>=0){close(gVid.fd);gVid.fd=-1;}}
void vidToPng(){ // codec indisponivel: salva uma sequencia de PNG
 vidEncClose(false);if(!gVid.path.empty()){remove(gVid.path.c_str());gVid.path.clear();}
 gVid.png=true;long t=(long)time(nullptr);gVid.fb=false;gVid.dir=fm("/storage/emulated/0/Download/nomad_video_%ld",t);
 if(mkdir(gVid.dir.c_str(),0755)!=0&&errno!=EEXIST){gVid.dir=fm("%s/nomad_video_%ld",gDir,t);gVid.fb=true;mkdir(gVid.dir.c_str(),0755);}}
void vidFinish(bool ok){
 bool png=gVid.png,fb=gVid.fb;int n=gVid.done;std::string p=gVid.path,dr=gVid.dir;float t0=gVid.tm0;
 vidEncClose(ok&&n>0&&!png);
 gVid.on=false;gVidBusy=false;gMcSpp=gVid.spp0;gMcFinW=gVid.fw0;gMcBn=gVid.bn0;gMcFinal=false;gMcHaveCam=false;gSigS=-1;mcReset();tm=t0;applyAnim();
 if(gApp&&gApp->activity)::plat_keep_screen_on(false);
 if(n<=0){if(!png&&!p.empty())remove(p.c_str());toast("Render de video cancelado");return;}
 const char*nota=fb?"  (ative Acesso a todos os arquivos para salvar em Downloads)":"";
 if(png)toast(fm("Frames PNG salvos (%d): %s%s",n,dr.c_str(),nota));else toast(fm("Video salvo (%d frames): %s%s",n,p.c_str(),nota));}
void rgbaToYuv(const unsigned char*px,int sw,int sh,uint8_t*d){ // px: RGBA de baixo para cima (glReadPixels)
 int vw=gVid.vw,vh=gVid.vh;uint8_t*yp=d,*up=d+(size_t)vw*vh;
 auto at=[&](int x,int y)->const unsigned char*{int sx=sw==vw?x:std::min(sw-1,(int)((long)x*sw/vw)),sy=sh==vh?y:std::min(sh-1,(int)((long)y*sh/vh));return px+((size_t)(sh-1-sy)*sw+sx)*4;};
 for(int y=0;y<vh;y++)for(int x=0;x<vw;x++){const unsigned char*p=at(x,y);yp[(size_t)y*vw+x]=(uint8_t)std::clamp(((47*p[0]+157*p[1]+16*p[2]+128)>>8)+16,16,235);}
 for(int y=0;y<vh/2;y++)for(int x=0;x<vw/2;x++){const unsigned char*p=at(2*x,2*y);const unsigned char*p2=at(2*x+1,2*y+1);
  int r=(p[0]+p2[0])/2,g=(p[1]+p2[1])/2,b=(p[2]+p2[2])/2;
  int cu=std::clamp(((-26*r-87*g+112*b+128)>>8)+128,16,240),cv=std::clamp(((112*r-102*g-10*b+128)>>8)+128,16,240);
  if(gVid.cf==21){up[(size_t)y*vw+2*x]=(uint8_t)cu;up[(size_t)y*vw+2*x+1]=(uint8_t)cv;}
  else{up[(size_t)y*(vw/2)+x]=(uint8_t)cu;up[(size_t)(vh/2)*(vw/2)+(size_t)y*(vw/2)+x]=(uint8_t)cv;}}}
bool vidPush(const unsigned char*px,int sw,int sh){
 if(!gVid.png){
  size_t need=(size_t)gVid.vw*gVid.vh*3/2;bool fed=false;
  for(int tries=0;tries<300&&!fed;tries++){
   ssize_t ii=AMediaCodec_dequeueInputBuffer(gVid.cd,10000);
   if(ii>=0){size_t cap=0;uint8_t*b=AMediaCodec_getInputBuffer(gVid.cd,(size_t)ii,&cap);
    if(!b||cap<need){if(gVid.done==0){vidToPng();break;}return false;}
    rgbaToYuv(px,sw,sh,b);AMediaCodec_queueInputBuffer(gVid.cd,(size_t)ii,0,need,(uint64_t)gVid.done*1000000/gVid.fps,0);fed=true;}
   else vidDrain(false);}
  if(fed){vidDrain(false);return true;}
  if(!gVid.png)return false;}
 std::vector<unsigned char> fl(px,px+(size_t)sw*sh*4);
 for(int y=0;y<sh/2;y++)for(int x=0;x<sw*4;x++)std::swap(fl[(size_t)y*sw*4+x],fl[(size_t)(sh-1-y)*sw*4+x]);
 std::string pth=fm("%s/frame_%04d.png",gVid.dir.c_str(),gVid.f);FILE*f=fopen(pth.c_str(),"wb");if(!f)return false;
 stbi_write_png_to_func(mcWrite,f,sw,sh,4,fl.data(),sw*4);fclose(f);return true;}
void vidStart(){
 if(!gMcOK){toast("Render de video precisa de ES 3.1");return;}
 if(!filesGranted()){needFiles();return;}
 gVid=Vid();{int ra=gRA>=0?gRA:std::min(gFS,gFE),rb=gRB>=0?gRB:std::max(gFS,gFE);if(rb<ra)std::swap(ra,rb);gVid.fps=std::clamp(gVidFps,1,120);gVid.t0=ra/24.f;gVid.f0=0;gVid.total=std::max(1,(int)floorf((rb-ra)/24.f*gVid.fps+1e-3f)+1);gVid.f1=gVid.total-1;gVid.f=0;gVid.tm0=tm;}
 static const int PW[3]={1024,1280,1920},PB[3]={6,8,12},PS[3]={8,16,0};
 gVid.spp0=gMcSpp;gVid.fw0=gMcFinW;gVid.bn0=gMcBn;gMcFinW=PW[gVidQ];gMcBn=PB[gVidQ];if(PS[gVidQ])gMcSpp=PS[gVidQ];gVidBusy=true;
 float sc=std::min(1.f,(float)gMcFinW/W);int sw=(int)(W*sc),sh=(int)(H*sc);
 gVid.vw=gMcFinW;gVid.vh=std::max(16,(int)(gVid.vw*(double)sh/std::max(sw,1)+8)/16*16);
 if(gMcCW>0&&gMcCH>0){gVid.vw=(gMcCW+15)/16*16;gVid.vh=(gMcCH+15)/16*16;} // tamanho personalizado
 long t=(long)time(nullptr);bool ok=false;
 for(int cf:{21,19}){if(ok)break;
  AMediaFormat*f_=AMediaFormat_new();
  AMediaFormat_setString(f_,"mime","video/avc");AMediaFormat_setInt32(f_,"width",gVid.vw);AMediaFormat_setInt32(f_,"height",gVid.vh);
  AMediaFormat_setInt32(f_,"bitrate",std::clamp((int)((double)gVid.vw*gVid.vh*gVid.fps*.25),2000000,40000000));
  AMediaFormat_setInt32(f_,"frame-rate",gVid.fps);AMediaFormat_setInt32(f_,"i-frame-interval",1);AMediaFormat_setInt32(f_,"color-format",cf);
  AMediaFormat_setInt32(f_,"color-standard",1);AMediaFormat_setInt32(f_,"color-range",2);AMediaFormat_setInt32(f_,"color-transfer",3);
  AMediaCodec*c=AMediaCodec_createEncoderByType("video/avc");
  if(c){if(AMediaCodec_configure(c,f_,nullptr,nullptr,1)==AMEDIA_OK&&AMediaCodec_start(c)==AMEDIA_OK){gVid.cd=c;gVid.cf=cf;ok=true;}else AMediaCodec_delete(c);}
  AMediaFormat_delete(f_);}
 if(ok){gVid.path=fm("/storage/emulated/0/Download/nomad_%ld.mp4",t);gVid.fd=open(gVid.path.c_str(),O_CREAT|O_RDWR|O_TRUNC,0644);
  if(gVid.fd<0){gVid.path=fm("%s/nomad_%ld.mp4",gDir,t);gVid.fb=true;gVid.fd=open(gVid.path.c_str(),O_CREAT|O_RDWR|O_TRUNC,0644);}
  if(gVid.fd>=0)gVid.mx=AMediaMuxer_new(gVid.fd,AMEDIAMUXER_OUTPUT_FORMAT_MPEG_4);
  if(!gVid.mx){vidEncClose(false);ok=false;}}
 if(!ok)vidToPng();
 gVid.on=true;gVid.stage=0;
 if(gApp&&gApp->activity)::plat_keep_screen_on(true);
 toast(gVid.png?"MP4 indisponivel: salvando sequencia de PNG":"Renderizando video... mantenha o app aberto");}
void vidStep(){
 if(!gVid.on)return;
 if(gVid.stage==0){tm=gVid.t0+gVid.f/(float)gVid.fps;applyAnim();gMcFinal=true;gMcPrev=false;gMcHaveCam=false;gSigS=-1;mcReset();gVid.stage=1;return;}
 if(!(gMcFinal&&gMcSamples>=gMcSpp&&!gDnDirty&&mcShow()))return;
 glBindFramebuffer(GL_FRAMEBUFFER,gMcOutF);glViewport(0,0,gMcW,gMcH);mcDraw();
 std::vector<unsigned char> px((size_t)gMcW*gMcH*4);glReadPixels(0,0,gMcW,gMcH,GL_RGBA,GL_UNSIGNED_BYTE,px.data());
 glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,W,H);
 if(!vidPush(px.data(),gMcW,gMcH)){toast("Falha ao gravar frame");vidFinish(true);return;}
 gVid.done++;gVid.f++;
 if(gVid.f>gVid.f1){vidFinish(true);return;}
 gVid.stage=0;}
// ---------- projeto: novo / salvar / abrir / importar .mad ----------
struct Rd{const unsigned char*p;size_t n,i=0;bool ok=true;
 uint32_t u(){if(i+4>n){ok=false;return 0;}uint32_t v;memcpy(&v,p+i,4);i+=4;return v;}
 float f(){uint32_t v=u();float x;memcpy(&x,&v,4);return x;}int s(){return (int)u();}
 void b(void*d,size_t k){if(i+k>n){ok=false;memset(d,0,k);return;}memcpy(d,p+i,k);i+=k;}};
// ---------- desfazer / refazer (snapshots do estado persistente da cena) ----------
struct Snap{std::vector<Obj> o;std::vector<Skin> sk;std::vector<Rig> rg;int sel=-1;};
std::vector<Snap> gUndo,gRedo;Snap gUCur;uint64_t gUHash=0;
static void hmix(uint64_t&h,const void*p,size_t n){const unsigned char*b=(const unsigned char*)p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}}
uint64_t stateHash(){uint64_t h=1469598103934665603ULL;
 for(size_t i=0;i<objs.size();i++){const Obj&o=objs[i];
  int meta[6]={o.mesh,o.parent,o.skin,o.kind,o.shape,(int)o.k.size()};hmix(h,meta,sizeof(meta));hmix(h,o.nm,strnlen(o.nm,24));
  hmix(h,&o.col,sizeof(V));hmix(h,&o.met,4);hmix(h,&o.rou,4);hmix(h,&o.len,4);hmix(h,&o.fov,4);
  bool derived=false;for(auto&g:rigs)if((g.a==(int)i||g.b==(int)i||g.c==(int)i)&&(g.w>.001f||!g.wk.empty()))derived=true; // osso movido pelo IK
  if(o.k.empty()&&!derived)hmix(h,&o.cur,sizeof(T));
  for(auto&k:o.k){hmix(h,&k.t,4);hmix(h,&k.x,sizeof(T));}}
 for(auto&g:rigs){int m[7]={g.a,g.b,g.c,g.tgt,g.pole,(int)g.rotc,(int)g.wk.size()};hmix(h,m,sizeof(m));hmix(h,&g.w,4);for(auto&k:g.wk)hmix(h,&k,sizeof(k));}
 return h;}
Snap snapNow(){Snap s;s.o=objs;s.sk=skins;s.rg=rigs;s.sel=sel;return s;}
void undoClear(){gUndo.clear();gRedo.clear();gUCur=snapNow();gUHash=stateHash();gCanUndo=gCanRedo=gSceneCU=gSceneCR=false;}
void undoCommit(){uint64_t h=stateHash();if(h==gUHash)return;gUndo.push_back(std::move(gUCur));if(gUndo.size()>40)gUndo.erase(gUndo.begin());gRedo.clear();gUCur=snapNow();gUHash=h;gCanUndo=gSceneCU=true;gCanRedo=gSceneCR=false;}
static void undoApply(const Snap&s){objs=s.o;skins=s.sk;rigs=s.rg;sel=s.sel<(int)objs.size()?s.sel:-1;gUCur=s;gUHash=stateHash();gMcHaveCam=false;gSigS=-1;mcReset();applyAnim();gSceneCU=!gUndo.empty();gSceneCR=!gRedo.empty();if(!gSculpt){gCanUndo=gSceneCU;gCanRedo=gSceneCR;}}
void doUndo(){undoCommit();if(gUndo.empty())return;gRedo.push_back(gUCur);Snap p=std::move(gUndo.back());gUndo.pop_back();undoApply(p);toast("Desfeito");}
void doRedo(){undoCommit();if(gRedo.empty())return;gUndo.push_back(gUCur);Snap n=std::move(gRedo.back());gRedo.pop_back();undoApply(n);toast("Refeito");}
void clearScene(){for(size_t i=6;i<meshes.size();i++){glDeleteBuffers(1,&meshes[i].vbo);if(meshes[i].svbo)glDeleteBuffers(1,&meshes[i].svbo);glDeleteVertexArrays(1,&meshes[i].vao);}
 meshes.resize(6);objs.clear();rigs.clear();skins.clear();camViewOff();gCamAct=-1;sel=-1;tm=0;playing=false;parentMode=false;gImportOpen=false;gSigS=-1;mcReset();}
bool loadMAD(const std::string&path,bool replace){
 FILE*f=fopen(path.c_str(),"rb");if(!f){toast("Nao consegui abrir o arquivo");return false;}
 fseek(f,0,SEEK_END);long sz=ftell(f);fseek(f,0,SEEK_SET);if(sz<16){fclose(f);toast("Arquivo invalido");return false;}
 std::vector<unsigned char> buf(sz);size_t got=fread(buf.data(),1,sz,f);fclose(f);if((long)got!=sz){toast("Falha ao ler");return false;}
 Rd r{buf.data(),(size_t)sz};char mg[4];r.b(mg,4);uint32_t ver=r.u();
 if(memcmp(mg,"MAD2",4)!=0||ver!=2){toast("Formato .mad antigo ou invalido (so MAD2)");return false;}
 r.f();r.f();
 struct TM{std::vector<float> cpu,skin;float rad=1;};std::vector<TM> tms;uint32_t nm=r.u();if(nm>4096)r.ok=false;
 for(uint32_t i=0;i<nm&&r.ok;i++){TM t;uint32_t nc=r.u();if((size_t)nc*4>r.n-r.i){r.ok=false;break;}t.cpu.resize(nc);r.b(t.cpu.data(),(size_t)nc*4);t.rad=r.f();
  uint32_t ns=r.u();if((size_t)ns*4>r.n-r.i){r.ok=false;break;}t.skin.resize(ns);r.b(t.skin.data(),(size_t)ns*4);tms.push_back(std::move(t));}
 std::vector<Skin> sks;uint32_t nsk=r.u();if(nsk>4096)r.ok=false;
 for(uint32_t i=0;i<nsk&&r.ok;i++){Skin sk;uint32_t nj=r.u();if(nj>4096||(size_t)nj*136>r.n-r.i){r.ok=false;break;}
  for(uint32_t k=0;k<nj;k++){sk.j.push_back(r.s());M a,b;r.b(a.m,64);r.b(b.m,64);sk.ibm.push_back(a);sk.fz.push_back(b);}sks.push_back(sk);}
 std::vector<Obj> os;uint32_t no=r.u();if(no>100000)r.ok=false;
 for(uint32_t i=0;i<no&&r.ok;i++){Obj o;o.mesh=r.s();o.parent=r.s();o.skin=r.s();r.b(o.nm,24);o.nm[23]=0;o.len=r.f();o.met=r.f();o.rou=r.f();o.col.x=r.f();o.col.y=r.f();o.col.z=r.f();
  float c[9];r.b(c,36);memcpy(&o.cur.p.x,c,36);uint32_t nk=r.u();if(nk>100000||(size_t)nk*40>r.n-r.i){r.ok=false;break;}
  for(uint32_t k=0;k<nk;k++){Key kk;kk.t=r.f();float q[9];r.b(q,36);memcpy(&kk.x.p.x,q,36);o.k.push_back(kk);}os.push_back(o);}
 float cam[7];r.b(cam,28);
 std::vector<int> kinds,shapes;std::vector<float> fovs;std::vector<Rig> rgs;
 {Rd q=r;if(q.ok&&q.n-q.i>=8){char m2[4];q.b(m2,4);bool v3=memcmp(m2,"RIG3",4)==0,v2=v3||memcmp(m2,"RIG2",4)==0;if(v2||memcmp(m2,"RIG1",4)==0){uint32_t nk=q.u();if(nk==no&&nk<=100000){for(uint32_t i=0;i<nk&&q.ok;i++)kinds.push_back(q.s());if(v2)for(uint32_t i=0;i<nk&&q.ok;i++)shapes.push_back(q.s());if(v3)for(uint32_t i=0;i<nk&&q.ok;i++)fovs.push_back(q.f());
   uint32_t nr=q.u();if(nr>4096)q.ok=false;
   for(uint32_t i=0;i<nr&&q.ok;i++){Rig g;g.a=q.s();g.b=q.s();g.c=q.s();g.tgt=q.s();g.pole=q.s();g.w=q.f();g.rotc=q.u()!=0;uint32_t nw=q.u();if(nw>100000||(size_t)nw*8>q.n-q.i){q.ok=false;break;}
    for(uint32_t k=0;k<nw;k++){float t_=q.f(),v_=q.f();g.wk.push_back({t_,v_});}rgs.push_back(g);}
   if(!q.ok){kinds.clear();shapes.clear();fovs.clear();rgs.clear();}}}}}
 for(size_t i=0;i<os.size()&&i<kinds.size();i++)os[i].kind=kinds[i];
 for(size_t i=0;i<os.size()&&i<shapes.size();i++)os[i].shape=std::clamp(shapes[i],0,5);
 for(size_t i=0;i<os.size()&&i<fovs.size();i++)os[i].fov=std::clamp(fovs[i],.1f,3.f);
 if(!r.ok){toast("Arquivo .mad corrompido");return false;}
 if(replace)clearScene();
 int mb=(int)meshes.size(),sb2=(int)skins.size(),ob=(int)objs.size();
 for(auto&t:tms){Mesh mm=upload(t.cpu,GL_TRIANGLES);mm.rad=t.rad;if(!t.skin.empty())attachSkin(mm,t.skin);meshes.push_back(mm);}
 for(auto&sk:sks){for(auto&jj:sk.j)if(jj>=0)jj+=ob;skins.push_back(sk);}
 for(auto&o:os){if(o.mesh>=6)o.mesh=mb+(o.mesh-6);if(o.mesh<0||o.mesh>=(int)meshes.size())o.mesh=0;if(o.parent>=0)o.parent+=ob;
  if(o.skin>=0){o.skin+=sb2;if(o.skin>=(int)skins.size())o.skin=-1;}objs.push_back(o);}
 for(Rig g:rgs){int*qq[5]={&g.a,&g.b,&g.c,&g.tgt,&g.pole};for(int*pp:qq)if(*pp>=0)*pp+=ob;rigs.push_back(g);}
 if(replace){yaw=cam[0];pitch=cam[1];cd=cam[2];tgt=V{cam[3],cam[4],cam[5]};tm=cam[6];}
 sel=os.empty()?-1:ob;if(replace)undoClear();toast(fm("Carregado: %d objetos",(int)os.size()));return true;}
double gNewT=-10;
static const char*kUntitled="Projeto sem título";
static bool projExists(const std::string&n){std::string a=fm("/storage/emulated/0/Download/%s.mad",n.c_str()),b=fm("%s/%s.mad",gDir,n.c_str());return access(a.c_str(),F_OK)==0||access(b.c_str(),F_OK)==0;}
std::string uniqueProjName(const std::string&base){if(!projExists(base))return base;for(int k=1;k<10000;k++){std::string n=fm("%s %d",base.c_str(),k);if(!projExists(n))return n;}return base;}
int jniShowKb(){ // InputMethodManager.showSoftInput(decorView, flags) via JNI; devolve 1 se o Android aceitou
 if(!gApp||!gApp->activity||!gApp->activity->vm||!gApp->activity->clazz)return -1;
 ShimVM*vm=gApp->activity->vm;JNIEnv*env=nullptr;bool att=false;
 if(vm->GetEnv((void**)&env,JNI_VERSION_1_6)!=JNI_OK){if(vm->AttachCurrentThread(&env,nullptr)!=JNI_OK)return -2;att=true;}
 auto bad=[&](){if(env->ExceptionCheck()){env->ExceptionClear();return true;}return false;};
 int res=-3;jobject act=gApp->activity->clazz;jclass ac=env->GetObjectClass(act);
 jmethodID gss=ac?env->GetMethodID(ac,"getSystemService","(Ljava/lang/String;)Ljava/lang/Object;"):nullptr;bad();
 jmethodID gw=ac?env->GetMethodID(ac,"getWindow","()Landroid/view/Window;"):nullptr;bad();
 if(gss&&gw){
  jstring sn=env->NewStringUTF("input_method");jobject imm=env->CallObjectMethod(act,gss,sn);env->DeleteLocalRef(sn);bad();
  jobject win=env->CallObjectMethod(act,gw);bad();
  if(imm&&win){jclass wc=env->GetObjectClass(win);jmethodID gd=env->GetMethodID(wc,"getDecorView","()Landroid/view/View;");bad();
   jobject dv=gd?env->CallObjectMethod(win,gd):nullptr;bad();
   if(dv){jclass ic=env->GetObjectClass(imm);jmethodID ss=env->GetMethodID(ic,"showSoftInput","(Landroid/view/View;I)Z");bad();
    if(ss){res=0;for(int fl:{0,1,2}){jboolean ok=env->CallBooleanMethod(imm,ss,dv,fl);if(bad()){res=-4;break;}if(ok){res=1;break;}}}
    env->DeleteLocalRef(ic);env->DeleteLocalRef(dv);}
   env->DeleteLocalRef(wc);}
  if(imm)env->DeleteLocalRef(imm);if(win)env->DeleteLocalRef(win);}
 if(ac)env->DeleteLocalRef(ac);bad();
 if(att)vm->DetachCurrentThread();
 return res;}
void showKb(){::plat_show_keyboard();}
void hideKb(){::plat_hide_keyboard();}
void newProject(){gNameMode=0;gNameBuf.clear();gNameOpen=true;gImportOpen=false;showKb();}
void nameConfirm(){std::string n=gNameBuf;while(!n.empty()&&n.front()==' ')n.erase(n.begin());while(!n.empty()&&n.back()==' ')n.pop_back();
 if(n.empty())n=kUntitled;
 if(gNameMode==0){gNewT=-10;clearScene();gProj=uniqueProjName(n);toast("Novo projeto: "+gProj);}
 else if(n!=gProj){gProj=uniqueProjName(n);toast("Projeto renomeado: "+gProj);}
 gNameOpen=false;hideKb();}
void nameBack(){while(!gNameBuf.empty()&&(((unsigned char)gNameBuf.back())&0xC0)==0x80)gNameBuf.pop_back();if(!gNameBuf.empty())gNameBuf.pop_back();}
void nameAddCp(int cp){ // aceita letras, numeros e acentos; recusa caracteres proibidos em nomes de arquivo
 if(cp<32||cp==127||cp>0x7FF)return;if(cp=='/'||cp=='\\'||cp==':'||cp=='*'||cp=='?'||cp=='"'||cp=='<'||cp=='>'||cp=='|')return;
 if(gNameBuf.size()>=44)return;
 if(cp<0x80)gNameBuf.push_back((char)cp);else{gNameBuf.push_back((char)(0xC0|(cp>>6)));gNameBuf.push_back((char)(0x80|(cp&0x3F)));}}
void sizeOpen(){gSizeOpen=true;gSizeField=0;gSizeBuf[0]=gMcCW>0?std::to_string(gMcCW):"";gSizeBuf[1]=gMcCH>0?std::to_string(gMcCH):"";showKb();}
void sizeApply(){int w=atoi(gSizeBuf[0].c_str()),h=atoi(gSizeBuf[1].c_str());
 if(w<=0&&h<=0){gMcCW=gMcCH=0;toast("Tamanho da render: automatico (tela)");}
 else{if(w<=0)w=(int)(h*(float)W/std::max(H,1));if(h<=0)h=(int)(w*(float)H/std::max(W,1));
  w=std::clamp(w,64,4096);h=std::clamp(h,64,4096);while((long)w*h>8500000L){w=w*9/10;h=h*9/10;}
  gMcCW=w;gMcCH=h;toast(fm("Tamanho da render: %dx%d",w,h));}
 gMcHaveCam=false;gSigS=-1;mcReset();gSizeOpen=false;hideKb();}
void sizeAddDigit(int cp){if(cp<'0'||cp>'9')return;std::string&b=gSizeBuf[gSizeField];if(b.size()<4)b.push_back((char)cp);}
void sizeBack(){std::string&b=gSizeBuf[gSizeField];if(!b.empty())b.pop_back();}
void kbKey(int c){
 if(gSizeOpen){if(c==10||c==11){gSizeField=c-10;showKb();}else if(c==12)sizeApply();else if(c==13){gSizeOpen=false;hideKb();}
  else if(c>=20&&c<=26){static const int PW[7]={0,1920,1280,2560,3840,1080,1080},PH[7]={0,1080,720,1440,2160,1080,1920};int k=c-20;gSizeBuf[0]=PW[k]?std::to_string(PW[k]):"";gSizeBuf[1]=PH[k]?std::to_string(PH[k]):"";}
  return;}
 if(c==3)nameConfirm();else if(c==4){gNameOpen=false;hideKb();}else if(c==5)showKb();}
int jniUnicode(int keyCode,int meta){ // caractere digitado: KeyEvent.getUnicodeChar() via JNI
 if(!gApp||!gApp->activity||!gApp->activity->vm)return 0;
 ShimVM*vm=gApp->activity->vm;JNIEnv*env=nullptr;bool att=false;
 if(vm->GetEnv((void**)&env,JNI_VERSION_1_6)!=JNI_OK){if(vm->AttachCurrentThread(&env,nullptr)!=JNI_OK)return 0;att=true;}
 int uc=0;jclass kc=env->FindClass("android/view/KeyEvent");
 if(kc){jmethodID ctor=env->GetMethodID(kc,"<init>","(II)V"),gu=env->GetMethodID(kc,"getUnicodeChar","(I)I");
  if(ctor&&gu){jobject ke=env->NewObject(kc,ctor,0,keyCode);if(ke){uc=env->CallIntMethod(ke,gu,meta);env->DeleteLocalRef(ke);}}
  env->DeleteLocalRef(kc);}
 if(env->ExceptionCheck())env->ExceptionClear();
 if(att)vm->DetachCurrentThread();
 return uc;}
void pickPath(const std::string&p){
 if(gPick==0){importPath(p);return;}
 if(gPick==1){if(loadMAD(p,true)){size_t sl=p.rfind('/');std::string n=sl==std::string::npos?p:p.substr(sl+1);size_t dt=n.rfind('.');if(dt!=std::string::npos)n=n.substr(0,dt);gProj=n;}}
 else loadMAD(p,false);}
bool setJoints(int si){if(si<0||si>=(int)skins.size())return false;Skin&sk=skins[si];int n=std::min((int)sk.j.size(),64);static M J[64];
 for(int k=0;k<n;k++){M jw=sk.j[k]>=0?worldM(sk.j[k]):(k<(int)sk.fz.size()?sk.fz[k]:id());J[k]=mul(jw,sk.ibm[k]);}
 glUniformMatrix4fv(uJL,n,GL_FALSE,J[0].m);return true;}
// ---------- controles do rig: formas personalizadas (custom shapes) ----------
const char*gShapeN[6]={"Cubo","Circulo","Esfera","Diamante","Cruz","Quadrado"};
static void ctrlSeg(V a,V b,float tk,float r,float g,float bl,float al,float ext=-1.f){V d=b-a;float sl=vlen(d);if(sl<1e-7f)return;float ll=ext<0?sl*1.15f:sl+ext;V e1=d*(1/sl),r0=fabsf(e1.y)<.9f?V{0,1,0}:V{1,0,0},e2=norm(cross(e1,r0)),e3=cross(e1,e2),mid=(a+b)*.5f;
 M m={};float*q=m.m;q[0]=e1.x*ll;q[1]=e1.y*ll;q[2]=e1.z*ll;q[4]=e2.x*tk;q[5]=e2.y*tk;q[6]=e2.z*tk;q[8]=e3.x*tk;q[9]=e3.y*tk;q[10]=e3.z*tk;q[12]=mid.x;q[13]=mid.y;q[14]=mid.z;q[15]=1;
 gMR[0]=0;gMR[1]=.5f;gSk=0;draw(meshes[0],mul(gVP,m),m,r,g,bl,al,0);}
static void ctrlQuad(V c,V rv,V uv,V nv,float w,float h,float tk,float r,float g,float bl){ // retangulo preenchido (caixa fina)
 M m={};float*q=m.m;q[0]=rv.x*w;q[1]=rv.y*w;q[2]=rv.z*w;q[4]=uv.x*h;q[5]=uv.y*h;q[6]=uv.z*h;q[8]=nv.x*tk;q[9]=nv.y*tk;q[10]=nv.z*tk;q[12]=c.x;q[13]=c.y;q[14]=c.z;q[15]=1;
 gMR[0]=0;gMR[1]=.5f;gSk=0;draw(meshes[0],mul(gVP,m),m,r,g,bl,1,0);}
static void ctrlShape(int shape,V c,V ex,V ey,V ez,float R,float tk,float cr,float cg,float cb){
 auto P=[&](float x,float y,float z){return c+ex*(x*R)+ey*(y*R)+ez*(z*R);};
 auto L=[&](V a,V b){ctrlSeg(a,b,tk,cr,cg,cb,1);};
 auto ring=[&](int ax){const int NS=32;V prev;for(int k=0;k<=NS;k++){float an=k*2*PI/NS,ca=cosf(an),sa=sinf(an);V q=ax==0?P(0,ca,sa):ax==1?P(ca,0,sa):P(ca,sa,0);if(k)L(prev,q);prev=q;}};
 if(shape==0){for(int i=0;i<8;i++)for(int b=0;b<3;b++){int j=i^(1<<b);if(j>i)L(P(i&1?1:-1,i&2?1:-1,i&4?1:-1),P(j&1?1:-1,j&2?1:-1,j&4?1:-1));}}
 else if(shape==1)ring(1);
 else if(shape==2){ring(0);ring(1);ring(2);}
 else if(shape==3){V p6[6]={P(1,0,0),P(-1,0,0),P(0,1,0),P(0,-1,0),P(0,0,1),P(0,0,-1)};for(int i=0;i<6;i++)for(int j=i+1;j<6;j++)if((i^j)!=1)L(p6[i],p6[j]);}
 else if(shape==4){L(P(-1,0,0),P(1,0,0));L(P(0,-1,0),P(0,1,0));L(P(0,0,-1),P(0,0,1));}
 else{L(P(-1,0,-1),P(1,0,-1));L(P(1,0,-1),P(1,0,1));L(P(1,0,1),P(-1,0,1));L(P(-1,0,1),P(-1,0,-1));}}
static void drawLightObj(int i){ // luz como objeto 3D: esfera na cor da luz + marcas conforme o tipo
 Obj&o=objs[i];M w=worldM(i);V c{w.m[12],w.m[13],w.m[14]};
 V ex{w.m[0],w.m[1],w.m[2]},ey{w.m[4],w.m[5],w.m[6]},ez{w.m[8],w.m[9],w.m[10]};
 if(vlen(ex)<1e-8f||vlen(ey)<1e-8f||vlen(ez)<1e-8f)return;ex=norm(ex);ey=norm(ey);ez=norm(ez);
 V e=camEye();float th=tanf(FOV/2);float R=std::max({o.cur.s.x,o.cur.s.y,o.cur.s.z})*.35f;
 float dc=std::max(vlen(c-e),1e-3f),pxr=R/(dc*th)*H*.5f;if(pxr>60)R*=60/pxr;else if(pxr<13)R*=13/pxr;
 bool s=(i==sel);float cr=s?1.f:o.col.x,cg=s?.85f:o.col.y,cb=s?.2f:o.col.z;int ty=std::clamp(o.shape,0,2);
 gMR[0]=0;gMR[1]=.5f;gSk=0;
 {T t;t.p=c;t.s={R*.9f,R*.9f,R*.9f};M m=model(t);draw(meshes[1],mul(gVP,m),m,o.col.x,o.col.y,o.col.z,1,0);}
 float tk=R*.07f;
 auto P=[&](float x,float y,float z){return c+ex*(x*R)+ey*(y*R)+ez*(z*R);};
 auto L=[&](V a,V b){ctrlSeg(a,b,tk,cr,cg,cb,1);};
 auto ring=[&](int ax,float rr,float yy){const int NS=24;V prev;for(int k=0;k<=NS;k++){float an=k*2*PI/NS,ca=cosf(an)*rr,sa=sinf(an)*rr;V q=ax==0?P(0,ca,sa):ax==1?P(ca,yy,sa):P(ca,sa,0);if(k)L(prev,q);prev=q;}};
 if(ty==0){ // sol: raios em volta e seta de direcao
  for(int k=0;k<8;k++){float an=k*PI/4;L(P(cosf(an)*1.1f,0,sinf(an)*1.1f),P(cosf(an)*1.9f,0,sinf(an)*1.9f));}
  ring(1,1.1f,0);L(P(0,-1.1f,0),P(0,-4.f,0));L(P(0,-4.f,0),P(.45f,-3.4f,0));L(P(0,-4.f,0),P(-.45f,-3.4f,0));}
 else if(ty==1){ring(0,1.5f,0);ring(1,1.5f,0);ring(2,1.5f,0);}
 else{float len=4.f,rr=std::min(tanf(std::clamp(o.fov,.1f,1.4f))*len,3.6f);ring(1,rr,-len);L(P(0,0,0),P(0,-len,0));
  for(int k=0;k<4;k++){float an=k*PI/2;L(P(0,0,0),P(cosf(an)*rr,-len,sinf(an)*rr));}}}
static void drawCamObj(int i,bool selc){V e,f,r,u;if(!camBasis(i,e,f,r,u))return;float d=std::max({objs[i].cur.s.x,objs[i].cur.s.y,objs[i].cur.s.z})*2.f;
 float ra=gMcCW>0?(float)gMcCW/gMcCH:(float)W/std::max(H,1);float hh=d*tanf(objs[i].fov/2),hw=hh*ra;
 V c=e+f*d,p0=c+r*hw+u*hh,p1=c-r*hw+u*hh,p2=c-r*hw-u*hh,p3=c+r*hw-u*hh;
 float cr=.03f,cg=.03f,cb=.03f;if(selc){cr=1;cg=.85f;cb=.2f;}float tk=d*.012f; // preto; amarelo quando selecionada
 ctrlSeg(e,p0,tk,cr,cg,cb,1,tk);ctrlSeg(e,p1,tk,cr,cg,cb,1,tk);ctrlSeg(e,p2,tk,cr,cg,cb,1,tk);ctrlSeg(e,p3,tk,cr,cg,cb,1,tk);
 ctrlSeg(p0,p1,tk,cr,cg,cb,1,tk);ctrlSeg(p1,p2,tk,cr,cg,cb,1,tk);ctrlSeg(p2,p3,tk,cr,cg,cb,1,tk);ctrlSeg(p3,p0,tk,cr,cg,cb,1,tk);
 ctrlQuad(c+u*(hh*1.65f),r,u,f,hw*2.f,hh*.70f,tk*.8f,cr,cg,cb);} // barra de "cima" preenchida, separada da moldura
void drawCtrls(){bool any=false;for(auto&o:objs)if(o.kind){any=true;break;}if(!any)return;
 V e=camEye();float th=tanf(FOV/2);
 for(int i=0;i<(int)objs.size();i++){Obj&o=objs[i];if(!o.kind||o.hide)continue;if(o.kind==4){drawLightObj(i);continue;}if(o.kind==3){if(i!=gCamView)drawCamObj(i,i==sel);continue;}M w=worldM(i);V c{w.m[12],w.m[13],w.m[14]};
  V ex=V{w.m[0],w.m[1],w.m[2]},ey=V{w.m[4],w.m[5],w.m[6]},ez=V{w.m[8],w.m[9],w.m[10]};
  if(vlen(ex)<1e-8f||vlen(ey)<1e-8f||vlen(ez)<1e-8f)continue;ex=norm(ex);ey=norm(ey);ez=norm(ez);
  float R=std::max({o.cur.s.x,o.cur.s.y,o.cur.s.z})*.5f;
  float dc=std::max(vlen(c-e),1e-3f),pxr=R/(dc*th)*H*.5f; // tamanho na tela: entre 16 e 90 px
  if(pxr>90)R*=90/pxr;else if(pxr<16)R*=16/pxr;
  bool s=(i==sel);float cr=o.kind==1?.25f:.3f,cg=o.kind==1?.6f:.9f,cb=o.kind==1?1.f:.4f;if(s){cr=1;cg=.85f;cb=.2f;}
  ctrlShape(o.shape,c,ex,ey,ez,R,R*.05f,cr,cg,cb);
  if(o.kind==2){int ri=rigOf(i);if(ri>=0&&rigs[ri].b>=0&&rigs[ri].b<(int)objs.size())ctrlSeg(c,wp(rigs[ri].b),R*.03f,cr,cg,cb,.8f);}}}
void drawBones(){ // estilo Maya: losango fino (aresta + corpo translucido) e junta em aneis; preto normal, verde selecionado
 std::vector<char> hc(objs.size(),0);for(auto&q:objs)if(q.mesh==5&&q.parent>=0&&q.parent<(int)objs.size())hc[q.parent]=1;
 for(int i=0;i<(int)objs.size();i++){Obj&o=objs[i];if(o.mesh!=5||o.hide)continue;
  M w=worldM(i);bool sl=(i==sel);
  float cr=sl?.45f:.03f,cg=sl?.95f:.03f,cb=sl?.35f:.03f;
  gMR[0]=0;gMR[1]=.5f;gSk=0;
  float wd=o.len*.35f;M mod=w;for(int k=0;k<4;k++){mod.m[k]*=wd;mod.m[4+k]*=o.len;mod.m[8+k]*=wd;}
  M mvp=mul(gVP,mod);
  draw(meshes[5],mvp,mod,cr,cg,cb,sl?.24f:.28f,0);
  draw(gBoneWire,mvp,mod,cr,cg,cb,1,0);
  float rj=o.len*(sl?.115f:.10f);
  M jm=w;for(int k=0;k<4;k++){jm.m[k]*=rj;jm.m[4+k]*=rj;jm.m[8+k]*=rj;}
  draw(gJointRing,mul(gVP,jm),jm,cr,cg,cb,1,0);
  if(!hc[i]){M jt=jm;jt.m[12]=w.m[12]+w.m[4]*o.len;jt.m[13]=w.m[13]+w.m[5]*o.len;jt.m[14]=w.m[14]+w.m[6]*o.len;
   draw(gJointRing,mul(gVP,jt),jt,cr,cg,cb,1,0);}}}
// ---------- timeline estilo Maya/Houdini ----------
Rc gFldR[5]={};float gTrX0=0,gTrW=1,gBarY=0,gBarH=0,gDX0=0;int gDV0=0,gDV1=0;
void tri(float cx,float cy,float s,int dir){const float c=.93f;for(int i=0;i<10;i++){float h=2*s*(1-i/10.f);float x=dir>0?cx-s*.7f+i*s*.14f:cx+s*.7f-(i+1)*s*.14f;rect(x,cy-h/2,s*.15f,h,c,c,c);}}
void icon3(int id,float cx,float cy,float s,bool on){const float c=.93f;
 if(id==50){rect(cx-s*1.05f,cy-s*.8f,s*.3f,s*1.6f,c,c,c);tri(cx-s*.05f,cy,s*.7f,-1);tri(cx+s*.65f,cy,s*.7f,-1);}
 else if(id==54){rect(cx+s*.75f,cy-s*.8f,s*.3f,s*1.6f,c,c,c);tri(cx-s*.65f,cy,s*.7f,1);tri(cx+s*.05f,cy,s*.7f,1);}
 else if(id==51)tri(cx,cy,s,-1);else if(id==53)tri(cx,cy,s,1);
 else if(id==52)rect(cx-s*.75f,cy-s*.75f,s*1.5f,s*1.5f,c,c,c,1,s*.15f);
 else if(id==55){rect(cx-s*.9f,cy-s*.8f,s*.3f,s*1.6f,c,c,c);tri(cx+s*.2f,cy,s*.8f,-1);}
 else if(id==56){rect(cx+s*.6f,cy-s*.8f,s*.3f,s*1.6f,c,c,c);tri(cx-s*.2f,cy,s*.8f,1);}
 else if(id==57){for(int i=0;i<10;i++){float w=2*s*(1-i/10.f);rect(cx-w/2,cy-s*.5f+i*s*.1f,w,s*.11f,c,c,c);}}
 else if(id==7){rect(cx-s*.95f,cy-s*.55f,s*1.1f,s*1.1f,c,c,c,1,s*.55f);rect(cx-s*.6f,cy-s*.2f,s*.4f,s*.4f,.2f,.2f,.22f,1,s*.2f);
  rect(cx-s*.1f,cy-s*.12f,s*1.2f,s*.24f,c,c,c);rect(cx+s*.65f,cy,s*.22f,s*.55f,c,c,c);rect(cx+s*.35f,cy,s*.22f,s*.4f,c,c,c);}}
void jumpKey(int dir){if(sel<0||objs[sel].k.empty()){toast("Sem keyframes no objeto selecionado");return;}
 float best=0;bool f=false;for(auto&k:objs[sel].k){if(dir>0&&k.t>tm+.001f&&(!f||k.t<best)){best=k.t;f=true;}if(dir<0&&k.t<tm-.001f&&(!f||k.t>best)){best=k.t;f=true;}}
 if(!f){toast(dir>0?"Nao ha proximo keyframe":"Nao ha keyframe anterior");return;}tm=best;applyAnim();}
void delKey(){if(sel<0)return;auto&k=objs[sel].k;for(size_t i=0;i<k.size();i++)if(fabsf(k[i].t-tm)<.02f){k.erase(k.begin()+i);toast("Keyframe removido");return;}toast("Sem keyframe neste frame");}
bool tlHit(float x,float y,int&mode){
 for(int k=0;k<5;k++)if(inR(gFldR[k],x,y)){mode=k==4?27:23+k;gDX0=x;gDV0=k==0?gFS:k==1?gVS:k==2?gVE:k==3?gFE:(int)roundf(tm*24);return true;}
 float u=U();
 if(y>=gBarY-.6f*u&&y<=gBarY+gBarH+.6f*u&&x>=gTrX0-2*u&&x<=gTrX0+gTrW+2*u){
  float rng=(float)std::max(gFE-gFS,1);float b0=gTrX0+(gVS-gFS)/rng*gTrW,b1=gTrX0+(gVE-gFS)/rng*gTrW;gDX0=x;gDV0=gVS;gDV1=gVE;
  if(fabsf(x-b0)<2.2f*u)mode=20;else if(fabsf(x-b1)<2.2f*u)mode=21;
  else{if(x<b0||x>b1){int c=(int)roundf(gFS+(x-gTrX0)/gTrW*rng),w=gVE-gVS;int ns=std::clamp(c-w/2,gFS,std::max(gFE-w,gFS));gVS=ns;gVE=ns+w;gDV0=gVS;gDV1=gVE;}mode=22;}
  return true;}
 return false;}
void tlDrag(int mode,float x){float rng=(float)std::max(gFE-gFS,1);float u=U();int df=(int)roundf((x-gDX0)/(.55f*u));
 if(mode==20)gVS=std::clamp((int)roundf(gFS+(x-gTrX0)/gTrW*rng),gFS,gVE-5);
 else if(mode==21)gVE=std::clamp((int)roundf(gFS+(x-gTrX0)/gTrW*rng),gVS+5,gFE);
 else if(mode==22){int w=gDV1-gDV0;int sh=(int)roundf((x-gDX0)/gTrW*rng);int ns=std::clamp(gDV0+sh,gFS,std::max(gFE-w,gFS));gVS=ns;gVE=ns+w;}
 else if(mode==23){gFS=std::clamp(gDV0+df,0,gFE-10);syncRange();}
 else if(mode==24)gVS=std::clamp(gDV0+df,gFS,gVE-5);
 else if(mode==25)gVE=std::clamp(gDV0+df,gVS+5,gFE);
 else if(mode==26){gFE=std::clamp(gDV0+df,gFS+10,2400);syncRange();}
 else if(mode==27){int f=std::clamp(gDV0+df,gFS,gFE);tm=f/24.f;applyAnim();}
 if(mode>=23&&mode<=26){float a=gFS/24.f,b=gFE/24.f;if(tm<a)tm=a;if(tm>b)tm=b;applyAnim();}}
void frameTap(float x){float rel=(x-gFldR[4].x)/std::max(gFldR[4].w,1.f);int f=(int)roundf(tm*24);if(rel<.33f)f--;else if(rel>.66f)f++;tm=std::clamp(f,gFS,gFE)/24.f;applyAnim();}
// Fundo em gradiente vertical (cores do viewport do Maya): claro azulado em cima, escuro embaixo
GLuint gBgP=0;GLint gBgH=-1;
void drawBg(){
 if(!gBgP){
  const char*vs="#version 300 es\nvoid main(){vec2 p=vec2(float((gl_VertexID<<1)&2),float(gl_VertexID&2));gl_Position=vec4(p*2.0-1.0,0.0,1.0);}";
  const char*fs="#version 300 es\nprecision highp float;uniform float uH;out vec4 o;\nvoid main(){float t=clamp(gl_FragCoord.y/uH,0.0,1.0);vec3 top=vec3(.49,.56,.64),bot=vec3(.065,.07,.08);o=vec4(mix(bot,top,t),1.0);}";
  gBgP=mkProg(vs,fs);gBgH=glGetUniformLocation(gBgP,"uH");}
 glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_BLEND);
 glBindVertexArray(0);glUseProgram(gBgP);glUniform1f(gBgH,(float)H);
 glDrawArrays(GL_TRIANGLES,0,3);
 glDepthMask(GL_TRUE);glEnable(GL_BLEND);glUseProgram(gProg);}
void frame(){solveRigs();sculptSync();if(gCamView>=0)viewFromCam();
 bool post=gSSAO&&gPostOK;glBindFramebuffer(GL_FRAMEBUFFER,post?(gMsOK?gMsFbo:gFbo):0);
 glViewport(0,0,W,H);glClearColor(.24f,.24f,.26f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);drawBg();
 glEnable(GL_DEPTH_TEST);
 gVP=mul(persp(FOV,(float)W/H,nearP(),100),lookAt(camEye(),tgt,{0,1,0}));{V e_=camEye();glUniform3f(uCamL,e_.x,e_.y,e_.z);V f_=norm(V{tgt.x-e_.x,tgt.y-e_.y,tgt.z-e_.z});V r_=norm(cross(f_,V{0,1,0}));V u_=cross(r_,f_);glUniform1f(uMcL,gMatcap?1.f:0.f);glUniform3f(uMcRL,r_.x,r_.y,r_.z);glUniform3f(uMcUL,u_.x,u_.y,u_.z);glUniform3f(uMcBL,-f_.x,-f_.y,-f_.z);}
 if(!gCapNow)draw(meshes[4],gVP,id(),.88f,.91f,.96f,.2f,0);
 auto ln=[&](V c,V sc,float r,float g,float b){T t;t.p=c;t.s=sc;M m=model(t);draw(meshes[0],mul(gVP,m),m,r,g,b,1,0);};
 ln({0,0,0},{20,.014f,.014f},.85f,.28f,.3f);ln({0,0,0},{.014f,.014f,20},.3f,.5f,.9f);
 glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D_ARRAY,gTexArr);glActiveTexture(GL_TEXTURE0);
 for(int i=0;i<(int)objs.size();i++){Obj&o=objs[i];if(o.mesh==5||o.kind||o.hide)continue;M mod=worldM(i);bool skd=o.skin>=0&&meshes[o.mesh].skinned&&setJoints(o.skin);if(skd)mod=id();gSk=skd?1.f:0.f;float h=(i==sel)?.35f:0;gMR[0]=o.met;gMR[1]=o.rou;gTL[0]=(float)o.tb;gTL[1]=(float)o.tm;gTL[2]=(float)o.tn;gTL[3]=o.ac;
  draw(meshes[o.mesh],mul(gVP,mod),mod,o.col.x+(1-o.col.x)*h,o.col.y+(1-o.col.y)*h,o.col.z+(1-o.col.z)*h,1,1);}
 gMR[0]=0;gMR[1]=.5f;gSk=0;gTL[0]=gTL[1]=gTL[2]=-1;gTL[3]=0;glActiveTexture(GL_TEXTURE0);if(post)runPost();mcFrame();vidStep();
 glDisable(GL_DEPTH_TEST);
 if(gBones&&!gCapNow)drawBones();
 if(!gMcFinal&&!gCapNow)drawCtrls();
 }
void frameUI(){
 gBtn.clear();gBlock.clear();float u=U(),ML_=ML(),hH=8.6f*u;
 if(gCamView>=0&&gCamView<(int)objs.size()){float ra=gMcCW>0?(float)gMcCW/gMcCH:(float)W/H,sa=(float)W/H;
  if(ra<sa-.002f){float fw=H*ra,fx=(W-fw)/2;rect(0,0,fx,(float)H,0,0,0,.5f);rect(fx+fw,0,W-fx-fw,(float)H,0,0,0,.5f);rect(fx-.12f*u,0,.12f*u,(float)H,.9f,.9f,.9f,.6f);rect(fx+fw,0,.12f*u,(float)H,.9f,.9f,.9f,.6f);}
  pill(W/2-13*u,hH+1.2f*u,26*u,5.6f*u,19,fm("Câmera: %.14s  (sair)",objs[gCamView].nm).c_str(),true,.2f,.38f,.55f);}
 if(gSculpt&&gSt.on){for(int k=0;k<40;k++){float an=k*2*PI/40;float qx=gSCx+cosf(an)*gSRad,qy=gSCy+sinf(an)*gSRad;rect(qx-.18f*u,qy-.18f*u,.36f*u,.36f*u,.95f,.95f,.95f,.8f,.18f*u);}}
 // topo: apenas os cantos (estilo app 3D movel)
            auto sld=[&](int i,float px_,float pw_,float yy,float rh_,float tp_,const char*lab,float v){float lw=9.5f*u,sx=px_+lw,sw=pw_-lw-1.4f*u;text(lab,px_+1.2f*u,yy+rh_/2-2.5f*tp_,tp_,.85f,.85f,.88f);
  rect(sx,yy+rh_/2-.45f*u,sw,.9f*u,.1f,.1f,.11f,1,.45f*u);rect(sx,yy+rh_/2-.45f*u,sw*v,.9f*u,.28f,.45f,.7f,1,.45f*u);
  rect(sx+sw*v-.7f*u,yy+rh_/2-1.1f*u,1.4f*u,2.2f*u,.95f,.95f,.95f,1,.7f*u);gBtn.push_back({{sx-.8f*u,yy,sw+1.6f*u,rh_},300+i});gSlX[i]=sx;gSlW[i]=sw;};
 float Rd=26*u,cx0=ML_+9.5f*u,cy0=5.5f*u,ra=17.5f*u,ib=8.6f*u;float tbX=ML_+(gUiQml?16.5f*u:0.f),tbW=0; // menu radial (estilo Prisma)
 if(!gUiQml){rect(cx0-Rd,cy0-Rd,2*Rd,2*Rd,.09f,.09f,.1f,.9f,Rd);gBlock.push_back({0,0,cx0+Rd*.72f,cy0+Rd*.72f});
 rect(cx0-9.5f*u,cy0-9.5f*u,19*u,19*u,.05f,.05f,.06f,.95f,9.5f*u);
 icon2(0,cx0,cy0,3.4f*u,gAddFan);gBtn.push_back({{cx0-9.5f*u,cy0-9.5f*u,19*u,19*u},44});
 for(int k=0;k<4;k++){float an=k*30.f*PI/180.f,tx_=cx0+cosf(an)*ra,ty_=cy0+sinf(an)*ra;bool on=tool==k;
  if(on)rect(tx_-ib/2,ty_-ib/2,ib,ib,.28f,.45f,.7f,.95f,ib/2);icon2(3+k,tx_,ty_,ib*.32f,on);gBtn.push_back({{tx_-ib/2,ty_-ib/2,ib,ib},3+k});}
 if(gAddFan){const char*nm_[5]={"Cubo","Esfera","Plano","Osso","Câmera"};const int ai[5]={0,1,2,10,18};float lx=ML_+1.5f*u,ly=cy0+Rd+1*u,lw=24*u,lh=6*u,lt=5*(lh+.5f*u)+1.5f*u;panel(lx-1*u,ly-1*u,lw+2*u,lt,1*u);
  for(int k=0;k<5;k++)pill(lx,ly+k*(lh+.5f*u),lw,lh,ai[k],nm_[k],false,.33f,.33f,.35f);gBtn.push_back({{lx-1*u,ly-1*u,lw+2*u,lt},299});}}
 {float ps=.52f*u;const char*tn[2]={"Animação","Escultura"};const int ti[2]={84,93};float w0=tw(tn[0],ps),w1=tw(tn[1],ps),gap=5*u,x=(W-(w0+w1+gap))/2;
  for(int k=0;k<2;k++){bool act=(k==1)==gSculpt;float ww=k?w1:w0;text(tn[k],x,hH/2-2.5f*ps,ps,act?1.f:.5f,act?1.f:.5f,act?1.f:.55f);
   if(act)rect(x,hH/2+3.6f*ps,ww,.28f*u,.95f,.62f,.2f);gBtn.push_back({{x-1.5f*u,0,ww+3*u,hH},ti[k]});x+=ww+gap;}}
 if(!gUiQml){float bs=8*u,by=.6f*u,x=W-ML_-bs;
  for(int k=0;k<3;k++)rect(x+bs*.22f,by+bs*(.28f+k*.22f),bs*.56f,bs*.075f,.93f,.93f,.93f,1,bs*.04f);gBtn.push_back({{x,by,bs,bs},60});x-=bs+1*u;
  for(int k=1;k>=0;k--){int id=91+k;icon2(id,x+bs/2,by+bs/2,bs*.32f,false);gBtn.push_back({{x,by,bs,bs},id});x-=bs+1*u;}}
 if(gFileMenu){ // menu do botao de tres linhas
  const char*rl[9]={"Novo projeto","Abrir projeto","Salvar projeto","Renomear projeto","Renderizar","Import glTF","Import MAD","Export glTF","Export MAD"};const int rid[9]={40,41,42,90,61,15,43,14,13};
  float mw=30*u,ph=5.8f*u,mx=W-ML_-mw,my=hH+(gUiQml?2.4f:.4f)*u,mh=9*(ph+.5f*u)+1.5f*u+3.4f*u;panel(mx,my,mw,mh,1*u);
  {char nb[64];snprintf(nb,64,"%.22s  ·  %d FPS",gProj.c_str(),(int)(gFps+.5f));text(nb,mx+1.4f*u,my+.9f*u,.38f*u,.65f,.65f,.7f);}
  for(int i=0;i<9;i++){bool on=gFlash[rid[i]]>0||(rid[i]==15&&gImportOpen&&gPick==0)||(rid[i]==43&&gImportOpen&&gPick==2)||(rid[i]==41&&gImportOpen&&gPick==1)||(rid[i]==61&&gRenderOpen);
   pill(mx+1*u,my+.75f*u+3.4f*u+i*(ph+.5f*u),mw-2*u,ph,rid[i],rl[i],on,(i==5||i==6)?.2f:.33f,(i==5||i==6)?.38f:.33f,(i==5||i==6)?.55f:.35f);}
  gBtn.push_back({{mx,my,mw,mh},299});}
 if(gRenderOpen){ // painel Render (viewport + path tracing), recolhivel
  float px=tbX+tbW+1*u,py=hH+10*u,pw2=27*u,rh=4.4f*u,tp2=.38f*u,bw_=pw2-2.4f*u,bh_=rh-.6f*u,y1=py+1.0f*u;int nr=gMcOK?14:4;
  panel(px,py,pw2,rh*nr+1.6f*u,1*u);
  sld(2,px,pw2,y1,rh,tp2,"AO",gAOs);
  pill(px+1.2f*u,y1+rh+.3f*u,bw_,bh_,16,(gSSAO&&gPostOK)?"SSAO: on":"SSAO: off",gSSAO,.3f,.3f,.33f);
  pill(px+1.2f*u,y1+2*rh+.3f*u,bw_,bh_,17,gBones?"Bones: on":"Bones: off",gBones,.3f,.3f,.33f);
  if(!gMcOK){text("MC indisponivel (precisa de ES 3.1)",px+1.2f*u,y1+3*rh+rh/2-2.5f*tp2,tp2*.85f,.85f,.55f,.5f);}
  else{bool done=gMcFinal&&gMcSamples>=gMcSpp;float y2=y1+3*rh;
   pill(px+1.2f*u,y2,bw_,bh_,30,gMcPrev?"Preview MC: on":"Preview MC: off",gMcPrev,.3f,.3f,.33f);
   pill(px+1.2f*u,y2+rh,bw_,bh_,31,gMcFinal?(done?"Fechar render":"Parar render"):"Render final",gMcFinal,.5f,.34f,.12f);
   pill(px+1.2f*u,y2+2*rh,bw_,bh_,33,fm("Amostras: %d",gMcSpp).c_str(),false,.3f,.3f,.33f);
   pill(px+1.2f*u,y2+3*rh,bw_,bh_,34,gMcGround?"Chao: on":"Chao: off",gMcGround,.3f,.3f,.33f);
   pill(px+1.2f*u,y2+4*rh,bw_,bh_,35,gMcDn?"Denoise: on":"Denoise: off",gMcDn,.3f,.3f,.33f);
   pill(px+1.2f*u,y2+5*rh,bw_,bh_,32,"Salvar PNG",false,.2f,.38f,.55f);
  pill(px+1.2f*u,y2+6*rh,bw_,bh_,37,fm("Video: %s",gVidQN[gVidQ]).c_str(),false,.3f,.3f,.33f);
  pill(px+1.2f*u,y2+7*rh,bw_,bh_,36,gVid.on?fm("Parar video %d/%d",gVid.done,gVid.total).c_str():fm("Render video %d-%d",std::min(gFS,gFE),std::max(gFS,gFE)).c_str(),gVid.on,.5f,.34f,.12f);
   pill(px+1.2f*u,y2+8*rh,bw_,bh_,38,gMcLeak?"Anti-fresta: on":"Anti-fresta: off",gMcLeak,.3f,.3f,.33f);
   pill(px+1.2f*u,y2+9*rh,bw_,bh_,39,gMcCW>0?fm("Tamanho: %dx%d",gMcCW,gMcCH).c_str():"Tamanho: tela",gMcCW>0,.3f,.3f,.33f);
   char sb[64];if(gVid.on)snprintf(sb,64,"Frame %d  %d/%d  %d/%d spp",gVid.f,gVid.done,gVid.total,gMcSamples,gMcSpp);else snprintf(sb,64,"%d / %d spp  %dx%d",gMcSamples,gMcFinal?gMcSpp:128,gMcW,gMcH);text(sb,px+1.2f*u,y2+10*rh+rh/2-2.5f*tp2,tp2*.9f,.7f,.7f,.75f);}
  gBtn.push_back({{px,py,pw2,rh*nr+1.6f*u},299});}
 if(gSculpt){ // barra inferior de escultura
  float bh0=14*u,by0=H-bh0;rect(0,by0,(float)W,bh0,0,0,0,.55f);gBlock.push_back({0,by0,(float)W,bh0});
  float x0=ML_+.4f*u,pw_=15.2f*u,ph_=6.2f*u,r1=by0+1.4f*u,r2=r1+ph_+1*u,tp2=.38f*u;
  for(int k=0;k<5;k++)pill(x0+k*(pw_+.4f*u),r1,pw_,ph_,94+k,gSBN[k],gSBrush==k,.3f,.3f,.33f);
  pill(x0,r2,24*u,ph_,99,gSInv?"Inverter: on":"Inverter: off",gSInv,.3f,.3f,.33f);pill(x0+24.4f*u,r2,27*u,ph_,82,gSSym?"Simetria X: on":"Simetria X: off",gSSym,.3f,.3f,.33f);
  float sx=x0+5*(pw_+.4f*u)+3*u;
  sld(5,sx,44*u,r1,ph_+.4f*u,tp2,fm("Raio %d",gSRad).c_str(),(gSRad-20)/180.f);
  sld(6,sx,44*u,r2,ph_+.4f*u,tp2,fm("Força %d%%",(int)(gSStr*100+.5f)).c_str(),(gSStr-.05f)/.95f);}
 if(gRigOpen){ // painel Rig (IK de 2 ossos + IK/FK + formas)
  float px=tbX+tbW+1*u,py=hH+10*u,pw2=27*u,rh=4.4f*u,tp2=.38f*u,bw_=pw2-2.4f*u,bh_=rh-.6f*u,y1=py+1.0f*u,hw=(bw_-.6f*u)/2,x2=px+1.2f*u+hw+.6f*u;
  int ri=sel>=0?rigOf(sel):-1;int nr=ri>=0?13:2;panel(px,py,pw2,rh*nr+1.6f*u,1*u);
  char tl[64];if(ri>=0)snprintf(tl,64,"IK: %.20s",objs[rigs[ri].c].nm);else snprintf(tl,64,"%s",sel>=0?"Criar IK neste osso":"Selecione mao ou pe");
  text(tl,px+1.2f*u,y1+rh/2-2.5f*tp2,tp2*.9f,.8f,.8f,.85f);
  pill(px+1.2f*u,y1+rh,bw_,bh_,70,"Criar IK (2 ossos)",false,.2f,.38f,.55f);
  if(ri>=0){Rig&g=rigs[ri];float w=rigW(g);int co=(sel>=0&&objs[sel].kind)?sel:g.tgt;
   pill(px+1.2f*u,y1+2*rh,bw_,bh_,71,fm("Modo: %s  (%d%%)",w>=.5f?"IK":"FK",(int)(w*100+.5f)).c_str(),w>=.5f,.3f,.3f,.33f);
   sld(3,px,pw2,y1+3*rh,rh,tp2,"Mistura",w);
   pill(px+1.2f*u,y1+4*rh,bw_,bh_,72,g.wk.empty()?"Chave IK/FK":fm("Chave IK/FK (%d)",(int)g.wk.size()).c_str(),!g.wk.empty(),.3f,.3f,.33f);
   pill(px+1.2f*u,y1+5*rh,bw_,bh_,77,"Limpar chaves IK/FK",false,.3f,.3f,.33f);
   pill(px+1.2f*u,y1+6*rh,hw,bh_,73,"Controle",false,.3f,.3f,.33f);pill(x2,y1+6*rh,hw,bh_,74,"Pole",false,.3f,.3f,.33f);
   pill(px+1.2f*u,y1+7*rh,bw_,bh_,78,fm("Forma: %s",gShapeN[std::clamp(objs[co].shape,0,5)]).c_str(),false,.3f,.3f,.33f);
   pill(px+1.2f*u,y1+8*rh,hw,bh_,80,"Tamanho -",false,.3f,.3f,.33f);pill(x2,y1+8*rh,hw,bh_,81,"Tamanho +",false,.3f,.3f,.33f);
   pill(px+1.2f*u,y1+9*rh,bw_,bh_,79,"Pole: inverter lado",false,.3f,.3f,.33f);
   pill(px+1.2f*u,y1+10*rh,bw_,bh_,17,gBones?"Ossos: on":"Ossos: off",gBones,.3f,.3f,.33f);
   pill(px+1.2f*u,y1+11*rh,bw_,bh_,75,"Converter em FK (bake)",false,.5f,.34f,.12f);
   pill(px+1.2f*u,y1+12*rh,bw_,bh_,76,"Remover IK",false,.5f,.2f,.2f);}
  gBtn.push_back({{px,py,pw2,rh*nr+1.6f*u},299});}
 // outliner + transform (direita)
 if(!gFileMenu){float pw=30*u,rx=W-ML_-pw,ry=11.5f*u,rh=4.4f*u,tpx=.38f*u;
 int n=(int)objs.size(),rows=std::min(n,3),st=(n>3&&sel>=0)?std::clamp(sel-2,0,n-3):0;
 float oh=16.3f*u; // o painel Scene agora e QML (SceneOutliner.qml); aqui so reserva o espaco

 if(sel>=0){T&t=objs[sel].cur;float ty=ry+oh+1.5f*u;panel(rx,ty,pw,4*u+(5+(objs[sel].kind==4?1:0))*rh+1*u,1*u);

  float lw=5.5f*u,fw=(pw-lw-1.2f*u)/3;static const char*XYZ[3]={"X","Y","Z"};
  for(int j=0;j<3;j++)tcen(XYZ[j],rx+lw+j*fw+fw/2,ty+2*u,tpx,AC[j][0],AC[j][1],AC[j][2]);
  static const char*LB3[3]={"Loc","Rot","Scl"};V val[3]={t.p,t.r*(180/PI),t.s};
  for(int k=0;k<3;k++){float yy=ty+4*u+k*rh;text(LB3[k],rx+1.2f*u,yy+2.2f*u-2.5f*tpx,tpx,.75f,.75f,.78f);
   for(int j=0;j<3;j++){char b[16];snprintf(b,16,k==1?"%.1f":"%.2f",(&val[k].x)[j]);float fx=rx+lw+j*fw;
    rect(fx,yy+.3f*u,fw-.5f*u,rh-.6f*u,.25f,.25f,.27f,1,.6f*u);tcen(b,fx+(fw-.5f*u)/2,yy+2.2f*u,tpx*.95f,.92f,.92f,.92f);}}
 if(objs[sel].kind==3){sld(4,rx,pw,ty+4*u+3*rh,rh,tpx,fm("FOV %d",(int)(objs[sel].fov*57.2958f+.5f)).c_str(),(objs[sel].fov-.17f)/2.23f);
   pill(rx+.9f*u,ty+4*u+4*rh+.3f*u,pw-1.8f*u,rh-.6f*u,19,gCamView==sel?"Sair da câmera":"Ver pela câmera",gCamView==sel,.2f,.38f,.55f);}
  else if(objs[sel].kind==4){Obj&lo_=objs[sel];float lo=lo_.shape==0?.3f:3.f,hi=lo_.shape==0?30.f:1000.f;
   sld(7,rx,pw,ty+4*u+3*rh,rh,tpx,fm("Intens. %.1f",lo_.len).c_str(),logf(std::max(lo_.len,lo)/lo)/logf(hi/lo));
   if(lo_.shape==2)sld(8,rx,pw,ty+4*u+4*rh,rh,tpx,fm("Cone %d",(int)(lo_.fov*57.2958f+.5f)).c_str(),(lo_.fov-.1f)/1.3f);
   else text(lo_.shape==0?"Sol (direcional)":"Luz de ponto",rx+1.2f*u,ty+4*u+4*rh+rh/2-2.5f*tpx,tpx,.7f,.7f,.74f);
   {float ssz=rh*.7f,sx0=rx+1.2f*u,sy0=ty+4*u+5*rh;for(int k=0;k<6;k++){V cc=LCOL[k];float sxk=sx0+k*(ssz+.9f*u);rect(sxk,sy0+(rh-ssz)/2,ssz,ssz,cc.x,cc.y,cc.z,1,ssz/2);gBtn.push_back({{sxk,sy0,ssz+.9f*u,rh},23+k});}}}
  else{sld(0,rx,pw,ty+4*u+3*rh,rh,tpx,"Metal",objs[sel].met);sld(1,rx,pw,ty+4*u+4*rh,rh,tpx,"Rough",objs[sel].rou);}}
 if(sel>=0){float ty2=ry+oh+1.5f*u+4*u+(5+(objs[sel].kind==4?1:0))*rh+1*u+1.2f*u,bs2=6.2f*u,gx=rx;static const int AI[3]={9,11,12};
  for(int k=0;k<3;k++){int id=AI[k];bool on=(id==11&&parentMode);rect(gx,ty2,bs2,bs2,on?.28f:.16f,on?.45f:.16f,on?.7f:.18f,.94f,bs2/2);icon2(id,gx+bs2/2,ty2+bs2/2,bs2*.27f,on);gBtn.push_back({{gx,ty2,bs2,bs2},id});gx+=bs2+.8f*u;}}}
 // timeline estilo Maya/Houdini (compacta)
 float TH=17.5f*u,tT=100*u-3.6f*u-TH,tw_=W-2*ML_;if(gTlOpen)panel(ML_,tT,tw_,TH,1*u);
 if(gTlOpen){float x0=ML_+.9f*u,wd=tw_-1.8f*u;pill(x0+wd-15*u,tT+.8f*u,14.5f*u,6.2f*u,62,"Recolher",false,.27f,.27f,.29f);
  { // linha 1: transporte, frame atual, keys
   float y=tT+.8f*u,h=6.2f*u,bwt=6.4f*u,gp=.3f*u,x=x0;static const int TID[5]={50,51,52,53,54};
   for(int k=0;k<5;k++){bool on=(k==2&&!playing)||(k==1&&playing&&gPlayDir<0)||(k==3&&playing&&gPlayDir>0)||gFlash[TID[k]]>0;
    rect(x,y,bwt,h,on?.28f:.27f,on?.45f:.27f,on?.7f:.29f,1,.7f*u);icon3(TID[k],x+bwt/2,y+h/2,h*.26f,on);gBtn.push_back({{x,y,bwt,h},TID[k]});x+=bwt+gp;}
   x+=1.0f*u;float fw=11*u;rect(x,y,fw,h,.09f,.09f,.1f,1,.7f*u);gFldR[4]={x,y,fw,h};
   char fb_[16];snprintf(fb_,16,"%d",(int)roundf(tm*24));tcen(fb_,x+fw/2,y+h/2,.46f*u,.95f,.95f,.95f);
   tcen("<",x+1.3f*u,y+h/2,.36f*u,.5f,.5f,.55f);tcen(">",x+fw-1.3f*u,y+h/2,.36f*u,.5f,.5f,.55f);x+=fw+gp+.6f*u;
   for(int k=0;k<2;k++){int id=55+k;float bw2=4.4f*u;rect(x,y,bw2,h,.27f,.27f,.29f,1,.7f*u);icon3(id,x+bw2/2,y+h/2,h*.2f,false);gBtn.push_back({{x,y,bw2,h},id});x+=bw2+gp;}
   x+=1.2f*u;bool kon=gFlash[7]>0;rect(x,y,h,h,kon?.28f:.2f,kon?.45f:.2f,kon?.7f:.22f,1,h/2);icon3(7,x+h/2,y+h/2,h*.28f,false);gBtn.push_back({{x,y,h,h},7});x+=h+.3f*u;
   float dw=3.2f*u;rect(x,y,dw,h,.22f,.22f,.24f,1,.7f*u);icon3(57,x+dw/2,y+h/2,h*.14f,false);gBtn.push_back({{x,y,dw,h},57});}
  { // linha 2: regua
   float ry=tT+7.4f*u,rh=6.0f*u,ppf;gTX0=x0+.6f*u;gTW=wd-1.2f*u;gTrack={x0,ry,wd,rh};gBlock.push_back(gTrack);
   rect(x0,ry,wd,rh,.12f,.12f,.13f,1,.6f*u);ppf=gTW/(float)std::max(gVE-gVS,1);float yl=ry+4.3f*u;
   if(sel>=0)for(auto&k:objs[sel].k){float kx=tx(k.t);if(kx<gTX0-1||kx>gTX0+gTW+1)continue;float bw3=std::clamp(ppf*.85f,.5f*u,1.8f*u);rect(kx-bw3/2,yl-1.9f*u,bw3,3.8f*u,.3f,.62f,.08f);}
   rect(x0+.3f*u,yl-.05f*u,wd-.6f*u,.1f*u,.6f,.6f,.63f);
   static const int ST[8]={1,2,5,10,20,25,50,100};int step=100;for(int k=0;k<8;k++)if(ST[k]*ppf>=9*u){step=ST[k];break;}
   int minor=step>=10?step/5:(step>=5?1:0);
   for(int f=gVS;f<=gVE;f++){float xx=tx(f/24.f);bool major=f%step==0;
    if(major){rect(xx-.08f*u,yl-1.8f*u,.16f*u,3.6f*u,.78f,.78f,.8f);char b[12];snprintf(b,12,"%d",f);tcen(b,xx,ry+1.3f*u,.32f*u,.6f,.6f,.64f);}
    else if(minor>0&&f%minor==0)rect(xx-.06f*u,yl-.9f*u,.12f*u,1.8f*u,.5f,.5f,.54f);}
   float cf=tm*24;if(cf>=gVS-.01f&&cf<=gVE+.01f){float px=tx(tm);char b[12];snprintf(b,12,"%d",(int)roundf(cf));float bw3=tw(b,.4f*u)+3.2f*u;
    rect(px-.06f*u,ry+3.6f*u,.12f*u,rh-3.6f*u,1,1,1);rect(px-bw3/2-.15f*u,ry+.1f*u,bw3+.3f*u,3.5f*u,.95f,.95f,.95f,1,.6f*u);
    rect(px-bw3/2,ry+.25f*u,bw3,3.2f*u,.05f,.05f,.06f,1,.5f*u);tcen(b,px,ry+1.85f*u,.4f*u,1,1,1);dia(px,ry+3.6f*u,1.4f*u,.95f,.95f,.95f);}}
  { // linha 3: intervalo global e janela visivel
   float ry3=tT+13.9f*u,h3=3.4f*u,fw3=8.2f*u;
   gFldR[0]={x0,ry3,fw3,h3};gFldR[1]={x0+fw3+.6f*u,ry3,fw3,h3};gFldR[2]={x0+wd-2*fw3-.6f*u,ry3,fw3,h3};gFldR[3]={x0+wd-fw3,ry3,fw3,h3};
   int vals[4]={gFS,gVS,gVE,gFE};
   for(int k=0;k<4;k++){bool inn=k==1||k==2;rect(gFldR[k].x,ry3,fw3,h3,inn?.07f:.16f,inn?.07f:.16f,inn?.08f:.18f,1,.5f*u);
    char b[12];snprintf(b,12,"%d",vals[k]);tcen(b,gFldR[k].x+fw3/2,ry3+h3/2,.4f*u,.92f,.92f,.92f);}
   gTrX0=gFldR[1].x+fw3+1.2f*u;gTrW=gFldR[2].x-1.2f*u-gTrX0;gBarY=ry3;gBarH=h3;
   float rng=(float)std::max(gFE-gFS,1),b0=gTrX0+(gVS-gFS)/rng*gTrW,b1=gTrX0+(gVE-gFS)/rng*gTrW,cy=ry3+h3/2;
   rect(gTrX0,cy-.2f*u,gTrW,.4f*u,.07f,.07f,.08f,1,.2f*u);rect(b0,cy-.35f*u,std::max(b1-b0,.5f*u),.7f*u,.55f,.55f,.58f,1,.3f*u);
   rect(b0-.5f*u,ry3+.3f*u,1.0f*u,h3-.6f*u,.75f,.75f,.78f,1,.3f*u);rect(b1-.5f*u,ry3+.3f*u,1.0f*u,h3-.6f*u,.75f,.75f,.78f,1,.3f*u);}}
else if(!gSculpt&&!gTlQml){ // faixa fina: play + linha do tempo
 float sh=9*u,y0=H-sh;rect(0,y0,(float)W,sh,0,0,0,.5f);gBlock.push_back({0,y0,(float)W,sh});
 float pb=6.6f*u,px0=ML_+.2f*u,py0=y0+(sh-pb)/2;bool pon=playing||gFlash[8]>0;
 rect(px0,py0,pb,pb,pon?.28f:.2f,pon?.45f:.2f,pon?.7f:.22f,.95f,pb/2);icon2(8,px0+pb/2,py0+pb/2,pb*.28f,pon);gBtn.push_back({{px0,py0,pb,pb},8});
 char fb2[16];snprintf(fb2,16,"%d",(int)roundf(tm*24));tcen(fb2,px0+pb+4.5f*u,y0+sh/2,.5f*u,.95f,.95f,.95f);
 float trX=ML_+17*u,trW=W-ML_-trX-15*u,ty0=y0+1*u,th=7*u;gTX0=trX+.6f*u;gTW=trW-1.2f*u;gTrack={trX,ty0,trW,th};gBlock.push_back(gTrack);
 float ppf=gTW/(float)std::max(gVE-gVS,1),yl=ty0+4.6f*u;
 static const int ST2[8]={1,2,5,10,20,25,50,100};int stp=100;for(int k=0;k<8;k++)if(ST2[k]*ppf>=9*u){stp=ST2[k];break;}
 for(int f=gVS;f<=gVE;f++){float xx=tx(f/24.f);if(f%stp==0){rect(xx-.08f*u,yl-1.4f*u,.16f*u,2.8f*u,.7f,.7f,.74f);char b[12];snprintf(b,12,"%d",f);tcen(b,xx,ty0+1.1f*u,.3f*u,.6f,.6f,.64f);}else if(stp>=5&&f%(stp>=10?stp/5:1)==0)rect(xx-.05f*u,yl-.6f*u,.1f*u,1.2f*u,.45f,.45f,.5f);}
 if(sel>=0)for(auto&k:objs[sel].k){float kx=tx(k.t);if(kx<gTX0-1||kx>gTX0+gTW+1)continue;dia(kx,yl,1.4f*u,.95f,.7f,.15f);}
 {float cf=tm*24;if(cf>=gVS-.01f&&cf<=gVE+.01f){float pxh=tx(tm);rect(pxh-.07f*u,ty0+2.2f*u,.14f*u,th-2.2f*u,1,1,1);dia(pxh,ty0+2.4f*u,1.5f*u,.95f,.95f,.95f);}}
 pill(W-ML_-13.5f*u,y0+(sh-6.2f*u)/2,13.5f*u,6.2f*u,62,"Timeline",false,.27f,.27f,.29f);}
else{gTrack={0,0,0,0};}
{float bs=7.4f*u,yb=(gTlOpen?tT:(gSculpt?(float)H-14*u:(float)H-(gTlQml?20.5f:9.f)*u))-bs-1.2f*u,xb=W-ML_-bs;bool on=gRigOpen;rect(xb,yb,bs,bs,on?.28f:.16f,on?.45f:.16f,on?.7f:.18f,.94f,bs/2);icon2(63,xb+bs/2,yb+bs/2,bs*.27f,on);gBtn.push_back({{xb,yb,bs,bs},63});}
 if(gImportOpen){float pw2=82*u,rh2=5.8f*u,tp2=.46f*u;int n=(int)gFiles.size();float lh=6*rh2,ph2=6.5f*u+lh+1.5f*u,px2=(W-pw2)/2,py2=(H-ph2)/2;
             float tot=n*rh2,maxs=std::max(0.f,tot-lh);gScrollPx=std::clamp(gScrollPx,0.f,maxs);gRowH=rh2;gListR={px2+1*u,py2+6.5f*u,pw2-2*u,lh};
             panel(px2,py2,pw2,ph2,1.2f*u);text((gPick==0?"Importar glTF / GLB / ZIP":gPick==1?"Abrir projeto (.mad)":"Importar .mad"),px2+2*u,py2+2.6f*u-2.5f*tp2,tp2,.7f,.7f,.75f);
             if(gFiles.empty()){text((gPick==0?"Nenhum .gltf/.glb/.zip em Download.":"Nenhum .mad em Download."),px2+2*u,py2+8.1f*u-2.5f*tp2,tp2,.9f,.9f,.9f);
              text("Ative Acesso a todos os arquivos nas permissoes do app.",px2+2*u,py2+8.1f*u+rh2-2.5f*tp2,tp2*.9f,.6f,.6f,.65f);}
             else{glEnable(GL_SCISSOR_TEST);glScissor((GLint)gListR.x,(GLint)(H-gListR.y-gListR.h),(GLsizei)gListR.w,(GLsizei)gListR.h);
              for(int i=0;i<n;i++){float yy=gListR.y+i*rh2-gScrollPx;if(yy+rh2<gListR.y||yy>gListR.y+lh)continue;rect(px2+1*u,yy,pw2-2*u-1.6f*u,rh2-.5f*u,.24f,.24f,.27f,1,.7f*u);
               std::string nf=gFiles[i];size_t s1=nf.rfind('/');if(s1!=std::string::npos&&s1>0){size_t s2=nf.rfind('/',s1-1);nf=nf.substr(s2==std::string::npos?0:s2+1);}if(nf.size()>34)nf=nf.substr(nf.size()-34);
               text(nf.c_str(),px2+2.5f*u,yy+rh2/2-.25f*u-2.5f*tp2,tp2,.95f,.95f,.95f);}
              glDisable(GL_SCISSOR_TEST);
              if(maxs>0){float bh_=lh*lh/tot,by_=gListR.y+(lh-bh_)*(gScrollPx/maxs);rect(px2+pw2-1.8f*u,by_,.6f*u,bh_,.6f,.6f,.65f,1,.3f*u);}}
             gBtn.push_back({{px2,py2,pw2,ph2},299});}
if(gSizeOpen){ // caixa "Tamanho da render" (teclado do Android)
 rect(0,0,(float)W,(float)H,0,0,0,.55f);gBlock.push_back({0,0,(float)W,(float)H});
 float pad=2*u,pw2=std::min(104*u,W-2*ML_),fh=6.6f*u,bh=5.8f*u,rh=5.4f*u,ph2=pad+5.6f*u+fh+1.6f*u+2*(rh+.6f*u)+1.2f*u+bh+1*u+3.2f*u+pad,px=(W-pw2)/2,py=hH+2*u;
 panel(px,py,pw2,ph2,1.4f*u);
 text("Tamanho da render",px+pad,py+pad,.6f*u,.95f,.95f,.97f);
 float fy=py+pad+5.6f*u,tF=.56f*u,fw=(pw2-2*pad-3*u)/2;
 for(int k=0;k<2;k++){float fx=px+pad+k*(fw+3*u);bool act=gSizeField==k;rect(fx,fy,fw,fh,act?.16f:.07f,act?.2f:.07f,act?.28f:.08f,1,.9f*u);gBtn.push_back({{fx,fy,fw,fh},2010+k});
  text(k?"Altura":"Largura",fx+1.4f*u,fy+fh/2-2.5f*tF*.75f,tF*.75f,.6f,.6f,.66f);
  bool emp=gSizeBuf[k].empty();const char*sv=emp?"auto":gSizeBuf[k].c_str();float vx=fx+10.5f*u;
  text(sv,vx,fy+fh/2-2.5f*tF,tF,emp?.45f:.95f,emp?.45f:.95f,emp?.5f:.97f);
  if(act){float cx=vx+(emp?0:tw(sv,tF));rect(cx+.2f*u,fy+1.1f*u,.3f*u,fh-2.2f*u,.9f,.9f,.95f,1);}}
 text("x",px+pad+fw+1.1f*u,fy+fh/2-2.5f*tF,tF,.7f,.7f,.75f);
 static const char*pn[7]={"Tela (auto)","FHD 1920x1080","HD 1280x720","2K 2560x1440","4K 3840x2160","1:1 1080x1080","9:16 1080x1920"};
 static const int ppw[7]={0,1920,1280,2560,3840,1080,1080},pph[7]={0,1080,720,1440,2160,1080,1920};
 float pbw=(pw2-2*pad-3*.6f*u)/4,py0=fy+fh+1.6f*u;
 for(int k=0;k<7;k++){int row=k/4,col=k%4;bool on=(ppw[k]==0)?(gSizeBuf[0].empty()&&gSizeBuf[1].empty()):(gSizeBuf[0]==std::to_string(ppw[k])&&gSizeBuf[1]==std::to_string(pph[k]));
  pill(px+pad+col*(pbw+.6f*u),py0+row*(rh+.6f*u),pbw,rh,2020+k,pn[k],on,.3f,.3f,.33f);}
 {float y5=py0+2*(rh+.6f*u)+1.2f*u,bw=(pw2-2*pad-.8f*u)/2;pill(px+pad,y5,bw,bh,2013,"Cancelar",false,.3f,.3f,.33f);pill(px+pad+bw+.8f*u,y5,bw,bh,2012,"OK",true,.2f,.38f,.55f);
  text("Toque num campo para digitar. Tamanhos grandes usam muita memoria.",px+pad,y5+bh+1*u,.3f*u,.55f,.55f,.6f);}
 gBtn.push_back({{0,0,(float)W,(float)H},299});}
if(gNameOpen){ // caixa "Nomeie o projeto" (o teclado e o do Android)
 rect(0,0,(float)W,(float)H,0,0,0,.55f);gBlock.push_back({0,0,(float)W,(float)H});
 float pad=2.2f*u,pw2=std::min(92*u,W-2*ML_),fh=7.2f*u,bh=6.4f*u,ph2=pad+6.2f*u+fh+1.8f*u+bh+1.2f*u+3.6f*u+pad,px=(W-pw2)/2,py=hH+3*u;
 panel(px,py,pw2,ph2,1.4f*u);
 float tT=.62f*u;text(gNameMode==0?"Nomeie o projeto":"Renomear o projeto",px+pad,py+pad,tT,.95f,.95f,.97f);
 float fy=py+pad+6.2f*u,tF=.6f*u;rect(px+pad,fy,pw2-2*pad,fh,.07f,.07f,.08f,1,.9f*u);gBtn.push_back({{px+pad,fy,pw2-2*pad,fh},2005});
 bool emp=gNameBuf.empty();const char*shown=emp?"Projeto sem título":gNameBuf.c_str();
 text(shown,px+pad+1.6f*u,fy+fh/2-2.5f*tF,tF,emp?.45f:.95f,emp?.45f:.95f,emp?.5f:.97f);
 {float cx=px+pad+1.6f*u+(emp?0:tw(shown,tF));rect(cx+.2f*u,fy+1.2f*u,.3f*u,fh-2.4f*u,.9f,.9f,.95f,1);}
 {float y5=fy+fh+1.8f*u,bw=(pw2-2*pad-.8f*u)/2;pill(px+pad,y5,bw,bh,2004,"Cancelar",false,.3f,.3f,.33f);pill(px+pad+bw+.8f*u,y5,bw,bh,2003,"OK",true,.2f,.38f,.55f);
  text("Toque no campo para abrir o teclado",px+pad,y5+bh+1.2f*u,.32f*u,.55f,.55f,.6f);}
 gBtn.push_back({{0,0,(float)W,(float)H},299});}
 if(gToastT>0){float tt=.4f*u,w2=tw(gToast.c_str(),tt)+4*u,bx=(W-w2)/2,by=(gTlOpen?tT:(float)H-(gTlQml?21.f:9.5f)*u)-7*u;rect(bx,by,w2,5.4f*u,.08f,.08f,.08f,.93f,1*u);text(gToast.c_str(),bx+2*u,by+2.7f*u-2.5f*tt,tt,1,1,1);}
}

// ---------- gizmos (ImGuizmo) ----------
// So os gizmos de mover/girar/escalar usam o ImGuizmo. O resto da interface continua como estava.
bool gGzEdited=false;bool gGzOK=false,gGzBackend=false,gGzUsing=false,gGzClear=false;float gGzLen0=1;
void gzInit(){
 if(ImGui::GetCurrentContext()){if(gGzBackend)ImGui_ImplOpenGL3_Shutdown();ImGui::DestroyContext();gGzBackend=false;} // GL novo: recria tudo
 ImGui::CreateContext();ImGuiIO&io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.ConfigFlags|=ImGuiConfigFlags_NoMouseCursorChange;
 gGzBackend=gGzOK=ImGui_ImplOpenGL3_Init("#version 300 es");gGzUsing=false;gGzClear=false;}
bool gzActive(){return gGzOK&&sel>=0&&sel<(int)objs.size()&&tool<3&&!gSculpt&&!gMcFinal&&sel!=gCamView&&W>8&&H>8;}
M gzObjM(int i){M w=worldM(i);
 if(objs[i].mesh==5)for(int c=0;c<3;c++){float l=sqrtf(w.m[c*4]*w.m[c*4]+w.m[c*4+1]*w.m[c*4+1]+w.m[c*4+2]*w.m[c*4+2]);if(l>1e-8f)for(int r=0;r<3;r++)w.m[c*4+r]/=l;} // osso: gizmo em escala 1 (o tamanho e o "len")
 return w;}
void gzApply(const M&res){gGzEdited=true;Obj&ob=objs[sel];M P=ob.parent>=0?worldM(ob.parent):id();T d=decomp(mul(invAff(P),res));
 if(tool==0)ob.cur.p=d.p;
 else if(tool==1)ob.cur.r=d.r;
 else if(ob.mesh==5){float f[3];for(int c=0;c<3;c++)f[c]=sqrtf(res.m[c*4]*res.m[c*4]+res.m[c*4+1]*res.m[c*4+1]+res.m[c*4+2]*res.m[c*4+2]);
  float b=f[0];for(int c=1;c<3;c++)if(fabsf(logf(std::max(f[c],1e-4f)))>fabsf(logf(std::max(b,1e-4f))))b=f[c];
  ob.len=std::max(.05f,gGzLen0*b);}
 else{ob.cur.s=V{std::max(.05f,d.s.x),std::max(.05f,d.s.y),std::max(.05f,d.s.z)};}}
// Roda um quadro do ImGui com o gizmo (o quadro de teste do toque tambem renderiza; o motor limpa a tela logo depois).
void gzRun(bool draw){
 ImGuiIO&io=ImGui::GetIO();io.DisplaySize=ImVec2((float)W,(float)H);io.DeltaTime=std::clamp(gDt,.001f,.1f);
 float k=std::max(1.f,H/540.f);ImGui::GetStyle().FontScaleMain=k;
 ImGui_ImplOpenGL3_NewFrame();ImGui::NewFrame();ImGuizmo::BeginFrame();
 if(gGzClear&&!io.MouseDown[0]){io.AddMousePosEvent(-FLT_MAX,-FLT_MAX);gGzClear=false;} // dedo saiu: apaga o destaque
 ImGuizmo::SetOrthographic(false);ImGuizmo::Enable(true);ImGuizmo::AllowAxisFlip(false);ImGuizmo::SetGizmoSizeClipSpace(.17f);
 ImGuizmo::Style&st=ImGuizmo::GetStyle();st.TranslationLineThickness=3.f*k;st.TranslationLineArrowSize=7.f*k;st.RotationLineThickness=2.f*k;st.RotationOuterLineThickness=3.f*k;
 st.ScaleLineThickness=3.f*k;st.ScaleLineCircleSize=7.f*k;st.HatchedAxisLineThickness=6.f*k;st.CenterCircleSize=7.f*k;
 ImGui::SetNextWindowPos(ImVec2(0,0));ImGui::SetNextWindowSize(ImVec2((float)W,(float)H*.685f)); // so ate acima dos paineis da timeline
 ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0.f);
 ImGui::Begin("##gz",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoFocusOnAppearing|ImGuiWindowFlags_NoNav);
 ImGuizmo::SetDrawlist();ImGuizmo::SetRect(0,0,(float)W,(float)H);
 M vm=lookAt(camEye(),tgt,{0,1,0}),pm=persp(FOV,(float)W/H,nearP(),100),mat=gzObjM(sel);
 ImGuizmo::OPERATION op=tool==0?ImGuizmo::TRANSLATE:tool==1?ImGuizmo::ROTATE:ImGuizmo::SCALE;
 ImGuizmo::Manipulate(vm.m,pm.m,op,tool==2?ImGuizmo::LOCAL:ImGuizmo::WORLD,mat.m,nullptr,nullptr);
 bool use=ImGuizmo::IsUsing();
 if(use&&!gGzUsing)gGzLen0=objs[sel].len;
 gGzUsing=use;
 if(use&&draw)gzApply(mat);
 ImGui::End();ImGui::PopStyleVar(2);
 if(draw){ImGui::Render();ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());}else ImGui::EndFrame();}
float gGzOX=0,gGzOY=0; // deslocamento do dedo ate o ponto que acertou o gizmo (tolerancia de toque)
bool gzProbe(float x,float y){if(!gzActive())return false;ImGuiIO&io=ImGui::GetIO();
 float r=16.f*std::max(1.f,H/540.f);static const float ox[9]={0,1,-1,0,0,.7f,-.7f,.7f,-.7f},oy[9]={0,0,0,1,-1,.7f,.7f,-.7f,-.7f};
 for(int i=0;i<9;i++){float px=x+ox[i]*r,py=y+oy[i]*r;io.AddMousePosEvent(px,py);
  gzRun(false);gzRun(false); // IsOver() do ImGuizmo e' do quadro anterior: precisa de 2 quadros
  if(ImGuizmo::IsOver()){gGzOX=px-x;gGzOY=py-y;return true;}}
 io.AddMousePosEvent(-FLT_MAX,-FLT_MAX);return false;}
void gzDown(){ImGui::GetIO().AddMouseButtonEvent(0,true);}
void gzMove(float x,float y){ImGui::GetIO().AddMousePosEvent(x+gGzOX,y+gGzOY);}
void gzUp(){ImGui::GetIO().AddMouseButtonEvent(0,false);gGzClear=true;}
// ---------- input ----------
void pressInner(int i){
 if(gVid.on&&i!=36)return;
 if(i==44){gAddFan=!gAddFan;return;}
 if(i>=3&&i<=6)gToolFan=false;if(i<3||i==10||i==18)gAddFan=false;
 if(i>=0&&i<64)gFlash[i]=.35f;
 if(i==60){gFileMenu=!gFileMenu;return;}if(i==61){gFileMenu=false;gRenderReq=true;return;}if(i==63){gRigOpen=!gRigOpen;if(gRigOpen)gRenderOpen=false;return;}if(i==62){gTlOpen=!gTlOpen;return;}
 if(i>=2000&&i<2200){kbKey(i-2000);return;}
 if(i>=200){int k=i-200;if(i!=299){gImportOpen=false;if(k<(int)gFiles.size())pickPath(gFiles[k]);}return;}
 if(i<3)addObj(i);else if(i<7)tool=i-3;
 else if(i==7)insertKey();else if(i==8){playing=!playing;if(playing)gPlayDir=1;}
 else if(i==9){if(sel>=0)delObj(sel);}
 else if(i==10)addObj(5);
 else if(i==11){if(sel<0)toast("Selecione um objeto");else{parentMode=true;toast("Toque no objeto PAI");}}
 else if(i==12){if(sel>=0)clearParent(sel);}
 else if(i==13||i==14){gFileMenu=false;if(!filesGranted())needFiles();else if(i==13)exportMAD();else exportGLTF();}
 else if(i==15||i==41||i==43){gFileMenu=false;if(!filesGranted())needFiles();else{gPick=i==15?0:i==41?1:2;scanFiles();gScrollPx=0;gImportOpen=true;}}
 else if(i==40){gFileMenu=false;newProject();}
 else if(i==42){gFileMenu=false;if(!filesGranted())needFiles();else saveProject();}
 else if(i==35){gMcDn=!gMcDn;gDnDirty=true;}
 else if(i==36){if(gVid.on)vidFinish(true);else vidStart();}
 else if(i==37){gVidQ=(gVidQ+1)%3;}
 else if(i==38){gMcLeak=!gMcLeak;gMcHaveCam=false;gSigS=-1;mcReset();}
 else if(i==70)rigCreate();
 else if(i==39)sizeOpen();
 else if(i==18)addCamera();
 else if(i==19)camViewToggle();
 else if(i==91){if(gSculpt)sculptUndo();else doUndo();}
 else if(i==92){if(gSculpt)sculptRedo();else doRedo();}
 else if(i==93||i==83)sculptToggle();
 else if(i>=94&&i<=98)gSBrush=i-94;
 else if(i==99)gSInv=!gSInv;
 else if(i==82)gSSym=!gSSym;
 else if(i==84){if(gSculpt)sculptToggle();}
 else if(i==85||i==86)toast(i==85?"Modo Modelagem: em desenvolvimento":"Modo Escultura: em desenvolvimento");
 else if(i==90){gFileMenu=false;gNameMode=1;gNameBuf=gProj;gNameOpen=true;gImportOpen=false;showKb();}
 else if(i>=71&&i<=81){int ri=sel>=0?rigOf(sel):-1;if(ri>=0){
  if(i==71)rigToggle(ri);
  else if(i==72){rigKeyW(rigs[ri],rigW(rigs[ri]));toast("Chave IK/FK criada");}
  else if(i==73)sel=rigs[ri].tgt;
  else if(i==74){if(rigs[ri].pole>=0)sel=rigs[ri].pole;}
  else if(i==75)rigBake(ri);
  else if(i==76)rigRemove(ri);
  else if(i==77){float w=rigW(rigs[ri]);rigs[ri].wk.clear();rigs[ri].w=w;}
  else if(i==78||i==80||i==81){int co=(sel>=0&&objs[sel].kind)?sel:rigs[ri].tgt;
   if(i==78)objs[co].shape=(objs[co].shape+1)%6;else objs[co].cur.s=objs[co].cur.s*(i==80?.8f:1.25f);}
  else if(i==79){int po=rigs[ri].pole;if(po>=0&&po<(int)objs.size()){V Bp=wp(rigs[ri].b),Pp=wp(po);objs[po].cur.p=Bp+(Bp-Pp);toast("Pole do outro lado");}}}}
 else if(i==16)gSSAO=!gSSAO;
 else if(i==17)gBones=!gBones;
 else if(i==45)addLight(0);else if(i==46)addLight(1);else if(i==47)addLight(2);
 else if(i>=23&&i<=28&&sel>=0&&objs[sel].kind==4)objs[sel].col=LCOL[i-23];
 else if(i==30){gMcPrev=!gMcPrev;if(gMcPrev)gMcFinal=false;gMcHaveCam=false;gSigS=-1;mcReset();}
 else if(i==31){gMcFinal=!gMcFinal;if(gMcFinal)gMcPrev=false;gMcHaveCam=false;gSigS=-1;mcReset();}
 else if(i==32){if(!filesGranted())needFiles();else mcSave();}
 else if(i==33)gMcSpp=gMcSpp>=1024?16:gMcSpp*4;
 else if(i==34){gMcGround=!gMcGround;gSigS=-1;}
 else if(i==50){tm=gFS/24.f;applyAnim();}else if(i==51){playing=true;gPlayDir=-1;}else if(i==52)playing=false;else if(i==53){playing=true;gPlayDir=1;}else if(i==54){tm=gFE/24.f;applyAnim();}
 else if(i==55)jumpKey(-1);else if(i==56)jumpKey(1);else if(i==57)delKey();
 else if(i>=20&&i<=23){if(i==20)tm=0;else if(i==21)tm=std::max(0.f,tm-1/24.f);else if(i==22)tm=std::min(DUR,tm+1/24.f);else tm=DUR;applyAnim();}}
void press(int i){pressInner(i);if(i!=91&&i!=92)undoCommit();}
void scrub(float x){int f=(int)roundf(gVS+std::clamp((x-gTX0)/gTW,0.f,1.f)*(gVE-gVS));tm=std::clamp(f,gFS,gFE)/24.f;applyAnim();}
void pick(float x,float y){
 {int cb=-1;float cdist=50;for(int i=0;i<(int)objs.size();i++){if(!objs[i].kind||objs[i].hide)continue;float hx,hy;if(!prj(wp(i),hx,hy))continue;float dd=hypotf(x-hx,y-hy);if(dd<cdist){cdist=dd;cb=i;}}
  if(cb>=0){if(parentMode)finishParent(cb);else if(objs[cb].kind==3&&sel==cb)camViewToggle();else sel=cb;return;}}
 if(gBones){int bb=-1;float bd=40;for(int i=0;i<(int)objs.size();i++){if(objs[i].mesh!=5||objs[i].hide)continue;M w=worldM(i);
   V hd{w.m[12],w.m[13],w.m[14]},tl=hd+V{w.m[4],w.m[5],w.m[6]}*objs[i].len;float hx,hy,qx,qy;if(!prj(hd,hx,hy)||!prj(tl,qx,qy))continue;
   float vx=qx-hx,vy=qy-hy,tt=std::clamp(((x-hx)*vx+(y-hy)*vy)/(vx*vx+vy*vy+1e-3f),0.f,1.f);float dd=hypotf(x-hx-vx*tt,y-hy-vy*tt);if(dd<bd){bd=dd;bb=i;}}
  if(bb>=0){if(parentMode)finishParent(bb);else sel=bb;return;}}
 V e=camEye(),f=norm(tgt-e),s2=norm(cross(f,V{0,1,0})),u=cross(s2,f);
 float th=tanf(FOV/2),as=(float)W/H;V d=norm(f+s2*((2*x/W-1)*th*as)+u*((1-2*y/H)*th));
 static const float RF[3]={.87f,.5f,1.42f};int best=-1;float bt=1e9f;
 for(int i=0;i<(int)objs.size();i++){Obj&o=objs[i];if(o.hide)continue;V c=wp(i);float r;
  if(o.mesh==5){M w=worldM(i);c=c+V{w.m[4],w.m[5],w.m[6]}*(o.len*.5f);r=o.len*.35f;}
  else r=(o.mesh<3?RF[o.mesh]:meshes[o.mesh].rad)*std::max({o.cur.s.x,o.cur.s.y,o.cur.s.z});
  V oc=e-c;float b=dot(oc,d),cc=dot(oc,oc)-r*r,ds=b*b-cc;if(ds<0)continue;
  float tt=-b-sqrtf(ds);if(tt>0&&tt<bt){bt=tt;best=i;}}
 if(parentMode)finishParent(best);else sel=best;}
void panCam(float dx,float dy){V e=camEye(),f=norm(tgt-e),r=norm(cross(f,V{0,1,0})),u=cross(r,f);float s=2*cd*tanf(FOV/2)/H;tgt=tgt+r*(-dx*s)+u*(dy*s);if(gCamView>=0)camFromView();}
void orbit(float dx,float dy){yaw-=dx*.006f;pitch=std::clamp(pitch+dy*.006f,-1.5f,1.5f);if(gCamView>=0)camFromView();}
void gdrag(float dx,float dy){
 Obj&ob=objs[sel];T&t=ob.cur;V o=wp(sel);float ox,oy,ex,ey;
 if(tool==1){float px,py,qx,qy;if(!prj(ringPt(gAxis,gIdx+1),px,py)||!prj(ringPt(gAxis,gIdx+31),qx,qy))return;
  float vx=px-qx,vy=py-qy,l=hypotf(vx,vy);if(l<1)return;rotateWorld(ob,gAxis,(dx*vx+dy*vy)/l*.012f);return;}
 if(!prj(o,ox,oy)||!prj(o+AX[gAxis]*gl(),ex,ey))return;
 float vx=ex-ox,vy=ey-oy,k=(dx*vx+dy*vy)/(vx*vx+vy*vy+1e-3f);
 if(tool==0){V nw=o+AX[gAxis]*(k*gl());t.p=ob.parent>=0?invPt(worldM(ob.parent),nw):nw;}
 else if(ob.mesh==5)ob.len=std::max(.05f,ob.len*(1+k));
 else{float&sc=(&t.s.x)[gAxis];sc=std::max(.05f,sc*(1+k));}}
void snap(float x,float y){ND d[6];navPts(d);
 for(auto&e:d)if(hypotf(x-e.x,y-e.y)<H*.035f){
  if(e.a==0){yaw=e.pos?PI/2:-PI/2;pitch=0;}else if(e.a==1)pitch=e.pos?1.45f:-1.45f;else{yaw=e.pos?0.f:PI;pitch=0;}return;}}
int32_t onInput(ShimEvt*e){
 static int mode=0;static float lx,ly,sx,sy,pinch,lmx,lmy;static bool moved,rl;
 if(e->type==1&&(gNameOpen||gSizeOpen)){
  int ac=e->keyAction,kc=e->keyCode;bool used=false;
  if(kc==AKEYCODE_DEL){used=true;if(ac==AKEY_EVENT_ACTION_DOWN){if(gNameOpen)nameBack();else sizeBack();}}
  else if(kc==AKEYCODE_ENTER||kc==AKEYCODE_NUMPAD_ENTER){used=true;if(ac==AKEY_EVENT_ACTION_DOWN){if(gNameOpen)nameConfirm();else sizeApply();}}
  else if(kc==AKEYCODE_BACK){used=true;if(ac==AKEY_EVENT_ACTION_DOWN){gNameOpen=false;gSizeOpen=false;hideKb();}}
  else if(ac==AKEY_EVENT_ACTION_DOWN){int cp=jniUnicode(kc,e->meta);if(cp>0){used=true;if(gNameOpen)nameAddCp(cp);else sizeAddDigit(cp);}}
  return used?1:0;}
 if(e->type!=2)return 0;
 int act=e->action&AMOTION_EVENT_ACTION_MASK,n=(int)e->n;
 float x=e->x[0],y=e->y[0];
 if(act==AMOTION_EVENT_ACTION_DOWN){
  moved=false;rl=true;sx=x;sy=y;pinch=0;gDrag=false;mode=-1;
  if(gImportOpen&&!gNameOpen&&!gSizeOpen&&!gFiles.empty()&&inR(gListR,x,y)){mode=30;gSY0=y;gSS0=gScrollPx;}
  for(auto&b:gBtn)if(mode<0&&inR(b.r,x,y)&&(!(gNameOpen||gSizeOpen)||b.id>=2000)){if(b.id>=300&&b.id<=306){setSlider(b.id-300,x);mode=6+b.id-300;break;}if(b.id>=200||b.id<100)press(b.id);else if(b.id-100<(int)objs.size()){if(parentMode)finishParent(b.id-100);else sel=b.id-100;}mode=2;break;}
  if((gVid.on||gNameOpen||gSizeOpen)&&mode<0)mode=2;
  if(mode<0&&gImportOpen){gImportOpen=false;mode=2;}
  if(mode<0&&gFileMenu){gFileMenu=false;mode=2;}
  if(mode<0&&(gToolFan||gAddFan)){gToolFan=gAddFan=false;mode=2;}
  if(mode<0){
   if(gTlOpen&&tlHit(x,y,mode)){}
   else if(inR(gTrack,x,y)){mode=3;scrub(x);}
   
   else{bool blk=false;for(auto&r:gBlock)if(inR(r,x,y))blk=true;
    if(blk)mode=2;else if(gSculpt){mode=sculptBegin(x,y)?40:2;}else{if(gzProbe(x,y)){gDrag=true;mode=5;gzDown();}else mode=1;}}}
 }else if(act==AMOTION_EVENT_ACTION_POINTER_DOWN){if(mode==40){sculptEnd();mode=1;}moved=true;rl=true;pinch=0;}
 else if(act==AMOTION_EVENT_ACTION_POINTER_UP){rl=true;pinch=0;}
 else if(act==AMOTION_EVENT_ACTION_MOVE){
  if(mode==40)sculptMove(x,y);
  else if(mode==30){float dyy=y-gSY0;if(fabsf(dyy)>12)moved=true;gScrollPx=std::max(0.f,gSS0-dyy);}
  else if(mode==3)scrub(x);
  else if(mode>=20){if(hypotf(x-sx,y-sy)>20)moved=true;tlDrag(mode,x);}
  else if(mode>=6)setSlider(mode-6,x);
  else if(mode==1&&n>=2){float d=hypotf(e->x[1]-x,e->y[1]-y);
   float mx=(x+e->x[1])/2,my=(y+e->y[1])/2;if(pinch>0&&d>1){cd=std::clamp(cd*pinch/d,.04f,40.f);panCam(mx-lmx,my-lmy);}pinch=d;lmx=mx;lmy=my;moved=true;}
  else if(mode==1||mode==4||mode==5){
   if(rl){rl=false;lx=x;ly=y;}
   else{float dx=x-lx,dy=y-ly;lx=x;ly=y;if(hypotf(x-sx,y-sy)>20)moved=true;
    if(mode==5)gzMove(x,y);else if(moved){orbit(dx,dy);}}}
 }else if(act==AMOTION_EVENT_ACTION_UP||act==AMOTION_EVENT_ACTION_CANCEL){
  if(mode==40)sculptEnd();
  if(mode==30&&!moved&&gImportOpen){int k=(int)floorf((y-gListR.y+gScrollPx)/gRowH);if(k>=0&&k<(int)gFiles.size()){gImportOpen=false;pickPath(gFiles[k]);}}
  if(mode==1&&!moved)pick(x,y);if(mode==4&&!moved)snap(x,y);if(mode==27&&!moved)frameTap(x);if(mode==5){gzUp();if(gGzEdited&&gAutoKey&&sel>=0&&sel<(int)objs.size())insertKey();gGzEdited=false;}undoCommit();mode=0;gDrag=false;}
 return 1;}

// ---- API publica (implementada dentro do namespace para manter os mesmos nomes do motor) ----
static ShimVM gShimVM;
static ShimActivity gShimAct;
static android_app gShimApp;
static std::string gDirStore;
static double gLastT = 0;
static bool gFirstInit = true;
static double engNow() { timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

void api_set_android(JavaVM *vm, jobject activity) {
    gShimVM.vm = vm;
    gShimAct.vm = &gShimVM;
    gShimAct.clazz = activity;
    gShimApp.activity = &gShimAct;
    gApp = &gShimApp;
}

bool api_ready() { return ready; }

void api_init(const char *dir, int w, int h) {
    if (gFirstInit) {  // so na primeira vez: o projeto na memoria sobrevive a perda de contexto GL
        gDirStore = (dir && *dir) ? dir : ".";
        gDir = gDirStore.c_str();
        gProj = uniqueProjName(kUntitled);
        if (objs.empty()) {   // cena inicial como no Blender: cubo e camera
            addObj(0); snprintf(objs.back().nm, 24, "Cube"); objs.back().cur.p = {0, .5f, 0};
            addCameraAt({7.36f, 4.96f, 6.93f}, {0, .5f, 0}); sel = 0;
        }
        undoClear();
        gFirstInit = false;
    }
    W = w; H = h;
    gBgP = 0;
    initGL();
    gzInit();
    ready = true;
    gLastT = engNow();
}

void api_resize(int w, int h) {
    if (!ready || (w == W && h == H)) return;
    W = w; H = h;
    initPost();
}

static bool animRange(float &t0, float &t1);   // definida mais abaixo
void api_timeline(EngTimeline &t) {
    t.frame = tm * 24.f; t.fs = gFS; t.fe = gFE; t.vs = gVS; t.ve = gVE;
    t.playing = playing; t.dir = gPlayDir; t.visible = !gSculpt;
    t.hasSel = sel >= 0 && sel < (int)objs.size();
    t.tool = tool; t.canUndo = gCanUndo; t.canRedo = gCanRedo; t.sculpt = gSculpt; t.autoKey = gAutoKey;
    {   // tela de render: keyframes da cena, andamento, previa
        float kmin = 0.f, kmax = 0.f;
        t.anyKeys = animRange(kmin, kmax);   // so conta keys que realmente mudam alguma coisa
        t.keyFirst = t.anyKeys ? (int)roundf(kmin * 24.f) : 0;
        t.keyLast = t.anyKeys ? (int)roundf(kmax * 24.f) : 0;
        t.rendering = gVid.on || gAutoPng > 0;
        t.progress = gVid.on ? (float)gVid.done / std::max(gVid.total, 1) : (gAutoPng > 0 ? std::min(1.f, (float)gMcSamples / std::max(gMcSpp, 1)) : 0.f);
        t.openRender = gRenderReq; gRenderReq = false;
        t.previewSeq = gPrevSeq; t.previewPath = gPrevPath;
    }
    t.rows.clear();
    t.selObj = sel;
    for (auto &o : objs) {
        EngRow r;
        r.name = o.nm;
        r.type = o.kind == 3 ? 1 : (o.kind == 4 ? 2 : (o.mesh == 5 ? 3 : 0));
        r.parent = o.parent;
        r.hide = o.hide;
        r.nor = o.nor;
        t.rows.push_back(std::move(r));
    }
    t.keys.clear();
    if (t.hasSel) for (auto &k : objs[sel].k) t.keys.push_back(k.t * 24.f);
}

static bool tDiff(const T &a, const T &b) {
    auto d = [](V x, V y) { return fabsf(x.x - y.x) + fabsf(x.y - y.y) + fabsf(x.z - y.z); };
    return d(a.p, b.p) > 1e-4f || d(a.r, b.r) > 1e-4f || d(a.s, b.s) > 1e-4f;
}

// Intervalo animado da cena (em segundos): do key anterior a primeira mudanca ate o ultimo key que muda algo.
static bool animRange(float &t0, float &t1) {
    bool any = false;
    t0 = 1e9f; t1 = -1e9f;
    for (auto &o : objs) {
        if (o.k.size() < 2) continue;
        std::vector<Key> k = o.k;
        std::sort(k.begin(), k.end(), [](const Key &a, const Key &b) { return a.t < b.t; });
        int first = -1, last = -1;
        for (size_t i = 1; i < k.size(); i++)
            if (tDiff(k[i].x, k[i - 1].x)) { if (first < 0) first = (int)i - 1; last = (int)i; }
        if (first >= 0) { any = true; t0 = std::min(t0, k[first].t); t1 = std::max(t1, k[last].t); }
    }
    return any;
}

// Previa: copia o que o viewport mostra agora (mesma camera), recortado na proporcao do render.
static void capturePreview(int rw, int rh) {
    if (W < 8 || H < 8 || rw < 1 || rh < 1) return;
    std::vector<unsigned char> px((size_t)W * H * 4);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    float ar = (float)rw / rh;
    int cw = std::min(W, (int)(H * ar)), ch = std::min(H, (int)(W / ar));
    int cx = (W - cw) / 2, cy = (H - ch) / 2;
    int ow = std::min(640, cw), oh = std::max(1, (int)(ow / ar));
    std::vector<unsigned char> out((size_t)ow * oh * 4);
    for (int y = 0; y < oh; y++)
        for (int x = 0; x < ow; x++) {
            int ix = std::clamp((int)(cx + (x + .5f) * cw / ow), 0, W - 1);
            int iy = std::clamp((int)(cy + (oh - 1 - y + .5f) * ch / oh), 0, H - 1);
            int x2 = std::min(ix + 1, W - 1), y2 = std::min(iy + 1, H - 1);
            for (int c = 0; c < 3; c++) {
                int v = px[((size_t)iy * W + ix) * 4 + c] + px[((size_t)iy * W + x2) * 4 + c] + px[((size_t)y2 * W + ix) * 4 + c] + px[((size_t)y2 * W + x2) * 4 + c];
                out[((size_t)y * ow + x) * 4 + c] = (unsigned char)(v / 4);
            }
            out[((size_t)y * ow + x) * 4 + 3] = 255;
        }
    gPrevPath = std::string(gDir) + "/preview.png";
    FILE *f = fopen(gPrevPath.c_str(), "wb");
    if (!f) return;
    stbi_write_png_to_func(mcWrite, f, ow, oh, 4, out.data(), ow * 4);
    fclose(f);
    gPrevSeq++;
}

static void startImage(int mode) {
    if (!gMcOK) { toast("Render precisa de ES 3.1"); return; }
    gAutoPng = mode; gMcFinal = true; gMcPrev = false; gMcHaveCam = false; gSigS = -1;
}

void api_cmd(int c, float a, float b) {
    switch (c) {
    case 0: tm = gFS / 24.f; applyAnim(); break;
    case 1: playing = true; gPlayDir = -1; break;
    case 2: playing = false; break;
    case 3: playing = true; gPlayDir = 1; break;
    case 4: tm = gFE / 24.f; applyAnim(); break;
    case 5: jumpKey(-1); break;
    case 6: jumpKey(1); break;
    case 7: insertKey(); break;
    case 8: delKey(); break;
    case 9: { int f = std::clamp((int)roundf(a), gFS, gFE); tm = f / 24.f; applyAnim(); break; }
    case 10: { int e = std::clamp((int)roundf(b), gFS + 5, gFE); int st = std::clamp((int)roundf(a), gFS, e - 5); gVS = st; gVE = e; break; }
    case 11: press((int)roundf(a)); break;
    case 12: gMcCW = std::clamp((int)roundf(a), 64, 4096); gMcCH = std::clamp((int)roundf(b), 64, 4096); gMcHaveCam = false; gSigS = -1; break;
    case 13: gVidFps = std::clamp((int)roundf(a), 1, 120); break;
    case 14: gRA = (int)roundf(a); gRB = (int)roundf(b); break;
    case 15: gMcNight = a > .5f; gMcHaveCam = false; gSigS = -1; break;
    case 16: gMcLeak = a > .5f; gMcHaveCam = false; break;
    case 18: startImage((int)roundf(a)); break;
    case 19: if (!gVid.on) vidStart(); break;
    case 20: if (gVid.on) vidFinish(true); gAutoPng = 0; gMcFinal = false; gMcHaveCam = false; break;
    case 23: { int i = (int)roundf(a); if (i >= 0 && i < (int)objs.size()) sel = i; break; }
    case 24: {   // parentesco: a = filho, b = pai (b < 0 remove)
        int c2 = (int)roundf(a), p2 = (int)roundf(b);
        if (c2 >= 0 && c2 < (int)objs.size()) {
            if (p2 < 0 || p2 >= (int)objs.size()) { if (objs[c2].parent >= 0) { clearParent(c2); toast("Parentesco removido"); undoCommit(); } }
            else if (c2 != p2 && objs[c2].parent != p2) { setParent(c2, p2); undoCommit(); }
        }
        break; }
    case 25: { int i = (int)roundf(a); if (i >= 0 && i < (int)objs.size()) { objs[i].hide = !objs[i].hide; if (sel == i && objs[i].hide) sel = -1; undoCommit(); } break; }
    case 26: { int i = (int)roundf(a); if (i >= 0 && i < (int)objs.size()) { objs[i].nor = !objs[i].nor; undoCommit(); } break; }
    case 22: gAutoKey = !gAutoKey; toast(gAutoKey ? "Auto Key ligado" : "Auto Key desligado"); break;
    case 21: gCapReq = true; gCapW = std::clamp((int)roundf(a), 16, 8192); gCapH = std::clamp((int)roundf(b), 16, 8192); break;
    }
}

void api_event(const EngEvent &e) {
    switch (e.kind) {
    case 0: {
        ShimEvt s;
        s.type = 2; s.action = e.action; s.n = e.n;
        for (int i = 0; i < 2; i++) { s.x[i] = e.x[i]; s.y[i] = e.y[i]; }
        onInput(&s);
        break;
    }
    case 1: if (gNameOpen) nameAddCp(e.cp); else if (gSizeOpen) sizeAddDigit(e.cp); break;
    case 2: if (gNameOpen) nameBack(); else if (gSizeOpen) sizeBack(); break;
    case 3: if (gNameOpen) nameConfirm(); else if (gSizeOpen) sizeApply(); break;
    case 4: if (gNameOpen || gSizeOpen) { gNameOpen = false; gSizeOpen = false; hideKb(); } break;
    case 5: api_cmd(e.action, e.x[0], e.x[1]); break;
    }
}

void api_frame() {
    if (!ready) return;
    // estado GL que o motor so configurava uma vez (o Qt mexe nele entre quadros)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindVertexArray(0);
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDisable(GL_SCISSOR_TEST); glDisable(GL_STENCIL_TEST);
    glDepthMask(GL_TRUE); glDepthFunc(GL_LESS); glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_BLEND); glBlendEquation(GL_FUNC_ADD); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(gProg);
    gCapNow = gCapReq && !gVid.on && gAutoPng == 0;   // este quadro sai sem overlays do viewport
    bool matcapWas = gMatcap;
    if (gCapNow) gMatcap = false;   // a previa usa materiais e texturas (PBR), nao o visual cinza do viewport
    double t = engNow(); float dt = (float)(t - gLastT); gLastT = t;
    gDt = dt; gToastT -= dt; gFAcc += dt; gFN++;
    if (gFAcc >= .5f) { gFps = gFN / gFAcc; gFAcc = 0; gFN = 0; }
    for (float &fl : gFlash) if (fl > 0) fl -= dt;
    if (playing) {
        tm += dt * gPlayDir; float a_ = gFS / 24.f, b_ = gFE / 24.f;
        if (tm > b_) tm = a_;
        if (tm < a_) tm = b_;
        applyAnim();
    }
    frame();
    if (gCapNow) { gCapNow = false; gCapReq = false; capturePreview(gCapW, gCapH); }
    gMatcap = matcapWas;
    if (gAutoPng && gMcFinal && gMcSamples >= gMcSpp && !gDnDirty && mcShow()) {   // imagem pronta
        int mode = gAutoPng; gAutoPng = 0;
        gPrevPath = std::string(gDir) + "/preview.png";
        mcSaveTo(gPrevPath); gPrevSeq++;
        if (mode == 2) { if (!filesGranted()) needFiles(); else mcSave(); }
        gMcFinal = false; gMcHaveCam = false;
    }
    // O motor desenha direto na tela e deixa profundidade/stencil preenchidos: limpa para o Qt Quick
    // desenhar a interface (timeline, botoes) por cima, sem o 3D "vencer" no teste de profundidade.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_SCISSOR_TEST);
    glDepthMask(GL_TRUE);
    glClearDepthf(1.f);
    glClearStencil(0);
    glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

void api_frame_ui() {
    if (!ready) return;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, W, H);
    glBindVertexArray(0);
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDisable(GL_SCISSOR_TEST); glDisable(GL_STENCIL_TEST);
    glDepthMask(GL_TRUE); glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_BLEND); glBlendEquation(GL_FUNC_ADD); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(gProg);
    if (gzActive()) {   // gizmos do ImGuizmo, por baixo dos paineis do motor
        gzRun(true);
        glBindFramebuffer(GL_FRAMEBUFFER, 0); glViewport(0, 0, W, H); glBindVertexArray(0);
        glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDisable(GL_SCISSOR_TEST); glDisable(GL_STENCIL_TEST);
        glActiveTexture(GL_TEXTURE0);
        glEnable(GL_BLEND); glBlendEquation(GL_FUNC_ADD); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(gProg);
    }
    frameUI();
}

void api_ui_rects(std::vector<float> &o) {
    o.clear();
    for (auto &r : gBlock) { o.push_back(r.x); o.push_back(r.y); o.push_back(r.w); o.push_back(r.h); }
    for (auto &b : gBtn) { o.push_back(b.r.x); o.push_back(b.r.y); o.push_back(b.r.w); o.push_back(b.r.h); }
}

void api_gl_lost() {
    if (gVid.on) vidFinish(true);
    ready = false;  // proximo quadro recria os recursos GL; cena e projeto continuam na memoria
}

bool api_dialog_open() { return gNameOpen || gSizeOpen; }

}  // namespace eng

// ======================= funcoes de engine_api.h =======================
void eng_set_android(JavaVM *vm, jobject activity) { eng::api_set_android(vm, activity); }
bool eng_ready() { return eng::api_ready(); }
void eng_init(const char *dir, int w, int h) { eng::api_init(dir, w, h); }
void eng_resize(int w, int h) { eng::api_resize(w, h); }
void eng_event(const EngEvent &e) { eng::api_event(e); }
void eng_frame() { eng::api_frame(); }
void eng_gl_lost() { eng::api_gl_lost(); }
bool eng_dialog_open() { return eng::api_dialog_open(); }
void eng_timeline(EngTimeline &t) { eng::api_timeline(t); }
void eng_frame_ui() { eng::api_frame_ui(); }
void eng_ui_rects(std::vector<float> &o) { eng::api_ui_rects(o); }

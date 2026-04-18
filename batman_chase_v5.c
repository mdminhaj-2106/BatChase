/*
 * batman_chase_v5.c — Batman: Gotham Chase
 * DE1-SoC / CPUlator pseudo-3D racer
 *
 * KEY FIX: Car moves on screen. Road stays centered.
 *   - Road center = SCX + curve offset (no player shift)
 *   - Car drawn at SCX + playerX * road_hw / FP
 *   - Enemies drawn relative to road center
 *   - Z-sorted far-to-near (no flicker)
 *   - Road stripe scroll tied to speed
 *   - Software FB + single blit = zero flicker
 *
 * W/Up=accel S/Down=brake A/Left=steer left D/Right=steer right
 * SHIFT=boost ENTER=start ESC=pause
 */
#include <stdbool.h>

/* Hardware */
#define PIXCTRL 0xFF203020u
#define PIXBUF  0xC8000000u
#define PS2ADR  0xFF200100u
#define TMRADR  0xFF202000u

/* Display */
#define SW  320
#define SH  240
#define STR 512
#define SCX (SW/2)

/* Projection — proven from v3/v4 */
#define HOR   76
#define PROJ  512
#define RHW   400

/* Player — FP=256, playerX range -256..+256 = -1.0..+1.0 */
#define FP   256
#define MXVL 128
#define BSVL 192
#define ACCL 4
#define BRKF 10
#define FRIC 1
#define STRF 8
#define MXST 28
#define BSDR 150

/* Gameplay */
#define NEMAX  12
#define SPWNZ  260
#define SPWNT  70
#define COLLZ  8
#define COLLX  55
#define PLHP   5
#define INVT   90

/* Colours */
#define C_K    0x0000u
#define C_W    0xFFFFu
#define C_R    0xF800u
#define C_O    0xFD20u
#define C_Y    0xFFE0u
#define C_C    0x07FFu
#define C_B    0x001Fu
#define C_G    0x07E0u
#define C_M    0xF81Fu
#define C_DK   0x4208u
#define C_MG   0x8410u
#define C_LG   0xCE59u
#define C_HOR  0x18A8u
#define C_GRF  0x0160u
#define C_GRN  0x0280u
#define C_RMA  0xF800u
#define C_RMB  0xFFFFu
#define C_SWK  0x4A09u
#define C_BD1  0x0841u
#define C_BD3  0x294Au
#define C_BD4  0x3186u
#define C_NP   0xF81Fu
#define C_NT   0x07EFu
#define C_BB0  0x0841u
#define C_BB1  0x1082u
#define C_BB2  0x18C3u
#define C_BCK  0x0019u
#define C_BED  0x294Au
#define C_BAC  0xFFE0u
#define C_SHA  0x0841u

/* Sine LUT (64 entries) */
static const short SIN64[64]={
  0,25,50,75,100,125,150,175,198,222,245,267,289,310,330,350,
  368,385,401,416,429,441,452,462,471,478,484,489,492,494,495,495,
  494,492,489,484,478,471,462,452,441,429,416,401,385,368,350,330,
  310,289,267,245,222,198,175,150,125,100,75,50,25,0,-25,-50
};
static int isin(int a){
  a&=255;
  if(a<64)return SIN64[a];
  if(a<128)return SIN64[127-a];
  if(a<192)return -SIN64[a-128];
  return -SIN64[255-a];
}

/* Types */
typedef struct{
  int x_fp, stv, vel, hp, score, invuln, bst;
  bool alive, boosting;
} Plr;
typedef struct{
  bool active;
  int x_fp, wz, spd, type, anim;
} Enm;
typedef enum{ST_MENU,ST_PLAY,ST_PAUSE,ST_OVER}GSt;
typedef struct{
  GSt st;
  int fr, scroll, curve, curve_t, spawn_t;
} Gm;

/* Globals */
static volatile unsigned int*const g_ps2=(volatile unsigned int*)PS2ADR;
static volatile unsigned int*const g_tmr=(volatile unsigned int*)TMRADR;
static volatile unsigned int*const g_pbc=(volatile unsigned int*)PIXCTRL;
static unsigned short fb[SH*STR];
static Plr pl;
static Enm en[NEMAX];
static Gm gm;
static bool kU,kD,kL,kR,kSh,kEnt,kEsc;
static unsigned int rng=0xACE1u;

/* Math */
static int iabs(int v){return v<0?-v:v;}
static int iclamp(int v,int lo,int hi){return v<lo?lo:v>hi?hi:v;}
static int imax(int a,int b){return a>b?a:b;}
static int irand(int lo,int hi){
  rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;
  if(hi<=lo)return lo;
  return lo+(int)(rng&0x7FFFu)%(hi-lo+1);
}

/* Framebuffer primitives */
static inline void putpx(int x,int y,unsigned short c){
  if((unsigned)x<SW&&(unsigned)y<(unsigned)SH)fb[y*STR+x]=c;
}
static void hline(int x,int y,int l,unsigned short c){
  if((unsigned)y>=SH||l<=0)return;
  int x0=x,x1=x+l-1;
  if(x1<0||x0>=SW)return;
  if(x0<0)x0=0;
  if(x1>=SW)x1=SW-1;
  for(int i=x0;i<=x1;i++)fb[y*STR+i]=c;
}
static void fillr(int x,int y,int w,int h,unsigned short c){
  if(w<=0||h<=0)return;
  int x0=x,y0=y,x1=x+w,y1=y+h;
  if(x1<=0||y1<=0||x0>=SW||y0>=SH)return;
  if(x0<0)x0=0;
  if(y0<0)y0=0;
  if(x1>SW)x1=SW;
  if(y1>SH)y1=SH;
  for(int j=y0;j<y1;j++)
    for(int i=x0;i<x1;i++)
      fb[j*STR+i]=c;
}
static void bordr(int x,int y,int w,int h,unsigned short c){
  hline(x,y,w,c);hline(x,y+h-1,w,c);
  for(int j=y;j<y+h;j++){putpx(x,j,c);putpx(x+w-1,j,c);}
}
static void vline(int x,int y,int l,unsigned short c){
  for(int j=0;j<l;j++)putpx(x,y+j,c);
}
static void diamond(int cx,int cy,int r,unsigned short c){
  for(int dy=-r;dy<=r;dy++){int hw=r-iabs(dy);hline(cx-hw,cy+dy,hw*2+1,c);}
}
static void clrfb(unsigned short c){
  unsigned int pk=((unsigned int)c<<16)|c;
  unsigned int*p=(unsigned int*)fb;
  int n=(STR*SH)>>1;
  for(int i=0;i<n;i++)p[i]=pk;
}

/* 5x7 digit font */
static const unsigned char DFONT[10][7]={
  {0x1C,0x14,0x14,0x14,0x14,0x14,0x1C},{0x04,0x04,0x04,0x04,0x04,0x04,0x04},
  {0x1C,0x04,0x04,0x1C,0x10,0x10,0x1C},{0x1C,0x04,0x04,0x1C,0x04,0x04,0x1C},
  {0x14,0x14,0x14,0x1C,0x04,0x04,0x04},{0x1C,0x10,0x10,0x1C,0x04,0x04,0x1C},
  {0x1C,0x10,0x10,0x1C,0x14,0x14,0x1C},{0x1C,0x04,0x04,0x04,0x04,0x04,0x04},
  {0x1C,0x14,0x14,0x1C,0x14,0x14,0x1C},{0x1C,0x14,0x14,0x1C,0x04,0x04,0x1C},
};
static void drawdig(int x,int y,int d,unsigned short c){
  if(d<0||d>9)return;
  for(int r=0;r<7;r++){
    unsigned char b=DFONT[d][r];
    for(int col=0;col<5;col++)
      if(b&(0x10>>col))putpx(x+col,y+r,c);
  }
}
static void drawnum(int x,int y,int n,unsigned short c){
  if(n<0)n=0;
  if(n==0){drawdig(x,y,0,c);return;}
  char buf[10];int len=0;
  while(n>0){buf[len++]=(char)(n%10);n/=10;}
  for(int i=len-1;i>=0;i--){drawdig(x,y,(int)buf[i],c);x+=6;}
}

/* Hardware */
static void blit(void){
  volatile unsigned int*d=(volatile unsigned int*)PIXBUF;
  unsigned int*s=(unsigned int*)fb;
  int n=(STR*SH)>>1;
  for(int i=0;i<n;i++)d[i]=s[i];
}
static void hw_init(void){g_pbc[1]=PIXBUF;clrfb(0);blit();}
static void tmr_init(void){g_tmr[2]=0xB4B3u;g_tmr[3]=0x000Cu;g_tmr[1]=0x0006u;}
static void tmr_wait(void){while(!(g_tmr[0]&1u));g_tmr[0]=0;}

/* PS/2 input */
static void poll_input(void){
  static bool ext=false,rel=false;
  while(1){
    unsigned int d=*g_ps2;
    if(!(d&0x8000u))break;
    unsigned int b=d&0xFFu;
    if(b==0xE0u){ext=true;continue;}
    if(b==0xF0u){rel=true;continue;}
    bool p=!rel;rel=false;
    if(ext){
      if(b==0x75u)kU=p;
      else if(b==0x72u)kD=p;
      else if(b==0x6Bu)kL=p;
      else if(b==0x74u)kR=p;
      ext=false;continue;
    }
    if(b==0x1Du)kU=p;
    else if(b==0x1Bu)kD=p;
    else if(b==0x1Cu)kL=p;
    else if(b==0x23u)kR=p;
    else if(b==0x12u||b==0x59u)kSh=p;
    else if(b==0x5Au)kEnt=p;
    else if(b==0x76u)kEsc=p;
  }
}

/* ================================================================
 * SKY, BUILDINGS, RAIN
 * ================================================================ */
static void draw_sky(void){
  for(int y=0;y<=HOR+2;y++){
    unsigned short c;
    if(y<=HOR/2){int b=1+(y>>2);c=(unsigned short)b;}
    else{int b=3+((y-HOR/2)>>2);int g=1+((y-HOR/2)>>3);c=(unsigned short)((g<<5)|b);}
    hline(0,y,SW,c);
  }
  hline(0,HOR,SW,C_HOR);hline(0,HOR+1,SW,0x1082u);
  /* Bat-Signal */
  int bx=264,by=26;
  for(int y=6;y<HOR;y++){int hw=2+(y-6)*3/(HOR-6);hline(bx-hw,y,hw*2,0x0103u);}
  diamond(bx,by,12,0x3180u);diamond(bx,by,10,0x8C00u);diamond(bx,by,8,C_Y);
  fillr(bx-5,by-3,10,8,0x0001u);fillr(bx-9,by-1,5,4,0x0001u);
  fillr(bx+4,by-1,5,4,0x0001u);fillr(bx-3,by-6,2,4,0x0001u);fillr(bx+1,by-6,2,4,0x0001u);
}
static void draw_buildings(void){
  int scr=gm.scroll;
  /* Far layer */
  {int off=(scr/4)%320;unsigned int s=0xCAFE0000u;
   for(int b=0;b<18;b++){s=s*1664525u+1013904223u;
     int bw=10+(int)((s>>28)&0xF)*2;int bh=7+(int)((s>>24)&0xF);
     int bx=((int)((s>>8)&0xFF)%320-off+640)%320;
     int by=HOR+5-bh;fillr(bx,by,bw,bh,C_BD1);
     if((s>>3)&1)putpx(bx+bw/2,by+2,((s>>2)&1)?0x0040u:0x0003u);}}
  /* Mid layer */
  {int off=(scr/2)%320;unsigned int s=0xDEADBEEFu;
   for(int b=0;b<12;b++){s=s*1664525u+1013904223u;
     int bw=14+(int)((s>>28)&0xF)*3;int bh=12+(int)((s>>22)&0x1F);
     int bx=((int)((s>>8)&0x1FF)%320-off+960)%320;
     int by=HOR+5-bh;if(by<2)by=2;
     fillr(bx,by,bw,bh,C_BD3);bordr(bx,by,bw,bh,0x31A6u);
     fillr(bx+2,by-2,bw-4,3,C_BD3);
     unsigned short nc=((s>>1)&1)?C_NP:C_NT;
     fillr(bx+2,by+bh-4,bw-4,2,nc);
     for(int wi=0;wi<(bw-4)/5&&wi<3;wi++)
       for(int wj=0;wj<2;wj++)
         if((s>>(wi+wj*3))&1)fillr(bx+3+wi*5,by+4+wj*5,3,3,C_Y);}}
  /* Near right */
  {int off=(scr*3/4)%200;unsigned int s=0xABCD1234u;
   for(int b=0;b<5;b++){s=s*1664525u+1013904223u;
     int bw=20+(int)((s>>28)&0xF)*4;int bh=24+(int)((s>>22)&0x1F)*2;
     int bx=225+(int)((s>>8)&0x3F)%(SW-225-bw+1);
     if(bx+bw>SW)bx=SW-bw;
     int by=HOR+5-bh;if(by<2)by=2;
     int vo=((b*47+off*2)%(bh+40))-20;by+=vo;if(by<2)by=2;
     fillr(bx,by,bw,bh,C_BD4);bordr(bx,by,bw,bh,C_MG);
     unsigned short nc=((b^(gm.fr/60))&1)?C_NP:C_O;
     fillr(bx+3,by+3,bw-6,5,C_K);
     for(int si=0;si<(bw-8)/4;si++)fillr(bx+4+si*4,by+4,2,3,nc);}}
  /* Near left */
  {int off=(scr*3/4+100)%200;unsigned int s=0x5A5ABEADu;
   for(int b=0;b<5;b++){s=s*1664525u+1013904223u;
     int bw=20+(int)((s>>28)&0xF)*4;int bh=24+(int)((s>>22)&0x1F)*2;
     int bx2=(int)((s>>8)&0x3F)%imax(95-bw,1);
     if(bx2<0)bx2=0;
     if(bx2+bw>95)bx2=95-bw;
     int by=HOR+5-bh;if(by<2)by=2;
     int vo=((b*53+off*2)%(bh+40))-20;by+=vo;if(by<2)by=2;
     fillr(bx2,by,bw,bh,C_BD4);bordr(bx2,by,bw,bh,C_MG);
     unsigned short nc=((b^(gm.fr/60+1))&1)?C_O:C_C;
     fillr(bx2+3,by+3,bw-6,5,C_K);
     for(int si=0;si<(bw-8)/4;si++)fillr(bx2+4+si*4,by+4,2,3,nc);}}
}
static void draw_rain(void){
  for(int i=0;i<60;i++){
    int rx=(i*73+gm.fr*2)%SW;int ry=(i*53+gm.fr*5)%SH;
    putpx(rx,ry,0x4C99u);putpx(rx,ry+1,0x4C99u);
  }
}

/* ================================================================
 * ROAD — scanline renderer
 *
 * Road center stays at SCX + curve_offset — it does NOT shift
 * with playerX. This means steering moves the CAR, not the road.
 *
 * Stripes scroll with gm.scroll (tied to speed).
 * ================================================================ */
static void draw_road(void){
  for(int y=HOR+1;y<SH;y++){
    int dy=y-HOR;
    int hw=(RHW*dy)/PROJ;              /* road half-width in px */
    /* Curve shifts road center (NOT player — player moves independently) */
    int cvoff=(gm.curve*dy*dy)/(PROJ*64);
    int cx=SCX+cvoff;                  /* road center on screen */

    /* Stripe pattern based on scroll */
    int zDepth=(PROJ*16)/(dy+1);       /* virtual z for stripe calc */
    int stripe=((gm.scroll+zDepth)>>5)&1;

    /* Depth-shaded road colour */
    int t=(dy*12)/(SH-HOR);
    unsigned short road_c=(unsigned short)(((4+t)<<11)|((9+t)<<5)|(4+t));
    if(stripe) road_c=(unsigned short)(road_c+0x0842u);

    unsigned short kc=stripe?C_RMA:C_RMB;
    unsigned short grass=dy<60?C_GRF:C_GRN;
    int kw=imax(dy/20,1);
    int rl=cx-hw, rr=cx+hw;

    /* Grass (full line first) */
    hline(0,y,SW,grass);
    /* Sidewalk */
    if(dy>30){int sw=imax(hw/16,1);hline(rl-sw,y,sw,C_SWK);hline(rr,y,sw,C_SWK);}
    /* Rumble strips */
    hline(rl,y,kw,kc);
    hline(rr-kw,y,kw,kc);
    /* Road surface */
    if(rr-rl-kw*2>0) hline(rl+kw,y,rr-rl-kw*2,road_c);
    /* Lane markers */
    if(!stripe||dy<20){
      for(int ln=1;ln<3;ln++){
        int lx=cx-hw+ln*(2*hw/3);
        if((unsigned)lx<(unsigned)SW)putpx(lx,y,C_W);
        if(dy>80&&(unsigned)(lx+1)<(unsigned)SW)putpx(lx+1,y,C_W);
      }
    }
    /* Centre line */
    if((unsigned)cx<(unsigned)SW)putpx(cx,y,C_Y);
    if((unsigned)(cx+1)<(unsigned)SW)putpx(cx+1,y,C_Y);
  }
}

/* ================================================================
 * STREET LAMPS
 * ================================================================ */
static void draw_lamps(void){
  int spacing=90;int off=gm.scroll%spacing;
  for(int yi=-spacing;yi<SH+spacing;yi+=spacing){
    int y=yi+off;if(y<=HOR||y>=SH)continue;
    int dy=y-HOR;
    int hw=(RHW*dy)/PROJ;
    int cvoff=(gm.curve*dy*dy)/(PROJ*64);
    int cx=SCX+cvoff;
    int ph=imax(dy*18/PROJ+2,3);int arm=imax(dy*10/PROJ+1,2);
    {int lx=cx-hw-imax(dy*8/PROJ,2);int ty=y-ph;if(ty<0)ty=0;
     vline(lx,ty,ph,C_LG);hline(lx,ty,arm,C_LG);diamond(lx+arm,ty,imax(dy*2/PROJ,1),C_Y);}
    {int rx=cx+hw+imax(dy*8/PROJ,2);int ty=y-ph;if(ty<0)ty=0;
     vline(rx,ty,ph,C_LG);hline(rx-arm,ty,arm,C_LG);diamond(rx-arm,ty,imax(dy*2/PROJ,1),C_Y);}
  }
}

/* ================================================================
 * BATMOBILE — drawn at CAR position (moves with playerX!)
 *
 * car_sx = road_center + playerX * road_hw_at_bottom / FP
 * This is the KEY difference: the car slides across the road.
 * ================================================================ */
static void draw_batmobile(void){
  if(!pl.alive)return;
  if(pl.invuln>0&&(gm.fr&3))return;

  int dy_bot=SH-1-HOR;
  int hw_bot=(RHW*dy_bot)/PROJ;
  int cvoff=(gm.curve*dy_bot*dy_bot)/(PROJ*64);
  int road_cx=SCX+cvoff;

  /* CAR position = road centre + player offset */
  int cx=road_cx+(pl.x_fp*hw_bot)/FP;
  /* Add visual lean from steering input */
  cx+=iclamp(pl.stv*2,-12,12);
  int cy=SH-38;

  /* Shadow */
  fillr(cx-20,cy+3,40,5,C_SHA);
  /* Exhaust */
  if(pl.boosting){int fh=8+((gm.fr>>1)&3);fillr(cx-5,cy+2,10,fh,C_O);fillr(cx-3,cy+3,6,fh-2,C_Y);}
  else if(gm.fr&8){fillr(cx-3,cy+2,6,5,C_O);}
  /* L0: Tail fins */
  fillr(cx-24,cy-4,14,10,C_BB0);hline(cx-24,cy-4,14,C_BED);
  fillr(cx+10,cy-4,14,10,C_BB0);hline(cx+10,cy-4,14,C_BED);
  fillr(cx-9,cy-4,18,10,C_BB1);diamond(cx,cy+1,3,C_DK);
  /* L1: Lower hull */
  fillr(cx-11,cy-14,22,11,C_BB0);vline(cx-11,cy-14,9,C_BED);vline(cx+10,cy-14,9,C_BED);
  /* L2: Mid body + slats */
  fillr(cx-10,cy-24,20,11,C_BB0);
  hline(cx-7,cy-23,14,C_BED);hline(cx-7,cy-21,14,C_BED);hline(cx-7,cy-19,14,C_BED);
  /* L3: Upper hood */
  fillr(cx-8,cy-32,16,9,C_BB1);
  /* L4: Nose */
  fillr(cx-5,cy-38,10,7,C_BB1);fillr(cx-3,cy-42,6,5,C_BB2);
  hline(cx-3,cy-41,6,C_BED);hline(cx-3,cy-39,6,C_BED);
  /* L5: Cockpit canopy */
  fillr(cx-7,cy-28,14,7,C_BCK);fillr(cx-5,cy-27,10,5,0x0017u);
  hline(cx-7,cy-28,14,C_BED);hline(cx-7,cy-22,14,0x3166u);
  /* L6: Bat-wing emblem */
  fillr(cx-1,cy-19,2,4,C_BAC);fillr(cx-8,cy-18,7,2,C_BAC);fillr(cx+1,cy-18,7,2,C_BAC);
  /* L7: Headlights + taillights */
  fillr(cx-6,cy-41,3,2,C_W);fillr(cx+3,cy-41,3,2,C_W);
  fillr(cx-12,cy-6,3,2,C_R);fillr(cx+9,cy-6,3,2,C_R);
}

/* ================================================================
 * ENEMY CAR — positioned relative to ROAD center
 * ================================================================ */
static void draw_enemy(const Enm*e){
  if(!e->active||e->wz<=0)return;
  int sy=HOR+PROJ/e->wz;
  if(sy<=HOR+2||sy>=SH)return;
  int dy=sy-HOR;
  int hw=(RHW*dy)/PROJ;
  int cvoff=(gm.curve*dy*dy)/(PROJ*64);
  int road_cx=SCX+cvoff;
  /* Enemy screen X: offset from road centre */
  int sx=road_cx+(e->x_fp*hw)/FP;
  if(sx<-60||sx>SW+60)return;

  int sc=(dy*100)/(SH-HOR);
  int bw=imax(52*sc/100,4);int bh=imax(72*sc/100,4);
  int bw2=bw/2;

  unsigned short bc,rc;
  switch(e->type){
    case 0:bc=C_W;rc=C_W;break;
    case 1:bc=C_O;rc=0xFC40u;break;
    case 2:bc=C_C;rc=0x06DFu;break;
    default:bc=C_M;rc=0x7800u;break;
  }
  /* Shadow */
  fillr(sx-bw2-2,sy,bw+4,imax(bh/5,1),C_SHA);
  /* Bumper */
  fillr(sx-bw2,sy-bh/4,bw,bh/4,bc);
  {int ww=imax(bw/5,1);fillr(sx-bw2,sy-bh/4,ww,bh/4,C_K);fillr(sx+bw2-ww,sy-bh/4,ww,bh/4,C_K);}
  /* Body */
  fillr(sx-bw2,sy-bh*3/4,bw,bh/2,bc);hline(sx-bw2,sy-bh/2,bw,rc);
  /* Glass */
  {int gw=bw*3/4;int gh=imax(bh/7,1);fillr(sx-gw/2,sy-bh*5/8,gw,gh,C_DK);}
  /* Roof */
  {int rw=bw*3/4;int rh=imax(bh/5,1);int ry=sy-bh-rh;
   fillr(sx-rw/2,ry,rw,rh,rc);hline(sx-rw/2,ry,rw,C_MG);}
  /* Headlights */
  if(bw>4){
    fillr(sx-bw2+1,sy-bh/5,imax(bw/5,1),imax(bh/8,1),C_W);
    fillr(sx+bw2-imax(bw/5,1)-1,sy-bh/5,imax(bw/5,1),imax(bh/8,1),C_W);}
  /* Police siren */
  if(e->type==0&&bw>6){
    int rw2=bw*3/4;int rh2=imax(bh/5,1);int ry2=sy-bh-rh2;
    int sw2=imax(rw2/2,2);unsigned short sl=(e->anim&8)?C_R:C_B;
    fillr(sx-sw2/2,ry2-imax(rh2/2,1),sw2/2,imax(rh2/2,1),sl);
    fillr(sx,ry2-imax(rh2/2,1),sw2/2,imax(rh2/2,1),sl==C_R?C_B:C_R);}
  /* Taillights */
  if((e->anim>>3)&1){fillr(sx-bw2,sy-2,imax(bw/6,1),2,C_R);
    fillr(sx+bw2-imax(bw/6,1),sy-2,imax(bw/6,1),2,C_R);}
}

/* ================================================================
 * HUD
 * ================================================================ */
static void draw_hud(void){
  fillr(0,0,90,48,0x1082u);bordr(0,0,90,48,C_DK);
  fillr(4,4,66,8,C_BD1);
  {int mxv=pl.boosting?BSVL:MXVL;int bar=iclamp(pl.vel*64/mxv,0,64);
   fillr(5,5,bar,6,pl.boosting?C_O:C_C);}
  drawnum(72,5,pl.vel,C_LG);
  for(int i=0;i<PLHP;i++){unsigned short hc=i<pl.hp?C_G:0x2104u;diamond(8+i*14,20,4,hc);}
  drawnum(4,34,pl.score,C_Y);
  if(pl.bst>0){int bw=(pl.bst*60)/BSDR;fillr(4,44,bw,3,C_O);}
  fillr(SW-20,0,20,50,0x1082u);bordr(SW-20,0,20,50,C_DK);
  {int bh=iclamp(pl.vel*44/MXVL,0,44);fillr(SW-16,48-bh,12,bh,pl.boosting?C_O:C_NT);}
}

/* ================================================================
 * OVERLAYS
 * ================================================================ */
static void dim_fb(void){
  int n=STR*SH;
  for(int i=0;i<n;i++){unsigned short p=fb[i];
    fb[i]=(unsigned short)(((p>>1)&0x7800u)|((p>>1)&0x03E0u)|((p>>1)&0x000Fu));}
}
static void draw_menu(void){
  clrfb(C_K);
  for(int y=0;y<SH;y++){unsigned short sky;
    if(y<80){int b=3+(y>>3);sky=(unsigned short)((y/16<<5)|b);}
    else if(y<160){int b=13+((y-80)>>4);int g=5+((y-80)>>4);sky=(unsigned short)((g<<5)|b);}
    else{int r=(y-160)>>3;int g=3+((y-160)>>4);sky=(unsigned short)((r<<11)|(g<<5)|8);}
    hline(0,y,SW,sky);}
  draw_sky();draw_rain();
  for(int b=0;b<12;b++){int bx=b*27;int bh=18+(b*7)%28;
    fillr(bx,68-bh,22,bh,C_BD3);fillr(bx+2,68-bh-3,4,4,C_BD3);fillr(bx+15,68-bh-3,4,4,C_BD3);}
  fillr(108,48,22,32,C_DK);fillr(190,48,22,32,C_DK);fillr(128,30,64,52,C_DK);
  fillr(140,18,12,18,C_DK);fillr(168,18,12,18,C_DK);fillr(148,42,24,22,0x2945u);
  fillr(148,60,24,5,C_Y);diamond(160,52,7,C_Y);
  fillr(54,88,212,30,C_Y);fillr(56,90,208,26,C_K);
  int tx=64;
  fillr(tx,92,4,22,C_Y);fillr(tx+4,92,8,6,C_Y);fillr(tx+4,100,8,6,C_Y);fillr(tx+4,108,8,7,C_Y);tx+=16;
  fillr(tx,96,4,18,C_Y);fillr(tx+8,96,4,18,C_Y);fillr(tx+4,92,4,5,C_Y);fillr(tx+4,101,4,4,C_Y);tx+=16;
  fillr(tx,92,20,6,C_Y);fillr(tx+8,92,4,22,C_Y);tx+=22;
  fillr(tx,92,4,22,C_Y);fillr(tx+14,92,4,22,C_Y);fillr(tx+4,94,5,7,C_Y);fillr(tx+9,94,5,7,C_Y);tx+=20;
  fillr(tx,96,4,18,C_Y);fillr(tx+8,96,4,18,C_Y);fillr(tx+4,92,4,5,C_Y);fillr(tx+4,101,4,4,C_Y);tx+=16;
  fillr(tx,92,4,22,C_Y);fillr(tx+14,92,4,22,C_Y);fillr(tx+4,94,6,5,C_Y);fillr(tx+9,100,6,5,C_Y);
  fillr(80,122,160,10,C_C);fillr(82,123,156,8,C_K);
  for(int i=0;i<8;i++)fillr(84+i*18,125,14,4,C_C);
  unsigned short bc=(gm.fr&32)?C_Y:C_O;
  fillr(78,160,164,16,bc);fillr(80,162,160,12,C_K);
  for(int i=0;i<6;i++)fillr(86+i*24,165,20,6,bc);
  drawnum(138,192,pl.score,C_Y);
}
static void draw_pause(void){dim_fb();
  fillr(80,90,160,60,C_DK);bordr(80,90,160,60,C_Y);
  fillr(120,108,12,22,C_Y);fillr(148,108,12,22,C_Y);fillr(100,136,120,7,C_MG);}
static void draw_gameover(void){dim_fb();
  fillr(50,70,220,100,C_K);bordr(50,70,220,100,C_R);fillr(70,85,180,7,C_R);
  drawnum(120,103,pl.score,C_Y);if(gm.fr&32)fillr(90,148,140,6,C_W);}

/* ================================================================
 * SPAWN / INIT
 * ================================================================ */
static void spawn_enemy(void){
  int lanes_fp[3]={-170,0,170};
  for(int i=0;i<NEMAX;i++){
    if(en[i].active)continue;
    en[i].active=true;
    en[i].x_fp=lanes_fp[irand(0,2)]+irand(-20,20);
    en[i].wz=SPWNZ+irand(0,60);
    en[i].spd=1+irand(0,2);
    en[i].type=irand(0,3);
    en[i].anim=0;
    return;
  }
}
static void game_init(void){
  pl.x_fp=0;pl.stv=0;pl.vel=0;pl.hp=PLHP;pl.score=0;
  pl.invuln=0;pl.bst=0;pl.alive=true;pl.boosting=false;
  for(int i=0;i<NEMAX;i++)en[i].active=false;
  gm.scroll=0;gm.fr=0;gm.curve=0;gm.curve_t=0;gm.spawn_t=SPWNT;
}

/* ================================================================
 * PHYSICS
 * ================================================================ */
static void update_physics(void){
  if(!pl.alive)return;
  int maxv=pl.boosting?BSVL:MXVL;
  /* Acceleration/braking */
  if(kU){pl.vel+=ACCL;if(pl.vel>maxv)pl.vel=maxv;}
  else if(kD){pl.vel-=BRKF;if(pl.vel<0)pl.vel=0;}
  else{pl.vel-=FRIC;if(pl.vel<0)pl.vel=0;}
  /* Off-road slow */
  if(iabs(pl.x_fp)>FP&&pl.vel>20)pl.vel-=3;
  /* Boost */
  if(kSh&&!pl.boosting&&pl.vel>40){pl.boosting=true;pl.bst=BSDR;pl.vel=imax(pl.vel,80);}
  if(pl.boosting){pl.bst--;if(pl.bst<=0)pl.boosting=false;}
  /* Steering — directly changes playerX (proportional to speed) */
  if(kL)pl.stv-=STRF;
  if(kR)pl.stv+=STRF;
  if(!kL&&!kR)pl.stv=(pl.stv*3)/4;  /* drag to centre */
  /* Centrifugal push from road curve */
  pl.stv-=gm.curve>>4;
  pl.stv=iclamp(pl.stv,-MXST,MXST);
  /* Apply steer — scaled by speed */
  int speedFrac=pl.vel*FP/MXVL;
  pl.x_fp+=(pl.stv*speedFrac)/FP;
  /* Clamp */
  pl.x_fp=iclamp(pl.x_fp,-FP*3/2,FP*3/2);
  if(iabs(pl.x_fp)>=FP*3/2-5)pl.stv=0;
  /* Scroll road (speed-proportional) */
  gm.scroll+=pl.vel>>3;
  if(pl.invuln>0)pl.invuln--;
  if(gm.fr%15==0&&pl.vel>20)pl.score++;
}

/* ================================================================
 * WORLD UPDATE
 * ================================================================ */
static void update_world(void){
  int spd=pl.vel>>3;
  /* Curve animation */
  gm.curve_t++;
  int target=(isin((gm.curve_t>>3)&0xFF)*36)>>8;
  gm.curve+=(target-gm.curve)>>4;
  /* Enemies */
  for(int i=0;i<NEMAX;i++){
    Enm*e=&en[i];
    if(!e->active)continue;
    e->wz-=spd+e->spd;
    e->anim++;
    if(e->wz<-10){e->active=false;continue;}
    /* Collision */
    if(e->wz<=COLLZ&&e->wz>=-5){
      int dx_fp=iabs(e->x_fp-pl.x_fp);
      if(dx_fp<COLLX&&pl.invuln<=0&&pl.alive){
        pl.hp--;pl.invuln=INVT;pl.vel/=2;
        if(pl.hp<=0){pl.alive=false;gm.st=ST_OVER;}
        e->active=false;
      }
    }
  }
  /* Spawning */
  if(--gm.spawn_t<=0){
    gm.spawn_t=imax(SPWNT-gm.fr/200,40);
    spawn_enemy();
  }
}

/* ================================================================
 * RENDER SCENE
 * ================================================================ */
static void render_scene(void){
  clrfb(C_K);
  draw_sky();
  draw_buildings();
  draw_road();
  draw_lamps();
  draw_rain();
  /* Enemies: sort far-to-near, then draw */
  int idx[NEMAX];int cnt=0;
  for(int i=0;i<NEMAX;i++)if(en[i].active)idx[cnt++]=i;
  for(int i=0;i<cnt-1;i++)
    for(int j=i+1;j<cnt;j++)
      if(en[idx[i]].wz<en[idx[j]].wz){int t=idx[i];idx[i]=idx[j];idx[j]=t;}
  for(int i=0;i<cnt;i++)draw_enemy(&en[idx[i]]);
  draw_batmobile();
  draw_hud();
  if(gm.st==ST_PAUSE)draw_pause();
  if(gm.st==ST_OVER)draw_gameover();
  blit();
}

/* ================================================================
 * MAIN
 * ================================================================ */
int main(void){
  hw_init();tmr_init();
  gm.st=ST_MENU;gm.fr=0;game_init();
  kU=kD=kL=kR=kSh=kEnt=kEsc=false;

  while(1){
    tmr_wait();gm.fr++;poll_input();
    switch(gm.st){
      case ST_MENU:
        if(kEnt){game_init();gm.st=ST_PLAY;kEnt=false;}
        draw_menu();blit();break;
      case ST_PLAY:
        if(kEsc){gm.st=ST_PAUSE;kEsc=false;}
        else{update_physics();update_world();}
        render_scene();break;
      case ST_PAUSE:
        if(kEsc||kEnt){gm.st=ST_PLAY;kEsc=kEnt=false;}
        render_scene();break;
      case ST_OVER:
        if(kEnt){gm.st=ST_MENU;kEnt=false;}
        render_scene();break;
    }
  }
  return 0;
}
/*
 * CPUlator: https://cpulator.01xz.net/?sys=arm-de1soc
 * Paste -> Compile and Load -> Continue -> ENTER
 * W/Up=Accel S/Down=Brake A/Left=Steer D/Right=Steer SHIFT=Boost ESC=Pause
 */

/* Copyright 1990-2009, Jsoftware Inc.  All rights reserved.               */
/* Licensed use only. Any other use is in violation of copyright.          */
/*                                                                         */
/* Conjunctions: Inner Product                                             */

#include "j.h"
#include "vasm.h"

// Analysis for inner product
// a,w are arguments
// zt is type of result
// *pm is # 1-cells of a
// *pn is # atoms in an item of w
// *pp is number of inner-product muladds
//   (in each, an atom of a multiplies an item of w)
static A jtipprep(J jt,A a,A w,I zt,I*pm,I*pn,I*pp){A z=mark;I*as,ar,ar1,m,mn,n,p,*ws,wr,wr1;
 ar=AR(a); as=AS(a); ar1=ar?ar-1:0; RE(*pm=m=prod(ar1,as));  // m=# 1-cells of a.  It could overflow, if there are no atoms
 wr=AR(w); ws=AS(w); wr1=wr?wr-1:0; RE(*pn=n=prod(wr1,1+ws)); RE(mn=mult(m,n));  // n=#atoms in 1-cell of w; mn = #atoms in result
 *pp=p=ar?*(as+ar1):wr?*ws:1;  // if a is an array, the length of a 1-cell; otherwise, the number of items of w
 ASSERT(!(ar&&wr)||p==*ws,EVLENGTH);
 GA(z,zt,mn,ar1+wr1,0);   // allocate result area
 ICPY(AS(z),      as,ar1);  // Set shape: 1-frame of a followed by shape of item of w
 ICPY(AS(z)+ar1,1+ws,wr1);
 R z;
}    /* argument validation & result for an inner product */

#define IINC(x,y)    {b=0>x; x+=y; BOV(b==0>y&&b!=0>x);}
#define DINC(x,y)    {x+=y;}
#define ZINC(x,y)    {(x).re+=(y).re; (x).im+=(y).im;}

#define PDTBY(Tz,Tw,INC)          \
 {Tw*v,*wv;Tz c,*x,zero,*zv;      \
  v=wv=(Tw*)AV(w); zv=(Tz*)AV(z); memset(&zero,C0,sizeof(Tz));     \
  if(1==n)DO(m, v=wv; c=zero;           DO(p,       if(*u++)INC(c,*v);             ++v;);             *zv++=c;)   \
  else    DO(m, v=wv; memset(zv,C0,zk); DO(p, x=zv; if(*u++)DO(n, INC(*x,*v); ++x; ++v;) else v+=n;); zv+=n;  );  \
 }

#define PDTXB(Tz,Ta,INC,INIT)     \
 {Ta*u;Tz c,*x,zero,*zv;          \
  u=   (Ta*)AV(a); zv=(Tz*)AV(z); memset(&zero,C0,sizeof(Tz));     \
  if(1==n)DO(m, v=wv; c=zero;           DO(p,                   if(*v++)INC(c,*u); ++u;  ); *zv++=c;)   \
  else    DO(m, v=wv; memset(zv,C0,zk); DO(p, x=zv; INIT; DO(n, if(*v++)INC(*x,c); ++x;);); zv+=n;  );  \
 }

static F2(jtpdtby){A z;B b,*u,*v,*wv;C er=0;I at,m,n,p,t,wt,zk;
 at=AT(a); wt=AT(w); t=at&B01?wt:at;
 RZ(z=ipprep(a,w,t,&m,&n,&p)); zk=n*bp(t); u=BAV(a); v=wv=BAV(w);
 NAN0;
 switch(CTTZ(t)){
  default:   ASSERT(0,EVDOMAIN);
  case CMPXX:  if(at&B01)PDTBY(Z,Z,ZINC) else PDTXB(Z,Z,ZINC,c=*u++   ); break;
  case FLX:    if(at&B01)PDTBY(D,D,DINC) else PDTXB(D,D,DINC,c=*u++   ); break;
  case INTX:   if(at&B01)PDTBY(I,I,IINC) else PDTXB(I,I,IINC,c=*u++   ); 
             if(er>=EWOV){
              RZ(z=ipprep(a,w,FL,&m,&n,&p)); zk=n*sizeof(D); u=BAV(a); v=wv=BAV(w);
              if(at&B01)PDTBY(D,I,IINC) else PDTXB(D,I,IINC,c=(D)*u++);
 }}
 NAN1;
 R z;
}    /* x +/ .* y where x or y (but not both) is Boolean */

#define BBLOCK(nn)  \
     d=ti; DO(nw, *d++=0;);                                           \
     DO(nn, if(*u++){vi=(UI*)v; d=ti; DO(nw, *d+++=*vi++;);} v+=n;);  \
     x=zv; c=tc; DO(n, *x+++=*c++;);

#if 0&&C_NA    // scaf  asm function no longer used, and no longer defined in C
/*
*** from asm64noovf.c
C asminnerprodx(I m,I*z,I u,I*y)
{
 I i=-1,t;
l1:
 ++i;
 if(i==m) return 0;
 t= u FTIMES y[i];
 ov(t)
 t= t FPLUS z[i];
 ov(t)
 z[i]=t;
 goto l1;
}
*/

C asminnerprodx(I m,I*z,I u,I*y)
{
 I i=-1,t,p;DI tdi;
l1:
 ++i;
 if(i==m) return 0;
 tdi = u * (DI)y[i];
 t=(I)tdi;
 if (tdi<IMIN||IMAX<tdi) R EWOV;
 p=0>t;
 t= t + z[i];
 if (p==0>z[i]&&p!=0>t) R EWOV;
 z[i]=t;
 goto l1;
}
#endif

// cache-blocking code
#define OPHEIGHT 2  // height of outer-product block
#define OPWIDTH 4  // width of outer-product block
#define CACHEWIDTH 64  // width of resident cache block
#define CACHEHEIGHT 16  // height of resident cache block
// Floating-point matrix multiply, hived off to a subroutine to get fresh register allocation
// *zv=*av * *wv, with *cv being a cache-aligned region big enough to hold CACHEWIDTH*CACHEHEIGHT floats
// a is shape mxp, w is shape pxn
static void cachedmmult (D* av,D* wv,D* zv,I m,I n,I p){D c[(CACHEHEIGHT+1)*CACHEWIDTH + (CACHEHEIGHT+1)*OPHEIGHT*OPWIDTH + 2*CACHELINESIZE/sizeof(D)];
 // m is # 1-cells of a
 // n is # atoms in an item of w (and result)
 // p is number of inner-product muladds (length of a row of a, and # items of w)
 // point to cache-aligned areas we will use for staging the inner-product info
#if C_AVX
 // Since we sometimes use 128-bit instructions, make sure we don't get stuck in slow state
 _mm256_zeroupper();
#endif
 D *cvw = (D*)(((I)&c+(CACHELINESIZE-1))&-CACHELINESIZE);  // place where cache-blocks of w are staged
 D *cva = (D*)(((I)cvw+(CACHEHEIGHT+1)*CACHEWIDTH*sizeof(D)+(CACHELINESIZE-1))&-CACHELINESIZE);   // place where expanded rows of a are staged
 // zero the result area
 memset(zv,C0,m*n*sizeof(D));
 // process each 64-float vertical stripe of w, producing the corresponding columns of z
 D* w0base = wv; D* z0base = zv; I w0rem = n;
 for(;w0rem>0;w0rem-=CACHEWIDTH,w0base+=CACHEWIDTH,z0base+=CACHEWIDTH){
  // process each 16x64 section of w, adding each result to the columns of z
  D* a1base=av; D* w1base=w0base; D* z1base=z0base; I w1rem=p;
  for(;w1rem>0;w1rem-=CACHEHEIGHT,a1base+=CACHEHEIGHT,w1base+=CACHEHEIGHT*n){D* RESTRICT cvx;D* w1next=w1base;I i;
   // read the 16x64 section of w into the cache area (8KB, 2 ways of cache), with prefetch of rows
   for(i=MIN(CACHEHEIGHT,w1rem),cvx=cvw;i;--i){I j;
    D* RESTRICT w1x=w1next; w1next+=n;  // save start of current input row, point to next row...
    // I don't think it's worth the trouble to move the data with AVX instructions - though it was to prefetch it
    for(j=0;j<MIN(CACHEWIDTH,w0rem);++j)*cvx++=*w1x++; for(;j<CACHEWIDTH;++j)*cvx++=0.0;   // move current row during prefetch
   }
   // Because of loop unrolling, we fetch and multiply one extra value in each cache column.  We make sure those values are 0 to avoid NaN errors
   for(i=0;i<MIN(CACHEWIDTH,w0rem);++i)*cvx++=0.0;

   // w1next is left pointing to the next cache block in the column.  We will use that to prefetch

   // the nx16 vertical strip of a will be multiplied by the 16x64 section of w and accumulated into z
   // process each 2x16 section of a against the 16x64 cache block
   D *a2base0=a1base; D* w2base=w1base; I a2rem=m; D* z2base=z1base; D* c2base=cvw;
   for(;a2rem>0;a2rem-=OPHEIGHT,a2base0+=OPHEIGHT*p,z2base+=OPHEIGHT*n){
    static D missingrow[CACHEHEIGHT]={1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
    // Prepare for the 2x16 block of a
    // If the second row of a is off the end of the data, we mustn't fetch it - switch the pointer to a row of 1s so it won't give NaN error on multiplying by infinity
    D *a2base1 = (a2rem>1)?a2base0+p:missingrow;
#if C_AVX
    // Make 4 sequential copies of each float in a, to support the parallel multiply for the outer product.  Interleave the values
    // from the two rows, putting them in the correct order for the multiply.  We fetch each row in order, to make sure we get fast page mode
    // for the row
    {__m256d *cvx;D *a0x;I j;
     for(a0x=a2base0,i=0;i<OPHEIGHT;++i,a0x=a2base1){
      for(cvx=(__m256d*)cva+i,j=MIN(CACHEHEIGHT,w1rem);j;--j,++a0x,cvx+=OPHEIGHT){
       *cvx = _mm256_set_pd(*a0x,*a0x,*a0x,*a0x);
      }
      // Because of loop unrolling, we fetch and multiply one extra outer product.  Make sure it is harmless, to avoid NaN errors
      *cvx = _mm256_set_pd(0.0,0.0,0.0,0.0);
     }
    }
#endif
#ifdef PREFETCH
    // While we are processing the sections of a, move the next cache block into L2 (not L1, so we don't overrun it)
    // We would like to do all the prefetches for a CACHEWIDTH at once to stay in page mode
    // but that might overrun the prefetch queue, which holds 10 prefetches.  So we just squeeze off 3 at a time
    if(a2rem<=4*OPHEIGHT*CACHEHEIGHT){I offset= (a2rem&(3*OPHEIGHT))*(2*CACHELINESIZE/OPHEIGHT);
     PREFETCH2((C*)w1next+offset); PREFETCH2((C*)w1next+offset+CACHELINESIZE); PREFETCH2((C*)w1next+offset+2*CACHELINESIZE);
     if(offset==CACHELINESIZE*3*OPHEIGHT)w1next += n;  // advance to next row for next time
    }
#endif
    // process each 16x4 section of cache, accumulating into z
    I a3rem=MIN(w0rem,CACHEWIDTH);
    D* RESTRICT z3base=z2base; D* c3base=c2base;
    for(;a3rem>0;a3rem-=OPWIDTH,c3base+=OPWIDTH,z3base+=OPWIDTH){
     // initialize accumulator with the z values accumulated so far.
#if C_AVX
     __m256d z00,z01,z10,z11,z20,z21; static I valmask[8]={0, 0,0,0,-1,-1,-1,-1};
     z21 = z20 = z11 = z10 = _mm256_set_pd(0.0,0.0,0.0,0.0);
     // We have to use masked load at the edges of the array, to make sure we don't fetch from undefined memory.  Fill anything not loaded with 0
     if(a3rem>3){z00 = _mm256_loadu_pd(z3base);if(a2rem>1)z01 = _mm256_loadu_pd(z3base+n); else z01=z21;   // In the main area, do normal (unaligned) loads
     }else{z01 = z00 = z20;
           z00 = _mm256_maskload_pd(z3base,_mm256_set_epi64x(valmask[a3rem],valmask[a3rem+1],valmask[a3rem+2],valmask[a3rem+3]));
           I vx= (a2rem>1)?a3rem:0; z01 = _mm256_maskload_pd(z3base+n,_mm256_set_epi64x(valmask[vx],valmask[vx+1],valmask[vx+2],valmask[vx+3]));
     }
#else
     D z00,z01,z02,z03,z10,z11,z12,z13;
     z00=z3base[0];
     if(a3rem>3){z01=z3base[1],z02=z3base[2],z03=z3base[3]; if(a2rem>1)z10=z3base[n],z11=z3base[n+1],z12=z3base[n+2],z13=z3base[n+3];
     }else{if(a3rem>1){z01=z3base[1];if(a3rem>2)z02=z3base[2];}; if(a2rem>1){z10=z3base[n];if(a3rem>1)z11=z3base[n+1];if(a3rem>2)z12=z3base[n+2];}}
#endif
     // process outer product of each 2x1 section on each 1x4 section of cache

     // Prefetch the next lines of a and z into L2 cache.  We don't prefetch all the way to L1 because that might overfill L1,
     // if all the prefetches hit the same line (we already have 2 lines for our cache area, plus the current z values)
     // The number of prefetches we need per line is (CACHEHEIGHT*sizeof(D)/CACHELINESIZE)+1, and we need that for
     // OPHEIGHT*(OPWIDTH/4) lines for each of a and z.  We squeeze off half the prefetches for a row at once so we can use fast page mode
     // to read the data (only half to avoid overfilling the prefetch buffer), and we space the reads as much as possible through the column-swatches
#ifdef PREFETCH   // if architecture doesn't support prefetch, skip it
     if((3*OPWIDTH)==(a3rem&(3*OPWIDTH))){  // every fourth swatch
      I inc=((a3rem&(8*OPWIDTH))?p:n)*sizeof(D);    // counting down, a then z
      C *base=(C*)((a3rem&(8*OPWIDTH))?a2base0:z2base) + inc + (a3rem&(4*OPWIDTH)?0:inc);
      PREFETCH2(base); PREFETCH2(base+CACHELINESIZE); PREFETCH2(base+2*CACHELINESIZE);
     }
#endif

     I a4rem=MIN(w1rem,CACHEHEIGHT);
     D* RESTRICT c4base=c3base;
#if C_AVX  // This version if AVX instruction set is available.
     __m256d *a4base0=(__m256d *)cva;   // Can't put RESTRICT on this - the loop to init *cva gets optimized away
      // read the 2x1 a values and the 1x4 cache values
      // form outer product, add to accumulator
// This loop is hand-unrolled because the compiler doesn't seem to do it.  Unroll 3 times - needed on dual ALUs
     __m256d cval0 = _mm256_load_pd(c4base);  // read from cache

     __m256d cval1 = _mm256_load_pd(c4base+CACHEWIDTH);
     __m256d aval00 = _mm256_mul_pd(cval0,a4base0[0]);  // multiply by a
     __m256d aval01 = _mm256_mul_pd(cval0,a4base0[1]);

     do{
      __m256d cval2 = _mm256_load_pd(c4base+2*CACHEWIDTH);
      __m256d aval10 = _mm256_mul_pd(cval1,a4base0[2]);
      __m256d aval11 = _mm256_mul_pd(cval1,a4base0[3]);
      z00 = _mm256_add_pd(z00 , aval00);    // accumulate into z
      z01 = _mm256_add_pd(z01 , aval01);
      if(--a4rem<=0)break;

      cval0 = _mm256_load_pd(c4base+3*CACHEWIDTH);
      __m256d aval20 = _mm256_mul_pd(cval2,a4base0[4]);
      __m256d aval21 = _mm256_mul_pd(cval2,a4base0[5]);
      z10 = _mm256_add_pd(z10 , aval10);
      z11 = _mm256_add_pd(z11 , aval11);
      if(--a4rem<=0)break;

      cval1 = _mm256_load_pd(c4base+4*CACHEWIDTH);
      aval00 = _mm256_mul_pd(cval0,a4base0[6]);
      aval01 = _mm256_mul_pd(cval0,a4base0[7]);
      z20 = _mm256_add_pd(z20 , aval20);
      z21 = _mm256_add_pd(z21 , aval21);
      if(--a4rem<=0)break;

      a4base0 += OPHEIGHT*3;  // OPWIDTH is implied by __m256d type
      c4base+=CACHEWIDTH*3;
     }while(1);

     // Collect the sums of products
     z10 = _mm256_add_pd(z10,z20);z11 = _mm256_add_pd(z11,z21); z00 = _mm256_add_pd(z00,z10); z01 = _mm256_add_pd(z01,z11);
     // Store accumulator into z.  Don't store outside the array
     if(a3rem>3){_mm256_storeu_pd(z3base,z00);if(a2rem>1)_mm256_storeu_pd(z3base+n,z01);
     }else{_mm256_maskstore_pd(z3base,_mm256_set_epi64x(valmask[a3rem],valmask[a3rem+1],valmask[a3rem+2],valmask[a3rem+3]),z00);
           if(a2rem>1)_mm256_maskstore_pd(z3base+n,_mm256_set_epi64x(valmask[a3rem],valmask[a3rem+1],valmask[a3rem+2],valmask[a3rem+3]),z01);
     }

#else   // If no AVX instructions
     D* RESTRICT a4base0=a2base0; D* RESTRICT a4base1=a2base1; 
     do{
      // read the 2x1 a values and the 1x4 cache values
      // form outer product, add to accumulator
      D t0,t1,a0,a1,c0,c1,c2,c3;
      a0=a4base0[0]; t0=c4base[0]; 
      a1=a4base1[0]; c0=c4base[0];
      t0 *= a0; c0 *= a1;
      t1=c4base[1]; c1=c4base[1]; t1 *= a0; c1 *= a1;
      z00 += t0; t0 = c4base[2]; c2 = c4base[2]; c3 = c4base[3];
      z10 += c0; z01 += t1; z11 += c1;
      t0 *= a0; c2 *= a1; a0 *= c3; c3 *= a1;
      z02 += t0; z12 += c2; z03 += a0; z13 += c3;
      a4base0++,a4base1++;
      c4base+=CACHEWIDTH;
     }while(--a4rem>0);

     // Store accumulator into z.  Don't store outside the array
     z3base[0]=z00;
     if(a3rem>3){z3base[1]=z01,z3base[2]=z02,z3base[3]=z03; if(a2rem>1)z3base[n]=z10,z3base[n+1]=z11,z3base[n+2]=z12,z3base[n+3]=z13;
     }else{if(a3rem>1){z3base[1]=z01;if(a3rem>2)z3base[2]=z02;}; if(a2rem>1){z3base[n]=z10;if(a3rem>1){z3base[n+1]=z11;if(a3rem>2)z3base[n+2]=z12;}}}
#endif
    }
   }
  }
 }
}




F2(jtpdt){PROLOG(0038);A z;I ar,at,i,m,n,p,p1,t,wr,wt;
 RZ(a&&w);
 // ?r = rank, ?t = type (but set Boolean type for an empty argument)
 ar=AR(a); at=AN(a)?AT(a):B01;
 wr=AR(w); wt=AN(w)?AT(w):B01;
 if((at|wt)&SPARSE)R pdtsp(a,w);  // Transfer to sparse code if either arg sparse
 if((at|wt)&XNUM+RAT)R df2(a,w,atop(slash(ds(CPLUS)),qq(ds(CSTAR),v2(1L,AR(w)))));  // On indirect numeric, execute as +/@(*"(1,(wr)))
 if(ar&&wr&&AN(a)&&AN(w)&&TYPESNE(at,wt)&&B01&at+wt)R pdtby(a,w);   // If exactly one arg is boolean, handle separately
 t=coerce2(&a,&w,B01);  // convert a/w to common type, using b01 if both empty
 ASSERT(t&NUMERIC,EVDOMAIN);
 // Allocate result area and calculate loop controls
 // m is # 1-cells of a
 // n is # atoms in an item of w
 // p is number of inner-product muladds (length of a row of a)

 RZ(z=ipprep(a,w,t&B01?INT:t&INT&&!SY_64?FL:t,&m,&n,&p));
 if(!p){memset(AV(z),C0,AN(z)*bp(AT(z))); R z;}
 // If either arg is atomic, reshape it to a list
 if(!ar!=!wr){if(ar)RZ(w=reshape(sc(p),w)) else RZ(a=reshape(sc(p),a));}
 p1=p-1;
 // Perform the inner product according to the type
 switch(CTTZNOFLAG(t)){
  case B01X:
   if(0==n%SZI||!SY_ALIGN){A tt;B*u,*v,*wv;I nw,q,r,*x,*zv;UC*c,*tc;UI*d,*ti,*vi;
    q=p/255; r=p%255; nw=(n+SZI-1)/SZI;
    GATV(tt,INT,nw,1,0); ti=(UI*)AV(tt); tc=(UC*)ti;
    u=BAV(a); v=wv=BAV(w); zv=AV(z);
    for(i=0;i<m;++i,v=wv,zv+=n){x=zv; DO(n, *x++=0;); DO(q, BBLOCK(255);); BBLOCK(r);}
   }else{B*u,*v,*wv;I*x,*zv;
    u=BAV(a); v=wv=BAV(w); zv=AV(z);
    for(i=0;i<m;++i,v=wv,zv+=n){
            x=zv; if(*u++)DO(n, *x++ =*v++;) else{v+=n; DO(n, *x++=0;);}
     DO(p1, x=zv; if(*u++)DO(n, *x+++=*v++;) else v+=n;);
   }}
   break;
  case INTX:
#if SY_64
   {I er=0;I c,* RESTRICT u,* RESTRICT v,* RESTRICT wv,* RESTRICT x,* RESTRICT zv;
    u=AV(a); v=wv=AV(w); zv=AV(z);
 /*
   for(i=0;i<m;++i,v=wv,zv+=n){
     x=zv; c=*u++; er=asmtymes1v(n,x,c,v);    if(er)break; v+=n;
     DO(p1, x=zv; c=*u++; er=asminnerprodx(n,x,c,v); if(er)break; v+=n;);

 */
#if C_NA   // non-assembler version
    for(i=0;i<m;++i,v=wv,zv+=n){DPMULDECLS I o,oc,lp;
     x=zv; c=*u++; DQ(n, DPMUL(c,*v, x, ++er); ++v; ++x;)
    DQ(p1, x=zv; c=*u++; DQ(n, DPMULD(c,*v, lp, ++er;) o=*x; oc=(~o)^lp; lp+=o; *x++=lp; o^=lp; ++v; if(XANDY(oc,o)<0)++er;) if(er)break;)  // oflo if signs equal, and different from result sign
    }
#else
    for(i=0;i<m;++i,v=wv,zv+=n){
     x=zv; c=*u++; TYMES1V(n,x,c,v); if(er)break; v+=n;
     DO(p1, x=zv; c=*u++; er=asminnerprodx(n,x,c,v); if(er)break; v+=n;);
     if(er)break;
    }
#endif
    if(er){A z1;D c,* RESTRICT x,* RESTRICT zv;I* RESTRICT u,* RESTRICT v,* RESTRICT wv;
     GATV(z1,FL,AN(z),AR(z),AS(z)); z=z1;
     u=AV(a); v=wv=AV(w); zv=DAV(z);
     for(i=0;i<m;++i,v=wv,zv+=n){
             x=zv; c=(D)*u++; DO(n, *x++ =c**v++;);
      DO(p1, x=zv; c=(D)*u++; DO(n, *x+++=c**v++;););
   }}}
#else
   {D c,*x,*zv;I*u,*v,*wv;
    u=AV(a); v=wv=AV(w); zv=DAV(z);
    if(1==n)DO(m, v=wv; c=0.0; DO(p, c+=*u++*(D)*v++;); *zv++=c;)
    else for(i=0;i<m;++i,v=wv,zv+=n){
            x=zv; c=(D)*u++; DO(n, *x++ =c**v++;);
     DO(p1, x=zv; c=(D)*u++; DO(n, *x+++=c**v++;););
    }
    RZ(z=icvt(z));
   }
#endif
   break;
  case FLX:
   {NAN0;
    cachedmmult(DAV(a),DAV(w),DAV(z),m,n,p);
    // If there was a floating-point error, retry it the old way in case it was _ * 0
    if(NANTEST){D c,s,t,*u,*v,*wv,*x,*zv;
     u=DAV(a); v=wv=DAV(w); zv=DAV(z);
     NAN0;
     if(1==n){DO(m, v=wv; c=0.0; DO(p, s=*u++; t=*v++; c+=s&&t?s*t:0;); *zv++=c;);}
     else for(i=0;i<m;++i,v=wv,zv+=n){
             x=zv; if(c=*u++){if(INF(c))DO(n, *x++ =*v?c**v:0.0; ++v;)else DO(n, *x++ =c**v++;);}else{v+=n; DO(n, *x++=0.0;);}
      DO(p1, x=zv; if(c=*u++){if(INF(c))DO(n, *x+++=*v?c**v:0.0; ++v;)else DO(n, *x+++=c**v++;);}else v+=n;);
     }
     NAN1;
    }
   }
   break;
  case CMPXX:
   {Z c,*u,*v,*wv,*x,*zv;
    u=ZAV(a); v=wv=ZAV(w); zv=ZAV(z);
    if(1==n)DO(m, v=wv; c=zeroZ; DO(p, c.re+=ZRE(*u,*v); c.im+=ZIM(*u,*v); ++u; ++v;); *zv++=c;)
    else for(i=0;i<m;++i,v=wv,zv+=n){
            x=zv; c=*u++; DO(n, x->re =ZRE(c,*v); x->im =ZIM(c,*v); ++x; ++v;);
     DO(p1, x=zv; c=*u++; DO(n, x->re+=ZRE(c,*v); x->im+=ZIM(c,*v); ++x; ++v;););
 }}}
 EPILOG(z);
}

#define IPBX(F)  \
 for(i=0;i<m;++i){                                       \
  MC(zv,*av?v1:v0,n); if(ac)++av;                    \
  for(j=1;j<p;++j){                                      \
   uu=(I*)zv; vv=(I*)(*av?v1+j*wc:v0+j*wc); if(ac)++av;  \
   DO(q, *uu++F=*vv++;);                                 \
   if(r){u=(B*)uu; v=(B*)vv; DO(r, *u++F=*v++;);}        \
  }                                                      \
  zv+=n;                                                 \
 }

#define IPBX0  0
#define IPBX1  1
#define IPBXW  2
#define IPBXNW 3

// a f/ . g w  for boolean a and w
// c is pseudochar for f, d is pseudochar for g
static A jtipbx(J jt,A a,A w,C c,C d){A g=0,x0,x1,z;B*av,*av0,b,*u,*v,*v0,*v1,*zv;C c0,c1;
    I ana,i,j,m,n,p,q,r,*uu,*vv,wc;
 RZ(a&&w);
 RZ(z=ipprep(a,w,B01,&m,&n,&p));
 // m=#1-cells of a, n=# bytes in 1-cell of w, p=length of individual inner product creating an atom
 ana=!!AR(a); wc=AR(w)?n:0; q=n/SZI; r=n%SZI;  // ana = 1 if a is not atomic; wc = stride between items of w; q=#fullwords in an item of w, r=remainder
 // Set c0 & c1 to classify the g operation
 switch(B01&AT(w)?d:0){
  case CEQ:                             c0=IPBXNW; c1=IPBXW;  break;
  case CNE:                             c0=IPBXW;  c1=IPBXNW; break;
  case CPLUSDOT: case CMAX:             c0=IPBXW;  c1=IPBX1;  break;
  case CSTARDOT: case CMIN: case CSTAR: c0=IPBX0;  c1=IPBXW;  break;
  case CLT:                             c0=IPBXW;  c1=IPBX0;  break;
  case CGT:                             c0=IPBX0;  c1=IPBXNW; break; 
  case CLE:                             c0=IPBX1;  c1=IPBXW;  break;
  case CGE:                             c0=IPBXNW; c1=IPBX1;  break;
  case CPLUSCO:                         c0=IPBXNW; c1=IPBX0;  break;
  case CSTARCO:                         c0=IPBX1;  c1=IPBXNW; break;
  default: c0=c1=-1; g=ds(d); RZ(x0=df2(zero,w,g)); RZ(x1=df2(zero,w,g));
 }
 // Set up x0 to be the argument to use for y if the atom of x is 0: 0, 1, y, -.y
 // Set up x1 to be the arg if xatom is 1
 if(!g)RZ(x0=c0==IPBX0?reshape(sc(n),zero):c0==IPBX1?reshape(sc(c==CNE?AN(w):n),one):c0==IPBXW?w:not(w));
 if(!g)RZ(x1=c1==IPBX0?reshape(sc(n),zero):c1==IPBX1?reshape(sc(c==CNE?AN(w):n),one):c1==IPBXW?w:not(w));
 // av->a arg, zv->result, v0->input for 0, v1->input for 1
 av0=BAV(a); zv=BAV(z); v0=BAV(x0); v1=BAV(x1);

 // See if the operations are such that a 0 or 1 from a can skip the innerproduct, or perhaps
 // terminate the entire operation
 I esat=2, eskip=2;  // if byte of a equals esat, the entire result-cell is set to its limit value; if it equals eskip, the innerproduct is skipped
 if(c==CPLUSDOT&&(c0==IPBX1||c1==IPBX1)||c==CSTARDOT&&(c0==IPBX0||c1==IPBX0)){
 // +./ . (+. <: >: *:)   *./ . (*. < > +:)   a byte of a can saturate the entire result-cell: see which value does that
  esat=c==CPLUSDOT?c1==IPBX1:c1==IPBX0;  // esat==1 if +./ . (+. >:)   or *./ . (< +:) where x=1 overrides y (producing 1);  if esat=0, x=0 overrides y (producing 1)
 }else if(c==CPLUSDOT&&(c0==IPBX0||c1==IPBX0)||c==CSTARDOT&&(c0==IPBX1||c1==IPBX1)||
     c==CNE&&(c0==IPBX0||c1==IPBX0)){
  // (+. ~:)/ . (*. < > +:)   *./ . (+. <: >: *:)  a byte of a can guarantee the current innerproduct has no effect
  eskip=c==CSTARDOT?c1==IPBX1:c1==IPBX0;  // eskip==1 if  (+. ~:)/ . (+:)   *./ . (+. >:)  where x=1 has no effect; if esat=0, x=0 has no effect
 }

 switch(c){
  case CPLUSDOT:
#define F |=
#include "cip_t.h"
   break;
  case CSTARDOT:
#define F &=
#include "cip_t.h"
   break;
  case CNE:
#define F ^=
#include "cip_t.h"
   break;
 }
 R z;
}    /* a f/ . g w  where a and w are nonempty and a is boolean */

static DF2(jtdotprod){A fs,gs;C c,d;I r;V*sv;
 RZ(a&&w&&self);
 sv=VAV(self); fs=sv->f; gs=sv->g;
 if(B01&AT(a)&&AN(a)&&AN(w)&&CSLASH==ID(fs)&&(d=vaid(gs))&&
     (c=vaid(VAV(fs)->f),c==CSTARDOT||c==CPLUSDOT||c==CNE))R ipbx(a,w,c,d);
 r=lr(gs);
 R df2(a,w,atop(fs,qq(gs,v2(r==RMAX?r:1+r,RMAX))));
}


static F1(jtminors){A d;
 RZ(d=apv(3L,-1L,1L)); *AV(d)=0;
 R drop(d,df2(one,w,bsdot(ds(CLEFT))));
}

static DF1(jtdet){DECLFG;A h=sv->h;I c,r,*s;
 RZ(w);
 r=AR(w); s=AS(w);
 if(h&&1<r&&2==s[r-1]&&s[r-2]==s[r-1])R df1(w,h);
 F1RANK(2,jtdet,self);
 c=2>r?1:s[1];
 R !c ? df1(mtv,slash(gs)) : 1==c ? CALL1(f1,ravel(w),fs) : h && c==*s ? gaussdet(w) : detxm(w,self); 
}

DF1(jtdetxm){R dotprod(irs1(w,0L,1L,jthead),det(minors(w),self),self);}
     /* determinant via expansion by minors. w is matrix with >1 columns */

F2(jtdot){A f,h=0;AF f2=jtdotprod;C c,d;
 ASSERTVV(a,w);
 if(CSLASH==ID(a)){
  f=VAV(a)->f; c=ID(f); d=ID(w);
  if(d==CSTAR){
   if(c==CPLUS )f2=jtpdt; 
   if(c==CMINUS)RZ(h=eval("[: -/\"1 {.\"2 * |.\"1@:({:\"2)"));
 }}
 R fdef(CDOT,VERB, jtdet,f2, a,w,h, 0L, 2L,RMAX,RMAX);
}

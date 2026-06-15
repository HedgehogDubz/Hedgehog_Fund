import React, { useEffect, useRef, useState, useMemo, useCallback } from 'react';
import { PanelContainer, Row, Dropdown } from '../components/widgets';
import { usePanelValue } from '../state/store';
import { theme } from '../utils/theme';

const CHART_STYLES = ['Line', 'Mountain', 'Candle', 'OHLC', 'All Lines'];
const TIME_NAMES = ['timestamp', 'time', 'date', 'datetime'];
const COLORS_GL = ['#e6db74','#66d9ef','#f92672','#a6e22e','#fd971f','#ae81ff','#a1efe4','#f8f8f2'].map(hexToGL);
const UP_GL = hexToGL('#a6e22e'), DN_GL = hexToGL('#f92672');
const UP_VOL = [0.651,0.886,0.18,0.5], DN_VOL = [0.976,0.149,0.447,0.5];
const ML = 2, MR = 70, MT = 10, MB = 30;

function hexToGL(h) { return [parseInt(h.slice(1,3),16)/255, parseInt(h.slice(3,5),16)/255, parseInt(h.slice(5,7),16)/255, 1]; }
function findCI(cols, cands) { return cols.findIndex(c => cands.includes(c.toLowerCase())); }
function findOHLC(cols) {
  const o=findCI(cols,['open']),h=findCI(cols,['high']),l=findCI(cols,['low']),c=findCI(cols,['close']);
  return (o>=0&&h>=0&&l>=0&&c>=0) ? {o,h,l,c} : null;
}
function findFirst(cols, excl) { for(let i=0;i<cols.length;i++) if(!excl.has(i)) return i; return 0; }

// ── Shaders ─────────────────────────────────────────────────────────────────
const VS_L=`#version 300 es
in vec2 a;uniform vec2 s,t;void main(){gl_Position=vec4(a*s+t,0,1);}`;
const FS_L=`#version 300 es
precision mediump float;uniform vec4 c;out vec4 o;void main(){o=c;}`;
const VS_Q=`#version 300 es
in vec2 a;in vec4 b;uniform vec2 s,t;out vec4 v;void main(){gl_Position=vec4(a*s+t,0,1);v=b;}`;
const FS_Q=`#version 300 es
precision mediump float;in vec4 v;out vec4 o;void main(){o=v;}`;

function mkS(gl,t,s){const x=gl.createShader(t);gl.shaderSource(x,s);gl.compileShader(x);return x;}
function mkP(gl,v,f){const p=gl.createProgram();gl.attachShader(p,v);gl.attachShader(p,f);gl.linkProgram(p);return p;}
function pushQ(p,c,x1,y1,x2,y2,cl){p.push(x1,y1,x2,y1,x1,y2,x1,y2,x2,y1,x2,y2);for(let i=0;i<6;i++)c.push(cl[0],cl[1],cl[2],cl[3]);}

// ── Engine ──────────────────────────────────────────────────────────────────
class Engine {
  constructor(glC, ovC) {
    this.glC=glC; this.ov=ovC; this.ctx=ovC.getContext('2d');
    this.gl=glC.getContext('webgl2',{antialias:true,alpha:false});
    if(!this.gl) return;
    const gl=this.gl;
    const lv=mkS(gl,gl.VERTEX_SHADER,VS_L),lf=mkS(gl,gl.FRAGMENT_SHADER,FS_L);
    this.lP=mkP(gl,lv,lf); this.lS=gl.getUniformLocation(this.lP,'s'); this.lT=gl.getUniformLocation(this.lP,'t');
    this.lC=gl.getUniformLocation(this.lP,'c'); this.lA=gl.getAttribLocation(this.lP,'a');
    const qv=mkS(gl,gl.VERTEX_SHADER,VS_Q),qf=mkS(gl,gl.FRAGMENT_SHADER,FS_Q);
    this.qP=mkP(gl,qv,qf); this.qS=gl.getUniformLocation(this.qP,'s'); this.qT=gl.getUniformLocation(this.qP,'t');
    this.qA=gl.getAttribLocation(this.qP,'a'); this.qB=gl.getAttribLocation(this.qP,'b');
    this.b1=gl.createBuffer(); this.b2=gl.createBuffer(); this.b3=gl.createBuffer();

    this.data=null; this.cols=null; this.style='Line';
    this.signals=null; // [{action:'buy'|'sell', index:number}]
    this.lines=null;   // { name: [{x:number,y:number}, ...], ... }
    // View window in data space
    this.x0=0; this.x1=1; this.y0=0; this.y1=1;
    this.yAuto=true; // when true, y range auto-fits on x change
    this.xLock=false; this.yLock=false;
    this.dirty=true; this._raf=null;
    this._tick=this._tick.bind(this); this._tick();
  }

  _computeYRange(x0,x1) {
    if(!this.data||!this.cols) return [0,1];
    const vs=Math.max(0,Math.floor(x0)), ve=Math.min(this.data.length,Math.ceil(x1));
    const ohlc=findOHLC(this.cols); const volI=findCI(this.cols,['volume']); const tI=findCI(this.cols,TIME_NAMES);
    const excl=new Set([tI,volI].filter(i=>i>=0));
    const s=this.style;
    const scan = s==='All Lines' ? Array.from({length:this.cols.length},(_,i)=>i).filter(i=>!excl.has(i))
      : (ohlc&&(s==='Candle'||s==='OHLC')) ? [ohlc.o,ohlc.h,ohlc.l,ohlc.c]
      : [ohlc?ohlc.c:findFirst(this.cols,excl)];
    let mn=Infinity, mx=-Infinity;
    for(let i=vs;i<ve;i++) for(const ci of scan){const v=this.data[i][ci];if(typeof v==='number'&&isFinite(v)){if(v<mn)mn=v;if(v>mx)mx=v;}}
    if(!isFinite(mn)||mn===mx){mn-=1;mx+=1;}
    const p=(mx-mn)*0.05; return [mn-p,mx+p];
  }

  setData(d,c){
    this.data=d;this.cols=c;
    if(d&&d.length>0){this.x0=0;this.x1=d.length;const[y0,y1]=this._computeYRange(0,d.length);this.y0=y0;this.y1=y1;this.yAuto=true;}
    this.dirty=true;
  }
  setStyle(s){this.style=s;if(this.yAuto){const[y0,y1]=this._computeYRange(this.x0,this.x1);this.y0=y0;this.y1=y1;}this.dirty=true;}
  setSignals(s){this.signals=s||null;this.dirty=true;}
  setLines(l){this.lines=l&&Object.keys(l).length?l:null;this.dirty=true;}
  home(){if(this.data){this.x0=0;this.x1=this.data.length;const[y0,y1]=this._computeYRange(0,this.data.length);this.y0=y0;this.y1=y1;this.yAuto=true;}this.dirty=true;}
  resize(){this.dirty=true;}
  setMode(m){this.xLock=(m==='vresize');this.yLock=(m==='hresize');}

  pan(dxPx,dyPx,cw,ch){
    if(!this.data) return;
    const volI=findCI(this.cols,['volume']); const volH=volI>=0?80:0;
    const chartW=cw-ML-MR, chartH=ch-MT-MB-volH;
    if(!this.xLock){
      const dx=-dxPx/chartW*(this.x1-this.x0);
      let nx0=this.x0+dx, nx1=this.x1+dx;
      if(nx0<0){nx1-=nx0;nx0=0;} if(nx1>this.data.length){nx0-=(nx1-this.data.length);nx1=this.data.length;}
      this.x0=nx0;this.x1=nx1;
    }
    if(this.xLock){
      // vresize mode: pan Y manually
      const dy=dyPx/chartH*(this.y1-this.y0);
      this.y0+=dy;this.y1+=dy;this.yAuto=false;
    } else {
      // Normal/hresize: auto-fit Y to visible X range
      const[y0,y1]=this._computeYRange(this.x0,this.x1);this.y0=y0;this.y1=y1;this.yAuto=true;
    }
    this.dirty=true;
  }

  zoom(factor,anchorPxX,anchorPxY,cw,ch){
    if(!this.data) return;
    const volI=findCI(this.cols,['volume']); const volH=volI>=0?80:0;
    const chartW=cw-ML-MR, chartH=ch-MT-MB-volH;
    if(!this.xLock){
      // Zoom X anchored to mouse
      const ar=Math.max(0,Math.min(1,(anchorPxX-ML)/chartW));
      const rng=this.x1-this.x0, anc=this.x0+rng*ar;
      let nr=Math.max(10,Math.min(this.data.length,rng*factor));
      let nx0=anc-nr*ar, nx1=nx0+nr;
      if(nx0<0){nx1-=nx0;nx0=0;} if(nx1>this.data.length){nx0-=(nx1-this.data.length);nx1=this.data.length;}
      this.x0=Math.max(0,nx0);this.x1=Math.min(this.data.length,nx1);
    }
    if(this.xLock){
      // vresize mode: zoom Y manually, anchored to mouse
      const ar=Math.max(0,Math.min(1,(anchorPxY-MT)/chartH));
      const rng=this.y1-this.y0, anc=this.y1-rng*ar;
      let nr=rng*factor;
      this.y0=anc-nr*(1-ar); this.y1=anc+nr*ar;
      this.yAuto=false;
    } else {
      // Normal/hresize: auto-fit Y to new visible X range
      const[y0,y1]=this._computeYRange(this.x0,this.x1);this.y0=y0;this.y1=y1;this.yAuto=true;
    }
    this.dirty=true;
  }

  // Zoom to rect (for magnifying glass mode) — pixel coords
  zoomToRect(px1,py1,px2,py2,cw,ch){
    if(!this.data) return;
    const volI=findCI(this.cols,['volume']); const volH=volI>=0?80:0;
    const chartW=cw-ML-MR, chartH=ch-MT-MB-volH;
    const xr0=(Math.min(px1,px2)-ML)/chartW, xr1=(Math.max(px1,px2)-ML)/chartW;
    const yr0=(Math.min(py1,py2)-MT)/chartH, yr1=(Math.max(py1,py2)-MT)/chartH;
    const xRange=this.x1-this.x0, yRange=this.y1-this.y0;
    const nx0=this.x0+xRange*Math.max(0,xr0), nx1=this.x0+xRange*Math.min(1,xr1);
    const ny1=this.y1-yRange*Math.max(0,yr0), ny0=this.y1-yRange*Math.min(1,yr1);
    if(nx1-nx0>=5){this.x0=nx0;this.x1=nx1;}
    if(ny1-ny0>0){this.y0=ny0;this.y1=ny1;this.yAuto=false;}
    this.dirty=true;
  }

  destroy(){if(this._raf)cancelAnimationFrame(this._raf);}
  _tick(){if(this.dirty){this.dirty=false;this._render();}this._raf=requestAnimationFrame(this._tick);}

  _render(){
    const gl=this.gl,ctx=this.ctx;if(!gl)return;
    const dpr=window.devicePixelRatio||1;
    const par=this.glC.parentElement;if(!par)return;
    const cw=par.clientWidth,ch=par.clientHeight;
    for(const c of[this.glC,this.ov]){c.width=cw*dpr;c.height=ch*dpr;c.style.width=cw+'px';c.style.height=ch+'px';}
    gl.viewport(0,0,cw*dpr,ch*dpr);gl.clearColor(0.118,0.118,0.118,1);gl.clear(gl.COLOR_BUFFER_BIT);
    gl.enable(gl.BLEND);gl.blendFunc(gl.SRC_ALPHA,gl.ONE_MINUS_SRC_ALPHA);
    ctx.clearRect(0,0,cw*dpr,ch*dpr);ctx.save();ctx.scale(dpr,dpr);

    if(!this.data||!this.cols||this.data.length===0){ctx.restore();return;}

    const ohlc=findOHLC(this.cols),volI=findCI(this.cols,['volume']),tI=findCI(this.cols,TIME_NAMES);
    const excl=new Set([tI,volI].filter(i=>i>=0));
    const hasVol=volI>=0,volH=hasVol?80:0;
    const chartW=cw-ML-MR, chartH=ch-MT-MB-volH;
    if(chartW<=0||chartH<=0){ctx.restore();return;}

    const vs=Math.max(0,Math.floor(this.x0)),ve=Math.min(this.data.length,Math.ceil(this.x1));
    const n=ve-vs;if(n<=0){ctx.restore();return;}
    const minP=this.y0,maxP=this.y1;
    if(maxP<=minP){ctx.restore();return;}

    const toX=i=>ML+((i-this.x0)/(this.x1-this.x0))*chartW;
    const toY=v=>MT+(1-(v-minP)/(maxP-minP))*chartH;
    const sx=2/cw,sy=-2/ch,tx=-1,ty=1;

    // Grid
    ctx.strokeStyle='#2a2a2a';ctx.lineWidth=0.5;ctx.font='10px Menlo,monospace';ctx.fillStyle='#75715e';ctx.textAlign='left';
    for(let i=0;i<=5;i++){
      const y=MT+(i/5)*chartH;
      ctx.beginPath();ctx.moveTo(ML,y);ctx.lineTo(ML+chartW,y);ctx.stroke();
      const price=maxP-(i/5)*(maxP-minP);
      ctx.fillText(price.toFixed(price>=100?2:5),ML+chartW+6,y+3);
    }
    if(hasVol){ctx.beginPath();ctx.moveTo(ML,ch-volH);ctx.lineTo(ML+chartW,ch-volH);ctx.stroke();}

    const style=this.style||'Line';
    if(style==='Candle'&&ohlc) this._candles(gl,vs,ve,n,ohlc,toX,toY,chartW,sx,sy,tx,ty);
    else if(style==='OHLC'&&ohlc) this._ohlcBars(gl,vs,ve,n,ohlc,toX,toY,chartW,sx,sy,tx,ty);
    else if(style==='Mountain'){
      const vi=ohlc?ohlc.c:findFirst(this.cols,excl);
      this._mountain(gl,vs,ve,vi,toX,toY,chartH,sx,sy,tx,ty);
      this._line(gl,vs,ve,vi,toX,toY,COLORS_GL[0],sx,sy,tx,ty);
    } else if(style==='All Lines'){
      let ci=0;for(let i=0;i<this.cols.length;i++){if(excl.has(i))continue;this._line(gl,vs,ve,i,toX,toY,COLORS_GL[ci%COLORS_GL.length],sx,sy,tx,ty);ci++;}
    } else {
      const vi=ohlc?ohlc.c:findFirst(this.cols,excl);
      this._line(gl,vs,ve,vi,toX,toY,COLORS_GL[0],sx,sy,tx,ty);
    }

    if(hasVol) this._volume(gl,vs,ve,n,volI,ohlc?ohlc.c:findFirst(this.cols,excl),toX,cw,ch,volH,sx,sy,tx,ty);
    if(this.lines) this._drawLines(ctx,toX,toY);
    if(this.signals&&this.signals.length) this._drawSignals(ctx,vs,ve,ohlc,excl,toX,toY);
    ctx.restore();
  }

  _drawLines(ctx,toX,toY){
    const palette=['#e6db74','#66d9ef','#a6e22e','#fd971f','#ae81ff','#a1efe4','#f8f8f2'];
    const names=Object.keys(this.lines);
    ctx.save();
    ctx.lineWidth=1.25;
    let ci=0;
    for(const name of names){
      const pts=this.lines[name];
      if(!pts||!pts.length){ ci++; continue; }
      ctx.strokeStyle=palette[ci%palette.length];
      ctx.beginPath();
      let started=false;
      for(const p of pts){
        const x=toX(p.x), y=toY(p.y);
        if(!Number.isFinite(x)||!Number.isFinite(y)) continue;
        if(!started){ ctx.moveTo(x,y); started=true; } else ctx.lineTo(x,y);
      }
      ctx.stroke();
      ci++;
    }
    // Legend
    ctx.font='11px Menlo,monospace';
    ctx.textAlign='left'; ctx.textBaseline='middle';
    let ly=MT+8; ci=0;
    for(const name of names){
      ctx.fillStyle=palette[ci%palette.length];
      ctx.fillRect(ML+6,ly-1,10,2);
      ctx.fillStyle='#f8f8f2';
      ctx.fillText(name,ML+22,ly);
      ly+=14; ci++;
    }
    ctx.restore();
  }

  _drawSignals(ctx,vs,ve,ohlc,excl,toX,toY){
    // Place markers on the close (or the only series if no OHLC), centered on
    // the point itself rather than offset above/below the bar.
    const closeI=ohlc?ohlc.c:findFirst(this.cols,excl);
    for(const sig of this.signals){
      const i=sig.index;
      if(i<vs||i>=ve) continue;
      const row=this.data[i]; if(!row) continue;
      const x=toX(i);
      const y=toY(Number(row[closeI])||0);
      ctx.strokeStyle='#000'; ctx.lineWidth=0.5;
      ctx.beginPath();
      if(sig.action==='buy'){
        ctx.fillStyle='#a6e22e';                                // up-triangle, apex on the close
        ctx.moveTo(x,y-4); ctx.lineTo(x-5,y+4); ctx.lineTo(x+5,y+4);
      } else if(sig.action==='sell'){
        ctx.fillStyle='#f92672';                                // down-triangle, apex on the close
        ctx.moveTo(x,y+4); ctx.lineTo(x-5,y-4); ctx.lineTo(x+5,y-4);
      } else {
        continue;
      }
      ctx.closePath(); ctx.fill(); ctx.stroke();
    }
  }

  _line(gl,vs,ve,ci,toX,toY,clr,sx,sy,tx,ty){
    const n=ve-vs,a=new Float32Array(n*2);
    for(let i=0;i<n;i++){a[i*2]=toX(vs+i);a[i*2+1]=toY(Number(this.data[vs+i][ci])||0);}
    gl.useProgram(this.lP);gl.uniform2f(this.lS,sx,sy);gl.uniform2f(this.lT,tx,ty);gl.uniform4fv(this.lC,clr);
    gl.bindBuffer(gl.ARRAY_BUFFER,this.b1);gl.bufferData(gl.ARRAY_BUFFER,a,gl.DYNAMIC_DRAW);
    gl.enableVertexAttribArray(this.lA);gl.vertexAttribPointer(this.lA,2,gl.FLOAT,false,0,0);
    gl.drawArrays(gl.LINE_STRIP,0,n);
  }

  _mountain(gl,vs,ve,ci,toX,toY,cH,sx,sy,tx,ty){
    const n=ve-vs,bot=MT+cH,p=new Float32Array(n*4),c=new Float32Array(n*8);
    for(let i=0;i<n;i++){const x=toX(vs+i),y=toY(Number(this.data[vs+i][ci])||0);
      p[i*4]=x;p[i*4+1]=y;p[i*4+2]=x;p[i*4+3]=bot;
      c[i*8]=0.651;c[i*8+1]=0.886;c[i*8+2]=0.18;c[i*8+3]=0.35;
      c[i*8+4]=0.651;c[i*8+5]=0.886;c[i*8+6]=0.18;c[i*8+7]=0;}
    gl.useProgram(this.qP);gl.uniform2f(this.qS,sx,sy);gl.uniform2f(this.qT,tx,ty);
    gl.bindBuffer(gl.ARRAY_BUFFER,this.b2);gl.bufferData(gl.ARRAY_BUFFER,p,gl.DYNAMIC_DRAW);
    gl.enableVertexAttribArray(this.qA);gl.vertexAttribPointer(this.qA,2,gl.FLOAT,false,0,0);
    gl.bindBuffer(gl.ARRAY_BUFFER,this.b3);gl.bufferData(gl.ARRAY_BUFFER,c,gl.DYNAMIC_DRAW);
    gl.enableVertexAttribArray(this.qB);gl.vertexAttribPointer(this.qB,4,gl.FLOAT,false,0,0);
    gl.drawArrays(gl.TRIANGLE_STRIP,0,n*2);
  }

  _candles(gl,vs,ve,n,ohlc,toX,toY,cW,sx,sy,tx,ty){
    const bw=Math.max(1,cW/n*0.7),ww=Math.max(0.5,bw*0.1),p=[],c=[];
    for(let i=0;i<n;i++){const r=this.data[vs+i],o=Number(r[ohlc.o]),h=Number(r[ohlc.h]),l=Number(r[ohlc.l]),cl=Number(r[ohlc.c]);
      const x=toX(vs+i),clr=cl>=o?UP_GL:DN_GL,bt=toY(Math.max(o,cl)),bb=toY(Math.min(o,cl));
      pushQ(p,c,x-ww,toY(h),x+ww,toY(l),clr);pushQ(p,c,x-bw/2,bt,x+bw/2,Math.max(bb,bt+1),clr);}
    this._batch(gl,p,c,sx,sy,tx,ty);
  }

  _ohlcBars(gl,vs,ve,n,ohlc,toX,toY,cW,sx,sy,tx,ty){
    const tw=Math.max(2,Math.min(cW/n*0.35,8)),p=[],c=[];
    for(let i=0;i<n;i++){const r=this.data[vs+i],o=Number(r[ohlc.o]),h=Number(r[ohlc.h]),l=Number(r[ohlc.l]),cl=Number(r[ohlc.c]);
      const x=toX(vs+i),clr=cl>=o?UP_GL:DN_GL;
      pushQ(p,c,x-0.5,toY(h),x+0.5,toY(l),clr);pushQ(p,c,x-tw,toY(o)-0.5,x,toY(o)+0.5,clr);pushQ(p,c,x,toY(cl)-0.5,x+tw,toY(cl)+0.5,clr);}
    this._batch(gl,p,c,sx,sy,tx,ty);
  }

  _volume(gl,vs,ve,n,vI,cI,toX,cw,ch,vH,sx,sy,tx,ty){
    let mx=0;for(let i=vs;i<ve;i++){const v=Number(this.data[i][vI])||0;if(v>mx)mx=v;}
    if(mx<=0)return;
    const cW=cw-ML-MR,bw=Math.max(1,cW/n*0.7),vBot=ch,p=[],c=[];
    for(let i=0;i<n;i++){const v=Number(this.data[vs+i][vI])||0,x=toX(vs+i),h=(v/mx)*(vH-15);
      const bull=i>0?Number(this.data[vs+i][cI])>=Number(this.data[vs+i-1][cI]):true;
      pushQ(p,c,x-bw/2,vBot-h,x+bw/2,vBot,bull?UP_VOL:DN_VOL);}
    this._batch(gl,p,c,sx,sy,tx,ty);
  }

  _batch(gl,p,c,sx,sy,tx,ty){
    if(!p.length)return;
    gl.useProgram(this.qP);gl.uniform2f(this.qS,sx,sy);gl.uniform2f(this.qT,tx,ty);
    gl.bindBuffer(gl.ARRAY_BUFFER,this.b2);gl.bufferData(gl.ARRAY_BUFFER,new Float32Array(p),gl.DYNAMIC_DRAW);
    gl.enableVertexAttribArray(this.qA);gl.vertexAttribPointer(this.qA,2,gl.FLOAT,false,0,0);
    gl.bindBuffer(gl.ARRAY_BUFFER,this.b3);gl.bufferData(gl.ARRAY_BUFFER,new Float32Array(c),gl.DYNAMIC_DRAW);
    gl.enableVertexAttribArray(this.qB);gl.vertexAttribPointer(this.qB,4,gl.FLOAT,false,0,0);
    gl.drawArrays(gl.TRIANGLES,0,p.length/2);
  }
}

// ── Icon button ─────────────────────────────────────────────────────────────
import { HomeIcon, HandIcon, ZoomIcon, HResizeIcon, VResizeIcon } from '../components/Icons';

function IconBtn({ icon: Icon, active, onClick, title }) {
  const [hover, setHover] = React.useState(false);
  return (
    <button
      onClick={onClick}
      title={title}
      onMouseEnter={() => setHover(true)}
      onMouseLeave={() => setHover(false)}
      style={{
        width: 26, height: 22, padding: 0,
        border: `1px solid ${theme.borderDark}`,
        borderRadius: 3,
        cursor: 'pointer',
        background: active
          ? theme.accentColor
          : (hover ? theme.widgetBgHover : theme.widgetBg),
        display: 'flex', alignItems: 'center', justifyContent: 'center',
        boxShadow: active
          ? 'inset 0 1px 2px rgba(0,0,0,0.3)'
          : 'inset 0 1px 0 rgba(255,255,255,0.06), 0 1px 0 rgba(0,0,0,0.4)',
        transition: 'background 0.1s',
      }}
    >
      <Icon color={active ? '#ffffff' : theme.textColor} />
    </button>
  );
}

// ── Zoom rect preview ───────────────────────────────────────────────────────
function drawZoomRect(canvas, r) {
  if (!canvas || !r) return;
  const ctx = canvas.getContext('2d');
  const dpr = window.devicePixelRatio || 1;
  const par = canvas.parentElement;
  if (!par) return;
  const cw = par.clientWidth, ch = par.clientHeight;
  canvas.width = cw * dpr; canvas.height = ch * dpr;
  canvas.style.width = cw + 'px'; canvas.style.height = ch + 'px';
  ctx.clearRect(0, 0, canvas.width, canvas.height);
  ctx.save(); ctx.scale(dpr, dpr);
  const x = Math.min(r.x1, r.x2), y = Math.min(r.y1, r.y2);
  const w = Math.abs(r.x2 - r.x1), h = Math.abs(r.y2 - r.y1);
  // Dim everything outside the rect
  ctx.fillStyle = 'rgba(0,0,0,0.4)';
  ctx.fillRect(0, 0, cw, ch);
  ctx.clearRect(x, y, w, h);
  // Rect border
  ctx.strokeStyle = theme.accentColor;
  ctx.lineWidth = 1.5;
  ctx.setLineDash([4, 3]);
  ctx.strokeRect(x, y, w, h);
  ctx.restore();
}

function clearCanvas(canvas) {
  if (!canvas) return;
  const ctx = canvas.getContext('2d');
  ctx.clearRect(0, 0, canvas.width, canvas.height);
}

// ── React Component ─────────────────────────────────────────────────────────
export default function ChartPanel({ fileStateKey, signalsKey, linesKey }) {
  const containerRef = useRef(null), glRef = useRef(null), overlayRef = useRef(null);
  const zoomRectRef = useRef(null); // third canvas for zoom preview
  const engineRef = useRef(null);
  const selectedFileArr = usePanelValue(fileStateKey, []);
  const chartStyle = usePanelValue('chart_style', 'Line');
  const signals = usePanelValue(signalsKey || '__no_signals__', null);
  const lines = usePanelValue(linesKey || '__no_lines__', null);
  const [chartData, setChartData] = useState(null);
  const [mode, setMode] = useState('pan');
  const dragRef = useRef(null);
  const rectRef = useRef(null);

  const filename = useMemo(() => {
    if (Array.isArray(selectedFileArr) && selectedFileArr.length > 0) return selectedFileArr[0];
    return typeof selectedFileArr === 'string' ? selectedFileArr : null;
  }, [selectedFileArr]);

  useEffect(() => {
    if (!glRef.current || !overlayRef.current) return;
    engineRef.current = new Engine(glRef.current, overlayRef.current);
    return () => { if (engineRef.current) engineRef.current.destroy(); };
  }, []);

  useEffect(() => {
    if (!filename) { setChartData(null); return; }
    let c = false;
    window.api.readParquet(filename).then(r => { if (!c) setChartData(r); }).catch(() => { if (!c) setChartData(null); });
    return () => { c = true; };
  }, [filename]);

  useEffect(() => {
    if (!engineRef.current) return;
    if (chartData?.columns && chartData?.data) engineRef.current.setData(chartData.data, chartData.columns);
    else engineRef.current.setData(null, null);
  }, [chartData]);

  useEffect(() => { if (engineRef.current) engineRef.current.setStyle(chartStyle || 'Line'); }, [chartStyle]);

  useEffect(() => { if (engineRef.current) engineRef.current.setSignals(signals); }, [signals]);

  useEffect(() => { if (engineRef.current) engineRef.current.setLines(lines); }, [lines]);

  useEffect(() => {
    const el = containerRef.current; if (!el) return;
    const obs = new ResizeObserver(() => { if (engineRef.current) engineRef.current.resize(); });
    obs.observe(el); return () => obs.disconnect();
  }, []);

  const onDown = useCallback(e => {
    const rect = containerRef.current?.getBoundingClientRect();
    if (!rect) return;
    const x = e.clientX - rect.left, y = e.clientY - rect.top;
    if (mode === 'zoom') {
      rectRef.current = { x1: x, y1: y, x2: x, y2: y, active: true };
    } else {
      dragRef.current = { x: e.clientX, y: e.clientY };
    }
  }, [mode]);

  const onMove = useCallback(e => {
    if (rectRef.current?.active && containerRef.current) {
      const rect = containerRef.current.getBoundingClientRect();
      rectRef.current.x2 = e.clientX - rect.left;
      rectRef.current.y2 = e.clientY - rect.top;
      drawZoomRect(zoomRectRef.current, rectRef.current);
      return;
    }
    if (!dragRef.current || !engineRef.current || !containerRef.current) return;
    const dx = e.clientX - dragRef.current.x, dy = e.clientY - dragRef.current.y;
    dragRef.current.x = e.clientX; dragRef.current.y = e.clientY;
    engineRef.current.pan(dx, dy, containerRef.current.clientWidth, containerRef.current.clientHeight);
  }, []);

  const onUp = useCallback(() => {
    if (rectRef.current?.active && engineRef.current && containerRef.current) {
      const r = rectRef.current;
      const w = Math.abs(r.x2 - r.x1), h = Math.abs(r.y2 - r.y1);
      if (w > 10 && h > 10) {
        engineRef.current.zoomToRect(r.x1, r.y1, r.x2, r.y2,
          containerRef.current.clientWidth, containerRef.current.clientHeight);
      }
      rectRef.current = null;
      clearCanvas(zoomRectRef.current);
    }
    dragRef.current = null;
  }, []);

  const onWheel = useCallback(e => {
    e.preventDefault();
    if (!engineRef.current || !containerRef.current) return;
    const rect = containerRef.current.getBoundingClientRect();
    const factor = e.deltaY > 0 ? 1.12 : 0.89;
    engineRef.current.zoom(factor, e.clientX - rect.left, e.clientY - rect.top,
      containerRef.current.clientWidth, containerRef.current.clientHeight);
  }, []);

  const doHome = useCallback(() => { if (engineRef.current) engineRef.current.home(); }, []);
  const doMode = useCallback(m => {
    setMode(m);
    if (engineRef.current) engineRef.current.setMode(m);
    rectRef.current = null;
    clearCanvas(zoomRectRef.current);
  }, []);

  const cursor = mode === 'pan' ? 'grab' : mode === 'zoom' ? 'crosshair' : 'default';

  return (
    <PanelContainer style={{ overflow: 'hidden' }}>
      <Row>
        <Dropdown options={CHART_STYLES} stateKey="chart_style" defaultValue="Line" />
        <div style={{ width: 1, height: 16, background: theme.dockBorderColor, margin: '0 4px' }} />
        <IconBtn icon={HomeIcon} onClick={doHome} title="Reset view" />
        <IconBtn icon={HandIcon} active={mode === 'pan'} onClick={() => doMode('pan')} title="Pan" />
        <IconBtn icon={ZoomIcon} active={mode === 'zoom'} onClick={() => doMode('zoom')} title="Zoom to rect" />
        <IconBtn icon={HResizeIcon} active={mode === 'hresize'} onClick={() => doMode('hresize')} title="Pan X only" />
        <IconBtn icon={VResizeIcon} active={mode === 'vresize'} onClick={() => doMode('vresize')} title="Pan Y only" />
      </Row>
      <div
        ref={containerRef}
        style={{ flex: 1, minHeight: 0, position: 'relative', cursor }}
        onMouseDown={onDown} onMouseMove={onMove} onMouseUp={onUp} onMouseLeave={onUp} onWheel={onWheel}
      >
        <canvas ref={glRef} style={{ position: 'absolute', top: 0, left: 0 }} />
        <canvas ref={overlayRef} style={{ position: 'absolute', top: 0, left: 0, pointerEvents: 'none' }} />
        <canvas ref={zoomRectRef} style={{ position: 'absolute', top: 0, left: 0, pointerEvents: 'none' }} />
      </div>
    </PanelContainer>
  );
}

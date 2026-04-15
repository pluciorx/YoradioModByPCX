// particle-network background — keep particles moving: stronger continuous noise + gentle flow field
(function(){
  // (snippet) create canvas at end of body and style it to sit behind UI
  const c = document.getElementById('yobg') || (function(){
    let x = document.createElement('canvas');
    x.id = 'yobg';
    // append as LAST child so it's less likely to overlap
    document.body.appendChild(x);
    return x;
  })();

  c.style.position = 'fixed';
  c.style.left = '0';
  c.style.top = '0';
  c.style.width = '100%';
  c.style.height = '100%';
  c.style.pointerEvents = 'none';
  c.style.zIndex = '0';

  // IMPORTANT: don't set a body background here (leave transparent) so canvas is visible behind content
  // document.body.style.background = 'transparent';  // optional if needed

  const ctx = c.getContext('2d');
  const dpr = Math.max(1, window.devicePixelRatio || 1);
  let W = 0, H = 0, particles = [], maxParticles = 160, connectDist = 180, neighbors = 4;
  let globalTime = 0;

  // palette from CSS theme vars (falls back to blue/grey scheme)
  function hexToRgbCsv(hex, fallback){
    if(!hex || typeof hex !== 'string') return fallback;
    const h = hex.trim();
    const m = h.match(/^#([0-9a-f]{3}|[0-9a-f]{6})$/i);
    if(!m) return fallback;
    let v = m[1];
    if(v.length === 3) v = v.split('').map(ch => ch + ch).join('');
    const n = parseInt(v, 16);
    const r = (n >> 16) & 255, g = (n >> 8) & 255, b = n & 255;
    return `${r},${g},${b}`;
  }

  function cssVar(name, fallback){
    const s = getComputedStyle(document.documentElement).getPropertyValue(name);
    return (s && s.trim()) ? s.trim() : fallback;
  }

  let ACCENT = '105,182,255';
  let ACCENT_DARK = '47,110,168';
  let BG_TOP = '11,17,27';
  let BG_BOT = '16,27,41';

  function refreshPalette(){
    ACCENT = hexToRgbCsv(cssVar('--accent-color', '#69b6ff'), '105,182,255');
    ACCENT_DARK = hexToRgbCsv(cssVar('--accent-dark', '#2f6ea8'), '47,110,168');
    BG_TOP = hexToRgbCsv(cssVar('--main-bg-color', '#0b111b'), '11,17,27');
    BG_BOT = hexToRgbCsv(cssVar('--odd-bg-color', '#111a27'), '16,27,41');
  }

  function rand(min, max){ return Math.random() * (max - min) + min; }

  function createParticle() {
    return {
      x: rand(0, W),
      y: rand(0, H),
      vx: rand(-40, 40),   // px/s
      vy: rand(-32, 32),   // px/s
      r: rand(1.1, 2.8),
      tw: rand(0.0, Math.PI * 2),
      tws: rand(0.6, 1.4)
    };
  }

  function initParticles(count){
    particles = [];
    for(let i=0;i<count;i++) particles.push(createParticle());
  }

  function adjustParticleCount(target){
    target = Math.max(130, Math.min(maxParticles, Math.floor(target)));
    const current = particles.length;
    if(current === 0){
      initParticles(target);
      return;
    }
    if(target > current){
      for(let i=0;i<target-current;i++) particles.push(createParticle());
    } else if(target < current){
      particles.splice(target, current-target);
    }
  }

  function resize(){
    W = innerWidth; H = innerHeight;
    c.style.width = W + 'px'; c.style.height = H + 'px';
    c.width = Math.round(W * dpr); c.height = Math.round(H * dpr);
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    const area = (W*H);
    const ideal = Math.min(maxParticles, Math.max(24, Math.floor(area / 90000 * 1.1)));
    adjustParticleCount(ideal);
    connectDist = Math.max(100, Math.min(300, Math.round(Math.max(W,H) * 0.18)));
  }
  addEventListener('resize', resize);

  function wrapDelta(dx, size){
    if(dx > size * 0.5) return dx - size;
    if(dx < -size * 0.5) return dx + size;
    return dx;
  }

  // motion params (tuned so motion persists)
  // tuned for a more subtle background
  let jitterAccel = 14.0;    // px/s^2 (reduced noise)
  let dampingRate = 0.8;     // per second (stronger damping for subtle motion)
  const maxSpeed = 160.0;    // px/s (lower max speed)
  const minSpeed = 6.0;      // px/s - gentle drift
  const sepDist = 36;        // px (separation radius)
  let sepForce = 80.0;       // px/s^2 (weaker separation)

  // gentle spatial flow field to keep movement distributed but subtle
  const flowAmp = 12.0;      // px/s acceleration scale from flow (reduced)
  const flowScale = 0.01;    // spatial frequency
  const flowTimeScale = 0.45;

  function step(dtSec){
    globalTime += dtSec;
    const damping = Math.exp(-dampingRate * dtSec);

    // separation (bounded linear)
    for (let i = 0; i < particles.length; i++){
      const a = particles[i];
      let ax = 0, ay = 0;
      for (let j = 0; j < particles.length; j++){
        if (i === j) continue;
        const b = particles[j];
        let dx = a.x - b.x, dy = a.y - b.y;
        dx = wrapDelta(dx, W);
        dy = wrapDelta(dy, H);
        const d2 = dx*dx + dy*dy;
        if(d2 === 0) continue;
        const d = Math.sqrt(d2);
        if(d < sepDist){
          const factor = (1 - d / sepDist); // 0..1
          const force = sepForce * factor;   // px/s^2
          ax += (dx / d) * force;
          ay += (dy / d) * force;
        }
      }
      a.vx += ax * dtSec;
      a.vy += ay * dtSec;
    }

    for(let p of particles){
      // random acceleration (noise)
      p.vx += (Math.random() - 0.5) * jitterAccel * dtSec;
      p.vy += (Math.random() - 0.5) * jitterAccel * dtSec * 0.9;

      // add smooth flow-field acceleration (space-time sinusoid)
      const fx = Math.cos((p.x + p.y) * flowScale + globalTime * flowTimeScale);
      const fy = Math.sin((p.y - p.x) * flowScale + globalTime * flowTimeScale * 0.9);
      p.vx += fx * flowAmp * dtSec;
      p.vy += fy * flowAmp * dtSec * 0.8;

      // damping (lighter)
      p.vx *= damping;
      p.vy *= damping;

      // clamp speed
      let s = Math.hypot(p.vx, p.vy);
      if(s > maxSpeed){
        const k = maxSpeed / s;
        p.vx *= k; p.vy *= k; s = maxSpeed;
      }

      // ensure minimum drift (larger min to avoid stillness)
      if(s < minSpeed){
        const ang = Math.random() * Math.PI * 2;
        p.vx += Math.cos(ang) * (minSpeed * 0.18);
        p.vy += Math.sin(ang) * (minSpeed * 0.18);
      }

      // integrate
      p.x += p.vx * dtSec;
      p.y += p.vy * dtSec;

      // smooth wrap
      if(p.x < -40) p.x = W + (p.x + 40);
      if(p.x > W + 40) p.x = p.x - (W + 40);
      if(p.y < -40) p.y = H + (p.y + 40);
      if(p.y > H + 40) p.y = p.y - (H + 40);
    }
  }

  function draw(){
    ctx.clearRect(0,0,W,H);
    const g = ctx.createLinearGradient(0,0,0,H);
    g.addColorStop(0, `rgba(${BG_TOP},0.90)`);
    g.addColorStop(1, `rgba(${BG_BOT},0.96)`);
    ctx.fillStyle = g;
    ctx.fillRect(0,0,W,H);

    // subtle radial bloom in upper-right for modern DAC feel
    const rg = ctx.createRadialGradient(W * 0.85, H * 0.08, 0, W * 0.85, H * 0.08, Math.max(W, H) * 0.75);
    rg.addColorStop(0, `rgba(${ACCENT},0.10)`);
    rg.addColorStop(1, `rgba(${ACCENT},0.0)`);
    ctx.fillStyle = rg;
    ctx.fillRect(0, 0, W, H);

    // glow layer
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    for(let i=0;i<particles.length;i++){
      const a = particles[i];
      let nearest = [];
      for(let j=0;j<particles.length;j++){
        if(i===j) continue;
        const b = particles[j];
        let dx = a.x - b.x, dy = a.y - b.y;
        dx = wrapDelta(dx, W); dy = wrapDelta(dy, H);
        const d2 = dx*dx + dy*dy;
        if(d2 > connectDist*connectDist) continue;
        nearest.push({idx:j, d2, dx, dy});
      }
      if(nearest.length>1) nearest.sort((p,q)=>p.d2-q.d2);
      const limit = Math.min(neighbors, nearest.length);
      for(let k=0;k<limit;k++){
        const nb = nearest[k];
        const b = particles[nb.idx];
        const dist = Math.sqrt(nb.d2);
        // subtler glow and smaller stroke
        const alpha = 0.18 * (1 - dist / connectDist);
        ctx.strokeStyle = `rgba(${ACCENT},${(alpha*0.82).toFixed(3)})`;
        ctx.lineWidth = 1.8;
        ctx.beginPath();
        const ax = a.x, ay = a.y;
        const bx = ax - nb.dx;
        const by = ay - nb.dy;
        const mx = (ax + bx) * 0.5 + ((ay - by) * 0.05);
        const my = (ay + by) * 0.5 + ((bx - ax) * 0.025);
        ctx.moveTo(ax, ay);
        ctx.quadraticCurveTo(mx, my, bx, by);
        ctx.stroke();
      }
    }
    ctx.restore();

    // crisp lines
    ctx.lineWidth = 1.8;
    for(let i=0;i<particles.length;i++){
      const a = particles[i];
      let nearest = [];
      for(let j=0;j<particles.length;j++){
        if(i===j) continue;
        const b = particles[j];
        let dx = a.x - b.x, dy = a.y - b.y;
        dx = wrapDelta(dx, W); dy = wrapDelta(dy, H);
        const d2 = dx*dx + dy*dy;
        if(d2 > connectDist*connectDist) continue;
        nearest.push({idx:j, d2, dx, dy});
      }
      if(nearest.length>1) nearest.sort((p,q)=>p.d2-q.d2);
      const limit = Math.min(neighbors, nearest.length);
      for(let k=0;k<limit;k++){
        const nb = nearest[k];
        const b = particles[nb.idx];
        const dist = Math.sqrt(nb.d2);
        const alpha = 0.20 * (1 - dist / connectDist);
        ctx.strokeStyle = `rgba(${ACCENT_DARK},${alpha.toFixed(3)})`;
        ctx.beginPath();
        const ax = a.x, ay = a.y;
        const bx = ax - nb.dx;
        const by = ay - nb.dy;
        const mx = (ax + bx) * 0.5 + ((ay - by) * 0.02);
        const my = (ay + by) * 0.5 + ((bx - ax) * 0.01);
        ctx.moveTo(ax, ay);
        ctx.quadraticCurveTo(mx, my, bx, by);
        ctx.stroke();
      }
    }

    // draw points
    for(let p of particles){
      p.tw += 0.02 * p.tws;
      const pulse = 0.82 + Math.sin(p.tw + globalTime * 1.1) * 0.18;
      ctx.beginPath();
      ctx.fillStyle = `rgba(${ACCENT},${Math.max(0.65, pulse).toFixed(3)})`;
      ctx.arc(p.x, p.y, p.r * (0.92 + pulse * 0.12), 0, Math.PI*2);
      ctx.fill();
      ctx.strokeStyle = `rgba(${BG_TOP},0.28)`;
      ctx.lineWidth = 0.6;
      ctx.stroke();
    }
  }

  // animation loop
  let last = performance.now();
  function loop(t){
    let dtMs = t - last;
    if(dtMs > 200) dtMs = 200;
    last = t;
    const dtSec = Math.max(0.001, dtMs / 1000);
    step(dtSec);
    draw();
    requestAnimationFrame(loop);
  }

  // API for tuning
  window.__bg_net = {
    setCount(n){ adjustParticleCount(Math.max(8, Math.min(400, n))); },
    setDist(d){ connectDist = d; },
    setNeighbors(n){ neighbors = Math.max(1, Math.min(20, n)); },
    setJitter(a){ jitterAccel = Number(a) || jitterAccel; },
    setDamping(r){ dampingRate = Number(r) || dampingRate; },
    setSepForce(f){ sepForce = Number(f) || sepForce; },
    regen(){ initParticles(particles.length); },
    getCount(){ return particles.length; }
  };

  refreshPalette();
  addEventListener('load', refreshPalette);
  resize();
  last = performance.now();
  requestAnimationFrame(loop);
})();
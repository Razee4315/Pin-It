/** Tiny synthesized pin/unpin feedback sound. */
export function playSound(pinned: boolean) {
  try {
    const ctx = new AudioContext();
    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.connect(gain);
    gain.connect(ctx.destination);
    osc.type = 'sine';
    // Pin = higher pitch, Unpin = lower pitch
    osc.frequency.value = pinned ? 880 : 660;
    gain.gain.value = 0.08;
    osc.start();
    gain.gain.exponentialRampToValueAtTime(0.001, ctx.currentTime + 0.12);
    osc.stop(ctx.currentTime + 0.12);
  } catch {
    // Audio not available, ignore
  }
}

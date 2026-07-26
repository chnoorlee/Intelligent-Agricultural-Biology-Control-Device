#ifndef HTML_PAGE_H
#define HTML_PAGE_H

const char CONTROL_PAGE[] PROGMEM = R"=====(

<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Drone Control</title>
<meta name="theme-color" content="#0c0a08">
<style>
:root {
  --bg0: #080706;
  --bg1: #12100e;
  --surface: #141210;
  --elevated: #1a1612;
  --border: rgba(255, 140, 0, 0.22);
  --border-strong: rgba(255, 140, 0, 0.42);
  --text: #ffe8d4;
  --text-dim: #c4a882;
  --accent: #ff8c22;
  --accent-soft: rgba(255, 140, 34, 0.14);
  --danger: #e83838;
  --radius: 14px;
  --radius-sm: 10px;
  --shadow: 0 8px 32px rgba(0, 0, 0, 0.45);
  --glow: 0 0 20px rgba(255, 120, 40, 0.12);
  --font: "Segoe UI", system-ui, -apple-system, Roboto, Ubuntu, sans-serif;
  --mono: ui-monospace, "Cascadia Code", "SF Mono", Consolas, monospace;
  --joy-size: 184px;
  --knob: 62px;
  --knob-r: 31px;
}

* { box-sizing: border-box; }

html { -webkit-tap-highlight-color: transparent; }

body {
  margin: 0;
  padding: 0 0 28px;
  font-family: var(--font);
  font-size: 15px;
  line-height: 1.45;
  color: var(--text);
  background: var(--bg0);
  background-image:
    radial-gradient(ellipse 120% 80% at 50% -20%, rgba(255, 100, 30, 0.09), transparent 55%),
    radial-gradient(ellipse 80% 50% at 100% 100%, rgba(255, 80, 0, 0.05), transparent 45%);
  color-scheme: dark;
}

h2, h3, h4, p { margin: 0; }

.wrap {
  max-width: 960px;
  margin: 0 auto;
  padding: 16px 18px;
}

.layout-root {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.tab-pages {
  display: flex;
  flex-direction: column;
  gap: 0;
  min-height: 40vh;
}

.page {
  display: none;
  margin-bottom: 0;
}

.page.page-active {
  display: block;
}

.app-header {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  justify-content: space-between;
  gap: 16px;
  padding: 18px 20px;
  margin-bottom: 0;
  background: linear-gradient(165deg, var(--elevated) 0%, var(--surface) 100%);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  box-shadow: var(--shadow), var(--glow);
}

.app-title {
  font-size: 1.35rem;
  font-weight: 750;
  letter-spacing: -0.02em;
  background: linear-gradient(90deg, #fff2e0, var(--accent));
  -webkit-background-clip: text;
  background-clip: text;
  color: transparent;
}

.subtitle {
  margin-top: 6px;
  font-size: 0.88rem;
  max-width: 42ch;
}

.badge-row {
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
  align-items: center;
}

.badge {
  display: inline-flex;
  align-items: center;
  padding: 7px 12px;
  border-radius: 999px;
  font-size: 12px;
  font-weight: 700;
  letter-spacing: 0.02em;
  transition: transform 0.12s ease, box-shadow 0.12s ease;
}

.badge.ok {
  color: #ffd699;
  background: rgba(255, 120, 40, 0.12);
  border: 1px solid var(--border-strong);
  box-shadow: 0 0 14px rgba(255, 140, 0, 0.2);
}

.badge.warn {
  color: #ffaa99;
  background: rgba(255, 60, 40, 0.1);
  border: 1px solid rgba(255, 90, 60, 0.45);
}

.badge.danger {
  color: #fff2f2;
  background: rgba(232, 56, 56, 0.18);
  border: 1px solid rgba(255, 90, 90, 0.55);
  box-shadow: 0 0 14px rgba(232, 56, 56, 0.22);
}

.muted { color: var(--text-dim); font-size: 0.88rem; }

.tabs {
  display: grid;
  grid-template-columns: repeat(5, minmax(0, 1fr));
  gap: 8px;
  margin-bottom: 12px;
  margin-top: 4px;
}

.tab {
  padding: 11px 8px;
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: var(--radius-sm);
  text-align: center;
  font-size: 0.82rem;
  font-weight: 700;
  user-select: none;
  color: #e8b88a;
  cursor: pointer;
  transition: background 0.15s ease, border-color 0.15s ease, color 0.15s ease, box-shadow 0.15s ease;
}

.tab:hover {
  background: var(--elevated);
  border-color: var(--border-strong);
  color: #ffd4a8;
}

.tab:focus-visible {
  outline: 2px solid var(--accent);
  outline-offset: 2px;
}

.tab.activeTab {
  background: linear-gradient(180deg, rgba(255, 120, 40, 0.2), rgba(40, 28, 14, 0.95));
  border-color: var(--border-strong);
  color: #fff0dd;
  box-shadow: inset 0 1px 0 rgba(255, 200, 120, 0.12), 0 4px 20px rgba(255, 100, 0, 0.12);
}

.card {
  background: linear-gradient(165deg, #161411 0%, #0e0c0a 100%);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  padding: 18px 20px;
  margin-top: 8px;
  margin-bottom: 0;
  box-shadow: var(--shadow), var(--glow);
}

.page-title {
  font-size: 1.15rem;
  font-weight: 750;
  margin-bottom: 16px;
  padding-bottom: 10px;
  border-bottom: 1px solid var(--border);
  letter-spacing: -0.015em;
}

.section-title {
  font-size: 0.95rem;
  font-weight: 700;
  margin: 0 0 10px 0;
  color: #ffd4b8;
}

.block-gap { margin-top: 18px; }

.status-grid {
  display: grid;
  grid-template-columns: repeat(5, minmax(0, 1fr));
  gap: 10px;
  margin-bottom: 16px;
}

.status-tile {
  min-width: 0;
  padding: 10px 12px;
  border-radius: var(--radius-sm);
  border: 1px solid rgba(255, 140, 60, 0.14);
  background: rgba(0, 0, 0, 0.22);
}

.status-label {
  display: block;
  font-size: 0.68rem;
  font-weight: 800;
  text-transform: uppercase;
  letter-spacing: 0.08em;
  color: var(--text-dim);
  margin-bottom: 4px;
}

.status-value {
  display: block;
  font-family: var(--mono);
  font-size: 0.92rem;
  font-weight: 700;
  color: #ffe4cc;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.workflow-intro {
  display: grid;
  grid-template-columns: minmax(0, 1fr) auto;
  gap: 14px;
  align-items: center;
  margin-bottom: 14px;
}

.setup-step {
  margin-top: 14px;
  padding: 16px;
  background: rgba(0, 0, 0, 0.18);
  border: 1px solid rgba(255, 140, 60, 0.12);
  border-radius: var(--radius-sm);
}

.setup-step-head {
  display: flex;
  align-items: flex-start;
  gap: 12px;
  margin-bottom: 12px;
}

.step-num {
  flex: 0 0 auto;
  display: inline-flex;
  width: 30px;
  height: 30px;
  align-items: center;
  justify-content: center;
  border-radius: 999px;
  color: #1a0f00;
  background: linear-gradient(180deg, #ffc266, var(--accent));
  font-weight: 900;
  font-size: 0.82rem;
}

.setup-step-title {
  font-size: 0.98rem;
  font-weight: 800;
  color: #ffd4b8;
}

.field-help {
  margin-top: 4px;
  color: var(--text-dim);
  font-size: 0.78rem;
  line-height: 1.35;
}

.danger-note {
  padding: 12px 14px;
  border-radius: var(--radius-sm);
  border: 1px solid rgba(255, 80, 80, 0.4);
  background: rgba(232, 56, 56, 0.12);
  color: #ffd1d1;
  font-weight: 700;
}

.info-note {
  padding: 12px 14px;
  border-radius: var(--radius-sm);
  border: 1px solid rgba(255, 140, 60, 0.16);
  background: rgba(255, 140, 40, 0.06);
  color: var(--text-dim);
}

.guide-list {
  margin: 10px 0 0;
  padding-left: 18px;
  color: var(--text-dim);
  font-size: 0.88rem;
}

.guide-list li { margin: 6px 0; }

.flight-stack {
  display: flex;
  flex-direction: column;
  gap: 20px;
}

.flight-sticks {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 16px;
  align-items: stretch;
}

.stick-panel {
  display: flex;
  flex-direction: column;
  align-items: center;
  text-align: center;
  padding: 14px 12px 16px;
  background: rgba(0, 0, 0, 0.2);
  border: 1px solid rgba(255, 140, 0, 0.12);
  border-radius: var(--radius-sm);
  min-height: 0;
}

.stick-panel h4 {
  font-size: 0.82rem;
  font-weight: 700;
  text-transform: uppercase;
  letter-spacing: 0.06em;
  color: var(--text-dim);
  margin-bottom: 10px;
}

.joystick {
  width: var(--joy-size);
  height: var(--joy-size);
  margin: 8px auto;
  background:
    radial-gradient(circle at 35% 30%, rgba(255, 160, 80, 0.08), transparent 45%),
    radial-gradient(circle at center, #1c1410, #0a0806);
  border-radius: 50%;
  border: 1px solid rgba(255, 120, 40, 0.35);
  touch-action: none;
  position: relative;
  box-shadow: inset 0 2px 24px rgba(0, 0, 0, 0.55), 0 4px 24px rgba(255, 80, 0, 0.08);
}

.joystick::after {
  content: "";
  position: absolute;
  inset: 12%;
  border-radius: 50%;
  border: 1px dashed rgba(255, 140, 60, 0.12);
  pointer-events: none;
}

.knob {
  width: var(--knob);
  height: var(--knob);
  background: radial-gradient(circle at 30% 25%, #ffb060, #e85a00 55%, #a83800);
  border: 2px solid rgba(255, 220, 180, 0.55);
  border-radius: 50%;
  position: absolute;
  top: calc(50% - var(--knob-r));
  left: calc(50% - var(--knob-r));
  box-shadow: 0 4px 20px rgba(255, 100, 0, 0.45), inset 0 2px 6px rgba(255, 255, 255, 0.25);
  transition: box-shadow 0.15s ease;
}

.valueRow {
  display: flex;
  justify-content: center;
  gap: 8px;
  flex-wrap: wrap;
  margin-top: 10px;
}

.chip {
  background: rgba(255, 100, 40, 0.06);
  border: 1px solid rgba(255, 140, 60, 0.2);
  border-radius: 999px;
  padding: 6px 12px;
  font-size: 12px;
  font-weight: 600;
  color: #ffd8b8;
}

.chip span { font-variant-numeric: tabular-nums; font-family: var(--mono); font-weight: 700; }

.panel {
  background: rgba(0, 0, 0, 0.22);
  border: 1px solid var(--border);
  border-radius: var(--radius-sm);
  padding: 14px 16px;
  margin-top: 0;
}

.panel .muted { margin-top: 8px; }

.flight-idle .section-title { margin-top: 0; }

.action-bar {
  display: flex;
  gap: 10px;
  justify-content: center;
  flex-wrap: wrap;
  padding-top: 18px;
  margin-top: 4px;
  border-top: 1px solid var(--border);
}

.actions {
  display: flex;
  gap: 10px;
  justify-content: center;
  flex-wrap: wrap;
}

button {
  padding: 12px 18px;
  border: 0;
  border-radius: var(--radius-sm);
  font-family: inherit;
  font-size: 13px;
  font-weight: 800;
  letter-spacing: 0.04em;
  text-transform: uppercase;
  cursor: pointer;
  transition: transform 0.1s ease, filter 0.12s ease, box-shadow 0.12s ease;
}

button:hover { filter: brightness(1.06); }
button:active { transform: scale(0.97); }
button:focus-visible {
  outline: 2px solid var(--accent);
  outline-offset: 2px;
}

.btnArm {
  color: #1a0f00;
  background: linear-gradient(180deg, #ffc266, var(--accent));
  box-shadow: 0 4px 18px rgba(255, 130, 40, 0.35);
}

.btnDisarm {
  color: #2a1008;
  background: linear-gradient(180deg, #ff9a66, #e84d18);
  box-shadow: 0 4px 16px rgba(255, 80, 40, 0.28);
}

.btnEstop {
  color: #fff8f8;
  background: linear-gradient(180deg, #ff6a6a, #c41818);
  border: 1px solid rgba(255, 180, 180, 0.35);
  box-shadow: 0 4px 22px rgba(255, 40, 40, 0.35);
}

.btnCal {
  color: #fff2e6;
  background: linear-gradient(180deg, #4a4540, #2a2622);
  border: 1px solid rgba(255, 160, 100, 0.25);
  box-shadow: 0 2px 12px rgba(0, 0, 0, 0.35);
}

.btnGhost, .presetRow button {
  color: #ffd8b8;
  background: rgba(255, 120, 40, 0.08);
  border: 1px solid var(--border);
  box-shadow: none;
  text-transform: none;
  font-weight: 700;
  letter-spacing: 0;
}

.presetRow {
  display: flex;
  gap: 10px;
  flex-wrap: wrap;
  margin-top: 14px;
}

.presetRow button {
  padding: 9px 14px;
  font-size: 13px;
}

.telem-panel .kv {
  display: grid;
  grid-template-columns: minmax(100px, 34%) 1fr;
  gap: 12px;
  padding: 10px 4px;
  border-bottom: 1px solid rgba(255, 140, 60, 0.1);
  align-items: baseline;
}

.telem-panel .kv:last-child { border-bottom: 0; }

.telem-split {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 8px 28px;
  margin-top: 4px;
}

.telem-col-title {
  font-size: 0.72rem;
  font-weight: 800;
  text-transform: uppercase;
  letter-spacing: 0.1em;
  color: var(--text-dim);
  margin: 4px 0 10px 0;
  padding-bottom: 6px;
  border-bottom: 1px solid rgba(255, 140, 60, 0.15);
}

.telem-col .kv:first-of-type {
  padding-top: 0;
}

.telem-panel .kv > div:first-child {
  font-size: 0.82rem;
  font-weight: 600;
  color: var(--text-dim);
  text-transform: uppercase;
  letter-spacing: 0.04em;
}

.telem-panel .kv > div:last-child {
  font-family: var(--mono);
  font-size: 0.95rem;
  font-weight: 600;
  color: #ffe4cc;
  text-align: right;
  word-break: break-all;
}

.hint {
  font-size: 0.84rem;
  color: var(--text-dim);
  margin-top: 12px;
  line-height: 1.5;
}

.cal-status {
  min-height: 1.4em;
  margin-top: 10px;
  text-align: center;
  font-size: 0.9rem;
}

.cfg-status {
  margin-top: 10px;
  padding: 10px 12px;
  border-radius: var(--radius-sm);
  background: rgba(255, 140, 40, 0.06);
  border: 1px solid rgba(255, 140, 60, 0.15);
  font-size: 0.88rem;
  min-height: 2.6em;
}

.motorBox {
  background: rgba(0, 0, 0, 0.25);
  padding: 12px 14px;
  margin: 12px 0;
  border-radius: var(--radius-sm);
  border: 1px solid rgba(255, 140, 60, 0.14);
}

.motorBox:first-of-type { margin-top: 0; }

.pid-row .pid-head {
  display: flex;
  justify-content: space-between;
  align-items: flex-start;
  gap: 12px;
  margin-bottom: 10px;
}

.pid-row .pid-head span:first-child {
  font-weight: 800;
  font-size: 0.95rem;
  color: var(--accent);
}

.pid-row .pid-head .pid-val {
  font-family: var(--mono);
  font-size: 0.88rem;
  color: #ffd8b8;
  font-weight: 600;
  flex: 0 0 auto;
  padding: 4px 8px;
  border-radius: 999px;
  background: rgba(255, 140, 40, 0.08);
  border: 1px solid rgba(255, 140, 60, 0.16);
}

.pid-layout {
  display: grid;
  grid-template-columns: minmax(240px, 320px) 1fr;
  gap: 16px;
  align-items: start;
}

.pid-guidance {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.pid-guidance .motorBox {
  margin: 0;
}

.pid-tune-label {
  display: flex;
  flex-direction: column;
  gap: 2px;
  min-width: 0;
  line-height: 1.25;
}

.pid-tune-label small {
  color: var(--text-dim);
  font-size: 0.76rem;
  font-weight: 600;
}

.motor-row {
  border-left: 3px solid rgba(255, 120, 40, 0.45);
  padding-left: 12px;
}

.motor-row.m2 { border-left-color: rgba(100, 200, 255, 0.5); }
.motor-row.m3 { border-left-color: rgba(180, 255, 120, 0.45); }
.motor-row.m4 { border-left-color: rgba(220, 140, 255, 0.5); }

.motor-row input[type="range"] { margin-top: 4px; }

input[type="range"] {
  width: 100%;
  height: 8px;
  accent-color: var(--accent);
  cursor: pointer;
}

.settings-block {
  margin-top: 8px;
  padding: 14px 16px;
  background: rgba(0, 0, 0, 0.18);
  border: 1px solid rgba(255, 140, 60, 0.1);
  border-radius: var(--radius-sm);
}

.settings-block .settings-section-title:first-child {
  margin-top: 0;
}

.settingsGrid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 12px;
  margin-top: 8px;
}

.settingsGrid .motorBox,
.settings-block > .motorBox {
  margin: 0;
}

.pid-split {
  display: grid;
  grid-template-columns: minmax(220px, 300px) 1fr;
  gap: 16px;
  align-items: start;
  margin-top: 4px;
}

.pid-aside {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.pid-aside .motorBox {
  margin: 0;
}

.pid-tune .motorBox {
  margin: 10px 0;
}

.pid-tune .motorBox:first-child {
  margin-top: 0;
}

.pid-tune .presetRow {
  margin-top: 8px;
}

.motors-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 12px;
  margin-top: 8px;
}

.motors-grid .motorBox {
  margin: 0;
}

.motor-map {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 12px;
  max-width: 620px;
  margin: 14px auto 0;
}

.motor-card {
  position: relative;
  min-height: 140px;
  display: flex;
  flex-direction: column;
  gap: 10px;
  justify-content: space-between;
}

.motor-card strong {
  display: block;
  font-size: 1rem;
  color: #ffd8b8;
}

.motor-card .motor-pos {
  color: var(--text-dim);
  font-size: 0.82rem;
  text-transform: uppercase;
  letter-spacing: 0.05em;
}

.motor-actions {
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
  align-items: center;
}

.motor-actions button {
  padding: 9px 12px;
}

.motor-stop-row {
  display: flex;
  flex-wrap: wrap;
  gap: 10px;
  justify-content: center;
  margin-top: 14px;
}

.settings-section-title {
  font-size: 0.78rem;
  font-weight: 800;
  text-transform: uppercase;
  letter-spacing: 0.08em;
  color: var(--text-dim);
  margin: 0 0 10px 0;
}

.settings-block + .settings-block {
  margin-top: 16px;
}

#settings > .settings-block:first-of-type {
  margin-top: 14px;
}

.field label {
  font-size: 0.78rem;
  font-weight: 700;
  color: #d4b896;
  margin-bottom: 6px;
  display: block;
  text-transform: uppercase;
  letter-spacing: 0.04em;
}

.field input, .field select {
  width: 100%;
  min-height: 44px;
  padding: 10px 12px;
  border-radius: var(--radius-sm);
  border: 1px solid rgba(255, 140, 60, 0.2);
  background: #0f0d0b;
  color: var(--text);
  font-family: inherit;
  font-size: 14px;
}

.field input:focus, .field select:focus {
  border-color: var(--border-strong);
  outline: none;
  box-shadow: 0 0 0 3px var(--accent-soft);
}

.check-row {
  display: flex;
  align-items: center;
  gap: 10px;
  cursor: pointer;
  font-size: 0.9rem;
}

.check-row input { width: 18px; height: 18px; accent-color: var(--accent); }

.tabs::-webkit-scrollbar {
  height: 6px;
}

.tabs::-webkit-scrollbar-thumb {
  background: rgba(255, 140, 60, 0.28);
  border-radius: 999px;
}

.pid-check {
  display: flex;
  align-items: flex-start;
  gap: 12px;
  cursor: pointer;
  font-weight: 700;
  font-size: 0.95rem;
}

.pid-check input {
  width: 20px;
  height: 20px;
  margin-top: 2px;
  flex-shrink: 0;
  accent-color: var(--accent);
}

@media (max-width: 820px) {
  .pid-split,
  .pid-layout {
    grid-template-columns: 1fr;
  }
  .status-grid {
    grid-template-columns: repeat(2, minmax(0, 1fr));
  }
}

@media (max-width: 760px) {
  .wrap { padding: 12px; }
  .tabs {
    display: flex;
    overflow-x: auto;
    padding-bottom: 4px;
    scroll-snap-type: x proximity;
  }
  .tab {
    flex: 0 0 auto;
    min-width: 128px;
    min-height: 44px;
    scroll-snap-align: start;
  }
  .flight-sticks { grid-template-columns: 1fr; }
  .telem-split { grid-template-columns: 1fr; }
  .motors-grid { grid-template-columns: 1fr; }
  .settingsGrid { grid-template-columns: 1fr; }
  .workflow-intro { grid-template-columns: 1fr; }
  .motor-map { grid-template-columns: 1fr; }
  button { min-height: 44px; }
  :root { --joy-size: 168px; }
}

@media (max-width: 460px) {
  .status-grid { grid-template-columns: 1fr; }
  .card { padding: 15px; }
  .setup-step { padding: 14px; }
}
</style>
</head>
<body>

<div class="wrap layout-root">
  <header class="app-header">
    <div class="brand">
      <h2 class="app-title">Drone Control</h2>
      <p class="subtitle muted">WebSocket link to the drone. Keep props off until you are ready to fly.</p>
    </div>
    <div class="badge-row">
      <span id="wsState" class="badge warn">WS Offline</span>
      <span id="armState" class="badge warn">DISARMED</span>
    </div>
  </header>

  <nav class="tabs" aria-label="Main tabs">
    <div id="tabFlight" class="tab activeTab" onclick="switchTab('flight', this)">Flight</div>
    <div id="tabTelem" class="tab" onclick="switchTab('telem', this)">Telemetry</div>
    <div id="tabPid" class="tab" onclick="switchTab('pid', this)">PID</div>
    <div id="tabMotors" class="tab" onclick="switchTab('motors', this)">Motor Test</div>
    <div id="tabSettings" class="tab" onclick="switchTab('settings', this)">Settings</div>
  </nav>

  <main class="tab-pages">

<div id="flight" class="page card page-active">
  <h3 class="page-title">Flight</h3>
  <div class="flight-stack">
  <div class="flight-sticks">
    <div class="stick-panel">
      <h4>Yaw / Throttle</h4>
      <div id="joyLeft" class="joystick"><div class="knob"></div></div>
      <div class="valueRow">
        <span class="chip">Yaw <span id="v_yaw">0.00</span></span>
        <span class="chip">Thr <span id="v_throttle">0%</span></span>
      </div>
    </div>

    <div class="stick-panel">
      <h4>Roll / Pitch</h4>
      <div id="joyRight" class="joystick"><div class="knob"></div></div>
      <div class="valueRow">
        <span class="chip">Roll <span id="v_roll">0.00</span></span>
        <span class="chip">Pitch <span id="v_pitch">0.00</span></span>
      </div>
    </div>
  </div>

  <div class="panel flight-idle">
    <div class="section-title">Armed startup / idle thrust</div>
    <p class="muted">Minimum throttle at full stick-down after ARM. Keep at 0% until you want props to spin at idle.</p>
    <input type="range" id="idleThr" min="0" max="40" step="1" value="0" oninput="sendIdleThr()" aria-label="Idle thrust percent">
    <div class="muted" style="margin-top:10px;">Setting <strong><span id="idleThr_val">0</span>%</strong>, about <span id="idleThr_pwm">0</span> / 255 PWM at zero stick</div>
    <div class="section-title" style="margin-top:16px;">Throttle command</div>
    <p class="muted">This slider sets the throttle value used when arming. Move it low before pressing ARM.</p>
    <input type="range" id="throttleCmd" min="0" max="100" step="1" value="0" oninput="setThrottleFromSlider()" aria-label="Throttle command">
    <div class="muted" style="margin-top:10px;">Command <strong><span id="throttleCmd_val">0%</span></strong> (0% low, 100% full)</div>
  </div>

  <div class="action-bar">
    <button class="btnArm" onclick="arm()">ARM</button>
    <button class="btnDisarm" onclick="disarm()">DISARM</button>
    <button class="btnEstop" onclick="estop()">E-STOP</button>
  </div>
  </div>
</div>

<!-- TELEMETRY TAB -->
<div id="telem" class="page card telem-panel">
  <h3 class="page-title">Telemetry</h3>
  <div class="telem-split">
    <div class="telem-col">
      <div class="telem-col-title">Attitude &amp; outputs</div>
      <div class="kv"><div>Roll</div><div><span id="t_roll">0</span></div></div>
      <div class="kv"><div>Pitch</div><div><span id="t_pitch">0</span></div></div>
      <div class="kv"><div>Yaw</div><div><span id="t_yaw">0</span></div></div>
      <div class="kv"><div>Motors</div><div><span id="t_motors">0,0,0,0</span></div></div>
      <div class="kv"><div>Arm</div><div><span id="t_arm">0</span></div></div>
      <div class="kv"><div>Calibrated</div><div><span id="t_cal">--</span></div></div>
      <div class="kv"><div>Failsafe</div><div><span id="t_failsafe">--</span></div></div>
      <div class="kv"><div>Status</div><div><span id="t_armReason">--</span></div></div>
    </div>
    <div class="telem-col">
      <div class="telem-col-title">Tuning &amp; link</div>
      <div class="kv"><div>Kp</div><div><span id="t_kp">0.00</span></div></div>
      <div class="kv"><div>Ki</div><div><span id="t_ki">0.00</span></div></div>
      <div class="kv"><div>Kd</div><div><span id="t_kd">0.00</span></div></div>
      <div class="kv"><div>PID on</div><div><span id="t_pidEn">--</span></div></div>
      <div class="kv"><div>Idle thrust %</div><div><span id="t_idle">--</span></div></div>
      <div class="kv"><div>Loop Hz</div><div><span id="t_loopHz">0.0</span></div></div>
      <div class="kv"><div>Link age</div><div><span id="t_linkAge">--</span></div></div>
      <div class="kv"><div>PWM max</div><div><span id="t_pwmMax">255</span></div></div>
      <div class="kv"><div>Packets</div><div><span id="t_pkt">0</span></div></div>
      <div class="kv"><div>Pkt Miss</div><div><span id="t_pktMiss">0</span></div></div>
    </div>
  </div>
</div>

<!-- PID TAB -->
<div id="pid" class="page card">
  <h3 class="page-title">PID &amp; sensors</h3>
  <div class="status-grid">
    <div class="status-tile"><span class="status-label">Armed</span><span class="status-value" id="s_arm">--</span></div>
    <div class="status-tile"><span class="status-label">Calibrated</span><span class="status-value" id="s_cal">--</span></div>
    <div class="status-tile"><span class="status-label">Failsafe</span><span class="status-value" id="s_failsafe">--</span></div>
    <div class="status-tile"><span class="status-label">Link age</span><span class="status-value" id="s_linkAge">--</span></div>
    <div class="status-tile"><span class="status-label">Loop Hz</span><span class="status-value" id="s_loopHz">--</span></div>
  </div>
  <div class="pid-layout">
    <aside class="pid-guidance">
      <div class="motorBox">
        <div class="section-title">IMU calibration</div>
        <button type="button" class="btnCal" onclick="calibrateImu()">Calibrate IMU</button>
        <p class="muted cal-status" id="calStatus" role="status"></p>
      </div>
      <div class="motorBox">
        <label class="pid-check" for="pidEnable">
          <input type="checkbox" id="pidEnable" checked onchange="sendPidEnable()">
          <span>Enable stabilization PID<br><span class="muted" style="font-weight:500;font-size:0.88rem;">Off = throttle + stick mix only (no angle hold).</span></span>
        </label>
      </div>
      <div class="motorBox">
        <div class="section-title">Tuning steps</div>
        <ol class="guide-list">
          <li>Keep propellers off until motor order and failsafe are checked.</li>
          <li>Raise P response until it feels firm or lightly oscillates.</li>
          <li>Add D damping to calm bounce and oscillation.</li>
          <li>Add a small I value only to correct slow drift.</li>
          <li>Copy live PID to saved fields, then save Settings.</li>
        </ol>
      </div>
    </aside>
    <div class="pid-tune">
      <p class="info-note">Live PID sliders send immediate `PID:` commands but do not persist by themselves. Use the copy button below, then save Settings.</p>
      <div class="motorBox pid-row">
        <div class="pid-head"><span class="pid-tune-label">P - Response<small>Return strength to target angle.</small></span><span class="pid-val" id="kp_val">0.00</span></div>
        <input type="range" min="0" max="10" step="0.01" value="2.00" id="kp" oninput="updatePID()" aria-label="P response gain">
      </div>
      <div class="motorBox pid-row">
        <div class="pid-head"><span class="pid-tune-label">I - Drift<small>Slow correction for steady drift.</small></span><span class="pid-val" id="ki_val">0.00</span></div>
        <input type="range" min="0" max="1" step="0.01" value="0.04" id="ki" oninput="updatePID()" aria-label="I drift correction gain">
      </div>
      <div class="motorBox pid-row">
        <div class="pid-head"><span class="pid-tune-label">D - Damping<small>Reduces bounce and oscillation.</small></span><span class="pid-val" id="kd_val">0.00</span></div>
        <input type="range" min="0" max="10" step="0.01" value="1.00" id="kd" oninput="updatePID()" aria-label="D damping gain">
      </div>
      <div class="presetRow">
        <button type="button" class="btnGhost" onclick="applyPreset('indoor')">Indoor</button>
        <button type="button" class="btnGhost" onclick="applyPreset('stable')">Stable</button>
        <button type="button" class="btnGhost" onclick="applyPreset('aggressive')">Aggressive</button>
        <button type="button" class="btnArm" onclick="copyLivePidToSaved()">Copy live PID to saved</button>
      </div>
    </div>
  </div>
</div>

<!-- SETTINGS TAB -->
<div id="settings" class="page card">
  <h3 class="page-title">Settings</h3>
  <div class="workflow-intro">
    <p class="muted">Follow these setup steps from top to bottom. Values are stored in ESP32 flash, and hardware mapping changes need a restart before they take effect.</p>
    <button type="button" class="btnGhost" onclick="loadSettingsForm()">Reload saved values</button>
  </div>
  <p class="cfg-status muted" id="cfg_save_status"></p>

  <div class="settings-block setup-step">
    <div class="setup-step-head">
      <span class="step-num">1</span>
      <div>
        <div class="setup-step-title">Network</div>
        <p class="field-help">Set the Wi-Fi network the drone joins after boot. Password can stay blank to keep the saved value.</p>
      </div>
    </div>
    <div class="settingsGrid">
      <div class="field motorBox"><label for="cfg_ssid">Wi-Fi SSID</label><input type="text" id="cfg_ssid" maxlength="32" autocomplete="off"><p class="field-help">Required. Up to 32 characters.</p></div>
      <div class="field motorBox"><label for="cfg_pass">Wi-Fi password</label><input type="password" id="cfg_pass" maxlength="64" autocomplete="new-password" placeholder="Leave blank to keep"><p class="field-help">Leave blank unless changing the password.</p></div>
    </div>
    <div class="motorBox" style="margin-top:12px;">
      <p class="muted" id="cfg_pass_note"></p>
      <label class="check-row" style="margin-top:10px;"><input type="checkbox" id="cfg_clear_pw"> Clear saved password (open network)</label>
    </div>
  </div>

  <div class="settings-block setup-step">
    <div class="setup-step-head">
      <span class="step-num">2</span>
      <div>
        <div class="setup-step-title">Hardware Mapping</div>
        <p class="field-help">Map each physical motor output. Save and restart after changing pins.</p>
      </div>
    </div>
    <div class="settingsGrid">
      <div class="field motorBox"><label for="cfg_mfl">Front left (M1)</label><input type="number" id="cfg_mfl" min="0" max="39" step="1"><p class="field-help">Quad X front-left motor output.</p></div>
      <div class="field motorBox"><label for="cfg_mfr">Front right (M2)</label><input type="number" id="cfg_mfr" min="0" max="39" step="1"><p class="field-help">Quad X front-right motor output.</p></div>
      <div class="field motorBox"><label for="cfg_mbl">Back left (M3)</label><input type="number" id="cfg_mbl" min="0" max="39" step="1"><p class="field-help">Quad X back-left motor output.</p></div>
      <div class="field motorBox"><label for="cfg_mbr">Back right (M4)</label><input type="number" id="cfg_mbr" min="0" max="39" step="1"><p class="field-help">Quad X back-right motor output.</p></div>
    </div>
  </div>

  <div class="settings-block setup-step">
    <div class="setup-step-head">
      <span class="step-num">3</span>
      <div>
        <div class="setup-step-title">Sensor And PWM</div>
        <p class="field-help">Tune the motor signal and MPU6050 bus. PWM frequency, resolution, and I2C pin changes require restart.</p>
      </div>
    </div>
    <div class="settingsGrid">
      <div class="field motorBox"><label for="cfg_pwmf">PWM frequency (Hz)</label><input type="number" id="cfg_pwmf" min="1000" max="200000" step="100"><p class="field-help">Higher values suit brushed drivers; ESCs may need lower rates.</p></div>
      <div class="field motorBox"><label for="cfg_pwmr">PWM resolution (bits)</label><select id="cfg_pwmr"></select><p class="field-help">Changes motor test max and output scaling.</p></div>
      <div class="field motorBox"><label for="cfg_sda">I2C SDA</label><input type="number" id="cfg_sda" min="0" max="39" step="1"><p class="field-help">MPU6050 data pin.</p></div>
      <div class="field motorBox"><label for="cfg_scl">I2C SCL</label><input type="number" id="cfg_scl" min="0" max="39" step="1"><p class="field-help">MPU6050 clock pin.</p></div>
    </div>
  </div>

  <div class="settings-block setup-step">
    <div class="setup-step-head">
      <span class="step-num">4</span>
      <div>
        <div class="setup-step-title">Flight Limits</div>
        <p class="field-help">Limit stick authority while you tune. Keep these conservative for first hover tests.</p>
      </div>
    </div>
    <div class="settingsGrid">
      <div class="field motorBox"><label for="cfg_beta">Madgwick beta (0.01-1)</label><input type="number" id="cfg_beta" min="0.01" max="1" step="0.01"><p class="field-help">Filter correction strength. Lower is smoother; higher corrects faster.</p></div>
      <div class="field motorBox"><label for="cfg_maxtilt">Max tilt (deg, stick full)</label><input type="number" id="cfg_maxtilt" min="5" max="55" step="1"><p class="field-help">Angle-mode roll/pitch limit.</p></div>
      <div class="field motorBox"><label for="cfg_maxyaw">Max yaw rate (deg/s)</label><input type="number" id="cfg_maxyaw" min="30" max="360" step="5"><p class="field-help">Yaw command limit at full stick.</p></div>
      <div class="field motorBox"><label for="cfg_yawsign">Yaw mix direction</label><select id="cfg_yawsign"><option value="1">Normal</option><option value="-1">Reversed</option></select><p class="field-help">Reverse if yaw correction spins the wrong way.</p></div>
    </div>
  </div>

  <div class="settings-block setup-step">
    <div class="setup-step-head">
      <span class="step-num">5</span>
      <div>
        <div class="setup-step-title">Safety And Failsafe</div>
        <p class="field-help">Arming and link-loss behavior. Test these with propellers removed before flight.</p>
      </div>
    </div>
    <div class="settingsGrid">
      <div class="field motorBox"><label for="cfg_idle">Idle throttle (%)</label><input type="number" id="cfg_idle" min="0" max="40" step="1"><p class="field-help">Motor floor after arming. Use 0 during setup.</p></div>
      <div class="field motorBox"><label for="cfg_minthr">Minimum throttle (%)</label><input type="number" id="cfg_minthr" min="0" max="40" step="1"><p class="field-help">Lowest commanded flight throttle.</p></div>
      <div class="field motorBox"><label for="cfg_maxthr">Maximum throttle (%)</label><input type="number" id="cfg_maxthr" min="40" max="100" step="1"><p class="field-help">Cap output power for early tests.</p></div>
      <div class="field motorBox"><label for="cfg_armthr">Arm throttle limit (%)</label><input type="number" id="cfg_armthr" min="0" max="20" step="1"><p class="field-help">Arming is blocked above this throttle.</p></div>
      <div class="field motorBox"><label for="cfg_armang">Max arm angle (deg)</label><input type="number" id="cfg_armang" min="5" max="45" step="1"><p class="field-help">Arming is blocked if the frame is too tilted.</p></div>
      <div class="field motorBox"><label for="cfg_fstime">Failsafe timeout (ms)</label><input type="number" id="cfg_fstime" min="100" max="5000" step="50"><p class="field-help">Time without joystick packets before failsafe starts.</p></div>
      <div class="field motorBox"><label for="cfg_fsdis">Failsafe disarm (ms)</label><input type="number" id="cfg_fsdis" min="100" max="10000" step="100"><p class="field-help">How long failsafe waits before disarming.</p></div>
    </div>
  </div>

  <div class="settings-block setup-step">
    <div class="setup-step-head">
      <span class="step-num">6</span>
      <div>
        <div class="setup-step-title">Saved PID Profiles</div>
        <p class="field-help">These values load at boot. Use the PID tab for live tuning, then copy the live values here before saving.</p>
      </div>
    </div>
    <div class="settingsGrid">
      <div class="field motorBox"><label for="cfg_kpr">Roll P</label><input type="number" id="cfg_kpr" min="0" max="20" step="0.01"><p class="field-help">Roll response strength.</p></div>
      <div class="field motorBox"><label for="cfg_kir">Roll I</label><input type="number" id="cfg_kir" min="0" max="5" step="0.01"><p class="field-help">Roll drift correction.</p></div>
      <div class="field motorBox"><label for="cfg_kdr">Roll D</label><input type="number" id="cfg_kdr" min="0" max="20" step="0.01"><p class="field-help">Roll damping.</p></div>
      <div class="field motorBox"><label for="cfg_kpp">Pitch P</label><input type="number" id="cfg_kpp" min="0" max="20" step="0.01"><p class="field-help">Pitch response strength.</p></div>
      <div class="field motorBox"><label for="cfg_kip">Pitch I</label><input type="number" id="cfg_kip" min="0" max="5" step="0.01"><p class="field-help">Pitch drift correction.</p></div>
      <div class="field motorBox"><label for="cfg_kdp">Pitch D</label><input type="number" id="cfg_kdp" min="0" max="20" step="0.01"><p class="field-help">Pitch damping.</p></div>
      <div class="field motorBox"><label for="cfg_kpy">Yaw P</label><input type="number" id="cfg_kpy" min="0" max="20" step="0.01"><p class="field-help">Yaw rate response.</p></div>
      <div class="field motorBox"><label for="cfg_kiy">Yaw I</label><input type="number" id="cfg_kiy" min="0" max="5" step="0.01"><p class="field-help">Yaw rate trim.</p></div>
      <div class="field motorBox"><label for="cfg_kdy">Yaw D</label><input type="number" id="cfg_kdy" min="0" max="20" step="0.01"><p class="field-help">Yaw damping, usually low.</p></div>
      <label class="check-row motorBox"><input type="checkbox" id="cfg_piden"> Start with PID enabled</label>
    </div>
  </div>

  <div class="presetRow block-gap">
    <button type="button" class="btnArm" onclick="saveSettings()">Save settings</button>
    <button type="button" class="btnDisarm" onclick="restartDevice()">Restart device</button>
  </div>
</div>

<!-- MOTOR TEST TAB -->
<div id="motors" class="page card">
  <h3 class="page-title">Motor test</h3>
  <p class="danger-note">Remove propellers before motor testing. The firmware only allows motor test while disarmed, but bad wiring can still be dangerous.</p>
  <p class="info-note block-gap">Use low PWM values first. Current output range is 0 to <strong><span id="motor_pwmMax">255</span></strong>.</p>
  <div class="motor-map" aria-label="Quad X motor order">
    <div class="motorBox motor-row motor-card m1">
      <div><strong>M1</strong><span class="motor-pos">Front left</span></div>
      <input id="m1" type="range" min="0" max="255" oninput="motorTest()" aria-label="Motor 1 front left">
      <div class="muted"><span id="m1v">0</span> PWM</div>
      <div class="motor-actions"><button type="button" class="btnGhost" onclick="pulseMotor(1)">Pulse M1</button></div>
    </div>
    <div class="motorBox motor-row motor-card m2">
      <div><strong>M2</strong><span class="motor-pos">Front right</span></div>
      <input id="m2" type="range" min="0" max="255" oninput="motorTest()" aria-label="Motor 2 front right">
      <div class="muted"><span id="m2v">0</span> PWM</div>
      <div class="motor-actions"><button type="button" class="btnGhost" onclick="pulseMotor(2)">Pulse M2</button></div>
    </div>
    <div class="motorBox motor-row motor-card m3">
      <div><strong>M3</strong><span class="motor-pos">Back left</span></div>
      <input id="m3" type="range" min="0" max="255" oninput="motorTest()" aria-label="Motor 3 back left">
      <div class="muted"><span id="m3v">0</span> PWM</div>
      <div class="motor-actions"><button type="button" class="btnGhost" onclick="pulseMotor(3)">Pulse M3</button></div>
    </div>
    <div class="motorBox motor-row motor-card m4">
      <div><strong>M4</strong><span class="motor-pos">Back right</span></div>
      <input id="m4" type="range" min="0" max="255" oninput="motorTest()" aria-label="Motor 4 back right">
      <div class="muted"><span id="m4v">0</span> PWM</div>
      <div class="motor-actions"><button type="button" class="btnGhost" onclick="pulseMotor(4)">Pulse M4</button></div>
    </div>
  </div>
  <div class="motor-stop-row">
    <button type="button" class="btnEstop" onclick="stopAllMotors()">Stop all motors</button>
    <button type="button" class="btnGhost" onclick="setAllMotors(0)">Zero sliders</button>
  </div>
</div>
  </main>
</div>

<script>

// ---------------------------------------------------------
// TAB SWITCHING
// ---------------------------------------------------------
function switchTab(id, el) {
  document.querySelectorAll('.page').forEach(p => {
    p.classList.remove('page-active');
  });
  document.getElementById(id).classList.add('page-active');

  document.querySelectorAll('.tab').forEach(t => t.classList.remove("activeTab"));
  el.classList.add("activeTab");
  if (id === "settings") loadSettingsForm();
}

// ---------------------------------------------------------
// WEBSOCKET
// ---------------------------------------------------------
let ws = null;
let reconnectTimer = null;
let pwmMax = 255;

let jsYaw=0, jsPitch=0, jsRoll=0, throttle=-1;

function updateWsBadge(ok) {
  const el = document.getElementById("wsState");
  el.textContent = ok ? "WS Online" : "WS Offline";
  el.className = "badge " + (ok ? "ok" : "warn");
}

function updateArmBadge(armed) {
  const el = document.getElementById("armState");
  el.textContent = armed ? "ARMED" : "DISARMED";
  el.className = "badge " + (armed ? "ok" : "warn");
}

function sendWS(msg) {
  if (ws && ws.readyState === WebSocket.OPEN) ws.send(msg);
}

function connectWS() {
  ws = new WebSocket("ws://" + location.hostname + ":81");
  ws.onopen = () => {
    updateWsBadge(true);
    if (reconnectTimer) {
      clearTimeout(reconnectTimer);
      reconnectTimer = null;
    }
  };

  ws.onclose = () => {
    updateWsBadge(false);
    if (!reconnectTimer) reconnectTimer = setTimeout(connectWS, 1000);
  };

  ws.onerror = () => {
    updateWsBadge(false);
    try { ws.close(); } catch (e) {}
  };

  ws.onmessage = function(e) {
    let d;
    try { d = JSON.parse(e.data); } catch (err) { return; }
    if (d.type === "telem") {
      t_roll.innerText = d.ax;
      t_pitch.innerText = d.ay;
      t_yaw.innerText = d.az;
      t_motors.innerText = d.m1 + "," + d.m2 + "," + d.m3 + "," + d.m4;
      t_arm.innerText = d.arm;
      if (typeof d.pwmMax !== "undefined") {
        pwmMax = Number(d.pwmMax) || 255;
        updateMotorScale();
      }
      t_pwmMax.innerText = pwmMax;
      t_cal.innerText = d.cal === 1 ? "yes" : "no";
      t_failsafe.innerText = d.failsafe === 1 ? "ACTIVE" : "ok";
      t_armReason.innerText = d.armReason || "--";
      s_arm.innerText = d.arm === 1 ? "armed" : "disarmed";
      s_cal.innerText = d.cal === 1 ? "yes" : "no";
      s_failsafe.innerText = d.failsafe === 1 ? "ACTIVE" : "ok";
      t_kp.innerText = Number(d.kp ?? 0).toFixed(2);
      t_ki.innerText = Number(d.ki ?? 0).toFixed(2);
      t_kd.innerText = Number(d.kd ?? 0).toFixed(2);
      if (![kp, ki, kd].includes(document.activeElement)) {
        kp.value = Number(d.kp ?? kp.value).toFixed(2);
        ki.value = Number(d.ki ?? ki.value).toFixed(2);
        kd.value = Number(d.kd ?? kd.value).toFixed(2);
        updatePIDDisplay();
      }
      const pidRow = document.getElementById("t_pidEn");
      if (pidRow && typeof d.pidEn !== "undefined")
        pidRow.innerText = (d.pidEn === 1) ? "yes" : "no";
      const pidCb = document.getElementById("pidEnable");
      if (pidCb && typeof d.pidEn !== "undefined") {
        const on = d.pidEn === 1;
        if (pidCb.checked !== on) pidCb.checked = on;
      }
      const tIdle = document.getElementById("t_idle");
      if (tIdle && typeof d.idlePct !== "undefined")
        tIdle.innerText = Number(d.idlePct).toFixed(0) + "%";
      const idleSl = document.getElementById("idleThr");
      if (idleSl && typeof d.idlePct !== "undefined") {
        const iv = String(Math.round(Number(d.idlePct)));
        if (idleSl.value !== iv) idleSl.value = iv;
        updateIdleThrDisp();
      }
      t_loopHz.innerText = Number(d.loopHz ?? 0).toFixed(1);
      const age = Number(d.linkAge ?? 0);
      t_linkAge.innerText = age > 1000000 ? "none" : age + " ms";
      s_linkAge.innerText = age > 1000000 ? "none" : age + " ms";
      s_loopHz.innerText = Number(d.loopHz ?? 0).toFixed(1);
      t_pkt.innerText = d.pkt ?? 0;
      t_pktMiss.innerText = d.pktMiss ?? 0;
      updateArmBadge(d.arm === 1);
      return;
    }
    if (d.type === "cal") {
      const el = document.getElementById("calStatus");
      if (el) el.textContent = d.ok ? "IMU calibration finished." : "Calibration failed.";
      return;
    }
  };
}

// ---------------------------------------------------------
// ARM / DISARM
// ---------------------------------------------------------
function arm() {
  setThrottleFromSlider();
  sendWS("JOY:" + jsYaw + "," + throttle + "," + jsRoll + "," + jsPitch);
  sendWS("ARM");
}
function disarm() { sendWS("DISARM"); updateArmBadge(false); }
function estop()  { sendWS("ESTOP"); updateArmBadge(false); }

function calibrateImu() {
  if (!confirm("Disarm and calibrate the IMU? Place the drone level on a still surface for about 2 seconds.")) return;
  const el = document.getElementById("calStatus");
  if (el) el.textContent = "Calibrating... keep still.";
  updateArmBadge(false);
  sendWS("CALIBRATE");
}

// ---------------------------------------------------------
// PID SEND
// ---------------------------------------------------------
function sendPidEnable() {
  const on = document.getElementById("pidEnable").checked ? 1 : 0;
  sendWS("USE_PID:" + on);
}

function updateIdleThrDisp() {
  const sl = document.getElementById("idleThr");
  if (!sl) return;
  const v = Number(sl.value);
  const elP = document.getElementById("idleThr_val");
  const elW = document.getElementById("idleThr_pwm");
  if (elP) elP.innerText = String(v);
  if (elW) elW.innerText = String(Math.round(v / 100 * pwmMax));
}

function sendIdleThr() {
  updateIdleThrDisp();
  const sl = document.getElementById("idleThr");
  if (sl) sendWS("IDLE:" + sl.value);
}

function constrainThrottlePct(v) {
  return Math.max(0, Math.min(100, Number(v) || 0));
}

function setThrottleFromSlider() {
  const sl = document.getElementById("throttleCmd");
  if (!sl) return;
  const pct = Number(sl.value);
  throttle = (pct / 50.0) - 1.0;
  v_throttle.innerText = pct.toFixed(0) + "%";
  const out = document.getElementById("throttleCmd_val");
  if (out) out.innerText = pct.toFixed(0) + "%";
  sendWS("JOY:" + jsYaw + "," + throttle + "," + jsRoll + "," + jsPitch);
}

function updatePIDDisplay() {
  let p=kp.value, i=ki.value, d=kd.value;
  kp_val.innerText=Number(p).toFixed(2);
  ki_val.innerText=Number(i).toFixed(2);
  kd_val.innerText=Number(d).toFixed(2);
}

function updatePID() {
  updatePIDDisplay();
  let p=kp.value, i=ki.value, d=kd.value;
  sendWS("PID:"+p+","+i+","+d);
}

function setPidValues(p, i, d) {
  kp.value = p.toFixed(2);
  ki.value = i.toFixed(2);
  kd.value = d.toFixed(2);
  updatePID();
}

function applyPreset(name) {
  if (name === "indoor") setPidValues(1.60, 0.06, 0.90);
  if (name === "stable") setPidValues(2.20, 0.08, 1.20);
  if (name === "aggressive") setPidValues(3.00, 0.11, 1.60);
}

async function copyLivePidToSaved() {
  const loaded = await loadSettingsForm();
  if (!loaded) return;
  const p = Number(kp.value).toFixed(2);
  const i = Number(ki.value).toFixed(2);
  const d = Number(kd.value).toFixed(2);
  ["cfg_kpr", "cfg_kpp"].forEach(id => document.getElementById(id).value = p);
  ["cfg_kir", "cfg_kip"].forEach(id => document.getElementById(id).value = i);
  ["cfg_kdr", "cfg_kdp"].forEach(id => document.getElementById(id).value = d);
  const st = document.getElementById("cfg_save_status");
  if (st) st.textContent = "Live roll/pitch PID copied to saved fields. Save Settings to persist after restart.";
  document.querySelectorAll('.page').forEach(p => p.classList.remove('page-active'));
  document.getElementById('settings').classList.add('page-active');
  document.querySelectorAll('.tab').forEach(t => t.classList.remove("activeTab"));
  document.getElementById('tabSettings').classList.add("activeTab");
}

// ---------------------------------------------------------
// MOTOR TEST
// ---------------------------------------------------------
function motorTest() {
  updateMotorReadouts();
  sendWS(
    "MOTORS:" +
    m1.value + "," +
    m2.value + "," +
    m3.value + "," +
    m4.value
  );
}

function updateMotorReadouts() {
  m1v.innerText = m1.value;
  m2v.innerText = m2.value;
  m3v.innerText = m3.value;
  m4v.innerText = m4.value;
}

function setAllMotors(value) {
  ["m1", "m2", "m3", "m4"].forEach(id => {
    const el = document.getElementById(id);
    el.value = String(value);
  });
  motorTest();
}

function stopAllMotors() {
  setAllMotors(0);
}

function pulseMotor(index) {
  const pulse = Math.max(1, Math.round(pwmMax * 0.08));
  ["m1", "m2", "m3", "m4"].forEach((id, i) => {
    document.getElementById(id).value = (i === index - 1) ? String(pulse) : "0";
  });
  motorTest();
  setTimeout(() => setAllMotors(0), 350);
}

function updateMotorScale() {
  ["m1", "m2", "m3", "m4"].forEach(id => {
    const el = document.getElementById(id);
    if (!el) return;
    el.max = String(pwmMax);
    if (Number(el.value) > pwmMax) el.value = String(pwmMax);
  });
  const motorMax = document.getElementById("motor_pwmMax");
  if (motorMax) motorMax.innerText = String(pwmMax);
  updateMotorReadouts();
}

// ---------------------------------------------------------
// SETTINGS (HTTP, same host as the page)
// ---------------------------------------------------------
(function initCfgPwmResSelect() {
  const sel = document.getElementById("cfg_pwmr");
  if (!sel) return;
  for (let b = 8; b <= 13; b++) {
    const o = document.createElement("option");
    o.value = String(b);
    o.textContent = b + " bits";
    sel.appendChild(o);
  }
})();

async function loadSettingsForm() {
  const st = document.getElementById("cfg_save_status");
  try {
    const r = await fetch("/api/cfg");
    if (!r.ok) {
      if (st) st.textContent = "Could not load settings (HTTP " + r.status + ").";
      return false;
    }
    const j = await r.json();
    document.getElementById("cfg_ssid").value = j.ssid || "";
    document.getElementById("cfg_pass").value = "";
    document.getElementById("cfg_clear_pw").checked = false;
    const note = document.getElementById("cfg_pass_note");
    if (note) {
      note.textContent = j.pwSet
        ? "A password is saved. Leave the password field blank to keep it."
        : "No password is saved for this Wi-Fi network.";
    }
    document.getElementById("cfg_mfl").value = j.mfl;
    document.getElementById("cfg_mfr").value = j.mfr;
    document.getElementById("cfg_mbl").value = j.mbl;
    document.getElementById("cfg_mbr").value = j.mbr;
    document.getElementById("cfg_pwmf").value = j.pwmf;
    document.getElementById("cfg_pwmr").value = String(j.pwmr);
    document.getElementById("cfg_sda").value = j.sda;
    document.getElementById("cfg_scl").value = j.scl;
    document.getElementById("cfg_beta").value = j.beta;
    document.getElementById("cfg_maxtilt").value = j.maxtilt;
    document.getElementById("cfg_maxyaw").value = j.maxyaw;
    document.getElementById("cfg_yawsign").value = String(j.yawsign);
    document.getElementById("cfg_idle").value = j.idle;
    document.getElementById("cfg_minthr").value = j.minthr;
    document.getElementById("cfg_maxthr").value = j.maxthr;
    document.getElementById("cfg_armthr").value = j.armthr;
    document.getElementById("cfg_armang").value = j.armang;
    document.getElementById("cfg_fstime").value = j.fstime;
    document.getElementById("cfg_fsdis").value = j.fsdis;
    document.getElementById("cfg_kpr").value = j.kpr;
    document.getElementById("cfg_kir").value = j.kir;
    document.getElementById("cfg_kdr").value = j.kdr;
    document.getElementById("cfg_kpp").value = j.kpp;
    document.getElementById("cfg_kip").value = j.kip;
    document.getElementById("cfg_kdp").value = j.kdp;
    document.getElementById("cfg_kpy").value = j.kpy;
    document.getElementById("cfg_kiy").value = j.kiy;
    document.getElementById("cfg_kdy").value = j.kdy;
    document.getElementById("cfg_piden").checked = j.piden === 1;
    if (st) st.textContent = "";
    return true;
  } catch (e) {
    if (st) st.textContent = "Could not load settings.";
    return false;
  }
}

async function saveSettings() {
  const st = document.getElementById("cfg_save_status");
  if (st) st.textContent = "Saving...";
  const fd = new URLSearchParams();
  fd.append("ssid", document.getElementById("cfg_ssid").value.trim());
  fd.append("password", document.getElementById("cfg_pass").value);
  if (document.getElementById("cfg_clear_pw").checked) fd.append("clear_pw", "1");
  fd.append("mfl", document.getElementById("cfg_mfl").value);
  fd.append("mfr", document.getElementById("cfg_mfr").value);
  fd.append("mbl", document.getElementById("cfg_mbl").value);
  fd.append("mbr", document.getElementById("cfg_mbr").value);
  fd.append("pwmf", document.getElementById("cfg_pwmf").value);
  fd.append("pwmr", document.getElementById("cfg_pwmr").value);
  fd.append("sda", document.getElementById("cfg_sda").value);
  fd.append("scl", document.getElementById("cfg_scl").value);
  fd.append("beta", document.getElementById("cfg_beta").value);
  fd.append("maxtilt", document.getElementById("cfg_maxtilt").value);
  fd.append("maxyaw", document.getElementById("cfg_maxyaw").value);
  fd.append("yawsign", document.getElementById("cfg_yawsign").value);
  fd.append("idle", document.getElementById("cfg_idle").value);
  fd.append("minthr", document.getElementById("cfg_minthr").value);
  fd.append("maxthr", document.getElementById("cfg_maxthr").value);
  fd.append("armthr", document.getElementById("cfg_armthr").value);
  fd.append("armang", document.getElementById("cfg_armang").value);
  fd.append("fstime", document.getElementById("cfg_fstime").value);
  fd.append("fsdis", document.getElementById("cfg_fsdis").value);
  fd.append("kpr", document.getElementById("cfg_kpr").value);
  fd.append("kir", document.getElementById("cfg_kir").value);
  fd.append("kdr", document.getElementById("cfg_kdr").value);
  fd.append("kpp", document.getElementById("cfg_kpp").value);
  fd.append("kip", document.getElementById("cfg_kip").value);
  fd.append("kdp", document.getElementById("cfg_kdp").value);
  fd.append("kpy", document.getElementById("cfg_kpy").value);
  fd.append("kiy", document.getElementById("cfg_kiy").value);
  fd.append("kdy", document.getElementById("cfg_kdy").value);
  fd.append("piden", document.getElementById("cfg_piden").checked ? "1" : "0");
  try {
    const r = await fetch("/settings", { method: "POST", headers: { "Content-Type": "application/x-www-form-urlencoded" }, body: fd.toString() });
    const t = await r.text();
    if (st) st.textContent = t + (r.ok ? "" : " (HTTP " + r.status + ")");
  } catch (e) {
    if (st) st.textContent = "Save failed.";
  }
}

async function restartDevice() {
  if (!confirm("Restart the ESP32 now? You will lose the connection until it rejoins Wi-Fi.")) return;
  try {
    await fetch("/restart", { method: "POST" });
  } catch (e) {}
}

// ---------------------------------------------------------
// JOYSTICKS
// ---------------------------------------------------------
function attachJoystick(id, callback, releaseX = 0, releaseY = 0) {
  const joy = document.getElementById(id);
  const knob = joy.children[0];
  const knobHalf = 31;
  let cx = 0, cy = 0, r = 0;
  let active = false;

  function syncGeom() {
    cx = joy.clientWidth / 2;
    cy = joy.clientHeight / 2;
    r = cx;
  }

  function release() {
    if (!active) return;
    active = false;
    syncGeom();
    knob.style.left = (cx + releaseX * r - knobHalf) + "px";
    knob.style.top  = (cy + releaseY * r - knobHalf) + "px";
    callback(releaseX, releaseY);
  }

  joy.addEventListener("pointerdown", (e) => {
    active = true;
    syncGeom();
    joy.setPointerCapture(e.pointerId);
  });
  joy.addEventListener("pointerup", release);
  joy.addEventListener("pointercancel", release);

  joy.addEventListener("pointermove", (e) => {
    if (!active) return;
    syncGeom();
    const rect = joy.getBoundingClientRect();
    let x = e.clientX - rect.left;
    let y = e.clientY - rect.top;
    let dx = x - cx, dy = y - cy;
    const dist = Math.sqrt(dx * dx + dy * dy);
    if (dist > r && dist > 0) { dx *= r / dist; dy *= r / dist; }
    knob.style.left = (cx + dx - knobHalf) + "px";
    knob.style.top  = (cy + dy - knobHalf) + "px";
    const nr = r > 1e-6 ? r : 1;
    callback(dx / nr, dy / nr);
  });
}

attachJoystick("joyLeft", (x,y)=>{
  jsYaw = x;
  throttle = -y;
  const pct = constrainThrottlePct((throttle + 1.0) * 50.0);
  v_yaw.innerText = x.toFixed(2);
  v_throttle.innerText = pct.toFixed(0) + "%";
  const sl = document.getElementById("throttleCmd");
  if (sl) {
    sl.value = String(Math.round(pct));
    throttleCmd_val.innerText = pct.toFixed(0) + "%";
  }
  sendWS("JOY:"+x+","+(-y)+","+jsRoll+","+jsPitch);
});

attachJoystick("joyRight", (x,y)=>{
  jsRoll = x;
  jsPitch = -y;
  v_roll.innerText = x.toFixed(2);
  v_pitch.innerText = (-y).toFixed(2);
  sendWS("JOY:"+jsYaw+","+throttle+","+x+","+(-y));
});

updateWsBadge(false);
updateArmBadge(false);
setThrottleFromSlider();
updatePIDDisplay();
updateIdleThrDisp();
updateMotorScale();
updateMotorReadouts();
connectWS();
setInterval(() => {
  sendWS("JOY:" + jsYaw + "," + throttle + "," + jsRoll + "," + jsPitch);
}, 50);

</script>
</body>
</html>

)=====";

#endif

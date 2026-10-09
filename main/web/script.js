// Set to false for live operation against the ESP32.
const USE_MOCK = false;

// Logs every raw frame arriving from the ESP32. Turn off once bring-up is done.
const DEBUG_FRAMES = true;

// If no 'end' frame arrives within this window the request is considered lost
// and the poller is released, rather than stalling for good.
const RESPONSE_TIMEOUT_MS = 3000;

const gateway = `ws://${window.location.hostname}/ws`;
let websocket;
let isPopupOpen = false;
let waitingForResponse = false;
let waitingForImport = false;
// Queued commands. Two shapes:
//   { kind: 'field',   ch, field, param }  - value re-read from the DOM on send
//   { kind: 'payload', ch, params }        - values captured when queued
const cmds = [];
let zoneval = [];
let pollIntervalId = null;     // Handle for the 250 ms readings poller
let lastRequestTs = 0;         // When the outstanding request was sent

// How each exported/imported key maps onto the raw value in the device's
// 'settings' frame (key = prefix + channel). toRaw converts the file's units
// (as written by exportSettings) into the device's raw units, so the
// post-import check is an exact comparison, not against rounded display.
// This is also the allowlist validateImportFile() enforces.
const IMPORT_VERIFY_FIELDS = {
  VOUT_COMMAND:        { raw: 'vset',    toRaw: v => Math.round(v * 100) },
  VOUT_DROOP:          { raw: 'drp',     toRaw: v => Math.round(v) },
  VOUT_OV_WARN_LIMIT:  { raw: 'ov',      toRaw: v => Math.round(v * 100) },
  VOUT_UV_WARN_LIMIT:  { raw: 'uv',      toRaw: v => Math.round(v * 100) },
  TON_MAX_FLT:         { raw: 'ton',     toRaw: v => Math.round(v * 10) },
  IOUT_OC_FAULT_LIMIT: { raw: 'ocp',     toRaw: v => Math.round(v * 100) },
  ZONE_CONFIG:         { raw: 'zone',    toRaw: v => Math.round(v) },
  MFR_SMBUS_ADDRESS:   { raw: 'addr',    toRaw: v => Math.round(v) },
  // Firmware masks MFR_SETTINGS with 1 (sense: 1 = internal).
  MFR_SETTINGS:        { raw: 'snsterm', toRaw: v => v & 1, fromRaw: r => r & 1 },
  // OPERATION is the raw PMBus byte (0x80 = on); the file holds 1/0.
  OPERATION:           { raw: 'onoff',   toRaw: v => (v ? 1 : 0), fromRaw: r => ((r & 0x80) ? 1 : 0) },
};
let pendingImportVerification = null; // imported settings, while awaiting the post-store readback
let importReadback = {};       // raw 'settings' frame per channel from that readback
let queuedImport = null;       // import chosen while a request was outstanding; sent once the poller is free
// Store NVM alone can take ~10 s worst case (two 5 s PMBus waits on the
// device), plus the readback round -- this needs comfortable headroom.
const IMPORT_TIMEOUT_MS = 30000;
let importTimeoutId = null;

// Module Mode & Chemistry State Tracking
const moduleModes = {};       // Track 'INSTRUMENT' vs 'BATTERY'
const moduleChems = {};       // Track selected chemistry per channel
let activeModalChannel = null;
let activePreviewChemKey = 'lifepo4';
let modalChart = null;
let editingCustomKey = null;   // Tracks custom profile key currently being edited
const charging = {};           // True while a channel is actively charging
const floatStage = {};         // True once a 3-stage profile has dropped to float
const chargeComplete = {};     // Latched when a 2-stage charge self-terminates
const mahAccum = {};           // Integrated charge, mAh, per channel
const lastSampleTs = {};       // Timestamp of the previous integrated sample
const lastCurrent = {};        // Previous current sample, for trapezoidal integration
const belowCutoff = {};        // Consecutive samples under the taper threshold

// A single noisy sample must not end a charge - require this many in a row.
// At the 250 ms poll rate that is ~0.75 s of sustained low current.
const CUTOFF_SAMPLES = 3;

// Ignore integration gaps longer than this (tab backgrounded, socket stall)
// rather than assuming the current held steady across them.
const MAX_INTEGRATION_GAP_MS = 5000;

// Rich Profiles Database
const chemProfiles = {
  lifepo4: {
    name: "Lithium Iron Phosphate",
    fullName: "Lithium Iron Phosphate (2-Stage CC/CV)",
    type: 'cccv',
    cutoffPercent: 5,
    characteristics: "Fast constant-current bulk phase with a flat voltage curve, followed by a brief constant-voltage saturation phase. Typically requires no float charge.",
    markers: [
      "3.00V/cell - Bulk Charge (100% Current)",
      "3.25V/cell - Linear Slope",
      "3.28V/cell - Stable Voltage Plateau",
      "3.30V/cell - Stable Voltage Plateau",
      "3.35V/cell - Approaching Full Capacity",
      "3.60V/cell - CV Absorption Phase Begins",
      "3.60V/cell - Current Drops Rapidly",
      "3.60V/cell - Termination (Current < 5%)"
    ],
    labels: ['0%', '20%', '40%', '60%', '80%', '90% (CV Start)', '95%', '100% (Termination)'],
    voltageCurve: [3.00, 3.25, 3.28, 3.30, 3.35, 3.60, 3.60, 3.60],
    currentCurve: [100, 100, 100, 100, 100, 80, 40, 0]
  },
  nmc: {
    name: "Li-Ion (NMC/LCO)",
    fullName: "Lithium-Ion NMC/LCO (Standard 2-Stage)",
    type: 'cccv',
    cutoffPercent: 5,
    characteristics: "High energy density profile with progressive voltage rise during constant-current, followed by extended constant-voltage taper.",
    markers: [
      "3.00V/cell - Depleted Start",
      "3.60V/cell - Nominal Voltage",
      "3.80V/cell - Bulk CC Phase",
      "4.00V/cell - Upper CC Region",
      "4.10V/cell - Pre-Saturation",
      "4.20V/cell - CV Saturation Begins",
      "4.20V/cell - Tapering Current",
      "4.20V/cell - Cutoff Termination"
    ],
    labels: ['0%', '20%', '40%', '60%', '80%', '90% (CV Start)', '95%', '100% (Termination)'],
    voltageCurve: [3.00, 3.60, 3.80, 4.00, 4.10, 4.20, 4.20, 4.20],
    currentCurve: [100, 100, 100, 100, 100, 75, 30, 0]
  },
  lead: {
    name: "Flooded Lead-Acid",
    fullName: "Flooded Lead-Acid (3-Stage Bulk/Abs/Float)",
    type: '3stage',
    cutoffPercent: 10,
    characteristics: "Classic lead-acid profile featuring Bulk constant current, Absorption saturation, and a constant Float trickle stage to prevent self-discharge.",
    markers: [
      "1.75V/cell - Discharge Cutoff",
      "2.10V/cell - Bulk Phase",
      "2.25V/cell - Transition Region",
      "2.40V/cell - Absorption Voltage limit",
      "2.40V/cell - Gassing Threshold (vented)",
      "2.25V/cell - Float Stage Transition",
      "2.25V/cell - Continuous Float Maintenance"
    ],
    labels: ['0%', '25%', '50%', '75%', '90% (Abs)', '95%', '100% (Float)'],
    voltageCurve: [1.75, 2.10, 2.25, 2.40, 2.40, 2.25, 2.25],
    currentCurve: [100, 100, 100, 80, 30, 10, 5]
  },
  agm: {
    name: "AGM Lead-Acid",
    fullName: "AGM Sealed Lead-Acid (3-Stage Bulk/Abs/Float)",
    type: '3stage',
    cutoffPercent: 10,
    characteristics: "Absorbed glass mat VRLA cell. Takes a higher absorption ceiling than GEL, regulated to prevent venting and electrolyte dry-out.",
    markers: [
      "1.75V/cell - Discharge Cutoff",
      "2.15V/cell - Bulk CC Stage",
      "2.30V/cell - Absorption Entry",
      "2.40V/cell - Max Absorption Voltage",
      "2.40V/cell - Current Tapering",
      "2.27V/cell - Float Charge Maintenance"
    ],
    labels: ['0%', '25%', '50%', '75%', '90% (Abs)', '100% (Float)'],
    voltageCurve: [1.75, 2.15, 2.30, 2.40, 2.40, 2.27],
    currentCurve: [100, 100, 100, 70, 25, 5]
  },
  gel: {
    name: "GEL Lead-Acid",
    fullName: "GEL Sealed Lead-Acid (3-Stage Bulk/Abs/Float)",
    type: '3stage',
    cutoffPercent: 10,
    characteristics: "Gelled electrolyte VRLA cell. Needs a lower absorption ceiling than AGM - overvoltage drives off electrolyte and leaves permanent voids in the gel.",
    markers: [
      "1.75V/cell - Discharge Cutoff",
      "2.15V/cell - Bulk CC Stage",
      "2.28V/cell - Absorption Entry",
      "2.35V/cell - Max Absorption Voltage",
      "2.35V/cell - Current Tapering",
      "2.25V/cell - Float Charge Maintenance"
    ],
    labels: ['0%', '25%', '50%', '75%', '90% (Abs)', '100% (Float)'],
    voltageCurve: [1.75, 2.15, 2.28, 2.35, 2.35, 2.25],
    currentCurve: [100, 100, 100, 70, 25, 5]
  }
};

function openTab(evt, tabId) {
  document.querySelectorAll('.tab-content').forEach(t => t.classList.remove('active-tab'));
  document.querySelectorAll('.header-tab').forEach(b => b.classList.remove('active'));
  document.getElementById(tabId).classList.add('active-tab');
  evt.currentTarget.classList.add('active');
  
  const CommandContainer = document.getElementById('CommandContainer');
  if (tabId === 'instrumentTab') {
    CommandContainer.style.display = 'flex';
  } else {
    CommandContainer.style.display = 'none';
  }
}

function createModule(ch) {
  moduleModes[ch] = moduleModes[ch] || 'INSTRUMENT';
  moduleChems[ch] = moduleChems[ch] || 'lifepo4';

  const isBattery = moduleModes[ch] === 'BATTERY';
  const chemName = (chemProfiles[moduleChems[ch]] || chemProfiles.lifepo4).name;

  return `
  <div class="psu-module" id="ch${ch}_module">
    <div class="module-header">
      <div class="channel-number">${ch}</div>
      <div class="module-info">     
        <div id="ch${ch}_model"></div>
        <div id="ch${ch}_serial"></div>
      </div>
    </div>

    <!-- READINGS -->
    <div class="section-title">Readings</div>
    <div class="readings">
      <div class="reading"><span class="value" id="ch${ch}_voltage">--</span><span class="unit">V</span></div>
      <div class="reading"><span class="value" id="ch${ch}_current">--</span><span class="unit">A</span></div>
      <div class="reading"><span class="value" id="ch${ch}_power">--</span><span class="unit">W</span></div>
      <div class="reading"><span class="value" id="ch${ch}_temp">--</span><span class="unit">\u00B0C</span></div>
    </div>

    <!-- INSTRUMENT MODE VIEW -->
    <div class="mode-view ${isBattery ? 'hidden' : ''}" id="ch${ch}_inst_view">
      <div class="section-title">Settings</div>
      <div class="settings">
        <label>
          <span>VSET</span>
          <input
            type="number"
            id="ch${ch}_vset"
            value="0"
            onkeydown="if(event.key==='Enter') {document.activeElement.blur();}"
            onchange="applySetting(${ch}, 'vset', 'VOUT_COMMAND')"
          />
          <small>V</small>
        </label>
        
        <label>
          <span>DROOP</span>
          <input
            type="number" 
            id="ch${ch}_droop" 
            value="0" 
            onkeydown="if(event.key==='Enter') {document.activeElement.blur();}"
            onchange="applySetting(${ch}, 'droop', 'VOUT_DROOP')"
          /> 
          <small>mR</small>
        </label>
        
        <label>
          <span id="ch${ch}_ovLabel" class="setting-label-indicator">OV</span>
          <input
            type="number"
            id="ch${ch}_ov"
            value="0"
            onkeydown="if(event.key==='Enter') {document.activeElement.blur();}"
            onchange="applySetting(${ch}, 'ov','VOUT_OV_WARN_LIMIT')"
          />
          <small>V</small>
        </label>
        
        <label>
          <span id="ch${ch}_uvLabel" class="setting-label-indicator">UV</span>
          <input
            type="number"
            id="ch${ch}_uv"
            value="0"
            onkeydown="if(event.key==='Enter') {document.activeElement.blur();}"
            onchange="applySetting(${ch}, 'uv','VOUT_UV_WARN_LIMIT')"
          />
          <small>V</small>
        </label>
        
        <label>
          <span id="ch${ch}_tonLabel" class="setting-label-indicator">TON</span>
          <input 
            type="number" 
            id="ch${ch}_ton" 
            value="0" 
            onkeydown="if(event.key==='Enter') {document.activeElement.blur();}"
            onchange="applySetting(${ch}, 'ton', 'TON_MAX_FLT')"
          /> 
          <small>mS</small>
        </label>
        
        <label>
          <span id="ch${ch}_ocpLabel" class="setting-label-indicator">OCP</span>
          <input 
            type="number" 
            id="ch${ch}_ocp" 
            value="0" 
            onkeydown="if(event.key==='Enter') {document.activeElement.blur();}"
            onchange="applySetting(${ch}, 'ocp','IOUT_OC_FAULT_LIMIT')"
          /> 
          <small>A</small>
        </label>
        
        <div class="indicators">
          <div id="ch${ch}_tempStatus" class="indicator green">TEMP</div>
          <div id="ch${ch}_commStatus" class="indicator green">COMM</div>
        </div>

        <label>
          <span>ZONE</span>
          <input 
            type="number" 
            id="ch${ch}_zone" 
            value="0" 
            onkeydown="if(event.key==='Enter') {document.activeElement.blur();}"
            onchange="onZoneChange(${ch})"
          />
        </label>
        
        <label>
          <span>ADDR</span>
          <input 
            type="number" 
            id="ch${ch}_addr" 
            value="0" 
            onkeydown="if(event.key==='Enter') {document.activeElement.blur();}"
            onchange="applySetting(${ch}, 'addr','MFR_SMBUS_ADDRESS')"
          />
        </label>
        
        <div class="hardware" id="ch${ch}_hardware">HW: v1.0</div>
      </div>

      <div class="buttons">
        <button id="ch${ch}_termBtn" onclick="toggleTerm(${ch});">SNS</button>
        <button id="ch${ch}_strnvm" onclick="applySetting(${ch}, 'strnvm', 'STORE_DEFAULT_ALL');">STORE NVM</button>
        <button id="ch${ch}_onoffBtn" class="onoff-btn full-width" onclick="toggleOutput(${ch});">ON</button>
      </div>
    </div>

    <!-- BATTERY MODE VIEW -->
    <div class="mode-view ${isBattery ? '' : 'hidden'}" id="ch${ch}_batt_view">
      <div class="section-title">Settings</div>
      <div class="settings">

        <div class="battery-field">
          <label>Chemistry</label>
          <button class="chem-select-btn" id="ch${ch}_chemBtn" onclick="openChemistryModal(${ch})">${chemName}</button>
        </div>

        <label>
          <span>CELLS</span>
          <input
            type="number"
            id="ch${ch}_cellCount"
            value="1"
            min="1"
            step="1"
            onkeydown="if(event.key==='Enter') {document.activeElement.blur();}"
            onchange="onCellCountChange(${ch})"
          />
          <small></small>
        </label>

        <label title="Max cell voltage x cell count - sets the CV level">
          <span>VMAX</span>
          <input
            type="number"
            id="ch${ch}_maxVoltage"
            value="0"
            readonly
          />
          <small>V</small>
        </label>

        <label title="Sets the CC / current limit level">
          <span>IMAX</span>
          <input
            type="number"
            id="ch${ch}_maxCurrent"
            value="0"
            onkeydown="if(event.key==='Enter') {document.activeElement.blur();}"
            onchange="onMaxCurrentChange(${ch})"
          />
          <small>A</small>
        </label>

      </div>

      <div class="buttons">
        <button class="vp-btn-start full-width" id="ch${ch}_startBtn" onclick="battStart(${ch})">START</button>

        <div class="readings batt-mah">
          <div class="reading"><span class="value" id="ch${ch}_mah">0.00</span><span class="unit unit-mah">mAh</span></div>
        </div>

        <button class="vp-btn-stop full-width" id="ch${ch}_stopBtn" onclick="battStop(${ch})">STOP</button>
        <button class="vp-btn-reset full-width" id="ch${ch}_resetBtn" onclick="battReset(${ch})">RESET</button>
      </div>

      <div class="vp-pill vp-pill-green">Connected</div>
      <div class="vp-pill vp-pill-gray" id="ch${ch}_cvccPill">IDLE</div>
    </div>

    <!-- MODE TOGGLE BUTTON -->
    <div class="buttons mode-toggle-wrap">
      <button class="mode-toggle-btn full-width" id="ch${ch}_modeBtn" onclick="toggleModuleMode(${ch});">
        ${isBattery ? 'BATTERY CHARGER MODE' : 'INSTRUMENT MODE'}
      </button>
    </div>
  </div>`;
}

// Per-model hardware ceilings. Keyed on the model string reported in the
// settings frame. An unrecognised model is NOT limited - it only warns - so a
// future module isn't silently clamped to the wrong envelope. Flip
// UNKNOWN_MODEL_IS_LIMITED if you would rather fail closed.
const modelLimits = {
  OP1D:  { vMax: 7.5,  iMax: 25.0  },
  OP2D:  { vMax: 15.0, iMax: 15.0  },
  OP3D:  { vMax: 30.0, iMax: 7.5   },
  OP4D:  { vMax: 60.0, iMax: 3.75  },
  OPA2D: { vMax: 15.0, iMax: 25.0  },
  OPA3D: { vMax: 30.0, iMax: 15.0  }
};

const UNKNOWN_MODEL_IS_LIMITED = false;
const FALLBACK_LIMIT = { vMax: 7.5, iMax: 3.75 };   // most restrictive combination

const moduleModels = {};       // Model string per channel, from the settings frame
const blockedReason = {};      // Non-empty when START is refused for this channel
const warnedModels = {};       // So an unknown model only warns once per channel

function limitsFor(ch) {
  const model = moduleModels[ch];
  if (model && modelLimits[model]) return modelLimits[model];

  if (model && !warnedModels[ch]) {
    warnedModels[ch] = true;
    console.warn(`CH${ch}: model '${model}' has no entry in modelLimits`);
  }
  return UNKNOWN_MODEL_IS_LIMITED ? FALLBACK_LIMIT : null;
}

// Shown on the status pill, and set immediately rather than waiting for the
// next readings frame so the operator sees why START did nothing.
function setBlocked(ch, reason) {
  blockedReason[ch] = reason || '';

  const pill = document.getElementById(`ch${ch}_cvccPill`);
  if (pill && reason) {
    pill.innerText = reason;
    pill.className = 'vp-pill vp-pill-fault';
  }
}

// IMAX is operator-entered, so clamp it down to what the module can deliver.
function clampMaxCurrent(ch) {
  const field = document.getElementById(`ch${ch}_maxCurrent`);
  if (!field) return 0;

  let value = parseFloat(field.value);
  if (!Number.isFinite(value) || value < 0) value = 0;

  const limits = limitsFor(ch);
  if (limits && value > limits.iMax) {
    console.warn(`CH${ch}: IMAX ${value} A exceeds ${moduleModels[ch]} limit, clamped to ${limits.iMax} A`);
    value = limits.iMax;
  }

  const formatted = value.toFixed(2);
  if (field.value !== formatted) field.value = formatted;
  return value;
}

function onMaxCurrentChange(ch) {
  clampMaxCurrent(ch);
  setBlocked(ch, '');
}

// Battery command plumbing
// Nothing is pushed to PMBus from the battery pane until START is pressed;
// editing CELLS / IMAX only updates the browser-side values.
function sendBatteryCommand(ch, params) {
  // Queued rather than written straight out, so a START issued while a
  // readings request is in flight cannot interleave with the response.
  // Values are captured now, not re-read when the queue drains.
  if (waitingForResponse || waitingForImport) {
    cmds.push({ kind: 'payload', ch, params });
    return;
  }

  writeSettings(ch, params);
}

function battStart(ch) {
  if (charging[ch]) return;

  const vmax = recalcBatteryLimits(ch);
  const imax = clampMaxCurrent(ch);
  const limits = limitsFor(ch);

  // The pack needs more voltage than this module can produce. Starting anyway
  // would charge to the module ceiling and stall there, never terminating.
  if (limits && vmax > limits.vMax) {
    console.warn(`CH${ch} START refused: VMAX ${vmax.toFixed(2)} V exceeds ${moduleModels[ch]} limit of ${limits.vMax} V`);
    setBlocked(ch, 'V LIMIT');
    return;
  }

  if (imax <= 0) {
    console.warn(`CH${ch} START refused: IMAX is zero`);
    setBlocked(ch, 'SET IMAX');
    return;
  }

  setBlocked(ch, '');

  // CV level, CC level, then enable the output - in that order
  sendBatteryCommand(ch, {
    VOUT_COMMAND: vmax,
    IOUT_OC_FAULT_LIMIT: imax,
    OPERATION: 1
  });

  floatStage[ch] = false;
  chargeComplete[ch] = false;
  belowCutoff[ch] = 0;
  lastSampleTs[ch] = null;
  lastCurrent[ch] = null;

  setChargingState(ch, true);
}

function battStop(ch) {
  // Stops charging, mAh accumulator left intact
  sendBatteryCommand(ch, { OPERATION: 0 });
  floatStage[ch] = false;
  chargeComplete[ch] = false;
  belowCutoff[ch] = 0;
  setBlocked(ch, '');
  setChargingState(ch, false);
}

function battReset(ch) {
  // Stops charging and zeroes the mAh count
  sendBatteryCommand(ch, { OPERATION: 0 });
  floatStage[ch] = false;
  chargeComplete[ch] = false;
  belowCutoff[ch] = 0;
  setBlocked(ch, '');
  resetMah(ch);
  setChargingState(ch, false);
}

// While charging, START reads CHARGING and both it and the mode toggle
// are locked out until STOP or RESET is pressed.
function setChargingState(ch, isCharging) {
  charging[ch] = isCharging;

  const startBtn = document.getElementById(`ch${ch}_startBtn`);
  if (startBtn) {
    startBtn.innerText = isCharging ? 'CHARGING' : 'START';
    startBtn.disabled = isCharging;
  }

  const modeBtn = document.getElementById(`ch${ch}_modeBtn`);
  if (modeBtn) {
    modeBtn.disabled = isCharging;
  }

  // Charge parameters are locked once the charge is running - changing them
  // mid-charge would leave the UI disagreeing with what the module was told,
  // since these are only pushed to PMBus on START.
  ['cellCount', 'maxCurrent', 'chemBtn'].forEach(field => {
    const el = document.getElementById(`ch${ch}_${field}`);
    if (el) el.disabled = isCharging;
  });
}

function resetMah(ch) {
  mahAccum[ch] = 0;
  lastSampleTs[ch] = null;
  lastCurrent[ch] = null;
  renderMah(ch);
  // NOTE: this zeroes the browser-side integrator only. If the ESP32 keeps
  // its own coulomb counter, add the firmware reset command here too.
}

function renderMah(ch) {
  const el = document.getElementById(`ch${ch}_mah`);
  if (el) el.innerText = (mahAccum[ch] || 0).toFixed(2);
}

// Trapezoidal integration of output current over the poll interval.
// Only accumulates while the channel is charging.
function integrateCharge(ch, obj) {
  if (!charging[ch]) {
    lastSampleTs[ch] = null;
    lastCurrent[ch] = null;
    return;
  }

  const iRaw = parseFloat(obj[`i${ch}`]);
  if (!Number.isFinite(iRaw)) return;   // one bad sample must not poison mAh

  const iNow = iRaw / 100;
  const now = Date.now();
  const prevTs = lastSampleTs[ch];

  if (prevTs != null) {
    const dtMs = now - prevTs;
    if (dtMs > 0 && dtMs < MAX_INTEGRATION_GAP_MS) {
      const prevI = lastCurrent[ch];
      const iAvg = (prevI != null) ? (prevI + iNow) / 2 : iNow;
      mahAccum[ch] = (mahAccum[ch] || 0) + iAvg * (dtMs / 3600000) * 1000;
    }
  }

  lastSampleTs[ch] = now;
  lastCurrent[ch] = iNow;
  renderMah(ch);
}

// Taper detection. Once the channel is in CV and the current has fallen
// below the profile's cutoff, either drop to float or terminate.
function updateChargeControl(ch, obj) {
  if (!charging[ch]) return;

  const statByte = obj[`StatByte${ch}`];
  const outputOn = (statByte & 64) !== 64;
  const inCC = (statByte & 16) === 16;

  // Only meaningful in CV - in CC the current is at the limit by definition
  if (!outputOn || inCC) {
    belowCutoff[ch] = 0;
    return;
  }

  const profile = profileForChannel(ch);
  const type = profile.type || 'cccv';

  const imaxField = document.getElementById(`ch${ch}_maxCurrent`);
  const imax = parseFloat(imaxField ? imaxField.value : 0) || 0;
  if (imax <= 0) return;

  const cutoff = imax * ((profile.cutoffPercent || 5) / 100);
  const iNow = parseFloat(obj[`i${ch}`]) / 100;

  if (iNow > cutoff) {
    belowCutoff[ch] = 0;
    return;
  }

  belowCutoff[ch] = (belowCutoff[ch] || 0) + 1;
  if (belowCutoff[ch] < CUTOFF_SAMPLES) return;
  belowCutoff[ch] = 0;

  const curve = profile.voltageCurve;
  const vFloatCell = (type === '3stage') ? curve[curve.length - 1] : null;

  if (vFloatCell !== null && !floatStage[ch]) {
    const vFloat = parseFloat((vFloatCell * cellCountFor(ch)).toFixed(2));
    floatStage[ch] = true;
    sendBatteryCommand(ch, { VOUT_COMMAND: vFloat });
    console.log(`CH${ch} absorption complete (I < ${cutoff.toFixed(2)} A) -> float at ${vFloat} V`);
  } else if (vFloatCell === null) {
    console.log(`CH${ch} charge terminated (I < ${cutoff.toFixed(2)} A)`);
    battStop(ch);
    chargeComplete[ch] = true;   // set after battStop, which clears it
  }
}

// Battery limit derivation
function profileForChannel(ch) {
  return chemProfiles[moduleChems[ch]] || chemProfiles.lifepo4;
}

// The CV target is the highest per-cell voltage the profile reaches
// (CV/absorption ceiling), not the float or termination voltage.
function maxCellVoltage(profile) {
  return Math.max.apply(null, profile.voltageCurve);
}

// A pack has at least one cell. Anything lower (0, blank, negative, non-numeric)
// is clamped and written back, so VMAX can never be derived from a zero count.
function cellCountFor(ch) {
  const cellField = document.getElementById(`ch${ch}_cellCount`);
  if (!cellField) return 1;

  let cells = parseInt(cellField.value, 10);
  if (!Number.isFinite(cells) || cells < 1) {
    cells = 1;
  }

  if (String(cells) !== cellField.value) {
    cellField.value = cells;
  }
  return cells;
}

function recalcBatteryLimits(ch) {
  const vmaxField = document.getElementById(`ch${ch}_maxVoltage`);
  const cellField = document.getElementById(`ch${ch}_cellCount`);
  if (!vmaxField || !cellField) return 0;

  const vmax = maxCellVoltage(profileForChannel(ch)) * cellCountFor(ch);
  vmaxField.value = vmax.toFixed(2);

  const limits = limitsFor(ch);
  const overLimit = !!limits && vmax > limits.vMax;

  vmaxField.classList.toggle('over-limit', overLimit);
  vmaxField.title = overLimit
    ? `Exceeds the ${moduleModels[ch]} maximum of ${limits.vMax} V`
    : 'Max cell voltage x cell count - sets the CV level';

  return vmax;
}

function onCellCountChange(ch) {
  // Recalculate only - VMAX is not pushed to PMBus until START
  recalcBatteryLimits(ch);
  setBlocked(ch, '');
}

// Mode Toggle Handler
function toggleModuleMode(ch) {
  if (charging[ch]) return;   // locked out mid-charge

  const instView = document.getElementById(`ch${ch}_inst_view`);
  const battView = document.getElementById(`ch${ch}_batt_view`);
  const modeBtn = document.getElementById(`ch${ch}_modeBtn`);

  if (instView.classList.contains('hidden')) {
    instView.classList.remove('hidden');
    battView.classList.add('hidden');
    modeBtn.innerText = "INSTRUMENT MODE";
    moduleModes[ch] = "INSTRUMENT";
  } else {
    instView.classList.add('hidden');
    battView.classList.remove('hidden');
    modeBtn.innerText = "BATTERY CHARGER MODE";
    moduleModes[ch] = "BATTERY";
    recalcBatteryLimits(ch);

    // Entering battery mode starts from a known state: output off, no droop,
    // mAh zeroed. Droop would offset the CV target against charge current.
    sendBatteryCommand(ch, { OPERATION: 0, VOUT_DROOP: 0 });
    resetMah(ch);
    chargeComplete[ch] = false;
    setBlocked(ch, '');
    setChargingState(ch, false);
  }
}

// ADVANCED CHEMISTRY MODAL HANDLERS
function openChemistryModal(ch) {
  if (charging[ch]) return;   // parameters are locked while charging

  activeModalChannel = ch;
  isPopupOpen = true;
  document.getElementById('modalTitle').innerText = `Select Battery Chemistry \u2014 Channel ${ch}`;
  document.getElementById('chemistryModal').classList.add('open');
  
  hideCustomProfileForm();
  previewChemistry(moduleChems[ch] || activePreviewChemKey || 'lifepo4');
}

function closeChemistryModal() {
  document.getElementById('chemistryModal').classList.remove('open');
  isPopupOpen = false;
  activeModalChannel = null;
  hideCustomProfileForm();
}

function previewChemistry(key) {
  hideCustomProfileForm();
  activePreviewChemKey = key;
  const profile = chemProfiles[key];
  if (!profile) return;

  // Update button active state
  document.querySelectorAll('.chem-option-btn').forEach(btn => btn.classList.remove('active'));
  const activeBtn = document.getElementById(`btn-${key}`);
  if (activeBtn) activeBtn.classList.add('active');

  // Update sidebar text
  document.getElementById('chemDetailTitle').innerText = profile.fullName;
  document.getElementById('chemCharacteristics').innerText = profile.characteristics;

  const markersList = document.getElementById('chemPhaseMarkers');
  markersList.innerHTML = profile.markers.map(m => `<li>${m}</li>`).join('');

  // Update main header title
  document.getElementById('chemGraphTitle').innerText = `${profile.fullName} - Cell Charging Curve`;

  // Render/Update Chart
  renderModalChart(profile);
}

function renderModalChart(profile) {
  const canvas = document.getElementById('chemModalChart');
  const ctx = canvas.getContext('2d');

  if (modalChart) {
    modalChart.destroy();
  }

  modalChart = new Chart(ctx, {
    type: 'line',
    data: {
      labels: profile.labels,
      datasets: [
        {
          label: 'Per-Cell Voltage (V/cell)',
          data: profile.voltageCurve,
          borderColor: '#8da61e',
          backgroundColor: 'rgba(141, 166, 30, 0.1)',
          borderWidth: 3,
          tension: 0.3,
          fill: true,
          yAxisID: 'yV'
        },
        {
          label: 'Current Profile (% of Max I)',
          data: profile.currentCurve,
          borderColor: '#f4a27e',
          borderWidth: 2,
          borderDash: [5, 5],
          tension: 0.1,
          fill: false,
          yAxisID: 'yI'
        }
      ]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      plugins: {
        legend: { display: true, position: 'top' }
      },
      scales: {
        x: {
          title: { display: true, text: 'Charging Progression Stage' },
          grid: { color: '#eee' }
        },
        yV: {
          type: 'linear',
          display: true,
          position: 'left',
          title: { display: true, text: 'Cell Voltage (V/cell)', color: '#8da61e' },
          grid: { color: '#eee' }
        },
        yI: {
          type: 'linear',
          display: true,
          position: 'right',
          title: { display: true, text: 'Current (%)', color: '#f4a27e' },
          grid: { drawOnChartArea: false },
          min: 0,
          max: 110
        }
      }
    }
  });
}

function applyChemistrySelection() {
  if (activeModalChannel !== null && activePreviewChemKey) {
    const ch = activeModalChannel;
    const selectedProfile = chemProfiles[activePreviewChemKey];

    // Store the KEY so the profile can be looked up again for VMAX
    moduleChems[ch] = activePreviewChemKey;

    const chemBtn = document.getElementById(`ch${ch}_chemBtn`);
    if (chemBtn) {
      chemBtn.innerText = selectedProfile.name;
    }

    recalcBatteryLimits(ch);
    setBlocked(ch, '');
  }
  closeChemistryModal();
}

// CUSTOM PROFILE CREATION & EDITING FUNCTIONS
function showCustomProfileForm(editKey = null) {
  document.getElementById('chemGraphView').style.display = 'none';
  document.getElementById('chemCustomForm').style.display = 'flex';
  document.getElementById('chemDetailsBox').style.opacity = '0.4';
  document.querySelectorAll('.chem-option-btn').forEach(btn => btn.classList.remove('active'));

  if (editKey && chemProfiles[editKey]) {
    editingCustomKey = editKey;
    const p = chemProfiles[editKey];
    document.getElementById('customFormTitle').innerText = "Edit Custom Chemistry Profile";
    document.getElementById('custName').value = p.name;
    document.getElementById('custFullName').value = p.fullName;
    document.getElementById('custChars').value = p.characteristics;
    // Restore the raw inputs so editing round-trips instead of resetting
    document.getElementById('custType').value = p.type || 'cccv';
    document.getElementById('custVMin').value = p.vMin;
    document.getElementById('custVNom').value = p.vNom;
    document.getElementById('custVMax').value = p.vMax;
    document.getElementById('custVFloat').value = p.vFloat;
    document.getElementById('custIMax').value = p.iMax;
  } else {
    editingCustomKey = null;
    document.getElementById('customFormTitle').innerText = "Create Custom Chemistry Profile";
    document.getElementById('custName').value = '';
    document.getElementById('custFullName').value = '';
    document.getElementById('custChars').value = '';
    document.getElementById('custVMin').value = '1.80';
    document.getElementById('custVNom').value = '2.30';
    document.getElementById('custVMax').value = '2.80';
    document.getElementById('custIMax').value = '100';
  }
  toggleCustomFormFields();
}

function hideCustomProfileForm() {
  document.getElementById('chemCustomForm').style.display = 'none';
  document.getElementById('chemGraphView').style.display = 'flex';
  document.getElementById('chemDetailsBox').style.opacity = '1';
  editingCustomKey = null;
}

function toggleCustomFormFields() {
  const type = document.getElementById('custType').value;
  const floatGroup = document.getElementById('custVFloatGroup');

  if (floatGroup) {
    floatGroup.style.display = type === '3stage' ? 'flex' : 'none';
  }
}

function saveCustomProfile() {
  const shortName = document.getElementById('custName').value.trim() || 'Custom Profile';
  const profileType = document.getElementById('custType').value;
  const fullName = document.getElementById('custFullName').value.trim() || `${shortName} (${profileType.toUpperCase()} Profile)`;
  const chars = document.getElementById('custChars').value.trim() || 'User-defined custom battery chemistry profile.';
  
  const vMin = parseFloat(document.getElementById('custVMin').value) || 2.0;
  const vNom = parseFloat(document.getElementById('custVNom').value) || 3.2;
  const vMax = parseFloat(document.getElementById('custVMax').value) || 3.65;
  const vFloat = parseFloat(document.getElementById('custVFloat').value) || (vNom * 1.05);
  const iMax = parseFloat(document.getElementById('custIMax').value) || 100;

  // Use existing key if editing, otherwise generate a new unique key
  const customKey = editingCustomKey || ('custom_' + Date.now());

  let labels = [];
  let vCurve = [];
  let iCurve = [];
  let markers = [];

  if (profileType === '3stage') {
    labels = ['0%', '25%', '50%', '75%', '90% (Abs)', '95%', '100% (Float)'];
    vCurve = [
      vMin,
      vNom,
      parseFloat((vNom + (vMax - vNom) * 0.5).toFixed(2)),
      vMax,
      vMax,
      vFloat,
      vFloat
    ];
    iCurve = [
      iMax,
      iMax,
      iMax,
      Math.round(iMax * 0.8),
      Math.round(iMax * 0.3),
      Math.round(iMax * 0.1),
      Math.round(iMax * 0.05)
    ];
    markers = [
      `${vMin.toFixed(2)}V/cell - Deep Discharge / Bulk Start`,
      `${vNom.toFixed(2)}V/cell - Bulk CC Phase`,
      `${vMax.toFixed(2)}V/cell - Max Absorption Voltage Limit`,
      `${vMax.toFixed(2)}V/cell - Current Saturation Taper`,
      `${vFloat.toFixed(2)}V/cell - Float Stage Transition`,
      `${vFloat.toFixed(2)}V/cell - Continuous Trickle Float Maintenance`
    ];
  } else {
    labels = ['0%', '20%', '40%', '60%', '80%', '90% (CV Start)', '95%', '100% (Termination)'];
    vCurve = [
      vMin,
      parseFloat((vMin + (vNom - vMin) * 0.5).toFixed(2)),
      vNom,
      parseFloat((vNom + (vMax - vNom) * 0.4).toFixed(2)),
      parseFloat((vNom + (vMax - vNom) * 0.8).toFixed(2)),
      vMax,
      vMax,
      vMax
    ];
    iCurve = [
      iMax,
      iMax,
      iMax,
      iMax,
      iMax,
      Math.round(iMax * 0.75),
      Math.round(iMax * 0.35),
      0
    ];
    markers = [
      `${vMin.toFixed(2)}V/cell - Initial Start Voltage`,
      `${vNom.toFixed(2)}V/cell - Nominal Mid-Charge Voltage`,
      `${vMax.toFixed(2)}V/cell - Constant Voltage (CV) Absorption Entry`,
      `${vMax.toFixed(2)}V/cell - Current Taper to Cutoff (< 5%)`
    ];
  }

  chemProfiles[customKey] = {
    name: shortName,
    fullName: fullName,
    characteristics: chars,
    markers: markers,
    labels: labels,
    voltageCurve: vCurve,
    currentCurve: iCurve,
    // Raw form inputs, kept so [Edit] can repopulate the form accurately
    type: profileType,
    cutoffPercent: profileType === '3stage' ? 10 : 5,
    vMin: vMin,
    vNom: vNom,
    vMax: vMax,
    vFloat: vFloat,
    iMax: iMax,
    isCustom: true
  };

  // Add or update sidebar button entry
  let existingBtn = document.getElementById(`btn-${customKey}`);
  if (!existingBtn) {
    const listContainer = document.querySelector('.chem-options-list');
    const addBtn = document.querySelector('.chem-add-custom');

    const newBtn = document.createElement('button');
    newBtn.className = 'chem-option-btn';
    newBtn.id = `btn-${customKey}`;
    newBtn.innerHTML = customProfileRowMarkup(customKey, shortName);
    newBtn.onclick = () => previewChemistry(customKey);

    listContainer.insertBefore(newBtn, addBtn);
  } else {
    existingBtn.innerHTML = customProfileRowMarkup(customKey, shortName);
  }

  previewChemistry(customKey);
}

function customProfileRowMarkup(key, shortName) {
  return `<span>${shortName}</span>` +
    `<span class="chem-row-actions">` +
      `<span class="chem-edit-link" title="Edit this profile" ` +
        `onclick="event.stopPropagation(); showCustomProfileForm('${key}')">[Edit]</span>` +
      `<span class="chem-del-link" title="Delete this profile" ` +
        `onclick="event.stopPropagation(); deleteCustomProfile('${key}')">[Del]</span>` +
    `</span>`;
}

function deleteCustomProfile(key) {
  const profile = chemProfiles[key];
  if (!profile || !profile.isCustom) return;
  if (!confirm(`Delete custom profile "${profile.name}"?`)) return;

  delete chemProfiles[key];

  const btn = document.getElementById(`btn-${key}`);
  if (btn) btn.remove();

  // Any channel still pointing at it falls back to the default profile
  for (let ch = 1; ch <= 8; ch++) {
    if (moduleChems[ch] === key) {
      moduleChems[ch] = 'lifepo4';
      const chemBtn = document.getElementById(`ch${ch}_chemBtn`);
      if (chemBtn) chemBtn.innerText = chemProfiles.lifepo4.name;
      recalcBatteryLimits(ch);
    }
  }

  if (editingCustomKey === key) hideCustomProfileForm();
  if (activePreviewChemKey === key) previewChemistry('lifepo4');
}

const loggerCharts = {};        
const loggerDataBuffers = {};   

function initWebSocket() {
  // Don't stack sockets if one is already live or still handshaking
  if (websocket &&
      (websocket.readyState === WebSocket.OPEN ||
       websocket.readyState === WebSocket.CONNECTING)) {
    return;
  }

  console.log('Trying to open WebSocket connection...');
  websocket = new WebSocket(gateway);
  websocket.onopen = onOpen;
  websocket.onclose = onClose;
  websocket.onerror = onSocketError;
  websocket.onmessage = onMessage;
}

function stopPolling() {
  if (pollIntervalId !== null) {
    clearInterval(pollIntervalId);
    pollIntervalId = null;
  }
}

async function onOpen(event) {
  console.log('WS Connection Open');
  websocket.send(JSON.stringify({ message: "Send settings" }));
  waitingForResponse = true;
  lastRequestTs = Date.now();

  // Clear any previous poller before starting a new one, otherwise each
  // reconnect adds another 250 ms timer that never goes away
  stopPolling();
  pollIntervalId = setInterval(() => {
    if (!websocket || websocket.readyState !== WebSocket.OPEN) return;

    // Stall recovery: waitingForResponse is only cleared by an 'end' frame,
    // so a missing or differently-named end frame would wedge this forever.
    if (waitingForResponse && (Date.now() - lastRequestTs) > RESPONSE_TIMEOUT_MS) {
      console.warn(`No 'end' frame within ${RESPONSE_TIMEOUT_MS} ms - releasing poller`);
      waitingForResponse = false;
    }

    // An import chosen while a request was outstanding goes as soon as the
    // poller is free -- whether an 'end' frame or stall recovery freed it.
    if (!waitingForResponse && queuedImport) {
      const job = queuedImport;
      queuedImport = null;
      sendImport(job);
      return;
    }

    if (!waitingForResponse && !waitingForImport && !isPopupOpen) {
      websocket.send(JSON.stringify({ message: "Send readings" }));
      waitingForResponse = true;
      lastRequestTs = Date.now();
    }
  }, 250);
}

function onSocketError(event) {
  console.log('WS error', event);
}

function onClose(event) {
  console.log('WS connection closed');
  stopPolling();

  // The device closes the socket when Store NVM hits a PMBus error (a
  // handler returning ESP_FAIL closes it). Without this, the reconnect's
  // "Send settings" round would be verified as if the store had succeeded.
  abortImport('the connection to the DevKit closed before the NVM store was confirmed.');

  // Always retry. The poller itself is gated on isPopupOpen, so reconnecting
  // while the modal is open is harmless - and refusing to would leave the
  // page permanently disconnected if the drop happened mid-selection.
  waitingForResponse = false;
  setTimeout(initWebSocket, 2000);
}

// Safe scaling for values arriving from the ESP32. parseFloat on a missing
// key yields NaN, and NaN.toFixed() returns the string "NaN" - which is
// truthy, so a trailing `|| 0` never fires. Check the number instead.
function scaled(raw, divisor, dp) {
  const v = parseFloat(raw);
  return (Number.isFinite(v) ? v / divisor : 0).toFixed(dp);
}

// Null-safe DOM writers. Previously a single missing element threw partway
// through the settings branch, aborting the frame silently.
function setText(id, value) {
  const el = document.getElementById(id);
  if (el) el.innerText = value; else console.warn(`missing element #${id}`);
}

function setVal(id, value) {
  const el = document.getElementById(id);
  if (el) el.value = value; else console.warn(`missing element #${id}`);
}

function onMessage(event) {
  if (DEBUG_FRAMES) console.log('RX', event.data);

  let obj;
  try {
    obj = JSON.parse(event.data);
  } catch (err) {
    console.error('Frame is not valid JSON:', event.data);
    waitingForResponse = false;
    return;
  }

  try {
    handleFrame(obj);
  } catch (err) {
    // Never let one malformed frame wedge the poller permanently
    console.error('Frame handling failed:', err, obj);
    waitingForResponse = false;
  }
}

function handleFrame(obj) {
  let ch = obj[`ch`];

  if (obj[`type`] !== 'end') {
    if (!(ch >= 1 && ch <= 8)) {
      console.warn(`Frame has no usable channel number (ch=${ch})`, obj);
      return;
    }
  }

  if (obj[`type`] === 'settings') {
    if (pendingImportVerification) importReadback[ch] = obj;

    if (obj[`model${ch}`] === undefined) {
      console.warn(`Settings frame for ch${ch} has no 'model${ch}' key - card will stay hidden`, obj);
    }

    moduleModels[ch] = String(obj[`model${ch}`] || '').trim().toUpperCase();
    setText(`ch${ch}_model`, obj[`model${ch}`] || '\u2014');
    setText(`ch${ch}_serial`, obj[`serial${ch}`] || '\u2014');
    
    const chart = loggerCharts[ch];
    const wrapper = document.getElementById(`ch${ch}_chartWrapper`);
            
    if (chart) {
      chart.config.options.plugins.channelInfoText = `Channel ${ch} \u2014 ${obj[`model${ch}`]} (S/N: ${obj[`serial${ch}`]})`;
      
      const maxV = obj[`model${ch}`] === 'OP1D' ? 10 : (obj[`model${ch}`] === 'OP2D' || obj[`model${ch}`] === 'OPA2D') ? 20 : (obj[`model${ch}`] === 'OP3D' || obj[`model${ch}`] === 'OPA3D') ? 35 : 70;
      const maxI = obj[`model${ch}`] === 'OP1D' ? 30 : (obj[`model${ch}`] === 'OP2D' || obj[`model${ch}`] === 'OPA2D') ? 20 : (obj[`model${ch}`] === 'OP3D' || obj[`model${ch}`] === 'OPA3D') ? 10 : 5;
      const maxP = obj[`model${ch}`] === 'OP1D' ? 200 : (obj[`model${ch}`] === 'OP2D' || obj[`model${ch}`] === 'OPA2D') ? 250 : 500;
      
      chart.config.options.scales.y.max = maxV;
      chart.config.options.scales.y1.max = maxI;
      chart.config.options.scales.y2.max = maxP;
      chart.update();
    }
      
    setText(`ch${ch}_hardware`, "HW: " + (obj[`hw${ch}`] || '\u2014'));
    setVal(`ch${ch}_vset`, scaled(obj[`vset${ch}`], 100, 2));
    setVal(`ch${ch}_droop`, parseInt(obj[`drp${ch}`]) || 0);
    setVal(`ch${ch}_ov`, scaled(obj[`ov${ch}`], 100, 2));
    setVal(`ch${ch}_uv`, scaled(obj[`uv${ch}`], 100, 2));
    setVal(`ch${ch}_ton`, scaled(obj[`ton${ch}`], 10, 1));
    setVal(`ch${ch}_ocp`, scaled(obj[`ocp${ch}`], 100, 2));
    zoneval[ch] = parseInt(obj[`zone${ch}`] || 0);
    setVal(`ch${ch}_zone`, zoneval[ch] & 0xff);
    setVal(`ch${ch}_addr`, parseInt(obj[`addr${ch}`] || 0));

    // Model is known now, so re-apply the hardware envelope
    clampMaxCurrent(ch);
    recalcBatteryLimits(ch);

    const mod = document.getElementById(`ch${ch}_module`);
    if (obj[`model${ch}`] === "") {
      if (mod) mod.style.display = 'none';
      if (wrapper) wrapper.style.display = 'none'; 
    } else {
      // 'flex', not 'block' - the card is a flex column so the mode
      // button can be pinned to the bottom with margin-top:auto
      if (mod) mod.style.display = 'flex';
      if (wrapper) wrapper.style.display = 'block';
    }
    
    setText(`ch${ch}_termBtn`, obj[`snsterm${ch}`] == 1 ? "SENSE: INT" : "SENSE: EXT");
    
  } else if (obj[`type`] === 'readings') {
    document.getElementById(`ch${ch}_voltage`).innerText = scaled(obj[`v${ch}`], 100, 2);
    document.getElementById(`ch${ch}_current`).innerText = scaled(obj[`i${ch}`], 100, 2);
    document.getElementById(`ch${ch}_power`).innerText = scaled(obj[`p${ch}`], 10, 1);
    document.getElementById(`ch${ch}_temp`).innerText = scaled(obj[`t${ch}`], 100, 2);
    
    // mAh is integrated in the browser from the current readings rather than
    // taken from obj.mah, so it zeroes cleanly on RESET.
    integrateCharge(ch, obj);
    
    if (loggerCharts[ch]) {
      const now = new Date().toLocaleTimeString();
      const buffer = loggerDataBuffers[ch];
      const w = window.innerWidth;
      let maxPoints = w <= 600 ? 75 : w <= 1000 ? 150 : 300;

      if (buffer.labels.length >= maxPoints) {
        buffer.labels.shift();
        buffer.voltage.shift();
        buffer.current.shift();
        buffer.power.shift();
        buffer.temp.shift();
      }

      buffer.labels.push(now);
      buffer.voltage.push(parseFloat(obj[`v${ch}`]) / 100);
      buffer.current.push(parseFloat(obj[`i${ch}`]) / 100);
      buffer.power.push(parseFloat(obj[`p${ch}`]) / 10);
      buffer.temp.push(parseFloat(obj[`t${ch}`]) / 100);

      loggerCharts[ch].data.labels = buffer.labels;
      loggerCharts[ch].data.datasets[0].data = buffer.voltage;
      loggerCharts[ch].data.datasets[1].data = buffer.current;
      loggerCharts[ch].data.datasets[2].data = buffer.power;
      loggerCharts[ch].data.datasets[3].data = buffer.temp;
      loggerCharts[ch].update();
    }
    
    document.getElementById(`ch${ch}_commStatus`).className = 'indicator ' + ((obj[`StatByte${ch}`] & 2 ) === 2 ? 'red' : 'green');
    document.getElementById(`ch${ch}_tempStatus`).className = 'indicator ' + ((obj[`StatByte${ch}`] & 4 ) === 4 ? 'red' : 'green');
    document.getElementById(`ch${ch}_ovLabel`).className = 'setting-label-indicator ' + ((obj[`StatVout${ch}`] & 64 ) === 64 ?  'red' : 'green');
    document.getElementById(`ch${ch}_uvLabel`).className = 'setting-label-indicator ' + ((obj[`StatVout${ch}`] & 32 ) === 32 ?  'red' : 'green');
    document.getElementById(`ch${ch}_tonLabel`).className = 'setting-label-indicator ' + ((obj[`StatVout${ch}`] & 4 ) === 4 ?  'red' : 'green');
    document.getElementById(`ch${ch}_ocpLabel`).className = 'setting-label-indicator ' + ((obj[`StatByte${ch}`] & 16 ) === 16 ?  'red' : 'green');
    
    // CV/CC state: the OCP status bit means the channel is in current limit,
    // i.e. constant current. Output enabled but not in current limit = CV.
    const cvccPill = document.getElementById(`ch${ch}_cvccPill`);
    if (cvccPill) {
      const outputOn = (obj[`StatByte${ch}`] & 64) !== 64;
      const inCC = (obj[`StatByte${ch}`] & 16) === 16;
      if (blockedReason[ch]) {
        cvccPill.innerText = blockedReason[ch];
        cvccPill.className = 'vp-pill vp-pill-fault';
      } else if (chargeComplete[ch]) {
        cvccPill.innerText = 'CHARGE COMPLETE';
        cvccPill.className = 'vp-pill vp-pill-complete';
      } else if (!outputOn) {
        cvccPill.innerText = 'IDLE';
        cvccPill.className = 'vp-pill vp-pill-gray';
      } else if (floatStage[ch]) {
        cvccPill.innerText = 'FLOAT';
        cvccPill.className = 'vp-pill vp-pill-amber';
      } else if (inCC) {
        cvccPill.innerText = 'CC';
        cvccPill.className = 'vp-pill vp-pill-green';
      } else {
        cvccPill.innerText = 'CV';
        cvccPill.className = 'vp-pill vp-pill-amber';
      }
    }

    const btn = document.getElementById(`ch${ch}_onoffBtn`);
    btn.innerText = ((obj[`StatByte${ch}`] & 64 ) === 64 ?  'OFF' : 'ON');
    if (btn.innerText === "ON") {
      btn.classList.remove('off');
      btn.classList.add('on');
    } else {
      btn.classList.remove('on');
      btn.classList.add('off');
    }
    updateChargeControl(ch, obj);

  } else if (obj[`type`] === 'end') {
    waitingForResponse = false;

    // The Store NVM readback is a 'settings' round, so it fills
    // importReadback. An 'end' with nothing captured is from some other
    // round (e.g. a slow readings poll released by stall recovery) and is
    // not the result -- keep waiting.
    if (pendingImportVerification && Object.keys(importReadback).length > 0) {
      clearTimeout(importTimeoutId);
      importTimeoutId = null;
      verifyImportedSettings(pendingImportVerification);
      pendingImportVerification = null;
      waitingForImport = false;
    }

    // Not while an import is pending: applySetting() and
    // sendBatteryCommand() would push each command straight back onto
    // cmds, and the drain would loop forever.
    if (!waitingForImport) drainCommandQueue();

  } else {
    console.warn(`Unrecognised frame type '${obj['type']}' - expected settings / readings / end`, obj);
  }
}

// Generate Module DOM Nodes
const container = document.getElementById('modulesContainer');
for (let i = 1; i <= 8; i++) {
  container.insertAdjacentHTML('beforeend', createModule(i));
}
      
// Single writer for every settings message
function writeSettings(ch, params) {
  const payload = Object.assign({ channel: ch }, params);

  if (!websocket || websocket.readyState !== WebSocket.OPEN) {
    console.log(`CH${ch} settings write (offline, not sent):`, payload);
    return;
  }

  websocket.send(JSON.stringify({
    message: "Import settings",
    settings: [payload]
  }, null, 2));
}

function drainCommandQueue() {
  while (cmds.length > 0) {
    const next = cmds.shift();
    if (next.kind === 'payload') {
      sendBatteryCommand(next.ch, next.params);
    } else {
      applySetting(next.ch, next.field, next.param);
    }
  }
}

async function applySetting(ch, field, param) {
  if (waitingForResponse || waitingForImport) {
    cmds.push({ kind: 'field', ch, field, param });
    return;
  }

  let value = 0;

  if (!['term','sns','on','off','strnvm','start','hold','stop'].includes(field)) {
    value = parseFloat(document.getElementById(`ch${ch}_${field}`).value);
  }

  if (field === 'on') value = 1;
  if (field === 'term') value = 1;

  writeSettings(ch, { [param]: value });
}

// The ZONE input only ever showed the low byte (zoneval & 0xff), but
// exportSettings writes back the whole zoneval word. Without this the export
// carried the last value received from the module, silently discarding any
// edit. Upper bits are preserved rather than zeroed.
function onZoneChange(ch) {
  const input = document.getElementById(`ch${ch}_zone`);
  const low = (parseInt(input.value) || 0) & 0xff;
  const upper = (zoneval[ch] || 0) & ~0xff;
  zoneval[ch] = upper | low;
  applySetting(ch, 'zone', 'ZONE_CONFIG');
}

function toggleOutput(ch) {
  const btn = document.getElementById(`ch${ch}_onoffBtn`);
  if (btn.innerText === "ON") {
    applySetting(ch, 'off', 'OPERATION');
  } else {
    applySetting(ch, 'on','OPERATION');
  }
}

function toggleTerm(ch) {
  const termBtn = document.getElementById(`ch${ch}_termBtn`);
  if (termBtn.innerText === "SENSE: EXT") {
    applySetting(ch, 'term', 'MFR_SETTINGS');
  } else {
    applySetting(ch, 'sns', 'MFR_SETTINGS');
  }
}

const chartChannelInfo = {
  id: 'channelInfoPlugin',
  afterDraw(chart) {
    const opts = chart.config?.options?.plugins;
    const label = opts?.channelInfoText;
    if (!label) return;
    const chartArea = chart.chartArea;
    const ctx = chart.ctx;
    if (!chartArea || !ctx) return;
    const isNarrow = chart.width <= 1000;
    ctx.save();
    ctx.font = isNarrow ? 'bold 11px sans-serif' : 'bold 12px sans-serif';
    ctx.fillStyle = 'black';
    ctx.textBaseline = 'top';
    const y = isNarrow ? (chartArea.top + 4) : (chartArea.top - 20);
    ctx.fillText(label, chartArea.left + 10, y);
    ctx.restore();
  }
};

function setupLoggerCharts() {
  const loggerCont = document.getElementById("loggerContainer");

  for (let ch = 1; ch <= 8; ch++) {
    const wrapper = document.createElement("div");
    wrapper.id = `ch${ch}_chartWrapper`;
    wrapper.style.marginBottom = "20px";
    wrapper.style.display = "none";
    wrapper.style.maxWidth = "100%";
    wrapper.style.width = "100%";

    const canvas = document.createElement("canvas");
    canvas.id = `ch${ch}_chart`;
    canvas.style.width = "100%";
    canvas.style.display = "block";
    const w = window.innerWidth;
    canvas.style.height = w <= 600 ? "90px" : w <= 1200 ? "60px" : "35px";

    wrapper.appendChild(canvas);
    loggerCont.appendChild(wrapper);

    loggerDataBuffers[ch] = { labels: [], voltage: [], current: [], power: [], temp: [] };

    loggerCharts[ch] = new Chart(canvas, {
      type: 'line',
      data: {
        labels: [],
        datasets: [
          { label: 'Voltage (V)', data: [], borderColor: 'blue', fill: false, yAxisID: 'y' },
          { label: 'Current (A)', data: [], borderColor: 'green', fill: false, yAxisID: 'y1' },
          { label: 'Power (W)', data: [], borderColor: 'orange', fill: false, yAxisID: 'y2' },
          { label: 'Temp (\u00B0C)', data: [], borderColor: 'red', fill: false, yAxisID: 'y3' }
        ]
      },
      options: {
        responsive: true,
        animation: false,
        plugins: { legend: { display: true, position: 'top' }, channelInfoText: `Channel ${ch}` },
        scales: {
          x: { display: false },
          y: { type: 'linear', display: 'auto', position: 'left', min: 0, max: 70, title: { display: true, text: 'Voltage (V)' } },
          y1: { type: 'linear', display: 'auto', position: 'left', min: 0, max: 30, grid: { drawOnChartArea: false }, title: { display: true, text: 'Current (A)' } },
          y2: { type: 'linear', display: 'auto', position: 'right', beginAtZero: true, min: 0, max: 500, grid: { drawOnChartArea: false }, title: { display: true, text: 'Power (W)' } },
          y3: { type: 'linear', display: 'auto', position: 'right', min: -20, max: 120, grid: { drawOnChartArea: false }, title: { display: true, text: 'Temperature (\u00B0C)' } }
        }
      },
      plugins: [chartChannelInfo]
    });
  }
}

async function storeAllNVM() {
  if (websocket && websocket.readyState === WebSocket.OPEN) {
    websocket.send(JSON.stringify({ message: "Store NVM" }));
  }
}

function exportSettings() {
  let settings = [];
  for (let ch = 1; ch <= 8; ch++) {
    const modelText = document.getElementById(`ch${ch}_model`)?.innerText;
    if (!modelText || modelText === '\u2014') continue;

    const onoff_value = document.getElementById(`ch${ch}_onoffBtn`).innerText === "ON" ? 1 : 0;
    const snsterm_value = document.getElementById(`ch${ch}_termBtn`).innerText === "SENSE: INT" ? 1 : 0;

    settings.push({
      channel: ch,
      VOUT_COMMAND: parseFloat(document.getElementById(`ch${ch}_vset`).value) || 0,
      VOUT_DROOP: parseInt(document.getElementById(`ch${ch}_droop`).value) || 0,
      VOUT_OV_WARN_LIMIT: parseFloat(document.getElementById(`ch${ch}_ov`).value) || 0,
      VOUT_UV_WARN_LIMIT: parseFloat(document.getElementById(`ch${ch}_uv`).value) || 0,
      TON_MAX_FLT: parseFloat(document.getElementById(`ch${ch}_ton`).value) || 0,
      IOUT_OC_FAULT_LIMIT: parseFloat(document.getElementById(`ch${ch}_ocp`).value) || 0,
      MFR_SETTINGS: snsterm_value,
      OPERATION: onoff_value,
      ZONE_CONFIG: zoneval[ch] || 0,
      MFR_SMBUS_ADDRESS: parseInt(document.getElementById(`ch${ch}_addr`).value) || 0
    });
  }

  const blob = new Blob([JSON.stringify({ date: new Date().toISOString(), message: "Import settings", settings }, null, 2)], { type: 'application/json' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = 'NEVO_Settings.json';
  a.click();
  URL.revokeObjectURL(url);
}

function importSettings() {
  const input = document.createElement('input');
  input.type = 'file';
  input.accept = 'application/json';

  // waitingForImport is raised only once a valid file has been read, and is
  // held until the post-store readback has been verified (or the import is
  // aborted) -- a cancelled dialog or a rejected file never raises it.
  input.onchange = event => {
    const file = event.target.files[0];
    if (!file) return;

    const reader = new FileReader();

    reader.onload = e => {
      // An import ends with Store NVM, which inhibits every output -- that
      // would cut off a charge in progress. Same lockout as the mode toggle.
      const chargingChannels = Object.keys(charging).filter(ch => charging[ch]);
      if (chargingChannels.length) {
        alert(`Import refused: channel(s) ${chargingChannels.join(', ')} are charging. Stop charging first -- the NVM store at the end of an import switches all outputs off briefly.`);
        return;
      }

      let parsed;
      try {
        parsed = JSON.parse(e.target.result);
      } catch (err) {
        alert('Invalid JSON file: ' + err.message);
        return;
      }
      const problems = validateImportFile(parsed);
      if (problems.length) {
        const shown = problems.slice(0, 15);
        const more = problems.length > shown.length ? `\n...and ${problems.length - shown.length} more` : '';
        alert(`Import rejected -- nothing was sent to the DevKit.\n\n${shown.join('\n')}${more}`);
        return;
      }

      waitingForImport = true;
      importTimeoutId = setTimeout(
        () => abortImport(`no response from the DevKit within ${IMPORT_TIMEOUT_MS / 1000} s.`),
        IMPORT_TIMEOUT_MS);
      // Rebuilt from the validated object rather than sending the file text
      // as-is: the file's own "message" would otherwise be dispatched, and
      // JSON.parse keeps only the last of any duplicated key while the
      // firmware's cJSON would apply every copy.
      const job = {
        payload: JSON.stringify({ message: 'Import settings', settings: parsed.settings }),
        settings: parsed.settings,
      };
      if (waitingForResponse) {
        queuedImport = job;   // the poller sends it once the outstanding request clears
      } else {
        sendImport(job);
      }
    };

    reader.onerror = () => {
      alert('Import failed: the file could not be read.');
    };

    reader.readAsText(file);
  };

  input.click();
}

function sendImport(job) {
  if (!websocket || websocket.readyState !== WebSocket.OPEN) {
    abortImport('the connection to the DevKit is not open.');
    return;
  }
  pendingImportVerification = job.settings;
  importReadback = {};
  waitingForResponse = true;
  lastRequestTs = Date.now();
  websocket.send(job.payload);
  // Apply, then persist -- same two steps as importing and then pressing
  // STORE ALL NVM. The server handles frames in the order received, so the
  // import's per-channel apply loop has finished before Store NVM starts.
  websocket.send(JSON.stringify({ message: "Store NVM" }));
}

// Abandons a pending import so polling and queued commands resume, and tells
// the user plainly -- a failed or unconfirmed store must never read as success.
function abortImport(reason) {
  clearTimeout(importTimeoutId);
  importTimeoutId = null;
  if (!waitingForImport) return;
  pendingImportVerification = null;
  queuedImport = null;
  waitingForImport = false;
  waitingForResponse = false;
  alert(`Import NOT confirmed: ${reason}\n\nThe imported settings may be active now but not stored to NVM, so they could be lost at the next power cycle. Check each module's settings and outputs, then import again.`);
}

// Only accepts files shaped exactly like exportSettings() output: the ten
// keys it writes (IMPORT_VERIFY_FIELDS) plus channel, with numeric values.
// Anything else is rejected before it reaches the device -- in particular,
// PMBus command names the firmware knows but has no scpi_str for (e.g.
// MFR_VOUT_MAX) would crash it.
function validateImportFile(parsed) {
  const problems = [];
  if (parsed === null || typeof parsed !== 'object' || Array.isArray(parsed)) {
    return ['The file is not a settings export (expected a JSON object).'];
  }
  for (const key of Object.keys(parsed)) {
    if (!['date', 'message', 'settings'].includes(key)) {
      problems.push(`Unexpected top-level key "${key}".`);
    }
  }
  if (parsed.message !== 'Import settings') {
    problems.push(`"message" must be "Import settings" (found ${JSON.stringify(parsed.message)}).`);
  }
  if (!Array.isArray(parsed.settings) || parsed.settings.length === 0) {
    problems.push('"settings" must be a non-empty list of channels.');
    return problems;
  }
  const seenChannels = new Set();
  parsed.settings.forEach((entry, i) => {
    const where = `Entry ${i + 1}`;
    if (entry === null || typeof entry !== 'object' || Array.isArray(entry)) {
      problems.push(`${where}: not an object.`);
      return;
    }
    const ch = entry.channel;
    if (!Number.isInteger(ch) || ch < 1 || ch > 8) {
      problems.push(`${where}: "channel" must be a whole number from 1 to 8 (found ${JSON.stringify(ch)}).`);
    } else if (seenChannels.has(ch)) {
      problems.push(`${where}: channel ${ch} appears more than once.`);
    } else {
      seenChannels.add(ch);
    }
    const label = Number.isInteger(ch) ? `CH${ch}` : where;
    for (const [key, value] of Object.entries(entry)) {
      if (key === 'channel') continue;
      if (!IMPORT_VERIFY_FIELDS[key]) {
        problems.push(`${label}: "${key}" is not a setting the export writes.`);
      } else if (typeof value !== 'number' || !Number.isFinite(value)) {
        problems.push(`${label}: "${key}" must be a number (found ${JSON.stringify(value)}).`);
      }
    }
  });
  return problems;
}

// Compares the imported settings against the raw values the device reported
// after Store NVM recalled them, so an import is confirmed to have genuinely
// persisted -- not just applied to the live registers.
function verifyImportedSettings(imported) {
  const mismatches = [];
  const notChecked = [];
  let checked = 0;
  for (const chSetting of imported) {
    const ch = chSetting.channel;
    const readback = importReadback[ch];
    if (!readback) {
      mismatches.push(`CH${ch}: no settings read back from the device (channel not detected?)`);
      continue;
    }
    for (const [field, value] of Object.entries(chSetting)) {
      if (field === 'channel') continue;
      const spec = IMPORT_VERIFY_FIELDS[field];
      if (!spec) {
        notChecked.push(`CH${ch} ${field}`);
        continue;
      }
      checked++;
      const fileValue = Number(value);
      const rawActual = readback[`${spec.raw}${ch}`];
      const expected = spec.toRaw(fileValue);
      const actual = spec.fromRaw ? spec.fromRaw(rawActual) : rawActual;
      if (Number.isNaN(fileValue) || actual !== expected) {
        mismatches.push(`CH${ch} ${field}: file ${value} (raw ${expected}), device reports ${actual}`);
      }
    }
  }
  const notCheckedNote = notChecked.length
    ? `\n\nNot checked (no readback available): ${notChecked.join(', ')}`
    : '';
  if (mismatches.length === 0) {
    alert(`Import verified: ${checked} setting(s) match what was recalled from NVM.${notCheckedNote}`);
  } else {
    alert(`Import NVM verification found ${mismatches.length} mismatch(es):\n\n${mismatches.join('\n')}${notCheckedNote}`);
  }
}

// DCOM Mocking Initializer for All 8 Channels
function mockAllModules() {
  for (let ch = 1; ch <= 8; ch++) {
    onMessage({
      data: JSON.stringify({
        type: 'settings',
        ch: ch,
        [`model${ch}`]: 'OP4D',
        [`serial${ch}`]: `SN00${ch}`,
        [`hw${ch}`]: 'v1.0',
        [`vset${ch}`]: '1200',
        [`drp${ch}`]: '0',
        [`ov${ch}`]: '1500',
        [`uv${ch}`]: '1000',
        [`ton${ch}`]: '100',
        [`ocp${ch}`]: '500',
        [`zone${ch}`]: '1',
        [`addr${ch}`]: `${ch}`
      })
    });

    onMessage({
      data: JSON.stringify({
        type: 'readings',
        ch: ch,
        [`v${ch}`]: '1200',
        [`i${ch}`]: '250',
        [`p${ch}`]: '3000',
        [`t${ch}`]: '3500',
        [`mah${ch}`]: '15000',
        [`StatByte${ch}`]: 0,
        [`StatVout${ch}`]: 0
      })
    });
  }
}

// Initialization on DOM Load
window.addEventListener('DOMContentLoaded', () => {
  setupLoggerCharts();

  // USE_MOCK is declared at the top of this file.
  // true  = populate all 8 channels with fake DCOM data, no WebSocket
  // false = live connection to the ESP32, no mock data
  if (USE_MOCK) {
    console.warn('USE_MOCK is true - showing simulated DCOM data, not live hardware');
    mockAllModules();
  } else {
    initWebSocket();
  }

  // Seed the derived VMAX field for every channel
  for (let ch = 1; ch <= 8; ch++) {
    recalcBatteryLimits(ch);
  }
});
//
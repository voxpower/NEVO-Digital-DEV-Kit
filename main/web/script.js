const gateway = `ws://${window.location.hostname}/ws`;
 let websocket;
 let isPopupOpen = false;
 let waitingForResponse = false;
 let waitingForImport = false;
 const cmds = [];
 let zoneval = [];


   function openTab(evt, tabId) {
     document.querySelectorAll('.tab-content').forEach(t => t.classList.remove('active-tab'));
     document.querySelectorAll('.header-tab').forEach(b => b.classList.remove('active'));
     document.getElementById(tabId).classList.add('active-tab');
     evt.currentTarget.classList.add('active');
     
     // Show the command bar container only if the instrument tab is selected
	 const CommandContainer = document.getElementById('CommandContainer');
	 if (tabId === 'instrumentTab') {
	   CommandContainer.style.display = 'flex';
	 } else {
	   CommandContainer.style.display = 'none';
	 }
   }

   function createModule(ch) {
     return `
     <div class="psu-module" id="ch${ch}_module">
       <div class="module-header">
         <div class="channel-number">${ch}</div>
         <div class="module-info">     
           <div id="ch${ch}_model"></div>
           <div id="ch${ch}_serial"></div>
         </div>
       </div>
       <div class="section-title">Readings</div>
       <div class="readings">
         <div class="reading"><span class="value" id="ch${ch}_voltage">--</span><span class="unit">&nbsp;&nbsp;V</span></div>
         <div class="reading"><span class="value" id="ch${ch}_current">--</span><span class="unit">&nbsp;&nbsp;A</span></div>
         <div class="reading"><span class="value" id="ch${ch}_power">--</span><span class="unit">&nbsp;&nbsp;W</span></div>
         <div class="reading"><span class="value" id="ch${ch}_temp">--</span><span class="unit">&nbsp;°C</span></div>
       </div>
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
             onchange="applySetting(${ch}, 'zone','ZONE_CONFIG')"
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
     </div>`;
   }

   const loggerCharts = {};        // Stores Chart instances for each channel
   const loggerDataBuffers = {};   // Stores live data arrays for each channel
   
   function initWebSocket() {
  if (isPopupOpen) return;
  console.log('Trying to open WebSocket connection...');
  websocket = new WebSocket(gateway);
  websocket.onopen = onOpen;
  websocket.onclose = onClose;
  websocket.onmessage = onMessage;
}

async function onOpen(event) {
  console.log('WS Connection Open');
  //Read settting here.
  websocket.send(JSON.stringify({ message: "Send settings" }));
  waitingForResponse = true;
  //Start timed readings request here.
   intervalId = setInterval(() => {
	   if (!waitingForResponse && !waitingForImport)
	   {
       websocket.send(JSON.stringify({ message: "Send readings" }));
       waitingForResponse = true;
       const now = new Date();
       const timestamp = `${now.getHours()}:${now.getMinutes()}:${now.getSeconds()}.${now.getMilliseconds()}`;
       console.log(`${timestamp} - WebSocket message sent.`);
	   }
   }, 250);
}


function onClose(event) {
  console.log('WS connection closed');
  if (!isPopupOpen) {
    console.log('Reconnecting WebSocket...');
    setTimeout(initWebSocket, 2000);
  }
}

function onMessage(event) {
  const obj = JSON.parse(event.data);
  console.log(obj);

	  let ch = obj[`ch`];
	  if (obj[`type`] === 'settings') {
		//Fixed values
		document.getElementById(`ch${ch}_model`).innerText = obj[`model${ch}`] || '—';
	    document.getElementById(`ch${ch}_serial`).innerText = obj[`serial${ch}`] || '—';
		
	    const chart = loggerCharts[ch];
		const wrapper = document.getElementById(`ch${ch}_chartWrapper`);
	    		
	    chart.config.options.plugins.channelInfoText = `Channel ${ch} — ${obj[`model${ch}`]} (S/N: ${obj[`serial${ch}`]})`;
	    
	    const maxV = (function() {
			switch (obj[`model${ch}`]) {
			case 'OP1D':
				return 10;
			  case 'OP2D':
				return 20;
			  case 'OPA2D':
				return 20;
			  case 'OP3D':
				return 35;
			  case 'OPA3D':
				return 35;
			  case 'OP4D':
				return 70;
			  default:
				return 70;
			}
	    })();
	    const maxI = (function() {
			switch (obj[`model${ch}`]) {
			  case 'OP1D':
				return 30;
			  case 'OP2D':
				return 20;
			  case 'OPA2D':
				return 30;
			  case 'OP3D':
				return 10;
			  case 'OPA3D':
				return 20;
			  case 'OP4D':
				return 5;
			  default:
				return 30;
			}
	    })();
	    const maxP = (function() {
			switch (obj[`model${ch}`]) {
			case 'OP1D':
				return 200;
			  case 'OP2D':
				return 250;
			  case 'OPA2D':
				return 400;
			  case 'OP3D':
				return 250;
			  case 'OPA3D':
				return 500;
			  case 'OP4D':
				return 250;
			  default:
				return 500;
			}
	    })();
			
		if (chart) {
			chart.config.options.scales.y.max = maxV;
            chart.config.options.scales.y1.max = maxI;
            chart.config.options.scales.y2.max = maxP;
        }
    	chart.update();
    	
	    document.getElementById(`ch${ch}_hardware`).innerText = "HW: " + (obj[`hw${ch}`] || '—');
		document.getElementById(`ch${ch}_vset`).value = (parseFloat(obj[`vset${ch}`]) / 100).toFixed(2) || 0;
	    document.getElementById(`ch${ch}_droop`).value = parseInt(obj[`drp${ch}`]) || 0;
	    document.getElementById(`ch${ch}_ov`).value = (parseFloat(obj[`ov${ch}`]) / 100).toFixed(2)  || 0;
		document.getElementById(`ch${ch}_uv`).value = (parseFloat(obj[`uv${ch}`]) / 100).toFixed(2)  || 0;
		document.getElementById(`ch${ch}_ton`).value = (parseFloat(obj[`ton${ch}`]) /10).toFixed(1) || 0;
	    document.getElementById(`ch${ch}_ocp`).value = (parseFloat(obj[`ocp${ch}`]) / 100).toFixed(2)  || 0;
	    zoneval[ch] = parseInt(obj[`zone${ch}`] || 0);
   		document.getElementById(`ch${ch}_zone`).value = (zoneval[ch] & 0xff);
	    document.getElementById(`ch${ch}_addr`).value = parseInt(obj[`addr${ch}`] || 0);
	    if (obj[`model${ch}`] == "") 
	    {
	    	document.querySelector(`#modulesContainer .psu-module:nth-child(${ch})`).style.display = 'none';
	    	wrapper.style.display = 'none';	
	    } else {
	    	document.querySelector(`#modulesContainer .psu-module:nth-child(${ch})`).style.display = 'block';
	    	wrapper.style.display = 'block';
	    };
	    
	    if (obj[`snsterm${ch}`] == 1) {
    		 document.getElementById(`ch${ch}_termBtn`).innerText = "SENSE: INT";
    	 } else {
    		 document.getElementById(`ch${ch}_termBtn`).innerText = "SENSE: EXT";
    	 }
	    
	  } else if (obj[`type`] === 'readings') {
		//Readback Values
	    document.getElementById(`ch${ch}_voltage`).innerText = (parseFloat(obj[`v${ch}`]) / 100).toFixed(2);
	    document.getElementById(`ch${ch}_current`).innerText = (parseFloat(obj[`i${ch}`]) / 100).toFixed(2);
	    document.getElementById(`ch${ch}_power`).innerText = (parseFloat(obj[`p${ch}`]) / 10).toFixed(1);
	    document.getElementById(`ch${ch}_temp`).innerText = (parseFloat(obj[`t${ch}`]) / 100).toFixed(2);
	    
	    //Charts
	    if (loggerCharts[ch]) 
	    {
	       const now = new Date().toLocaleTimeString(); // Label (timestamp)
	
	       const buffer = loggerDataBuffers[ch];
	
	       const w = window.innerWidth;
	       let maxPoints;

	       if (w <= 600) {
	    	 maxPoints = 75;
	       } else if (w <= 1000) {
	    	 maxPoints = 150;
	       } else {
	    	 maxPoints = 300;
	       }

	       if (buffer.labels.length >= maxPoints)
	       {
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
	    
	    //Indicators
	    document.getElementById(`ch${ch}_commStatus`).className = 'indicator ' + ((obj[`StatByte${ch}`] & 2 ) === 2 ? 'red' : 'green');
	    document.getElementById(`ch${ch}_tempStatus`).className = 'indicator ' + ((obj[`StatByte${ch}`] & 4 ) === 4 ? 'red' : 'green');
	    document.getElementById(`ch${ch}_ovLabel`).className = 'setting-label-indicator ' + ((obj[`StatVout${ch}`] & 64 ) === 64 ?  'red' : 'green');
	    document.getElementById(`ch${ch}_uvLabel`).className = 'setting-label-indicator ' + ((obj[`StatVout${ch}`] & 32 ) === 32 ?  'red' : 'green');
	    document.getElementById(`ch${ch}_tonLabel`).className = 'setting-label-indicator ' + ((obj[`StatVout${ch}`] & 4 ) === 4 ?  'red' : 'green');
	    document.getElementById(`ch${ch}_ocpLabel`).className = 'setting-label-indicator ' + ((obj[`StatByte${ch}`] & 16 ) === 16 ?  'red' : 'green');
	    
	    //Buttons
	    const btn = document.getElementById(`ch${ch}_onoffBtn`);
	    btn.innerText = ((obj[`StatByte${ch}`] & 64 ) === 64 ?  'OFF' : 'ON');
	    if (btn.innerText === "ON") {
	      btn.classList.remove('off');
	      btn.classList.add('on');
	    } else {
	      btn.classList.remove('on');
	      btn.classList.add('off');
	    }
	} else if (obj[`type`] === 'end') {
	
	waitingForResponse = false;
    
	//Process any queued commands
	while (cmds.length > 0) {
	    const nextCmd = cmds.shift(); 
	    applySetting(nextCmd.ch, nextCmd.field, nextCmd.param);
	  }
   }
}


const container = document.getElementById('modulesContainer');
console.log('create modules')
for (let i = 1; i <= 8; i++) {
  container.insertAdjacentHTML('beforeend', createModule(i));
}
	    
	    
async function applySetting(ch, field, param) {
	
	//Push command to queue
	if (waitingForResponse || waitingForImport) {
	    cmds.push({ ch, field, param });
	    return;
	  }

	//Process command
	let settings = [];
	
	if (!['term','sns','on','off','strnvm'].includes(field)) {
    	value = parseFloat(document.getElementById(`ch${ch}_${field}`).value);
    	console.log(`CH${ch} ${field.toUpperCase()} =`, value);
	}
	else {
		value = 0; //sns, off, strnvm
	}

      if (field === 'on') {
       	 value = 1;
      }
      if (field === 'term') {
       	 value = 1;
      }
      if (field === 'zone') {
   	 	 value = value;
      }

		settings.push({
		      channel: ch,
		      [param]: value
		    });
      websocket.send(JSON.stringify({ message: "Import settings", settings: settings }, null, 2));
}


function toggleOutput(ch) {
  const btn = document.getElementById(`ch${ch}_onoffBtn`);
  const currentState = btn.innerText;
  if (currentState === "ON") {
	  applySetting(ch, 'off', 'OPERATION');
    } else {
      applySetting(ch, 'on','OPERATION');
    }
}


function toggleTerm(ch) {
  const termBtn = document.getElementById(`ch${ch}_termBtn`);
  const currentState = termBtn.innerText;
  if (currentState === "SENSE: EXT") {
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
    // Prevent errors if chart is not yet visible (e.g., tab hidden)
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
  const container = document.getElementById("loggerContainer");

  for (let ch = 1; ch <= 8; ch++) {
	// wrapper for each channel chart
	    const wrapper = document.createElement("div");
	    wrapper.id = `ch${ch}_chartWrapper`;
	    wrapper.style.marginBottom = "20px";
	    wrapper.style.display = "none";
	    wrapper.style.maxWidth = "100%";
	    wrapper.style.width = "100%";

	    // canvas
	    const canvas = document.createElement("canvas");
	    canvas.id = `ch${ch}_chart`;
	    canvas.style.width = "100%";
	    canvas.style.display = "block";
	    const w = window.innerWidth;
	    if (w <= 600) {
	      canvas.style.height = "90px";
	    } else if (w <= 1200) {
	      canvas.style.height = "60px";
	    } else {
	      canvas.style.height = "35px";
	    }
    wrapper.appendChild(canvas);

    // Add to page
    container.appendChild(wrapper);

    // Init buffer
    loggerDataBuffers[ch] = {
      labels: [],
      voltage: [],
      current: [],
      power: [],
      temp: []
    };

    // Init chart
    loggerCharts[ch] = new Chart(canvas, {
  type: 'line',
  data: {
    labels: [],
    datasets: [
      { label: 'Voltage (V)', data: [], borderColor: 'blue', fill: false, yAxisID: 'y' },
	  { label: 'Current (A)', data: [], borderColor: 'green', fill: false, yAxisID: 'y1' },
	  { label: 'Power (W)', data: [], borderColor: 'orange', fill: false, yAxisID: 'y2' },
	  { label: 'Temp (°C)', data: [], borderColor: 'red', fill: false, yAxisID: 'y3' }
    ]
  },
  options: {
    responsive: true,
    animation: false,
    plugins: {
      legend: { display: true, position: 'top' },
      channelInfoText: `Channel ${ch}` // Placeholder
    },
  scales: {
			x: { display: false },
			y: {
			  type: 'linear',
			  display: 'auto',
			  position: 'left',
			  min: 0,
			  max: 70,  
			  title: {
				display: true,
				text: 'Voltage (V)'
			  }
			},
			y1: {
			  type: 'linear',
			  display: 'auto',
			  position: 'left',
			  min: 0,
			  max: 30,
			  grid: {
				drawOnChartArea: false // This ensures the grid lines for this axis don't clutter the chart
			  },
			  title: {
				display: true,
				text: 'Current (A)'
			  }
			},
		  y2: {
			  type: 'linear',
			  display: 'auto',
			  position: 'right',
			  beginAtZero: true,
			  min: 0,
			  max: 500,
			  grid: {
				drawOnChartArea: false // This ensures the grid lines for this axis don't clutter the chart
			  },
			  title: {
				display: true,
				text: 'Power (W)'
			  }
			},
			y3: {
			  type: 'linear',
			  display: 'auto',
			  position: 'right',
			  min: -20,
			  max: 120,
			  grid: {
				drawOnChartArea: false // This ensures the grid lines for this axis don't clutter the chart
			  },
			  title: {
				display: true,
				text: 'Temperature (°C)'
			  }
			}
    }
  },
  plugins: [chartChannelInfo] // 👈 Register plugin here
});
  }
}

window.addEventListener('DOMContentLoaded', () => {
  // Setup logger canvas charts
  setupLoggerCharts();
  
  // Start WebSocket connection
  initWebSocket();
});


async function storeAllNVM() {
	websocket.send(JSON.stringify({ message: "Store NVM" }));
}


function exportSettings() {
  let settings = [],onoff_value,snsterm_value;
  for (let ch = 1; ch <= 8; ch++) {
	  const modelText = document.getElementById(`ch${ch}_model`)?.innerText;
	  if (!modelText || modelText === '—') {
		  continue; // Skips the rest of the current loop iteration
	  } else {

		  if (document.getElementById(`ch${ch}_onoffBtn`).innerText === "ON") {
		   	 onoff_value = 1;
// 		   	 onoff_value = 128;
		  } else {
			  onoff_value = 0;
		  }
		  if (document.getElementById(`ch${ch}_termBtn`).innerText === "SENSE: INT") {
			  snsterm_value = 1;
		  } else {
			  snsterm_value = 0;
		  }
		    settings.push({
		      channel: ch,
		      VOUT_COMMAND: parseFloat(document.getElementById(`ch${ch}_vset`).value) || 0,
		      VOUT_DROOP: parseInt(document.getElementById(`ch${ch}_droop`).value) || 0,
		      VOUT_OV_WARN_LIMIT: parseFloat(document.getElementById(`ch${ch}_ov`).value) || 0,
		      VOUT_UV_WARN_LIMIT: parseFloat(document.getElementById(`ch${ch}_uv`).value) || 0,
		      TON_MAX_FLT: parseFloat(document.getElementById(`ch${ch}_ton`).value) || 0,
		      IOUT_OC_FAULT_LIMIT: parseFloat(document.getElementById(`ch${ch}_ocp`).value) || 0,
		      MFR_SETTINGS: snsterm_value || 0,
		      OPERATION: onoff_value || 0,
		      ZONE_CONFIG: zoneval[ch] || 0,
		      addr: parseInt(document.getElementById(`ch${ch}_addr`).value) || 0
		    });
  	 }
  }
  const blob = new Blob([JSON.stringify({ date: new Date().toISOString(), message: "Import settings", settings: settings }, null, 2)], { type: 'application/json' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = 'NEVO_Settings.json';
  a.click();
  URL.revokeObjectURL(url);
}

function importSettings() {
	waitingForImport = true;
	let onoff_value = [],snsterm_value = [];
  	const input = document.createElement('input');
  	input.type = 'file';
  	input.accept = 'application/json';
  	input.onchange = event => {
    const file = event.target.files[0];
    const reader = new FileReader();
    reader.onload = e => {
    websocket.send(e.target.result);
    };
    reader.readAsText(file);
  };
  input.click();
  waitingForImport = false;
}
//
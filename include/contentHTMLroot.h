#ifndef WIFI_CONTENT_HTML_ROOT_h
#define WIFI_CONTENT_HTML_ROOT_h


const char contentHTMLroot[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="en"><head>
  <meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>
  <style>
    *{margin:0; padding:0; box-sizing:border-box;}
    body{display:flex; justify-content:center; align-items:center; font-family:sans-serif; background: #f4f4f9; padding:20px 15px;  min-height:100vh;}
    .text-center { text-align: center; margin: 5px 0; }
    .container { width:100%; max-width:500px; margin:auto; text-align:center; display:flex; 
             flex-direction:column; align-items:center; padding:20px 15px; background:white; 
             border-radius:10px; box-shadow:0 2px 10px rgba(0,0,0,0.1); }
    
    
    .airport-list { list-style: none; margin-bottom: 20px; min-height: 60px;}
    .airport-item { background:#f8f9fa; border: 2px solid #e0e0e0; border-radius:8px; padding:4px 12px; margin-bottom:4px; 
          display:flex; align-items:center; gap:8px; }
    .airport-item:hover { border-color: #667eea; box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);}
    .airport-input:focus { outline:none; border-color: #5568d3; box-shadow: 0 0 0 3px rgba(102, 126, 234, 0.1);}
    .airport-input{ flex:1; width:80px; padding: 8px 6px; font-size:18px; font-weight:700; letter-spacing:1px;
            background:white; max-width:100%; text-transform:uppercase; border: 2px solid #667eea; border-radius:6px;}
    .airport-code { flex:1; padding:8px 12px; font-size:18px; font-weight:700; letter-spacing:1px; color: #333; }

    .row-number { font-weight:700; font-size:18px; color: #333; min-width:25px; text-align:center;}
    button { padding: 6px 12px; border:none; border-radius: 8px; font-size:16px; font-weight: 600; cursor:pointer; transition: all 0.3s;}
    .order-buttons { display:flex; flex-direction:column; gap:4px;}
    .order-btn { background: #667eea; color:white; border:none; border-radius:4px; width:32px; height:24px;
      cursor: pointer; transition: all 0.2s; font-size:12px; display:flex; align-items:center; justify-content:center;}
    .order-btn:hover:not(:disabled) { background: #5568d3; transform: scale(1.1);}
    .order-btn:disabled {background: #ccc; cursor:not-allowed; opacity: 0.5;}
    .remove-btn { background: #ff4757; color:white; padding: 8px 16px; border:none; border-radius:6px; font-size:14px; font-weight:600; cursor: pointer; transition: all 0.3s;}
    .remove-btn:hover { background: #ee5a6f; transform: translateY(-1px);}
    .add-row-btn { width:20%; background:#667eea; color:white; text-align:center; margin-bottom:20px; }
    .add-row-btn:hover { background: #5568d3; transform: translateY(-2px); box-shadow: 0 4px 12px rgba(102, 126, 234, 0.4);}
    .action-buttons { display:flex; gap:10px; margin-top: 20px; margin-bottom: 4px;}
    .save-btn { width:60%; background: #26de81; color: white;}
    .save-btn:hover { background: #20bf6b; transform: translateY(-2px); box-shadow: 0 4px 12px rgba(38, 222, 129, 0.4);}
    .reset-btn { width:60%; background: #fc5c65; color:white;}
    .reset-btn:hover { background: #eb3b5a; transform: translateY(-2px); box-shadow: 0 4px 12px rgba(252, 92, 101, 0.4);}
    .message { text-align:center; padding:12px; border-radius:8px; margin-bottom:20px; font-weight:500; display:none;}
    .message.success { background: #d4edda; color: #155724; display:block;}
    .message.error { background: #f8d7da; color: #721c24; display:block;}
    .empty-state { text-align: center; padding: 40px; color: #999; font-style: italic;}

    .options-section { width:100%; background:#f8f9fa; border:2px solid #e0e0e0; border-radius:8px; padding:15px; margin-bottom:20px; }
    .options-title { font-weight:700; font-size:16px; color:#333; margin-bottom:12px; text-align:left; }
    .option-row { display:flex; align-items:center; gap:10px; margin-bottom:10px; }
    .option-label { font-size:14px; color:#666; min-width:100px; text-align:left; }
    .option-input { flex:1; padding:8px; border:2px solid #e0e0e0; border-radius:6px; font-size:14px; }
    .option-input:focus { outline:none; border-color:#667eea; box-shadow:0 0 0 2px rgba(102,126,234,0.1); }
    .option-checkbox { width:20px; height:20px; cursor:pointer; accent-color:#667eea; }
    .option-select { flex:1; padding:8px; border:2px solid #e0e0e0; border-radius:6px; font-size:14px; background:white; cursor:pointer; }
    .option-select:focus { outline:none; border-color:#667eea; box-shadow:0 0 0 2px rgba(102,126,234,0.1); }
   

    #clock { font-size: 24px; font-family: monospace; color: #007bff; }

   @media (max-width:400px) { 
      .airport-item { padding:8px; } 
      .remove-btn { padding:6px 12px; font-size:12px; }
      .option-row { flex-direction:column; align-items:flex-start; }
      .option-label { min-width:auto; }
    }
  </style>
  <title>[ESP32] Airport Code Manager</title>
  </head>
  <body>
  <div class='container'>
  <h2><span id="deviceName">[...]</span></h2>
  <p style='color:#666;' class='text-center'>Firmware version: <span id="firmwareVersion">...</span></p>

  <div id="message" class="message"></div>

  <div class="options-section">
      <div class="options-title">Device Options</div>
    
      
      <div class="option-row">
        <label class="option-label" for="isResetDaily">Reset Daily:</label>
        <input type="checkbox" id="isResetDaily" class="option-checkbox" checked />

        <div style="display:flex; align-items:center; gap:2px;">
        <select id="resetHourSelect" class="option-select" style="flex:0.5; margin-right:0px;">
          <option value="0">00</option>
          <option value="1">01</option>
          <option value="2">02</option>
          <option value="3">03</option>
          <option value="4">04</option>
          <option value="5">05</option>
          <option value="6">06</option>
          <option value="7">07</option>
          <option value="8">08</option>
          <option value="9">09</option>
          <option value="10">10</option>
          <option value="11">11</option>
          <option value="12">12</option>
          <option value="13">13</option>
          <option value="14">14</option>
          <option value="15">15</option>
          <option value="16">16</option>
          <option value="17">17</option>
          <option value="18">18</option>
          <option value="19">19</option>
          <option value="20">20</option>
          <option value="21">21</option>
          <option value="22">22</option>
          <option value="23">23</option>
        </select><span style="margin-right:8px;">H</span>
        </div>
        <div style="display:flex; align-items:center; gap:2px;">
        <select id="resetMinuteSelect" class="option-select" style="flex:0.5; margin-right:0px;">
          <option value="0">00</option>
          <option value="5">05</option>
          <option value="10">10</option>
          <option value="15">15</option>
          <option value="20">20</option>
          <option value="25">25</option>
          <option value="30">30</option>
          <option value="35">35</option>
          <option value="40">40</option>
          <option value="45">45</option>
          <option value="50">50</option>
          <option value="55">55</option>
        </select><span style="margin-right:8px;">M</span>
        </div>

      </div>

      <div class="option-row">
        <label class="option-label" for="timezoneSelect">From Timezone:</label>
        <select id="timezoneSelect" class="option-select">
          <option value='0' >Zulu Universal Time (0)</option>
            <option value='1' >Europe/Berlin (+1)</option>
            <option value='2' >Europe/Helsinki (+2)</option>
            <option value='3' >Europe/Moscow (+3)</option>
            <option value='4' >Asia/Dubai (+4)</option>
            <option value='5' >Asia/Karachi (+5)</option>
            <option value='6' >Asia/Dhaka (+6)</option>
            <option value='7' >Asia/Jakarta (+7)</option>
            <option value='8' >Asia/Manila (+8)</option>
            <option value='9' >Asia/Tokyo (+9)</option>
            <option value='10'>Australia/Sydney (+10)</option>
            <option value='11'>Pacific/Noumea (+11)</option>
            <option value='12'>Pacific/Auckland (+12)</option>
            <option value='13'>Atlantic/Azores (-1)</option>
            <option value='14'>America/Noronha (-2)</option>
            <option value='15'>America/Araguaina (-3)</option>
            <option value='16'>America/Blanc-Sablon (-4)</option>
            <option value='17'>America/New_York (-5)</option>
            <option value='18'>America/Chicago (-6)</option>
            <option value='19'>America/Denver (-7)</option>
            <option value='20'>America/Los_Angeles (-8)</option>
            <option value='21'>America/Anchorage (-9)</option>
            <option value='22'>Pacific/Honolulu (-10)</option>
            <option value='23'>Pacific/Niue (-11)</option>
        </select>
      </div>
      <p>Resetting daily will provide better long-term stability.</p>
      
  </div>

  <div class="options-section">
      <div class="options-title">API Parameters</div>
    
      <div class="option-row">
        <label class="option-label" for="apiUrl">API URL:</label>
        <input type="text" id="apiUrl" class="option-input" placeholder="e.g., https://aviationweather.gov/api/data/metar" />
      </div>
      
      <div class="option-row">
        <label class="option-label" for="apiIsComma">Allow one fetch with comma separated:</label>
        <input type="checkbox" id="apiIsComma" class="option-checkbox" checked />
      </div>

      <div class="option-row">
        <label class="option-label" for="apiKeyICAOid">ICAO ID Key:</label>
        <input type="text" id="apiKeyICAOid" class="option-input" placeholder="e.g., icaoId" />
      </div>

      <div class="option-row">
        <label class="option-label" for="apiKeyFlightCategory">Flight Category Key:</label>
        <input type="text" id="apiKeyFlightCategory" class="option-input" placeholder="e.g., fltCat" />
      </div>

      <div class="action-buttons">
      <button class="reset-btn" id="resetBtnOptionsAPI">Reset API Options to Default</button>
      </div>
      <p>API Reference: <a href="https://aviationweather.gov/data/api/#schema" target="_blank">https://aviationweather.gov/data/api/#schema</a></p>
  </div>

    <h2> Airport Code List</h2>
  <ul class="airport-list" id="airportList">
      <li class="empty-state">Click "Add Row" below to start adding airport codes!</li>
  </ul>

  <button class="add-row-btn" id="addRowBtn">Add</button>

  <div class="action-buttons">
      <button class="save-btn" id="saveBtn">Save</button>
      <button class="reset-btn" id="resetBtnList">Reset List</button>
  </div>
  

  <p id='clock' style='color:#666;' class='text-center'> Fetching time...</p>
  
  <script>
    const airportList = document.getElementById('airportList');
    const addRowBtn = document.getElementById('addRowBtn');
    const saveBtn = document.getElementById('saveBtn');
    const resetBtnOptionsAPI = document.getElementById('resetBtnOptionsAPI');
    const resetBtnList = document.getElementById('resetBtnList');
    const messageEl = document.getElementById('message');
    const apiUrlInput = document.getElementById('apiUrl');
    const apiIsCommaCheckbox = document.getElementById('apiIsComma');
    const apiKeyFlightCategoryInput = document.getElementById('apiKeyFlightCategory');
    const apiKeyICAOidInput = document.getElementById('apiKeyICAOid');
    const timezoneSelect = document.getElementById('timezoneSelect');
    const isResetDailyCheckbox = document.getElementById('isResetDaily');
    const resetHourSelect = document.getElementById('resetHourSelect');
    const resetMinuteSelect = document.getElementById('resetMinuteSelect');

    let airports = [];
    let clockTimer;
    let options = {
      timezone: 0,
      isResetDaily: true,
      resetHour: 0,
      resetMinute: 0
    };
    let optionsAPI = {
      apiUrl: '',
      apiIsComma: true,
      apiKeyICAOid: '',
      apiKeyFlightCategory: ''
    };

  function fetchTime() {
    fetch('/time')
      .then(response => {
        console.log('fetchTime status:', response.status);
        return response.text();
      })
      .then(data => {
        if (data !== "") {
          console.log('Time fetched:', data);
          document.getElementById("clock").innerText = data;
        }
      })
      .catch(err => console.warn('Clock Fetch error:', err));
  }

  function showMessage(text, type) {
      messageEl.textContent = text;
      messageEl.className = `message ${type}`;
      setTimeout(() => {
          messageEl.className = 'message';
      }, 5000);
  }

  function updateField(id, value) {
    const el = document.getElementById(id);
    if (el && value !== undefined) el.textContent = value;
  }

  async function loadAirports() {
    try {
      const response = await fetch('/fetchAirports');
      if (response.ok) {
        const data = await response.json();
        
        // Update device info
        document.title = data.deviceName + " Airport List";
        updateField('deviceName', data.deviceName);
        updateField('firmwareVersion', data.fwVersion);
        
        // Load airport codes from server
        airports = (data.codes || []).map(code => ({ code, editing: false }));

        apiUrlInput.value = data.api_url || '';
        apiKeyICAOidInput.value = data.api_keyICAOid || '';
        apiKeyFlightCategoryInput.value = data.api_keyFlightCategory || '';
        apiIsCommaCheckbox.checked = data.isApiUseComma ?? false;
        timezoneSelect.value = data.timezone ?? 0;
        isResetDailyCheckbox.checked = data.isResetDaily ?? true;
        resetHourSelect.value = data.resetHour ?? 0;
        resetMinuteSelect.value = data.resetMinute ?? 0;

      } else {
        throw new Error('Server response not OK');
      }
    } catch (error) {
      console.warn('Failed to fetch from server, loading from localStorage:', error);
      
      // Fallback to localStorage
      const saved = localStorage.getItem('airports');
      const savedOptions = localStorage.getItem('airportOptions');
      const savedOptionsAPI = localStorage.getItem('airportOptionsAPI');
      if (saved) {
        try {
          airports = JSON.parse(saved);
        } catch (e) {
          console.warn('Error parsing saved airports:', e);
          airports = [];
        }
      }
    if (savedOptions) {
        try {
            options = JSON.parse(savedOptions);
            timezoneSelect.value = options.timezone ?? 0;
            isResetDailyCheckbox.checked = options.isResetDaily ?? true;
            resetHourSelect.value = options.resetHour ?? 0;
            resetMinuteSelect.value = options.resetMinute ?? 0;
        } catch (e) {
            console.warn('Error parsing saved options:', e);
        }
    }
    if (savedOptionsAPI) {
        try {
            optionsAPI = JSON.parse(savedOptionsAPI);
            apiUrlInput.value = optionsAPI.apiUrl || '';
            apiIsCommaCheckbox.checked = optionsAPI.apiIsComma !== undefined ? optionsAPI.apiIsComma : false;
            apiKeyFlightCategoryInput.value = optionsAPI.apiKeyFlightCategory || '';
            apiKeyICAOidInput.value = optionsAPI.apiKeyICAOid || '';
        } catch (e) {
            console.warn('Error parsing saved optionsAPI:', e);
        }
    }
    } finally {
      renderAirports();
    }
  }

  function renderAirports() {
      if (airports.length === 0) {
          airportList.innerHTML = '<li class="empty-state">Click "Add" to add an airport code.</li>';
          return;
      }

      airportList.innerHTML = airports.map((airport, index) => {
          const isFirst = index === 0;
          const isLast = index === airports.length - 1;

          if (airport.editing) {
              return `
                  <li class="airport-item" data-index="${index}">
                      <span class="row-number">${index + 1}.</span>
                      <input 
                          type="text" 
                          class="airport-input" 
                          value="${airport.code || ''}" 
                          maxlength="4"
                          placeholder="XXXX"
                          data-index="${index}"
                          autocomplete="off"
                      >
                      <div class="order-buttons">
                          <button class="order-btn" onclick="moveUp(${index})" ${isFirst ? 'disabled' : ''}>▲</button>
                          <button class="order-btn" onclick="moveDown(${index})" ${isLast ? 'disabled' : ''}>▼</button>
                      </div>
                      <button class="remove-btn" onclick="removeAirport(${index})">Remove</button>
                  </li>
              `;
          } else {
              return `
                  <li class="airport-item" data-index="${index}">
                      <span class="row-number">${index + 1}.</span>
                      <span class="airport-code" ondblclick="editAirport(${index})">${airport.code}</span>
                      <div class="order-buttons">
                          <button class="order-btn" onclick="moveUp(${index})" ${isFirst ? 'disabled' : ''}>▲</button>
                          <button class="order-btn" onclick="moveDown(${index})" ${isLast ? 'disabled' : ''}>▼</button>
                      </div>
                      <button class="remove-btn" onclick="removeAirport(${index})">Remove</button>
                  </li>
              `;
          }
      }).join('');

      attachInputListeners();
  }
  
  function moveUp(index) {
      if (index === 0) return;
      [airports[index], airports[index - 1]] = [airports[index - 1], airports[index]];
      renderAirports();
  }

  function moveDown(index) {
      if (index === airports.length - 1) return;
      [airports[index], airports[index + 1]] = [airports[index + 1], airports[index]];
      renderAirports();
  }

  function addRow() {
      airports.push({ code: '', editing: true });
      renderAirports();
      // Focus on the new input
      setTimeout(() => {
          const inputs = document.querySelectorAll('.airport-input');
          if (inputs.length > 0) { inputs[inputs.length - 1].focus();}
      }, 0);
  }

  // Edit existing airport (double-click)
  function editAirport(index) {
      airports[index].editing = true;
      renderAirports();
      setTimeout(() => {
          const input = document.querySelector(`input[data-index="${index}"]`);
          if (input) { input.focus(); input.select(); }
      }, 0);
  }

  function removeAirport(index) {
      const removed = airports[index].code || 'Empty row';
      airports.splice(index, 1);
      renderAirports();
      showMessage(`${removed} removed`, 'success');
  }

  async function saveAirports() {
      const validAirports = airports.filter(a => !a.editing && a.code && a.code.length >= 2 && a.code.length <= 4);
      if (validAirports.length === 0) {
          showMessage('No valid airports to save', 'error');
          return;
      }

      optionsAPI.apiUrl = apiUrlInput.value.trim();
      optionsAPI.apiIsComma = apiIsCommaCheckbox.checked;
      optionsAPI.apiKeyFlightCategory = apiKeyFlightCategoryInput.value.trim();
      optionsAPI.apiKeyICAOid = apiKeyICAOidInput.value.trim();
      options.timezone = parseInt(timezoneSelect.value);
      options.isResetDaily = isResetDailyCheckbox.checked;
      options.resetHour = parseInt(resetHourSelect.value);
      options.resetMinute = parseInt(resetMinuteSelect.value);

      try {
        const response = await fetch('/saveAirports', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ 
                codes: validAirports.map(a => a.code),
                apiUrl: optionsAPI.apiUrl,
                apiIsComma: optionsAPI.apiIsComma,
                apiKeyFlightCategory: optionsAPI.apiKeyFlightCategory,
                apiKeyICAOid: optionsAPI.apiKeyICAOid,
                timezone: options.timezone,
                isResetDaily: options.isResetDaily,
                resetHour: options.resetHour,
                resetMinute: options.resetMinute
          })
        });
        if (response.ok) {
          localStorage.setItem('airports', JSON.stringify(validAirports));
          localStorage.setItem('airportOptions', JSON.stringify(options));
          localStorage.setItem('airportOptionsAPI', JSON.stringify(optionsAPI));
          airports = validAirports.map(a => ({ code: a.code, editing: false }));
          renderAirports();
          showMessage('List saved successfully!', 'success');
        } else {
          showMessage('Server rejected the data', 'error');
        }
      } catch (error) {
        console.warn('Save error:', error);
        showMessage('Connection lost to ESP32', 'error');
      }
      
  }

  function resetList() {
      if (airports.length === 0) {
          return;
      }

      if (confirm('Are you sure you want to clear the Airport List?')) {
          airports = [];
          localStorage.removeItem('airports');
          renderAirports();
          showMessage('Airport List cleared successfully', 'success');
      }
  }

  function resetOptionsAPI() {

      if (confirm('Are you sure you want to reset the API Options to Default?')) {
          optionsAPI = {
              apiUrl: 'https://aviationweather.gov/api/data/metar?format=json&taf=false&ids=',
              apiIsComma: true,
              apiKeyICAOid: 'icaoId',
              apiKeyFlightCategory: 'fltCat'
          };

          apiUrlInput.value = optionsAPI.apiUrl || '';
          apiKeyFlightCategoryInput.value = optionsAPI.apiKeyFlightCategory || '';
          apiKeyICAOidInput.value = optionsAPI.apiKeyICAOid || '';
          apiIsCommaCheckbox.checked = optionsAPI.apiIsComma ?? false;

          localStorage.setItem('airportOptionsAPI', JSON.stringify(optionsAPI));
          showMessage('API Options reset to default successfully', 'success');
      }
  }

  

  function attachInputListeners() {
    const inputs = document.querySelectorAll('.airport-input');
    inputs.forEach(input => {
        input.addEventListener('input', (e) => {
            const index = parseInt(e.target.getAttribute('data-index'));
            airports[index].code = e.target.value.toUpperCase();
            e.target.value = e.target.value.toUpperCase();
        });

        input.addEventListener('blur', (e) => {
            const index = parseInt(e.target.getAttribute('data-index'));
            const code = airports[index].code.trim();
            if (code === '') {
                // Remove empty rows on blur
                airports.splice(index, 1);
                renderAirports();
                return;
            }

            if (code.length > 4 || code.length < 2) {
                showMessage('Airport code must be at least 2 and up to 4 characters', 'error');
                return;
            }

            if (!/^[A-Z0-9]{2,4}$/.test(code)) {
                showMessage('Airport code must contain only letters and numbers', 'error');
                return;
            }

            // Check for duplicates
            const duplicates = airports.filter((a, i) => i !== index && a.code === code);
            if (duplicates.length > 0) {
                showMessage('Airport code already exists', 'error');
                airports.splice(index, 1);
                renderAirports();
                return;
            }

            airports[index].editing = false;
            renderAirports();
        });

        input.addEventListener('keypress', (e) => {
            if (e.key === 'Enter') {
                e.target.blur();
            }
        });
    });
  }

  function toggleResetDropdowns() {
    const isDisabled = !isResetDailyCheckbox.checked;
    resetHourSelect.disabled = isDisabled;
    resetMinuteSelect.disabled = isDisabled;
    timezoneSelect.disabled = isDisabled;
  }

  // Event listeners
  addRowBtn.addEventListener('click', addRow);
  saveBtn.addEventListener('click', saveAirports);
  resetBtnList.addEventListener('click', resetList);
  resetBtnOptionsAPI.addEventListener('click', resetOptionsAPI);
  isResetDailyCheckbox.addEventListener('change', toggleResetDropdowns);

  // Load airports on page load
  loadAirports();
  toggleResetDropdowns();
  clockTimer = setInterval(fetchTime, 1000);

  </script>
  </body></html>
  )rawliteral";


#endif
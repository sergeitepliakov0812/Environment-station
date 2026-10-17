// Global WebSocket variable for ESP32 communication
let ws = null;
// Maximum number of data points to display on the chart
const MAX_POINTS = 128; //128 points every 2 seconds = 4 minutes of data

// ---- Settings for every sensor that can be shown on the chart ----
const SENSORS = {
  temperature: { label: "Temperature", unit: "°C",  decimals: 1 },
  humidity:    { label: "Humidity",    unit: "%",   decimals: 1 },
  pressure:    { label: "Pressure",    unit: "hPa", decimals: 1 },
  light:       { label: "Light",       unit: "lux", decimals: 0 },
  noise:       { label: "Noise",       unit: "dB",  decimals: 1 },
  airQuality:  { label: "Air quality", unit: "raw", decimals: 0 }
};
// Air quality value above which the sidebar block turns red
// (keep the same as MQ135_ALERT_THRESHOLD in main.cpp)
const AIR_ALERT = 1500;
// One history array per sensor, e.g. histories.humidity = [60.1, 60.3, ...]
const histories = {};
Object.keys(SENSORS).forEach((key) => { histories[key] = []; });
// Which sensor the chart is showing right now
let selected = "temperature";

// DOM element references for UI interactions
const statusDot = document.getElementById("status");
const espIpInput = document.getElementById("espip");
const connectBtn = document.getElementById("connectBtn");

// Attach click event listener to connect button
connectBtn.addEventListener("click", connectToESP);

/**
 * Establishes WebSocket connection to ESP32 device
 * Validates IP input, closes existing connection, and sets up event handlers
 */
function connectToESP() {
  // Get and trim the IP address from input field
  const espIp = espIpInput.value.trim();
  // Validate that IP address was provided
  if (!espIp) {
    alert("Please enter the ESP IP address.");
    return;
  }

  // Close existing connection if one is already open
  if (ws) {
    ws.close();
  }

  // Create new WebSocket connection to ESP32
  ws = new WebSocket(`ws://${espIp}/ws`);

  // Handle successful connection
  ws.onopen = () => {
    statusDot.classList.remove("offline");
    statusDot.classList.add("online");
    // Request current sensor states from ESP32
    ws.send('getStates');
  };

  // Handle connection closed
  ws.onclose = () => {
    statusDot.classList.remove("online");
    statusDot.classList.add("offline");
  };

  // Handle connection errors
  ws.onerror = () => {
    statusDot.classList.remove("online");
    statusDot.classList.add("offline");
    alert("Failed to connect to the ESP. Please check the IP address and try again.");
  };

  // Handle incoming messages from ESP32
  ws.onmessage = (event) => {
    try {
        // Parse JSON data from WebSocket message
        const data = JSON.parse(event.data);
        // Update dashboard with new sensor values
        updateDashboard(data);
    } catch (error) {
        // Log parsing errors to console for debugging
        console.error("Error parsing message:", error);
    }
  };
}

/**
 * Updates dashboard display with incoming sensor data
 * Processes each sensor value and updates corresponding HTML elements
 * @param {Object} data - Object containing array of sensor values
 */
function updateDashboard(data) {
    // Exit early if no sensor values are present
    if (!data.values) return;

    data.values.forEach((item) => {
        // Look up this sensor's label, unit and decimals in the SENSORS list
        const sensor = SENSORS[item.name];
        if (!sensor) return;   // ignore anything we don't know

        // The air quality sensor sends ready = false while it warms up
        const ready = item.ready !== false;

        // Show the live value inside the sidebar block
        document.getElementById("val-" + item.name).textContent =
            ready ? item.value.toFixed(sensor.decimals) + " " + sensor.unit : "warming up";

        // Save the reading so the chart can draw it
        if (ready) pushHistory(item.name, item.value);

        // Turn the air quality block red when above the alert level
        if (item.name === "airQuality") {
            document.getElementById("block-airQuality")
                .classList.toggle("alert", ready && item.value > AIR_ALERT);
        }

        // Update the big number in the main panel too (if that element exists)
        const big = document.getElementById(item.name);
        if (big) big.textContent = item.value.toFixed(sensor.decimals);
    });

    // Redraw the chart for whichever sensor is selected
    drawChart();
}

/**
 * Adds a temperature value to the history array
 * Maintains maximum of MAX_POINTS by removing oldest value if exceeded
 * @param {number} value - Temperature value to add to history
 */
function pushHistory(name, value) {
    // Pick this sensor's own history list
    const list = histories[name];
    list.push(value);
    // Remove the oldest value if there are too many
    if (list.length > MAX_POINTS) {
        list.shift();
    }
}

/**
 * Draws a line chart on canvas showing temperature history
 * Includes axes, data line, data points, and x-axis labels
 */
function drawChart() {
    const history = histories[selected]; // Get the history array for the currently selected sensor
    const unit = SENSORS[selected].unit; // Get the unit for the currently selected sensor
    const decimals = SENSORS[selected].decimals; // Get the number of decimal places for the currently selected sensor
    // Get canvas element and 2D drawing context
    const canvas = document.getElementById("chart");
    const ctx = canvas.getContext("2d");
    const width = canvas.width;
    const height = canvas.height;
    // Padding around the chart edges
    const padding = 40;


    // Clear entire canvas for redraw
    ctx.clearRect(0, 0, width, height);

    // Draw chart axes (Y-axis and X-axis)
    ctx.strokeStyle = '#1a5799';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(padding, padding);
    ctx.lineTo(padding, height - padding);
    ctx.lineTo(width - padding, height - padding);
    ctx.stroke();

    // If there are no readings yet, show placeholders and exit
    if (history.length === 0) {
    // Nothing to show yet for this sensor
    ['chartMin', 'chartAvg', 'chartMax'].forEach((id) => {
        document.getElementById(id).textContent = '--';
    });
    return;
}
    // Min / average / max of the readings currently on the chart
    const sum = history.reduce((a, b) => a + b, 0);
    document.getElementById('chartMin').textContent = Math.min(...history).toFixed(decimals) + ' ' + unit;
    document.getElementById('chartAvg').textContent = (sum / history.length).toFixed(decimals) + ' ' + unit;
    document.getElementById('chartMax').textContent = Math.max(...history).toFixed(decimals) + ' ' + unit;

    // Calculate min and max values with 2-unit buffer for better visualization
    const minValue = Math.min(...history) - 2;
    const maxValue = Math.max(...history) + 2;
    // Calculate range of values (minimum 1 to avoid division by zero)
    const range = maxValue - minValue || 1;
    // Y-axis labels: top of the scale, bottom of the scale, and the unit
    ctx.fillStyle = '#1a5799';
    ctx.font = '12px Arial';
    ctx.textAlign = 'right';                                  // right-align so numbers end at the axis
    ctx.fillText(maxValue.toFixed(1), padding - 6, padding + 4);
    ctx.fillText(minValue.toFixed(1), padding - 6, height - padding);
    ctx.textAlign = 'left';                                   // reset, or the x-axis labels will shift
    ctx.fillText(unit, 5, 20);  

    // Calculate horizontal spacing between data points
    const stepX = (width - 2 * padding) / (MAX_POINTS - 1);

    // Draw line connecting all data points
    ctx.strokeStyle = '#1a5799';
    ctx.lineWidth = 3;
    ctx.beginPath();
    history.forEach((value, index) => {
        // Calculate x position based on index
        const x = padding + index * stepX;
        // Calculate y position by normalizing value to canvas height
        const y = height - padding - ((value - minValue) / range) * (height - 2 * padding);
        // Move to first point, then draw lines to subsequent points
        if (index === 0) {
            ctx.moveTo(x, y);
        } else {
            ctx.lineTo(x, y);
        }
    });
    ctx.stroke();

    // Draw circles at each data point
    ctx.fillStyle = '#f81414';
    history.forEach((value, index) => {
        // Calculate point position
        const x = padding + index * stepX;
        const y = height - padding - ((value - minValue) / range) * (height - 2 * padding);
        // Draw circle with radius of 4 pixels
        ctx.beginPath();
        ctx.arc(x, y, 1.5, 0, Math.PI * 2);
        ctx.fill();
    });

    // Draw x-axis labels (point numbers 1, 2, 3, etc.)
    ctx.fillStyle = '#ec1a1a';
    ctx.font = '14px Arial';
    history.forEach((value, index) => {
         if ((index + 1) % 16 !== 0) return;
        const x = padding + index * stepX;
        // Display point number below x-axis
        ctx.fillText(index + 1, x - 10, height - padding + 20);
    });
}

// Allow pressing Enter key in IP input field to trigger connection to ESP32
espIpInput.addEventListener("keydown", (event) => {
  if (event.key === "Enter") {
    connectToESP();
  }
});
// ---- Sidebar: click a sensor block to show it on the chart ----
const blocks = document.querySelectorAll(".sensor-block");

function selectSensor(block) {
    // Remember which sensor the chart should show
    selected = block.dataset.sensor;

    // Move the highlight to the clicked block
    blocks.forEach((b) => b.classList.remove("active"));
    block.classList.add("active");

    // Update the chart title, e.g. "Humidity history (%)"
    const s = SENSORS[selected];
    document.getElementById("chartTitle").textContent = s.label + " history (" + s.unit + ")";

    // Redraw the chart with that sensor's history
    drawChart();
}

blocks.forEach((block) => {
    // Mouse click
    block.addEventListener("click", () => selectSensor(block));
    // Keyboard: Enter or Space while the block is focused
    block.addEventListener("keydown", (event) => {
        if (event.key === "Enter" || event.key === " ") {
            event.preventDefault();
            selectSensor(block);
        }
    });
});
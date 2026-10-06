// Global WebSocket variable for ESP32 communication
let ws = null;
// Maximum number of data points to display on the chart
const MAX_POINTS = 6;
// Array to store historical temperature values for charting
let history = [];

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

    // Iterate through each sensor reading
    data.values.forEach((item) => {
        switch (item.name) {
            case "temperature":
                // Update temperature display with one decimal place
                document.getElementById("temperature").textContent = item.value.toFixed(1); 
                // Add temperature to history for charting
                pushHistory(item.value);
                break;
            case "humidity":
                // Update humidity display with one decimal place
                document.getElementById("humidity").textContent = item.value.toFixed(1); 
                break;
            case "pressure":
                // Update pressure display with one decimal place
                document.getElementById("pressure").textContent = item.value.toFixed(1); 
                break;
            case 'light':
                // Update light intensity with no decimal places
                document.getElementById('light').textContent = item.value.toFixed(0);
                // Update maximum light reading display
                document.getElementById('maxLight').textContent = Math.round(item.value);
                break;
            case 'airQuality':
                // Display air quality value if ready, otherwise show "..."
                document.getElementById('airRaw').textContent = item.ready ? item.value : '...';
                break;
            case 'noise':
                // Update noise level display with one decimal place
                document.getElementById('noise').textContent = item.value.toFixed(1);
                break;
        }
    });

    // Redraw the temperature chart with updated data
    drawChart();
}

/**
 * Adds a temperature value to the history array
 * Maintains maximum of MAX_POINTS by removing oldest value if exceeded
 * @param {number} value - Temperature value to add to history
 */
function pushHistory(value) {
    // Add new value to end of history array
    history.push(value);
    // Remove oldest value if history exceeds maximum points
    if (history.length > MAX_POINTS) {
        history.shift();
    }
}

/**
 * Draws a line chart on canvas showing temperature history
 * Includes axes, data line, data points, and x-axis labels
 */
function drawChart() {
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
    ctx.strokeStyle = '#45A9A9';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(padding, padding);
    ctx.lineTo(padding, height - padding);
    ctx.lineTo(width - padding, height - padding);
    ctx.stroke();

    // Exit if no data points to plot
    if (history.length === 0) return;

    // Calculate min and max values with 2-unit buffer for better visualization
    const minValue = Math.min(...history) - 2;
    const maxValue = Math.max(...history) + 2;
    // Calculate range of values (minimum 1 to avoid division by zero)
    const range = maxValue - minValue || 1;

    // Calculate horizontal spacing between data points
    const stepX = (width - 2 * padding) / (MAX_POINTS - 1);

    // Draw line connecting all data points
    ctx.strokeStyle = '#45A9A9';
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
    ctx.fillStyle = '#45A9A9';
    history.forEach((value, index) => {
        // Calculate point position
        const x = padding + index * stepX;
        const y = height - padding - ((value - minValue) / range) * (height - 2 * padding);
        // Draw circle with radius of 4 pixels
        ctx.beginPath();
        ctx.arc(x, y, 4, 0, Math.PI * 2);
        ctx.fill();
    });

    // Draw x-axis labels (point numbers 1, 2, 3, etc.)
    ctx.fillStyle = '#45A9A9';
    ctx.font = '14px Arial';
    history.forEach((value, index) => {
        const x = padding + index * stepX;
        // Display point number below x-axis
        ctx.fillText(index + 1, x - 3, height - padding + 20);
    });
}

// Allow pressing Enter key in IP input field to trigger connection to ESP32
espIpInput.addEventListener("keydown", (event) => {
  if (event.key === "Enter") {
    connectToESP();
  }
});
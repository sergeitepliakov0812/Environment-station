let ws = null;
const MAX_POINTS = 6;
let history = [];

const statusDot = document.getElementById("status");
const espIpInput = document.getElementById("espip");
const connectBtn = document.getElementById("connectBtn");

connectBtn.addEventListener("click", connectToESP);

function connectToESP() {
  const espIp = espIpInput.value.trim();
  if (!espIp) {
    alert("Please enter the ESP IP address.");
    return;
  }

  if (ws) {
    ws.close();
  }

  ws = new WebSocket(`ws://${espIp}/ws`);

  ws.onopen = () => {
    statusDot.classList.remove("offline");
    statusDot.classList.add("online");
    ws.send('getStates');
  };

  ws.onclose = () => {
    statusDot.classList.remove("online");
    statusDot.classList.add("offline");
  };

  ws.onerror = () => {
    statusDot.classList.remove("online");
    statusDot.classList.add("offline");
    alert("Failed to connect to the ESP. Please check the IP address and try again.");
  };

  ws.onmessage = (event) => {
    try {
        const data = JSON.parse(event.data);
        updateDashboard(data);
    } catch (error) {
        console.error("Error parsing message:", error);
    }
    };
}

function updateDashboard(data) {
    if (!data.values) return;

    data.values.forEach((item) => {
        switch (item.name) {
            case "temperature":
                document.getElementById("temperature").textContent = item.value.toFixed(1); 
                pushHistory(item.value);
                break;
            case "humidity":
                document.getElementById("humidity").textContent = item.value.toFixed(1); 
                break;
            case "pressure":
                document.getElementById("pressure").textContent = item.value.toFixed(1); 
                break;
                case 'light':
                document.getElementById('light').textContent = item.value.toFixed(0);
                document.getElementById('maxLight').textContent = Math.round(item.value);
                break;
            case 'airQuality':
                document.getElementById('airRaw'). textContent = item.ready ? item.value : '...';
                break;
            case 'noise':
                document.getElementById('noise').textContent = item.value.toFixed(1);
                break;
        }
    });

    drawChart();
}

function pushHistory(value) {
    history.push(value);
    if (history.length > MAX_POINTS) {
        history.shift();
    }
}

function drawChart() {
    const canvas = document.getElementById("chart");
    const ctx = canvas.getContext("2d");
    const width = canvas.width;
    const height = canvas.height;
    const padding = 40;

    ctx.clearRect(0, 0, width, height);

    ctx.strokeStyle = '#e2447a';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(padding, padding);
    ctx.lineTo(padding, height - padding);
    ctx.lineTo(width - padding, height - padding);
    ctx.stroke();

    if (history.length === 0) return;

    const minValue = Math.min(...history) - 2;
    const maxValue = Math.max(...history) + 2;
    const range = maxValue - minValue || 1;

    const stepX = (width - 2 * padding) / (MAX_POINTS - 1);

    ctx.strokeStyle = '#e2447a';
    ctx.lineWidth = 3;
    ctx.beginPath();
    history.forEach((value, index) => {
        const x = padding + index * stepX;
        const y = height - padding - ((value - minValue) / range) * (height - 2 * padding);
        if (index === 0) {
            ctx.moveTo(x, y);
        } else {
            ctx.lineTo(x, y);
        }
    });
    ctx.stroke();

    ctx.fillStyle = '#e2447a';
    history.forEach((value, index) => {
        const x = padding + index * stepX;
        const y = height - padding - ((value - minValue) / range) * (height - 2 * padding);
        ctx.beginPath();
        ctx.arc(x, y, 4, 0, Math.PI * 2);
        ctx.fill();
    });

    ctx.fillStyle = '#e2447a';
    ctx.font = '14px Arial';
    history.forEach((value, index) => {
        const x = padding + index * stepX;
        ctx.fillText(index + 1, x - 3, height - padding + 20);
    });
}

espIpInput.addEventListener("keydown", (event) => {
  if (event.key === "Enter") {
    connectToESP();
  }
});
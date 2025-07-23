
// Local runtime-only state for applying dead zones
const localState = {
    left: { x: 0.0, y: 0.0 },
    right: { x: 0.0, y: 0.0 }
};

// Intended-to-send state
const joystickState = {
    use_web_inputs: true,
    left: { x: 0.0, y: 0.0 },
    right: { x: 0.0, y: 0.0 },
    dead_zone_left: { x: 0.1, y: 0.1 },
    dead_zone_right: { x: 0.1, y: 0.1 },
    scale_left: { x: 1.0, y: 1.0 },
    scale_right: { x: 1.0, y: 1.0 }
};

const lastSentState = JSON.parse(JSON.stringify(joystickState));
let dirtyKeys = new Set();
let ticking = false;

function applyDeadZone(value, threshold) {
    if (Math.abs(value) < threshold) return 0.0;
    const sign = value > 0 ? 1 : -1;
    return ((Math.abs(value) - threshold) / (1.0 - threshold)) * sign;
}

function sendJoystickInput(topic, normX, normY) {
    const mapping = {
        "left_stick": "left",
        "right_stick": "right"
    };

    const key = mapping[topic];
    if (!key) return;

    const dz = joystickState[key === 'left' ? 'dead_zone_left' : 'dead_zone_right'];
    const filteredX = applyDeadZone(normX, dz.x);
    const filteredY = applyDeadZone(normY, dz.y);

    localState[key].x = filteredX;
    localState[key].y = filteredY;

    joystickState[key].x = filteredX;
    joystickState[key].y = filteredY;
    dirtyKeys.add(key);

    if (!ticking) startTickLoop();
}

function startTickLoop() {
    ticking = true;
    const tickIntervalMs = 33; // 30Hz

    const tick = () => {
        if (dirtyKeys.size === 0) {
            ticking = false;
            return;
        }

        const payload = { use_web_inputs: joystickState.use_web_inputs };

        for (const key of dirtyKeys) {
            const current = joystickState[key];
            const last = lastSentState[key];
            if (
                Math.abs(current.x - last.x) > 0.001 ||
                Math.abs(current.y - last.y) > 0.001
            ) {
                payload[key] = { x: current.x, y: current.y };
                last.x = current.x;
                last.y = current.y;
            }
        }

        dirtyKeys.clear();

        fetch('/api/joystick_input', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        }).catch(err => console.error('POST error:', err));

        setTimeout(tick, tickIntervalMs);
    };

    tick();
}

function createStick(areaId, knobId, topic, autoCenterX, autoCenterY) {
    const area = document.getElementById(areaId);
    const knob = document.getElementById(knobId);
    const areaRect = area.getBoundingClientRect();
    const originX = areaRect.width / 2;
    const originY = areaRect.height / 2;

    function setKnob(x, y) {
        knob.style.left = `${x}px`;
        knob.style.top = `${y}px`;
    }

    function movePointer(globalX, globalY) {
        const rect = area.getBoundingClientRect();
        const originX = rect.width / 2;
        const originY = rect.height / 2;
        let dx = globalX - rect.left - originX;
        let dy = globalY - rect.top - originY;

        const maxRangeX = rect.width / 2 - knob.offsetWidth / 2;
        const maxRangeY = rect.height / 2 - knob.offsetHeight / 2;
        dx = Math.max(-maxRangeX, Math.min(maxRangeX, dx));
        dy = Math.max(-maxRangeY, Math.min(maxRangeY, dy));

        setKnob(originX + dx, originY + dy);

        const normX = dx / maxRangeX;
        const normY = -dy / maxRangeY; // invert Y so up is positive (more intuitive)
        sendJoystickInput(topic, normX, normY);
    }

    function resetKnob() {
        const x = autoCenterX ? originX : knob.offsetLeft;
        const y = autoCenterY ? originY : knob.offsetTop;
        setKnob(x, y);

        const normX = autoCenterX ? 0 : (x - originX) / ((areaRect.width / 2) - (knob.offsetWidth / 2));
        const normY = autoCenterY ? 0 : -(y - originY) / ((areaRect.height / 2) - (knob.offsetHeight / 2));
        sendJoystickInput(topic, normX, normY);
    }

    return { movePointer, resetKnob, area };
}

const leftStick = createStick('left-area', 'left-knob', 'left_stick', true, true);
const rightStick = createStick('right-area', 'right-knob', 'right_stick', true, false);

const activeTouches = {};

function touchStartedInArea(touch, area) {
    const rect = area.getBoundingClientRect();
    return (
        touch.clientX >= rect.left &&
        touch.clientX <= rect.right &&
        touch.clientY >= rect.top &&
        touch.clientY <= rect.bottom
    );
}

function handleTouchStart(e) {
    for (const touch of e.changedTouches) {
        if (touchStartedInArea(touch, leftStick.area)) {
            activeTouches[touch.identifier] = { side: 'left', startX: touch.clientX, startY: touch.clientY };
        } else if (touchStartedInArea(touch, rightStick.area)) {
            activeTouches[touch.identifier] = { side: 'right', startX: touch.clientX, startY: touch.clientY };
        }
    }
}

function handleTouchMove(e) {
    for (const touch of e.changedTouches) {
        const touchData = activeTouches[touch.identifier];
        if (touchData) {
            const dx = touch.clientX - touchData.startX;
            const dy = touch.clientY - touchData.startY;

            const area = touchData.side === 'left' ? leftStick.area : rightStick.area;
            const stick = touchData.side === 'left' ? leftStick : rightStick;
            const rect = area.getBoundingClientRect();
            const centerX = rect.left + area.clientWidth / 2;
            const centerY = rect.top + area.clientHeight / 2;

            stick.movePointer(centerX + dx, centerY + dy);
        }
    }
}

function handleTouchEnd(e) {
    for (const touch of e.changedTouches) {
        const touchData = activeTouches[touch.identifier];
        if (touchData) {
            (touchData.side === 'left' ? leftStick : rightStick).resetKnob();
            delete activeTouches[touch.identifier];
        }
    }
}

function setupVideoFeed() {
    const cameraImg = document.getElementById("camera-stream");
    let lastFrameBlobUrl = null;

    function refreshCameraFrame() {
        const loaderImg = new Image();
        loaderImg.onload = () => {
            // Swap only after successful load
            cameraImg.src = loaderImg.src;

            // Revoke the previous blob URL to avoid leaks
            if (lastFrameBlobUrl) URL.revokeObjectURL(lastFrameBlobUrl);
            lastFrameBlobUrl = loaderImg.src;
        };

        loaderImg.onerror = () => {
            // Ignore errors, keep old image
            console.warn("Camera frame failed to load");
        };

        // Fetch image as a blob
        fetch(`/api/jpeg_data?t=${Date.now()}`)
            .then(response => {
                if (!response.ok) throw new Error("HTTP error");
                return response.blob();
            })
            .then(blob => {
                const blobUrl = URL.createObjectURL(blob);
                loaderImg.src = blobUrl;
            })
            .catch(err => {
                console.warn("Camera fetch failed:", err);
            });
    }

    const videoRefreshRateHz = 15;
    const videoRefreshInternalMs = 1000 / videoRefreshRateHz; 
    setInterval(refreshCameraFrame, videoRefreshInternalMs); // at ~15 FPS
}

document.addEventListener('touchstart', handleTouchStart);
document.addEventListener('touchmove', handleTouchMove);
document.addEventListener('touchend', handleTouchEnd);
document.addEventListener('touchcancel', handleTouchEnd);

let mouseActive = null;
let mouseStartX = 0;
let mouseStartY = 0;

document.addEventListener('mousedown', e => {
    if (leftStick.area.contains(e.target)) {
        mouseActive = 'left';
        mouseStartX = e.clientX;
        mouseStartY = e.clientY;
        leftStick.movePointer(leftStick.area.getBoundingClientRect().left + leftStick.area.clientWidth / 2,
                              leftStick.area.getBoundingClientRect().top + leftStick.area.clientHeight / 2);
    } else if (rightStick.area.contains(e.target)) {
        mouseActive = 'right';
        mouseStartX = e.clientX;
        mouseStartY = e.clientY;
        rightStick.movePointer(rightStick.area.getBoundingClientRect().left + rightStick.area.clientWidth / 2,
                               rightStick.area.getBoundingClientRect().top + rightStick.area.clientHeight / 2);
    }
});

document.addEventListener('mousemove', e => {
    if (mouseActive) {
        const dx = e.clientX - mouseStartX;
        const dy = e.clientY - mouseStartY;

        const area = mouseActive === 'left' ? leftStick.area : rightStick.area;
        const stick = mouseActive === 'left' ? leftStick : rightStick;
        const rect = area.getBoundingClientRect();
        const centerX = rect.left + area.clientWidth / 2;
        const centerY = rect.top + area.clientHeight / 2;

        stick.movePointer(centerX + dx, centerY + dy);
    }
});

document.addEventListener('mouseup', () => {
    if (mouseActive === 'left') {
        leftStick.resetKnob();
    } else if (mouseActive === 'right') {
        rightStick.resetKnob();
    }
    mouseActive = null;
});

// UI control setup
window.onload = () => {
    const takeoverBtn = document.getElementById("takeover-button");
    takeoverBtn.classList.add("active");

    takeoverBtn.onclick = () => {
        joystickState.use_web_inputs = !joystickState.use_web_inputs;
        takeoverBtn.classList.toggle("active", joystickState.use_web_inputs);
        takeoverBtn.classList.toggle("inactive", !joystickState.use_web_inputs);
        sendFullState();
    };

    const deadzoneBindings = [
        ["deadzone-left-x", "dead_zone_left", "x"],
        ["deadzone-left-y", "dead_zone_left", "y"],
        ["deadzone-right-x", "dead_zone_right", "x"],
        ["deadzone-right-y", "dead_zone_right", "y"]
    ];

    deadzoneBindings.forEach(([id, group, axis]) => {
        const slider = document.getElementById(id);
        slider.value = joystickState[group][axis];
        slider.oninput = () => {
            joystickState[group][axis] = parseFloat(slider.value);
            dirtyKeys.add(group);
            if (!ticking) startTickLoop();
        };
    });

    sendFullState();

    setupVideoFeed();
};

function sendFullState() {
    fetch('/api/joystick_input', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(joystickState)
    }).catch(err => console.error('POST error:', err));
}
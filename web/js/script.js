const client = mqtt.connect('ws://' + location.hostname + ':9001');
client.on('connect', () => console.log('MQTT connected'));
client.on('error', err => console.error('MQTT error:', err));

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

    function sendMQTT(normX, normY) {
        const payload = JSON.stringify({ x: normX, y: normY });
        client.publish(topic, payload);
    }

    function movePointer(globalX, globalY) {
        const rect = area.getBoundingClientRect();
        let dx = globalX - rect.left - originX;
        let dy = globalY - rect.top - originY;

        const maxRange = (areaRect.width / 2) - (knob.offsetWidth / 2);
        dx = Math.max(-maxRange, Math.min(maxRange, dx));
        dy = Math.max(-maxRange, Math.min(maxRange, dy));

        setKnob(originX + dx, originY + dy);

        const normX = dx / maxRange;
        const normY = -dy / maxRange; // invert Y so up=+1
        sendMQTT(normX, normY);
    }

    function resetKnob() {
        const x = autoCenterX ? originX : knob.offsetLeft;
        const y = autoCenterY ? originY : knob.offsetTop;
        setKnob(x, y);

        const normX = autoCenterX ? 0 : (x - originX) / ((areaRect.width / 2) - (knob.offsetWidth / 2));
        const normY = autoCenterY ? 0 : -(y - originY) / ((areaRect.height / 2) - (knob.offsetHeight / 2));
        sendMQTT(normX, normY);
    }

    return { movePointer, resetKnob, area };
}

const leftStick = createStick('left-area', 'left-knob', 'robot/control/left', true, true);
const rightStick = createStick('right-area', 'right-knob', 'robot/control/right', true, false);

const activeTouches = {}; // touchId → {side: 'left'|'right'}

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
            activeTouches[touch.identifier] = 'left';
            leftStick.movePointer(touch.clientX, touch.clientY);
        } else if (touchStartedInArea(touch, rightStick.area)) {
            activeTouches[touch.identifier] = 'right';
            rightStick.movePointer(touch.clientX, touch.clientY);
        }
        // else: ignore touches starting outside both areas
    }
}

function handleTouchMove(e) {
    for (const touch of e.changedTouches) {
        const side = activeTouches[touch.identifier];
        if (side === 'left') {
            leftStick.movePointer(touch.clientX, touch.clientY);
        } else if (side === 'right') {
            rightStick.movePointer(touch.clientX, touch.clientY);
        }
    }
}

function handleTouchEnd(e) {
    for (const touch of e.changedTouches) {
        const side = activeTouches[touch.identifier];
        if (side === 'left') {
            leftStick.resetKnob();
        } else if (side === 'right') {
            rightStick.resetKnob();
        }
        delete activeTouches[touch.identifier];
    }
}

document.addEventListener('touchstart', handleTouchStart);
document.addEventListener('touchmove', handleTouchMove);
document.addEventListener('touchend', handleTouchEnd);
document.addEventListener('touchcancel', handleTouchEnd);

// Mouse support for testing
let mouseActiveSide = null;
document.addEventListener('mousedown', e => {
    if (leftStick.area.contains(e.target)) {
        mouseActiveSide = 'left';
        leftStick.movePointer(e.clientX, e.clientY);
    } else if (rightStick.area.contains(e.target)) {
        mouseActiveSide = 'right';
        rightStick.movePointer(e.clientX, e.clientY);
    }
});
document.addEventListener('mousemove', e => {
    if (mouseActiveSide === 'left') {
        leftStick.movePointer(e.clientX, e.clientY);
    } else if (mouseActiveSide === 'right') {
        rightStick.movePointer(e.clientX, e.clientY);
    }
});
document.addEventListener('mouseup', () => {
    if (mouseActiveSide === 'left') {
        leftStick.resetKnob();
    } else if (mouseActiveSide === 'right') {
        rightStick.resetKnob();
    }
    mouseActiveSide = null;
});

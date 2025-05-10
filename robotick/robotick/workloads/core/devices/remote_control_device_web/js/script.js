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
        const payload = JSON.stringify([normX, normY]);
        client.publish(topic, payload);
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
        const normY = -dy / maxRangeY; // invert Y so up=+1
        
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

const leftStick = createStick('left-area', 'left-knob', 'control/remote_control_device/left_stick', true, true);
const rightStick = createStick('right-area', 'right-knob', 'control/remote_control_device/right_stick', true, false);

const activeTouches = {}; // touchId → { side: 'left'|'right', startX, startY }

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
            if (touchData.side === 'left') {
                leftStick.resetKnob();
            } else if (touchData.side === 'right') {
                rightStick.resetKnob();
            }
            delete activeTouches[touch.identifier];
        }
    }
}

document.addEventListener('touchstart', handleTouchStart);
document.addEventListener('touchmove', handleTouchMove);
document.addEventListener('touchend', handleTouchEnd);
document.addEventListener('touchcancel', handleTouchEnd);

// Mouse support for testing
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

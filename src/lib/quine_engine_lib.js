let isRunning = true;
let currentTick = 0;
let nextTick = 0;
let pendingTileUpdates = [];

let config = {
    ticksPerSecond: 60,
    skipTicks: 16,
    windowWidth: 0,
    windowHeight: 0,
}

let rootId = 'game';
let rootElement;

let bgText = '';
let backgroundColor = '#000000';
let charSizePx = 13;
let charBoxSizePercent = 0.8
let charsPerRow = 0;

let init = () => { }
let beforeRun = () => { }
let afterRun = () => { }

let onGameTick = () => { }
let onFrameRender = () => {
    while (pendingTileUpdates.length > 0) {
        const change = pendingTileUpdates.shift();
        setPixel(change.x, change.y, change.color);
    }
}

let onWindowResize = () => { }

const renderBackground = () => {
    rootElement = document.getElementById(rootId);
    charsPerRow = Math.floor(rootElement.offsetWidth / (charSizePx * charBoxSizePercent));

    if(bgText.length < 1) {
        rootElement.innerHTML = '';
        return;
    }
    const res = '<div style="width:100%;display:flex;flex-wrap:nowrap;">' + bgText.split("").filter(ch => ch !== ' ').map((ch, idx) => {
        if (idx % charsPerRow === 0) return `</div><div style="width:100%;display:flex;flex-wrap:nowrap;">`
        else return `<div style="color: ${backgroundColor}; font-size:${charSizePx}px; min-width: ${charBoxSizePercent}em; min-height: ${charBoxSizePercent}em;user-select:none;">${ch}</div>`
    }).join("") + '</div>';

    rootElement.innerHTML = res;
    rootElement.style['font-size'] = `${charSizePx}px`;
    rootElement.style['overflow-x'] = 'hidden';
    rootElement.style['overflow-y'] = 'auto';
    rootElement.removeChild(rootElement.children[0]);
}

const run = async () => {
    beforeRun();

    let sleepTime = 0;
    isRunning = true;
    while (isRunning) {
        currentTick++;
        while (currentTick > nextTick) {
            onGameTick();
            nextTick += config.skipTicks;
        }
        onFrameRender();
        sleepTime = nextTick - currentTick;
        if (sleepTime >= 0)
            await new Promise(r => setTimeout(r, sleepTime));
    }

    afterRun();
}

export const setBgText = (newBgText) => { bgText = newBgText; }

export const setRootId = (newRootId) => { rootId = newRootId; }

export const setIsRunning = (newIsRunning) => { isRunning = newIsRunning; }
export const getIsRunning = () => { return isRunning; }

export const setBackgroundColor = (newColor) => { backgroundColor = newColor; }

export const setCharSizePx = (newSize) => { charSizePx = newSize; }

export const setInit = (newInit) => { init = newInit; }

export const setBeforeRun = (newBeforeRun) => { beforeRun = newBeforeRun; }

export const setAfterRun = (newAfterRun) => { afterRun = newAfterRun; }

export const setOnGameTick = (newOnGameTick) => { onGameTick = newOnGameTick; }

export const setOnFrameRender = (newOnFrameRender) => { onFrameRender = newOnFrameRender; }
export const setPixel = (x, y, color) => {
    const pixel = rootElement.children[y]?.children[x];
    if(typeof pixel !== 'undefined')
        pixel.style['color'] = color;
}

export const setOnWindowResize = (newOnWindowResize) => { onWindowResize = newOnWindowResize; }

export const getCurrentGameTick = () => { return currentTick; }

export const getTicksPerSecond = () => { return config.ticksPerSecond; }

export const getWindowDimensions = () => { return { windowWidth: config.windowWidth, windowHeight: config.windowHeight }; }

export const initConfig = (newConfig) => {
    config = { ...config, ...newConfig };
};

export const renderAtPos = (x, y, color) => {
    x += Math.floor((charsPerRow - config.windowWidth) / 2) - 1;
    pendingTileUpdates.push({ x: x, y: y, color: color })
}

export const clearPendingFrameChanges = () => {
    pendingTileUpdates = [];
}

export const setKeybindings = (keybindings) => {
    window.addEventListener('keypress', (e) => {
        e = e.key.toLocaleLowerCase();
        keybindings.forEach(keybinding => {
            if (keybinding.key === e) {
                keybinding.action();
            }
        });
    }, false);
}

export const start = () => {
    window.addEventListener("resize", (_) => {
        let windowWidth = rootElement.offsetWidth;
        charsPerRow = Math.floor(windowWidth / (charSizePx * charBoxSizePercent));
        renderBackground();
        onWindowResize();
    });

    renderBackground();
    init();
    run();
}
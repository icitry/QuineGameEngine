import * as lib from "../../src/lib/quine_engine_lib.js"

const bgColor = "#41454d";

const Direction = {
    Up: { v2d: { x: 0, y: -1 }, key: 'w' },
    Down: { v2d: { x: 0, y: 1 }, key: 's' },
    Left: { v2d: { x: -1, y: 0 }, key: 'a' },
    Right: { v2d: { x: 1, y: 0 }, key: 'd' },
    None: { v2d: { x: 0, y: 0 }, key: 'q' }
};

const GhostType = {
    Blinky: 0,
    Pinky: 1,
    Inky: 2,
    Clyde: 3,
};

const GhostState = {
    None: 0,
    Chase: 1,
    Scatter: 2,
    Frightened: 3,
}

const TileType = {
    Empty: 0,
    GhostBlinky: 1,
    GhostPinky: 2,
    GhostInky: 3,
    GhostClyde: 4,
    PacMan: 5,
    Wall: 6,
    Point: 7,
    Energizer: 8,
    Heart: 9
};

const TileTypeToColorMap = new Map([
    [TileType.Empty, bgColor],
    [TileType.GhostBlinky, "#e81515"],
    [TileType.GhostPinky, "#e815a2"],
    [TileType.GhostInky, "#15bed1"],
    [TileType.GhostClyde, "#d67f15"],
    [TileType.PacMan, "#f0d807"],
    [TileType.Wall, "#290fd4"],
    [TileType.Point, "#ffffff"],
    [TileType.Energizer, "#0fd478"],
    [TileType.Heart, "#b30c28"]
])

let GlobalState = {
    gameData: {
        score: 0,
        numLives: 0,
        level: 0,
        levelMultiplier: 0,

        ghosts: [],
        pacMan: {},

        tiles: [],
        heartTiles: [],
        remainingPointTiles: 0
    },

    defVals: {
        pacmanMaxLives: 3,
        totalPointTiles: 0,

        pacManStartPos: { x: 13, y: 26 },
        ghostStartPos: [
            { x: 13, y: 16 },
            { x: 13, y: 17 },
            { x: 11, y: 17 },
            { x: 15, y: 17 }
        ],
        ghostScatterTargetPos: [
            { x: 25, y: 0 },
            { x: 2, y: 0 },
            { x: 27, y: 34 },
            { x: 0, y: 34 },
        ]
    }
};

const getRandomInt = (max) => { return Math.floor(Math.random() * max); }

const vector2dAdd = (a, b) => { return { x: a.x + b.x, y: a.y + b.y }; }
const vector2dSub = (a, b) => { return { x: a.x - b.x, y: a.y - b.y }; }
const vector2dMulScalar = (a, s) => { return { x: Math.floor(a.x * s), y: Math.floor(a.y * s) }; }
const vector2dEq = (a, b) => { return a.x === b.x && a.y === b.y; }
const vector2dEuclideanDistance = (a, b) => { return (a.x - b.x) ** 2 + (a.y - b.y) ** 2; }

const reverseDir = (dir) => { return vector2dMulScalar(dir, -1); }
const clampVector2d = (a, minValX, maxValX, minValY, maxValY) => { return { x: a.x < minValX ? maxValX : a.x > maxValX ? minValX : a.x, y: a.y < minValY ? maxValY : a.y > maxValY ? minValY : a.y }; }

const genRandomTarget = () => {
    const windowDimensions = lib.getWindowDimensions();
    return { x: getRandomInt(windowDimensions.windowWidth), y: getRandomInt(windowDimensions.windowHeight) }
}

const calculateLevelMultiplier = (level) => { return 1 + level * 0.1; }

const getTileColor = (tile) => {
    let color = TileTypeToColorMap.get(tile.type);
    if (typeof color === 'undefined') color = bgColor;
    if (!tile.canInteract && [TileType.Point, TileType.Heart, TileType.Energizer].includes(tile.type)) color = bgColor;
    return color;
}

const renderTiles = () => {
    const windowDimensions = lib.getWindowDimensions();
    for (let i = 0; i < windowDimensions.windowHeight; i++)
        for (let j = 0; j < windowDimensions.windowWidth; j++)
            lib.renderAtPos(j, i, getTileColor(GlobalState.gameData.tiles[i][j]));
}

const initGhost = (type, releaseThreshold, initialPos = null) => {
    GlobalState.gameData.ghosts[type] = {
        entityState: { pos: initialPos !== null ? initialPos : GlobalState.defVals.ghostStartPos[type], dir: Direction.Left.v2d },
        type: type,
        ghostState: GhostState.None,
        targetPos: GlobalState.defVals.ghostScatterTargetPos[type],
        releaseThreshold: releaseThreshold,
        lastFrightened: -1,
        lastEaten: -1,
        lastChase: -1,
        lastScatter: -1,
        stateCyclesCompleted: 0
    }
}

const initGhosts = () => {
    initGhost(GhostType.Blinky, GlobalState.gameData.score, { x: 13, y: 14 });
    initGhost(GhostType.Pinky, GlobalState.gameData.score);
    initGhost(GhostType.Inky, GlobalState.gameData.score + Math.floor(30 / GlobalState.gameData.levelMultiplier));
    initGhost(GhostType.Clyde, GlobalState.gameData.score + Math.floor(60 / GlobalState.gameData.levelMultiplier));
}

const initPacMan = () => {
    GlobalState.gameData.pacMan = {
        entityState: {
            pos: GlobalState.defVals.pacManStartPos,
            dir: Direction.None.v2d
        }
    }
}

const setTile = (c, x, y) => {
    GlobalState.gameData.tiles[y][x] = {
        type: TileType.Empty,
        defaultType: TileType.Empty,
        canInteract: true
    }

    switch (c) {
        case '#':
            GlobalState.gameData.tiles[y][x].type = TileType.Wall;
            GlobalState.gameData.tiles[y][x].defaultType = TileType.Wall;
            break;
        case '.':
            GlobalState.gameData.tiles[y][x].type = TileType.Point;
            GlobalState.gameData.tiles[y][x].defaultType = TileType.Point;
            GlobalState.defVals.totalPointTiles++;
            break;
        case '@':
            GlobalState.gameData.tiles[y][x].type = TileType.Energizer;
            GlobalState.gameData.tiles[y][x].defaultType = TileType.Energizer;
            break;
        case 'o':
            if (GlobalState.gameData.numLives >= GlobalState.defVals.pacmanMaxLives) break;
            GlobalState.gameData.tiles[y][x].type = TileType.Heart;
            GlobalState.gameData.tiles[y][x].defaultType = TileType.Heart;
            GlobalState.gameData.heartTiles[GlobalState.gameData.numLives] = { x: x, y: y }
            GlobalState.gameData.numLives++;
            break;
        default:
            break;
    }
}

const initTiles = () => {
    const tilemap =
        "                            \
                            \
                            \
############################\
#............##............#\
#.####.#####.##.#####.####.#\
#@#  #.#   #.##.#   #.#  #@#\
#.####.#####.##.#####.####.#\
#..........................#\
#.####.##.########.##.####.#\
#.####.##.########.##.####.#\
#......##....##....##......#\
######.##### ## #####.######\
     #.##### ## #####.#     \
     #.##          ##.#     \
     #.## ###  ### ##.#     \
######.## #      # ##.######\
      .   #      #   .      \
######.## #      # ##.######\
     #.## ######## ##.#     \
     #.##          ##.#     \
     #.## ######## ##.#     \
######.## ######## ##.######\
#............##............#\
#.####.#####.##.#####.####.#\
#.####.#####.##.#####.####.#\
#@..##.......  .......##..@#\
###.##.##.########.##.##.###\
###.##.##.########.##.##.###\
#......##....##....##......#\
#.##########.##.##########.#\
#.##########.##.##########.#\
#..........................#\
############################\
 ooo                        \
                            ";

    const windowDimensions = lib.getWindowDimensions();

    for (let i = 0; i < windowDimensions.windowHeight; i++) {
        GlobalState.gameData.tiles[i] = []
        for (let j = 0; j < windowDimensions.windowWidth; j++) {
            let tileIdx = i * windowDimensions.windowWidth + j;
            setTile(tilemap[tileIdx], j, i);
        }
    }
}

const resetTiles = () => {
    const windowDimensions = lib.getWindowDimensions();

    for (let i = 0; i < windowDimensions.windowHeight; i++) {
        for (let j = 0; j < windowDimensions.windowWidth; j++) {
            GlobalState.gameData.tiles[i][j].type = GlobalState.gameData.tiles[i][j].defaultType;
            GlobalState.gameData.tiles[i][j].canInteract = true;
        }
    }
}

const initLevel = (level) => {
    GlobalState.gameData.level = level;
    GlobalState.gameData.levelMultiplier = calculateLevelMultiplier(level);

    lib.clearPendingFrameChanges();

    if (level == 0) initTiles();
    else resetTiles();

    GlobalState.gameData.remainingPointTiles = GlobalState.defVals.totalPointTiles;
    initGhosts();
    initPacMan();
}

const offsetScore = (offset) => {
    GlobalState.gameData.score += offset;
    document.getElementById("score").innerHTML = `SCORE: ${GlobalState.gameData.score}`;
}

const frightenGhosts = () => {
    for (const type in Object.keys(GhostType)) {
        if (GlobalState.gameData.ghosts[type].ghostState === GhostState.None) continue;
        if (GlobalState.gameData.ghosts[type].ghostState === GhostState.Scatter || GlobalState.gameData.ghosts[type].ghostState === GhostState.Chase) {
            GlobalState.gameData.ghosts[type].entityState.dir = reverseDir(GlobalState.gameData.ghosts[type].entityState.dir);
        }

        GlobalState.gameData.ghosts[type].ghostState = GhostState.Frightened;
        GlobalState.gameData.ghosts[type].lastFrightened = lib.getCurrentGameTick();
    }
}

const eatGhost = (ghost) => {
    ghost.ghostState = GhostState.None;
    ghost.entityState.dir = Direction.None.v2d;
    ghost.entityState.pos = GlobalState.defVals.ghostStartPos[ghost.type];
    ghost.lastEaten = lib.getCurrentGameTick();
    offsetScore(10);
}

const checkPacManCollisions = (newPos) => {
    const collisionsMap = new Map([
        [TileType.Wall, 2],
        [TileType.Point, 1],
        [TileType.Energizer, 1],
        [TileType.GhostBlinky, -1],
        [TileType.GhostPinky, -1],
        [TileType.GhostInky, -1],
        [TileType.GhostClyde, -1],
    ]);

    let ghostType = GlobalState.gameData.tiles[newPos.y][newPos.x].type;
    let collisionRes = collisionsMap.get(ghostType);

    if (collisionRes === 1) {
        if(GlobalState.gameData.tiles[newPos.y][newPos.x].canInteract)
            GlobalState.gameData.tiles[newPos.y][newPos.x].canInteract = false;
        else 
            collisionRes = 0;
    }
    if (ghostType === TileType.Energizer)
        frightenGhosts();
    if (ghostType === TileType.GhostBlinky && GlobalState.gameData.ghosts[GhostType.Blinky].ghostState === GhostState.Frightened) {
        eatGhost(GlobalState.gameData.ghosts[GhostType.Blinky]);
        collisionRes = 1;
    }
    if (ghostType === TileType.GhostPinky && GlobalState.gameData.ghosts[GhostType.Pinky].ghostState === GhostState.Frightened) {
        eatGhost(GlobalState.gameData.ghosts[GhostType.Pinky]);
        collisionRes = 1;
    }
    if (ghostType === TileType.GhostInky && GlobalState.gameData.ghosts[GhostType.Inky].ghostState === GhostState.Frightened) {
        eatGhost(GlobalState.gameData.ghosts[GhostType.Inky]);
        collisionRes = 1;
    }
    if (ghostType === TileType.GhostClyde && GlobalState.gameData.ghosts[GhostType.Clyde].ghostState === GhostState.Frightened) {
        eatGhost(GlobalState.gameData.ghosts[GhostType.Clyde]);
        collisionRes = 1;
    }
    return collisionRes;
}

const updatePacManPos = (newPos) => {
    let pX = GlobalState.gameData.pacMan.entityState.pos.x, pY = GlobalState.gameData.pacMan.entityState.pos.y;
    GlobalState.gameData.tiles[pY][pX].type = GlobalState.gameData.tiles[pY][pX].defaultType;
    lib.renderAtPos(pX, pY, getTileColor(GlobalState.gameData.tiles[pY][pX]));

    GlobalState.gameData.pacMan.entityState.pos = { x: newPos.x, y: newPos.y };
    pX = newPos.x, pY = newPos.y;

    GlobalState.gameData.tiles[pY][pX].type = TileType.PacMan;
    lib.renderAtPos(pX, pY, getTileColor(GlobalState.gameData.tiles[pY][pX]));
}

const pacManLoseLife = () => {
    if (GlobalState.gameData.numLives === 0) {
        lib.setIsRunning(false);
        return;
    }

    GlobalState.gameData.numLives--;
    let heartTilePos = GlobalState.gameData.heartTiles[GlobalState.gameData.numLives];
    GlobalState.gameData.tiles[heartTilePos.y][heartTilePos.x].canInteract = false;
    lib.renderAtPos(heartTilePos.x, heartTilePos.y, getTileColor(GlobalState.gameData.tiles[heartTilePos.y][heartTilePos.x]));

    if (GlobalState.gameData.numLives === 0) {
        lib.setIsRunning(false);
        return;
    }

    updatePacManPos(GlobalState.defVals.pacManStartPos);
}

const updatePacMan = () => {
    let pPos = GlobalState.gameData.pacMan.entityState.pos, pDir = GlobalState.gameData.pacMan.entityState.dir;
    let newPos = vector2dAdd(pPos, pDir);

    const windowDimensions = lib.getWindowDimensions();

    newPos = clampVector2d(newPos, 0, windowDimensions.windowWidth - 1, 0, windowDimensions.windowHeight - 1);

    let collisionResult = checkPacManCollisions(newPos);

    if (collisionResult === 2) {
        return;
    }
    else if (collisionResult === 1) {
        offsetScore(1);
        GlobalState.gameData.remainingPointTiles--;

        if (GlobalState.gameData.remainingPointTiles == 0) {
            initLevel(GlobalState.gameData.level + 1);
            renderTiles();
            return;
        }
    }
    else if (collisionResult == -1) {
        pacManLoseLife();
        return;
    }

    updatePacManPos(newPos);
}

const navigateGhostStateCycle = (ghost, scatterDurationS, chaseDurationS) => {
    const currentTick = lib.getCurrentGameTick();
    const ticksPerSecond = lib.getTicksPerSecond();

    if (scatterDurationS > 0) {
        if (ghost.ghostState === GhostState.Scatter && currentTick - ghost.lastScatter >= scatterDurationS * ticksPerSecond) {
            ghost.ghostState = GhostState.Chase;
            ghost.lastChase = currentTick;
        }
        if (chaseDurationS > 0) {
            if (ghost.ghostState === GhostState.Chase && currentTick - ghost.lastChase >= chaseDurationS * ticksPerSecond) {
                ghost.ghostState = GhostState.Scatter;
                ghost.lastScatter = currentTick;
                ghost.stateCyclesCompleted++;
            }
        }
    }
}

const updateGhostState = (ghost) => {
    if (ghost.releaseThreshold > GlobalState.gameData.score) return;
    const oldState = JSON.parse(JSON.stringify(ghost.ghostState));
    const currentTick = lib.getCurrentGameTick();
    const ticksPerSecond = lib.getTicksPerSecond();

    if (ghost.ghostState === GhostState.None) {
        if (ghost.lastEaten === -1 || currentTick - ghost.lastEaten > 6 * ticksPerSecond) {
            ghost.ghostState = GhostState.Scatter;
            ghost.lastScatter = currentTick;
        }
        return;
    }

    if (ghost.ghostState === GhostState.Frightened) {
        if (currentTick - ghost.lastFrightened > 6 * ticksPerSecond) {
            ghost.lastScatter += currentTick - ghost.lastFrightened;
            ghost.lastChase += currentTick - ghost.lastFrightened;
            ghost.ghostState = GhostState.Scatter;
        }
        else return;
    }

    switch (ghost.stateCyclesCompleted) {
        case 0:
        case 1:
            navigateGhostStateCycle(ghost, Math.floor(7 / GlobalState.gameData.levelMultiplier), 20);
            break;
        case 2:
            navigateGhostStateCycle(ghost, Math.floor(5 / GlobalState.gameData.levelMultiplier), 20);
            break;
        case 3:
            navigateGhostStateCycle(ghost, Math.floor(5 / GlobalState.gameData.levelMultiplier), -1);
            break;
        default:
            break;
    }

    if (oldState !== ghost.ghostState && (oldState === GhostState.Scatter || oldState === GhostState.Chase))
        ghost.entityState.dir = reverseDir(ghost.entityState.dir);
}

const updateGhostTarget = (ghost) => {
    const pacManState = GlobalState.gameData.pacMan.entityState;
    const currPos = ghost.entityState.pos;

    if (currPos.x >= 11 && currPos.x <= 16 && currPos.y >= 16 && currPos.y <= 18) {

        ghost.targetPos = { x: 13, y: 15 };
        return;
    }

    switch (ghost.ghostState) {
        case GhostState.Scatter:
            ghost.targetPos = GlobalState.defVals.ghostScatterTargetPos[ghost.type];
            break;
        case GhostState.Chase:
            switch (ghost.type) {
                case GhostType.Blinky:
                    ghost.targetPos = pacManState.pos;
                    break;
                case GhostType.Pinky:
                    ghost.targetPos = vector2dAdd(pacManState.pos, vector2dMulScalar(pacManState.dir, 4));
                    break;
                case GhostType.Inky:
                    const blinkyPos = GlobalState.gameData.ghosts[GhostType.Blinky].entityState.pos;
                    const p = vector2dAdd(pacManState.pos, vector2dMulScalar(pacManState.dir, 2));
                    const d = vector2dSub(p, blinkyPos);
                    ghost.targetPos = vector2dAdd(blinkyPos, vector2dMulScalar(d, 4));
                    break;
                case GhostType.Clyde:
                    if (vector2dEuclideanDistance(ghost.entityState.pos, pacManState.pos) > 64)
                        ghost.targetPos = pacManState.pos;
                    else
                        ghost.targetPos = GlobalState.defVals.ghostScatterTargetPos[GhostType.Clyde];
                    break;
                default:
                    break;
            }
        case GhostState.Frightened:
            ghost.targetPos = genRandomTarget();
            break;
        default:
            break;
    }
}

const checkGhostCollisions = (ghost, newPos) => {
    switch (GlobalState.gameData.tiles[newPos.y][newPos.x].type) {
        case TileType.Wall:
            return 2;
        case TileType.PacMan:
            if (ghost.ghostState === GhostState.Frightened) {
                eatGhost(ghost);
                return -1;
            } else {
                pacManLoseLife();
                return 1;
            }
        default:
            break;
    }
    return 0;
}

const isRedzone = (pos) => { return pos.x >= 10 && pos.x <= 17 && (pos.y === 14 || pos.y === 26); }

const updateGhostPos = (ghost) => {
    if (ghost.ghostState === GhostState.None) return;

    updateGhostTarget(ghost);

    const windowDimensions = lib.getWindowDimensions();

    let newPos = vector2dAdd(ghost.entityState.pos, vector2dMulScalar(ghost.entityState.dir, GlobalState.gameData.levelMultiplier));
    newPos = clampVector2d(newPos, 0, windowDimensions.windowWidth - 1, 0, windowDimensions.windowHeight - 1);

    const collisionResult = checkGhostCollisions(ghost, newPos);
    if (collisionResult === -1 || collisionResult === 2) return;
    ghost.entityState.pos = { x: newPos.x, y: newPos.y };

    let minDist = 100000;
    let dist = 0;
    const originalDirReverse = reverseDir(ghost.entityState.dir);
    for (let [key, val] of Object.entries(Direction)) {
        if (key === "None") continue;
        if (isRedzone(ghost.entityState.pos) && key === "Up") continue;

        if (vector2dEq(originalDirReverse, val.v2d)) continue;

        let testPos = vector2dAdd(ghost.entityState.pos, vector2dMulScalar(val.v2d, GlobalState.gameData.levelMultiplier));
        testPos = clampVector2d(testPos, 0, windowDimensions.windowWidth - 1, 0, windowDimensions.windowHeight - 1);

        const testCollisionResult = checkGhostCollisions(ghost, testPos);
        if (testCollisionResult !== -1 && testCollisionResult !== 2) {
            dist = vector2dEuclideanDistance(testPos, ghost.targetPos);
            if (dist < minDist) {
                minDist = dist;
                ghost.entityState.dir = val.v2d;
            }
        }
    }
}

const updateGhostRepr = (ghost, oldPos) => {
    const posX = ghost.entityState.pos.x, posY = ghost.entityState.pos.y;
    GlobalState.gameData.tiles[oldPos.y][oldPos.x].type = GlobalState.gameData.tiles[oldPos.y][oldPos.x].defaultType;
    lib.renderAtPos(oldPos.x, oldPos.y, getTileColor(GlobalState.gameData.tiles[oldPos.y][oldPos.x]));

    switch (ghost.type) {
        case GhostType.Blinky:
            GlobalState.gameData.tiles[posY][posX].type = TileType.GhostBlinky;
            break;
        case GhostType.Pinky:
            GlobalState.gameData.tiles[posY][posX].type = TileType.GhostPinky;
            break;
        case GhostType.Inky:
            GlobalState.gameData.tiles[posY][posX].type = TileType.GhostInky;
            break;
        case GhostType.Clyde:
            GlobalState.gameData.tiles[posY][posX].type = TileType.GhostClyde;
            break;
        default:
            break;
    }
    lib.renderAtPos(posX, posY, getTileColor(GlobalState.gameData.tiles[posY][posX]));
}

const updateGhost = (ghost) => {
    const oldPos = JSON.parse(JSON.stringify(ghost.entityState.pos));
    updateGhostState(ghost);
    updateGhostPos(ghost);
    updateGhostRepr(ghost, oldPos);
}

const updateGhosts = () => {
    updateGhost(GlobalState.gameData.ghosts[GhostType.Blinky]);
    updateGhost(GlobalState.gameData.ghosts[GhostType.Pinky]);
    updateGhost(GlobalState.gameData.ghosts[GhostType.Inky]);
    updateGhost(GlobalState.gameData.ghosts[GhostType.Clyde]);
}

lib.setBackgroundColor(bgColor);

lib.initConfig({
    windowWidth: 28,
    windowHeight: 36
});

lib.setInit(() => {
    initLevel(0);
})

lib.setBeforeRun(() => {
    renderTiles();
})

lib.setOnGameTick(() => {
    updateGhosts();
    updatePacMan();
})

lib.setOnWindowResize(() => {
    renderTiles();
});

lib.setKeybindings([
    {
        key: Direction.Up.key,
        action: () => { GlobalState.gameData.pacMan.entityState.dir = Direction.Up.v2d; }
    },
    {
        key: Direction.Down.key,
        action: () => { GlobalState.gameData.pacMan.entityState.dir = Direction.Down.v2d; }
    },
    {
        key: Direction.Left.key,
        action: () => { GlobalState.gameData.pacMan.entityState.dir = Direction.Left.v2d; }
    },
    {
        key: Direction.Right.key,
        action: () => { GlobalState.gameData.pacMan.entityState.dir = Direction.Right.v2d; }
    },
    {
        key: 'q',
        action: () => { lib.setIsRunning(false); }
    },
    {
        key: ' ',
        action: () => { if (!lib.getIsRunning()) lib.start(); }
    }
]);

lib.start();
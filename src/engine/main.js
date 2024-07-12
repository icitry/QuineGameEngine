const args = process.argv.slice(2);

if(args.length < 1) {
    console.error("No required arguments provided.");
    process.exit(1);
}

const srcFilePath = args[0];

if(srcFilePath.length < 1) {
    console.error("No main source file provided.");
    process.exit(1);
}

import * as fs from 'fs';
import * as path from 'path';

if(!fs.existsSync(srcFilePath)) {
    console.error("Specified main source file does not exist.");
    process.exit(1);
}

if(path.extname(srcFilePath) !== '.js') {
    console.error("Specified main source file is not a JS file.");
    process.exit(1);
}

let libPath = JSON.stringify(path.resolve('../lib/quine_engine_lib.js'));
libPath = libPath.substring(1, libPath.length-1);

import * as rollup from 'rollup';
import config from './rollup.config.mjs'

config.input = path.join(path.parse(srcFilePath).dir, `${path.parse(srcFilePath).name}-wrap.js`)
config.output.file = `build/${path.parse(srcFilePath).name}-build.js`;

fs.copyFileSync(srcFilePath, config.input);

const wrapContent = (filePath) => {
    const before = `\nimport { setBgText } from '${libPath}';\nconst r=()=>{\nsetBgText(\`const r=\${r};r();\`);\n`;
    const after = "\n};r();"

    const importEndRegex = /^(import\s|require\().*/gm;
    const content = fs.readFileSync(filePath, 'utf8')

    let match;
    let insertIndex = 0;
    while ((match = importEndRegex.exec(content)) !== null) {
        insertIndex = importEndRegex.lastIndex;
    }
    const combinedContent = content.slice(0, insertIndex) +  before + content.slice(insertIndex) + after;
    fs.writeFileSync(filePath, combinedContent)
}

const createBundle = async (config) => {
    try {
        const bundle = await rollup.rollup(config);
        await bundle.write(config.output);
        console.log('Bundle created successfully!');
    } catch (error) {
        console.error('Error during bundle creation:', error);
    }
}

const cleanup = () => {
    fs.unlinkSync(config.input);
}

const build = async () => {
    wrapContent(config.input);
    await createBundle(config);
    cleanup();
}

build();
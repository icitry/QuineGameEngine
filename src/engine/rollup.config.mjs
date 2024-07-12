import terser from '@rollup/plugin-terser';

export default {
    input: 'tba-input.js',
    output: {
        file: 'build/tba-output.js',
        inlineDynamicImports: true,
        compact: true,
        generatedCode: "es2015",
        format: 'es',
    },
    plugins: [
        terser({
            ecma: 2016,
            compress: {
                arguments: true,
                booleans_as_integers: true,
                module: true,
                passes: 10,
            },
            mangle: {
                properties: {
                    regex: /^/
                }
            }
        })
    ],
};
import globals from "globals";
import js from "@eslint/js";

export default [
    js.configs.recommended,
    {
        files: ["**/*.js"],
        ignores: ["third-party"],

        languageOptions: {
            globals: {
                ...globals.browser,
            },

            sourceType: "script",
        },


        rules: {
            semi: ["error"],

            "no-unused-vars": ["error", {
                args: "none",
            }],

            quotes: ["error", "single"],
            strict: ["error", "global"],
            eqeqeq: ["error"],
            "no-var": ["error"],
            "prefer-const": ["error"],
        },
    }
];
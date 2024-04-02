const FusesPlugin = require('@electron-forge/plugin-fuses').FusesPlugin;
const FuseV1Options = require('@electron/fuses').FuseV1Options;
const FuseVersion = require('@electron/fuses').FuseVersion;

exports.packagerConfig = {
  asar: true,
  ignore: [
    '**/node_modules/**',
    '**/out/**',
    '**/bin/**',
    '**/src/**',
    '**/docs/**'
  ],
  'icon': './gui/icon',
};
  exports.rebuildConfig = {};
  exports.makers = [
    {
      name: '@electron-forge/maker-squirrel',
      config: {},
    },
    {
      name: '@electron-forge/maker-zip',
      platforms: ['darwin'],
    },
    {
      name: '@electron-forge/maker-deb',
      config: {
        options: {
          icon: './gui/icon.png',
        },
      },
    }
  ];
exports.plugins = [
  {
    name: '@electron-forge/plugin-auto-unpack-natives',
    config: {},
  },
  // Fuses are used to enable/disable various Electron functionality
  // at package time, before code signing the application
  new FusesPlugin({
    version: FuseVersion.V1,
    [FuseV1Options.RunAsNode]: false,
    [FuseV1Options.EnableCookieEncryption]: true,
    [FuseV1Options.EnableNodeOptionsEnvironmentVariable]: false,
    [FuseV1Options.EnableNodeCliInspectArguments]: false,
    [FuseV1Options.EnableEmbeddedAsarIntegrityValidation]: true,
    [FuseV1Options.OnlyLoadAppFromAsar]: true,
  }),
];

const FusesPlugin = require('@electron-forge/plugin-fuses').FusesPlugin;
const FuseV1Options = require('@electron/fuses').FuseV1Options;
const FuseVersion = require('@electron/fuses').FuseVersion;
const os = require('os');
let osPath
if (os.platform() === 'win32') {
  osPath = 'win'
} else if (os.platform() === 'darwin') {
  osPath = 'mac'
} else {
  osPath = 'linux'
}

exports.packagerConfig = {
  asar: true,
  extraFiles: [{
    "from": `bin/${osPath}`,
    "to": "bin",
    "filter": [
      "**/*"
    ]
  }],
  'icon': './gui/icon',
};
  exports.rebuildConfig = {};
  exports.makers = [
    {
      name: '@electron-forge/maker-squirrel',
      config: {
        'author': 'Rodrigo Arce',
        'description': 'Bialet Desktop is a graphical user interface for Bialet.'
      },
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
          mantainer: 'Rodrigo Arce',
          homepage: 'https://bialet.dev',
          genericName: 'Web Application Builder',
          productName: 'Bialet Desktop',
          depends: ["libssl3", "libcurl4", "libsqlite3-dev"],
          categories: ["Development", "Utility"],
          description: 'Bialet Desktop is a graphical user interface for Bialet.',
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

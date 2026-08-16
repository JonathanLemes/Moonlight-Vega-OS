module.exports = {
  dependency: {
    platforms: {
      kepler: {
        autolink: {
          MoonlightVegaCore: {
            libraryName: 'libMoonlightVegaCore.so',
            linkDynamic: true,
            provider: 'application',
            components: [],
            turbomodules: ['MoonlightVegaCore'],
          },
        },
      },
    },
  },
};


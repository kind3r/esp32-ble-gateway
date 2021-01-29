const HtmlWebpackInlineSourcePlugin = require('html-webpack-inline-source-plugin');

module.exports = {
  productionSourceMap: false,
  css: {
    extract: false
  },
  configureWebpack: {
    optimization: {
      splitChunks: false
    },
    plugins: [
      new HtmlWebpackInlineSourcePlugin(),
    ],
  },
  chainWebpack: (config) => {
    config
      .plugin('html')
      .tap((args) => {
        args[0].inlineSource = '.(js|css)$';
        return args
      });
  },
  pluginOptions: {
    compression: {
      gzip: {
        filename: '[path].gz[query]',
        algorithm: 'gzip',
        include: /\.(html)(\?.*)?$/i,
        minRatio: 0.8,
        deleteOriginalAssets: true
      }
    }
  }
}
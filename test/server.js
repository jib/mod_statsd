var http = require("http");
var U    = require('util');

http.createServer(function(request, response) {
//  U.debug( U.inspect( request ) )
//  U.debug( U.inspect( response ) )
  U.debug( U.inspect( request.url ) );
  U.debug( U.inspect( request.headers ) ); 

  // different response code?
  var m = request.url.match(/^\/(\d+)/);
  var r = m && m[0] ? m[1] : 204;

  response.writeHead(r);
  response.end();
}).listen( process.argv[2] || 7001 );

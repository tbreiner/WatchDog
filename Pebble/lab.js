/* Script for sending messages from Pebble Watch to middleware.
 * Run with WatchDog.c
 * Authors: Ryan Smith, Theresa Breiner, and Brad Thompson
 * Citation: The initial skelton and onload function were provided
 *           by Dr. Chris Murphy at the University of Pennsylvania.
 */
Pebble.addEventListener("appmessage",
  function(e) {
    sendToServer(e.payload.hello_msg);}
);

function sendToServer(requestMessage) {

	var req = new XMLHttpRequest();
	var ipAddress = "10.0.0.4"; // Hard coded IP address
	var port = "3002"; // Same port specified as argument to server
	var url = "http://" + ipAddress + ":" + port + "/" + requestMessage;
	var method = "GET";
	var async = true;

	req.onload = function(e) {
                // see what came back
                var msg = "no response";
                var response = JSON.parse(req.responseText);
                if (response) {
                    if (response.name) {
                        msg = response.name;
                    }
                    else msg = "noname";
                }
                // sends message back to pebble
                Pebble.sendAppMessage({ "0": msg });
	};
  
  req.onerror = function(e) {
    var msg = "Server Error!!!";
    Pebble.sendAppMessage({ "0": msg });
  };

        req.open(method, url, async);
        req.send(null);
}
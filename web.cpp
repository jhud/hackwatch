#include "web.h"

#include <HTTPClient.h>





void toggleRoomLights() {
	        HTTPClient http;
	static bool isOn = true;
	isOn = !isOn;
	  
	  http.begin("http://192.168.2.111/state/");
	  http.addHeader("Content-Type", "application/x-www-form-urlencoded"); 
	  int httpCode = http.POST(isOn?"relay=on":"");
	  if (httpCode > 0) {
	  	http.writeToStream(&Serial);
			  Serial.println("Toggled room light.");
	  }
	  else {
	  		  Serial.print("Error: ");
			  Serial.println(httpCode);
	  }
	  http.end();

	  
}

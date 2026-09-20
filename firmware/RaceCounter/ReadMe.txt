Start Up:

if both buttons are pressed
	clear Standalone Mode
	clear SSID and WiFi password
	Restart - device comes up in AP mode to allow WiFi setup

if Standalone Mode is set
	Device skips WiFi connection and web server setup
	Go to Operation Mode - controlled via buttons only, no web interface

if there are no saved WiFi credentials
	Device will come up in WiFI AP mode serving SSID "RaceCounter-Setup" (No password is required to connect)
	Serves a web page at http://192.168.4.1 where SSID and password can be set
		When the SSID and password are submitted, they are saved 
		restart

	if both buttons are pressed (while in AP mode)
		set standalone mode
		restart

else
	Device will attempt to connect to the saved WiFi SSID using the saved password
	while attempting to connect, if both buttons are pressed 
		clear WiFi credentials and standalone mode (which isn't actually set in this case, but just to be sure)
		restart
	When connection is made, start web server and go to Operation Mode


Operation Mode:

RotorHazard splash screen is displayed and the device waits for input

Pushing a button will increment/decrement the race # by 1 - minimum number is 1, maximum is 99, race # will wrap

Pushing both buttons at the same time will switch to PRACTICE mode
Pushing both buttons again will switch back to displaying the last race number

if a WiFi connection has been established, the device can also be controlled via the web interface at the assigned IP address (displayed for a few seconds when connection is successful)

Web interface has a settings page that can be accessed by clicking the tool icon in the upper right corner of the page
	The settings page allows the SSID and password to be cleared or changed to a new value
	Pushing either button will initiate a restart
	Use browser back button to return to the main page without making changes

Note the web server isn't all that responsive, give it a few seconds to update after clicking something, pay attention to the browser's loading indicator
Note that HEAT # can only be controlled via the web interface, it cannot be displayed using the buttons

The current race number is always displayed in large font on the lower part of the screen - minimum number is 1, maximum is 99, count will wrap

If HEAT is nonzero, the header line on the display will show "HEAT # NN" (where NN is the heat # set via the web interface)

If HEAT has not been set, or is reest to zero (via web interface), the display header line will show "RACE #" only (no heat number)

The + and - buttons increment/decrement their respective counts by 1
Numbers can be entered directly in the input box, clicking SET or hitting enter will commit the change and update the display accordingly
Race cannot be set to 0, Heat can
Maximum is 99 for both

The PRACTICE button toggles practice mode on and off - button turns green when practice mode is active
	When practice mode is active, the display header will show "PRACTICE" and the the letter "P" will be displayed below the header 

The RESET button will reset race # to 1, heat to 0, turn off practice mode, and update the display to show the RotorHazard splash screen

See RaceCounterMainPage.png and RaceCounterSettingsPage.png for web page images

Note: Uses SPIFFS to store the web background image - BG.PNG must be loaded to the root directory of the SPIFFS partition. If the image is not found, system will still work but the web page background will just be white.
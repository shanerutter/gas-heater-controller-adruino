#include <Arduino.h>
#include <../lib/ArduinoSchedule/src/schedule.h>

// Debug
const bool debug = false;

// Relay Outputs
const int relayGasPin = 7;
const int relayHeaterPin = 8;

// Inputs
const int inputManualGasSwitchPin = 6; // Switch pulling down to negative
const int inputHeaterColdModePin = 5; // Hooked into propex controller
const int inputEngineStatusPin = 4; // Hooked into ignition
const int inputRemoteStatusPin = 11; // Remote signal to turn on remotely

// Schedules
schedule schedule_2_seconds = {0, 2000};
schedule schedule_set_relays = {0, 50};
schedule schedule_remote = {0, (long)3600 * (long)1000}; // 1 hour run time

// Vars
bool relayGasStatus = false;
bool relayHeaterStatus = false;
bool statusManualGasSwitch = false;
bool statusHeaterColdMode = false;
bool statusEngine = false;
bool statusRemote = false;

// Remote Timer Vars
bool remoteTimerRunning = false;
bool remoteTimerIsBlocked = false;



void setup() 
{
    // Serial
    if (debug) {
        Serial.begin(9600);
    }

    // Outputs
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(relayGasPin, OUTPUT);
    pinMode(relayHeaterPin, OUTPUT); // Closest to power input

    // Inputs
    pinMode(inputManualGasSwitchPin, INPUT_PULLUP);
    pinMode(inputHeaterColdModePin, INPUT);
    pinMode(inputEngineStatusPin, INPUT);
    pinMode(inputRemoteStatusPin, INPUT);

    // Wait for a couple seconds
    delay(2000);
}

void schedule_2_second_run()
{
    // Debug
    if (debug) {
        Serial.println("-------------");
        Serial.println("Inputs");
        Serial.print("Manual Gas Switch: ");
        Serial.println(statusManualGasSwitch);
        Serial.print("Header Cold Mode: ");
        Serial.println(statusHeaterColdMode);
        Serial.print("Engine Status: ");
        Serial.println(statusEngine);
        Serial.print("Remote Status: ");  
        Serial.println(statusRemote);
        Serial.println("Outputs");
        Serial.print("Gas Relay: ");  
        Serial.println(relayGasStatus);
        Serial.print("Heater Relay: ");  
        Serial.println(relayHeaterStatus);
        Serial.println("Timer");
        Serial.print("Running: ");  
        Serial.println(remoteTimerRunning);
        Serial.print("Blocked: ");  
        Serial.println(remoteTimerIsBlocked);
    }
}

void setRelays()
{
    digitalWrite(LED_BUILTIN, relayGasStatus ? HIGH : LOW);
    digitalWrite(relayGasPin, relayGasStatus ? HIGH : LOW);
    digitalWrite(relayHeaterPin, relayHeaterStatus ? HIGH : LOW);
}

void remoteTimerReset(bool resetBlock = false)
{
    remoteTimerRunning = false;

    if (resetBlock) {
        remoteTimerIsBlocked = false;
    }
}

void remoteTimerBlock()
{
    remoteTimerIsBlocked = true;
    remoteTimerReset();
}

void remoteTimerStart()
{
    remoteTimerRunning = true;
    scheduleRun(&schedule_remote);
}

void logicRelays()
{
    // Default to off
    relayGasStatus = false;
    relayHeaterStatus = false;

    // Remote, do not trigger if heater is in cold mode
    if (statusRemote) {
        if (remoteTimerRunning) {
            if (scheduleCheck(&schedule_remote)) {
                remoteTimerBlock();
            }
        } else if(!remoteTimerIsBlocked) {
            remoteTimerStart();
        }

        if (remoteTimerRunning) {
            relayGasStatus = true;
            relayHeaterStatus = true;
        }
    }

    // Manual switch, turn gass on
    if (statusManualGasSwitch) {
        relayGasStatus = true;
    }

    // Do not allow heater relay to turn on if heater is in cold mode
    if (statusHeaterColdMode) {
        // If manual gas is not turned on, turn gas off as well
        if (!statusManualGasSwitch) {
            relayGasStatus = false;
        }
        
        relayHeaterStatus = false;
    }

    // Reset timer and block once the remote signal has turned off
    // Means we are ready for a new signal to kick off the timer process again
    if ((remoteTimerIsBlocked && !statusRemote) || (!statusRemote && remoteTimerRunning)) {
        remoteTimerReset(true);
    }

    // Engine running, turn off relays
    if (statusEngine) {
        relayGasStatus = false;
        relayHeaterStatus = false;
    }

    // Do not allow remote timer to operate whilst driving or heater is on hold mode
    // This stops causes where remote is triggered whilst driving and without this, 
    // the heater would engage as soon as the engine stopped (and still within the timer run period).
    if (statusRemote && (statusEngine || statusHeaterColdMode)) {
        remoteTimerBlock();
    }
}

// the loop function runs over and over again forever
void loop() 
{
    // digitalWrite(LED_BUILTIN, HIGH);
    // digitalWrite(relayGasPin, HIGH);
    // digitalWrite(relayHeaterPin, HIGH);


    // delay(500);

    // digitalWrite(LED_BUILTIN, LOW);
    // digitalWrite(relayGasPin, LOW);
    // digitalWrite(relayHeaterPin, LOW);

    // delay(500);

    // Get current input statuses
    statusManualGasSwitch = digitalRead(inputManualGasSwitchPin);
    statusHeaterColdMode = digitalRead(inputHeaterColdModePin);
    statusEngine = digitalRead(inputEngineStatusPin);
    statusRemote = digitalRead(inputRemoteStatusPin);

    // Determine actions
    logicRelays();

    // If heater cold enabled, we must ensure we set relays asap so that heater cannot be 
    // put into both hot and cold mode at the same time, otherwise the normal delayed relay 
    // setting is fine to stop spam relay operation.
    if (statusHeaterColdMode) {
        setRelays();
    }

    // Schedules - Set relays
    if (scheduleCheck(&schedule_set_relays)) {
        setRelays();
        scheduleRun(&schedule_set_relays);
    }

    // Schedules
    if (scheduleCheck(&schedule_2_seconds)) {
        schedule_2_second_run();
        scheduleRun(&schedule_2_seconds);
    }
}
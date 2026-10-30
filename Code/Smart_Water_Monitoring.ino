#define IR_USE_AVR_TIMER1 // Fixes internal Timer conflicts between tone() and IRremote

#include <LiquidCrystal.h>
#include <IRremote.hpp>

//---------------- PIN DEFINITIONS ----------------//
int trigPin = 4;
int echoPin = 3;
int irPin = 5;

// LED Pins
int greenLEDPin = 12;
int yellowLEDPin = 13;
int redLEDPin = A0;
int whiteLEDPin = A1; // Turns on when the motor/pump is rotating

int buzzerPin = 2;   
int pumpPin = A2;     

// Initializing LCD: RS, E, D4, D5, D6, D7 (Pins 6, 7, 8, 9, 10, 11)
LiquidCrystal lcd(6, 7, 8, 9, 10, 11);

float tankHeight = 100.0;   

//-------------- VARIABLES ----------------//
float distance;
float waterHeight;
int percentage;
int status; // 0 = LOW, 1 = NORMAL, 2 = FULL
int currentPage = 0;

//--- SMART CONTROLS & TIMERS ---//
bool buzzerMuted = false;   // Toggled by OK button
bool systemActive = true;   // Toggled by Power button
bool buzzerState = false;
unsigned long buzzerTimer = 0;
int sirenFrequency = 500;
bool sirenUp = true;
bool manualOverride = false;
bool manualPumpState = false;

void setup()
{
    pinMode(trigPin, OUTPUT);
    pinMode(echoPin, INPUT);

    // Initialize IR receiver on pin 5
    IrReceiver.begin(irPin, ENABLE_LED_FEEDBACK);
  
    pinMode(greenLEDPin, OUTPUT);
    pinMode(yellowLEDPin, OUTPUT);
    pinMode(redLEDPin, OUTPUT);
    pinMode(whiteLEDPin, OUTPUT);
    pinMode(buzzerPin, OUTPUT);
    
    // Initialize L293D Pump Pin on A2
    pinMode(pumpPin, OUTPUT);  
    digitalWrite(pumpPin, LOW); 

    lcd.begin(16,2);
    Serial.begin(9600);

    lcd.print("SMART WATER");
    lcd.setCursor(0,1);
    lcd.print("MONITOR PRO");

    delay(2000);
    lcd.clear();
}

void loop()
{
    handleIR();

    if (systemActive) 
    {
        // Normal Operation
        readSensor();
        calculateLevel();
        checkStatus();
        updateLEDs();
        handleBuzzer();
        handlePump(); 
        displayPage();
    } 
    else 
    {
        // Master Shutdown State (Power button pressed)
        shutDownSystem();
    }

    printSerial();
    delay(30); 
}

//------------------------------------------------
// READ SENSOR 
//------------------------------------------------
void readSensor()
{
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    long duration = pulseIn(echoPin, HIGH, 25000); 

    if(duration > 0) {
        distance = duration * 0.0343 / 2.0;
    }

    if(distance > tankHeight) distance = tankHeight;
    if(distance < 0)          distance = 0;
}

//------------------------------------------------
// CALCULATE LEVEL & PERSIST STATUS
//------------------------------------------------
void calculateLevel()
{
    waterHeight = tankHeight - distance;
    percentage = (waterHeight * 100) / tankHeight;

    if(percentage < 0)   percentage = 0;
    if(percentage > 100) percentage = 100;
}

void checkStatus()
{
    if(percentage < 20)      status = 0; // LOW
    else if(percentage <= 80) status = 1; // NORMAL
    else                      status = 2; // FULL
}

//------------------------------------------------
// IR REMOTE MANAGEMENT
//------------------------------------------------
void handleIR()
{
    if(IrReceiver.decode())
    {
        int command = IrReceiver.decodedIRData.command;

        Serial.print("IR Command Received: ");
        Serial.println(command);

        if(command == 0 || command == 21) 
        {
            systemActive = !systemActive;
            lcd.clear();
        }
        
        // Only allow other actions if the system isn't disabled
        if(systemActive)
        {
            // PAGE NAVIGATION (Buttons '1', '2', '3')
            if(command == 16) 
            {
                currentPage = 0;
                lcd.clear();
            }
            else if(command == 17) 
            {
                currentPage = 1;
                lcd.clear();
            }
            else if(command == 18) 
            {
                currentPage = 2;
                lcd.clear();
            }
            // ▼ FOR TURNING THE MOTOR OFF
          	else if(command == 8){
              	manualOverride = true;
    			manualPumpState = false;
          	}
          	// ▲ FOR TURNING THE MOTOR ON
          	else if(command == 10){
              	manualOverride = true;
    			manualPumpState = true;
          	}
            // ⏯ BUTTON FOR MUTE THE BUZZER
            else if(command == 5) 
            {
                buzzerMuted = !buzzerMuted;
                if(buzzerMuted) noTone(buzzerPin); // Instant silence kill-switch
            }
        }

        IrReceiver.resume();
    }
}

//------------------------------------------------
// AUTOMATED L293D MOTOR PUMP CONTROL WITH WHITE LED
//------------------------------------------------
void handlePump()
{
    if(manualOverride)
	{
    digitalWrite(pumpPin, manualPumpState);
    digitalWrite(whiteLEDPin, manualPumpState);
    return;
	}
    // Turn motor and white LED ON strictly when water level drops below 10%
    if (percentage < 10)
    {
        digitalWrite(pumpPin, HIGH); 
        digitalWrite(whiteLEDPin, HIGH);
    }
    // Keep running until water level safely clears 80%, then turn both OFF
    else if (percentage > 80)
    {
        digitalWrite(pumpPin, LOW); 
        digitalWrite(whiteLEDPin, LOW);
    }
    // Note: Between 10% and 80%, they maintain their previous state cleanly!
}

//------------------------------------------------
// OUTPUT ACTIONS: LCD UPDATES
//------------------------------------------------
void displayPage()
{
    switch(currentPage)
    {
        case 0:
            lcd.setCursor(0,0);
            lcd.print("Tank:");
            lcd.print(percentage);
            lcd.print("%   ");
            if(buzzerMuted) lcd.print("[M]"); // Indicates visually that buzzer is muted
            else            lcd.print("   ");

            lcd.setCursor(0,1);
            if(status == 0)      lcd.print("STATUS: LOW   ");
            else if(status == 1) lcd.print("STATUS: NORMAL");
            else                 lcd.print("STATUS: FULL  ");
            break;

        case 1:
            lcd.setCursor(0,0);
            lcd.print("Dist:");
            lcd.print(distance,1);
            lcd.print("cm   ");

            lcd.setCursor(0,1);
            lcd.print("Water:");
            lcd.print(waterHeight,1);
            lcd.print("cm   ");
            break;

        case 2:
            lcd.setCursor(0,0);
            lcd.print("Tank Max Limit");
            lcd.setCursor(0,1);
            lcd.print(tankHeight, 1);
            lcd.print(" cm   ");
            break;
    }
}

//------------------------------------------------
// LED INDICATIONS
//------------------------------------------------
void updateLEDs()
{
    // Clear all LEDs first to ensure a clean state
    digitalWrite(greenLEDPin, LOW);
    digitalWrite(yellowLEDPin, LOW);
    digitalWrite(redLEDPin, LOW);

    if (percentage < 20)
    {
        digitalWrite(redLEDPin, HIGH);
    }
    else if (percentage >= 20 && percentage <= 80)
    {
        digitalWrite(yellowLEDPin, HIGH);
    }
    else if (percentage > 80)
    {
        digitalWrite(greenLEDPin, HIGH);
    }
}

//------------------------------------------------
// BUZZER
//------------------------------------------------
void handleBuzzer()
{
    if(buzzerMuted)
    {
        noTone(buzzerPin);
        return;
    }

    if(percentage < 10)
    {
        if(millis() - buzzerTimer > 250)
        {
            buzzerTimer = millis();
            buzzerState = !buzzerState;

            if(buzzerState)  tone(buzzerPin, 500);
            else             noTone(buzzerPin);
        }
    }
    else if(percentage > 90)
    {
        if(millis() - buzzerTimer > 25)
        {
            buzzerTimer = millis();
            tone(buzzerPin, sirenFrequency);

            if(sirenUp)
            {
                sirenFrequency += 20;
                if(sirenFrequency >= 900) sirenUp = false;
            }
            else
            {
                sirenFrequency -= 20;
                if(sirenFrequency <= 500) sirenUp = true;
            }
        }
    }
    else
    {
        noTone(buzzerPin);
        buzzerState = false;
        sirenFrequency = 500;
    }
}

//------------------------------------------------
// SYSTEM MASTER SHUTDOWN ROUTINE
//------------------------------------------------
void shutDownSystem()
{
    // Turn off all actuators and indicators safely
    digitalWrite(pumpPin, LOW);
    digitalWrite(whiteLEDPin, LOW);
    digitalWrite(greenLEDPin, LOW);
    digitalWrite(yellowLEDPin, LOW);
    digitalWrite(redLEDPin, LOW);
    noTone(buzzerPin);

    // Display localized safety message
    lcd.setCursor(0,0);
    lcd.print("SYSTEM OFF     ");
    lcd.setCursor(0,1);
    lcd.print("PRESS POWER ON  ");
}

void printSerial()
{
    Serial.print("Sys Active: ");
    Serial.print(systemActive ? "YES" : "NO");
    Serial.print(" | Fill: ");
    Serial.print(percentage);
    Serial.print("% | Mute: ");
    Serial.print(buzzerMuted ? "ON" : "OFF");
    Serial.print(" | Pump: ");
    Serial.println(digitalRead(pumpPin) == HIGH ? "RUNNING" : "OFF");
}

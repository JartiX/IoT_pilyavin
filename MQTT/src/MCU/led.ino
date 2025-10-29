const int LED_PIN = LED_BUILTIN;
const char LED_ON = 'u';
const char LED_OFF = 'd';

char LED_STATE = 'd';

void setup()
{
    Serial.begin(9600);
}

void updateState()
{
    if (Serial.available() > 0)
    {
        LED_STATE = Serial.read();
        if (LED_STATE == LED_ON)
        {
            Serial.println("LED_GOES_ON");
        }
        else if (LED_STATE == LED_OFF)
        {
            Serial.println("LED_GOES_OFF");
        }
    }
}

void handleLED()
{
    if (LED_STATE == LED_ON)
    {
        digitalWrite(LED_PIN, HIGH);
    }
    else if (LED_STATE == LED_OFF)
    {
        digitalWrite(LED_PIN, LOW);
    }
}

void loop()
{
    updateState();
    handleLED();
}
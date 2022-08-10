#include <SoftwareSerial.h>

const byte MACHINETYPE = 1; // 1 = handing out, 0 = receiving cards

const byte rxPin = 8;
const byte txPin = 9;
const byte buttonPin = 2;
const byte invertedSignal = 1;
bool weShouldFeedCard = false;

// Set up a new SoftwareSerial object
SoftwareSerial mySerial (rxPin, txPin, invertedSignal);

// Reference lib: https://github.com/catzhou2002/K750Demo
int counter = 0;
int LEDPIN = 13;
int waitCounter = 0;

enum class States {READY, SENDING, WAITING};
States state;

enum class Commands {STATUS_AP, SEND_CARD_DC, WAIT_FOR_CARD_REMOVE};
Commands nextCommand;

bool cardOutputPreviouslyHadCard = true;

void triggerMachine() {
  weShouldFeedCard = true;
}

void setup()
{
  state = States::SENDING;
  nextCommand = Commands::STATUS_AP;
  delay(500);
  Serial.begin(9600);
  mySerial.begin(9600);
  pinMode(LEDPIN, OUTPUT);
  pinMode(txPin, OUTPUT);
  pinMode(rxPin, INPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(buttonPin), triggerMachine, FALLING);
  delay(1000);
}

void sendData(char cmdLetter1, char cmdLetter2)
{
  int data[20];
  int len = 9;
  data[0] = 0x02;//stx
  data[1] = 0x30; //addr H
  data[2] = 0x30; //addr L
  data[3] = 0x00; //len H
  data[4] = 2; //len L
  data[5] = (byte)cmdLetter1;
  data[6] = (byte)cmdLetter2;
  data[7] = 0x03; //etx

  data [8] =  data[0]; // check code
  for (int i = 1; i < len - 1; i++)
  {
    data[8] ^= byte(data[i]);
  }
  Serial.print("Sending:");
  for (int i = 0; i < len; i++)
  {
    Serial.print(" 0x");
    Serial.print(data[i], HEX);
    mySerial.write(data[i]);
  }
  Serial.println("");
  // Sending: 0x02 0x30 0x30 0x00 0x02 0x41 0x50 0x03 0x12
}

void soundTheAlarm()
{
  Serial.println("Card feeder error!");
  delay(500);
}

void sendACK()
{
  int data[20];
  int len = 3;
  data[0] = 0x05;
  data[1] = 0x30;
  data[2] = 0x30;

  Serial.print("ACK Received! Confirming: ");
  for (int i = 0; i < len; i++)
  {
    Serial.print(" 0x");
    Serial.print(data[i], HEX);
    mySerial.write(data[i]);
  }
  Serial.println("");
}

void loop()
{
  // Send requests every now and then
  if ( counter % 15000 == 0 )
  {
    if ( state == States::SENDING )
    {
      if ( nextCommand == Commands::SEND_CARD_DC )
      {
        sendData('D', 'C'); // 0x2 0x30 0x30 0x0 0x2 0x43 0x44 0x3 0x10
      }
      else if ( nextCommand == Commands::WAIT_FOR_CARD_REMOVE )
      {
        sendData('A', 'P'); // 0x2 0x30 0x30 0x0 0x2 0x41 0x50 0x3 0x12
      }
      else // Commands::STATUS_AP
      {
        sendData('A', 'P'); // 0x2 0x30 0x30 0x0 0x2 0x41 0x50 0x3 0x12
      }


      state = States::WAITING;
      waitCounter = 0;
      Serial.print("Waiting ");
    }
    else if ( state == States::WAITING )
    {
      waitCounter++;
      Serial.print(" ");
      if (waitCounter > 2 )
      {
        Serial.println("Timeout!");
        state = States::SENDING;
        nextCommand = Commands::STATUS_AP;
      }
      else
      {
        Serial.print(waitCounter);
      }
    }
  }

  // Always print received data
  if (mySerial.available() > 0) {
    digitalWrite(LEDPIN, HIGH); // turn on LED to show we got data
    Serial.print("Received: ");
    int data[20];
    int len = 0;
    while ( mySerial.available() > 0 )
    {
      char ch = mySerial.read();
      Serial.print(" 0x");
      Serial.print(ch, HEX);
      data[len] = ch;
      len++;
    }
    Serial.println("");

    if ( len == 3 && data[0] == 0x06 && data[1] == 0x30 && data[2] == 0x30 ) // ACK
    {
      sendACK();
    }
    else if ( len == 3 && data[0] == 0x15 && data[1] == 0x30 && data[2] == 0x30 ) // NAK
    {
      state = States::SENDING;
      Serial.println("NAK???");
    }
    else if ( len > 3 ) // Status bytes returned from the machine
    {
      // 0  1  2  3  4  5  6  7  8  9  0  1  2
      // 02 30 30 00 06 53 46 30 30 31 31 03 12
      // 02 30 30 00 06 53 46 30 30 30 31 3 13
      // 02 30 30 06 53 46 30 30 30 31 03 13

      // #7  For mange kort?  30 = ok, 31 = for mange
      // #8
      // #9  Kort i maskinen: 30 = nok kort, 31 = tom for kort, 38 = helt full
      // #10 Utmating:        30 = ingen kort, 31 = kort i utmater

      bool weHaveCards = (data[9] == 0x30) && (data[7] == 0x30);
      bool cardOutputCurrentlyHasCard = (data[10] != 0x30);

      Serial.println("");
      Serial.print("cardOutputCurrentlyHasCard: ");
      Serial.println(cardOutputCurrentlyHasCard);
      Serial.println("");
      if ( !cardOutputPreviouslyHadCard && cardOutputCurrentlyHasCard )
      {
        nextCommand = Commands::STATUS_AP;
        weShouldFeedCard = false;
        Serial.println("Card removed!");
      }
      else if ( weHaveCards )
      {
        if ( weShouldFeedCard )
        {
          nextCommand = Commands::SEND_CARD_DC;
          Serial.println("Send a Card!");
          weShouldFeedCard = false;
          delay(200); // DEBOUNCE
        }
      }
      else if ( cardOutputCurrentlyHasCard )
      {
        nextCommand = Commands::STATUS_AP;
      }
      else
      {
        soundTheAlarm();
      }
      state = States::SENDING;
      cardOutputCurrentlyHasCard = cardOutputCurrentlyHasCard; // Keep status for next loop
    }
    else
    {
      // enum class Commands {STATUS_AP, SEND_CARD_DC, WAIT_FOR_CARD_REMOVE};
      nextCommand = Commands::STATUS_AP;
      Serial.println("");
      state = States::SENDING;
    }
  } else {
    digitalWrite(LEDPIN, LOW);
  }
  counter++;
}

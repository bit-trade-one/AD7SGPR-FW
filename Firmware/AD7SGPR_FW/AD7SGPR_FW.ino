// bto_ODA_230628V2.5
#include <TM1638plus_Model2.h>
#include <EEPROM.h>

const int STROBE_TM = 10;
const int CLOCK_TM = 9;
const int DIO_TM = 8;
const bool swap_nibbles = false;
const bool high_freq = true; // If using a high freq CPU > ~100 MHZ set to true.

// ピンを定義
const int BUTTON_A = 20;
const int BUTTON_B = 19;
const int BUTTON_C = 18;
const int BUTTON_D = 17;

const int ad7sgprLED = 25; // 裏面のLED

// displayAnimation用の変数
int animationType = 0;

// アニメーションフラグ
bool animationFlag = false;

// TM1638モジュールのインスタンス化
TM1638plus_Model2 tm(STROBE_TM, CLOCK_TM, DIO_TM, swap_nibbles, high_freq);

// 輝度設定変数
const uint8_t InitialBrightness = 4; // 初期輝度設定
uint8_t brightness;                  // 0-7の範囲で設定。消灯はなし

// 改行文字の設定
const char END_OF_LINE_CHARACTER[][3] = {{"\r\n"}, {"\r"}, {"\n"}};
const int eol_index = 0; // 改行文字設定 0: "\r\n", 1: "\r", 2: "\n"

// 7セグメントディスプレイの点灯モードフラグ
bool displayOn = true;

// EEPROMに保存する輝度設定データのアドレス
const byte brightnessAddress = 0;

// EEPROMから読み取った輝度設定を格納する変数
uint8_t storedBrightness;

// ボタンの状態を格納する変数
volatile uint16_t statusButtonA = 0;
volatile uint16_t statusButtonB = 0;
volatile uint16_t statusButtonC = 0;
volatile uint16_t statusButtonD = 0;

// 割り込みハンドラの定義
void onButtonA()
{
  statusButtonA = 1;
}

void onButtonB()
{
  statusButtonB = 1;
}

void onButtonC()
{
  statusButtonC = 1;
}

void onButtonD()
{
  statusButtonD = 1;
}

void setup()
{
  tm.displayBegin();
  Serial.begin(115200);

  // while (!Serial);   // シリアルが開くのを待つ
  EEPROM.begin(256);
  storedBrightness = EEPROM.read(brightnessAddress);

  pinMode(ad7sgprLED, OUTPUT);
  analogWrite(ad7sgprLED, 128);

  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);
  pinMode(BUTTON_D, INPUT_PULLUP);

  // EEPROMから輝度設定を読み出す
  if (0 <= storedBrightness && storedBrightness <= 7)
  {
    tm.brightness(storedBrightness);
    brightness = storedBrightness;
  }
  else
  {
    EEPROM.write(brightnessAddress, InitialBrightness); // EEPROMに初期値を書き込む
    brightness = InitialBrightness;
    EEPROM.commit(); // 書き込んだデータをFlashに保存する
    tm.brightness(brightness);
  }

  if (digitalRead(BUTTON_A) == LOW && digitalRead(BUTTON_B) == LOW && digitalRead(BUTTON_C) == LOW && digitalRead(BUTTON_D) == LOW)
  {                    // ABCD同時押し起動でバージョン確認
    versionLighting(); // 起動時の点灯シーケンス
  }

  Serial.println("start");
  Serial.print("brightness:");
  Serial.println(brightness);
  // 割り込みハンドラ設定
  attachInterrupt(BUTTON_A, onButtonA, RISING);
  attachInterrupt(BUTTON_B, onButtonB, RISING);
  attachInterrupt(BUTTON_C, onButtonC, RISING);
  attachInterrupt(BUTTON_D, onButtonD, RISING);
}

void loop()
{
  readSerialDataPrint();          // 読み出しシリアルデータ出力
  brightnessSetAPress();          // ボタンAディスプレイの輝度設定
  printModeDisplayButtonBPress(); // ボタンBディスプレイの点灯モード切り替え
  ButtonCPress();                 // ボタンC押下時の処理
  ButtonDPress();                 // ボタンD押下時の処理

  delay(1);
}

bool isEndOfLine(char ch)
{ // 改行文字
  char eol[] = {END_OF_LINE_CHARACTER[eol_index][0], END_OF_LINE_CHARACTER[eol_index][1], '\0'};
  return (ch == eol[0]) || (ch == eol[1]);
}

void readSerialDataPrint()
{
  // 読み出しシリアルデータ
  static String buffer = "";
  char ch;
  // シリアルバッファにデータがある場合
  while (Serial.available() > 0)
  {
    ch = Serial.read();
    if (isEndOfLine(ch))
    {
      if (buffer.length() > 0)
      {
        String rxStr = buffer;
        buffer = "";
        // 各種処理関数呼び出し
        if (rxStr == "@CLR")
        {
          displayClearCommand();
        }
        else if (rxStr.startsWith("@ANI"))
        {
          displayAnimationCommand(rxStr.substring(4));
        }
        else if (rxStr.startsWith("@7SG"))
        {
          printModeDisplayCommand(rxStr.substring(4));
        }
        else if (rxStr.startsWith("@BRI"))
        {
          brightnessSetCommand(rxStr.substring(4));
        }
        else if (rxStr.startsWith("@BRR"))
        {
          brightnessReturnCommand();
        }
        else if (rxStr.startsWith("@HEX"))
        {
          if (animationFlag == true && displayOn == true)
          {
            displayAnimation();
          }
          displayHexPattern(rxStr.substring(4));
        }
        else
        {
          if (animationFlag == true && displayOn == true)
          {
            displayAnimation();
          }
          displayReceivedData(rxStr);
        }
      }
    }
    else
    {
      buffer += ch;
      if (buffer.length() > 20)
      {
        Serial.print("rxData: ");
        Serial.println(buffer);
        Serial.println("E0:Over receive length"); // エラー：受信文字が20文字未満ではない
        buffer = "";
        while (Serial.available() > 0)
        {
          ch = Serial.read();
          if (isEndOfLine(ch))
          {
            break;
          }
        }
        break;
      }
    }
  }
}

void displayReceivedData(const String &data)
{
  // 受信データ表示
  Serial.print("rxData:");
  Serial.println(data);

  // 表示する文字列とドットの位置を格納する変数を初期化
  char displayChars[9] = "        ";
  uint16_t dotPositions = 0;
  int segmentCount = 0;
  int displayIndex = 7;

  // ドットが含まれているかどうかチェック
  if (data.indexOf('.') != -1)
  { // ドットがある場合の処理
    bool dotFound = false;
    for (int i = data.length() - 1; i >= 0; i--)
    {
      char c = data[i]; // Stringの文字を直接取得

      if (c == '.')
      {
        if (dotFound)
        {
          Serial.println("E1: Multiple dots"); // ドットが複数個合った場合はエラーを返す
          return;
        }
        dotFound = true;
        dotPositions |= (1 << (7 - displayIndex));
      }
      else
      {
        // 文字がドットでなければ、その文字を表示文字列に追加
        displayChars[displayIndex--] = c;
      }
    }
  }
  else
  { // ドットがない場合の処理
    for (int i = data.length() - 1; i >= 0; i--)
    {
      displayChars[displayIndex--] = data[i]; // Stringの文字を直接取得
    }
  }

  // 文字列を表示し、ドットの位置も指定
  if (displayOn)
  {
    tm.DisplayStr(displayChars, dotPositions);
  }
  else
  {
    Serial.println(">>>>> During display off mode <<<<<"); // 消灯モード中
  }
}

void displayClearCommand()
{
  // 受信データが "@CLR" の場合、画面クリア処理を実行
  tm.reset();
  Serial.println("rxData:@CLR Display clear");
}

void displayHexPattern(const String &hexData)
{
  // HEX固定長の表示
  // シリアルモニタへのログ出力
  Serial.print("rxData:@HEX");
  Serial.println(hexData);

  if (hexData.length() != 16)
  {
    Serial.println("E3: Need 16 hex digits"); // エラー：16桁の16進数が必要
    return;
  }

  byte byteData[8];
  for (int i = 0; i < 8; i++)
  {
    char buf[3] = {hexData[i * 2], hexData[i * 2 + 1], 0}; // Stringの文字を直接取得
    byteData[i] = strtol(buf, NULL, 16);
  }
  if (displayOn)
  { // 7セグ消灯フラグをチェック
    tm.ASCIItoSegment(byteData);
  }
  else
  {
    Serial.println(">>>>> During display off mode <<<<<"); // 消灯モード中
  }
}

void brightnessSetAPress()
{ // ディスプレイの輝度設定
  // ボタンA（ピン20）が押された場合
  if (statusButtonA == 1)
  { // 割込みハンドラ内のフラグを参照に変更
    brightness++;
    if (brightness > 7)
    {
      brightness = 0;
    }
    EEPROM.write(brightnessAddress, brightness);
    EEPROM.commit();
    tm.brightness(brightness);
    Serial.println("pushA");
    Serial.print("brightness:");
    Serial.println(brightness);

    statusButtonA = 0; // フラグクリア
  }
}

void printModeDisplayButtonBPress()
{
  // ボタンB（ピン19）が押された場合の処理
  if (statusButtonB == 1)
  { // 割込みハンドラ内のフラグを参照に変更
    Serial.println("pushB");
    if (displayOn == false)
    {
      Serial.println("7SGon");
      analogWrite(ad7sgprLED, 128); // 裏面LED点灯
      displayOn = true;
    }
    else
    {
      Serial.println("7SGoff");
      tm.reset();                 // 7セグメントディスプレイを消去
      analogWrite(ad7sgprLED, 8); // 裏面LED点灯暗め
      displayOn = false;
    }

    statusButtonB = 0; // フラグクリア
  }
}

void printModeDisplayCommand(const String &printModeDisplayData)
{
  int command = printModeDisplayData.toInt();

  switch (command)
  {

  case 0:
    Serial.println("rxData:@7SG0");
    Serial.println("7sgOff");
    tm.reset();
    analogWrite(ad7sgprLED, 8); // 裏面LED点灯暗め
    displayOn = false;          // 7セグメントディスプレイ点灯フラグを解除
    break;

  case 1:
    Serial.println("rxData:@7SG1");
    Serial.println("7sgOn");
    analogWrite(ad7sgprLED, 128); // 裏面LED点灯
    displayOn = true;             // 7セグメントディスプレイ点灯フラグを立てる
    break;

  default:
    Serial.println("rxData:@7SGx");
    Serial.println("7sgOn");
    analogWrite(ad7sgprLED, 128); // 裏面LED点灯
    displayOn = true;             // 7セグメントディスプレイ点灯フラグを立てる
    break;
  }
}

void brightnessSetCommand(const String &brightnessData)
{
  uint8_t brightnessInt = brightnessData.toInt();
  if (brightnessInt < 0 || brightnessInt > 7)
  {
    Serial.println("rxData:@BRIx");
    Serial.println("E4: Brightness out of range"); // エラー輝度が範囲外 0-7の範囲ではない
    return;
  }
  brightness = brightnessInt;
  EEPROM.write(brightnessAddress, brightness);
  EEPROM.commit();
  tm.brightness(brightnessInt);
  Serial.print("rxData:@BRI");
  Serial.println(brightnessInt);
  Serial.print("brightness:");
  Serial.println(brightnessInt);
}

void brightnessReturnCommand()
{ // 輝度を返す
  Serial.println("rxData:@BRR");
  Serial.print("brightness:");
  Serial.println(brightness);
}

void ButtonCPress()
{
  // 割込みハンドラ内のフラグを参照に変更
  if (statusButtonC == 1)
  {

    Serial.println("pushC");
    statusButtonC = 0; // フラグクリア
  }
}

void ButtonDPress()
{
  // 割込みハンドラ内のフラグを参照に変更
  if (statusButtonD == 1)
  {
    Serial.println("pushD");
    statusButtonD = 0; // チャタリング収束後、フラグクリア
  }
}

void displayAnimationCommand(const String &animationData)
{
  int command = animationData.toInt();
  switch (command)
  {
  case 0:
    animationFlag = false;
    animationType = 0;
    Serial.println("rxData:@ANI0");
    Serial.println("No animation");
    break;
  case 1:
    animationFlag = true;
    animationType = 1;
    Serial.println("rxData:@ANI1");
    Serial.println("Animation1");
    break;
  case 2:
    animationFlag = true;
    animationType = 2;
    Serial.println("rxData:@ANI2");
    Serial.println("Animation2");
    break;
  default:
    animationFlag = false;
    animationType = 0;
    Serial.println("rxData:@ANIx");
    Serial.println("No animation");
    break;
  }
}

void displayAnimation()
{
  int displayAnimationDelay = 50;
  int displayAnimationloop = 33;
  int currentIndex = 0;
  if (animationType == 1)
  {
    for (int i = 0; i < displayAnimationloop; i++)
    {
      String displayStr;
      switch (currentIndex)
      {
      case 0:
        displayStr = "00000000";
        break;
      case 1:
        displayStr = "11111111";
        break;
      case 2:
        displayStr = "22222222";
        break;
      case 3:
        displayStr = "33333333";
        break;
      case 4:
        displayStr = "44444444";
        break;
      case 5:
        displayStr = "55555555";
        break;
      case 6:
        displayStr = "66666666";
        break;
      case 7:
        displayStr = "77777777";
        break;
      case 8:
        displayStr = "88888888";
        break;
      case 9:
        displayStr = "99999999";
        break;
      }
      tm.DisplayStr(displayStr.c_str(), 0);
      currentIndex++;
      if (currentIndex > 9)
      {
        currentIndex = 0;
      }
      delay(displayAnimationDelay);
    }
  }
  else if (animationType == 2)
  {
    for (int i = 0; i < displayAnimationloop; i++)
    {
      switch (currentIndex)
      {
      case 0:
        tm.reset();
        tm.DisplaySegments(0x00, 0xFF);
        break;
      case 1:
        tm.reset();
        tm.DisplaySegments(0x01, 0xFF);
        break;
      case 2:
        tm.reset();
        tm.DisplaySegments(0x02, 0xFF);
        break;
      case 3:
        tm.reset();
        tm.DisplaySegments(0x03, 0xFF);
        break;
      case 4:
        tm.reset();
        tm.DisplaySegments(0x04, 0xFF);
        break;
      case 5:
        tm.reset();
        tm.DisplaySegments(0x05, 0xFF);
        break;
      }
      currentIndex++;
      if (currentIndex > 5)
      {
        currentIndex = 0;
      }
      delay(displayAnimationDelay);
    }
  }
}

void versionLighting()
{
  // 起動時のバージョン確認シーケンス
  tm.DisplaySegments(0, 0xFF);
  tm.DisplaySegments(1, 0xFF);
  tm.DisplaySegments(2, 0xFF);
  tm.DisplaySegments(3, 0xFF);
  tm.DisplaySegments(4, 0xFF);
  tm.DisplaySegments(5, 0xFF);
  tm.DisplaySegments(6, 0xFF);
  tm.DisplaySegments(7, 0xFF);
  for (int i = 0; i < 8; i++)
  {
    tm.brightness(i);
    delay(250);
  }
  delay(1000);
  String setupRxStr = "AD7SGPR";
  tm.DisplayStr(setupRxStr.c_str(), 0);
  delay(1000);
  tm.reset();
  setupRxStr = "V025_bto";
  tm.DisplayStr(setupRxStr.c_str(), 0x20);
  delay(1000);
  tm.reset();
}
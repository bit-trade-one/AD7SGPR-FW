// bto_ODA_230725V1.1
#include <TM1638plus_Model2.h>

const int STROBE_TM = 10;
const int CLOCK_TM = 9;
const int DIO_TM = 8;
const bool swap_nibbles = false;
const bool high_freq = true; // 100 MHzを超える高周波数CPU を使用している場合は、true に設定。

const int BUTTON_A = 20;// ピンを定義
const int BUTTON_B = 19;
const int BUTTON_C = 18;
const int BUTTON_D = 17;

bool lastState_A = true; // 立ち上がり検出用の前回値保持
bool lastState_B = true;
bool lastState_C = true;
bool lastState_D = true;

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

//  最後に表示したものを保持
String lastrxStr;

// 7セグメントディスプレイの点灯モードフラグ
bool displayOn = true;

void setup()
{
  tm.displayBegin();
  Serial.begin(115200);

  pinMode(ad7sgprLED, OUTPUT);
  analogWrite(ad7sgprLED, 128);

  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);
  pinMode(BUTTON_D, INPUT_PULLUP);

  brightness = InitialBrightness;
  tm.brightness(brightness);

  if (digitalRead(BUTTON_C) == LOW && digitalRead(BUTTON_D) == LOW)
  {                    // CD同時押し起動でバージョン確認
    versionLighting(); // バージョン確認シーケンス
  }
}

void loop()
{
  readSerialDataPrint();          // 読み出し・7セグ出力
  brightnessSetAPress();          // ボタンA
  printModeDisplayButtonBPress(); // ボタンB
  ButtonCPress();                 // ボタンC
  ButtonDPress();                 // ボタンD

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
        else if (rxStr.startsWith("@SGR"))
        {
          displayOnStatusCommand();
        }
        else if (rxStr.startsWith("@HEX"))
        {
          if (animationFlag == true && displayOn == true)
          {
            displayAnimation();
          }
          displayHexPattern(rxStr.substring(4));
          lastrxStr = rxStr;
        }
        else
        {
          if (animationFlag == true && displayOn == true)
          {
            displayAnimation();
          }
          displayReceivedData(rxStr);
          lastrxStr = rxStr;
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

//=============================================
//  受信文字列の表示
//=============================================
// MAX_CHARSは実際の最大文字数より1つ大きい値（9）に設定
// dispCharsおよびdispDotsの各配列にも余分な要素を1つ追加
// displayStar関数にはdispCharsの最初の要素を飛ばして渡す

const int MAX_CHARS = 9;
static void displayStar(char *displayChars, int16_t dotPositons) {
  if (displayOn) {
      tm.DisplayStr(displayChars, dotPositons);
  } else {
      Serial.println(">>>>> During display off mode <<<<<");
  }
}
void displayReceivedData(const String &data)
{
  char    dispChars[MAX_CHARS + 1] = "         ";
  char    dispDots [MAX_CHARS + 1] = ".        ";
  int16_t dotPositons = 0;
  int16_t dispIdx     = 0;
  Serial.print("rxData:");
  Serial.println(data);
  //  受信文字の解析
  for (int i = 0; i < data.length(); i++) {
    char c = data[i];
    if (c == '.') {                       // '.'の処理
      if (dispDots[dispIdx] == '.') {     //  すでに'.'を格納済み
        if(++dispIdx < MAX_CHARS) {
          dispChars[dispIdx] = ' ';       //    文字に' 'を格納
          dispDots[dispIdx]  = '.';       //    次の桁に'.'を格納
        } else {
          break;
        }
      } else {                            //  あらたに'.'を格納
        dispDots[dispIdx] = '.';
      }
    } else {                              // '.'以外の処理
      if(++dispIdx < MAX_CHARS) {
        dispChars[dispIdx] = c;
      }
    }
  }
  if(dispIdx < MAX_CHARS) {
    // ドットの変換
    for (int i = 1; i < MAX_CHARS; i++) {
      if (dispDots[i] == '.') {
        dotPositons |= (1 << (MAX_CHARS - i - 1));
      }
    }
    displayStar(&dispChars[1], dotPositons);
  } else {                                // オーバーフロー
    Serial.println("E0:Over receive length"); // エラー：表示文字が8文字を超える
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
  bool currentState = digitalRead(BUTTON_A);
  // ボタンA（ピン20）が押された場合
  if (lastState_A == false && currentState == HIGH) // 立ち上がり検出
  {
    brightness++;
    if (brightness > 7)
    {
      brightness = 0;
    }
    tm.brightness(brightness);
    Serial.println("pushA");
    Serial.print("brightness:");
    Serial.println(brightness);
    delay(10);
  }
  lastState_A = currentState;
}

void printModeDisplayButtonBPress()
{
  bool currentState = digitalRead(BUTTON_B);
  // ボタンB（ピン19）が押された場合の処理
  if (lastState_B == false && currentState == HIGH)
  {
    Serial.println("pushB");
    if (displayOn == false)
    {
      Serial.println("7SGon");
      analogWrite(ad7sgprLED, 128); // 裏面LED点灯

      displayOn = true;
      if (lastrxStr.startsWith("@HEX"))
      {
        displayHexPattern(lastrxStr.substring(4));
      }
      else
      {
        displayReceivedData(lastrxStr);
      }
    }
    else
    {
      Serial.println("7SGoff");
      tm.reset();                 // 7セグメントディスプレイを消去
      analogWrite(ad7sgprLED, 8); // 裏面LED点灯暗め
      displayOn = false;
    }
    delay(10);
  }
  lastState_B = currentState;
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
    if (lastrxStr.startsWith("@HEX"))
    {
      displayHexPattern(lastrxStr.substring(4));
    }
    else
    {
      displayReceivedData(lastrxStr);
    }

    break;

  default:

    Serial.println("rxData:@7SGx");
    Serial.println("7sgOn");
    analogWrite(ad7sgprLED, 128); // 裏面LED点灯
    displayOn = true;             // 7セグメントディスプレイ点灯フラグを立てる
    if (lastrxStr.startsWith("@HEX"))
    {
      displayHexPattern(lastrxStr.substring(4));
    }
    else
    {
      displayReceivedData(lastrxStr);
    }

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

void displayOnStatusCommand()
{
  Serial.println("rxData:@SGR");
  if (displayOn)
  {
    Serial.println("7segmentsdisplay is ON mode");
  }
  else
  {
    Serial.println("7segmentsdisplay is OFF mode");
  }
}

void ButtonCPress()
{
  bool currentState = digitalRead(BUTTON_C);
  if (lastState_C == false && currentState == HIGH)
  {
    Serial.println("pushC");
    delay(10);
  }
  lastState_C = currentState;
}

void ButtonDPress()
{
  bool currentState = digitalRead(BUTTON_D);
  if (lastState_D == false && currentState == HIGH)
  {
    Serial.println("pushD");
    delay(10);
  }
  lastState_D = currentState;
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
  tm.reset();
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
  setupRxStr = "V011_bto";
  tm.DisplayStr(setupRxStr.c_str(), 0x20);
  delay(1000);
  tm.reset();
}

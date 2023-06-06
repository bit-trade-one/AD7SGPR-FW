//bto_ODA_230606V0.91
//発売時のVendorID:0x22EA
//発売時のProductID:-

//#include <TM1638plus.h>//カソードコモン
#include <TM1638plus_Model2.h>//アノードコモン
#include <EEPROM.h>

// GPIO I/O pins on the Arduino connected to strobe, clock, data,
//pick on any I/O you want.
const int STROBE_TM = 10;  // strobe = GPIO connected to strobe line of module
const int CLOCK_TM = 9;    // clock = GPIO connected to clock line of module
const int DIO_TM = 8;      // data = GPIO connected to data line of module
bool swap_nibbles = false; //Default is false if left out, see note in readme at URL
bool high_freq = true;     //default false,, If using a high freq CPU > ~100 MHZ set to true.

// ボタン用のピンを定義
const int BUTTON_A = 20;
const int BUTTON_B = 19;
const int BUTTON_C = 18;
const int BUTTON_D = 17;

// EEPROMアドレス
const int EEPROM_BRIGHTNESS_ADDRESS = 0;
const int EEPROM_DISPLAY_ADDRESS = 1;


//Constructor object (GPIO STB , GPIO CLOCK , GPIO DIO, use high freq MCU)
TM1638plus_Model2 tm(STROBE_TM, CLOCK_TM, DIO_TM, swap_nibbles, high_freq);

// 輝度設定変数
uint8_t brightness = 5; // 初期値は5 0-7で消灯は無い

const char END_OF_LINE_CHARACTER[][3] = { { "\r\n" }, { "\r" }, { "\n" } };
int eol_index = 0;  // 改行文字設定 0 :  \r\n 1 : \r  2 :  \n 
String rxStr = "";  // 受信した文字列を格納するための変数を宣言します。

bool displayCleared = false;  // ディスプレイが消去されたかどうかを示すフラグを追加

bool Bstate = false;
bool BlastState = false;


void setup() {
  //Serialinit();
  tm.displayBegin();
  
  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);
  pinMode(BUTTON_D, INPUT_PULLUP);

  // Serial通信の初期化
  Serial.begin(115200);
  delay(1000);
  pinMode(25, OUTPUT);
  digitalWrite(25, HIGH); // 裏面LED点灯

  startLighting();  // 起動時の点灯処理
  tm.brightness(brightness);
  Serial.println("start");
  Serial.print("brightness: ");
  Serial.println(brightness);
}

void loop() {
  readSerialData();// 読み出しシリアルデータ
  brightnessSet();// ディスプレイの輝度設定
  //changeEOLIndexOnButtonDPress();

  delay(100);
}

bool isEndOfLine(char ch) {  //改行文字
  char eol[] = { END_OF_LINE_CHARACTER[eol_index][0], END_OF_LINE_CHARACTER[eol_index][1], '\0' };
  return (ch == eol[0]) || (ch == eol[1]);
}

void readSerialData() {
  // 読み出しシリアルデータ
  static String buffer = "";

  // シリアルバッファにデータがある場合
  while (Serial.available() > 0) {
    char ch = Serial.read();
    if (isEndOfLine(ch)) {
      if (buffer.length() > 0) {
        String rxStr = buffer;
        buffer = "";

        // 各種処理関数呼び出し
        if (rxStr == "?CLR") {
          handleReceivedData(rxStr);
        } else if (rxStr.startsWith("?HEX")) {
          displayHexPattern(rxStr.substring(4));  // 受信データを16進数として表示
        } else if (rxStr.startsWith("?SGA")) {
          processSegmentControlCommand(rxStr.substring(4));
        } else {
          displayReceivedData(rxStr);  // 受信データを表示
        }
      }
    } else {
      buffer += ch;
    }
  }
}

void displayReceivedData(const String& data) {
  // 受信データを表示
  Serial.print("受信データ: ");
  Serial.println(data);

  // 文字列を後方切り捨てに整形
  String formattedData = data;
  int maxDisplayLength = data.indexOf('.') != -1 ? 9 : 8;
  if (formattedData.length() > maxDisplayLength) {
    formattedData = formattedData.substring(formattedData.length() - maxDisplayLength);
  }

  // ピリオドの数をカウント
  int periodCount = 0;
  for (int i = 0; i < formattedData.length(); i++) {
    if (formattedData.charAt(i) == '.') {
      periodCount++;
    }
  }

  // ピリオドが2つ以上の場合、7セグメントディスプレイに何も表示しない
  if (periodCount >= 2) {
    Serial.println("エラー: ピリオドが2つ以上含まれています");
    return;
  }

  // 文字列を右詰めに整形
  int spacesToAdd = 8 - (formattedData.length() - periodCount);
  String rightAlignedString = "";
  for (int i = 0; i < spacesToAdd; i++) {
    rightAlignedString += " ";
  }
  rightAlignedString += formattedData;

  // 前回の文字をクリア
  tm.reset();

  // 右詰めの文字列を表示
  tm.DisplayStr(rightAlignedString.c_str());
   displayCleared = false;
}


void handleReceivedData(const String& data) {
  // 受信データが "?CLR" の場合、画面クリア処理を実行
  tm.reset();
  displayCleared = true;  // ディスプレイが消去されたことを示すフラグを設定
  // 表示状態を保存する配列をリセット
  //memset(displayValues, 0, sizeof(displayValues));
  Serial.println("画面クリアコマンドが受信されました。7セグメントディスプレイを消去しました。");
}

bool processSegmentControlCommand(const String& command) {
  //セグメント個別制御
  // 受信データをシリアルモニタに表示
  Serial.print("受信データ: ?SGA");
  Serial.println(command);
  if (command.length() < 2) {
    return false;
  }

  uint8_t value = 0;
  int position = -1;

  for (int i = 0; i < command.length(); i++) {
    char ch = toupper(command.charAt(i));

    if (ch >= '1' && ch <= '8') {  // アスキー文字が数字の場合
      if (position != -1) {        // 前の桁がある場合、その桁にセグメント値を表示
        tm.DisplaySegments(position, value);
      }
      position = (8 - (ch - '0'));  // 桁の値を反転させる（1->7, 2->6, 3->5, ..., 8->0）
      value = 0;                    // セグメント値をリセット
    } else {
      switch (ch) {
        case 'A':
          value |= 0b00000001;
          break;
        case 'B':
          value |= 0b00000010;
          break;
        case 'C':
          value |= 0b00000100;
          break;
        case 'D':
          value |= 0b00001000;
          break;
        case 'E':
          value |= 0b00010000;
          break;
        case 'F':
          value |= 0b00100000;
          break;
        case 'G':
          value |= 0b01000000;
          break;
        case 'P':
          value |= 0b10000000;
          break;
        default:
          return false;
      }
    }
  }

  if (position != -1) {
    tm.DisplaySegments(position, value);
  }

  return true;
}

void displayHexPattern(const String &hexData) {
  // 16進数パターン表示?HEX
  
  if (hexData.length() != 16) {
    Serial.println("エラー: 16桁の16進数データが必要です");
    return;
  }
  
  for (int i = 0; i < 8; i++) {
    String hexPair = hexData.substring(i * 2, i * 2 + 2);
    uint8_t hexValue = (uint8_t)strtol(hexPair.c_str(), NULL, 16);
    tm.DisplaySegments(i, hexValue);
  }

  // 正常に出力された際のシリアルモニタへのログ出力
  Serial.print("受信データ: ?HEX");
  Serial.println(hexData);
}



void brightnessSet() {  // ディスプレイの輝度設定
                        // ボタンA（ピン20）が押された場合
  if (digitalRead(BUTTON_A) == LOW) {
    brightness++;
    if (brightness > 7) {
      brightness = 0;
    }
    tm.brightness(brightness);

    Serial.print("ボタンAが押されました。輝度: ");
    Serial.println(brightness);

    delay(200); // ボタン入力のデバウンス
  }

  // ボタンB（ピン19）が押された場合
  if (digitalRead(BUTTON_B) == LOW) {
    if (displayCleared) {
      Serial.println("復帰");
      tm.DisplaySegments(0, 0xFF);
      tm.DisplaySegments(1, 0xFF);
      tm.DisplaySegments(2, 0xFF);
      tm.DisplaySegments(3, 0xFF);
      tm.DisplaySegments(4, 0xFF);
      tm.DisplaySegments(5, 0xFF);
      tm.DisplaySegments(6, 0xFF);
      tm.DisplaySegments(7, 0xFF);
      displayCleared = false;
    } else {
      tm.reset();  // 7セグメントディスプレイを消去
      displayCleared = true;
      Serial.println("ボタンBが押されました。7セグメントディスプレイを消去しました。");
    }
    delay(200);  // ボタン入力のデバウンス
  }
}


/*
 tm.DisplaySegments(0, 0xFF);
      tm.DisplaySegments(1, 0xFF);
      tm.DisplaySegments(2, 0xFF);
      tm.DisplaySegments(3, 0xFF);
      tm.DisplaySegments(4, 0xFF);
      tm.DisplaySegments(5, 0xFF);
      tm.DisplaySegments(6, 0xFF);
      tm.DisplaySegments(7, 0xFF);
      */

/*バグが懸念されるため 無効化
void changeEOLIndexOnButtonDPress() {
  //ボタンDを押すたびに改行文字を変える
  if (digitalRead(BUTTON_D) == LOW) {
    eol_index++;
    if (eol_index > 2) {
      eol_index = 0;
    }

    String eolString;
    switch (eol_index) {
      case 0:
        eolString = "\\r\\n (CRLF)";
        break;
      case 1:
        eolString = "\\r (CR)";
        break;
      case 2:
        eolString = "\\n (LF)";
        break;
    }

    Serial.print("ボタンDが押されました。改行文字が変更されました: ");
    Serial.println(eolString);
    delay(200);  // ボタン入力のデバウンス
  }
} */

void startLighting() {
  rxStr = "AD7SGPR";
  displayReceivedData(rxStr);
  delay(2500);
  handleReceivedData(rxStr);
  // 7セグメントディスプレイに文字列を表示
  tm.DisplaySegments(0, 0xFF);
  tm.DisplaySegments(1, 0xFF);
  tm.DisplaySegments(2, 0xFF);
  tm.DisplaySegments(3, 0xFF);
  tm.DisplaySegments(4, 0xFF);
  tm.DisplaySegments(5, 0xFF);
  tm.DisplaySegments(6, 0xFF);
  tm.DisplaySegments(7, 0xFF);
  for(int i = 0; i < 8; i++){
    tm.brightness(i);
    delay(100);
  }
  delay(1000);
  handleReceivedData(rxStr);
  rxStr = "V001_bto";
  displayReceivedData(rxStr);
  delay(1000);
  handleReceivedData(rxStr);
}

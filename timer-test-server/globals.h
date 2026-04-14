// widgets

String serialAlphaNumerics = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
String iirList[] = {"SND", "CLR", "CAR", "IND", "FRQ", "SIG", "NSA", "MSA", "TRN", "BOB", "FRK"};
String serialParallel[] = {"serial", "parallel"};
String ports[] = {"dvi-d", "ps2", "rj-45", "stereo"};

struct Widgets {
  String serialNumber = "";
  bool hasVowel = false;

  int batteryAA = 0;
  int batteryD = 0;

  String selectedIIR;
  bool iirIsOn;
  String serialPort;
  String portSelected;

  Widgets() {
    this->hasVowel = false;

    for (int i = 0; i < 5; i++) {
      int rand = random(0, serialAlphaNumerics.length());
      char randomChar = serialAlphaNumerics[rand];

      if (hasVowel == false && (randomChar == 'A' || randomChar == 'E' || randomChar == 'I' || randomChar == 'O' || randomChar == 'U')) {
        this->hasVowel = true;
      }

      serialNumber += randomChar;
    } serialNumber += String(random(0, 9));

    this->batteryAA = random(0, 3) * 2;
    this->batteryD = random(0, 3);

    this->selectedIIR = iirList[random(0, 10)];
    this->iirIsOn = random(0, 1);
    this->serialPort = serialParallel[random(0, 1)];
    this-> portSelected = ports[random(0, 3)];
  }
};

struct PuzzleInfo {
  String ID;
  bool isFinished = false;

  PuzzleInfo(String id) : ID(id) {}
};

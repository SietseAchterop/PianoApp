//
// create apk
//   flutter build apk --split-per-abi
//   flutter build apk  --target-platform android-arm64

import 'dart:async';
import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:json_annotation/json_annotation.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';
import 'package:permission_handler/permission_handler.dart';
import 'dart:io';
import 'package:path_provider/path_provider.dart';
import 'package:app_settings/app_settings.dart';
import 'package:android_intent_plus/android_intent.dart';

part 'main.g.dart';
// to renew this file: flutter pub run build_runner build --delete-conflicting-outputs

part 'ble.dart';

ThemeData lightTheme = ThemeData(
  brightness: Brightness.light,
  useMaterial3: true,
  textTheme: const TextTheme(
    displayLarge: TextStyle(fontSize: 32, fontWeight: FontWeight.bold),
    bodyLarge: TextStyle(fontSize: 18, color: Colors.black87),
  ),
  appBarTheme: const AppBarTheme(
    color: Colors.blue,
    iconTheme: IconThemeData(color: Colors.white),
  ),
  colorScheme: ColorScheme.fromSeed(seedColor: Colors.blue),
);

late TextEditingController configTextField;

// Settings to be saved with the application
@JsonSerializable()
class Settings {
  String device;
  String name;
  bool calibrated = false;
  List<Position> settings;
  Settings(this.device, this.name, this.settings);

  factory Settings.fromJson(Map<String, dynamic> json) =>
      _$SettingsFromJson(json);
  Map<String, dynamic> toJson() => _$SettingsToJson(this);
}

@JsonSerializable()
class Position {
  String name;
  int L;
  int R;
  Position(this.name, this.L, this.R);

  factory Position.fromJson(Map<String, dynamic> json) =>
      _$PositionFromJson(json);
  Map<String, dynamic> toJson() => _$PositionToJson(this);

  @override
  String toString() {
    return 'Position: {name: $name, L: $L, R: $R}';
  }
}

// How to read/write the settings
Future<String> get _localPath async {
  final directory = await getApplicationSupportDirectory();
  //print('PAD:        ${directory.path}');
  return directory.path;
}

Future<File> get _localFile async {
  final path = await _localPath;
  //print("===== path  $path   == ");
  return File('$path/Piano_app');
}

Future<Settings> readSettings() async {
  try {
    final file = await _localFile;
    final contents = await file.readAsString();
    Map<String, dynamic> settingsMap = jsonDecode(contents);
    settings = Settings.fromJson(settingsMap);
    debugPrint("=====SET====   ${settingsMap.toString()}");
    if (settings.name != 'noName') {
      btButtonMessage = 'Connect to Piano';
//      btButtonMessage = 'Connect to ${settings.name}';
      connDevice.value = !connDevice.value;
      connDevice.value = !connDevice.value;
    }
    return settings;
  } catch (e) {
    // create initial version
    settings = initial;
    String jjson = jsonEncode(settings);
    writeSettings(jjson);
    return settings;
  }
}

Future<File> writeSettings(String settings) async {
  final file = await _localFile;
  return file.writeAsString(settings);
}

//
Position neutral = Position("Neutral", 0, 0);
Settings initial = Settings("noDevice", "noName", [neutral]);
Settings settings = initial;

// current selected configuration in the home page
int currentSetting = -1;
// currentSetting is actual setting
bool onPosition = false;
// actual positions (initially got from "i" command)
int curLeftPos = 0;
int curRightPos = 0;
// curPos in list
int indexInList = -1;

// get value from i-command
double motorgear = 8300;

// settings.device is found during scanning
bool found = false;

// connected to piano device
ValueNotifier<bool> connDevice = ValueNotifier<bool>(false);
ValueNotifier<double> voltage = ValueNotifier<double>(8.0);

//  'Connect to ${settings.name}'
//  'Connected to ${settings.name}'
//String btButtonMessage = 'Go to BT screen (long)';
String btButtonMessage = 'Press long';

//BT
Bluetooth bt = Bluetooth();

void dummyCallback() {
  debugPrint("======= dummy callback");
}

final Timer periodicTimer = Timer.periodic(
  const Duration(seconds: 1),
  (timer) {
    debugPrint("=======Timer: ${periodicTimer.tick}");
    if (connDevice.value == true) {
      debugPrint("====== PING");
    }
  },
);

// The main fuction
void main() {
  WidgetsFlutterBinding.ensureInitialized();
  readSettings();
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(theme: lightTheme, home: const HomePage());
  }
}

class HomePage extends StatefulWidget {
  const HomePage({super.key});

  @override
  State<HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> {
  Widget showSelection() => Container(
        decoration: BoxDecoration(
            borderRadius: BorderRadius.circular(10),
            border: Border.all(color: Colors.blue, width: 2)),
        child: SizedBox(
          height: 250,
          child: ListView.builder(
            padding: const EdgeInsets.all(50),
            itemCount: settings.settings.length,
            itemBuilder: (context, index) {
              return ListTile(
                title: Center(child: Text(settings.settings[index].name)),
                visualDensity: const VisualDensity(vertical: -3),
                // alleen selected tonen als de positie correct is!
                selected: onPosition && index == currentSetting,
                selectedColor: Colors.blue,
                onTap: () async {
                  debugPrint("=====comm  $currentSetting  $index");
                  if (connDevice.value) {
                    curLeftPos = settings.settings[index].L;
                    curRightPos = settings.settings[index].R;
                    // pp command
                    String command = "pp$curLeftPos,$curRightPos";
                    debugPrint("======  pp == $command");
                    await flutterReactiveBle.writeCharacteristicWithResponse(
                        _rxCharacteristic,
                        value: command.codeUnits);
                    onPosition = true;
                    debugPrint("====comm-done now at $index");
                    currentSetting = index;
                  }
                  setState(() {});
                },
              );
            },
          ),
        ),
      );

  void connectToPiano() async {
    debugPrint("===CON== ${settings.device}");
    if (settings.device != "noDevice") {
      await bt._startScan(dummyCallback);
      if (found) {
        if ((!_connected && _scanning) || (!_scanning && _connected)) {
        } else {
          bt.onConnectDevice(settings.device, settings.name, dummyCallback);
        }
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Center(child: Text('APT-e')),
      ),
      body: SingleChildScrollView(
        child: Column(
          children: [
            Container(
              //color: Colors.red,
              margin: const EdgeInsets.symmetric(vertical: 20),
              padding: const EdgeInsets.all(5),
              alignment: Alignment.topRight,
              child: ValueListenableBuilder(
                valueListenable: connDevice,
                builder: (BuildContext context, value, Widget? child) {
                  return ElevatedButton(
                    onPressed: () {
                      if (_connected) {
                        bt._disconnect();
                      } else {
                        connectToPiano();
                      }
                    },
                    onLongPress: () => Navigator.push(
                        context,
                        MaterialPageRoute(
                            builder: (context) => const BTPage())),
                    child: Text(btButtonMessage),
                  );
                },
              ),
            ),
            ValueListenableBuilder(
                valueListenable: connDevice,
                builder: (BuildContext context, value, child) {
                  return connDevice.value == true
                      ? showSelection()
                      : const Text('Please connect to the Piano.');
                }),
          ],
        ),
      ),
      bottomNavigationBar: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          ElevatedButton(
            onPressed: () {
              Navigator.push(context,
                  MaterialPageRoute(builder: (context) => const ConfigPage()));
            },
//            child: const Text('Go to config screen'),
            child: const Text('Go to Sliders'),
          ),
          ValueListenableBuilder(
            valueListenable: voltage,
            builder: (context, value, child) {
              return ElevatedButton(
                // als voltage >20 dan Error geven
                child: voltage.value > 20.0
                    ? const Text('Position error!',
                        style: TextStyle(color: Colors.red))
                    : Text('Battery: ${voltage.value} Volt',
                        style: TextStyle(
                            color: voltage.value > 6.9
                                ? const Color.fromARGB(
                                    255, 22, 106, 161) // ad hoc
                                : Colors.red)),
                onPressed: () {},
              );
            },
          ),
        ],
      ),
    );
  }
}

class BTPage extends StatefulWidget {
  const BTPage({super.key});

  @override
  State<BTPage> createState() => _BTPageState();
}

class _BTPageState extends State<BTPage> {
  @override
  void initState() {
    super.initState();
    _dataToSendText = TextEditingController();
  }

  void refreshScreen() {
    if (!mounted) {
      debugPrint("===== NOT mounted!");
      // happens when going back
      return;
    }
    setState(() {});
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
//        title: const Center(child: Text('Bluetooth settings')),
        title: const Center(child: Text('Information device')),
        leading: IconButton(
          icon: const Icon(Icons.arrow_back),
          onPressed: () async {
            // command i to get voltage
            if (connDevice.value) {
              //
              // we need to get the actual position of the piano from this
            }
            // now we get voltage on homescreen
            if (!mounted) {
              debugPrint("=====  Not Mounted when going back");
              return;
            }
            Navigator.of(context).pop();
          },
        ),
      ),
      body: SingleChildScrollView(
        child: Column(
          children: [
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                ElevatedButton(
                  onPressed: !_scanning && !_connected
                      ? () {
                          bt._startScan(refreshScreen);
                          //refreshScreen();
                          debugPrint(
                              "=======  na startscan  ${_foundBleUARTDevices.toString()}");
                        }
                      : () {},
                  child: const Text('Scan'),
                ),
                ElevatedButton(
                  onPressed: _connected
                      ? () {
                          bt._disconnect();
                          refreshScreen();
                        }
                      : () {},
                  child: const Text('Disconnect'),
                ),
              ],
            ),
            const Text('Devices found:'),
            Container(
                margin: const EdgeInsets.all(3.0),
                decoration: BoxDecoration(
                    borderRadius: BorderRadius.circular(10),
                    border: Border.all(color: Colors.blue, width: 2)),
                height: 100,
                child: ListView.builder(
                    itemCount: _foundBleUARTDevices.length,
                    itemBuilder: (context, index) => Card(
                            child: ListTile(
                          dense: true,
                          enabled: !((!_connected && _scanning) ||
                              (!_scanning && _connected)),
                          trailing: GestureDetector(
                            behavior: HitTestBehavior.translucent,
                            onTap: () {
                              if ((!_connected && _scanning) ||
                                  (!_scanning && _connected)) {
                              } else {
                                bt.onConnectDevice(
                                    _foundBleUARTDevices[index].id,
                                    _foundBleUARTDevices[index].name,
                                    refreshScreen);
                              }
                            },
                            child: Container(
                              width: 48,
                              height: 48,
                              padding:
                                  const EdgeInsets.symmetric(vertical: 4.0),
                              alignment: Alignment.center,
                              child: const Icon(Icons.add_link),
                            ),
                          ),
                          subtitle: Text(_foundBleUARTDevices[index].id),
                          title: Text(
                              "$index: ${_foundBleUARTDevices[index].name}"),
                        )))),
            const Text("Status messages:"),
            Container(
                margin: const EdgeInsets.all(3.0),
                width: 1400,
                decoration: BoxDecoration(
                    borderRadius: BorderRadius.circular(10),
                    border: Border.all(color: Colors.blue, width: 2)),
                height: 90,
                child: Scrollbar(
                    child: SingleChildScrollView(child: Text(_logTexts)))),
            const Text("Received data:"),
            Container(
                margin: const EdgeInsets.all(3.0),
                width: 1400,
                decoration: BoxDecoration(
                    borderRadius: BorderRadius.circular(10),
                    border: Border.all(color: Colors.blue, width: 2)),
                height: 90,
                child: SingleChildScrollView(
                    child: Text(_receivedData.join("\n")))),
            const Text("Send message:"),
            Container(
                margin: const EdgeInsets.all(3.0),
                padding: const EdgeInsets.all(8.0),
                decoration: BoxDecoration(
                    borderRadius: BorderRadius.circular(10),
                    border: Border.all(color: Colors.blue, width: 2)),
                child: Row(children: <Widget>[
                  Expanded(
                      child: TextField(
                    enabled: _connected,
                    controller: _dataToSendText,
                    decoration: const InputDecoration(
//                        border: InputBorder.none, hintText: 'Enter a string'),
                        border: InputBorder.none,
                        hintText: 'Enter command'),
                  )),
                  ElevatedButton(
                      onPressed: _connected ? bt.sendData : () {},
                      child: Icon(
                        Icons.send,
                        color: _connected ? Colors.blue : Colors.grey,
                      )),
                ]))
          ],
        ),
      ),
    );
  }
}

class ConfigPage extends StatefulWidget {
  const ConfigPage({super.key});

  @override
  State<ConfigPage> createState() => _ConfigPageState();
}

class _ConfigPageState extends State<ConfigPage> {
  @override
  void initState() {
    super.initState();
    configTextField = TextEditingController();
  }

  int _confIndex = 0;
  double leftPos = 0;
  double rightPos = 0;

  Widget theConfig() => Column(children: [
        Container(
          decoration: BoxDecoration(
              borderRadius: BorderRadius.circular(10),
              border: Border.all(color: Colors.blue, width: 2)),
          child: SingleChildScrollView(
            child: SizedBox(
              height: 200,
              child: ListView.builder(
                padding: const EdgeInsets.all(8),
                itemCount: settings.settings.length,
                itemBuilder: (context, index) {
                  return ListTile(
                      title: Text(settings.settings[index].name),
                      selected: index == _confIndex,
                      selectedColor: Colors.blue,
                      onTap: () {
                        setState(() {
                          _confIndex = index;
                        });
                      },
                      trailing: Text(
                          '${settings.settings[index].L}  ${settings.settings[index].R}'));
                },
              ),
            ),
          ),
        ),
        SizedBox(
          height: 40,
          child: ElevatedButton(
            onPressed: () {
              if (settings.settings.length > 1) {
                debugPrint(
                    "=====DEL  ${settings.settings.length}     $_confIndex");
                settings.settings.removeAt(_confIndex);
                if (_confIndex == settings.settings.length) {
                  _confIndex -= 1;
                }

                String jjson = jsonEncode(settings);
                writeSettings(jjson);
                setState(() {});
              }
            },
            child: const Text('Delete'),
          ),
        ),
        Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            Column(
              children: [
                Row(
                  children: [
                    const SizedBox(width: 60, child: Text('Left')),
                    Slider(
                        value: leftPos,
                        min: -5,
                        max: 5,
                        divisions: 10,
                        label: '${leftPos.round()}',
                        onChanged: (double value) {
                          setState(() {
                            leftPos = value;
                          });
                        }),
                  ],
                ),
                Row(
                  children: [
                    const SizedBox(width: 60, child: Text('Right')),
                    Slider(
                        value: rightPos,
                        min: -5,
                        max: 5,
                        divisions: 10,
                        label: '${rightPos.round()}',
                        onChanged: (double value) {
                          setState(() {
                            rightPos = value;
                          });
                        }),
                  ],
                ),
              ],
            ),
            Center(
              child: ElevatedButton(
                  onPressed: () async {
                    if (connDevice.value) {
                      debugPrint(
                          "====== Test  $leftPos  $rightPos , $curLeftPos  $curRightPos");
                      if (curLeftPos == leftPos && curRightPos == rightPos) {
                        onPosition = true; // waarschijnlijk overbodig
                      } else {
                        // TODO alleen doen als voltage.value > 6.3
                        String command =
                            "pp${leftPos.round()},${rightPos.round()}";
                        await flutterReactiveBle
                            .writeCharacteristicWithResponse(_rxCharacteristic,
                                value: command.codeUnits);
                        //force proper selection in home screen
                        currentSetting = -1;
                        settings.settings.asMap().forEach((index, value) {
                          if (value.L == leftPos && value.R == rightPos) {
                            indexInList = index;
                            debugPrint(
                                "===== POSITION  in Test===    $indexInList");
                            // set list in homepage to this item??
                            currentSetting = indexInList;
                            onPosition = true;
                          }
                          curLeftPos = leftPos.round();
                          curRightPos = rightPos.round();
                        });
                        // trigger, wat ook al weer?
                        connDevice != connDevice;
                        connDevice != connDevice;
                      }
                    }
                  },
//                  child: const Text("Test")),
                  child: const Text("Move")),
            )
          ],
        ),
        Row(
          children: [
            const Text('  New:   '),
            SizedBox(
              width: 300,
              child: TextField(
                decoration: const InputDecoration(
                  border: OutlineInputBorder(),
//                  hintText: 'Enter a new setting',
                  hintText: 'Name of new position',
                ),
                controller: configTextField,
                onSubmitted: (String value) {
                  settings.settings
                      .add(Position(value, leftPos.round(), rightPos.round()));
                  settings.settings.sort((a, b) => a.name.compareTo(b.name));
                  String jjson = jsonEncode(settings);
                  writeSettings(jjson);
                  configTextField.clear();
                  setState(() {});
                },
              ),
            ),
          ],
        ),
        Container(
          margin: const EdgeInsets.all(20),
          child: SizedBox(
            height: 40,
            child: ElevatedButton(
              child: const Text('Calibrate'),
              onPressed: () async {
                if (connDevice.value) {
                  // TODO alleen als voltage.value > 6.3 is
                  String command = "C";
                  await flutterReactiveBle.writeCharacteristicWithResponse(
                      _rxCharacteristic,
                      value: command.codeUnits);
                }
              },
            ),
          ),
        )
      ]);

  @override
  Widget build(BuildContext context) {
    return Scaffold(
        appBar: AppBar(
          leading: IconButton(
            icon: const Icon(Icons.arrow_back),
            onPressed: () {
              connDevice.value = !connDevice.value;
              connDevice.value = !connDevice.value;
              Navigator.of(context).pop();
            },
          ),
//          title: const Center(child: Text('Configuration')),
          title: const Center(child: Text('Change Position')),
        ),
        body: SingleChildScrollView(
          child: ValueListenableBuilder(
              valueListenable: connDevice,
              builder: (BuildContext context, value, child) {
                return theConfig();
              }),
        ));
  }
}

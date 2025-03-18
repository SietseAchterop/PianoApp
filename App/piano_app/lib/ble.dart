part of 'main.dart';

// This app from flutter_reactive_ble-uart
// It only works with BLE devices which advertise with a Nordic UART Service (NUS) UUID
// ignore: non_constant_identifier_names
Uuid _UART_UUID = Uuid.parse("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
// ignore: non_constant_identifier_names
Uuid _UART_RX = Uuid.parse("6E400002-B5A3-F393-E0A9-E50E24DCCA9E");
// ignore: non_constant_identifier_names
Uuid _UART_TX = Uuid.parse("6E400003-B5A3-F393-E0A9-E50E24DCCA9E");

final flutterReactiveBle = FlutterReactiveBle();
List<DiscoveredDevice> _foundBleUARTDevices = [];
late StreamSubscription<DiscoveredDevice> _scanStream;
late Stream<ConnectionStateUpdate> _currentConnectionStream;
late StreamSubscription<ConnectionStateUpdate> _connection;
late QualifiedCharacteristic _txCharacteristic;
late QualifiedCharacteristic _rxCharacteristic;
late Stream<List<int>> _receivedDataStream;
late TextEditingController _dataToSendText;
bool _scanning = false;
bool _connected = false;
String _logTexts = "";
List<String> _receivedData = [];
int _numberOfMessagesReceived = 0;

// ad hoc
bool initialized = false;

class Bluetooth {
  void sendData() async {
    await flutterReactiveBle.writeCharacteristicWithResponse(_rxCharacteristic,
        value: _dataToSendText.text.codeUnits);
    debugPrint("======    ${_dataToSendText.text}");
    _dataToSendText.clear();
  }

  void onNewReceivedData(List<int> data, VoidCallback refresh) {
    _numberOfMessagesReceived += 1;
    String str = String.fromCharCodes(data);
    debugPrint("====== command:   $str");
    _receivedData.add("$_numberOfMessagesReceived: $str");

    // get battery voltage, current position and gearvalue
    var words = str.split(' ');
    if (words[0] == "PApp:") {
      voltage.value = double.parse(words[1]);
      motorgear = double.parse(words[4]);
      debugPrint(
          "=== PApp ==  ${words[1]} ${words[2]}  ${words[3]}  ${words[4]}");
      curLeftPos = (double.parse(words[2]) / motorgear).round();
      curRightPos = (double.parse(words[3]) / motorgear).round();
      // find if this setting is in the list
      onPosition = false;
      indexInList = -1;
      // last match is used.
      settings.settings.asMap().forEach((index, value) {
        if (value.L == curLeftPos && value.R == curRightPos) {
          indexInList = index;
          debugPrint("===== POSITION  ===    $indexInList");
          // set list in homepage to this item. Ja, maar alleen de eerste keer!
          if (!initialized) {
            initialized = true;
            currentSetting = indexInList;
          }
          onPosition = true;
          connDevice.value = !connDevice.value;
          connDevice.value = !connDevice.value;
        }
      });
    }

    if (_receivedData.length > 5) {
      _receivedData.removeAt(0);
    }
    refresh();
  }

  void _disconnect() async {
    await _connection.cancel();
    _connected = false;
//    btButtonMessage = "Connect to ${settings.name}";
    btButtonMessage = "Connect to Piano";
    connDevice.value = false;
    debugPrint("===== _disconnect function called");
  }

  Future _stopScan() async {
    await _scanStream.cancel();
    _scanning = false;
  }

  //  Start and stop scanning.
  Future _startScan(VoidCallback refresh) async {
    debugPrint("======  startScan 1");
    bool goForIt = false;
    if (Platform.isLinux) {
      goForIt = true;
    } else {
      PermissionStatus permission = await Permission.location.status;
      if (permission == PermissionStatus.granted) {
        goForIt = true;
      } else {
        AppSettings.openAppSettings();
      }
    }
    // waarom geen openBluetoothSettings meer?
    flutterReactiveBle.statusStream.listen((event) {
      if (event == BleStatus.poweredOff) {
        if (Platform.isAndroid) {
          const AndroidIntent(
            action: 'android.bluetooth.adapter.action.REQUEST_ENABLE',
          ).launch().catchError((e) => AppSettings.openAppSettings());
        } else {
          AppSettings.openAppSettings();
        }
        refresh();
      }
    });

    if (goForIt) {
      _scanning = true;
      refresh();
      _foundBleUARTDevices = [];
      _scanStream = flutterReactiveBle.scanForDevices(
          withServices: [_UART_UUID],
          requireLocationServicesEnabled: false).listen((device) {
        if (_foundBleUARTDevices.every((element) => element.id != device.id)) {
          _foundBleUARTDevices.add(device);
          debugPrint("=====  FOUND ${device.name}");
          debugPrint("=====  FOUND ${_foundBleUARTDevices.toString()}");
          refresh();
        }
      }, onError: (Object error) {
        _logTexts = "${_logTexts}ERROR while scanning:$error \n";
        debugPrint(_logTexts);
        refresh();
      });
    } else {
      //await showNoPermissionDialog();
    }
    debugPrint("====== startScan 2");
    await Future.delayed(const Duration(seconds: 1));
    await _stopScan();
    debugPrint("====== stopScan 3, scanning: $_scanning");
    refresh();
    found = false;
    _foundBleUARTDevices.asMap().forEach((key, value) {
      if (value.id == settings.device) found = true;
      debugPrint(
          "======  endscan  ${value.id}  ${settings.device}  found $found");
    });
  }

  void onConnectDevice(deviceId, deviceName, VoidCallback refresh) {
    debugPrint("===== onConDev 1");
    _currentConnectionStream = flutterReactiveBle.connectToAdvertisingDevice(
      id: deviceId,
      prescanDuration: const Duration(seconds: 1),
      connectionTimeout: const Duration(seconds: 5),
      withServices: [_UART_UUID, _UART_RX, _UART_TX],
    );
    debugPrint("=====  onConDev 2");
    _logTexts = "";
    refresh();
//    btButtonMessage = 'Disconnect from $deviceName';
    btButtonMessage = 'Disconnect from Piano';
    _connection = _currentConnectionStream.listen((event) {
      var id = event.deviceId.toString();
      debugPrint("======   onConnectDevice event $deviceId  ====    $id");

      switch (event.connectionState) {
        case DeviceConnectionState.connecting:
          {
            _logTexts = "${_logTexts}Connecting to $id\n";
            debugPrint(_logTexts);
            if (settings.device != id || settings.name != deviceName) {
              // save this new device-id
              settings.device = id;
              settings.name = deviceName;
              String jjson = jsonEncode(settings);
              writeSettings(jjson);
            }
            break;
          }
        case DeviceConnectionState.connected:
          {
            _connected = true;
            _logTexts = "${_logTexts}Connected to $id\n";
            debugPrint(_logTexts);
            _numberOfMessagesReceived = 0;
            _receivedData = [];
            _txCharacteristic = QualifiedCharacteristic(
                serviceId: _UART_UUID,
                characteristicId: _UART_TX,
                deviceId: event.deviceId);
            _receivedDataStream =
                flutterReactiveBle.subscribeToCharacteristic(_txCharacteristic);
            _receivedDataStream.listen((data) {
              onNewReceivedData(data, refresh);
            }, onError: (dynamic error) {
              _logTexts = "${_logTexts}Error:$error$id\n";
            });
            _rxCharacteristic = QualifiedCharacteristic(
                serviceId: _UART_UUID,
                characteristicId: _UART_RX,
                deviceId: event.deviceId);

            connDevice.value = true;
            //
            String command = "i";
            flutterReactiveBle.writeCharacteristicWithResponse(
                _rxCharacteristic,
                value: command.codeUnits);
            break;
          }
        case DeviceConnectionState.disconnecting:
          {
            _connected = false;
            _logTexts = "${_logTexts}Disconnecting from $id\n";
            debugPrint(_logTexts);
            break;
          }
        case DeviceConnectionState.disconnected:
          {
            _logTexts = "${_logTexts}Disconnected from $id\n";
            debugPrint(_logTexts);
            debugPrint("====== DISCONNECTED state");
//            btButtonMessage = "Connect to ${settings.name}";
            btButtonMessage = "Connect to Piano";
            _connected = false;
            connDevice.value = false;
            break;
          }
      }
      refresh();
    });
  }
}

## 1.8.0
* Fix bug with Guid converted from string due to starting/ending with '{ }' in `WinBLE`

## 1.7.0
* Apply `flutter blue plus 1.28.5` (there is several breaking changes.).

## 1.6.6
* Add cache for storing characteristics.

## 1.6.0
* Apply `Flutter blue plus 1.26.0`, (there is a breaking change with `connect()`).

## 1.5.7
* Remove connection by OS when performing `startScan`.

## 1.5.3
* Write logs when connection state stream is started/terminated. 

## 1.5.2
* Fix a bug of features added in `1.5.1` 

## 1.5.1
* Remove device from connected device list when device is disconnected.

## 1.5.0
* Split functionality of `disconnect` / `removeBond`.

## 1.4.0
* Implement `Subscribe/Unsubscribe Characteristic`.

## 1.1.0
* Implement `Read/Write Characteristic`.

## 1.0.5
* Change `rxdart` version to `0.27.7`.

## 1.0.0
* Initial release (using Github action).
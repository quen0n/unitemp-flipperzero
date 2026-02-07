# Unitemp changelog
## Unitemp 2.0
- General code quality improvements. Use of native Flipper Zero firmware libraries in the GUI and sensor drivers
- Fixed memory leaks
- Sensor polling and screen rendering now occur asynchronously in separate threads
- Configuration and save files moved to apps_data
- On first application launch, measurement units will be automatically selected based on system localization settings
TODO: мигратор с старого конфига на новый
TODO: бесконечная подсветка только на экране с датчиками. в меню она работает согласно настройкам системы notification_message(app->notifications, &sequence_display_backlight_enforce_on);/notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
TODO: для отображение точки росы в иконке использовать значки с *С и *F
TODO: в readme добавить краткий гайд по использованию и основным фичам
TODO: в readme у изображений удалить фон, добавить уже поддерживаемые датчики
## Unitemp 1.8
- Fixes and improvements. Many thanks to [xMasterX](https://github.com/xMasterX) for their help and support.
## Unitemp 1.7
- Add MAX31725 temperature sensor. 
- Fix bug in SHT3x detection and reading. 
- Decrease SCD40 pooling interval. 
## Unitemp 1.6
- Add SCD30 and SCD40 Co2 sensors. 
## Unitemp 1.5
- Add Cry_dolph_55x52 image (build on latest API) (Thanks jamisonderek)
## Unitemp 1.4
- Remove version value from application manifest
- Small fix (Thanks for JamesDavid)
## Unitemp 1.3
- New FZ API satisfaction
## Unitemp 1.2
- New sensor - BME680 (temperature, humidity, pressure)
- New sensor - MAX31855 (temperature on termocouple)
- New sensor - MAX6675 (temperature on termocouple)
- Added the ability to quickly change the temperature unit by holding the OK key
- Fixes and improvements
## Unitemp 1.1
- Added new sensors: DHT20, AM2108, AHT10, AHT20, GXHT30/31/35, SHT30/31/35, BMP180, HTU21x, HDC1080.
- Fixed incorrect display of negative values of DHT sensors.
- Fixed incorrect editing and display of the I2C address
## Unitemp 1.0
- Application release
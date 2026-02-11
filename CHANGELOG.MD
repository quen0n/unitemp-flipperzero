# Unitemp changelog
## Unitemp 2.0
- General code quality improvements. Use of native Flipper Zero firmware libraries in the GUI and sensor drivers
- Fixed memory leaks
- Sensor polling and screen rendering now occur asynchronously in separate threads
- Configuration and save files moved to apps_data
- On first application launch, measurement units will be automatically selected based on system localization settings
- Saving the config is done only when it is initially created or subsequently modified
- The backlight glows indefinitely only when displaying sensor values. In the menu, the backlight will off according to the system settings
- The temperature index calculation has been corrected. It is now calculated only for temperatures above 26.67°C (80°F)

TODO: загрузка и сохранение датчиков
TODO: доделать добавление single wire датчиков

TODO: перелистывание датчиков в single sensor view
TODO: поддержка onewire датчиков
TODO: запускать и останавливать тред с опросом датчиков при входе и выходе из general scene. Синхронизировать вход - при входе в сцену дождаться обновления датчиков
TODO: автоматическое включение питание сделать настраиваемым
TODO: мигратор с старого конфига на новый
TODO: в хелпе описать что такое heat index, dew point
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
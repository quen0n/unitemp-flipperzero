# Unitemp changelog
## Unitemp 2.0
- General code quality improvements. Use of native Flipper Zero firmware libraries in the GUI and sensor drivers
- Fixed memory leaks
- Sensor polling and screen rendering now occur asynchronously in separate threads
- Configuration and save files moved to apps_data. Saved sensors will be moved automatically
- On first application launch, measurement units will be automatically selected based on system localization settings
- Saving the config is done only when it is initially created or subsequently modified
- The backlight glows indefinitely only when displaying sensor values. In the menu, the backlight will off according to the system settings
- The temperature index calculation has been corrected. It is now calculated only for temperatures above 26.67°C (80°F)
- It is now possible to disable automatic 5V power supply to port 1 in the settings
- The 1-wire driver has been rewritten using native Flipper Zero firmware features
- The display of CO2 concentration values ​​in the air has been improved. The ability to display the heat index along with the CO2 value has been added
- Добавлена проверка инициализации всех датчиков и реинициализация при ошибке
- Драйвер SCD30 переписан
- Добавлено состояние ожидания значений с датчика
- Порт 17 доступен для датчиков 1W
- no sensors found заменено на no sensors yet
- иконка плачущего дельфина заменена на сконфуженного
- новый датчик - HDC2080
- временно выпилены SCD40/41 (жду пока дойдут с алика)
- обновлён список датчиков в Readme
### TODO лист
- разделить на разные пункты меню датчики с разными названиями
- исправить невозможность сборки через ufbt датчика HDC2080
- индикатор состояния среды (светодиодная индикация, звуковая сигнализация)
- проверить сделать автоматический подогрев датчиков при 
- скан шины I2C
- добавить недостающие датчики из старой версии (MAX31725, SCD40, SCD41)
- напиздить датчики со всяких форков и PR (https://github.com/quen0n/unitemp-flipperzero/pull/50, TMP102 https://github.com/dolphindan23/unitemp-flipperzero-bme688/, BME688 https://github.com/macdja38/unimonit-flipperzero/blob/main/sensors/BME688.c)
- написать драйверы для SHT40
- пройтись по коду и подписать прототипы функций
- в хелпе описать что такое heat index, dew point
- в readme добавить краткий гайд по использованию и основным фичам
- в readme у изображений удалить фон, добавить уже поддерживаемые датчики
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
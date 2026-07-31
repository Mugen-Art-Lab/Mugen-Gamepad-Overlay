# Установка и удаление

## Рекомендуемый установщик

1. Полностью закройте OBS Studio.
2. Запустите `Mugen-Gamepad-Overlay-0.8.0-Windows-x64-Setup.exe` от имени администратора.
3. Запустите OBS Studio.
4. Добавьте источник **Mugen Gamepad Overlay**.

Установщик помещает плагин в:

`C:\ProgramData\obs-studio\plugins\mugen-gamepad-overlay`

Для обновления закройте OBS Studio и запустите новый установщик. Для удаления воспользуйтесь разделом **Установленные приложения** Windows либо удалите указанную папку при закрытой OBS Studio.

## Стандартный ZIP

Распакуйте содержимое `Mugen-Gamepad-Overlay-0.8.0-Windows-x64.zip` в:

`C:\ProgramData\obs-studio\plugins`

Итоговый путь к DLL должен быть:

`C:\ProgramData\obs-studio\plugins\mugen-gamepad-overlay\bin\64bit\mugen-gamepad-overlay.dll`

## ZIP для portable/нестандартной OBS

Закройте OBS Studio и распакуйте `Mugen-Gamepad-Overlay-0.8.0-Windows-x64-portable.zip` в корневую папку portable или нестандартной установки OBS. В архиве находятся папки `obs-plugins` и `data`.

## Если источник не появился

- убедитесь, что OBS Studio была полностью закрыта во время установки или обновления;
- проверьте, что Windows и OBS Studio имеют архитектуру x64;
- откройте **Справка → Файлы журнала → Просмотреть текущий журнал** и найдите `Mugen Gamepad Overlay` или `mugen-gamepad-overlay.dll`;
- при сообщении об ошибке приложите версию OBS, версию Windows, способ установки, название и режим подключения контроллера, а также текущий журнал.

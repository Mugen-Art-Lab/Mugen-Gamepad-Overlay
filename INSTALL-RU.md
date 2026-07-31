# Установка, обновление и удаление

[English version](INSTALL.md)

## Рекомендуемый установщик

1. Полностью закройте OBS Studio.
2. Запустите `Mugen-Gamepad-Overlay-0.8.0-Windows-x64-Setup.exe`.
3. Подтвердите запрос прав администратора Windows.
4. Запустите OBS Studio.
5. Добавьте источник **Mugen Gamepad Overlay**.

Установщик помещает плагин в:

`C:\ProgramData\obs-studio\plugins\mugen-gamepad-overlay`

Установщик пока не имеет цифровой подписи, поэтому Windows SmartScreen может показать предупреждение о неизвестном издателе. Скачивайте релизные файлы только из репозитория Mugen Art Lab на GitHub; при необходимости сверяйте SHA-256 с `SHA256SUMS.txt`.

Для обновления полностью закройте OBS Studio и запустите новый установщик. Настройки источника сохраняются в коллекции сцен OBS. Для удаления воспользуйтесь разделом **Установленные приложения** Windows.

## Стандартный ZIP

Распакуйте содержимое `Mugen-Gamepad-Overlay-0.8.0-Windows-x64.zip` в:

`C:\ProgramData\obs-studio\plugins`

Итоговый путь к DLL должен быть:

`C:\ProgramData\obs-studio\plugins\mugen-gamepad-overlay\bin\64bit\mugen-gamepad-overlay.dll`

Для ручного удаления полностью закройте OBS Studio и удалите папку `mugen-gamepad-overlay`.

## ZIP для portable или нестандартной OBS

Закройте OBS Studio и распакуйте `Mugen-Gamepad-Overlay-0.8.0-Windows-x64-portable.zip` в корневую папку portable или нестандартной установки OBS. В архиве находятся папки `obs-plugins` и `data`.

Для ручного удаления удалите:

```text
obs-plugins\64bit\mugen-gamepad-overlay.dll
data\obs-plugins\mugen-gamepad-overlay
```

## Если источник не появился

- убедитесь, что OBS Studio была полностью закрыта во время установки, обновления или удаления;
- проверьте, что Windows и OBS Studio имеют архитектуру x64;
- откройте **Справка → Файлы журнала → Просмотреть текущий журнал** и найдите `Mugen Gamepad Overlay` или `mugen-gamepad-overlay.dll`;
- при сообщении об ошибке приложите версию OBS, версию Windows, способ установки, название и режим подключения контроллера, а также текущий журнал.

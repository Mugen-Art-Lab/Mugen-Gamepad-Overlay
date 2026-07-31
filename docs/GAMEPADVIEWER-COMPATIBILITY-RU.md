# Совместимость с пользовательскими скинами GamepadViewer

Mugen Gamepad Overlay 0.8.0 не запускает страницу GamepadViewer и не является браузером. Плагин читает локальный CSS как описание слоёв, загружает локальные SVG/PNG/JPEG и отрисовывает их собственным движком внутри OBS.

## Как подготовить скин

Скачайте всю папку скина, сохранив относительные пути. Например:

```text
snk/
├── aes.css
└── aes/
    ├── base.svg
    ├── button-a.svg
    └── pressed-button-a.svg
```

В свойствах источника выберите:

```text
Источник скина → Локальный скин GamepadViewer (CSS)
Локальный CSS-файл скина → нужный .css
```

## Проверенный набор

Полностью проверена коллекция `frolovlife/gamepadviewer-skins`:

https://github.com/frolovlife/gamepadviewer-skins

## Поддерживаемые элементы

- `.a`, `.b`, `.x`, `.y`, `.start`, `.back`;
- `.meta`, `.guide`, `.home`, `.capture`, `.misc`, `.touchpad`;
- `.bumper.left/right`;
- `.trigger.left/right` с плавной прозрачностью от L2/R2;
- `.trigger-button.left/right` с пороговым pressed-состоянием;
- `.stick.left/right` с движением по осям и `.pressed` для L3/R3;
- `.dpad .face.up/right/down/left`;
- `.fstick` и восемь направлений аркадного стика;
- распространённые `.controller.custom` и `.custom.controller`.

## Поддерживаемое CSS-подмножество

- состояния `.pressed` и специфичность классов;
- локальные SVG, PNG и JPEG;
- `width`, `height`, `left`, `right`, `top`, `bottom`;
- `margin-left/top/right/bottom`;
- `translateX`, `translateY`, `translate(...)`;
- пиксели, проценты, `vw`, `vh`;
- простые `calc(...)`;
- `background-image`, `background-position`, `background-size`;
- SVG-атласы и отрицательные смещения;
- `cover`, `contain`, `center`;
- `opacity`, `display: none`, `visibility: hidden`;
- горизонтальное отражение через `rotateY(180deg)` или `scaleX(-1)`;
- ограниченный набор безопасных псевдоэлементов и цветовых фильтров.

## Нативная аналоговая логика

- SDL X/Y смещают `.stick.left/right`;
- мёртвая зона применяется до отрисовки;
- максимальный ход регулируется в свойствах;
- L3/R3 выбирают `.pressed`-слой стика;
- L2/R2 управляют прозрачностью или pressed-состоянием триггеров.

## Намеренные ограничения

- JavaScript и HTML не исполняются;
- сетевые URL и абсолютные пути отвергаются;
- CSS-анимации и полный браузерный CSS не поддерживаются;
- встроенные Base64-растры внутри SVG не гарантируются;
- один скин может использовать особенности, которых нет в безопасном локальном рендерере.

При ошибке плагин показывает экран `SKIN NOT LOADED`, а точная причина записывается в свойства и журнал OBS. Он больше не подменяет неудачный импорт встроенным геймпадом.

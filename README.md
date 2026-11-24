![baLONCUaga — Update V3.2](./assets/banner-v3.svg)

# baLONCUaga — Update V3.2

HUD для CS 1.6 Steam Legacy x86. Windows, OpenGL. Русский и английский интерфейс.

[Скачать для чистой CS](https://github.com/fr3cy/baloncuaga-hud-cs1.6/releases/download/v3.2/BALONCUAGA_v3.2_FULL.zip) · [Обновить ASI](https://github.com/fr3cy/baloncuaga-hud-cs1.6/releases/download/v3.2/CS16Customizer.asi) · [Исходники](./source)

## Что изменилось

- Компонент переименован в `cs_fix.asi`, вкладка — **CS Fix**.

- Обновлён запуск **fps_visual**: 20–1000, стандарт — 100. Настройка сохраняется.
- Запуск голосового фикса отделён от остальных патчей CS Fix.
- Добавлены проверки совместимости движка, клиента и Steam API.
- Старые распознанные копии CS Fix при обновлении сохраняются как `.disabled`.
- Ошибка установки дополнительных ресурсов больше не отключает всё меню. Код Windows и имя файла доступны во вкладке CS Fix.

CS Fix рассчитан на Steam Legacy **8684** с соответствующим клиентом.

## Установка

**Чистая CS:** распакуй FULL рядом с `hl.exe`. В комплекте `CS16Customizer.asi`, `cs_fix.asi`, `dinput.dll` и `global.ini`. Используй один ASI-загрузчик и одну копию CS Fix.

**Обновление:** закрой игру и замени только `CS16Customizer.asi`. После первого запуска полностью перезапусти CS ещё раз — компонент обновится автоматически.

Меню — **Insert**. **F11** выгружает HUD; отдельный CS Fix работает до закрытия игры.

## Возможности

Модели оружия и игроков, прицел, FOV, радар, чат и радио, скайбоксы, цвет дыма и флешки, Showkeys, тренажёр колеса и конфиги. История и избранные серверы Steam, шесть стилей статы стрейфов, локальный FOG. **БРАТУХА** — офлайн-собеседник. Главное меню и TAB остаются штатными.

Свои MDL: `cstrike/models/customizer/knife`, `he`, `flash`, `smoke`. Затем «Обновить список» → «Применить».

## Исходники

[source](./source) · [third_party](./third_party) · [Сборка](./BUILD_RU.txt). Готовые файлы для игры — в Releases.

## Авторы

[fr3cy](https://github.com/fr3cy) · [wetalik001](https://github.com/wetalik001)

CS Fix: [Garey27 / CS_Patches](https://github.com/Garey27/CS_Patches). [Лицензии компонентов](./licenses).

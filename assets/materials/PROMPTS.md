# Материалы одежды

Инструмент: встроенный OpenAI imagegen. Референсы — шесть фотографий одежды,
приложенных пользователем. Фотографии и личные пути не входят в поставку.
PNG — исходные материалы для сборки текстур, а не изображения результата в игре.

## КТ: задание генерации

Квадратный лист 3×3, девять заполненных ортографических панелей diffuse/albedo.
Верх: чёрная зипка Vetements FAN? спереди, сзади, материал капюшона и рукавов.
Середина: Mowalola Atlanta — левая штанина с красной 6, правая с белым конём и
LOTTA/mowa, задняя часть с красной ATLANTA. Низ: кожа Stomper сбоку, носок,
язычок со шнуровкой. Натуральный плотный хлопок, мягкие нерегулярные складки,
строчки, потёртая кожа, тени в швах. Без людей, подписей, водяных знаков,
каркасной сетки и регулярных вертикальных полос.

## ТТ: промпт

Use case: stylized-concept / production texture atlas for a real GoldSrc 3D player clothing asset. Generate a high resolution SQUARE 3x3 contact sheet, exactly nine equal square panels edge-to-edge, no margins/gutters/grid/captions. Reference images: charcoal Enfants Riches Deprimes hoodie, black Rick Owens Bolan Banana flared zipper pants, black/ivory Rick Owens Geobasket high tops. These images guide the materials and details, not an edit target. This is a filled orthographic diffuse/albedo material sheet, not a character, no people, no catalog background, no 3D turntable. Every cell's material extends to its four edges. Photographic natural cotton grain, heavy faded charcoal cotton with irregular soft compression folds, genuine leather with stitching and subtle wear, realistic baked crease occlusion. Avoid straight painted vertical lines, grid/wireframe, glossy synthetic fabric. TOP LEFT: front rectangular torso surface of hoodie, exact cream serif text in two lines 'ENFANTS RICHES' and 'DÉPRIMÉS', distressed lightly like reference, natural fabric folds and sewn kangaroo pocket. TOP CENTER: same hoodie's plain back torso with soft folds, no print. TOP RIGHT: hood and sleeve material with folded thick stitched hood hem in upper half and elbow compression folds in lower half, no text. MIDDLE LEFT and MIDDLE CENTER: corresponding left and right black Bolan Banana pants front leg surfaces, long silver curved banana zippers near outer seams, realistic black washed denim and long fluid folds, relaxed flare at lower end. MIDDLE RIGHT: plain rear of these black pants with back pockets and natural seat/knee folds, no letters. BOTTOM LEFT: flattened outer side leather panel of black/ivory Geobasket, black worn leather, cream angular side panel and stitched trim, ivory cupsole at bottom, all material fills tile. BOTTOM CENTER: rounded black leather toe/vamp surface viewed straight on with realistic leather creases and stitched border above a cream sole band. BOTTOM RIGHT: tall padded black tongue and crossing ivory laces with eyelets matching Geobasket, cream sole at bottom. Crisp physically plausible construction and realistic tactile material, no extra brands, labels, watermark or text beyond specified hoodie print. The entire result is a precise nine-tile game texture sheet.

## Экспорт

`../authoring/build_native_outfits.py` собирает три отдельных индексированных
атласа 512×512: кофта, штаны, обувь. Большие PNG не загружаются игровым путём
индексированных текстур. Изображения в `../previews_native/` отрисованы из
экспортированных MDL со штатными анимациями CS 1.6.

# Лабораторная работа №2 «Камера и освещение» — план (минимум на оценку 3)

**Цель: оценка 3 — только базовое задание, без дополнительных заданий.**

> Источники: условие [Lab #2 Camera and Lighting.pdf](Lab%20%232%20Camera%20and%20Lighting.pdf), сообщение
> преподавателя [read.txt](read.txt) (статья <https://learnopengl.com/Lighting/Light-casters>), код лабы 1
> (ветка `lab1`, коммит `40bd0d7`), образец отчёта [lab1/3XX_Фамилия_ЛабX.docx](../lab1/3XX_Фамилия_ЛабX.docx).

### Что проверено по коду лабы 1

| Проверка | Результат |
|----------|-----------|
| Сборка на текущей машине (Fedora): системные заголовки Vulkan **1.4.350** в `/usr/include` | ✅ хватает пресета `debug`, `CMakeUserPresets.json` не нужен |
| Кадров «в полёте» | **один** (одна fence `vk_fence_frame_in_flight` в [graphics_internal.cpp:733](../source/graphics_internal.cpp#L733)) → копировать данные в UBO в `render()` безопасно, как в лабе 1 |
| Формат swapchain | `use_default_format_selection()` у vk-bootstrap → **`B8G8R8A8_SRGB`** (проверено в исходниках vk-bootstrap): шейдер пишет линейный цвет, гамма-коррекцию делает железо |
| Что переиспользуем | `math::mat4`, `translate`, `rotateX/Y`, `perspective`, `radians` ([math.hpp](../source/math.hpp)); `createBuffer/uploadToBuffer/destroyBuffer/createShaderModule` ([graphics.cpp](../source/graphics.cpp)); дескриптор, pipeline и запись команд из [application.cpp](../source/application.cpp) — меняются точечно |

Код пишешь **сам**. Фрагменты ниже нужны, чтобы свериться, а не чтобы их копировать.

---

## Оглавление

0. [Оценка и сдача](#0-оценка-и-сдача)
1. [Что нужно сделать](#1-что-нужно-сделать)
2. [Что меняется относительно лабы 1](#2-что-меняется-относительно-лабы-1)
3. [Теория: Блинн-Фонг и направленный источник](#3-теория-блинн-фонг-и-направленный-источник)
4. [Раскладка UBO (std140)](#4-раскладка-ubo-std140)
5. [Этап A — математика vec3](#5-этап-a--математика-vec3)
6. [Этап B — свободная камера](#6-этап-b--свободная-камера)
7. [Этап C — нормали, процедурная генерация](#7-этап-c--нормали-процедурная-генерация)
8. [Этап D — Блинн-Фонг: направленный свет, ambient, материал (= оценка 3)](#8-этап-d--блинн-фонг-направленный-свет-ambient-материал--оценка-3)
9. [Типичные ловушки](#9-типичные-ловушки)
10. [README, отчёт, git](#10-readme-отчёт-git)
11. [Подготовка к защите](#11-подготовка-к-защите)

---

## 0. Оценка и сдача

- **Оценка 3 = базовое задание без дополнительных** (PDF, раздел «Оценивание»).
- На сдаче будет **3–4 случайных вопроса** по коду и математике. **Для 3 нужно правильно ответить хотя бы на один**
  (read.txt лабы 1). Если не ответишь, придёшь в следующий день сдачи.
- Максимальная оценка снижается каждые две недели, но **не ниже 3**, так что для тройки сроки не важны.
- Преподаватель видит LLM-код и проверяет понимание. Поэтому после каждого этапа есть блок **«Что нужно понимать»**.
- Vulkan + C++, ImGui. Подписи в ImGui только на английском: встроенный шрифт не содержит кириллицы.

---

## 1. Что нужно сделать

### 1.1. Базовое задание (из PDF)

> «Для объекта из предыдущей лабораторной работы вашего варианта необходимо сделать только его отрисовку
> относительно камеры с единственным направленным источником освещения. Все параметры камеры, источника освещения
> и параметры материала по модели освещения Блинн-Фонга, а также рассеянный свет (ambient light) должны быть
> изменяемыми из пользовательского интерфейса ImGUI.»

Из этого и общих пунктов условия, которые относятся к базе:

- пирамида из лабы 1 рисуется **относительно камеры**; камера управляется из ImGui: **позиция, поворот, угол обзора**;
- у вершин есть **атрибут нормали**, нормали **генерируются процедурно** (считаются в коде, а не вписываются руками);
- **один направленный источник** и **ambient**, освещение по **Блинн-Фонгу во фрагментном шейдере**;
- из UI меняются: камера; направление, цвет и интенсивность источника; цвет и интенсивность ambient; **материал**
  (альбедо, цвет блика, коэффициент блика).

### 1.2. Чего НЕ делаем (это доп. задания 1–4)

В разделе «Условие» PDF есть пункты про затухание точечных и прожекторных источников, мягкие края прожектора и
storage-буферы. Они относятся к **дополнительным заданиям 2–3**, к базе не относятся. Не делаем:

- два набора дескрипторов (сцена / модель) — доп. 1;
- точечные источники, прожекторы, затухание 1/d², storage-буферы — доп. 2, 3;
- визуализацию источников — доп. 4;
- пол, несколько объектов, WASD/мышь.

### 1.3. Объём

Новых файлов нет, CMakeLists не меняется. Меняются `math.hpp` (+ `vec3`), `application.cpp` и шейдеры
`pyramid.vert/frag`. Примерно +150 строк своего кода.

---

## 2. Что меняется относительно лабы 1

| Было (лаба 1) | Станет (лаба 2) |
|---------------|-----------------|
| Камера: `translate(0,0,−distance)·rotateX(tilt)`, смотрит на центр | Свободная камера: позиция + yaw + pitch + FOV |
| `Vertex {position, color}`, 5 вершин | `Vertex {position, normal}`, **16 вершин** (у каждой грани свои вершины с нормалью грани) |
| UBO `{mat4 mvp}` только для вершинного шейдера | UBO `Uniforms` (§4): `model`, `view_projection`, камера, свет, ambient, материал; виден **обоим** шейдерам |
| Шейдеры: цвет вершины | Шейдеры: мировые позиция и нормаль, Блинн-Фонг во фрагментном |
| UI в окне «Lab 1»: Background + 4 слайдера | UI: окно «Lab 2», Background и Rotation speed сверху, секции Camera / Ambient / Directional light / Material |

Что остаётся как было: один descriptor set с одним UBO (binding 0), один pipeline, буферы через VMA, порядок
`update()` → `render()`, `shutdown()`. Шейдеры можно не переименовывать: в CMakeLists уже стоят
`compile_shader(pyramid.vert)` и `compile_shader(pyramid.frag)`, а рисуется по-прежнему пирамида.

Сборка на этой машине (Fedora): `cmake --preset debug && cmake --build build-debug --parallel`, запуск из корня:
`./build-debug/vulkan-starter-app`.

---

## 3. Теория: Блинн-Фонг и направленный источник

Статья преподавателя: <https://learnopengl.com/Lighting/Light-casters>. Из неё для тройки нужен только
**направленный источник** (Directional Light).

### 3.1. Обозначения (всё в **мировых** координатах)

- `P` — точка поверхности (позиция фрагмента в мире).
- `N` — нормаль поверхности (единичная).
- `V = normalize(camera_position − P)` — от точки к камере.
- `L` — единичный вектор **от точки к источнику**. У направленного источника `L = −direction`, одинаковый для
  всех точек (источник «бесконечно далеко», как Солнце). Поэтому у него нет позиции и затухания.
- `H = normalize(L + V)` — **halfway-вектор**, в этом отличие Блинн-Фонга от Фонга.
- Материал: `albedo` (диффузный цвет), `specular_color` (цвет блика), `shininess` (коэффициент блика, ≥ 1).

### 3.2. Формула

```
ambient  = ambient_color · ambient_intensity · albedo
diffuse  = albedo · max(N·L, 0)
specular = specular_color · max(N·H, 0)^shininess          (только если N·L > 0)
итог     = ambient + (diffuse + specular) · light_color · light_intensity
```

- **ambient** — рассеянный свет, приходит отовсюду. Без него теневая сторона была бы абсолютно чёрной.
- **diffuse** (закон Ламберта) — `N·L = cos` угла падения. Свет, падающий перпендикулярно, даёт максимум, а свет
  по касательной — ноль. От камеры не зависит.
- **specular** — блик, зависит от положения камеры. Чем больше `shininess`, тем блик меньше и резче: косинус
  меньше 1 в большой степени быстро падает к нулю.
- Блик считаем только при `N·L > 0`: если свет падает с обратной стороны, блика быть не может.

**Фонг и Блинн-Фонг.** У Фонга `R = reflect(−L, N)` и `(R·V)^shininess`. Блинн-Фонг заменяет это на
`(N·H)^shininess`: так дешевле, и блик не обрывается резко, когда угол между `V` и `R` больше 90°.

### 3.3. Нормали

- **Normal matrix.** В общем случае нормаль переводится в мир матрицей `transpose(inverse(mat3(model)))`, а не
  матрицей модели: при неравномерном масштабе матрица модели «наклоняет» нормаль, и она перестаёт быть
  перпендикулярной грани. У нас `model = rotateY(angle)`, то есть чистый поворот, а для поворота
  `transpose(inverse(R)) = R`. Поэтому в шейдере достаточно `mat3(u.model) * in_normal`. На защите нужно
  уметь объяснить, почему в нашем случае так можно.
- **`normalize(v_normal)` во фрагментном шейдере** нужен в общем случае: если у вершин треугольника разные
  единичные нормали, интерполяция короче 1 (середина между `(1,0,0)` и `(0,1,0)` — `(0.5, 0.5, 0)`, длина 0.707).
  У нашей пирамиды у трёх вершин грани одна нормаль, поэтому после интерполяции она остаётся единичной.
  Нормализацию всё равно оставляем, чтобы шейдер не зависел от модели. Так и отвечать на защите.
- **Плоские грани.** Если у пирамиды 5 вершин, апекс общий для 4 граней, а у вершины может быть только одна
  нормаль, поэтому грани сгладятся. Чтобы каждая грань освещалась как плоскость, у неё свои вершины с нормалью
  этой грани: 4 боковые × 3 + основание 4 = **16 вершин**, 18 индексов.

### 3.4. sRGB

Swapchain `B8G8R8A8_SRGB`: шейдер выдаёт **линейную** яркость, при записи железо применяет гамма-кривую. Поэтому
ambient `0.1` на экране выглядит ярче, чем «10 % белого», а цвета из `ColorEdit3` на пирамиде светлее, чем
квадратик в палитре ImGui. Это не ошибка, но на вопрос «почему так светло» нужно уметь ответить.

---

## 4. Раскладка UBO (std140)

Один UBO, как в лабе 1, только полей больше. Калькулятор из read.txt лабы 1:
<https://jackjamison.net/interactive/glsl-layout-calculator/>.

Правило std140, которое здесь важно: **`vec3` выравнивается по 16 байтам**, но занимает 12. Поэтому после `vec3`
можно положить один `float`, и вместе они займут ровно 16 байт. Если `float` нет, в C++ нужно вписать 4 байта
выравнивания (`_pad`) руками: в GLSL компилятор добавит их сам, а C++ про std140 ничего не знает.

```cpp
// Must match "uniform Uniforms" block in shaders/pyramid.vert and pyramid.frag (std140 layout)
struct Uniforms {
	math::mat4 model;             //   0
	math::mat4 view_projection;   //  64
	math::vec3 camera_position;   // 128
	float _pad0;                  // 140  следующий vec3 должен начаться с 144
	math::vec3 ambient_color;     // 144
	float ambient_intensity;      // 156
	math::vec3 light_direction;   // 160  куда светит, единичный
	float light_intensity;        // 172
	math::vec3 light_color;       // 176
	float _pad1;                  // 188
	math::vec3 albedo;            // 192
	float _pad2;                  // 204
	math::vec3 specular_color;    // 208
	float shininess;              // 220
};                                // 224
static_assert(sizeof(Uniforms) == 224);
static_assert(offsetof(Uniforms, ambient_color) == 144);
static_assert(offsetof(Uniforms, albedo) == 192);
```

GLSL (в обоих шейдерах одинаково, поля `_pad` не пишем):

```glsl
layout(set = 0, binding = 0) uniform Uniforms {
	mat4 model;
	mat4 view_projection;
	vec3 camera_position;
	vec3 ambient_color;   float ambient_intensity;
	vec3 light_direction; float light_intensity;
	vec3 light_color;
	vec3 albedo;
	vec3 specular_color;  float shininess;
} u;
```

Зачем `model` и `view_projection` отдельно, а не одна `mvp`, как в лабе 1: свет считается в мировых координатах,
поэтому шейдеру нужна мировая позиция точки (`model · position`), а уже потом `view_projection` для экрана.

---

## 5. Этап A — математика vec3

В [math.hpp](../source/math.hpp) дописать в стиле `mat4`:

```cpp
struct vec3 {
	float x = 0.0f, y = 0.0f, z = 0.0f;
};
static_assert(sizeof(vec3) == 12);       // три float подряд, иначе раскладка §4 съедет

// ВСЕ функции — inline, с телом прямо в заголовке (как translate/rotateX/... в лабе 1)
inline vec3 operator+(vec3 a, vec3 b);  inline vec3 operator-(vec3 a, vec3 b);
inline vec3 operator-(vec3 a);          inline vec3 operator*(vec3 a, float s);
inline float dot(vec3 a, vec3 b);       // ax·bx + ay·by + az·bz
inline vec3  cross(vec3 a, vec3 b);     // (ay·bz − az·by,  az·bx − ax·bz,  ax·by − ay·bx)
inline float length(vec3 v);            // sqrt(dot(v, v))
inline vec3  normalize(vec3 v);         // v * (1 / length(v)); нулевой вектор сюда не передавать (§8.4)
inline mat4  translate(vec3 v);         // translate(v.x, v.y, v.z) — для камеры
```

**Почему `inline`.** `#pragma once` защищает от двойного включения заголовка **в один** `.cpp`. Если в будущем
`math.hpp` подключат два `.cpp`, функция без `inline` окажется в обоих объектных файлах, и компоновщик выдаст
`multiple definition`. `inline` разрешает одинаковые определения в разных единицах трансляции. Сейчас `math.hpp`
подключён только в `application.cpp`, но правило лучше соблюдать сразу, как в лабе 1.

**Проверка** (временно в `initialize()`, потом удалить):
- `cross({1,0,0}, {0,1,0}) = (0,0,1)`;
- `normalize(cross(A − B0, B1 − B0))` для пирамиды = `(0, 0.5547, −0.8321)` (см. §7.2).

✅ **Чекпоинт A:** числа совпадают. `git commit -m "Lab2: vec3 math"`.

**Что нужно понимать:** скалярное произведение = `|a||b|cos α`, отсюда `N·L` — косинус угла падения;
векторное произведение перпендикулярно обоим векторам, направление по правилу правой руки, поэтому знак нормали
зависит от порядка обхода.

---

## 6. Этап B — свободная камера

### 6.1. Параметры

```cpp
struct Camera {
	math::vec3 position = {0.0f, 2.0f, 6.0f};
	float yaw_deg = 0.0f;       // поворот вокруг Y; 0 → смотрит вдоль −Z; > 0 → влево
	float pitch_deg = -18.0f;   // наклон: > 0 вверх, < 0 вниз; [−89, 89]
	float fov_deg = 60.0f;
};
```

Из `(0, 2, 6)` с наклоном −18° камера смотрит в центр пирамиды (`atan(2/6) ≈ 18.4°`).

### 6.2. Матрица вида = обратная к трансформации камеры

Камера в мире = `translate(p) · rotateY(yaw) · rotateX(pitch)`. Вид — обратная матрица: обратная к произведению
равна произведению обратных в обратном порядке, а обратная к повороту — поворот на −угол.

```
view = rotateX(−pitch) · rotateY(−yaw) · translate(−p)
view_projection = perspective(fov, aspect, 0.1, 100) · view
```

Направление взгляда (для проверки): `forward = (−sin yaw · cos pitch,  sin pitch,  −cos yaw · cos pitch)`.

**Проверка:** `view · (p, 1) = (0, 0, 0, 1)`; `view · (p + forward, 1) = (0, 0, −1, 1)`.

### 6.3. UI

```cpp
ImGui::Begin("Lab 2");
if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
	ImGui::DragFloat3("Position", &camera.position.x, 0.05f);
	ImGui::SliderFloat("Yaw", &camera.yaw_deg, -180.0f, 180.0f);
	ImGui::SliderFloat("Pitch", &camera.pitch_deg, -89.0f, 89.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
	ImGui::SliderFloat("FOV", &camera.fov_deg, 20.0f, 120.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
}
```

`ImGuiSliderFlags_AlwaysClamp` нужен, потому что Ctrl+клик по слайдеру позволяет ввести число вне диапазона.

На этом этапе шейдеры и UBO ещё из лабы 1: `uniforms.mvp = view_projection · rotateY(angle)`. Слайдеры
Distance и Tilt убрать. Background и Rotation speed оставить как есть, над секциями: это параметры сцены, а не
камеры и не материала.

✅ **Чекпоинт B:** камеру можно сдвинуть и повернуть, пирамида остаётся на месте; при yaw > 0 сцена уходит вправо.
`git commit -m "Lab2: free camera"`.

**Что нужно понимать:** почему view — это обратная матрица (двигаем не камеру, а весь мир в противоположную
сторону); почему порядок `rotateX · rotateY · translate`, а не наоборот; зачем ограничивать pitch ±89° (на ±90°
взгляд вертикален, дальше картинка переворачивается).

---

## 7. Этап C — нормали, процедурная генерация

### 7.1. Новый формат вершины

```cpp
struct Vertex {
	math::vec3 position;
	math::vec3 normal;
};
```

Атрибуты в pipeline: `location 0` — `position`, `location 1` — `normal` (вместо `color`), оба
`VK_FORMAT_R32G32B32_SFLOAT`, `offsetof(Vertex, …)`.

Массивы `pyramid_vertices` / `pyramid_indices` из лабы 1 заменяются на `std::vector<Vertex>` и
`std::vector<uint32_t>` (нужен `#include <vector>`). Их можно сделать **локальными** в `initialize()`:
`createBuffer` сразу копирует данные в буфер (`uploadToBuffer` внутри), и после этого векторы больше не нужны.

- В `createBuffer` передаётся `vertices.size() * sizeof(Vertex)` и `vertices.data()` (то же для индексов).
- `constexpr uint32_t pyramid_index_count = sizeof(...) / sizeof(...)` из лабы 1 больше не компилируется: размер
  вектора известен только во время работы. Заменить на обычную глобальную переменную
  `uint32_t pyramid_index_count = 0;` и в `initialize()` записать в неё `uint32_t(indices.size())`.
  `vkCmdDrawIndexed` в `render()` по-прежнему использует `pyramid_index_count`.

### 7.2. Генерация нормалей (это и есть «процедурно»)

Углы и грани — те же, что в лабе 1. Для каждой грани: нормаль = нормированное векторное произведение двух рёбер,
вершины грани копируются с этой нормалью, многоугольник режется на треугольники веером.

```cpp
// Each face is a convex polygon of corner indices, counter-clockwise when looking from outside.
// Every face gets its own vertices with the face normal, so faces are lit flat.
void buildPyramid(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) {
	const math::vec3 corners[] = {
		{-1.0f, -0.75f, -1.0f}, {1.0f, -0.75f, -1.0f}, {1.0f, -0.75f, 1.0f}, {-1.0f, -0.75f, 1.0f}, // base
		{ 0.0f,  0.75f,  0.0f},                                                                      // apex
	};
	const std::vector<uint32_t> faces[] = {
		{0, 4, 1}, {1, 4, 2}, {2, 4, 3}, {3, 4, 0},   // sides
		{0, 1, 2, 3},                                 // base, looks down
	};

	for (const std::vector<uint32_t>& face : faces) {
		const math::vec3 a = corners[face[0]], b = corners[face[1]], c = corners[face[2]];
		const math::vec3 normal = math::normalize(math::cross(b - a, c - a));   // CCW снаружи → наружу

		const uint32_t first = uint32_t(vertices.size());
		for (uint32_t corner : face) {
			vertices.push_back({corners[corner], normal});
		}
		for (uint32_t i = 1; i + 1 < face.size(); ++i) {                        // веер: (0,1,2), (0,2,3)
			indices.insert(indices.end(), {first, first + i, first + i + 1});
		}
	}
}
```

Итог: 4 × 3 + 4 = **16 вершин**, 4 × 3 + 2 × 3 = **18 индексов** (как в лабе 1).

Проверка вручную (готовый ответ на защите): грань `{0,4,1}`:
`(A − B0) × (B1 − B0) = (1, 1.5, 1) × (2, 0, 0) = (0, 2, −3)` → `/√13` → `(0, 0.555, −0.832)`, наружу и вверх ✓.
Основание `{0,1,2,3}`: `(2,0,0) × (2,0,2) = (0, −4, 0)` → `(0, −1, 0)`, вниз ✓.

### 7.3. Временная отладка

Вершинный шейдер передаёт нормаль дальше (`layout(location = 1) in vec3 in_normal` → `out vec3 v_normal`),
фрагментный выводит `out_color = vec4(normalize(v_normal) * 0.5 + 0.5, 1.0)`. UBO пока прежний (`mvp`).

✅ **Чекпоинт C:** каждая грань залита **одним** цветом, у граней разные цвета. Основание смотрит вниз, поэтому с
камерой по умолчанию его не видно. Чтобы проверить его цвет `(0.5, 0, 0.5)` (фиолетовый), опусти камеру ниже
основания: Position `y = −2`, Pitch `+20°`. `git commit -m "Lab2: procedural flat normals"`.

**Что нужно понимать:** зачем 16 вершин вместо 5 (§3.3); почему векторное произведение даёт нормаль наружу
только при правильном обходе; что такое атрибут вершины и `location`.

---

## 8. Этап D — Блинн-Фонг: направленный свет, ambient, материал (= оценка 3)

### 8.1. UBO и дескриптор

- Структура `Uniforms` из §4 вместо `{ mat4 mvp }`. Старый `static_assert(sizeof(Uniforms) == 64)` из лабы 1
  заменить на проверки из §4, иначе сборка упадёт. Строку `uniforms.mvp = ...` в `update()` убрать.
- В `createDescriptorSet()` поменять одну вещь: `stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT`.
  UBO теперь читает и фрагментный шейдер, без этого будет ошибка валидации при создании pipeline.
- Размер буфера и `range` уже записаны как `sizeof(Uniforms)` в коде лабы 1, они подстроятся сами.

### 8.2. Шейдеры

`pyramid.vert`:

```glsl
#version 450
/* блок Uniforms из §4 */
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 0) out vec3 v_world_position;
layout(location = 1) out vec3 v_normal;

void main() {
	vec4 world = u.model * vec4(in_position, 1.0);
	gl_Position = u.view_projection * world;
	v_world_position = world.xyz;
	// model is a pure rotation, so transpose(inverse(model)) == model (§3.3)
	v_normal = mat3(u.model) * in_normal;
}
```

`pyramid.frag`:

```glsl
#version 450
/* блок Uniforms из §4 */
layout(location = 0) in vec3 v_world_position;
layout(location = 1) in vec3 v_normal;
layout(location = 0) out vec4 out_color;

void main() {
	vec3 N = normalize(v_normal);
	vec3 V = normalize(u.camera_position - v_world_position);
	vec3 L = -u.light_direction;                 // к источнику

	vec3 color = u.ambient_color * u.ambient_intensity * u.albedo;

	float n_dot_l = max(dot(N, L), 0.0);
	if (n_dot_l > 0.0) {                         // свет падает с лицевой стороны
		vec3 H = normalize(L + V);
		float specular = pow(max(dot(N, H), 0.0), u.shininess);
		color += (u.albedo * n_dot_l + u.specular_color * specular) * u.light_color * u.light_intensity;
	}

	out_color = vec4(color, 1.0);
}
```

### 8.3. Параметры и UI

```cpp
namespace {
Camera camera;
math::vec3 ambient_color = {1.0f, 1.0f, 1.0f};
float ambient_intensity = 0.1f;
math::vec3 light_direction = {-0.4f, -1.0f, -0.3f};     // нормализуется в update()
math::vec3 light_color = {1.0f, 0.95f, 0.9f};
float light_intensity = 1.0f;
math::vec3 albedo = {0.8f, 0.3f, 0.2f};
math::vec3 specular_color = {0.5f, 0.5f, 0.5f};
float shininess = 64.0f;
float rotation_speed = 0.5f;
}
```

| Секция | Элементы |
|--------|----------|
| `Camera` | §6.3 |
| `Ambient` | `ColorEdit3 Color`, `SliderFloat Intensity 0…1` |
| `Directional light` | `DragFloat3 Direction` (шаг 0.02), `ColorEdit3 Color`, `SliderFloat Intensity 0…5` |
| `Material` | `ColorEdit3 Albedo`, `ColorEdit3 Specular`, `SliderFloat Shininess 1…256` с `ImGuiSliderFlags_Logarithmic \| ImGuiSliderFlags_AlwaysClamp` |
| (без секции, сверху) | `ColorEdit3 Background`, `SliderFloat Rotation speed` — как в лабе 1 |

Одинаковые подписи («Color», «Intensity») в разных секциях — это один и тот же ID в ImGui, и виджеты будут
мешать друг другу. Секции оборачивать в `ImGui::PushID("ambient")` / `ImGui::PopID()` или давать разные подписи
(`"Ambient color"`, `"Light color"`).

Shininess не меньше 1: при 0 получается `pow(0, 0)`, а это неопределённое значение.

Значения по умолчанию дают разную освещённость граней: `L = normalize(0.4, 1, 0.3) ≈ (0.36, 0.89, 0.27)`, и при
нулевом повороте `N·L` у боковых граней от 0.2 (грань −X) до 0.79 (грань +X). Основание смотрит вниз и освещено
только ambient.

### 8.4. `update()` и `render()`

Направление задаётся через `DragFloat3`, его можно сделать нулевым, и тогда `normalize` даст NaN (всё станет
чёрным). Поэтому:

```cpp
// Direction from the UI may be zero: fall back to "straight down" instead of NaN
math::vec3 safeDirection(math::vec3 d) {
	return math::length(d) < 1e-4f ? math::vec3{0.0f, -1.0f, 0.0f} : math::normalize(d);
}

// update(), после UI:
rotation_angle += delta_time * rotation_speed;
uniforms.model = math::rotateY(rotation_angle);
uniforms.view_projection = projection * view;            // §6.2
uniforms.camera_position = camera.position;
uniforms.ambient_color = ambient_color;
uniforms.ambient_intensity = ambient_intensity;
uniforms.light_direction = safeDirection(light_direction);
uniforms.light_intensity = light_intensity;
uniforms.light_color = light_color;
uniforms.albedo = albedo;
uniforms.specular_color = specular_color;
uniforms.shininess = shininess;
```

`render()` не меняется: `uploadToBuffer(uniform_buffer, &uniforms, sizeof(uniforms))` до записи команд, затем
то же рисование, что в лабе 1. `shutdown()` тоже не меняется: новых объектов Vulkan нет.

✅ **Чекпоинт D = оценка 3:**
- грани пирамиды освещены по-разному, рёбра чёткие;
- на гранях, повёрнутых к свету, виден блик, и он сдвигается при движении камеры; диффузная часть при этом не
  меняется;
- `Shininess` меняет размер блика, `Ambient` подсвечивает теневые грани и основание;
- направление, цвет и интенсивность света, все параметры материала меняются на лету;
- ресайз, сворачивание и закрытие окна без `Validation Error`.

`git commit -m "Lab2: Blinn-Phong directional light"`.

**Что нужно понимать:** каждое слагаемое §3.2 и его смысл; зачем шейдеру `camera_position`; почему блик зависит
от камеры, а диффузная часть нет; почему всё считается в мировых координатах; почему свет считаем во фрагментном
шейдере (нормаль и позиция интерполируются, и освещение вычисляется в каждой точке грани, а не только в
вершинах); зачем UBO теперь виден фрагментному шейдеру (`stageFlags`).

---

## 9. Типичные ловушки

| Симптом | Причина / что делать |
|---------|---------------------|
| Всё чёрное | Направление света нулевое → NaN (§8.4); интенсивность 0; поля UBO съехали (§4) |
| Цвета/свет «странные», меняется не тот параметр | C++-структура не совпадает с std140: нет `_pad` → `static_assert` (§4) |
| `Validation Error` при создании pipeline про stage / descriptor | `stageFlags` у binding 0 без `FRAGMENT_BIT` (§8.1) |
| Грани сглажены, нет чётких рёбер | Общие вершины (5 вместо 16) — §3.3 |
| Освещены не те грани / «изнутри» | Нормали внутрь (порядок обхода) или `L = +direction` вместо `−direction` |
| Блик на неосвещённой стороне | Нет проверки `N·L > 0` перед бликом |
| Блик не двигается при движении камеры | В шейдер не передаётся `camera_position` или `V` считается без неё |
| NaN-пятна при shininess 0 | `pow(0, 0)` → минимум слайдера 1 и `AlwaysClamp` |
| Меняешь «Color» в одной секции, меняется в другой | Одинаковые подписи без `PushID` (§8.3) |
| Картинка переворачивается при большом наклоне | pitch не ограничен ±89° |
| Слишком светло, «выцветшие» цвета | sRGB swapchain (§3.4) — это нормально, уменьшить ambient |
| `multiple definition of 'math::…'` | Функции в `math.hpp` без `inline` (§5) |
| `Failed to open shaders/…spv` | Запуск не из корня проекта |

---

## 10. README, отчёт, git

### 10.1. Git

- Ветка `lab2` уже есть (коммит `prepare lab2`). По коммиту на каждый чекпоинт A–D.
- В конце: `git push -u origin lab2`, затем слить в `master` (лаба 1 туда тоже ещё не слита).

### 10.2. `lab2/README.md` (как [lab1/README.md](../lab1/README.md))

- «Лабораторная работа №2. Камера и освещение» + условие + «выполнено базовое задание (оценка 3)»;
- что сделано: свободная камера, процедурные нормали, Блинн-Фонг, направленный свет, ambient, материал;
- сборка и запуск (на Fedora: `cmake --preset debug`; ссылка на lab1/README для Ubuntu/Mint);
- таблица элементов UI;
- 2 скриншота в `lab2/screenshots/`.

### 10.3. Отчёт по образцу

```bash
cp "lab1/3XX_Фамилия_ЛабX.docx" "lab2/3XX_<Фамилия>_Лаб2.docx"
```

| Часть | Что вписать |
|-------|-------------|
| Титул | «Лабораторная работа №**2**», тема «Камера и освещение» |
| **Условие** | Цель из PDF, «выполнено базовое задание» |
| **Метод решения** | Камера (view как обратная матрица, §6.2); нормали через векторное произведение (§7.2); Блинн-Фонг (§3.2); направленный источник; почему normal matrix = поворот; раскладка UBO std140 (§4) |
| **Результаты** | Скриншоты: (1) пирамида по умолчанию; (2) другой shininess / цвет света; (3) другой ракурс камеры |
| **Выводы** | «Я научился…»: камера, нормали, модель освещения Блинн-Фонга, выравнивание std140 |

`libreoffice --headless --convert-to pdf --outdir lab2 "lab2/3XX_<Фамилия>_Лаб2.docx"`

### 10.4. Финальная проверка

- [ ] `rm -rf build-debug && cmake --preset debug && cmake --build build-debug --parallel` — с нуля без ошибок.
- [ ] Минута работы + ресайз + сворачивание + закрытие — нет `Validation Error`.
- [ ] Нет временной проверки математики (§5) и отладочного вывода нормалей (§7.3).
- [ ] README, отчёт PDF, скриншоты в `lab2/`.

---

## 11. Подготовка к защите

Для 3 достаточно ответить правильно **на один** вопрос из 3–4, но вопросы случайные, поэтому подготовь все
пункты, по 2–3 предложения на каждый.

**Освещение и математика**
1. Формула Блинн-Фонга: ambient, diffuse, specular; смысл `N·L`; что такое `H`; чем отличается от Фонга.
2. Что задаёт `shininess` и почему с ростом блик становится меньше и резче.
3. Направленный источник: почему `L = −direction` одинаковый для всех точек и почему нет затухания.
4. Нормали: как сгенерированы (векторное произведение, порядок обхода); почему 16 вершин, а не 5.
5. Normal matrix: почему в общем случае не матрица модели; почему у нас хватает `mat3(model)`.
6. Зачем `normalize(v_normal)` во фрагментном шейдере.
7. Камера: view как обратная матрица; порядок `rotateX(−pitch)·rotateY(−yaw)·translate(−p)`; формула `forward`.
8. Почему освещение в мировых координатах и зачем шейдеру `camera_position`.
9. Почему освещение во фрагментном, а не в вершинном шейдере.

**Vulkan и код**
10. Как данные попадают в шейдер: структура C++ → `VkBuffer` → descriptor set → `uniform` в GLSL.
11. Раскладка `Uniforms`: `vec3` занимает 12 байт, но начинается только с адреса, кратного 16; поэтому после
    `vec3` помещается один `float`, а если его нет — где и зачем `_pad`; почему в GLSL их нет.
12. Зачем `stageFlags` с `FRAGMENT_BIT`.
13. Vertex input: два атрибута, `location`, `offsetof`, stride.
14. Порядок кадра: что в `update()`, что в `render()` и почему UBO копируется в `render()` (лаба 1, §3.4).
15. Почему функции в `math.hpp` помечены `inline`.

Совет из лабы 1: после каждого этапа объясни свой код вслух. Если где-то «ну, так работает», разберись, прежде
чем идти дальше.

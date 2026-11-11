# Лабораторная работа №2 «Камера и освещение» — план (оценка 4)

**Цель: оценка 4 = базовое задание + дополнительные задания 1, 2, 3.** Задание 4 (визуализация источников) не делаем.

> Источники: условие [Lab #2 Camera and Lighting.pdf](Lab%20%232%20Camera%20and%20Lighting.pdf), сообщение
> преподавателя [read.txt](read.txt) (статья <https://learnopengl.com/Lighting/Light-casters>), код лабы 1
> (ветка `lab1`, коммит `40bd0d7`), образец отчёта [lab1/3XX_Фамилия_ЛабX.docx](../lab1/3XX_Фамилия_ЛабX.docx).

### Что проверено по коду лабы 1 (11.11)

| Проверка | Результат |
|----------|-----------|
| Сборка на текущей машине (Fedora): системные заголовки Vulkan **1.4.350** в `/usr/include` | ✅ хватает пресета `debug`, `CMakeUserPresets.json` не нужен |
| Кадров «в полёте» | **один** (одна fence `vk_fence_frame_in_flight` в [graphics_internal.cpp:733](../source/graphics_internal.cpp#L733)) → в `render()` после `prepare()` GPU ничего не читает, буферы и дескрипторы можно менять |
| Формат swapchain | `use_default_format_selection()` у vk-bootstrap → **`B8G8R8A8_SRGB`**: шейдер пишет линейный цвет, гамма-коррекцию делает железо |
| Что переиспользуем | `math::mat4`, `translate`, `rotateX/Y`, `perspective`, `radians` ([math.hpp](../source/math.hpp)); `createBuffer/uploadToBuffer/destroyBuffer/createShaderModule` ([graphics.cpp](../source/graphics.cpp)); pipeline и запись команд из [application.cpp](../source/application.cpp) |

Код пишешь **сам**. Фрагменты ниже нужны, чтобы свериться, а не чтобы их копировать.

---

## Оглавление

0. [Оценка и сдача](#0-оценка-и-сдача)
1. [Что нужно сделать](#1-что-нужно-сделать)
2. [Что меняется относительно лабы 1](#2-что-меняется-относительно-лабы-1)
3. [Теория: Блинн-Фонг и типы источников (по learnopengl)](#3-теория-блинн-фонг-и-типы-источников)
4. [Раскладка данных для GPU (std140 / std430)](#4-раскладка-данных-для-gpu)
5. [Этап A — математика vec3](#5-этап-a--математика-vec3)
6. [Этап B — свободная камера](#6-этап-b--свободная-камера)
7. [Этап C — нормали, процедурная генерация](#7-этап-c--нормали-процедурная-генерация)
8. [Этап D — два набора дескрипторов, пирамида + пол (доп. 1)](#8-этап-d--два-набора-дескрипторов-доп-1)
9. [Этап E — Блинн-Фонг: направленный свет + ambient (база)](#9-этап-e--блинн-фонг-направленный-свет--ambient)
10. [Этап F — точечные источники в storage-буфере (доп. 2)](#10-этап-f--точечные-источники-доп-2)
11. [Этап G — прожекторы с мягкими краями (доп. 3) = оценка 4](#11-этап-g--прожекторы-доп-3--оценка-4)
12. [Типичные ловушки](#12-типичные-ловушки)
13. [README, отчёт, git](#13-readme-отчёт-git)
14. [Подготовка к защите](#14-подготовка-к-защите)

---

## 0. Оценка и сдача

- **Оценка 4 = базовое задание + доп. задания 1–3** (PDF, раздел «Оценивание»).
- **На оценку 4 нужно правильно ответить на ВСЕ 3–4 вопроса** (read.txt лабы 1). В лабе 1 для тройки хватало
  одного ответа, здесь так не получится. Поэтому блоки **«Что нужно понимать»** после каждого этапа обязательны,
  а §14 нужно выучить целиком.
- Максимальная оценка снижается на балл каждые две недели после выдачи. Дату выдачи лабы 2 проверь в чате
  преподавателя: если с неё прошло больше 4 недель, выше 3 уже не поставят.
- Vulkan + C++, ImGui. Подписи в ImGui только на английском: встроенный шрифт не содержит кириллицы.

---

## 1. Что нужно сделать

### 1.1. Базовое задание (без него не будет и тройки)

- Пирамида из лабы 1 рисуется **относительно камеры**. Камера управляется из ImGui: **позиция, поворот, угол обзора**.
- У вершин есть **атрибут нормали**, нормали **генерируются процедурно**: считаются в коде, а не вписываются руками.
- Один **направленный** источник и **ambient**. Освещение по **Блинн-Фонгу во фрагментном шейдере**.
- Из UI меняются: параметры камеры, направление/цвет/интенсивность источника, ambient,
  **материал** (альбедо, цвет блика, коэффициент блика).

### 1.2. Дополнительные задания 1–3

| № | Что | Где в плане |
|---|-----|-------------|
| 1 | Два набора дескрипторов: **set 0 = сцена** (UBO: view-projection, камера, ambient, направленный свет, счётчики; плюс storage-буферы источников), **set 1 = модель** (UBO: матрица модели и материал) | §8 |
| 2 | Массив **точечных** источников в storage-буфере, затухание **1/d²**, в UI добавить/удалить/изменить | §10 |
| 3 | Массив **прожекторов** в storage-буфере, 1/d², **мягкие края** (внутренний и внешний углы), в UI добавить/удалить/изменить | §11 |

Общие требования к storage-буферам: в шейдере массив **без размера** (`PointLight point_lights[];`), поэтому
максимальное число источников в шейдере не задано. На стороне C++ буфер растёт при добавлении источников.

### 1.3. Решения по объёму (согласованы)

- **Камера управляется только через ImGui**: позиция `DragFloat3`, yaw/pitch слайдерами, FOV. Без WASD и мыши,
  `main.cpp` не трогаем.
- **Сцена: вращающаяся пирамида + пол.** Пол нужен, чтобы были видны пятна прожекторов и затухание. Два объекта
  показывают, зачем нужен отдельный set 1 на каждую модель.
- Не делаем: доп. задание 4 (кубики и конусы на месте источников), текстуры, тени.

Итог по объёму: 1 новый файл C++ (`mesh.hpp/.cpp`), 2 новых шейдера, заметно доработанный `application.cpp`.
Примерно +450 строк своего кода.

---

## 2. Что меняется относительно лабы 1

| Было (лаба 1) | Станет (лаба 2) |
|---------------|-----------------|
| Камера: `translate(0,0,−distance)·rotateX(tilt)`, смотрит на центр | Свободная камера: позиция + yaw + pitch + FOV |
| `Vertex {position, color}`, 5 вершин | `Vertex {position, normal}`, **16 вершин** у пирамиды (нормали граней), 4 у пола |
| Один UBO `{mat4 mvp}`, один set | **set 0**: `SceneUniforms` (UBO) + 2 storage-буфера; **set 1**: `ModelUniforms` (UBO) у каждого объекта |
| Один объект | Массив объектов `{mesh, позиция, вращение, материал, свой UBO, свой descriptor set}` |
| `pyramid.vert/frag`: цвет вершины | `lit.vert/frag`: мировые позиция и нормаль, Блинн-Фонг по всем источникам |
| UI: 5 слайдеров | UI: секции Camera / Scene / Directional light / Objects / Point lights / Spot lights |

### 2.1. Структура файлов

```
source/
  math.hpp            ← + vec3, dot, cross, length, normalize
  mesh.hpp / mesh.cpp ← НОВЫЙ: Vertex, MeshData, buildFlatMesh, makePyramid, makeFloor
  graphics.hpp/.cpp   ← + writeBufferDescriptor (одна функция-помощник)
  application.cpp     ← структуры для GPU, камера, объекты, источники, UI, дескрипторы, pipeline, рисование
shaders/
  lit.vert, lit.frag  ← вместо pyramid.vert/frag (их удалить: код лабы 1 остаётся в ветке lab1)
```

Правки [CMakeLists.txt](../CMakeLists.txt):

```cmake
# строка 9:
add_executable(${PROJECT_NAME} source/main.cpp source/graphics_internal.cpp source/application.cpp
               source/graphics.cpp source/mesh.cpp)

# вместо compile_shader(pyramid.*):
	compile_shader(lit.vert)
	compile_shader(lit.frag)
```

Сборка на этой машине (Fedora): `cmake --preset debug && cmake --build build-debug --parallel`, запуск из корня:
`./build-debug/vulkan-starter-app`.

### 2.2. Порядок данных в кадре (как в лабе 1, §3.4 п.2)

- `update()`: UI, затем пересчёт **всех** данных на CPU (матрицы, массивы источников для GPU).
- `render()`: после `prepare()` прошлый кадр на GPU уже закончен. Здесь (1) при необходимости увеличить
  storage-буферы и обновить дескрипторы, (2) скопировать данные во все буферы, (3) записать команды.

---

## 3. Теория: Блинн-Фонг и типы источников

Статья преподавателя: <https://learnopengl.com/Lighting/Light-casters>. Ниже то, что из неё берём, и **чем
наша реализация от неё отличается**.

### 3.1. Обозначения (всё в **мировых** координатах)

- `N` — нормаль поверхности (единичная).
- `V = normalize(camera_position − P)` — от точки `P` к камере.
- `L` — единичный вектор **от точки к источнику**.
- `H = normalize(L + V)` — **halfway-вектор** (в этом и состоит отличие Блинн-Фонга от Фонга).
- Материал: `albedo` (диффузный цвет), `specular_color` (цвет блика), `shininess` (коэффициент блика, ≥ 1).

### 3.2. Формула для одного источника

```
diffuse  = albedo · max(N·L, 0)
specular = specular_color · max(N·H, 0)^shininess          (только если N·L > 0)
вклад    = (diffuse + specular) · radiance
radiance = color · intensity · attenuation · cone
итог     = ambient_color · ambient_intensity · albedo  +  Σ вкладов всех источников
```

Фонг использует `R = reflect(−L, N)` и `(R·V)^n`. Блинн-Фонг заменяет это на `(N·H)^n`: так дешевле, и блик не
обрывается резко, когда угол между `V` и `R` больше 90°.

### 3.3. Типы источников

| Тип | `L` | `attenuation` | `cone` |
|-----|-----|---------------|--------|
| Направленный | `−direction` (одинаковый для всех точек) | 1 | 1 |
| Точечный | `normalize(position − P)` | **`1 / d²`**, `d = |position − P|` | 1 |
| Прожектор | как у точечного | **`1 / d²`** | `clamp((cos θ − cos_outer) / (cos_inner − cos_outer), 0, 1)`, `cos θ = dot(−L, direction)` |

**Отличие от learnopengl:** в статье затухание `1 / (Kc + Kl·d + Kq·d²)`. По условию нужен **закон обратных
квадратов**, то есть `1/d²` (физически правильно: та же энергия распределяется по сфере площадью `4πd²`). При `d → 0`
получается бесконечность, поэтому `d²` ограничиваем снизу: `max(d², 1e-4)`.

**Прожектор** (как в статье): углы задаются **от оси** конуса (половина раствора). В буфер передаём **косинусы**:
сравнивать косинусы дешевле, чем считать `acos` в каждом фрагменте. Косинус убывает на [0°, 90°], поэтому «внутри
конуса» означает `cos θ > cos_outer`. Если `outer == inner`, знаменатель равен 0, и в этом случае берём жёсткий край
`step(cos_outer, cos θ)`. В UI `outer` не может быть меньше `inner`.

### 3.4. Нормали и normal matrix

- Нормаль переводится в мировые координаты **не матрицей модели**, а `transpose(inverse(mat3(model)))`. При
  неравномерном масштабе матрица модели «наклоняет» нормаль, и она перестаёт быть перпендикулярной грани.
- У нас `model = translate(position) · rotateY(angle)`, масштаба нет. Перенос на направления не действует, а для
  поворота `R` выполняется `transpose(inverse(R)) = R`. Поэтому **normal matrix = `rotateY(angle)`**, и на CPU она
  считается без обращения матрицы.
- После интерполяции между вершинами длина нормали меньше 1, поэтому во фрагментном шейдере делаем `normalize(v_normal)`.

### 3.5. Плоское (flat) освещение граней

Если у пирамиды 5 вершин, апекс общий для 4 граней, и у него может быть только одна нормаль, то есть грани
сгладятся. Чтобы грань освещалась как плоскость, **у каждой грани свои вершины** с нормалью этой грани:
4 боковые × 3 + основание 4 = **16 вершин**, 18 индексов.

### 3.6. sRGB

Swapchain `B8G8R8A8_SRGB`: шейдер выдаёт **линейную** яркость, при записи железо применяет гамма-кривую. Поэтому
ambient `0.05` на экране выглядит ярче, чем «5 % белого». Это нормально, исправлять не нужно, но на вопрос
«почему так светло» нужно уметь ответить.

---

## 4. Раскладка данных для GPU

Калькулятор из read.txt лабы 1: <https://jackjamison.net/interactive/glsl-layout-calculator/>. UBO всегда
**std140**, storage-буфер объявляем **std430**. Поля упорядочены так, что `vec3 + float/uint` занимает ровно 16
байт и неявного выравнивания почти нет. Каждую структуру C++ проверяем `static_assert(sizeof)` и для пары полей
`offsetof`.

```cpp
// set 0, binding 0 — uniform Scene (std140)
struct SceneUniforms {
	math::mat4 view_projection;        //   0
	math::vec3 camera_position;        //  64
	uint32_t   point_light_count;      //  76
	math::vec3 ambient_color;          //  80
	float      ambient_intensity;      //  92
	math::vec3 dir_light_direction;    //  96  куда светит, единичный
	float      dir_light_intensity;    // 108
	math::vec3 dir_light_color;        // 112
	uint32_t   spot_light_count;       // 124
};                                     // 128
static_assert(sizeof(SceneUniforms) == 128);
static_assert(offsetof(SceneUniforms, ambient_color) == 80);

// set 1, binding 0 — uniform Model (std140), у каждого объекта свой
struct ModelUniforms {
	math::mat4 model;                  //   0
	math::mat4 normal_matrix;          //  64  mat4, а не mat3: в std140 mat3 = 3 столбца по 16 байт, легко ошибиться
	math::vec3 albedo;                 // 128
	float      shininess;              // 140
	math::vec3 specular_color;         // 144
	float      _pad;                   // 156  блок std140 округляется до 16 → 160
};                                     // 160
static_assert(sizeof(ModelUniforms) == 160);

// set 0, binding 1 — элемент массива точечных источников (std430)
struct PointLight {
	math::vec3 position;               //  0
	float      intensity;              // 12
	math::vec3 color;                  // 16
	float      _pad;                   // 28  выравнивание структуры = 16 (из-за vec3) → размер 32
};
static_assert(sizeof(PointLight) == 32);

// set 0, binding 2 — элемент массива прожекторов (std430)
struct SpotLight {
	math::vec3 position;               //  0
	float      intensity;              // 12
	math::vec3 direction;              // 16  единичный, ось конуса
	float      cos_inner;              // 28
	math::vec3 color;                  // 32
	float      cos_outer;              // 44
};                                     // 48
static_assert(sizeof(SpotLight) == 48);
```

GLSL-объявления те же (в GLSL поля `_pad` не пишем: компилятор сам добавляет выравнивание, а в C++ его нужно
вписать руками, как и говорил преподаватель).

**Почему счётчики источников передаются в UBO, а не через `point_lights.length()`:** буфер выделен «с запасом»
(capacity ≥ count), и `.length()` вернул бы ёмкость, а не число живых источников.

---

## 5. Этап A — математика vec3

В [math.hpp](../source/math.hpp) дописать (стиль тот же, что у `mat4`):

```cpp
struct vec3 {
	float x = 0.0f, y = 0.0f, z = 0.0f;
};
static_assert(sizeof(vec3) == 12);       // три float подряд, иначе раскладка §4 съедет

vec3 operator+(vec3 a, vec3 b);  vec3 operator-(vec3 a, vec3 b);
vec3 operator-(vec3 a);          vec3 operator*(vec3 a, float s);
float dot(vec3 a, vec3 b);       // ax·bx + ay·by + az·bz
vec3  cross(vec3 a, vec3 b);     // (ay·bz − az·by,  az·bx − ax·bz,  ax·by − ay·bx)
float length(vec3 v);            // sqrt(dot(v, v))
vec3  normalize(vec3 v);         // v * (1 / length(v)); нулевой вектор сюда не передавать (§9.4)
```

Перегрузка `translate(vec3)` удобна, но не обязательна.

**Проверка** (временно в `initialize()`, потом удалить):
- `cross({1,0,0}, {0,1,0}) = (0,0,1)`;
- `normalize(cross(A − B0, B1 − B0))` для пирамиды = `(0, 0.5547, −0.8321)` (см. §7.2).

✅ **Чекпоинт A:** числа совпадают. `git commit -m "Lab2: vec3 math"`.

**Что нужно понимать:** скалярное произведение = `|a||b|cos α` (отсюда `N·L` = косинус угла падения);
векторное произведение перпендикулярно обоим векторам, направление по правилу правой руки, поэтому знак нормали
зависит от порядка обхода.

---

## 6. Этап B — свободная камера

### 6.1. Параметры

```cpp
struct Camera {
	math::vec3 position = {0.0f, 2.0f, 6.0f};
	float yaw_deg = 0.0f;       // поворот вокруг Y; 0 → смотрит вдоль −Z; > 0 → влево
	float pitch_deg = -18.0f;   // наклон вверх/вниз, [−89, 89]
	float fov_deg = 60.0f;
};
```

### 6.2. Матрица вида = обратная к трансформации камеры

Камера в мире = `translate(p) · rotateY(yaw) · rotateX(pitch)`. Вид — обратная матрица (обратная к произведению =
произведение обратных в обратном порядке, обратная к повороту = поворот на −угол):

```
view = rotateX(−pitch) · rotateY(−yaw) · translate(−p)
view_projection = perspective(fov, aspect, 0.1, 100) · view
```

Направление взгляда (пригодится для проверки): `forward = (−sin yaw · cos pitch,  sin pitch,  −cos yaw · cos pitch)`.

**Проверка:** `view · (p, 1) = (0, 0, 0, 1)`; `view · (p + forward, 1) = (0, 0, −1, 1)`.

### 6.3. UI (секция `Camera`)

```cpp
ImGui::DragFloat3("Position", &camera.position.x, 0.05f);
ImGui::SliderFloat("Yaw", &camera.yaw_deg, -180.0f, 180.0f);
ImGui::SliderFloat("Pitch", &camera.pitch_deg, -89.0f, 89.0f);
ImGui::SliderFloat("FOV", &camera.fov_deg, 20.0f, 120.0f);
```

На этом этапе всё остальное как в лабе 1: в `uniforms.mvp = view_projection · model` подставляется новая камера.
Слайдеры Distance и Tilt убрать.

✅ **Чекпоинт B:** камеру можно сдвинуть и повернуть, пирамида остаётся на месте мира, при повороте
yaw > 0 сцена уходит вправо. `git commit -m "Lab2: free camera"`.

**Что нужно понимать:** почему view — это обратная матрица (двигаем не камеру, а весь мир в противоположную
сторону); почему порядок `rotateX · rotateY · translate`, а не наоборот; зачем ограничивать pitch ±89° (на ±90°
взгляд вертикален, дальше картинка переворачивается).

---

## 7. Этап C — нормали, процедурная генерация

### 7.1. Новый формат вершины (`mesh.hpp`)

```cpp
struct Vertex {
	math::vec3 position;
	math::vec3 normal;
};

struct MeshData {
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
};

// faces: каждая грань — выпуклый многоугольник из индексов углов, обход против часовой стрелки снаружи
MeshData buildFlatMesh(std::span<const math::vec3> corners,
                       std::span<const std::vector<uint32_t>> faces);
MeshData makePyramid();          // 16 вершин, 18 индексов
MeshData makeFloor(float half);  // 4 вершины, 6 индексов
```

Атрибуты в pipeline: `location 0` — `position`, `location 1` — `normal`, оба `VK_FORMAT_R32G32B32_SFLOAT`,
`offsetof(Vertex, …)`.

### 7.2. Генерация нормалей (это и есть «процедурно»)

```cpp
MeshData buildFlatMesh(std::span<const math::vec3> corners, std::span<const std::vector<uint32_t>> faces) {
	MeshData mesh;
	for (const auto& face : faces) {
		const math::vec3 a = corners[face[0]], b = corners[face[1]], c = corners[face[2]];
		const math::vec3 normal = math::normalize(math::cross(b - a, c - a));   // CCW снаружи → наружу

		const uint32_t first = uint32_t(mesh.vertices.size());
		for (uint32_t corner : face) {
			mesh.vertices.push_back({corners[corner], normal});
		}
		for (uint32_t i = 1; i + 1 < face.size(); ++i) {                         // веер: (0,1,2), (0,2,3)...
			mesh.indices.insert(mesh.indices.end(), {first, first + i, first + i + 1});
		}
	}
	return mesh;
}
```

**Пирамида** — углы и грани из лабы 1:

```
corners: B0(−1,−0.75,−1)  B1(1,−0.75,−1)  B2(1,−0.75,1)  B3(−1,−0.75,1)  A(0,0.75,0)
faces:   {0,4,1} {1,4,2} {2,4,3} {3,4,0}  {0,1,2,3}
```

Проверка вручную (готовый ответ на защите): грань `{0,4,1}`:
`(A − B0) × (B1 − B0) = (1, 1.5, 1) × (2, 0, 0) = (0, 2, −3)` → `/√13` → `(0, 0.555, −0.832)`, наружу и вверх ✓.
Основание `{0,1,2,…}`: `(2,0,0) × (2,0,2) = (0, −4, 0)` → `(0, −1, 0)`, вниз ✓.

**Пол** — квадрат в плоскости `y = 0`, обход CCW, если смотреть сверху:
`(−h,0,−h) (−h,0,h) (h,0,h) (h,0,−h)`, грань `{0,1,2,3}`. Проверка:
`(0,0,2h) × (2h,0,2h) = (0, 4h², 0)` → вверх ✓. `h = 10`.

### 7.3. Mesh на GPU

```cpp
struct Mesh {
	graphics::Buffer vertex_buffer;
	graphics::Buffer index_buffer;
	uint32_t index_count = 0;
};
Mesh createMesh(const MeshData& data);   // два graphics::createBuffer(...), как в лабе 1
void destroyMesh(Mesh& mesh);
```

### 7.4. Временная отладка

`lit.vert` передаёт нормаль дальше, `lit.frag` выводит `out_color = vec4(normalize(v_normal) * 0.5 + 0.5, 1.0)`.
Пока пол не нужен: рисуем только пирамиду со старым `mvp`.

✅ **Чекпоинт C:** каждая грань пирамиды залита **одним** цветом, и цвета у граней разные (у основания
`(0.5, 0, 0.5)`, фиолетовый). `git commit -m "Lab2: procedural flat normals"`.

**Что нужно понимать:** зачем 16 вершин вместо 5 (§3.5); почему векторное произведение даёт нормаль наружу
только при правильном обходе; что такое атрибут вершины и `location`.

---

## 8. Этап D — два набора дескрипторов (доп. 1)

### 8.1. Объекты сцены

```cpp
struct Material {
	math::vec3 albedo;
	math::vec3 specular_color;
	float shininess;
};

struct Object {
	const char* name;
	Mesh* mesh;
	math::vec3 position;
	float rotation_speed;              // рад/с вокруг Y
	float angle = 0.0f;
	Material material;
	graphics::Buffer uniform_buffer;   // ModelUniforms
	VkDescriptorSet descriptor_set;    // set 1 этого объекта
	ModelUniforms uniforms;
};

Mesh pyramid_mesh, floor_mesh;
Object objects[] = {
	{.name = "Pyramid", .mesh = &pyramid_mesh, .position = {0, 0.75f, 0}, .rotation_speed = 0.5f,
	 .material = {{0.8f, 0.3f, 0.2f}, {0.5f, 0.5f, 0.5f}, 64.0f}},
	{.name = "Floor", .mesh = &floor_mesh, .position = {0, 0, 0}, .rotation_speed = 0.0f,
	 .material = {{0.6f, 0.6f, 0.6f}, {0.2f, 0.2f, 0.2f}, 16.0f}},
};
```

Пирамида поднята на 0.75, поэтому основание лежит на полу (`y = 0`). Z-fighting не будет: снизу пол отсекается
(он смотрит вверх), а сверху отсекается основание (оно смотрит вниз).

### 8.2. Объекты Vulkan (в `initialize()`, по порядку)

| Объект | Содержимое |
|--------|-----------|
| `scene_set_layout` | binding 0: `UNIFORM_BUFFER`, `VERTEX \| FRAGMENT`. **На этапе F** добавится binding 1, **на этапе G** — binding 2 (`STORAGE_BUFFER`, `FRAGMENT`) |
| `model_set_layout` | binding 0: `UNIFORM_BUFFER`, `VERTEX \| FRAGMENT` |
| `pipeline_layout` | `setLayoutCount = 2`, `pSetLayouts = {scene_set_layout, model_set_layout}`. **Порядок = номер set** |
| `descriptor_pool` | `maxSets = 1 + objects`, pool sizes: `UNIFORM_BUFFER × (1 + objects)`, позже `STORAGE_BUFFER × 2` |
| `scene_uniform_buffer` | `createBuffer(sizeof(SceneUniforms), UNIFORM_BUFFER_BIT, nullptr)` |
| `scene_descriptor_set` | `vkAllocateDescriptorSets` с `scene_set_layout` |
| для каждого объекта | `uniform_buffer` + `descriptor_set` с `model_set_layout` |

Связывание буфера и binding повторяется 3–5 раз, поэтому выносим в [graphics.cpp](../source/graphics.cpp):

```cpp
// Points binding of the set to the whole buffer
void writeBufferDescriptor(VkDescriptorSet set, uint32_t binding, VkDescriptorType type, VkBuffer buffer) {
	const VkDescriptorBufferInfo info = {.buffer = buffer, .offset = 0, .range = VK_WHOLE_SIZE};
	const VkWriteDescriptorSet write = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = set,
		.dstBinding = binding,
		.descriptorCount = 1,
		.descriptorType = type,
		.pBufferInfo = &info,
	};
	vkUpdateDescriptorSets(internal::context.device, 1, &write, 0, nullptr);
}
```

### 8.3. Шейдеры (каркас)

```glsl
// lit.vert и lit.frag — одинаковые объявления блоков
layout(set = 0, binding = 0) uniform Scene {
	mat4 view_projection;
	vec3 camera_position;     uint  point_light_count;
	vec3 ambient_color;       float ambient_intensity;
	vec3 dir_light_direction; float dir_light_intensity;
	vec3 dir_light_color;     uint  spot_light_count;
} scene;

layout(set = 1, binding = 0) uniform Model {
	mat4 model;
	mat4 normal_matrix;
	vec3 albedo;          float shininess;
	vec3 specular_color;
} object;
```

`lit.vert`:

```glsl
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 0) out vec3 v_world_position;
layout(location = 1) out vec3 v_normal;

void main() {
	vec4 world = object.model * vec4(in_position, 1.0);
	gl_Position = scene.view_projection * world;
	v_world_position = world.xyz;
	v_normal = mat3(object.normal_matrix) * in_normal;
}
```

Временный `lit.frag`: `out_color = vec4(object.albedo * (0.5 + 0.5 * normalize(v_normal).y), 1.0);`. Так видно,
что материал приходит из set 1.

### 8.4. `update()` и `render()`

```cpp
// update(): для каждого объекта
object.angle += delta_time * object.rotation_speed;
const math::mat4 rotation = math::rotateY(object.angle);
object.uniforms.model = math::translate(object.position.x, object.position.y, object.position.z) * rotation;
object.uniforms.normal_matrix = rotation;                 // §3.4
object.uniforms.albedo = object.material.albedo;  /* ... specular_color, shininess ... */

// render(): копирование (после prepare())
graphics::uploadToBuffer(scene_uniform_buffer, &scene_uniforms, sizeof scene_uniforms);
for (const Object& object : objects) graphics::uploadToBuffer(object.uniform_buffer, &object.uniforms, sizeof object.uniforms);

// render(): рисование
vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout,
                        0, 1, &scene_descriptor_set, 0, nullptr);          // set 0 — один раз за кадр
for (const Object& object : objects) {
	vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout,
	                        1, 1, &object.descriptor_set, 0, nullptr);     // firstSet = 1 — set 0 не трогается
	vkCmdBindVertexBuffers(cb, 0, 1, &object.mesh->vertex_buffer.buffer, &offset);
	vkCmdBindIndexBuffer(cb, object.mesh->index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(cb, object.mesh->index_count, 1, 0, 0, 0);
}
```

### 8.5. UI (секция `Objects`)

Для каждого объекта: `ImGui::PushID(i)`, `TreeNode(object.name)`, внутри `DragFloat3 Position`,
`SliderFloat Rotation speed`, `ColorEdit3 Albedo`, `ColorEdit3 Specular`,
`SliderFloat Shininess` в диапазоне 1…256 с `ImGuiSliderFlags_Logarithmic`. Затем `TreePop`, `PopID`.

### 8.6. `shutdown()` (финальный список с учётом этапов F, G)

```
vkQueueWaitIdle
vkDestroyPipeline, vkDestroyPipelineLayout
vkDestroyDescriptorPool                      ← освобождает все наборы
vkDestroyDescriptorSetLayout × 2
destroyBuffer: scene_uniform_buffer, point_light_buffer, spot_light_buffer, uniform_buffer каждого объекта
destroyMesh: pyramid_mesh, floor_mesh
```

✅ **Чекпоинт D:** пирамида стоит на сером полу и вращается, цвет каждого объекта меняется из его секции UI
независимо от другого. Нет `Validation Error`. `git commit -m "Lab2: scene and model descriptor sets"`.

**Что нужно понимать:** set и binding; почему данные разделены по частоте обновления (set 0 — раз за кадр,
set 1 — на каждый объект); почему при `firstSet = 1` set 0 остаётся привязанным (совместимость pipeline layout);
как считаются размеры пула; почему UBO и descriptor set у каждого объекта свой (а не один, перезаписываемый между
draw-вызовами: команды выполняются позже, чем записываются, и GPU увидел бы только последнее значение).

---

## 9. Этап E — Блинн-Фонг: направленный свет + ambient

### 9.1. `lit.frag`

```glsl
#version 450
/* ... блоки Scene и Model, §8.3 ... */
layout(location = 0) in vec3 v_world_position;
layout(location = 1) in vec3 v_normal;
layout(location = 0) out vec4 out_color;

// L — к источнику, radiance — цвет · интенсивность · затухание
vec3 blinnPhong(vec3 N, vec3 V, vec3 L, vec3 radiance) {
	float n_dot_l = max(dot(N, L), 0.0);
	if (n_dot_l == 0.0) {
		return vec3(0.0);                 // источник за поверхностью — ни диффузного, ни блика
	}
	vec3 H = normalize(L + V);
	float specular = pow(max(dot(N, H), 0.0), object.shininess);
	return (object.albedo * n_dot_l + object.specular_color * specular) * radiance;
}

void main() {
	vec3 N = normalize(v_normal);
	vec3 V = normalize(scene.camera_position - v_world_position);

	vec3 color = scene.ambient_color * scene.ambient_intensity * object.albedo;
	color += blinnPhong(N, V, -scene.dir_light_direction,
	                    scene.dir_light_color * scene.dir_light_intensity);
	/* этапы F и G — циклы по источникам */
	out_color = vec4(color, 1.0);
}
```

### 9.2. Значения по умолчанию

| Параметр | Значение |
|----------|----------|
| Ambient | цвет `(1, 1, 1)`, интенсивность `0.05` |
| Directional | направление `(−0.4, −1, −0.3)` (нормализовать), цвет `(1, 0.95, 0.9)`, интенсивность `0.4` |

### 9.3. UI

Секция `Scene`: `ColorEdit3 Background`, `ColorEdit3 Ambient color`, `SliderFloat Ambient intensity 0…1`.
Секция `Directional light`: `DragFloat3 Direction`, `ColorEdit3 Color`, `SliderFloat Intensity 0…5`.

### 9.4. Нормализация направлений в `update()`

Направления задаются через `DragFloat3`, поэтому пользователь может сделать вектор нулевым. Тогда `normalize`
даст NaN, и всё станет чёрным. Правило: если `length(d) < 1e-4`, подставить `(0, −1, 0)`, иначе `normalize(d)`.
Это же правило используем для прожекторов в §11.

✅ **Чекпоинт E (= базовое задание):** у граней пирамиды разная яркость, на пирамиде виден блик, и он сдвигается
при движении камеры. Shininess меняет размер блика, ambient подсвечивает тень. `git commit -m "Lab2: Blinn-Phong directional light"`.

**Что нужно понимать:** каждое слагаемое §3.2 и его физический смысл; зачем `V` и, следовательно,
`camera_position`; почему блик зависит от положения камеры, а диффузная составляющая нет; почему всё считается в
мировых координатах; почему свет считаем во фрагментном шейдере, а не в вершинном (у пола всего 4 вершины,
и пятно прожектора между ними потерялось бы).

---

## 10. Этап F — точечные источники (доп. 2)

### 10.1. Storage-буфер, который растёт

```cpp
struct LightBuffer {
	graphics::Buffer buffer;
	uint32_t capacity = 0;
};

LightBuffer point_light_buffer;              // начальная ёмкость 8
std::vector<PointLight> point_lights;        // CPU-копия, её же редактирует UI

// Вызывать в render() ДО vkBeginCommandBuffer
bool ensureCapacity(LightBuffer& lights, uint32_t count, VkDeviceSize element_size, uint32_t binding) {
	if (count <= lights.capacity) return true;
	const uint32_t capacity = std::max(count, lights.capacity * 2);
	// Безопасно: prepare() дождался конца прошлого кадра, буфер никем не используется
	graphics::destroyBuffer(lights.buffer);
	lights.buffer = graphics::createBuffer(capacity * element_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, nullptr);
	if (lights.buffer.buffer == VK_NULL_HANDLE) return false;
	lights.capacity = capacity;
	graphics::writeBufferDescriptor(scene_descriptor_set, binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, lights.buffer.buffer);
	return true;
}
```

- Буфер нулевого размера создать нельзя, поэтому начальная ёмкость 8 создаётся сразу в `initialize()`.
- В `render()`: `ensureCapacity(...)`, затем `if (!point_lights.empty()) uploadToBuffer(..., count * sizeof(PointLight))`.
- `scene_uniforms.point_light_count = uint32_t(point_lights.size())` — в `update()`.
- Descriptor set обновляется **до** записи команд. Если обновить набор, который уже привязан в записываемом
  command buffer, этот буфер станет недействительным (ошибка валидации).

### 10.2. Set 0 расширяется

- `scene_set_layout`: + binding 1 `STORAGE_BUFFER`, `FRAGMENT`.
- Пул: + `STORAGE_BUFFER × 2` (сразу и для прожекторов).
- После выделения set 0: `writeBufferDescriptor(scene_descriptor_set, 1, STORAGE_BUFFER, point_light_buffer.buffer.buffer)`.

### 10.3. Шейдер

```glsl
struct PointLight {
	vec3 position; float intensity;
	vec3 color;
};
layout(std430, set = 0, binding = 1) readonly buffer PointLights {
	PointLight point_lights[];            // размер не задан, это требование условия
};

// в main():
for (uint i = 0; i < scene.point_light_count; ++i) {
	PointLight light = point_lights[i];
	vec3 to_light = light.position - v_world_position;
	float distance2 = max(dot(to_light, to_light), 1e-4);
	vec3 L = to_light * inversesqrt(distance2);
	color += blinnPhong(N, V, L, light.color * light.intensity / distance2);   // 1/d²
}
```

### 10.4. UI (секция `Point lights`)

```cpp
ImGui::Text("Count: %zu", point_lights.size());
if (ImGui::Button("Add point light")) {
	point_lights.push_back({.position = {0.0f, 1.5f, 2.0f}, .intensity = 5.0f, .color = {1.0f, 1.0f, 1.0f}});
}
int remove_index = -1;
for (size_t i = 0; i < point_lights.size(); ++i) {
	ImGui::PushID(int(i));                                  // иначе все «Position» — один и тот же виджет
	if (ImGui::TreeNode("light", "Point light %zu", i)) {
		ImGui::DragFloat3("Position", &point_lights[i].position.x, 0.05f);
		ImGui::ColorEdit3("Color", &point_lights[i].color.x);
		ImGui::DragFloat("Intensity", &point_lights[i].intensity, 0.1f, 0.0f, 100.0f);
		if (ImGui::Button("Remove")) remove_index = int(i);
		ImGui::TreePop();
	}
	ImGui::PopID();
}
if (remove_index >= 0) point_lights.erase(point_lights.begin() + remove_index);   // после цикла
```

По умолчанию один оранжевый источник: `(2, 1, 1)`, цвет `(1, 0.6, 0.2)`, интенсивность `4`.

✅ **Чекпоинт F:** вокруг точечного источника на полу видно пятно, которое быстро гаснет с расстоянием.
Добавлено 10+ источников (больше начальной ёмкости 8), ошибок нет. Все удалены — ошибок нет, свет пропал.
`git commit -m "Lab2: point lights in storage buffer"`.

**Что нужно понимать:** UBO против storage-буфера (размер: UBO гарантированно от 16 КБ, SSBO от 128 МБ;
std140 против std430; массив без размера возможен только в SSBO); зачем `readonly`; почему 1/d² и что
происходит при d → 0; зачем `_pad` в C++, но не в GLSL; почему рост буфера и обновление дескриптора безопасны
именно в `render()` (одна fence) и что изменилось бы при двух кадрах в полёте (буфер и набор на каждый кадр).

---

## 11. Этап G — прожекторы (доп. 3) = оценка 4

### 11.1. Настройки в UI и данные для GPU — разные структуры

В UI удобно задавать углы в градусах, а GPU нужны косинусы, поэтому структуры две:

```cpp
struct SpotLightSettings {
	math::vec3 position;
	math::vec3 direction;   // не обязательно единичный, нормализуется в update()
	math::vec3 color;
	float intensity;
	float inner_deg;        // от оси конуса
	float outer_deg;        // ≥ inner_deg
};
std::vector<SpotLightSettings> spot_lights;
std::vector<SpotLight> spot_lights_gpu;     // заполняется в update(), копируется в render()
LightBuffer spot_light_buffer;              // начальная ёмкость 8, binding 2

// update():
spot_lights_gpu.resize(spot_lights.size());
for (size_t i = 0; i < spot_lights.size(); ++i) {
	const SpotLightSettings& s = spot_lights[i];
	spot_lights_gpu[i] = {
		.position = s.position, .intensity = s.intensity,
		.direction = safeDirection(s.direction),               // §9.4
		.cos_inner = std::cos(math::radians(s.inner_deg)),
		.color = s.color,
		.cos_outer = std::cos(math::radians(s.outer_deg)),
	};
}
```

### 11.2. Шейдер

```glsl
struct SpotLight {
	vec3 position;  float intensity;
	vec3 direction; float cos_inner;
	vec3 color;     float cos_outer;
};
layout(std430, set = 0, binding = 2) readonly buffer SpotLights {
	SpotLight spot_lights[];
};

// в main():
for (uint i = 0; i < scene.spot_light_count; ++i) {
	SpotLight light = spot_lights[i];
	vec3 to_light = light.position - v_world_position;
	float distance2 = max(dot(to_light, to_light), 1e-4);
	vec3 L = to_light * inversesqrt(distance2);

	float cos_theta = dot(-L, light.direction);              // угол между осью и направлением на фрагмент
	float edge = light.cos_inner - light.cos_outer;          // > 0, если outer > inner
	float cone = edge > 0.0 ? clamp((cos_theta - light.cos_outer) / edge, 0.0, 1.0)
	                        : step(light.cos_outer, cos_theta);   // outer == inner → жёсткий край

	color += blinnPhong(N, V, L, light.color * light.intensity * cone / distance2);
}
```

Проверка формулы края: внутри внутреннего конуса `cos θ ≥ cos_inner`, значит `cone ≥ 1` → после clamp 1.
На внешней границе `cos θ = cos_outer` → 0. Между ними — линейный переход. Сзади прожектора `cos θ < 0` → 0.

### 11.3. Set 0 и render

- `scene_set_layout`: + binding 2 `STORAGE_BUFFER`, `FRAGMENT` (пул уже рассчитан на 2 SSBO, §10.2).
- `writeBufferDescriptor(scene_descriptor_set, 2, ...)` при создании; `ensureCapacity(spot_light_buffer, ..., sizeof(SpotLight), 2)` в `render()`.
- `scene_uniforms.spot_light_count = uint32_t(spot_lights.size())`.

### 11.4. UI (секция `Spot lights`)

Так же, как у точечных (§10.4, `PushID`, удаление после цикла), плюс:

```cpp
ImGui::DragFloat3("Direction", &s.direction.x, 0.02f);
ImGui::SliderFloat("Inner angle", &s.inner_deg, 0.0f, 89.0f);
ImGui::SliderFloat("Outer angle", &s.outer_deg, 0.0f, 89.0f);
s.outer_deg = std::max(s.outer_deg, s.inner_deg);         // внешний не меньше внутреннего
```

По умолчанию один белый прожектор над пирамидой: позиция `(−3, 4, 2)`, направление на пирамиду `(3, −3.5, −2)`,
интенсивность `25`, inner `12°`, outer `20°`. Кнопка «Add» создаёт прожектор `(0, 4, 0)` с направлением вниз.

✅ **Чекпоинт G = оценка 4:** на полу видно круглое пятно прожектора с плавным краем. Если сделать outer = inner,
край становится резким, а увеличение outer размывает его. Прожекторы добавляются (больше 8) и удаляются, все
параметры меняются на лету. Нет `Validation Error`, в том числе при закрытии. `git commit -m "Lab2: spot lights with soft edges"`.

**Что нужно понимать:** почему сравниваются косинусы, а не углы; формула мягкого края и что происходит при
`outer == inner`; почему углы отсчитываются от оси (половина раствора); почему прожектор подчиняется тому же
закону 1/d², что и точечный источник.

### 11.5. (Необязательно, «креатив») анимация

Checkbox `Animate point lights`: в `update()` позиции точечных источников вращаются вокруг оси Y
(`rotateY(dt · speed)` применить к позиции). Около 10 строк. Делать только после чекпоинта G.

---

## 12. Типичные ловушки

| Симптом | Причина / что делать |
|---------|---------------------|
| Всё чёрное | Нормаль или направление нулевые → NaN (§9.4); перепутан порядок set layouts в pipeline layout; интенсивности 0 |
| Грани пирамиды сглажены, нет чётких рёбер | Общие вершины (5 вместо 16) — §3.5 |
| Пирамида освещена «изнутри» / тёмная сторона светится | Нормали внутрь (обход граней) или `L` с неверным знаком (`−direction` у направленного) |
| Блик на неосвещённой стороне | Нет проверки `N·L > 0` перед бликом |
| Источники «мусорные», мигают, сдвинуты | Размер C++-структуры не равен std430 (нет `_pad`) → `static_assert` (§4) |
| Пол тёмный вдали от точечного источника | Так и должно быть (1/d²); увеличить интенсивность |
| Пятна света «едут» вместе с камерой | Смешаны системы координат: всё должно быть в мировых, включая `camera_position` |
| После 8 источников новые не светят | Нет `ensureCapacity` или счётчик берётся из `.length()` (§4) |
| `Validation Error … descriptor set … updated … while bound` | Дескриптор обновлён после `vkCmdBindDescriptorSets` в том же кадре → `ensureCapacity` до `vkBeginCommandBuffer` |
| `Validation Error` про размер буфера 0 | Создан SSBO нулевого размера → начальная ёмкость ≥ 1 |
| Правишь один источник, а меняются все | Нет `ImGui::PushID(i)` |
| Падение при Remove | `erase` внутри цикла по тому же вектору → удалять после цикла |
| Резкий край прожектора при outer > inner | В буфер записаны градусы, а не косинусы; или `edge` с обратным знаком |
| NaN-пятна при shininess 0 | `pow(0, 0)` → минимум слайдера 1 |
| Картинка переворачивается при большом наклоне | pitch не ограничен ±89° |
| Слишком светло, «выцветшие» цвета | sRGB swapchain (§3.6) — это нормально, уменьшить ambient |
| Изменения в шейдере не применяются | Нет `compile_shader(lit.*)` в CMakeLists или запуск не из корня |

---

## 13. README, отчёт, git

### 13.1. Git

- Ветка `lab2` уже создана от `lab1`. Первый коммит: `lab2/` (PDF, read.txt, PLAN.md) — «Lab2: add assignment, teacher notes and plan».
- По коммиту на каждый чекпоинт A–G.
- В конце: `git push -u origin lab2`, затем слить в `master` (в lab 1 слияние не делалось, сначала смержить `lab1`).

### 13.2. `lab2/README.md` (как [lab1/README.md](../lab1/README.md))

- «Лабораторная работа №2. Камера и освещение» + краткое условие + «выполнено: базовое + доп. 1–3»;
- что сделано (Блинн-Фонг, 3 типа источников, 1/d², мягкие края, set 0/set 1, storage-буферы);
- сборка и запуск (на Fedora: `cmake --preset debug`; ссылка на lab1/README для Ubuntu/Mint);
- таблица элементов UI по секциям;
- 2–3 скриншота в `lab2/screenshots/`.

### 13.3. Отчёт по образцу

```bash
cp "lab1/3XX_Фамилия_ЛабX.docx" "lab2/3XX_<Фамилия>_Лаб2.docx"
```

| Часть | Что вписать |
|-------|-------------|
| Титул | «Лабораторная работа №**2**», тема «Камера и освещение» |
| **Условие** | Цель из PDF, «выполнено базовое задание и дополнительные 1–3» |
| **Метод решения** | Камера (view как обратная матрица, §6.2); flat-нормали через векторное произведение (§7.2); Блинн-Фонг (§3.2); три типа источников и 1/d²; мягкий край прожектора; normal matrix; раскладка std140/std430 (таблица §4); set 0 / set 1, storage-буферы с ростом ёмкости |
| **Результаты** | Скриншоты: (1) только направленный + ambient; (2) несколько цветных точечных; (3) прожектор: мягкий край против жёсткого (outer = inner); (4) разный shininess |
| **Выводы** | «Я научился…»: модель освещения, типы источников, нормали, дескрипторы нескольких наборов, storage-буферы |

`libreoffice --headless --convert-to pdf --outdir lab2 "lab2/3XX_<Фамилия>_Лаб2.docx"`

### 13.4. Финальная проверка

- [ ] `rm -rf build-debug && cmake --preset debug && cmake --build build-debug --parallel` — собирается с нуля без ошибок и без новых предупреждений.
- [ ] 2 минуты работы: добавить/удалить по 10 источников каждого типа, ресайз, сворачивание, закрытие → нет `Validation Error`.
- [ ] Нет временных проверок математики (§5) и отладочных шейдеров (§7.4, §8.3).
- [ ] README, отчёт PDF, скриншоты в `lab2/`.

---

## 14. Подготовка к защите

**На 4 нужно ответить на ВСЕ вопросы.** К каждому пункту — 2–3 предложения своими словами, плюс уметь показать
место в коде.

**Освещение и математика**
1. Формула Блинн-Фонга: ambient, diffuse, specular; смысл `N·L`; что такое `H`; чем отличается от Фонга.
2. Что задаёт `shininess` и почему с ростом блик становится меньше и резче.
3. Чем отличаются направленный, точечный и прожекторный источники: как в каждом получается `L`.
4. Закон обратных квадратов: почему `1/d²`; чем отличается от `Kc + Kl·d + Kq·d²` из learnopengl; что при `d → 0`.
5. Прожектор: зачем косинусы, формула мягкого края, что при `outer == inner`, почему сзади не светит.
6. Нормали: как сгенерированы (векторное произведение, порядок обхода); почему 16 вершин, а не 5.
7. Normal matrix: почему не матрица модели; `transpose(inverse(M))`; почему у нас это просто поворот; пример с
   неравномерным масштабом.
8. Зачем `normalize(v_normal)` во фрагментном шейдере.
9. Камера: view как обратная матрица; порядок `rotateX(−pitch)·rotateY(−yaw)·translate(−p)`; формула `forward`.
10. Почему освещение в мировых координатах и зачем шейдеру `camera_position`.
11. Почему освещение во фрагментном, а не в вершинном шейдере.

**Vulkan и код**
12. Descriptor set layout / pool / set / pipeline layout; set и binding в GLSL; как считаются размеры пула.
13. Зачем два набора; как часто привязывается каждый; что делает `firstSet = 1`.
14. UBO против storage-буфера: размеры, std140 против std430, массив без размера, `readonly`.
15. Раскладка своих структур: смещения полей, где и зачем `_pad`, почему `normal_matrix` — `mat4`.
16. Как число источников попадает в шейдер и почему не `.length()`.
17. Рост storage-буфера: почему безопасно удалять буфер и обновлять дескриптор в `render()`; что было бы при 2 кадрах в полёте.
18. Почему у каждого объекта свой UBO, а не один общий, перезаписываемый между draw-вызовами.
19. Vertex input: два атрибута, `location`, `offsetof`, stride.
20. Порядок кадра: что в `update()`, что в `render()` и почему (лаба 1, §3.4).

Совет из лабы 1 здесь ещё важнее: после каждого этапа объясни свой код вслух. Тройку можно получить одним
ответом, четвёрку — нет.

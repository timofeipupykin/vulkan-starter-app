# Лабораторная работа №1 «Основы 3D-графики» — план (минимум на оценку 3)

**Вариант 6 — Пирамида. Цель: оценка 3 — только вариант, без дополнительных заданий.**

> Источники: условие [CGLab#1.pdf](CGLab#1.pdf), сообщения преподавателя [read.txt](read.txt),
> образец отчёта [3XX_Фамилия_ЛабX.docx](3XX_Фамилия_ЛабX.docx), стартовый код репозитория
> (форк совпадает с upstream `vladeemerr/vulkan-starter-app`, коммит `155bdcd`).

### Что проверено на практике (27.10)

| Проверка | Результат |
|----------|-----------|
| Сборка с системными заголовками Vulkan из Mint 22 / Ubuntu 24.04 (1.3.275) | ❌ **не собирается** — `vk-bootstrap v1.4.321` требует заголовки 1.4 (см. 2.1) |
| Сборка с Vulkan-Headers v1.4.321 через `CMakeUserPresets.json` (без правки CMakeLists) | ✅ |
| Прототип по этому плану (буферы VMA, pipeline, UBO, дескрипторы, ImGui) | ✅ компилируется с `-Wall -Wextra` без предупреждений |
| Матрица перспективы: near → z=0, far → z=1, «верх» → отрицательный Y | ✅ численно |
| Индексы пирамиды + `cullMode=BACK` + `CCW` + переворот Y: отсекаются ровно грани, отвёрнутые от камеры | ✅ численно |
| Запуск нетронутого стартового кода на Intel Iris Xe | ❌ `Segmentation fault` в драйвере Intel — ожидаемо, см. 2.5 |
| Запуск с `render()` из этапа A | ✅ окно работает, ошибок валидации нет |

Код пишешь **сам** (см. 0.2). Фрагменты ниже — чтобы свериться, а не скопировать.

---

## Оглавление

0. [Оценка и сдача](#0-оценка-и-сдача)
1. [Что нужно сделать](#1-что-нужно-сделать)
2. [Окружение: установка и первая сборка](#2-окружение-установка-и-первая-сборка)
3. [Разбор стартового кода](#3-разбор-стартового-кода)
4. [Структура решения и правила кода](#4-структура-решения-и-правила-кода)
5. [Этап A — пустой кадр](#5-этап-a--пустой-кадр)
6. [Этап B — шейдеры, pipeline, треугольник](#6-этап-b--шейдеры-pipeline-треугольник)
7. [Этап C — пирамида в вершинном и индексном буферах](#7-этап-c--пирамида-в-вершинном-и-индексном-буферах)
8. [Этап D — математика](#8-этап-d--математика)
9. [Этап E — UBO, дескриптор, 3D-пирамида (= оценка 3)](#9-этап-e--ubo-дескриптор-3d-пирамида--оценка-3)
10. [Типичные ловушки](#10-типичные-ловушки)
11. [README, отчёт, git](#11-readme-отчёт-git)
12. [Подготовка к защите](#12-подготовка-к-защите)

---

## 0. Оценка и сдача

- На оценку 3 нужно выполнить **вариант без дополнительных заданий** (PDF).
- Лаба выдана 22.09; максимум снижается каждые две недели, но **не ниже 3** — для оценки 3 сроки уже не важны.
- На сдаче — **3–4 случайных вопроса** по коду и математике. **На 3 нужно правильно ответить хотя бы на один.**
  Не ответил — приходишь в следующий день сдачи.
- Преподаватель видит LLM-код и проверяет понимание → после каждого этапа есть блок **«Что нужно понимать»**.
- Только Vulkan (у нас) и C++.

---

## 1. Что нужно сделать

Из PDF, без доп. заданий:

- правильная **четырёхугольная пирамида** (квадрат в основании) в 3D;
- **перспективная проекция** через матрицу;
- **аффинное преобразование** — пирамида вращается (матрица поворота);
- корректная отрисовка: буфер глубины, дальние грани не рисуются поверх ближних;
- реальное время + **взаимодействие с пользователем** через ImGui (несколько ползунков: скорость вращения,
  расстояние камеры, угол обзора).

Чего **не** делаем (это доп. задания 1–6): ортографию и переключатель проекций, UI позиции/масштаба, траектории,
выбор цвета, несколько объектов.

Итог по объёму: ~5 новых файлов, ~300 строк своего кода.

---

## 2. Окружение: установка и первая сборка

### 2.1. Проблема: на Mint 22 / Ubuntu 24.04 проект «как есть» не собирается

Преподаватель пишет, что для Ubuntu хватит `libvulkan-dev` и `vulkan-validationlayers`. Но в Ubuntu 24.04 (и в
Mint 22 на её основе) `libvulkan-dev` — версии **1.3.275**, а `vk-bootstrap v1.4.321` из
[CMakeLists.txt](../CMakeLists.txt) использует типы Vulkan 1.4. Проверено компиляцией:

```
VkBootstrapDispatch.h:3247: error: 'VkBindDescriptorSetsInfo' does not name a type
VkBootstrapDispatch.h:3797: error: 'VkPushConstantsInfo' does not name a type
```

Плюс сейчас конфигурация падает ещё раньше — GLFW не находит `wayland-scanner`.

**Решение (проверено):** библиотека `libvulkan.so` остаётся системной, а **заголовки** берём свежие
(`Vulkan-Headers v1.4.321`) и передаём CMake через `Vulkan_INCLUDE_DIR`. CMakeLists не трогаем.

### 2.2. Шаг 1 — пакеты

```bash
sudo apt update
sudo apt install \
    vulkan-validationlayers glslc \
    libwayland-dev libxkbcommon-dev wayland-protocols \
    libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
```

| Пакет | Зачем |
|-------|-------|
| `vulkan-validationlayers` | Слой валидации — без него ошибки Vulkan не печатаются |
| `glslc` | Компилятор шейдеров GLSL → SPIR-V. Без него CMake **молча** не компилирует шейдеры |
| `libwayland-dev libxkbcommon-dev wayland-protocols` | GLFW собирается с поддержкой Wayland |
| `libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev` | GLFW для X11 |

Проверка:

```bash
glslc --version                                          # печатает версию
vulkaninfo --summary | grep -i -A3 "Instance Layers"     # есть VK_LAYER_KHRONOS_validation
vulkaninfo --summary | grep -i deviceName                # Intel(R) Iris(R) Xe Graphics
```

### 2.3. Шаг 2 — свежие заголовки Vulkan

```bash
mkdir -p ~/libs
git clone --depth 1 --branch v1.4.321 https://github.com/KhronosGroup/Vulkan-Headers.git ~/libs/Vulkan-Headers
ls ~/libs/Vulkan-Headers/include/vulkan/vulkan_core.h     # файл должен существовать
```

### 2.4. Шаг 3 — пользовательский пресет CMake

Создать в корне проекта файл **`CMakeUserPresets.json`** (CMake и VS Code подхватывают его автоматически).
Путь должен быть **абсолютным**; если домашняя папка не `/home/tim` — исправить:

```json
{
	"version": 2,
	"configurePresets": [
		{
			"name": "local-debug",
			"inherits": "debug",
			"cacheVariables": {
				"Vulkan_INCLUDE_DIR": "/home/tim/libs/Vulkan-Headers/include"
			}
		}
	]
}
```

Добавить в `.gitignore` строку `CMakeUserPresets.json` (файл зависит от машины). Сборка по-прежнему в `build-debug`.

### 2.5. Шаг 4 — сборка и первый запуск

```bash
cd ~/Documents/vsCode/vulkan-starter-app
rm -rf build-debug                      # ОБЯЗАТЕЛЬНО: там старый кэш с Vulkan_INCLUDE_DIR=/usr/include
cmake --preset local-debug              # скачает glfw, vk-bootstrap, VMA, imgui (нужен интернет, ~1 мин)
cmake --build build-debug --parallel
./build-debug/vulkan-starter-app        # запускать ИЗ КОРНЯ проекта
```

Нормально при сборке: много предупреждений `unused parameter` из `vk_mem_alloc.h` и одно
`variable 'status' set but not used` из `main.cpp` — это чужой код. Ошибок быть не должно.

**При запуске на этой машине (Intel Iris Xe) нетронутый стартовый код падает — `Segmentation fault`.
Это ожидаемо, установка в порядке** (проверено под gdb). Перед падением в консоли будут две ошибки валидации:

```
Validation Error: [ VUID-vkQueueSubmit-pCommandBuffers-00070 ] ... VkCommandBuffer ... is unrecorded and contains no commands.
Validation Error: [ UNASSIGNED-CoreValidation-DrawState-InvalidImageLayout ] ... expects VkImage ... to be in layout
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL--instead, current layout is VK_IMAGE_LAYOUT_UNDEFINED.
```

Причина — §3.4 п.1: `application::render()` пустой, а `submitAndPresent()` всё равно отправляет на GPU
незаписанный command buffer. Драйверы NVIDIA/AMD это прощают (поэтому у преподавателя открывается демо-окно),
драйвер Intel (`libvulkan_intel.so`) — падает.

**Что делать:** сразу переходить к **этапу A** (§5) — как только `render()` записывает command buffer с render pass,
падение исчезает (проверено: окно работает, ошибок валидации нет).

Попутно эти два сообщения доказывают, что **слой валидации работает** — отдельно проверять не нужно.

### 2.6. Запасной путь: LunarG Vulkan SDK

Если 2.3–2.4 не подошли. Сайт `lunarg.com` из РФ без прокси не открывается, прямая ссылка работает (проверено):

```
https://sdk.lunarg.com/sdk/download/1.4.321.1/linux/vulkansdk-linux-x86_64-1.4.321.1.tar.xz
```

```bash
mkdir -p ~/libs/vulkansdk && tar -xf vulkansdk-linux-x86_64-1.4.321.1.tar.xz -C ~/libs/vulkansdk
source ~/libs/vulkansdk/1.4.321.1/setup-env.sh   # задаёт VULKAN_SDK, PATH (glslc), VK_LAYER_PATH
rm -rf build-debug && cmake --preset debug
```

`setup-env.sh` действует только в текущем терминале (добавить в `~/.bashrc` или запускать VS Code командой `code .` из него).

### 2.7. VS Code

- Расширения: **C/C++**, **CMake Tools**.
- CMake Tools → «Select Configure Preset» → **`local-debug`**.
- Запускать с рабочей папкой = корень проекта. `.vscode/launch.json`:

  ```json
  {
    "version": "0.2.0",
    "configurations": [{
      "name": "Lab1",
      "type": "cppdbg",
      "request": "launch",
      "program": "${workspaceFolder}/build-debug/vulkan-starter-app",
      "cwd": "${workspaceFolder}",
      "MIMode": "gdb"
    }]
  }
  ```

### 2.8. Git

```bash
git remote add upstream https://github.com/vladeemerr/vulkan-starter-app.git   # просьба преподавателя
git checkout -b lab1
```

Изменения преподавателя: `git fetch upstream && git merge upstream/master`. Чтобы не было конфликтов — не править
`main.cpp`, `graphics_internal.*`, `README.md`, `CMakePresets.json`.

---

## 3. Разбор стартового кода

### 3.1. Файлы

| Файл | Назначение | Трогаем? |
|------|-----------|----------|
| [source/main.cpp](../source/main.cpp) | Окно, ImGui, главный цикл | Нет |
| [source/graphics_internal.hpp](../source/graphics_internal.hpp) / [.cpp](../source/graphics_internal.cpp) | Инициализация Vulkan, swapchain, depth, render pass, синхронизация, ImGui-рендер, ресайз | Нет (читать) |
| [source/application.hpp](../source/application.hpp) / [.cpp](../source/application.cpp) | **Основной код лабы** | Да |
| `source/graphics.hpp/.cpp` | **Создать** — «имена оставлены под ваши абстракции»: буферы, шейдеры | Да |
| `source/math.hpp` | **Создать** — матрицы | Да |
| [CMakeLists.txt](../CMakeLists.txt) | Список `.cpp` и шейдеров | Да (2 места) |
| [shaders/](../shaders/) | GLSL; `*.spv` в `.gitignore` | Да |

### 3.2. Функции `application`

| Функция | Что в ней |
|---------|-----------|
| `initialize()` | Создать объекты Vulkan. Вернуть `false` при ошибке |
| `shutdown()` | Уничтожить всё созданное: каждому `vkCreate*` — `vkDestroy*`, каждому `vmaCreateBuffer` — `vmaDestroyBuffer` |
| `update(time)` | ImGui-окно + пересчёт матрицы. `time` — секунды с запуска |
| `render(fd)` | Запись команд `vkCmd*` в `fd.command_buffer` (+ копирование матрицы в UBO, см. 3.4) |

### 3.3. Порядок кадра ([main.cpp:75-88](../source/main.cpp#L75-L88))

```
application::update(time)              ← UI, матрица (на CPU)
ImGui::Render
fd = graphics::internal::prepare()     ← ЖДЁТ, пока GPU закончит прошлый кадр
application::render(fd)                ← запись команд
graphics::internal::submitAndPresent() ← ImGui поверх, отправка на GPU, показ
```

`graphics::internal::context`: `device`, `allocator` (VMA), `graphics_queue`, `swapchain_extent` (размер окна),
`render_pass` (цвет + глубина).

### 3.4. Неочевидные моменты

1. **`render()` обязан каждый кадр записать command buffer и начать/закончить render pass** — иначе GPU получит
   пустой буфер, а ImGui-проход — картинку в неправильном layout ([graphics_internal.cpp:540](../source/graphics_internal.cpp#L540)).
2. **Матрицу считаем в `update()`, а в UBO копируем в `render()`.** `update()` выполняется до `prepare()`, когда
   GPU ещё может читать UBO прошлого кадра; в `render()` прошлый кадр гарантированно закончен.
3. Render pass имеет **два вложения** (цвет + глубина) → два `VkClearValue`, в pipeline включён depth test.
4. Ресайз уже обработан. Нам — брать `context.swapchain_extent` каждый кадр и делать viewport/scissor динамическими.

---

## 4. Структура решения и правила кода

```
source/
  math.hpp              ← mat4, умножение, translate, rotateX, rotateY, perspective
  graphics.hpp/.cpp     ← Buffer, createBuffer/uploadToBuffer/destroyBuffer, createShaderModule
  application.cpp       ← пирамида, UI, pipeline, дескриптор, рисование
shaders/
  pyramid.vert, pyramid.frag
```

Правки [CMakeLists.txt](../CMakeLists.txt):

```cmake
# строка 6:
add_executable(${PROJECT_NAME} source/main.cpp source/graphics_internal.cpp source/application.cpp source/graphics.cpp)

# строки 106–107 (вместо закомментированного примера):
	compile_shader(pyramid.vert)
	compile_shader(pyramid.frag)
```

- После изменения списка `.cpp` — заново `cmake --preset local-debug`.
- Файлы шейдеров должны существовать до сборки, иначе Ninja: `'…/shaders/pyramid.vert' … missing and no known rule to make it`.
- Ошибки в GLSL показываются при сборке (`glslc`), сборка останавливается.

### 4.1. Правила C++, на которых легко ошибиться

1. **Поля в `{ .sType = …, .size = … }` — строго в порядке объявления в структуре**, иначе ошибка компиляции.
   Порядок — Ctrl+клик по типу в VS Code. Пропущенные поля = 0.
2. **Нельзя брать адрес временного объекта** (`&VkViewport{...}`) — сначала переменная, потом `&`.
3. Результат каждого `vkCreate*`/`vmaCreate*` проверять на `!= VK_SUCCESS` и печатать в `std::cerr`, что не создалось.
4. Хэндлы Vulkan — глобальные переменные в анонимном `namespace { }` в `application.cpp` (как в стартовом коде).
5. Углы: в `sin/cos` — радианы (`deg * pi / 180`).
6. **Подписи ImGui — на английском**: встроенный шрифт не содержит кириллицы (вместо букв будут «?»).

---

## 5. Этап A — пустой кадр

**Цель:** окно с фоном цвета из UI, без ошибок валидации.

```cpp
namespace {
float clear_color[3] = {0.1f, 0.1f, 0.12f};
}

void update(double time) {
	ImGui::Begin("Lab 1");
	ImGui::ColorEdit3("Background", clear_color);
	ImGui::End();                         // End() вызывать всегда
}

void render(const graphics::internal::FrameData& fd) {
	auto& context = graphics::internal::context;
	if (fd.command_buffer == VK_NULL_HANDLE) return;
	const VkCommandBuffer cb = fd.command_buffer;

	vkResetCommandBuffer(cb, 0);
	const VkCommandBufferBeginInfo begin = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	vkBeginCommandBuffer(cb, &begin);

	VkClearValue clear[2];
	clear[0].color = {{clear_color[0], clear_color[1], clear_color[2], 1.0f}};
	clear[1].depthStencil = {1.0f, 0};

	const VkRenderPassBeginInfo pass = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = context.render_pass,
		.framebuffer = fd.framebuffer,
		.renderArea = {.offset = {0, 0}, .extent = context.swapchain_extent},
		.clearValueCount = 2,
		.pClearValues = clear,
	};
	vkCmdBeginRenderPass(cb, &pass, VK_SUBPASS_CONTENTS_INLINE);
	// ... здесь будет рисование ...
	vkCmdEndRenderPass(cb);
	vkEndCommandBuffer(cb);
}
```

Убрать `ImGui::ShowDemoWindow()`.

✅ **Чекпоинт A:** фон меняется из UI, ресайз работает, нет `Validation Error`. `git commit`.

**Что нужно понимать:** command buffer (команды записываются, выполняются после отправки); render pass и
framebuffer; зачем два clear value; почему глубина очищается в 1.0.

---

## 6. Этап B — шейдеры, pipeline, треугольник

### 6.1. Временные шейдеры

`shaders/pyramid.vert`:

```glsl
#version 450
vec2 positions[3] = vec2[](vec2(0.0, -0.5), vec2(0.5, 0.5), vec2(-0.5, 0.5));
void main() { gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0); }
```

`shaders/pyramid.frag`:

```glsl
#version 450
layout(location = 0) out vec4 out_color;
void main() { out_color = vec4(1.0, 0.5, 0.2, 1.0); }
```

Добавить `compile_shader` в CMakeLists (§4), собрать, убедиться, что появились `shaders/pyramid.vert.spv` и
`shaders/pyramid.frag.spv`.

### 6.2. Загрузка шейдера (`graphics.cpp`)

```cpp
VkShaderModule createShaderModule(const char* path) {
	std::ifstream file(path, std::ios::binary | std::ios::ate);   // ate: сразу в конец, чтобы узнать размер
	if (!file) { std::cerr << "Failed to open " << path << '\n'; return VK_NULL_HANDLE; }
	const size_t size = size_t(file.tellg());
	std::vector<uint32_t> code(size / 4);                         // SPIR-V — 32-битные слова
	file.seekg(0);
	file.read(reinterpret_cast<char*>(code.data()), std::streamsize(size));

	const VkShaderModuleCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = size,                                         // в байтах
		.pCode = code.data(),
	};
	VkShaderModule module = VK_NULL_HANDLE;
	if (vkCreateShaderModule(graphics::internal::context.device, &info, nullptr, &module) != VK_SUCCESS)
		std::cerr << "Failed to create shader module " << path << '\n';
	return module;
}
```

Путь — `"shaders/pyramid.vert.spv"` (запуск из корня проекта).

### 6.3. Pipeline (в `initialize()`)

Порядок: шейдерные модули → `VkPipelineLayout` → `VkPipeline` → `vkDestroyShaderModule` для обоих модулей.

| Структура | Поля (остальные = 0) |
|-----------|----------------------|
| `VkPipelineShaderStageCreateInfo[2]` | `.stage = VK_SHADER_STAGE_VERTEX_BIT` / `FRAGMENT_BIT`, `.module`, `.pName = "main"` |
| `VkPipelineVertexInputStateCreateInfo` | на этапе B пусто; на этапе C — binding + 2 атрибута |
| `VkPipelineInputAssemblyStateCreateInfo` | `.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST` |
| `VkPipelineViewportStateCreateInfo` | `.viewportCount = 1, .scissorCount = 1` |
| `VkPipelineRasterizationStateCreateInfo` | `.polygonMode = VK_POLYGON_MODE_FILL`, `.cullMode = VK_CULL_MODE_NONE` (до этапа E), `.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE`, **`.lineWidth = 1.0f`** |
| `VkPipelineMultisampleStateCreateInfo` | **`.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT`** |
| `VkPipelineDepthStencilStateCreateInfo` | `.depthTestEnable = VK_TRUE`, `.depthWriteEnable = VK_TRUE`, `.depthCompareOp = VK_COMPARE_OP_LESS` |
| `VkPipelineColorBlendAttachmentState` | `.blendEnable = VK_FALSE`, **`.colorWriteMask = R\|G\|B\|A`** (при 0 ничего не нарисуется) |
| `VkPipelineColorBlendStateCreateInfo` | `.attachmentCount = 1`, `.pAttachments` |
| `VkPipelineDynamicStateCreateInfo` | `VK_DYNAMIC_STATE_VIEWPORT`, `VK_DYNAMIC_STATE_SCISSOR` |
| `VkPipelineLayoutCreateInfo` | на этапе B пустой; на этапе E — 1 set layout |
| `VkGraphicsPipelineCreateInfo` | указатели на всё выше, `.layout`, `.renderPass = context.render_pass`, `.subpass = 0` |

### 6.4. Рисование (между begin/end render pass)

```cpp
const VkViewport viewport = {
	.x = 0.0f, .y = 0.0f,
	.width = float(context.swapchain_extent.width),
	.height = float(context.swapchain_extent.height),
	.minDepth = 0.0f, .maxDepth = 1.0f,
};
const VkRect2D scissor = {.offset = {0, 0}, .extent = context.swapchain_extent};

vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
vkCmdSetViewport(cb, 0, 1, &viewport);
vkCmdSetScissor(cb, 0, 1, &scissor);
vkCmdDraw(cb, 3, 1, 0, 0);
```

`shutdown()`: после `vkQueueWaitIdle` — `vkDestroyPipeline`, `vkDestroyPipelineLayout`.

✅ **Чекпоинт B:** оранжевый треугольник вершиной вверх (в Vulkan Y = −0.5 — это верх экрана), без ошибок. `git commit`.

**Что нужно понимать:** стадии конвейера (вершинный шейдер → сборка треугольников → деление на w → viewport →
растеризация → фрагментный шейдер → тест глубины); SPIR-V и `glslc`; что такое pipeline и зачем dynamic state.
Чтение от преподавателя: [A trip through the Graphics Pipeline](https://fgiesen.wordpress.com/2011/07/09/a-trip-through-the-graphics-pipeline-2011-index/).

---

## 7. Этап C — пирамида в вершинном и индексном буферах

### 7.1. Формат вершины

```cpp
struct Vertex {
	float position[3];
	float color[3];
};
```

Vertex input pipeline:

```cpp
const VkVertexInputBindingDescription binding = {
	.binding = 0, .stride = sizeof(Vertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
};
const VkVertexInputAttributeDescription attributes[] = {
	{.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, position)},
	{.location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, color)},
};
// VkPipelineVertexInputStateCreateInfo: vertexBindingDescriptionCount = 1, vertexAttributeDescriptionCount = 2
```

### 7.2. Вершины и индексы пирамиды

Основание — квадрат 2×2 в плоскости `y = −0.75`, апекс в `(0, 0.75, 0)` (высота 1.5). Начало координат — в
середине по высоте, поэтому пирамида вращается «вокруг себя».

Цвета нужны обязательно: освещения нет, и одноцветная пирамида выглядит плоским треугольником.

```cpp
const Vertex vertices[] = {
	{{-1.0f, -0.75f, -1.0f}, {1.0f, 0.0f, 0.0f}},   // 0  B0  красный
	{{ 1.0f, -0.75f, -1.0f}, {0.0f, 1.0f, 0.0f}},   // 1  B1  зелёный
	{{ 1.0f, -0.75f,  1.0f}, {0.0f, 0.0f, 1.0f}},   // 2  B2  синий
	{{-1.0f, -0.75f,  1.0f}, {1.0f, 1.0f, 0.0f}},   // 3  B3  жёлтый
	{{ 0.0f,  0.75f,  0.0f}, {1.0f, 1.0f, 1.0f}},   // 4  A   белый (апекс)
};

const uint32_t indices[] = {
	0, 4, 1,   // боковая грань −Z
	1, 4, 2,   // боковая грань +X
	2, 4, 3,   // боковая грань +Z
	3, 4, 0,   // боковая грань −X
	0, 1, 2,   // основание
	0, 2, 3,   // основание
};
```

**5 вершин, 5 граней (4 треугольника + квадрат), 6 треугольников, 18 индексов.**

Обход каждого треугольника — **против часовой стрелки, если смотреть снаружи** (проверено). Как проверить:
нормаль `n = (b − a) × (c − a)` должна смотреть наружу. Пример для `0, 4, 1`:
`(A − B0) × (B1 − B0) = (1, 1.5, 1) × (2, 0, 0) = (0, 2, −3)` — наружу (по −Z и вверх) ✓.

### 7.3. Буферы через VMA (`graphics.hpp/.cpp`)

```cpp
struct Buffer {
	VkBuffer buffer = VK_NULL_HANDLE;
	VmaAllocation allocation = VK_NULL_HANDLE;
};

Buffer createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, const void* data) {
	const VkBufferCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};
	const VmaAllocationCreateInfo alloc = {
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};
	Buffer b;
	if (vmaCreateBuffer(graphics::internal::context.allocator, &info, &alloc,
	                    &b.buffer, &b.allocation, nullptr) != VK_SUCCESS) {
		std::cerr << "Failed to create buffer\n";
		return {};
	}
	if (data != nullptr) uploadToBuffer(b, data, size);
	return b;
}

void uploadToBuffer(const Buffer& b, const void* data, VkDeviceSize size) {
	vmaCopyMemoryToAllocation(graphics::internal::context.allocator, data, b.allocation, 0, size);
}

void destroyBuffer(Buffer& b) {
	vmaDestroyBuffer(graphics::internal::context.allocator, b.buffer, b.allocation);
	b = {};
}
```

`vmaCopyMemoryToAllocation` = копирование + сброс кэша CPU (flush), если память не «coherent».

В `initialize()`:

```cpp
vertex_buffer = graphics::createBuffer(sizeof(vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertices);
index_buffer  = graphics::createBuffer(sizeof(indices),  VK_BUFFER_USAGE_INDEX_BUFFER_BIT,  indices);
```

### 7.4. Временный вершинный шейдер (без матриц)

Без матриц координаты сразу идут в clip space, где **Z должен быть в [0, 1]** (иначе половина пирамиды
обрежется) и Y направлен вниз:

```glsl
#version 450
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_color;
layout(location = 0) out vec3 v_color;
void main() {
    gl_Position = vec4(in_position.x * 0.5, -in_position.y * 0.5, in_position.z * 0.25 + 0.5, 1.0);
    v_color = in_color;
}
```

Фрагментный: добавить `layout(location = 0) in vec3 v_color;`, писать `out_color = vec4(v_color, 1.0);`.

### 7.5. Рисование

```cpp
const VkDeviceSize offset = 0;
vkCmdBindVertexBuffers(cb, 0, 1, &vertex_buffer.buffer, &offset);
vkCmdBindIndexBuffer(cb, index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
vkCmdDrawIndexed(cb, 18, 1, 0, 0, 0);
```

`shutdown()`: `destroyBuffer(index_buffer); destroyBuffer(vertex_buffer);`.

✅ **Чекпоинт C:** разноцветная пирамида «спереди» (треугольник, перспективы ещё нет). `git commit`.

**Что нужно понимать:** зачем индексный буфер (5 вершин вместо 18); binding/attribute/location/stride/offset;
зачем порядок обхода; как цвет интерполируется внутри треугольника; что делает VMA.

---

## 8. Этап D — математика

Минимум функций: `mat4`, умножение, `translate`, `rotateX`, `rotateY`, `perspective`. Пишем сами в `math.hpp`
(~60 строк) — это главные вопросы на защите.

### 8.1. Хранение — по столбцам (column-major), как в GLSL

```cpp
struct mat4 { float m[16]; };
inline float& at(mat4& a, int row, int col) { return a.m[col * 4 + row]; }
```

Умножение `r = a · b`: `at(r, row, col) = Σ_k at(a, row, k) · at(b, k, col)` (три вложенных цикла).
`identity()` — единицы на диагонали.

Если перепутать строки и столбцы — в шейдер попадёт транспонированная матрица, пирамида исчезнет или исказится.

### 8.2. Матрицы (записаны по строкам, как на бумаге)

```
translate(tx,ty,tz):     rotateX(α):              rotateY(β):
| 1 0 0 tx |             | 1  0   0  0 |          |  c  0  s  0 |
| 0 1 0 ty |             | 0  c  −s  0 |          |  0  1  0  0 |
| 0 0 1 tz |             | 0  s   c  0 |          | −s  0  c  0 |
| 0 0 0 1  |             | 0  0   0  1 |          |  0  0  0  1 |
```

`c = cos(угла)`, `s = sin(угла)`. Внимание: у `rotateY` минус в **нижнем** левом углу, у `rotateX` — в верхнем.

### 8.3. Перспективная проекция под Vulkan

В Vulkan: **Y вниз**, **глубина Z ∈ [0, 1]** (в OpenGL Y вверх, Z ∈ [−1, 1]). `f = 1 / tan(fovY / 2)`:

```
| f/aspect   0          0                      0              |
| 0         −f          0                      0              |   ← «−»: переворот Y под Vulkan
| 0          0     far/(near−far)     near·far/(near−far)     |
| 0          0         −1                      0              |   ← w = −z
```

`aspect = float(extent.width) / float(extent.height)` — **каждый кадр** (окно меняет размер).
`near = 0.1`, `far = 100`.

**Проверено численно** (fov 60°): `(0, 0, −0.1)` → z = 0; `(0, 0, −100)` → z = 1; `(0, 1, −5)` → y = −0.35
(точка выше оси оказывается в верхней половине экрана). Повторить руками — готовый ответ на защите.

### 8.4. Итоговая матрица

```
model = rotateY(angle)                                  // пирамида вращается вокруг вертикальной оси
view  = translate(0, 0, −distance) · rotateX(tilt)      // камера: отодвинуть мир на distance, наклонить
proj  = perspective(fov, aspect, 0.1, 100)
mvp   = proj · view · model                             // применяется справа налево
```

`tilt` = 0.4 рад (≈23°) — смотрим чуть сверху, видно и бока, и наклон граней. `distance` = 4.

### 8.5. Проверка `math.hpp`

Временно в `initialize()` (потом удалить): умножить `perspective(...)` на точки из 8.3, поделить на `w`, напечатать
через `std::cout`. Плюс `translate(1,2,3) · (0,0,0,1) = (1,2,3,1)`.

✅ **Чекпоинт D:** числа совпадают. `git commit`.

**Что нужно понимать:** однородные координаты (зачем 4-я компонента `w = 1`); почему перенос нельзя сделать
матрицей 3×3; матрицы поворота; почему порядок умножения важен; откуда перспектива: точка дальше → делим на
большее `−z` → она ближе к центру; почему в Vulkan минус у `f`.

---

## 9. Этап E — UBO, дескриптор, 3D-пирамида (= оценка 3)

### 9.1. Шейдеры (финальные)

`shaders/pyramid.vert`:

```glsl
#version 450
layout(set = 0, binding = 0) uniform Uniforms { mat4 mvp; } u;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_color;
layout(location = 0) out vec3 v_color;

void main() {
    gl_Position = u.mvp * vec4(in_position, 1.0);
    v_color = in_color;
}
```

`shaders/pyramid.frag`:

```glsl
#version 450
layout(location = 0) in vec3 v_color;
layout(location = 0) out vec4 out_color;
void main() { out_color = vec4(v_color, 1.0); }
```

### 9.2. C++-структура UBO

```cpp
struct Uniforms {
	mat4 mvp;
};
static_assert(sizeof(Uniforms) == 64);
```

Одна `mat4` — 64 байта, выравнивание std140 тут не мешает. (Если добавишь поля — проверяй раскладку в
калькуляторе из read.txt: <https://jackjamison.net/interactive/glsl-layout-calculator/>.)

### 9.3. Создание (в `initialize()`, по порядку)

1. **UBO:** `ubo = graphics::createBuffer(sizeof(Uniforms), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, nullptr);`
2. **`VkDescriptorSetLayout`:** один binding — `.binding = 0`, `.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER`,
   `.descriptorCount = 1`, `.stageFlags = VK_SHADER_STAGE_VERTEX_BIT`.
3. **`VkPipelineLayout`:** `.setLayoutCount = 1`, `.pSetLayouts = &set_layout` (pipeline создавать уже с ним).
4. **`VkDescriptorPool`:** `.maxSets = 1`, pool size `{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}`.
5. **`vkAllocateDescriptorSets`:** `.descriptorPool`, `.descriptorSetCount = 1`, `.pSetLayouts = &set_layout`.
6. **`vkUpdateDescriptorSets`** — связать набор с буфером:

```cpp
const VkDescriptorBufferInfo buffer_info = {.buffer = ubo.buffer, .offset = 0, .range = sizeof(Uniforms)};
const VkWriteDescriptorSet write = {
	.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	.dstSet = descriptor_set,
	.dstBinding = 0,
	.descriptorCount = 1,
	.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	.pBufferInfo = &buffer_info,
};
vkUpdateDescriptorSets(context.device, 1, &write, 0, nullptr);
```

7. Pipeline — как в §6.3, но `.cullMode = VK_CULL_MODE_BACK_BIT`.

### 9.4. Кадр

```cpp
namespace {
Uniforms uniforms;
float rotation_speed = 1.0f, angle = 0.0f;
float distance = 4.0f, tilt_deg = 23.0f, fov_deg = 60.0f;
double prev_time = -1.0;
}

void update(double time) {
	if (prev_time < 0) prev_time = time;
	const float dt = float(time - prev_time);
	prev_time = time;
	angle += dt * rotation_speed;          // не time * speed: иначе при смене скорости пирамида прыгает

	ImGui::Begin("Lab 1");
	ImGui::ColorEdit3("Background", clear_color);
	ImGui::SliderFloat("Rotation speed", &rotation_speed, 0.0f, 3.0f);
	ImGui::SliderFloat("Distance", &distance, 2.0f, 10.0f);
	ImGui::SliderFloat("Tilt", &tilt_deg, -80.0f, 80.0f);
	ImGui::SliderFloat("FOV", &fov_deg, 20.0f, 120.0f);
	ImGui::End();

	const auto& extent = graphics::internal::context.swapchain_extent;
	const float aspect = float(extent.width) / float(extent.height);
	const mat4 model = rotateY(angle);
	const mat4 view = translate(0, 0, -distance) * rotateX(tilt_deg * pi / 180);
	const mat4 proj = perspective(fov_deg * pi / 180, aspect, 0.1f, 100.0f);
	uniforms.mvp = proj * view * model;
}
```

В `render()`:

```cpp
graphics::uploadToBuffer(ubo, &uniforms, sizeof uniforms);   // ДО записи команд, после prepare() — безопасно
// ... begin command buffer, begin render pass, bind pipeline, viewport, scissor, vertex/index buffers ...
vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);
vkCmdDrawIndexed(cb, 18, 1, 0, 0, 0);
```

Вернуть вершинный шейдер из 9.1 вместо временного (7.4).

### 9.5. `shutdown()` — полный список

```cpp
vkQueueWaitIdle(context.graphics_queue);     // уже есть
vkDestroyPipeline(context.device, pipeline, nullptr);
vkDestroyPipelineLayout(context.device, pipeline_layout, nullptr);
vkDestroyDescriptorPool(context.device, descriptor_pool, nullptr);   // освобождает и descriptor_set
vkDestroyDescriptorSetLayout(context.device, set_layout, nullptr);
graphics::destroyBuffer(ubo);
graphics::destroyBuffer(index_buffer);
graphics::destroyBuffer(vertex_buffer);
```

### 9.6. Проверка

- Пирамида объёмная, вращается, наклон камеры показывает и бока, и основание (при `Tilt` < 0).
- Дальние грани не рисуются поверх ближних.
- При изменении размера окна пирамида не растягивается.
- Если видны «внутренности» или пропадают ближние грани — ошибка в обходе (7.2) или в знаке `−f` (8.3).
  Для отладки временно `cullMode = VK_CULL_MODE_NONE`.
- В консоли нет `Validation Error`, в том числе после закрытия окна.

✅ **Чекпоинт E = оценка 3.** `git commit`. Дальше — §11 (README, отчёт) и §12 (подготовка).

**Что нужно понимать:** что такое uniform buffer; descriptor set layout / pool / set / pipeline layout и как
UBO попадает в шейдер (`set = 0, binding = 0`); почему копирование в UBO в `render()`; тест глубины
(`LESS`, clear = 1.0); отсечение задних граней и как GPU определяет лицевую сторону (порядок обхода на экране).

---

## 10. Типичные ловушки

| Симптом | Причина / что делать |
|---------|---------------------|
| `Failed to find wayland-scanner` | Не поставлены пакеты §2.2 |
| `'VkBindDescriptorSetsInfo' does not name a type` | Старые заголовки → §2.3–2.4 и `rm -rf build-debug` |
| Сборка «успешна», но `.spv` нет | Нет `glslc` → поставить, `rm -rf build-debug && cmake --preset local-debug` |
| `…/shaders/pyramid.vert … missing and no known rule` | `compile_shader` добавлен, а файла шейдера нет |
| `Failed to open shaders/…spv` | Запуск не из корня проекта |
| `Segmentation fault` + `is unrecorded and contains no commands` | `render()` не записывает command buffer (§2.5, §5) |
| Ошибка компиляции в `{ .sType = … }` | Поля не в порядке объявления (§4.1) |
| Ничего не рисуется, ошибок нет | `colorWriteMask = 0`; нет `vkCmdSetViewport/Scissor`; z вне [0,1] |
| Пирамида вверх ногами | Нет минуса у `f` в проекции |
| Основание видно сквозь бока | Нет depth test или не очищается глубина |
| Пропадают ближние грани | Обход треугольников / `frontFace` (§9.6) |
| Пирамида — плоский треугольник | Нет разных цветов вершин (§7.2) |
| Пирамида исчезла / искажена | Перепутаны строки и столбцы матрицы (§8.1); градусы вместо радиан |
| Сплющивается при ресайзе | `aspect` не пересчитывается каждый кадр |
| «???» вместо текста в окне | Русские подписи в ImGui (§4.1 п.6) |
| `Validation Error` при закрытии | Что-то не уничтожено в `shutdown()` (§9.5) |

---

## 11. README, отчёт, git

### 11.1. README

Корневой README — файл преподавателя, его не трогаем. Создать **`lab1/README.md`**:
- «Лабораторная работа №1. Основы 3D-графики. Вариант 6 — пирамида» + условие;
- установка зависимостей (команды §2.2–2.4);
- сборка и запуск (`cmake --preset local-debug`, `cmake --build build-debug`, запуск из корня);
- что делают ползунки в окне;
- 1 скриншот.

### 11.2. Отчёт по образцу [3XX_Фамилия_ЛабX.docx](3XX_Фамилия_ЛабX.docx)

```bash
cp "lab1/3XX_Фамилия_ЛабX.docx" "lab1/3XX_<Фамилия>_Лаб1.docx"    # 3XX — номер группы
```

| Часть | Что вписать |
|-------|-------------|
| Титул | «Лабораторная работа №**1**», тема «Основы 3D графики», «Выполнил: И. О. Фамилия», «Группа: М8О-3XXБ-2X» (преподаватель уже указан) |
| **Условие** | Цель работы (абзац из PDF) + «Вариант 6: пирамида» |
| **Метод решения** | Математика: вершины и индексы пирамиды (§7.2), матрицы поворота и переноса (§8.2), перспективная проекция (§8.3), `mvp = proj · view · model`, особенности Vulkan (Y вниз, Z ∈ [0,1]). Объекты Vulkan: `VkBuffer` + VMA (вершины, индексы, uniform), `VkShaderModule`, `VkPipeline`/`VkPipelineLayout`, `VkDescriptorSetLayout`/`VkDescriptorPool`/`VkDescriptorSet`, command buffer, render pass и буфер глубины стартового кода |
| **Результаты** | 2–3 скриншота с подписями: пирамида сбоку; вид сверху/снизу (другой `Tilt`); с другим FOV |
| **Выводы** | «Я научился…»: строить 3D-объект из треугольников, применять матрицы поворота и перспективной проекции, работать с конвейером Vulkan, uniform-буфером и дескрипторами |

Скриншоты — клавиша PrtSc, сохранить в `lab1/screenshots/`. Экспорт в PDF:

```bash
libreoffice --headless --convert-to pdf --outdir lab1 "lab1/3XX_<Фамилия>_Лаб1.docx"
```

### 11.3. Git и финальная проверка

- [ ] `.gitignore`: добавлена строка `CMakeUserPresets.json`.
- [ ] `rm -rf build-debug && cmake --preset local-debug && cmake --build build-debug --parallel` — с нуля без ошибок.
- [ ] Минута работы + ресайз + сворачивание + закрытие — нет `Validation Error`.
- [ ] Нет `ShowDemoWindow` и временной проверки математики (§8.5).
- [ ] `lab1/README.md`, отчёт PDF, скриншоты добавлены.
- [ ] `git checkout master && git merge lab1 && git tag lab1 && git push origin master --tags`.

---

## 12. Подготовка к защите

Для оценки 3 достаточно ответить правильно **на один** вопрос из 3–4, но вопросы случайные — подготовить все
пункты ниже, по 2–3 предложения на каждый.

**Математика**
1. Однородные координаты: зачем 4-я компонента; почему перенос не делается матрицей 3×3.
2. Написать `translate`, `rotateX`, `rotateY` на бумаге.
3. Почему порядок умножения матриц важен: `proj · view · model` и что будет, если поменять `model` местами.
4. Перспектива: дальние точки делятся на большее `−z` и сжимаются к центру; что делает `f = 1/tan(fov/2)`;
   подставить near и far → получить z = 0 и 1.
5. Чем Vulkan отличается от OpenGL (Y вниз, Z ∈ [0,1]) и где это в твоей матрице.
6. Column-major: как матрица хранится в памяти и почему так.
7. Пирамида: 5 вершин, 6 треугольников, 18 индексов; посчитать нормаль одной грани и показать, что обход CCW.

**Vulkan и код**
8. Что делают `initialize` / `update` / `render` / `shutdown`; почему UBO копируется в `render()`.
9. Command buffer и render pass; зачем два clear value.
10. Стадии графического конвейера; что делают вершинный и фрагментный шейдеры.
11. Pipeline: какие состояния в нём есть; vertex input (binding, attribute, stride, offset).
12. Как матрица попадает в шейдер: C++-структура → `VkBuffer` → descriptor set → `uniform` в GLSL.
13. Буфер глубины и тест глубины; отсечение задних граней.
14. Зачем индексный буфер.

Совет: после каждого этапа объясни свой код вслух. Если где-то «ну, так работает» — разберись, прежде чем идти дальше.

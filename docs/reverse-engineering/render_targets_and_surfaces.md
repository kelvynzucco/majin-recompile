# Majin and the Forsaken Kingdom - Render Targets & EDRAM Surfaces Architecture

## 1. Contexto e Objetivo (Passo 2.2 do Plano)
Este documento mapeia como o motor proprietário da *Game Republic* aloca e gerencia suas superfícies de renderização (cores, profundidade, texturas intermediárias de pós-processamento e frontbuffers) na memória de vídeo do Xbox 360 (EDRAM de 10 MB e RAM física unificada).

Compreender este layout é o pré-requisito fundamental (equivalente à tabela de 41 texturas do Forza Horizon, `kTextureTable`) para desacoplar a resolução da cena 3D (2x/1440p+) da camada de interface 2D (HUD/Menus/Fontes em 720p nativo).

---

## 2. Mapa Estruturado de Superfícies do Majin

| Identificador / Superfície | Resolução Base | Formato de Cor / Profundidade | Localização / Alocação | Estágio de Pipeline | Comportamento no Mod de Resolução |
|---|---|---|---|---|---|
| **SceneColor (3D HDR)** | 1280x720 (1x) | `k_16_16_16_16F` ou `k_8_8_8_8` | EDRAM (0x00000000) | `sub_82467540` (Geometria 3D) | **Escalonável (2x -> 2560x1440)** |
| **SceneDepth (Z-Buffer)** | 1280x720 (1x) | `k_24_8` (D24S8) | EDRAM (Tile 1152+) | `sub_82475C78` / `sub_82467540` | **Escalonável (2x -> 2560x1440)** |
| **BloomDownsample_0** | 640x360 (1/2) | `k_8_8_8_8` | RAM Física (Scratch) | `sub_8246C5A0` (Pós-processamento) | Mantém proporção da cena |
| **BloomDownsample_1** | 320x180 (1/4) | `k_8_8_8_8` | RAM Física (Scratch) | `sub_8246C5A0` (Blur) | Mantém proporção da cena |
| **UI Intermediate Target** | 1280x720 | `k_8_8_8_8` | EDRAM / RAM Física | `sub_824684F0` (Quads 2D/Menus) | **Estritamente 1280x720 Nativo** |
| **Tonemap Resolve Target** | 1280x720 | `k_8_8_8_8` (`VK_FORMAT_B8G8R8A8_UNORM`) | RAM Física (Linear) | `sub_8246C4F0` (Composição final) | **Receptor de Downsample 720p** |
| **Font & Subtitle Overlay** | 1280x720 | `k_8_8_8_8` | FrontBuffer | `sub_8246BAC0` (Rasterizador de fontes) | **Estritamente 1280x720 Nativo** |
| **FrontBuffer Ring (0 & 1)** | 1280x720 | `k_8_8_8_8` (32 bpp, 3.68 MB cada) | RAM Física (`0x82ADE2D4`) | `sub_824750E0` (`__imp__VdSwap`) | **Estritamente 1280x720 Nativo** |

---

## 3. Análise da EDRAM Física (10 MB) e Orçamento de Memória

No Xbox 360, a EDRAM possui 10 MB divididos em 2048 blocos de 80x16 amostras (sample tiles).
Para renderização em 1280x720:
- Largura em tiles: `(1280 + 79) / 80 = 16` tiles.
- Altura em tiles: `(720 + 15) / 16 = 45` tiles.
- Superfície de 1 amostra por pixel (1x MSAA): `16 * 45 = 720` tiles.
- Cor (32 bpp) + Profundidade (32 bpp) = `720 + 720 = 1440` tiles.
- Orçamento total disponível: `2048` tiles (sobrando 608 tiles para MSAA ou alvos adicionais).

### Conflito no Escalonamento Vulkan:
No modo de emulação padrão Vulkan `kHostRenderTargets` (FBO):
- O driver tenta criar imagens Vulkan 2x maiores (2560x1440).
- Os comandos de UI em `sub_824684F0` continuam enviando vértices em espaço ortográfico [0..1280, 0..720].
- Na ausência do `VK_EXT_fragment_shader_interlock` (FSI), o aliasing de buffers da EDRAM causa o descarte de pixels da UI, pois os quads não realizam blending adequado com o render target redimensionado.

---

## 4. Comparativo Direto com a Arquitetura do Forza Horizon

No projeto Forza Horizon (`C:\Users\kelvy\Desktop\Forza Horizon\data\build\forza-recomp\src\hooks\scene_resolution.cpp`):
1. O Forza possui uma tabela estática de 41 texturas (`kTextureTable = 0x832295B0`).
2. O mod do Forza intercepta `sub_8257C220` (alocador de texturas de tela) e avalia:
   ```cpp
   inline bool ScreenTextureFollowsScene(std::string_view name) {
     return name != "FrontBuffer_0" && name != "FrontBuffer_1" && name != "MapRender";
   }
   ```
   - As superfícies 3D (`WorldRender`, `WorldDepth`, `ScratchBuffer`, `ShadowMask`, `Bloom`) são ajustadas para a resolução da cena.
   - Os buffers de apresentação e mapa (`FrontBuffer_0`, `FrontBuffer_1`, `MapRender`) são forçados a permanecer em **1280x720**.
3. No Majin:
   - A inicialização em `sub_82150000` declara o FrontBuffer em 1280x720.
   - O estágio `sub_82467540` executa a geometria 3D.
   - O estágio `sub_8246C4F0` mapeia a imagem composta para o buffer de apresentação.
   - Portanto, a estratégia análoga para o Majin consiste em:
     1. Permitir que a passagem 3D (`sub_82467540`) opere em alta resolução.
     2. Interceptar a passagem de UI (`sub_824684F0` e `sub_8246BAC0`) garantindo que as matrizes de projeção, viewport (`PA_CL_VPORT_XSCALE/YSCALE`) e scissor permaneçam travados na escala 1:1 de 1280x720.
     3. Garantir que a apresentação (`__imp__VdSwap`) e swapchain Vulkan recebam o FrontBuffer composto de 720p e utilizem upscaling de apresentação (FSR / Post-Effect) sem quebra de elementos 2D.

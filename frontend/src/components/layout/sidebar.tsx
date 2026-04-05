import { type Component, createMemo, For, type JSX, Show } from "solid-js";

import { useStore } from "../../contexts/store";
import { ToggleScheduleButton } from "../../scheduler";

interface SidebarSectionProps {
  title: string;
  children: JSX.Element;
}

const SidebarSection: Component<SidebarSectionProps> = (props) => (
  <div class="space-y-3">
    <h3 class="text-sm font-semibold text-gray-700 uppercase tracking-wide">{props.title}</h3>
    <div class="space-y-2">{props.children}</div>
  </div>
);

interface SidebarProps {
  onRotate: (turnRight: boolean) => void;
  onLoadImage: () => void;
  onClear: () => void;
  onPersist: () => void;
  onLoad: () => void;
  onPluginChange: (pluginId: number) => void;
  onBrightnessChange: (value: number, shouldSend?: boolean) => void;
  onArtnetChange: (value: number, shouldSend?: boolean) => void;
  onPersistPlugin: () => void;
  onGOLDelayChange: (value: number, shouldSend?: boolean) => void;
  onAudioWaveGainChange: (value: number, shouldSend?: boolean) => void;
  onAudioWaveNoiseFloorChange: (value: number, shouldSend?: boolean) => void;
  onAudioWaveGradientChange: (value: number, shouldSend?: boolean) => void;
}

export const Sidebar: Component<SidebarProps> = (props) => {
  const [store] = useStore();
  const activePluginName = createMemo(
    () => store.plugins.find((p) => p.id === store.plugin)?.name ?? "",
  );
  const isPlugin = (name: string) =>
    activePluginName()
      .toLowerCase()
      .replace(/\s+/g, "")
      .includes(name.toLowerCase().replace(/\s+/g, ""));
  return (
    <>
      <div class="flex-1 min-h-0 overflow-y-auto">
        <Show
          when={!store?.isActiveScheduler}
          fallback={
            <Show when={store.schedule.length > 0}>
              <ToggleScheduleButton />
            </Show>
          }
        >
          {/* ── Display Mode ── */}
          <SidebarSection title="Display Mode">
            <div class="flex flex-col gap-2.5">
              <select
                class="flex-1 px-2.5 py-2.5 bg-gray-50 border border-gray-200 rounded"
                onChange={(e) => props.onPluginChange(parseInt(e.currentTarget.value, 10))}
                value={store?.plugin}
              >
                <For each={store?.plugins}>
                  {(plugin) => <option value={plugin.id}>{plugin.name}</option>}
                </For>
              </select>
              <button
                type="button"
                onClick={props.onPersistPlugin}
                class="w-full bg-gray-700 text-white border-0 px-3 py-2 text-sm cursor-pointer font-semibold hover:opacity-80 active:-translate-y-px transition-all rounded"
              >
                Set as Default
              </button>
            </div>
          </SidebarSection>

          <div class="my-6 border-t border-gray-200" />

          {/* ── Rotation ── */}
          <SidebarSection title={`Rotation (${[0, 90, 180, 270][store?.rotation || 0]}°)`}>
            <div class="flex gap-2.5">
              <button
                type="button"
                onClick={() => props.onRotate(false)}
                class="w-full bg-gray-700 text-white border-0 px-3 py-2 text-sm cursor-pointer font-semibold hover:opacity-80 active:-translate-y-px transition-all rounded flex items-center justify-center gap-2"
              >
                <i class="fa-solid fa-rotate-left" />
                <span class="hidden xl:inline">Left</span>
              </button>
              <button
                type="button"
                onClick={() => props.onRotate(true)}
                class="w-full bg-gray-700 text-white border-0 px-3 py-2 text-sm cursor-pointer font-semibold hover:opacity-80 active:-translate-y-px transition-all rounded flex items-center justify-center gap-2"
              >
                <i class="fa-solid fa-rotate-right" />
                <span class="hidden xl:inline">Right</span>
              </button>
            </div>
          </SidebarSection>

          <div class="my-6 border-t border-gray-200" />

          {/* ── Brightness ── always visible ── */}
          <SidebarSection title="Max Brightness">
            <div class="space-y-2">
              <input
                type="range"
                min="0"
                max="255"
                value={store?.brightness}
                class="w-full"
                onInput={(e) => props.onBrightnessChange(parseInt(e.currentTarget.value, 10))}
                onPointerUp={(e) =>
                  props.onBrightnessChange(parseInt(e.currentTarget.value, 10), true)
                }
              />
              <div class="text-sm text-gray-600 text-right">
                {Math.round(((store?.brightness ?? 255) / 255) * 100)}%
              </div>
            </div>
          </SidebarSection>

          {/* ── ArtNet — shown only for ArtNet plugin ── */}
          <Show when={isPlugin("artnet") && !store?.isActiveScheduler}>
            <div class="my-6 border-t border-gray-200" />
            <SidebarSection title="ArtNet Universe">
              <div class="space-y-2">
                <input
                  type="range"
                  min="0"
                  max="255"
                  value={store?.artnetUniverse}
                  class="w-full"
                  onInput={(e) => props.onArtnetChange(parseInt(e.currentTarget.value, 10))}
                  onPointerUp={(e) =>
                    props.onArtnetChange(parseInt(e.currentTarget.value, 10), true)
                  }
                />
                <div class="text-sm text-gray-600 text-right">{store?.artnetUniverse}</div>
              </div>
            </SidebarSection>
          </Show>

          {/* ── Game of Life delay — shown only for GOL plugin ── */}
          <Show when={isPlugin("life") && !store?.isActiveScheduler}>
            <div class="my-6 border-t border-gray-200" />
            <SidebarSection title="Time Step Delay">
              <div class="space-y-2">
                <input
                  type="range"
                  min="1"
                  max="4000"
                  value={store?.GOLDelay}
                  class="w-full"
                  onInput={(e) => props.onGOLDelayChange(parseInt(e.currentTarget.value, 10))}
                  onPointerUp={(e) =>
                    props.onGOLDelayChange(parseInt(e.currentTarget.value, 10), true)
                  }
                />
                <div class="text-sm text-gray-600 text-right">{store?.GOLDelay}ms</div>
              </div>
            </SidebarSection>
          </Show>

          {/* ── AudioWave controls ── */}
          <Show when={isPlugin("audiowave") && !store?.isActiveScheduler}>
            <div class="my-6 border-t border-gray-200" />
            <SidebarSection title="Audio Wave">
              <div class="space-y-4">
                <div class="space-y-1">
                  <div class="flex justify-between text-xs text-gray-500">
                    <span>Gain</span>
                    <span>{store?.audioWaveGain?.toFixed(3)}×</span>
                  </div>
                  <input
                    type="range"
                    min="1"
                    max="200"
                    value={Math.round((store?.audioWaveGain ?? 1.0) * 100)}
                    class="w-full"
                    onInput={(e) =>
                      props.onAudioWaveGainChange(parseInt(e.currentTarget.value, 10) / 100)
                    }
                    onPointerUp={(e) =>
                      props.onAudioWaveGainChange(parseInt(e.currentTarget.value, 10) / 100, true)
                    }
                  />
                  <div class="flex justify-between text-xs text-gray-400">
                    <span>0.1×</span>
                    <span>2×</span>
                  </div>
                </div>

                <div class="space-y-1">
                  <div class="flex justify-between text-xs text-gray-500">
                    <span>Noise Floor</span>
                    <span>{(store?.audioWaveNoiseFloor ?? 0.01).toFixed(3)}</span>
                  </div>
                  <input
                    type="range"
                    min="0"
                    max="500"
                    // 0–100 → 0.0–0.5
                    value={Math.round((store?.audioWaveNoiseFloor ?? 0.01) * 1000)}
                    class="w-full"
                    onInput={(e) =>
                      props.onAudioWaveNoiseFloorChange(parseInt(e.currentTarget.value, 10) / 1000)
                    }
                    onPointerUp={(e) =>
                      props.onAudioWaveNoiseFloorChange(
                        parseInt(e.currentTarget.value, 10) / 1000,
                        true,
                      )
                    }
                  />
                  <div class="flex justify-between text-xs text-gray-400">
                    <span>0.000</span>
                    <span>0.500</span>
                  </div>
                </div>

                <div class="space-y-1">
                  <div class="flex justify-between text-xs text-gray-500">
                    <span>Gradient Steepness</span>
                    <span>{(store?.audioWaveGradient ?? 2.2).toFixed(2)}</span>
                  </div>
                  <input
                    type="range"
                    min="0"
                    max="300"
                    value={Math.round((store?.audioWaveGradient ?? 2.2) * 100)}
                    class="w-full"
                    onInput={(e) =>
                      props.onAudioWaveGradientChange(parseInt(e.currentTarget.value, 10) / 100)
                    }
                    onPointerUp={(e) =>
                      props.onAudioWaveGradientChange(
                        parseInt(e.currentTarget.value, 10) / 100,
                        true,
                      )
                    }
                  />
                  <div class="flex justify-between text-xs text-gray-400">
                    <span>0.00</span>
                    <span>3.00</span>
                  </div>
                </div>
              </div>
            </SidebarSection>
          </Show>

          {/* ── Draw mode matrix controls ── */}
          <Show when={isPlugin("draw") && !store?.isActiveScheduler}>
            <div class="my-6 border-t border-gray-200 hidden lg:block" />
            <div class="hidden lg:block">
              <SidebarSection title="Matrix Controls">
                <div class="grid grid-cols-2 gap-2">
                  <button
                    type="button"
                    onClick={props.onLoadImage}
                    class="w-full bg-gray-700 text-white border-0 px-3 py-2 text-sm cursor-pointer font-semibold hover:opacity-80 active:-translate-y-px transition-all rounded flex flex-col items-center gap-1"
                  >
                    <i class="fa-solid fa-file-import text-base" />
                    <span class="text-xs">Import</span>
                  </button>
                  <button
                    type="button"
                    onClick={props.onClear}
                    class="w-full bg-gray-700 text-white border-0 px-3 py-2 text-sm cursor-pointer font-semibold hover:opacity-80 active:-translate-y-px transition-all rounded hover:bg-red-600 flex flex-col items-center gap-1"
                  >
                    <i class="fa-solid fa-trash text-base" />
                    <span class="text-xs">Clear</span>
                  </button>
                  <button
                    type="button"
                    onClick={props.onPersist}
                    class="w-full bg-gray-700 text-white border-0 px-3 py-2 text-sm cursor-pointer font-semibold hover:opacity-80 active:-translate-y-px transition-all rounded flex flex-col items-center gap-1"
                  >
                    <i class="fa-solid fa-floppy-disk text-base" />
                    <span class="text-xs">Save</span>
                  </button>
                  <button
                    type="button"
                    onClick={props.onLoad}
                    class="w-full bg-gray-700 text-white border-0 px-3 py-2 text-sm cursor-pointer font-semibold hover:opacity-80 active:-translate-y-px transition-all rounded flex flex-col items-center gap-1"
                  >
                    <i class="fa-solid fa-refresh text-base" />
                    <span class="text-xs">Load</span>
                  </button>
                </div>
              </SidebarSection>
            </div>
          </Show>
        </Show>
      </div>

      {/* ── Footer links ── */}
      <div class="flex flex-col shrink-0 pt-6 border-t border-gray-200 space-y-6">
        <Show when={store?.plugins.some((p) => p.name.includes("Animation"))}>
          <a
            href="#/creator"
            class="inline-flex items-center text-gray-700 hover:text-gray-900 font-medium"
          >
            <i class="fa-solid fa-pencil mr-2" />
            Animation Creator
          </a>
        </Show>

        <a href="#/scheduler" class="items-center text-gray-700 hover:text-gray-900 font-medium">
          <i class="fa-regular fa-clock mr-2" />
          Plugin Scheduler ({store.schedule.length})
        </a>

        <a
          href="/update"
          class="inline-flex items-center text-gray-700 hover:text-gray-900 font-medium"
        >
          <i class="fa-solid fa-download mr-2" />
          Firmware Update
        </a>
      </div>
    </>
  );
};

export default Sidebar;

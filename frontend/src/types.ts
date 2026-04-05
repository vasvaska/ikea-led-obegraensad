export enum SYSTEM_STATUS {
  NONE = "draw",
  WSBINARY = "wsbinary",
  // SYSTEM
  UPDATE = "update",
  LOADING = "loading",
}

export interface ScheduleItem {
  pluginId: number;
  duration: number;
}

export interface StoreActions {
  setIsActiveScheduler: (isActive: boolean) => void;
  setRotation: (rotation: number) => void;
  setPlugins: (plugins: []) => void;
  setPlugin: (plugin: number) => void;
  setBrightness: (brightness: number) => void;
  setIndexMatrix: (indexMatrix: number[]) => void;
  setLeds: (leds: number[]) => void;
  setSystemStatus: (systemStatus: SYSTEM_STATUS) => void;
  setSchedule: (items: ScheduleItem[]) => void;
  setArtnetUniverse: (artnetUniverse: number) => void;
  setGOLDelay: (GOLDelay: number) => void;
  send: (message: string | ArrayBuffer) => void;
  setAudioWaveGain: (audioWaveGain: number) => void;
  setAudioWaveNoiseFloor: (audioWaveNoiseFloor: number) => void;
  setAudioWaveGradient: (audioWaveGradient: number) => void;
}

export interface Store {
  isActiveScheduler: boolean;
  rotation: number;
  brightness: number;
  indexMatrix: number[];
  leds: number[];
  plugins: { id: number; name: string }[];
  plugin: number;
  artnetUniverse: number;
  GOLDelay: number;
  systemStatus: SYSTEM_STATUS;
  connectionState: () => number;
  connectionStatus?: string;
  schedule: ScheduleItem[];
  audioWaveGain: number;
  audioWaveNoiseFloor: number;
  audioWaveGradient: number;
}

export interface IToastContext {
  toast: (text: string, duration: number) => void;
}
